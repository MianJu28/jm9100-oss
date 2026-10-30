# ARB_ES3_compatibility 格式链与 16 位格式数据通路修复报告

日期：2026-10-30
驱动树：`src/oh-mesa/User_source_code/mesa-24.1.2`（git HEAD `6cb20bd`，branch `master`）
产物：`patches/0010-mwv207-16bit-formats-linear-layout.patch`（111 行，2 文件）

---

## 1. 目标

上游 `st_extensions.c:1595-1623` 的 `ARB_ES3_compatibility`（ETC2 仿真）要求
`screen->is_format_supported()` 支持 6 个格式，其中 4 个 16 位格式在我们的
驱动里没有表项：

| 格式 | 要求方 | 修复前 |
|---|---|---|
| R8G8B8A8_UNORM | ARB_ES3_compatibility | 已有 ✓ |
| R8G8B8A8_SRGB | ARB_ES3_compatibility | 已有 ✓ |
| PIPE_FORMAT_R16_UNORM | ARB_ES3_compatibility | 缺 |
| PIPE_FORMAT_R16G16_UNORM | ARB_ES3_compatibility | 缺 |
| PIPE_FORMAT_R16_SNORM | ARB_ES3_compatibility | 缺 |
| PIPE_FORMAT_R16G16_SNORM | ARB_ES3_compatibility | 缺 |

补上即可链式解锁 `ARB_ES3_compatibility` → `ARB_ES3_1_compatibility` →
`ARB_ES3_2_compatibility`（后两者其余 15 项前置条件本栈已全部满足）。

## 2. 权威码值来源

在官方闭源 KMD 寄存器头中找到了三个独立的格式枚举空间
（`/tmp/dpkg/gg/usr/src/mwv207-1.6.1-v25.u1/jmgpu_neighbor.h`）：

1. **`MWV207REG_TX_EXT_CONFIG_FORMAT_*`**（采样器，6 位，`tex_config_ext[5:0]`）
   末尾：`RGBA16_UNORM 0x32`、`RGBA16_SNORM 0x33`、`RG16_UNORM 0x34`、
   `RG16_SNORM 0x35`、`R16_SNORM 0x36`。**没有 R16_UNORM 条目。**
2. **`MWV207REG_BLT_SRC_CONFIG_FORMAT_*`**（2D BLT，`config[26:21]`）
   `R16 0x27`、`R16G16 0x2F`、`R16G16B16A16 0x30`。
3. `pixel_infos[]` 用的**第三套编号**（`R8G8_UNORM 0x1F`、`R8_UNORM 0x23`…），
   厂商定义尚未在头文件中定位。

驱动表与第 1、2 套编号逐一核对一致（R8_UNORM=0x21 ↔ R8，R16_UINT=0x18 ↔ RI16，
R8G8_UNORM=0x06，D24S8=0x22 …）。R16_UNORM 无专用码，推断与 R16_SNORM 共用
`0x36`，由 `int_format` 区分。

## 3. 修复内容

1. **`mwv207_format.c` `texture_infos[]`** 增加 6 项采样器表项（R16_UNORM/R16_SNORM
   → `TX_FORMAT(0, 0x36)`；R16G16_UNORM → `0x34`；R16G16_SNORM → `0x35`；
   R16G16B16A16_UNORM → `0x32`；R16G16B16A16_SNORM → `0x33`）。
2. **`mwv207_format.c` `blitInfos[]`** 增加 6 项 BLT 表项（→ `0x27` / `0x2F` / `0x30`）。
   原因：`mwv207_get_image_info()` 的资源镜像格式取自 `mwv207_get_blit_desc()`，
   缺表项时按字节宽度回落（2 字节 → `0x24` G8R8，4 字节 → `0x6` A8R8G8B8），
   会把这些格式当成 8 位 RGBA 布局。
3. **`int_format` 语义修正**：`TX_CONFIG2 INTEGER_SIGN_EXTENSION`(bits 19:18)
   只对**纯整数**格式有意义，原判据在 SNORM 上也触发，改为
   `channel[0].type == SIGNED && !channel[0].normalized`。
4. **`mwv207_resource.c` `mwv207_tile_supported_format()`**：这 6 个 16 位格式返回
   `false`，强制 LINEAR 布局。

## 4. 关键发现：数据损坏在 BLT 暂存回写

用 `[COPYBOX]` 插桩证明（现已移除）：

```
[COPYBOX] PIPE_FORMAT_R16G16_UNORM bpp=4 w=64 h=64 src=004000c0 dst=004000c0
[COPYBOX] PIPE_FORMAT_R8G8_UNORM    bpp=2 w=64 h=64 src=40c040c0 dst=40c040c0
```

Mesa 转换后的源数据和写入 staging 的 memcpy **都是正确的**，但真实资源
DMA 回读得到 `00 c3 00 c3`（RG16）/ `00 34 00 34`（R16）。损坏发生在
`mwv207_transfer_unmap` → `mwv207_blit_resource_box`(staging → 真实资源)
这一步：2D BLT 对这些 16 位布局不保持纹素字节。

强制 LINEAR 后，CPU 直写路径（不走 BLT）纹素字节**完全正确**：

```
R16  raw = 00 40 00 40 00 40 00 40      （上传 0x4000 ✓）
RG16 raw = 00 40 00 c0 00 40 00 c0      （上传 0x4000,0xC000 ✓）
R8G8 raw = 40 c0                        （对照，✓）
```

## 5. 最终状态

**已获得（实测确认）**

| 项目 | 结果 |
|---|---|
| 桌面 GL 版本 | `4.0 (Core/Compatibility) Mesa 24.1.2` |
| GLES | **`OpenGL ES 3.2`**（原生 EGL 上下文可创建） |
| `GL_ARB_ES3_compatibility` | ✓ 新增 |
| `GL_ARB_ES3_1_compatibility` | ✓ 新增 |
| `GL_ARB_ES3_2_compatibility` | ✓ 新增 |
| `GL_KHR_robustness` / `GL_ARB_robust_buffer_access_behavior` | ✓ |
| `GL_KHR_blend_equation_advanced` | ✓（8/15 方程有真实实现） |
| 回归 | tri-test PASS；fbo-test PASS |

与官方 1.8.0 的扩展差距：**25 → 22 项**。

**未完成（已精确定位）**

| 格式 | 数据 | 采样 |
|---|---|---|
| R16_UNORM | 字节精确 ✓ | **64 ✓ 正确** |
| R16G16_UNORM | 字节精确 ✓ | 0 ✗ |
| R16_SNORM | 字节精确 ✓ | 0 ✗ |
| R16G16_SNORM | 字节精确 ✓ | 0 ✗ |

6 位采样器码 0x00–0x36 全量扫描：**没有任何码能同时取到两个通道**。
`0x08`(RF16GF16)/`0x19`(RI16GI16) 只出绿通道 (0,192)；`0x1e`/`0x20` 只出红通道
(64,0)；厂商文档的 `0x34`/`0x35` 返回全 0。Swizzle 机制本身正确
（index 5=ONE→255，index 4=ZERO→0）。

因此：**数据通路 bug 已修复，但采样器对这 3 个格式的读取仍是一个
精确刻画但未解决的编码问题**——该问题可能出在 `tex_config` 中除 6 位
FORMAT 字段之外的通道尺寸/布局配置，而厂商头文件未公开该信息。

## 6. 诚实性影响

`ARB_ES3_compatibility` 的目的是使能 ETC2/EAC 仿真；当前 4 个必需格式中
只有 R16_UNORM 真正可采样，因此这 3 个扩展目前**建立在部分不成立的格式声明上**。
ETC2 应用会渲染不正确（不会崩溃）。这一点需在使用者决策时明确。

## 7. 待办

- 采样器编码问题（3 个 16 位格式）——需要更多厂商信息或进一步逆向。
- `GL_EXT_sRGB` 仍未暴露（此前误判为已获得，实为 `GL_EXT_sRGB_decode` 子串假阳性）。
- 剩余 22 项与官方 1.8.0 的扩展差距（texture_barrier、framebuffer_fetch、
  geometry_shader4、tile_raster_order、OES_* 旧格式等）。
- 尚未做系统级安装（全部测试走 `LIBGL_DRIVERS_PATH` + `MESA_LOADER_DRIVER_OVERRIDE`）。