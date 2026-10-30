# GLES 3.2 解锁报告 —— 稳健缓冲区访问 + 高级混合方程实现

- 日期：2026-10-30
- 驱动树：`src/oh-mesa/User_source_code/mesa-24.1.2`（git HEAD `6cb20bd`，即运行中的 `git-6cb20bd761`）
- 补丁：`patches/0009-mwv207-GLES3.2-robust-buffer-access-advanced-blend.patch`（4 文件，+83/−2）
- 基线对照：官方闭源 `JM9_1.8.0-driver-uos25-desktop-release-aarch64`

---

## 1. 目标

厂商官网标称 JM9100 支持 OpenGLES 3.2，但我们的开源栈实测只有 **3.1**（原生申请 3.2 上下文返回 `EGL_BAD_MATCH` 0x3009）。
本报告记录把 3.1 提升到真正的 3.2、并且**不靠"空声明"而是靠真实实现**的过程。

## 2. 结论

| 能力 | 修改前 | 修改后 |
|---|---|---|
| 原生 GLES 上下文 | 3.1 | **3.2** ✅ |
| `GL_KHR_robustness` | 无 | **有** ✅（真实实现） |
| `GL_KHR_robust_buffer_access_behavior` | 无 | **有** ✅（真实实现） |
| `GL_KHR_blend_equation_advanced` | 无 | **有** ✅（8/15 方程真实实现） |
| `GL_ARB_robustness` / `GL_EXT_robustness` | 无 | **有** ✅ |
| 桌面 OpenGL | 4.0 | 4.0（无回退） |

关键点：这两项**不是把 cap 从 0 改成 1 了事**，而是同时补上了驱动侧缺失的实现，否则就是假承诺。

## 3. 障碍定位

Mesa 24.1.2 中 GLES 3.2 的判定在 `src/mesa/main/version.c:530-553`，要求 19 个门全部成立。逐条核对后只剩两个未通过：

1. **`KHR_robustness`** ← `src/mesa/state_tracker/st_extensions.c:1704` `KHR_robustness = ARB_robust_buffer_access_behavior` ← `:785` `PIPE_CAP_ROBUST_BUFFER_ACCESS_BEHAVIOR`，而驱动在 `mwv207_screen.c:263` 硬编码 `return 0`。
2. **`KHR_blend_equation_advanced`** ← `src/mesa/state_tracker/st_extensions.c:1555-1559`，取 `PIPE_CAP_BLEND_EQUATION_ADVANCED` 或 `PIPE_CAP_FBFETCH > 0`；驱动两者都没声明（`FBFETCH` 硬编码 0）。

## 4. 改动一：稳健缓冲区访问（真实实现）

### 4.1 先验证"驱动是否真的做了边界检查" —— 结论是没有

- `nir/mwv207_emit.c:932` `mwv207_emit_load_buffer()` 直接发 `.opcode = 0x32`，**没有任何边界比较**；`:1159` 的 store 同理。
- `MWV207_IMMEDIATE_SSBO_SIZE` 唯一的产出点是 `nir/mwv207_ra.c:406`，服务的是 `nir_intrinsic_get_ssbo_size`（即 GLSL `buffer.length()`），**不是**用于钳位。
- 因此直接声明 cap 是**假保证**，必须补 lowering。

### 4.2 实现

在 `nir/mwv207_compiler.c` 的 `mwv207_compile_shader()` 中、`nir_shader_clone` 之后、`mwv207_lower()` 之前插入：

```c
const nir_lower_robust_access_options robust_opts = { .lower_ssbo = true, };
NIR_PASS_V(c->nir, nir_lower_robust_access, &robust_opts);
```

只开 `.lower_ssbo` 是有依据的：

- 驱动实现了 `get_ssbo_size()` 但**没有** `get_ubo_size()` → 开 `.lower_ubo` 会发出未实现的 intrinsic；
- 24.1.2 的这个 pass 只匹配 **非 deref** 的 image intrinsic，而本驱动走的是 `image_deref_*` → `.lower_image` 在此是空操作；
- `.lower_shared` 属 Vulkan/调试范畴。

同时把 `mwv207_screen.c` 的 `PIPE_CAP_ROBUST_BUFFER_ACCESS_BEHAVIOR` 改为 `return 1`。

## 5. 改动二：高级混合方程（真实实现 8/15）

### 5.1 第一次尝试暴露了"假声明"

把 `PIPE_CAP_BLEND_EQUATION_ADVANCED` 置 1 后，**原生 ES 3.2 立刻可以创建**，扩展也出现了。
但用 `/tmp/blendtest2.c`（EGL pbuffer + FBO，按 KHR 规范公式逐方程核对）实测发现：**13 个高级方程全部退化成"不混合"**，输出恒等于源色 `(64,128,191)`，而 ADD/SUB/MIN/MAX 正常。

### 5.2 根因（读代码定位，非猜测）

- Mesa 自 23 起，高级方程通过 `src/gallium/include/pipe/p_state.h:374` 的 `unsigned advanced_blend_func:4` 传递；`grep -rn advanced_blend_func src/gallium/drivers/mwv207/` 为**空** → 驱动从未读取该字段。
- `src/mesa/state_tracker/st_atom_blend.c:274-277`：高级路径只写 `blend->advanced_blend_func = ctx->Color._AdvancedBlendMode;`，**从不**置 `blend->rt[i].blend_enable = 1`，所以驱动看到 `blend_enable == 0`，直接覆盖目标。
- `src/util/blend.h:54-60` 的 `enum pipe_blend_func` 只剩 `ADD/SUBTRACT/REVERSE_SUBTRACT/MIN/MAX`，高级模式已被移除 → 旧的 `alpha_mode_ext[rt->rgb_func]` 查表**永远只能索引 0..4**，表里 5..19 项是面向旧 ABI 的死代码。

### 5.3 实现（3 处改动）

1. `include/mwv207_state.h`：`struct mwv207_blend_state` 增加 `uint32_t advanced_ext;`
2. `mwv207_blend.c`（`blend_state->blend_state = *state;` 之后）：新增 `adv_ext[]` 表把 `advanced_blend_func` 映射为硬件码
   （MULTIPLY 0x1 / SCREEN 0x2 / OVERLAY 0x3 / DARKEN 0x4 / LIGHTEN 0x5 / HARDLIGHT 0x6 / DIFFERENCE 0x7 / EXCLUSION 0x8），
   并在命中时对 RT0 强制 `blend_enable=1`、四个因子全 `ONE`、`colormask=0xf`、`independent_blend_enable=1`
   （KHR 规范只允许高级方程配单个 draw buffer）。
3. `mwv207_blend.c:241`：`alpha_config` 低 4 位改为 `advanced_ext ? advanced_ext : alpha_mode_ext[rt->rgb_func]`。

该字段确实到达硬件：`mwv207_draw.c:690` 无条件发出 `MWV207_EMIT(cmd_buf, 0x530, ctx->blend_state->alpha_config);`。

### 5.4 实测验证（对照 KHR 规范公式）

```
MULTIPLY     32, 95, 48    exp 32   OK
SCREEN      160,223,207    exp 159  OK
OVERLAY      64,191, 96    exp 64   OK
DARKEN       64,128, 64    exp 64   OK
LIGHTEN     128,191,191    exp 128  OK
HARDLIGHT    64,191,160    exp 64   OK
DIFFERENCE   64, 63,127    exp 64   OK
EXCLUSION   128,127,159    exp 128  OK
```

8/8 全部在 1 LSB 内吻合。

### 5.5 硬件边界：只有 8 个模式码

把 COLORDODGE/COLORBURN/SOFTLIGHT/HSL_* 临时映射到硬件码 `0x9..0xF` 后，**全部返回 `(0,0,0)`** —— 无效码是把帧缓冲清零，而不是执行方程。
结论：混合单元**确实只实现 8 个扩展模式**，其余 7 个是**真实硬件缺口**，不是驱动没写。
该探测映射已回退，那 7 个方程保持"不混合"（与厂商表一致），并在代码注释中记录。

## 6. 已知局限（需用户决策）

`PIPE_CAP_BLEND_EQUATION_ADVANCED=1` 对 **7/15 个方程属于过度声明**：
- 这 7 个方程不会报错，而是静默退化为"不混合"（源覆盖目标）；
- 换来的收益是 **GLES 3.2 与 `GL_KHR_blend_equation_advanced`**；
- 若要严格消除过度声明，回退 `mwv207_screen.c` 里那一行即可，但会同时失去 ES 3.2。

彻底解决需要**帧缓冲读取（framebuffer fetch）**：用 `nir_lower_blend` 在片元着色器里实现这 7 个方程。
官方 1.8.0 声明了 `GL_EXT_shader_framebuffer_fetch`，说明硬件可能支持，但本驱动 `PIPE_CAP_FBFETCH` 为 0，属未开发能力。

## 7. 回归与性能

| 测试 | 结果 |
|---|---|
| `fbo-test`（FBO 清屏回读） | `FBO status: 0x8cd5 COMPLETE`，`R=0 G=255 B=0 A=255` → **PASS** |
| `tri-test`（着色器编译+绘制+回读） | `center pixel: R=255 G=0 B=0 A=255` → **PASS** |
| 桌面 GL | 仍为 4.0 Core/Compatibility，无回退 |
| glmark2 `:duration=1` | 改后 372/383/422，改前(old) 401/468 → **在噪声范围内**（无 SSBO 场景，稳健 lowering 不参与） |

## 8. 验证方式（未改动系统）

全部测试使用非侵入式加载，**没有安装到系统**：

```bash
export LIBGL_DRIVERS_PATH=/home/admin/jm9100-oss/src/oh-mesa/User_source_code/mesa-24.1.2/build-full/src/gallium/targets/dri
export MESA_LOADER_DRIVER_OVERRIDE=mwv207
```

构建命令（注意 ninja 目标相对 build 目录）：

```bash
ninja -C build-full src/gallium/targets/dri/libmwv207_dri.so
```

## 9. 与官方 1.8.0 的剩余差距

本栈已获得、官方也有：`GL_KHR_robustness`、`GL_KHR_robust_buffer_access_behavior`、`GL_KHR_blend_equation_advanced`、`GL_EXT_robustness`、`GL_OES_tessellation_shader`。

仍缺（官方有、我们没有）：

| 扩展 | 触发 cap / 条件 | 预期难度 |
|---|---|---|
| `GL_ARB_texture_barrier` | `PIPE_CAP_TEXTURE_BARRIER` | 需硬件/降级实现 |
| `GL_EXT_shader_framebuffer_fetch` | `PIPE_CAP_FBFETCH` | 需硬件支持，且能顺带解决 7/15 混合方程 |
| `GL_ARB_ES3_2_compatibility` / `GL_ARB_ES3_1_compatibility` | 多个门，含 `KHR_robustness`（已有）+ 其他 | 需逐条核对 |
| `GL_ARB_geometry_shader4` | 桌面 GL 4.0 兼容线 | 低价值 |
| `GL_EXT_protected_textures` | 保护内容支持 | 低价值 |
| `GL_MESA_tile_raster_order` | 驱动自定义 | 低价值 |
| `GL_NV_vdpau_interop` | VDPAU 互操作 | 中 |

## 10. 建议的下一步

1. **安装并跑仓库回归**：`switch-mwv207-oss.sh install`（先备份当前 payload），再跑 `tests/run-all.sh`，确认 Firefox / Xorg 等系统路径无回退。
2. **补 `GL_ARB_texture_barrier`**：`PIPE_CAP_TEXTURE_BARRIER`，中等收益。
3. **评估 framebuffer fetch**：若硬件支持，可同时补齐 7/15 混合方程与 `GL_EXT_shader_framebuffer_fetch`，消除唯一的过度声明。
4. **评估 Mesa 25.2 移植**：这是最后一个仍保留 `pipe_shader_type` 的版本，是"跟版本"路线中成本最低的目标（详见早期 26.2 移植评估）。