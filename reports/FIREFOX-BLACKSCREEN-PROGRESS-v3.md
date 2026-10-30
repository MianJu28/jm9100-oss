# Firefox 硬件 WebRender chrome 黑屏 — 追查进度（第三版）

> 日期：2026-10-29 13:50 (+08:00)
> 上一版：`HANDOFF-FIREFOX-BLACKSCREEN.md` 第二版（2026-10-29 10:20）
> 原始任务：阅读仓库内容，继续追踪火狐黑屏的根因，并进行修复。
> 硬约束（用户）：「别再老是重启桌面了，确定修改再重启。」

## 0. 本版结论摘要

**根因未找到。** 本版的核心价值是**用可复现的隔离探针系统性地否证了驱动侧的一大批假设**，并建立了一套可信的测试方法。

三条最重要的结论：

1. **第二版的关键归因需要修正。** 第二版反复引用的 `[BANDSRC]`/`[BAND]` 探针存在**测量时序错位**（详见 §4.1），基于它们的因果推断（"源内容丢失导致黑"）**不成立**。本版已实测证明源内容与屏幕结果**反相关**。
2. **驱动在隔离环境中完全正确。** 新写的 4 个探针（probe220/221/222/223）覆盖 clear→draw、dual-source blend、RT-feedback、clear 后 alpha 混合，**全部 PASS**。驱动侧已知机制被逐一排除。
3. **唯一有效的开关仍是 `NOTILECLR=1`**，且本版在**几何对齐第二版**的条件下完整复现了其数字（band 19.7%→19.9%，L 0%，R≈98.4%→99.6%）。

## 1. 当前故障现象（未变）

Firefox 硬件 WebRender（`gfx.webrender.all=true` + mwv207 DRI）：
- chrome/UI 工具栏带（窗口内 y≈2..86）**恒黑**，band 黑占比 99.9%
- content 区 body 背景正常（`#ffe082`），三个彩色 div 不可见
- 关掉 WebRender 则完全正常（对照，非修复）

## 2. 测试环境（本版重建，可直接复用）

### 2.1 让 Firefox 真正走硬件栈

第二版的 `/tmp/eglglue` 在重启后丢失。重建方式（**这是判断"是否真的走了硬件路径"的唯一可靠手段**）：

```bash
mkdir -p /tmp/eglglue && cd /tmp/eglglue
BUILD=/home/admin/jm9100-oss/src/oh-mesa/User_source_code/mesa-24.1.2/build-full
ln -sf $BUILD/src/egl/libEGL.so.1                       libEGL.so
ln -sf $BUILD/src/egl/libEGL.so.1                       libEGL.so.1
ln -sf $BUILD/src/gbm/libgbm.so.1                       libgbm.so.1
ln -sf $BUILD/src/glx/libGL.so.1                        libGL.so
ln -sf $BUILD/src/glx/libGL.so.1                        libGL.so.1
ln -sf $BUILD/src/mapi/shared-glapi/libglapi.so.0       libglapi.so.0
ln -sf $BUILD/src/mapi/shared-glapi/libglapi_mwv207.so.0.0.0 libglapi_mwv207.so.0
ln -sf $BUILD/src/mapi/es2api/libGLESv2.so.2.0.0        libGLESv2.so.2
ln -sf $BUILD/src/mapi/es1api/libGLESv1_CM.so.1.1.0     libGLESv1_CM.so.1
```

启动必须带：
```bash
LD_LIBRARY_PATH=/tmp/eglglue:$BUILD/src/gallium/targets/dri \
LIBGL_DRIVERS_PATH=$BUILD/src/gallium/targets/dri \
DISPLAY=:0 XAUTHORITY=/home/admin/.Xauthority
```

**判据（必须逐个核对，缺一不可）**：
```bash
# 1) 进程实际加载的是我们的库（最硬的证据）
grep -E "mwv207_dri|libEGL" /proc/$FFPID/maps | awk '{print $NF}' | sort -u
#    期望看到 build-full 下的 libmwv207_dri.so 与 libEGL.so.1.0.0

# 2) 日志里没有软件回落标记
grep -c "driver (null)"              log.txt   # 必须 0
grep -c "failed to create dri2 screen" log.txt # 必须 0

# 3) 有我们 Mesa 的特征输出
grep -c "internalformat = GL_BGRA8_EXT" log.txt  # 应为 1
```

**踩过的坑**：`about:support` 里 `adapterDescription: MWV207` 并不代表硬件 WebRender 生效——`webgl1Renderer` 仍可能是 `llvmpipe`。曾因此误判"修复有效"。**只有 `/proc/PID/maps` 能作准。**

### 2.2 窗口几何必须对齐

测量时必须用**窗口相对坐标**。窗口位于 `+40+21`、尺寸 1282×931（WM 装饰后）。若按屏幕坐标 y=2..80 去量，量到的绝大部分是桌面壁纸——曾因此得出完全错误的 27.5% 假数据。

```bash
xdotool windowmove $FFWID 40 21
xdotool windowsize  $FFWID 1282 931
```

### 2.3 工具脚本（`/tmp/ffx/`）

| 文件 | 用途 |
|---|---|
| `run7.sh TAG PREFS [env...]` | 起一次 Firefox，自动移到 +40+21，t=8/14/20/26 各截一图 |
| `myfp.sh` | **安全**清理测试 Firefox（遍历 `/proc/*/cmdline` 匹配隔离 profile） |
| `q1.html` | `#ffe082` 底 + 蓝/绿/红三个 300×200 div |
| `probe220.c` | clear→draw 隔离验证 |
| `probe221.c` | dual-source blend 隔离验证（含零 dst 场景） |
| `probe222.c` | RT→temp→sample 反馈路径验证（40 帧） |
| `probe223.c` | clear 后 alpha 混合验证 |

**重要**：绝不要用 `pkill -f "profile /tmp/ffx/p_"`——该模式会匹配到你自己的 bash 命令行，把 shell 一起 SIGKILL。用 `myfp.sh`。

### 2.4 测量脚本的坑

`m2.py` 早期版本按屏幕绝对坐标测量，结果不可用。正确做法：`WX,WY = 40,21`，band = 窗口 rows 2..80，L = 窗口 x<1024，R = 窗口 x≥1024，content = rows 90..596 × x 0..1020。

## 3. 本版新增的隔离探针结论（全部否证驱动缺陷）

| 探针 | 验证内容 | 结果 |
|---|---|---|
| probe220 | 4 种尺寸 × 带/不带深度 × 零/白/无 clear → 不透明 quad → 回读 | **全 OK**（7/7） |
| probe221 | `glBindFragDataLocationIndexed` + `SRC1_COLOR/ONE_MINUS_SRC1_ALPHA`，含 dst 清零场景 | **全 PASS**（期望值 (89,115,140) 精确匹配，非"忽略混合"128，非"dst读0"64） |
| probe222 | 复刻 `config_dual_blend` 的 clear→RT拷贝到temp→采样temp→dual混合，跑 40 帧 | **全 PASS**（每 8 帧回读均 good=524288/524288） |
| probe223 | 零 clear + 各种 alpha 的 blended quad（1024×512 与 1280×929） | **全 OK** |

**probe221/223 都曾因"期望值公式写错"而假报 FAIL**，务必注意：
- 正确公式（`glBlendFuncSeparate(ONE, ONE_MINUS_SRC_ALPHA)`）是 `C = Cs·1 + Cd·(1-As)`，**不是** `Cs·As + Cd·(1-As)`
- 控制组若取 `Cs=0`，两个公式恰好同值，会掩盖错误。**对照组必须用非零值。**

## 4. 第二版结论的修正

### 4.1 `[BANDSRC]`/`[BAND]` 存在测量时序错位（本版最重要的修正）

`mwv207_draw.c` 中：
- `mwv207_config_render_piepline(...)` 在 **line 1517**
- `MWV207_BAND` / `MWV207_BANDSRC` 的 dump 代码在 **line 1956+**
- `mwv207_submit(ctx, NULL, false)` 在 **line 2395**

即：**BAND/BANDSRC 的 CPU mmap 读的是"上一批提交之后"的状态，不是本笔 draw 实际采样的内容。** 本笔 draw 的 GPU 执行尚未发生。

**实测反证**（同纹理 `6fa1b500`、同 dst `(0,0..1024,86)`）：

| draw | BANDSRC 报告的源内容 | 屏幕结果 |
|---|---|---|
| `#20` | 有内容 `nzbo=512/512` | **黑** |
| `#28` | 空 `nzbo=0/512` | **白** |

**源内容与结果完全反相关**——这只能由时序错位解释。因此第二版 §4.2、§4.3 中所有基于 `BANDSRC local/lpx/nzbo` 的推断都需要重新评估。

### 4.2 `sc=` 字段曾被截断

本版给 `[BAND]` 增加状态字段时，曾把 4 个 32 位 scissor 压进一个 u32：
```c
uint32_t sc_ = (l<<24)|(t<<16)|(r<<8)|b;   // 错误：高位被截断
```
产生 `sc=000503a1` 这类乱码。修正为分 4 个值输出后确认 scissor **始终是全窗口 `0,0..1280,929`**，与第二版 §2 的结论一致。

### 4.3 已用本版方法复现并确认第二版的数字

几何对齐 + 正确测量后：

| run | 开关 | band | L(x<1024) | R(x≥1024) |
|---|---|---|---|---|
| BASE2 | 无 | **99.9%** | 99.9% | 100.0% |
| NTC2 | `NOTILECLR=1` | **19.9%** | **0.0%** | 99.6% |
| DF2B | `DRAWFLUSH=2` | 99.9% | 99.9% | 100.0% |
| NRT1 | `NORTTS=1` | 99.9% | 99.9% | 100.0% |
| NFC1 | `NOFASTCLEAR=1` | 99.9% | 99.9% | 100.0% |
| SE1 | `SUBMITEACH=1` | 99.9% | 99.9% | 100.0% |
| PAINT1 | `DRAWFLUSH=1` | 99.9% | — | — |
| CLR3D | 零 clear 改走 3D 管线 | 99.9% | 99.9% | 100.0% |

`NOTILECLR` 的数字（19.9% / L 0% / R 99.6%）与第二版（19.7% / 0% / 98.4%）吻合，且 t8→t14 有过渡（95.1%→19.9%），对应第二版 §3 的「第 6~23 帧白、27 帧起黑」。

## 5. 本版发现的一个**真实**驱动缺陷（已修复并被回退，见 §7）

在给 `config_dual_blend` 的 RT→临时纹理 blit 加内存 dump（`MWV207_DUALDUMP`）后，抓到明确失败：

```
[DUALDUMP] rt=...1024x512 ts_en=0 tsclr=00000000 src_nz=512 first=ffffffff mid=ffffffff
         | tmp=... tmp_nz=0 first=00000000 mid=00000000   *** BLIT LOST CONTENT ***
```

RT 里有完整白色内容，拷到临时纹理后全空。

**根因**：`mwv207_get_image_info()`（`mwv207_format.c:701-706`）在 `res->ts.bo` 存在时**无条件**设置 `src_compress`/`dst_compress` 压缩描述位；`mwv207_copy_layer()`（`mwv207_blit.c:473`）又**无条件**把这些位带进 blit。于是资源被 `disable_tile()` 解压后（`ts.enable=0`，主 BO 是普通像素），blit 仍按压缩格式解读未压缩数据 → 读到零。

**修复**：压缩位改为跟随 `ts.enable`：
```c
src_configex = src_level->src_configex |
   (src->ts.enable ? src_level->src_compress : 0);
// dst 侧同理：dst->ts.enable ? dst_level->dst_compress : 0
```

**验证**：`BLIT LOST CONTENT` 从 1 次降到 **0 次**；probe220/221/222 全部无回归。

**⚠️ 但这不修黑屏**：应用后 band 仍 99.9%。它是**真实缺陷但不是充分原因**。该改动已随 §7 的回退一并撤销，补丁保留在归档中，可随时重新应用。

**附带价值**：这个缺陷可以解释第二版反复强调的「**双峰现象**」（同一二进制 band 在 20%~100% 随机）——`tsclr` 在 `ffffffff`/`00000000` 之间切换，压缩解读的结果随之跳变。

## 6. 累计排除清单（勿再走回头路）

| 假设 | 验证方式 | 结论 |
|---|---|---|
| clear 损坏 tile 内容 | probe220 | **证伪**，clear→draw 全正常 |
| dual-source blend 算法错 | probe221（含零 dst） | **证伪**，精确匹配 GL 规格 |
| RT→temp→sample 反馈路径 | probe222（40 帧） | **证伪** |
| clear 后 alpha 混合 | probe223 | **证伪** |
| texture cache 失效 | `DRAWFLUSH=1/2`（CACHE_ALL / CACHE_TEXTURE_ALL） | 无效 |
| RT 未解压 | `NORTTS=1` | 无效 |
| fast-clear 本身 | `NOFASTCLEAR=1` | 无效 |
| 命令批处理/状态陈旧 | `SUBMITEACH=1` | 无效 |
| dual-blend 硬件清零 | `NODUAL=1` | 无效 |
| blit engine cluster 切片 | `CLR3D`（改走 3D 管线 clear） | 无效 |
| texture descriptor 地址过期 | `TEXADDR` 实测 3506 次绑定 | **排除**，address 恒 0、reloc 跟随 BO |
| scissor 覆盖不足 | 修正截断后实测 | **排除**，恒为全窗口 |
| `texture_dirty` 发错位（本会话早期修复） | 实测 base 与 fix 完全相同 | **无效，已回退** |

## 7. 收尾状态（2026-10-29 13:50 核验）

- ✅ **源码已回退**：`git checkout -- src/gallium/drivers/mwv207 src/gallium/winsys/mwv207 src/loader/loader_dri3_helper.c`
  - 当前 `git status` 干净，HEAD = `6cb20bd`
- ✅ **补丁已归档**：`/home/admin/jm9100-oss/reports/instrumentation-2026-10-29-turn15.patch`
  - 2057 行，sha256 `79bf14301b453dee001cbe9de058886867c730869851171b47aac182bfef113b`
  - 含全部插桩 + §5 的压缩位修复
  - 副本：`$B/instrumentation-2026-10-29-turn15.patch`
- ✅ **插桩二进制已留存**：`/tmp/ffx/dri_instr_turn15.so`
  - sha256 `b76dbfb965115f76af10ae194ceeb279724b1945e8ae9ec97fa0c63861e7765f`
- ✅ **系统驱动从未被修改**：`/usr/local/lib/mwv207-oss/mwv207_dri.so`
  - sha256 `f1333e0416c317d88c562b094f031beaa7e979fd01864a47bd840a34bc297c77`（10-28 17:08，root 0644）
  - 全程仅用 `LIBGL_DRIVERS_PATH` 做隔离测试，未触碰系统路径
- ✅ **测试进程已清理**（`myfp.sh`）

## 8. 未解决问题与下一步建议

### 8.1 核心障碍

现有插桩**无法在「GPU 真正执行该 draw 的那一刻」读取 FB**：
- `MWV207_EACH` 会强制 submit+wait，**本身改变时序**
- `MWV207_BANDSRC/BAND` 在 submit 之前 dump，读到的是上一批的状态（§4.1）
- `MWV207_POSTGPU` 在 draw 内调用 `texture_map`，会重入 blit+submit 导致 **Firefox 崩溃**（本版实测，有 minidump）

### 8.2 建议的两条路径

**(a) 寄存器级取证** — 用 GPU 硬件计数器/性能寄存器确认该 draw 是否真的产生了 fragment 写入。需要 mwv207 寄存器文档，可先查 `include/hw/mwv207reg_3d.h` 里与 PE（pixel engine）统计相关的寄存器。

**(b) Firefox 侧取证** — 确认 WebRender 是否真的把该 tile 提交给 composite（是否只是"没画"而非"画了没显示"）。需要 Firefox 源码或 `MOZ_WEBRENDER_*` 调试开关。

本版倾向于认为，继续在驱动里加 env 开关试错的收益已很低。

### 8.3 若继续的安全约束

1. 改驱动后**必须**先跑 probe220+221+222+223 四个探针，全 PASS 才算无回归
2. 用 `LIBGL_DRIVERS_PATH` 隔离测试，**不动** `/usr/local/lib/mwv207-oss/`
3. 每次测量前核对 `/proc/PID/maps`，确认真的走了 build-full 驱动
4. 窗口必须移到 `+40+21` 并用**窗口相对坐标**测量
5. 单 run 内多时间点对比（双峰现象）；不要用单帧跨 run 比较

## 9. 关键数据文件（`$B`）

`B=/media/admin/f1b1d5e8-eece-4251-9dbe-3adb7b856280/MianJu/Data/per/Git/jm9100/ffx-evidence`

| 文件 | 说明 |
|---|---|
| `log_BASE2.txt` / `t_BASE2_*.png` | 基线（band 99.9%） |
| `log_NTC2.txt` / `t_NTC2_*.png` | `NOTILECLR=1`（band 19.9%，L 0%） |
| `log_DF2B.txt` / `log_NRT1.txt` / `log_NFC1.txt` / `log_SE1.txt` | 各开关对照，全部 99.9% |
| `log_CLR3D.txt` | 3D 管线 clear 实验 |
| `log_DBG2.txt` | `NOFASTCLEAR + CLR2 + TILE`（含 tile 满→零转捩的原始证据） |
| `log_BAND3.txt` | `BAND + CLR2 + TILE + TRACE`（含 §4.1 的反相关证据） |
| `log_DUAL2.txt` / `log_FIX10.txt` | DUALDUMP：修复前 `BLIT LOST CONTENT`=1，修复后=0 |
| `log_ADDR1.txt` | TEXADDR：3506 次 descriptor 绑定实测 |
| `instrumentation-2026-10-29-turn15.patch` | 全部改动归档 |
