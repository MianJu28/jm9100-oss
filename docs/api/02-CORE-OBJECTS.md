# 02 · 核心对象层（pipe_screen / pipe_context / resource / surface / format）

本层的代码根目录（下称 `MWV207/`）：

```
src/oh-mesa/User_source_code/mesa-24.1.2/src/gallium/drivers/mwv207/
```

Mesa 版本 **24.1.2**，`pipe_screen` / `pipe_context` 仍是 `get_param(PIPE_CAP_*)` 风格（capability 没有迁移到 `screen->caps.*` 结构）。

## 0. 证据约定与读法

| 约定 | 说明 |
|---|---|
| 行号 | 全部为 Mesa 24.1.2 快照的**实际行号**，格式 `文件:行号`；文件名省略 `MWV207/` 前缀时指本目录下的文件（如 `mwv207_screen.c:63`） |
| 函数行号 | 引用函数时给的是**函数名所在行**（例：`mwv207_get_param` → `mwv207_screen.c:60`）；引用语句/字段时给该语句自身行号 |
| 「未确认」 | 源码里读不出来、或需要厂商头文件/外部定义才能确定的结论，均显式标注 |
| 缺陷引用 | `D1`–`D7` 指 `reports/ANALYSIS-REPORT.md` 的驱动层缺陷清单；该报告针对 Mesa 22.3.7 fork，行号与本快照**不完全对应**，本文同时给出本快照行号 |
| 寄存器 | 命令流里的 16 位寄存器号 = 厂商寄存器 `_Address >> 2`（见 `00-ARCHITECTURE.md` §2.1、`registers/REGISTERS.md`） |

---

## 1. 覆盖范围与统计

| 文件 | 行数 | 角色 |
|---|---|---|
| `mwv207_screen.c` | 679 | `pipe_screen` 实现 + 全部 capability + 格式支持判定 |
| `include/mwv207_screen.h` | 56 | `struct mwv207_screen`、`mwv207_screen()` 内联、`RET` 宏 |
| `mwv207_context.c` | 232 | `pipe_context` 创建/销毁、flush、memory barrier、fence fd |
| `include/mwv207_context.h` | 190 | `struct mwv207_context`、dirty 位、`mwv207_pipe_info` |
| `mwv207_resource.c` | 661 | 资源创建/布局/tile、transfer map/unmap、dmabuf |
| `include/mwv207_resource.h` | 153 | `mwv207_resource` / `mwv207_level_info` / tile 模式 |
| `mwv207_surface.c` | 363 | 表面（镜像描述符）、framebuffer 状态 |
| `include/mwv207_surface.h` | 125 | `mwv207_surface` / `mwv207_pixel_info`、采样器与视图结构 |
| `mwv207_format.c` | 963 | 三张格式表 + 所有格式描述转换 |
| `include/mwv207_format.h` | 182 | `mwv207_texture_desc` 等结构、`TX_FORMAT()`、swizzle 枚举 |

关键计数（用 `grep` 逐条清点，非估算）：

| 统计项 | 数量 | 位置 |
|---|---|---|
| `case PIPE_CAP_*`（`get_param`） | **115** | `mwv207_screen.c:63-326` |
| `case PIPE_CAPF_*`（`get_paramf`） | **15** | `mwv207_screen.c:337-358` |
| `case PIPE_SHADER_CAP_*`（`get_shader_param`） | **28** | `mwv207_screen.c:371-463` |
| `case PIPE_COMPUTE_CAP_*`（`get_compute_param`） | **17** | `mwv207_screen.c:479-515` |
| `switch/case` 标签总数（`mwv207_screen.c`） | **203** | 含上述 115+15+28+17 与 `mwv207_sample_count_flags` 等 |
| `texture_infos[]` 条目 | **82** | `mwv207_format.c:54-177` |
| `pixel_infos[]` 条目 | **62** | `mwv207_format.c:233-321` |
| `blitInfos[]` 条目 | **37** | `mwv207_format.c:408-468` |

`get_param` 未列出的 cap 走 `default: u_pipe_screen_get_param_defaults(pscreen, param)`（`mwv207_screen.c:328-329`）；`get_paramf` 的 default 只是 `debug_printf("unknown paramf %d", param); return 0;`（`mwv207_screen.c:359-361`）；`get_shader_param` 的 default 返回 0（`mwv207_screen.c:465-466`）。

---

## 2. `pipe_screen` 层

### 2.1 `struct mwv207_screen` 字段表

定义在 `include/mwv207_screen.h:39-45`：

```c
/* include/mwv207_screen.h:39-45 */
struct mwv207_screen {
   struct pipe_screen base;              /* 必须放首位：mwv207_screen(pscreen) 用容器转换取回 */
   struct mwv207_winsys *ws;             /* winsys 函数表（bo_* / cs_* / fence_*） */
   struct slab_parent_pool transfer_pool;/* mwv207_transfer 的父级 slab，避免每 transfer 一次 malloc */
};
```

| 字段 | 类型 | 作用 | 证据 |
|---|---|---|---|
| `base` | `struct pipe_screen` | 对外的 `pipe_screen`，Mesa 只认这个 | `include/mwv207_screen.h:40` |
| `ws` | `struct mwv207_winsys *` | winsys 虚表指针，创建时由 `mwv207_create_screen()` 直接存入（`iw` 不做判空） | `include/mwv207_screen.h:41`、`mwv207_screen.c:649` |
| `transfer_pool` | `struct slab_parent_pool` | `mwv207_transfer` 的内存池；`slab_create_parent(&screen->transfer_pool, sizeof(struct mwv207_transfer), 16)` | `include/mwv207_screen.h:42`、`mwv207_screen.c:674` |

配套的取回宏与内联函数（`include/mwv207_screen.h:47-51`）：

```c
/* include/mwv207_screen.h:47-51 */
static inline struct mwv207_screen *
mwv207_screen(struct pipe_screen *screen)
{  return (struct mwv207_screen *)screen;  }
```

`RET` 宏（`include/mwv207_screen.h:33-37`）用于 `get_compute_param`：把 `*(uintN_t*)ret = <expr>` 与「返回字节数」合并成两个语句，供 `PIPE_COMPUTE_CAP_IR_TARGET` 之类的「数组/字符串」型 cap 使用（`mwv207_screen.c:486-515` 的 `RET(((uint64_t []){...}))`）。

### 2.2 `pipe_screen` 虚表映射

创建与装配全部在 `mwv207_create_screen()`（`mwv207_screen.c:642-679`）内完成；**没有** `struct pipe_screen` 的静态模板。

| `pipe_screen` 虚表成员 | 驱动函数 | 赋值行 | 实现行 | 作用 |
|---|---|---|---|---|
| `get_name` | `mwv207_get_name` | `mwv207_screen.c:651` | `mwv207_screen.c:54-57` | 返回 `"MWV207 (JingjiaMicro Corporation)"` |
| `get_vendor` | `mwv207_get_vendor` | `mwv207_screen.c:652` | `mwv207_screen.c:42-45` | 返回 `"MWV207"` |
| `get_device_vendor` | `mwv207_get_device_vendor` | `mwv207_screen.c:653` | `mwv207_screen.c:48-51` | 返回 `"MWV207 (vendor-id: 0x0731)"` |
| `get_param` | `mwv207_get_param` | `mwv207_screen.c:655` | `mwv207_screen.c:60-331` | 115 个 `PIPE_CAP_*`，见 §2.3.1 |
| `get_paramf` | `mwv207_get_paramf` | `mwv207_screen.c:656` | `mwv207_screen.c:334-363` | 15 个 `PIPE_CAPF_*`，见 §2.3.2 |
| `get_shader_param` | `mwv207_get_shader_param` | `mwv207_screen.c:657` | `mwv207_screen.c:366-470` | 28 个 `PIPE_SHADER_CAP_*`，见 §2.3.3 |
| `get_compute_param` | `mwv207_get_compute_param` | `mwv207_screen.c:658` | `mwv207_screen.c:473-519` | 17 个 `PIPE_COMPUTE_CAP_*`，见 §2.3.4 |
| `get_compiler_options` | `mwv207_get_compiler_options` | `mwv207_screen.c:659` | `mwv207_shader.c:856` | 上报 GLSL 编译器选项位 |
| `is_format_supported` | `mwv207_is_format_supported` | `mwv207_screen.c:660` | `mwv207_screen.c:539-592` | 格式/绑定/采样数支持判定，见 §2.3.5 |
| `get_timestamp` | `mwv207_get_timestamp` | `mwv207_screen.c:661` **和** `:663` | `mwv207_screen.c:595-598` | 返回 `os_time_get_nano()`；**赋值写了两遍**（冗余，无害） |
| `context_create` | `mwv207_context_create` | `mwv207_screen.c:665` | `mwv207_context.c:175-232` | 创建 `pipe_context`，见 §3 |
| `destroy` | `mwv207_destroy_screen` | `mwv207_screen.c:666` | `mwv207_screen.c:629-639` | `ws->destroy()` 成功才 `glsl_type_singleton_decref()` + `u_transfer_helper_destroy()` + `slab_destroy_parent()` + `FREE()` |
| `fence_reference` | `mwv207_fence_reference` | `mwv207_screen.c:668` | `mwv207_screen.c:600-607` | 转调 `ws->fence_reference()` |
| `fence_finish` | `mwv207_fence_finish` | `mwv207_screen.c:669` | `mwv207_screen.c:610-618` | 转调 `ws->fence_wait(ws, fence, timeout)` |
| `fence_get_fd` | `mwv207_fence_get_fd` | `mwv207_screen.c:670` | `mwv207_screen.c:620-626` | 转调 `ws->fence_get_fd()` |
| `can_create_resource` 等 7 个资源回调 | 见 §4.6 | `mwv207_screen.c:672` | `mwv207_resource.c:649-660` | 由 `mwv207_screen_resource_init(&screen->base)` 批量装配 |

`mwv207_create_screen()` 的其余关键行：`CALLOC_STRUCT(mwv207_screen)` 且**检查了 NULL**（`mwv207_screen.c:644-647`）、`slab_create_parent(...)` `mwv207_screen.c:674`、`glsl_type_singleton_init_or_ref()` `mwv207_screen.c:676`。

### 2.3 Capability 表（穷尽）

#### 2.3.1 `PIPE_CAP_*` —— 共 115 个 case（`mwv207_screen.c:60-331`）

「门控/消费方」列是该 cap 在 Mesa 中典型的使用者或对应的 GL/GLES 扩展；无法从本仓库源码确认的写「未确认」。

| # | 行 | `PIPE_CAP_*` | 返回值 | 语义 | 门控/消费方 |
|---|---|---|---|---|---|
| 1 | 63 | `PIPE_CAP_GRAPHICS` | `1` | 驱动提供图形管线（非纯计算） | 区分图形/计算驱动（具体调用点未确认） |
| 2 | 65 | `PIPE_CAP_TGSI_TEXCOORD` | `1` | TGSI 里用 TEXCOORD 语义传 `gl_TexCoord` | 老式 GL 兼容路径 |
| 3 | 67 | `PIPE_CAP_IMAGE_STORE_FORMATTED` | `1` | 支持格式化 `imageStore`（非仅 raw 存储） | `ARB_shader_image_load_store`（GL 4.2） |
| 4 | 69 | `PIPE_CAP_NPOT_TEXTURES` | `1` | 支持非 2 次幂纹理 | GL 2.0 / GLES 2.0 基础 |
| 5 | 71 | `PIPE_CAP_MAX_RENDER_TARGETS` | `4` | 最多 4 个颜色附件 | `GL_MAX_COLOR_ATTACHMENTS` / `GL_MAX_DRAW_BUFFERS`、MRT（GL 2.0/3.0） |
| 6 | 73 | `PIPE_CAP_OCCLUSION_QUERY` | `1` | 支持遮挡查询 | `GL_ARB_occlusion_query2`（GL 2.0+） |
| 7 | 75 | `PIPE_CAP_QUERY_TIME_ELAPSED` | `1` | 支持耗时查询 | `GL_ARB_timer_query`（GL 3.3） |
| 8 | 77 | `PIPE_CAP_TEXTURE_SHADOW_MAP` | `1` | 深度比较采样（compare mode） | GL 1.5 / GLES 3.0 阴影采样 |
| 9 | 79 | `PIPE_CAP_TEXTURE_SWIZZLE` | `1` | 纹理 swizzle | `GL_ARB_texture_swizzle`（GL 3.3） |
| 10 | 81 | `PIPE_CAP_MAX_TEXTURE_2D_SIZE` | `8192` | 2D 纹理边长上限 | `GL_MAX_TEXTURE_SIZE` |
| 11 | 83 | `PIPE_CAP_MAX_TEXTURE_3D_LEVELS` | `14` | 3D 纹理最大 mip 级数 | `GL_MAX_3D_TEXTURE_SIZE`（8182 尺寸场景的层数上限） |
| 12 | 85 | `PIPE_CAP_MAX_TEXTURE_CUBE_LEVELS` | `14` | 立方图最大 mip 级数 | `GL_MAX_CUBE_MAP_TEXTURE_SIZE = 8192` |
| 13 | 87 | `PIPE_CAP_BLEND_EQUATION_SEPARATE` | `1` | RGB/Alpha 混合方程可分离 | GL 2.0 |
| 14 | 89 | `PIPE_CAP_BLEND_EQUATION_ADVANCED` | `1`（**部分满足**） | 高级混合方程 | `KHR_blend_equation_advanced`；代码注释 `mwv207_screen.c:90-97` 明确：硬件扩展混合字段只编码 15 个 KHR 方程中的 8 个（MULTIPLY/SCREEN/OVERLAY/DARKEN/LIGHTEN/HARDLIGHT/DIFFERENCE/EXCLUSION），其余 7 个（COLORDODGE/COLORBURN/SOFTLIGHT/HSL_*）**渲染时不做混合**；声明它是为了过 native GLES 3.2 门控 |
| 15 | 99 | `PIPE_CAP_MAX_STREAM_OUTPUT_BUFFERS` | `4` | 变换反馈独立缓冲数 | `GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_ATTRIBS`（GL 3.0） |
| 16 | 101 | `PIPE_CAP_STREAM_OUTPUT_INTERLEAVE_BUFFERS` | `1` | 支持交错变换反馈 | GL 3.0 |
| 17 | 103 | `PIPE_CAP_MAX_STREAM_OUTPUT_SEPARATE_COMPONENTS` | `64` | 独立模式最大分量数 | `GL_MAX_TRANSFORM_FEEDBACK_SEPARATE_COMPONENTS` |
| 18 | 105 | `PIPE_CAP_MAX_STREAM_OUTPUT_INTERLEAVED_COMPONENTS` | `64` | 交错模式最大分量数 | `GL_MAX_TRANSFORM_FEEDBACK_INTERLEAVED_COMPONENTS` |
| 19 | 107 | `PIPE_CAP_STREAM_OUTPUT_PAUSE_RESUME` | `1` | 变换反馈可暂停/恢复 | GL 3.0 |
| 20 | 109 | `PIPE_CAP_PRIMITIVE_RESTART` | `1` | 图元重启 | GL 3.1 |
| 21 | 111 | `PIPE_CAP_INDEP_BLEND_ENABLE` | `1` | 每 RT 独立开关混合 | `GL_ARB_draw_buffers_blend`（GL 3.0） |
| 22 | 113 | `PIPE_CAP_INDEP_BLEND_FUNC` | `1` | 每 RT 独立混合函数 | 同上 |
| 23 | 115 | `PIPE_CAP_MAX_TEXTURE_ARRAY_LAYERS` | `8192` | 纹理数组最大层数 | `GL_MAX_ARRAY_TEXTURE_LAYERS`（GL 3.0；GLES 3.0 下限 256） |
| 24 | 117 | `PIPE_CAP_FS_COORD_ORIGIN_UPPER_LEFT` | `1` | `gl_FragCoord` 原点在左上 | GL 约定（`GL_ARB_fragment_coord_conventions`） |
| 25 | 119 | `PIPE_CAP_FS_COORD_PIXEL_CENTER_HALF_INTEGER` | `1` | 像素中心偏移 0.5 | 同上 |
| 26 | 121 | `PIPE_CAP_DEPTH_CLIP_DISABLE` | `1` | 可关闭深度裁剪 | `GL_ARB_depth_clamp`（GL 3.2 起核心） |
| 27 | 123 | `PIPE_CAP_VS_INSTANCEID` | `1` | VS 可读 `gl_InstanceID` | GL 3.1 / GLES 3.0 |
| 28 | 125 | `PIPE_CAP_VERTEX_ELEMENT_INSTANCE_DIVISOR` | `1` | 顶点属性除数（实例化数组） | `GL_ARB_instanced_arrays`（GL 3.3 / GLES 3.0） |
| 29 | 127 | `PIPE_CAP_MIXED_COLORBUFFER_FORMATS` | `1` | 允许各 RT 格式不同 | GL 3.0 |
| 30 | 129 | `PIPE_CAP_SEAMLESS_CUBE_MAP` | `1` | 立方图跨面无缝过滤 | `GL_ARB_seamless_cube_map`（GL 3.2） |
| 31 | 131 | `PIPE_CAP_MIN_TEXEL_OFFSET` | `-8` | 纹理取偏移下限 | `GL_MIN_PROGRAM_TEXEL_OFFSET` |
| 32 | 133 | `PIPE_CAP_MAX_TEXEL_OFFSET` | `7` | 纹理取偏移上限 | `GL_MAX_PROGRAM_TEXEL_OFFSET` |
| 33 | 135 | `PIPE_CAP_CONDITIONAL_RENDER` | `1` | 条件渲染 | `GL_NV_conditional_render`（GL 3.0） |
| 34 | 137 | `PIPE_CAP_VERTEX_COLOR_UNCLAMPED` | `1` | 顶点颜色不钳制 | GL 3.0 |
| 35 | 139 | `PIPE_CAP_CONSTANT_BUFFER_OFFSET_ALIGNMENT` | `256` | UBO 绑定偏移对齐 | `GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT` |
| 36 | 141 | `PIPE_CAP_MIN_MAP_BUFFER_ALIGNMENT` | `256` | 缓冲区映射最小对齐 | `glMapBufferRange` 对齐要求 |
| 37 | 143 | `PIPE_CAP_CUBE_MAP_ARRAY` | `1` | 立方图数组 | `GL_ARB_texture_cube_map_array`（GL 4.0） |
| 38 | 145 | `PIPE_CAP_TEXTURE_BUFFER_OBJECTS` | `1` | 纹理缓冲对象（TBO） | GL 3.1 / GLES 3.1 |
| 39 | 147 | `PIPE_CAP_TEXTURE_BUFFER_OFFSET_ALIGNMENT` | `256` | TBO 视图偏移对齐 | `GL_TEXTURE_BUFFER_OFFSET_ALIGNMENT` |
| 40 | 149 | `PIPE_CAP_MAX_TEXEL_BUFFER_ELEMENTS_UINT` | `65536` | TBO 最大元素数 | `GL_MAX_TEXTURE_BUFFER_SIZE` |
| 41 | 151 | `PIPE_CAP_MAX_VIEWPORTS` | `1` | 视口数组只有 1 个 | `GL_ARB_viewport_array`（GL 4.1）为 0 级支持 |
| 42 | 153 | `PIPE_CAP_ENDIANNESS` | `PIPE_ENDIAN_NATIVE` | 本机端序 | 非扩展门控，Mesa 内部约定 |
| 43 | 155 | `PIPE_CAP_MIXED_FRAMEBUFFER_SIZES` | `1` | 允许 FB 附件尺寸不同 | GL 3.0 |
| 44 | 157 | `PIPE_CAP_MAX_GEOMETRY_OUTPUT_VERTICES` | `256` | GS 单次输出顶点上限 | `GL_MAX_GEOMETRY_OUTPUT_VERTICES`（GL 3.2） |
| 45 | 159 | `PIPE_CAP_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS` | `1024` | GS 总输出分量上限 | `GL_MAX_GEOMETRY_TOTAL_OUTPUT_COMPONENTS` |
| 46 | 161 | `PIPE_CAP_TEXTURE_MULTISAMPLE` | `1` | 多重采样纹理 | GL 3.2 / GLES 3.1；与 §2.3.5 的 1/2/4 采样数支持配套 |
| 47 | 163 | `PIPE_CAP_FAKE_SW_MSAA` | `!mwv207_get_param(pscreen, PIPE_CAP_TEXTURE_MULTISAMPLE)` = **`0`** | 是否需要软件 MSAA 回退 | 因硬件/驱动已声明 MSAA 纹理而恒为 0 |
| 48 | 165 | `PIPE_CAP_MIN_TEXTURE_GATHER_OFFSET` | `-8` | gather 偏移下限 | `GL_ARB_texture_gather` |
| 49 | 167 | `PIPE_CAP_MAX_TEXTURE_GATHER_OFFSET` | `7` | gather 偏移上限 | 同上 |
| 50 | 169 | `PIPE_CAP_VENDOR_ID` | `0x0731` | PCI 厂商 ID | Mesa 用于 PCI 设备信息/GL_VENDOR 辅助 |
| 51 | 171 | `PIPE_CAP_DEVICE_ID` | `0x9100` | PCI 设备 ID | 同上（对应 JM9100） |
| 52 | 173 | `PIPE_CAP_ACCELERATED` | `1` | 硬件加速驱动 | EGL/GLX 判定，软件渲染回退的门控 |
| 53 | 175 | `PIPE_CAP_VIDEO_MEMORY` | `1024 << 20`（1 GiB） | 上报显存大小 | `GL_ATI_meminfo` / 分配策略 |
| 54 | 177 | `PIPE_CAP_UMA` | `1` | 统一内存架构 | 影响 Mesa 的资源分配/迁移策略 |
| 55 | 179 | `PIPE_CAP_MAX_VERTEX_ATTRIB_STRIDE` | `2048` | 顶点属性步长上限 | `GL_MAX_VERTEX_ATTRIB_STRIDE`（GL 4.4） |
| 56 | 181 | `PIPE_CAP_TEXTURE_HALF_FLOAT_LINEAR` | `1` | half-float 纹理可线性过滤 | GLES 3.0 要求 |
| 57 | 183 | `PIPE_CAP_SHAREABLE_SHADERS` | `1` | 着色器可跨上下文共享 | 内部（GL 4.0 语义）；具体调用点未确认 |
| 58 | 185 | `PIPE_CAP_FS_POSITION_IS_SYSVAL` | `1` | `gl_FragCoord` 由系统值提供 | Mesa ST 内部 |
| 59 | 187 | `PIPE_CAP_FS_FACE_IS_INTEGER_SYSVAL` | `1` | `gl_FrontFacing` 为整数系统值 | Mesa ST 内部 |
| 60 | 189 | `PIPE_CAP_SHADER_BUFFER_OFFSET_ALIGNMENT` | `256` | SSBO 偏移对齐 | `GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT`（GL 4.3） |
| 61 | 191 | `PIPE_CAP_GENERATE_MIPMAP` | `1` | 支持生成 mipmap | GL 3.0 / GLES 3.0 |
| 62 | 193 | `PIPE_CAP_VIEWPORT_SUBPIXEL_BITS` | `2` | 视口子像素精度 | GLES 视口/光栅精度查询 |
| 63 | 195 | `PIPE_CAP_RASTERIZER_SUBPIXEL_BITS` | `4` | 光栅化子像素精度 | 同上 |
| 64 | 197 | `PIPE_CAP_NATIVE_FENCE_FD` | `1` | 支持 native fence fd | `EGL_ANDROID_native_fence_sync`；配套 `mwv207_context.c:154-164`、`mwv207_resource.c` 的 dmabuf 导入 |
| 65 | 199 | `PIPE_CAP_ALLOW_MAPPED_BUFFERS_DURING_EXECUTION` | `1` | 绘制期间允许映射缓冲 | GL 3.0 |
| 66 | 201 | `PIPE_CAP_MAX_GS_INVOCATIONS` | `32` | GS 调用次数上限 | `GL_ARB_gpu_shader5`（GL 4.0） |
| 67 | 203 | `PIPE_CAP_MAX_SHADER_BUFFER_SIZE_UINT` | `1 << 16`（65536） | SSBO 单块最大字节 | `GL_MAX_SHADER_STORAGE_BLOCK_SIZE` |
| 68 | 205 | `PIPE_CAP_MAX_COMBINED_SHADER_BUFFERS` | `4` | 全部阶段合计 SSBO 数 | `GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS` |
| 69 | 207 | `PIPE_CAP_MAX_VERTEX_ELEMENT_SRC_OFFSET` | `2047` | 顶点属性源偏移上限 | 顶点装配内部校验 |
| 70 | 209 | `PIPE_CAP_DEST_SURFACE_SRGB_CONTROL` | `1` | 目标表面可切 sRGB 编码 | `GL_ARB_framebuffer_sRGB`（GL 3.0）；`mwv207_surface.c` 里 `srgb` 位（`pe_config` bit31）实现 |
| 71 | 211 | `PIPE_CAP_MAX_VARYINGS` | `8` | 最大 varying 数 | `GL_MAX_VARYING_VECTORS` 相关（GLES 2.0 下限 8） |
| 72 | 213 | `PIPE_CAP_THROTTLE` | `1` | 驱动支持节流 | 具体门控未确认 |
| 73 | 215 | `PIPE_CAP_DMABUF` | `1` | 支持 dma-buf 导入/导出 | `EGL_EXT_image_dma_buf_import`；`mwv207_resource.c:587-646` 的 modifier 查询/创建 |
| 74 | 217 | `PIPE_CAP_FRAGMENT_SHADER_TEXTURE_LOD` | `1` | 片元着色器可显式 LOD 采样 | `GL_ARB_fragment_shader_texture_lod`（GLES 2.0 扩展） |
| 75 | 219 | `PIPE_CAP_FRAGMENT_SHADER_DERIVATIVES` | `1` | 片元着色器可用导数 | GLES 2.0 `OES_standard_derivatives` |
| 76 | 221 | `PIPE_CAP_PREFER_IMM_ARRAYS_AS_CONSTBUF` | `1` | 立即数数组按常量缓冲处理 | Mesa 编译器内部偏好 |
| 77 | 223 | `PIPE_CAP_FLATSHADE` | `1` | flat 插值 | GL 3.0 / GLES 3.0 |
| 78 | 225 | `PIPE_CAP_ALPHA_TEST` | `1` | alpha 测试 | GL 2.0 / GLES 2.0 |
| 79 | 227 | `PIPE_CAP_POINT_SIZE_FIXED` | `1` | 点大小固定 | 具体门控未确认 |
| 80 | 229 | `PIPE_CAP_TWO_SIDED_COLOR` | `1` | 双面光照 | GL 2.0 |
| 81 | 231 | `PIPE_CAP_CLIP_PLANES` | `1` | 用户裁剪面 | `GL_ARB_cull_distance` 前的裁剪面支持 |
| 82 | 233 | `PIPE_CAP_MAX_VERTEX_BUFFERS` | `16` | 顶点缓冲绑定数 | `GL_MAX_VERTEX_ATTRIB_BINDINGS`（GL 4.3） |
| 83 | 235 | `PIPE_CAP_TEXTURE_TRANSFER_MODES` | `PIPE_TEXTURE_TRANSFER_DEFAULT` | 纹理传输模式 | ST 选择 map/blit 传输路径 |
| 84 | 238 | `PIPE_CAP_FRAMEBUFFER_NO_ATTACHMENT` | `1` | 允许无附件 FBO | `GL_ARB_framebuffer_no_attachments`（GL 4.3） |
| 85 | 240 | `PIPE_CAP_MAX_SHADER_PATCH_VARYINGS` | `32` | patch varying 上限 | 细分相关（具体门控未确认） |
| 86 | 243 | `PIPE_CAP_GLSL_FEATURE_LEVEL` | `400` | GLSL 最高版本 | **决定 GL 4.0** |
| 87 | 244 | `PIPE_CAP_GLSL_FEATURE_LEVEL_COMPATIBILITY` | `400` | 兼容 profile 的 GLSL 版本 | GL 4.0 compatibility |
| 88 | 245 | `PIPE_CAP_ESSL_FEATURE_LEVEL` | `400` | ESSL 版本 | **GLES 3.2 需要 ≥ 320 的 ESSL 版本上报**（这里给 400） |
| 89 | 248 | `PIPE_CAP_MAX_DUAL_SOURCE_RENDER_TARGETS` | `1` | 双源混合 RT 数 | `GL_ARB_blend_func_extended`（GL 3.3） |
| 90 | 250 | `PIPE_CAP_QUERY_TIMESTAMP` | `1` | 时间戳查询 | `GL_ARB_timer_query` |
| 91 | 253 | `PIPE_CAP_TEXTURE_QUERY_LOD` | `1` | `textureQueryLod` | `GL_ARB_texture_query_lod` |
| 92 | 255 | `PIPE_CAP_SAMPLE_SHADING` | `1` | 逐采样着色 | `GL_ARB_sample_shading`（GL 4.0） |
| 93 | 257 | `PIPE_CAP_DRAW_INDIRECT` | `1` | 间接绘制 | GL 4.0 |
| 94 | 259 | `PIPE_CAP_DOUBLES` | `1` | fp64 | `GL_ARB_gpu_shader_fp64`（GL 4.0） |
| 95 | 261 | `PIPE_CAP_GLSL_TESS_LEVELS_AS_INPUTS` | `1` | tess level 可作为 patch 输入 | GL 4.0 tessellation |
| 96 | 263 | `PIPE_CAP_INT64` | `1` | 64 位整数 | `GL_ARB_gpu_shader_int64`（GL 4.0） |
| 97 | 266 | `PIPE_CAP_COMPUTE` | `1` | 计算着色器 | GL 4.3 / GLES 3.1；配套 §2.3.4 |
| 98 | 268 | `PIPE_CAP_MAX_TEXTURE_GATHER_COMPONENTS` | `4` | gather 分量数 | `GL_ARB_texture_gather` |
| 99 | 271 | `PIPE_CAP_FBFETCH` | `0` | **不支持** framebuffer fetch | `GL_EXT_shader_framebuffer_fetch` 门控关闭 |
| 100 | 273 | `PIPE_CAP_ROBUST_BUFFER_ACCESS_BEHAVIOR` | `1` | 越界访问有确定行为 | `GL_KHR_robust_buffer_access_behavior` / `GL_KHR_robustness`；注释 `mwv207_screen.c:274-278` 说明由 `nir_lower_robust_access()` 在 `mwv207_compile_shader()` 中钳制 SSBO 偏移保证，并称 KHR_robustness 是 native GLES 3.2 的**最后一道门控** |
| 101 | 280 | `PIPE_CAP_COPY_BETWEEN_COMPRESSED_AND_PLAIN_FORMATS` | `1` | 压缩/非压缩格式互拷 | `GL_ARB_copy_image` 相关 |
| 102 | 283 | `PIPE_CAP_SUPPORTED_PRIM_MODES` | 位图（见下） | 支持的图元模式 | Mesa 校验 `glDrawArrays` 的 mode |
| 103 | 295 | `PIPE_CAP_SUPPORTED_PRIM_MODES_WITH_RESTART` | 位图（同上） | 允许图元重启的模式集合 | 同上 |
| 104 | 307 | `PIPE_CAP_NIR_IMAGES_AS_DEREF` | `1` | NIR 图像用 deref 表示 | Mesa NIR 内部约定 |
| 105 | 309 | `PIPE_CAP_GL_BEGIN_END_BUFFER_SIZE` | `512 * 1024` | `glBegin/glEnd` 兼容缓冲大小 | GL 兼容 profile |
| 106 | 311 | `PIPE_CAP_ALLOW_DYNAMIC_VAO_FASTPATH` | `1` | 允许动态 VAO 快路径 | Mesa 内部优化 |
| 107 | 313 | `PIPE_CAP_PREFER_BACK_BUFFER_REUSE` | `1` | 优选复用后缓冲 | 性能/交换行为 |
| 108 | 315 | `PIPE_CAP_CULL_DISTANCE_NOCOMBINE` | `1` | 裁剪距离不合并 | `GL_ARB_cull_distance`（GL 4.5） |
| 109 | 317 | `PIPE_CAP_GL_SPIRV` | `1` | 支持 SPIR-V | `GL_ARB_gl_spirv`（GL 4.6） |
| 110 | 319 | `PIPE_CAP_TIMER_RESOLUTION` | `DIV_ROUND_UP(1000000000ull, 50000000ull)` = **20** ns | 时间戳分辨率 | `GL_ARB_timer_query` 的分辨率上报 |
| 111 | 321 | `PIPE_CAP_PCI_GROUP` | `0` | PCI 组 | 未暴露 |
| 112 | 322 | `PIPE_CAP_PCI_BUS` | `0` | PCI 总线号 | 未暴露 |
| 113 | 323 | `PIPE_CAP_PCI_DEVICE` | `0` | PCI 设备号 | 未暴露 |
| 114 | 324 | `PIPE_CAP_PCI_FUNCTION` | `0` | PCI 功能号 | 未暴露 |
| 115 | 326 | `PIPE_CAP_ANISOTROPIC_FILTER` | `1` | 各向异性过滤 | `EXT_texture_filter_anisotropic`；配合 §2.3.2 的 `PIPE_CAPF_MAX_TEXTURE_ANISOTROPY = 16.0` |

`SUPPORTED_PRIM_MODES` / `..._WITH_RESTART`（`mwv207_screen.c:283-306`，两处返回值**完全相同**）置位：

```c
/* mwv207_screen.c:284-294 —— ..._WITH_RESTART 在 296-306 逐位重复同一集合 */
1 << MESA_PRIM_POINTS | 1 << MESA_PRIM_LINES | 1 << MESA_PRIM_LINE_STRIP |
1 << MESA_PRIM_TRIANGLES | 1 << MESA_PRIM_TRIANGLE_STRIP | 1 << MESA_PRIM_TRIANGLE_FAN |
1 << MESA_PRIM_LINES_ADJACENCY | 1 << MESA_PRIM_LINE_STRIP_ADJACENCY |
1 << MESA_PRIM_TRIANGLES_ADJACENCY | 1 << MESA_PRIM_TRIANGLE_STRIP_ADJACENCY |
1 << MESA_PRIM_PATCHES
```
#### 2.3.2 `PIPE_CAPF_*` —— 共 15 个 case（`mwv207_screen.c:334-363`）

| # | 行 | `PIPE_CAPF_*` | 返回值 | 语义 |
|---|---|---|---|---|
| 1 | 337 | `PIPE_CAPF_MAX_LINE_WIDTH` | `8.0f` | `GL_ALIASED_LINE_WIDTH_RANGE` 上限 |
| 2 | 338 | `PIPE_CAPF_MAX_LINE_WIDTH_AA` | `8.0f` | `GL_SMOOTH_LINE_WIDTH_RANGE` 上限 |
| 3 | 340 | `PIPE_CAPF_MAX_POINT_SIZE` | `128.0f` | 点大小上限 |
| 4 | 341 | `PIPE_CAPF_MAX_POINT_SIZE_AA` | `128.0f` | 抗锯齿点大小上限 |
| 5 | 343 | `PIPE_CAPF_MAX_TEXTURE_ANISOTROPY` | `16.0f` | `GL_MAX_TEXTURE_MAX_ANISOTROPY`（对应 §2.3.1 #115） |
| 6 | 345 | `PIPE_CAPF_MAX_TEXTURE_LOD_BIAS` | `2.0f` | `GL_MAX_TEXTURE_LOD_BIAS` |
| 7 | 347 | `PIPE_CAPF_MIN_CONSERVATIVE_RASTER_DILATE` | `0.0f` | 保守光栅化膨胀下界（**驱动未声明 `PIPE_CAP_CONSERVATIVE_RASTERIZATION`，此值实际不被使用**，未确认） |
| 8 | 348 | `PIPE_CAPF_MAX_CONSERVATIVE_RASTER_DILATE` | `0.0f` | 上界，同上 |
| 9 | 349 | `PIPE_CAPF_CONSERVATIVE_RASTER_DILATE_GRANULARITY` | `0.0f` | 粒度，同上 |
| 10 | 351 | `PIPE_CAPF_MIN_LINE_WIDTH` | `1.0f` | 线宽下界 |
| 11 | 352 | `PIPE_CAPF_MIN_LINE_WIDTH_AA` | `1.0f` | 平滑线宽下界 |
| 12 | 353 | `PIPE_CAPF_MIN_POINT_SIZE` | `1.0f` | 点大小下界 |
| 13 | 354 | `PIPE_CAPF_MIN_POINT_SIZE_AA` | `1.0f` | 平滑点大小下界 |
| 14 | 356 | `PIPE_CAPF_POINT_SIZE_GRANULARITY` | `0.1f` | 点大小粒度 |
| 15 | 357 | `PIPE_CAPF_LINE_WIDTH_GRANULARITY` | `0.1f` | 线宽粒度 |

#### 2.3.3 `PIPE_SHADER_CAP_*` —— 共 28 个 case（`mwv207_screen.c:366-470`）

| # | 行 | `PIPE_SHADER_CAP_*` | 返回值 | 语义 | 消费方 |
|---|---|---|---|---|---|
| 1 | 371 | `PIPE_SHADER_CAP_MAX_INSTRUCTIONS` | `INT_MAX`（VS/FS/GS/TCS/TES/CS），其他 0 | 指令数上限 | 着色器编译/校验 |
| 2 | 372 | `PIPE_SHADER_CAP_MAX_ALU_INSTRUCTIONS` | 同上 | ALU 指令上限 | 同上 |
| 3 | 373 | `PIPE_SHADER_CAP_MAX_TEX_INSTRUCTIONS` | 同上 | 纹理指令上限 | 同上 |
| 4 | 374 | `PIPE_SHADER_CAP_MAX_TEX_INDIRECTIONS` | 同上 | 间接纹理取上限 | 同上 |
| 5 | 375 | `PIPE_SHADER_CAP_MAX_CONTROL_FLOW_DEPTH` | 同上 | 控制流嵌套深度 | 同上 |
| 6 | 387 | `PIPE_SHADER_CAP_MAX_INPUTS` | `16`（六种阶段），其他 0 | 输入上限 | `GL_MAX_*_INPUT_COMPONENTS` 类 |
| 7 | 399 | `PIPE_SHADER_CAP_MAX_OUTPUTS` | VS `17`；FS/CS `4`；GS/TCS/TES `16` | 输出上限 | `GL_MAX_*_OUTPUT_COMPONENTS` |
| 8 | 413 | `PIPE_SHADER_CAP_MAX_CONST_BUFFER0_SIZE` | `65536` | 常量缓冲 0 大小 | `GL_MAX_*_UNIFORM_BLOCK_SIZE` |
| 9 | 415 | `PIPE_SHADER_CAP_MAX_CONST_BUFFERS` | `16` | 常量缓冲数 | `GL_MAX_*_UNIFORM_BLOCKS` |
| 10 | 417 | `PIPE_SHADER_CAP_MAX_TEMPS` | `64` | 临时寄存器数 | GLSL 编译器 |
| 11 | 419 | `PIPE_SHADER_CAP_SUPPORTED_IRS` | `1<<PIPE_SHADER_IR_NIR \| 1<<PIPE_SHADER_IR_TGSI \| 1<<PIPE_SHADER_IR_NIR_SERIALIZED` | 支持的中间表示 | ST 选择着色器提交格式 |
| 12 | 423 | `PIPE_SHADER_CAP_INDIRECT_CONST_ADDR` | `1` | 常量缓冲可间接寻址 | GLSL 动态索引 UBO |
| 13 | 425 | `PIPE_SHADER_CAP_INDIRECT_INPUT_ADDR` | 仅 TCS/TES 返回 `1`，其他 0 | 输入可间接寻址 | 各阶段 GLSL 动态索引 |
| 14 | 426 | `PIPE_SHADER_CAP_INDIRECT_OUTPUT_ADDR` | 同上 | 输出可间接寻址 | 同上 |
| 15 | 427 | `PIPE_SHADER_CAP_INDIRECT_TEMP_ADDR` | 同上 | 临时变量可间接寻址 | 同上 |
| 16 | 437 | `PIPE_SHADER_CAP_INTEGERS` | `1` | 支持整数运算 | GL 3.0 / GLES 3.0 |
| 17 | 439 | `PIPE_SHADER_CAP_MAX_TEXTURE_SAMPLERS` | `16` | 采样器数 | `GL_MAX_TEXTURE_IMAGE_UNITS` |
| 18 | 441 | `PIPE_SHADER_CAP_MAX_SAMPLER_VIEWS` | `MESA_SHADER_KERNEL` 时 `128`，否则 `16` | 采样器视图数 | OpenCL 内核路径专用 128 |
| 19 | 449 | `PIPE_SHADER_CAP_CONT_SUPPORTED` | `1` | 支持 continue | GLSL 循环 |
| 20 | 451 | `PIPE_SHADER_CAP_SUBROUTINES` | `0` | **不支持**子例程 | `GL_ARB_shader_subroutine` 门控关闭 |
| 21 | 452 | `PIPE_SHADER_CAP_INT64_ATOMICS` | `0` | **不支持** 64 位原子 | `GL_NV_shader_atomic_int64` 关闭 |
| 22 | 453 | `PIPE_SHADER_CAP_FP16` | `0` | **不支持** fp16 | `GL_AMD_gpu_shader_half_float` 等关闭（与 §2.3.1 #56 的 half-float 纹理过滤不矛盾） |
| 23 | 454 | `PIPE_SHADER_CAP_TGSI_SQRT_SUPPORTED` | `0` | TGSI 无 sqrt | 老式 TGSI 路径 |
| 24 | 455 | `PIPE_SHADER_CAP_TGSI_ANY_INOUT_DECL_RANGE` | `0` | 不支持任意 inout 声明范围 | 同上 |
| 25 | 457 | `PIPE_SHADER_CAP_MAX_SHADER_BUFFERS` | `0x4` = 4 | SSBO 数 | `GL_MAX_*_SHADER_STORAGE_BLOCKS` |
| 26 | 459 | `PIPE_SHADER_CAP_MAX_SHADER_IMAGES` | `8` | 图像单元数 | `GL_MAX_*_IMAGE_UNITS`（GL 4.2） |
| 27 | 461 | `PIPE_SHADER_CAP_MAX_HW_ATOMIC_COUNTERS` | `128` | 硬件原子计数器数 | `GL_MAX_*_ATOMIC_COUNTERS` |
| 28 | 463 | `PIPE_SHADER_CAP_MAX_HW_ATOMIC_COUNTER_BUFFERS` | `16` | 原子计数器缓冲数 | `GL_MAX_*_ATOMIC_COUNTER_BUFFERS` |

注意 `mwv207_screen.c:468-469` 在 `get_shader_param` 的 `switch` **之后**还有一句 `debug_printf("unknown paramf %d", param); return 0;` —— 因为 default 分支（465-466）已经 `return 0`，这两行是**不可达的死代码**。

#### 2.3.4 `PIPE_COMPUTE_CAP_*` —— 共 17 个 case（`mwv207_screen.c:473-519`）

`RET` 宏用法（`include/mwv207_screen.h:33-37`）等价于「写 `ret` 指向的数组 + 返回字节数」。全部条目：

| # | 行 | `PIPE_COMPUTE_CAP_*` | 返回值 | 语义 |
|---|---|---|---|---|
| 1 | 479 | `PIPE_COMPUTE_CAP_IR_TARGET` | 字符串 `"mwv207-nir"`，返回 `(strlen+2)*sizeof(char)` | 计算 IR 目标名 |
| 2 | 485 | `PIPE_COMPUTE_CAP_MAX_GRID_SIZE` | `{65535, 65535, 65535}` | 网格三维上限 |
| 3 | 487 | `PIPE_COMPUTE_CAP_MAX_BLOCK_SIZE` | `{1024, 1024, 1024}` | work-group 三维上限 |
| 4 | 489 | `PIPE_COMPUTE_CAP_MAX_THREADS_PER_BLOCK` | `1024` | 每组线程数（OpenCL 下限 1，此值偏保守） |
| 5 | 491 | `PIPE_COMPUTE_CAP_MAX_LOCAL_SIZE` | `32768` | local memory 字节 |
| 6 | 493 | `PIPE_COMPUTE_CAP_GRID_DIMENSION` | `3` | 维度 |
| 7 | 495 | `PIPE_COMPUTE_CAP_MAX_GLOBAL_SIZE` | `0x10000000`（256 MiB） | 全局内存 |
| 8 | 497 | `PIPE_COMPUTE_CAP_MAX_MEM_ALLOC_SIZE` | `0x8000000`（128 MiB） | 单次分配上限 |
| 9 | 499 | `PIPE_COMPUTE_CAP_MAX_PRIVATE_SIZE` | `4096` | private 内存 |
| 10 | 500 | `PIPE_COMPUTE_CAP_MAX_INPUT_SIZE` | `4096` | 内核参数大小 |
| 11 | 502 | `PIPE_COMPUTE_CAP_IMAGES_SUPPORTED` | `0` | **不支持**图像（与 §2.3.3 #26 的 `MAX_SHADER_IMAGES = 8` 是两条独立路径：前者是计算内核图像，后者是 GL 图形图像） |
| 12 | 504 | `PIPE_COMPUTE_CAP_MAX_VARIABLE_THREADS_PER_BLOCK` | `0` | 不支持可变线程数 |
| 13 | 506 | `PIPE_COMPUTE_CAP_SUBGROUP_SIZES` | `{64}` | 子组大小 64 |
| 14 | 508 | `PIPE_COMPUTE_CAP_MAX_COMPUTE_UNITS` | `2` | 计算单元数 2 |
| 15 | 510 | `PIPE_COMPUTE_CAP_MAX_CLOCK_FREQUENCY` | `50`（MHz） | 时钟频率 |
| 16 | 512 | `PIPE_COMPUTE_CAP_ADDRESS_BITS` | `32` | 地址位宽 |
| 17 | 514 | `PIPE_COMPUTE_CAP_MAX_SUBGROUPS` | `0` | 子组数（0 = 未声明/不适用） |

`switch` 之外 `mwv207_screen.c:518` 只有一句 `return 0;`，即未知 compute cap 返回 0 字节。

#### 2.3.5 采样数与格式支持判定链

采样数支持（`mwv207_screen.c:522-536`）：

```c
/* mwv207_screen.c:524-536  —— 注意 switch 里没有 case 0 */
switch (sample_count) {
case 1: case 2: case 4:      return true;
case 8: case 16: case 32: case 64:
default:                     return false;
}
```

* 明确支持 1 / 2 / 4 倍 MSAA；8/16/32/64 显式落到 `false`。
* **`sample_count == 0` 也会走 `default` 返回 `false`**：Mesa 在 `is_format_supported(..., sample_count = 0, ...)` 询问「非 MSAA 是否支持」时会被判为不支持——这正是 `ANALYSIS-REPORT.md` 的 **D4**（报告行号 `mwv207_screen.c:523-538`，本快照行号 `521-536`），在本 24.1.2 快照中**仍然存在**。

格式支持判定链（`mwv207_screen.c:539-592`）：

```c
/* mwv207_screen.c:539-592 摘要 */
if (format == PIPE_FORMAT_NONE)                 /* :546-547 */
   return mwv207_sample_count_flags(sample_count);
if (sample_count > 1) {                         /* :549-561 */
   if (!mwv207_sample_count_flags(sample_count)) return false;
   if (bind & PIPE_BIND_DEPTH_STENCIL && format == PIPE_FORMAT_Z32_FLOAT) return false;
   if (bind & PIPE_BIND_RENDER_TARGET && util_format_is_float(format))    return false;
}
uint32_t support = mwv207_get_format_desc(format);   /* :564 → mwv207_format.c:652-682 */
if (target == PIPE_BUFFER) {                          /* :566-568 只校验 VERTEX_BUFFER */
   if (bind & PIPE_BIND_VERTEX_BUFFER && !(support & PIPE_BIND_VERTEX_BUFFER)) return false;
} else {                                              /* :571-581 逐位与 get_format_desc 的位掩码比对 */
   ...RENDER_TARGET / BLENDABLE / SAMPLER_VIEW / DEPTH_STENCIL...
}
if (util_format_is_compressed(format))                /* :584-589 */
   if (desc->layout == UTIL_FORMAT_LAYOUT_BPTC) return false;
return true;                                          /* :591 */
```

要点：

1. `PIPE_BUFFER` 目标**只**检查 `PIPE_BIND_VERTEX_BUFFER`，其余 bind（如 SSBO/TBO 的 `PIPE_BIND_SHADER_BUFFER`）不校验——见 §6.5 的 `get_format_desc` 覆盖范围。
2. 压缩格式只排除 **BPTC**（`mwv207_format.c` 只声明了 ETC/S3TC/DXT 与 ASTC 相关码值），其余压缩格式依赖 `mwv207_get_format_desc()` 的 `support` 位。
3. `util_format_is_float()` 的 MSAA RT 一律拒绝（`mwv207_screen.c:558-561`），与硬件 8/16/32/64 MSAA 缺失叠加后，MSAA 支持面 = {非 float 的 RT、非 Z32_FLOAT 的深度} × {1,2,4}。

---

## 3. `pipe_context` 层

### 3.1 `struct mwv207_context` 字段表

定义在 `include/mwv207_context.h:109-168`。按功能分组：
| 组 | 字段 | 类型 | 作用 | 行 |
|---|---|---|---|---|
| 头部 | `base` | `struct pipe_context` | 对外上下文，必须首位 | `mwv207_context.h:110` |
| 后端 | `ws` | `struct mwv207_winsys *` | winsys 函数表 | `:112` |
| 命令流 | `cmd` | `struct mwv207_cmd *` | 主命令缓冲（`mwv207_create_cmd(ctx)`） | `:114` |
| 命令流 | `dma` | `struct mwv207_cmd *` | DMA/BLT 命令缓冲（`mwv207_dma()` 提交至此） | `:115` |
| 顶点 | `num_vertex_buffers` | `uint32_t` | 已绑定 VB 数 | `:117` |
| 顶点 | `element_state` | `struct mwv207_elements_state *` | 当前顶点元素状态 | `:118` |
| 顶点 | `vertex_buffer[]` | `struct pipe_vertex_buffer[PIPE_MAX_ATTRIBS]` | VB 数组 | `:119` |
| 常量 | `cb[][]` | `struct pipe_constant_buffer[PIPE_SHADER_TYPES][PIPE_MAX_CONSTANT_BUFFERS]` | 每阶段常量缓冲 | `:120` |
| 状态 | `viewport` / `scissor` | `pipe_viewport_state` / `pipe_scissor_state` | 单一视口/裁剪矩形（`MAX_VIEWPORTS = 1`） | `:122-123` |
| 状态 | `fb_state` | `struct pipe_framebuffer_state` | 帧缓冲状态 | `:125` |
| 状态 | `rast_state` / `zsa` / `stencil_ref` / `blend_state` / `blend_color` | 驱动/pipe 状态对象 | 光栅/深度模板/模板参考/混合/混合常量 | `:127-131` |
| 原子 | `atomic_bo[16]` / `atomic_mask` | `struct pipe_shader_buffer[16]` / `uint32_t` | 硬件原子计数器缓冲与掩码 | `:133-134` |
| SSBO | `ssbo[][]` / `ssbo_mask[]` | `pipe_shader_buffer[PIPE_SHADER_TYPES][PIPE_MAX_SHADER_BUFFERS]` / `uint32_t[]` | 着色器存储缓冲 | `:135-136` |
| 图像 | `ssimage[][]` / `num_images[]` | `pipe_image_view[PIPE_SHADER_TYPES][PIPE_MAX_SHADER_IMAGES]` / `uint32_t[]` | 着色器图像 | `:137-138` |
| 计算 | `local_memory` | `struct pipe_shader_buffer` | 局部内存（计算 launch 用） | `:140` |
| 采样 | `sample_mask` | `uint32_t` | 采样掩码（`context_create` 初始化为 `0xffff`） | `:142` |
| 采样器 | `num_samplers[]` / `num_sampler_views[]` | `uint32_t[PIPE_SHADER_TYPES]` | 每阶段计数 | `:143-144` |
| 采样器 | `samplers[][]` / `sampler_views[][]` | `mwv207_sampler *[PIPE_SHADER_TYPES][PIPE_MAX_SAMPLERS]` / `pipe_sampler_view *[PIPE_SHADER_TYPES][PIPE_MAX_SHADER_SAMPLER_VIEWS]` | 采样器与视图绑定 | `:146-147` |
| 着色器 | `shader_states[]` | `struct mwv207_shader_state *[PIPE_SHADER_TYPES]` | 各阶段着色器状态 | `:149` |
| 细分 | `tess_outer_level[4]` / `tess_inner_level[2]` | `float` | 细分级别（`set_tess_state` 写入） | `:151-152` |
| 脏位 | `dirty` | `uint32_t` | `mwv207_dirty` 位集 | `:154` |
| 同步 | `in_fence_fd` | `int` | 外部 fence fd（初始 `-1`） | `:155` |
| 变换反馈 | `stream_out` | `struct mwv207_stream_output` | SO 状态 | `:157` |
| 查询 | `query_info` | `struct mwv207_query_info *` | 查询状态 | `:159` |
| 辅助 | `blitter` | `struct blitter_context *` | `util_blitter_create()` | `:161` |
| 辅助 | `primconvert` | `struct primconvert_context *` | `util_primconvert_create()` | `:162` |
| 辅助 | `transfer_pool` | `struct slab_child_pool` | `mwv207_transfer` 子池（`slab_create_child`） | `:163` |
| 辅助 | `info` | `struct mwv207_pipe_info` | 硬件侧共享信息（见下） | `:165` |
| 计算 | `global_bindings` | `struct mwv207_global_bindings_state` | 全局绑定表 | `:167` |
| 访问器 | `mwv207_context()` | inline | `(struct mwv207_context *)context` | `:170-174` |

`struct mwv207_pipe_info`（`include/mwv207_context.h:85-107`）与硬件描述符强相关：

| 字段 | 含义 | 相关位置 |
|---|---|---|
| `tex_128bit` | 是否有 128 位纹素纹理需要拆分 | `mwv207_sampler.c`、`mwv207_surface.c:232-235` |
| `tg4_lower` | textureGather 是否走下降级路径 | 采样器 |
| `tex_record[PIPE_SHADER_TYPES][PIPE_MAX_SHADER_SAMPLER_VIEWS]` | 每阶段每个采样器视图的纹理记录 | `mwv207_sampler.c` |
| `pixel_128bit` | 目标表面是否为 128 位（RG/BA 拆分） | `mwv207_surface.c:60-64`、`mwv207_context.c:89-90` |
| `pixel_record[PIPE_MAX_COLOR_BUFS]` | 每个 color buf 的记录号 | `mwv207_surface.c:321-326` |
| `pixel_num` | 实际像素记录数（= `pixle_128_offset`） | `mwv207_surface.c:351` |
| `z32_shadow` | Z32 阴影/回读路径标志 | 深度处理 |
| `texture_dirty` | 纹理状态需重发 | 采样器 |
| `linear_outmode` | 输出为线性（任一 cbuf 非 SUPERTILED 即置位） | `mwv207_surface.c:291,303-304` |
| `frag_rgba_swap` | 相邻 cbuf swizzle 不同，需片元 RGBA 交换 | `mwv207_surface.c:337-342` |
| `swizzle_record[][4]` | 每 cbuf 的 4 路 swizzle | `mwv207_surface.c:331-334` |
| `cbufs_map[]` | RT 逻辑序号 → 硬件像素记录号 | `mwv207_surface.c:294-299,325` |
| `has_render_arrary` | 存在分层渲染目标数组 | `mwv207_surface.c:328-329` |
| `vertices_per_patch` | patch 顶点数 | 细分 |
| `alloc_page[]` / `max_thread[]` | 每阶段页分配/最大线程 | 计算/寄存器分配 |
| `patchs[2]` / `grid[3]` | patch 与网格参数 | 细分/计算 |
| `subsamplez_dirty` | 深度采样数变化需重发 | `mwv207_surface.c:344-351` |
| `rast_samples` | 光栅采样数 | `mwv207_surface.c:306,344-349` |
| `dsbi` | `struct mwv207_dual_source_blend_info`（`include/mwv207_context.h:78-83`） | 双源混合 |

### 3.2 `pipe_context` 虚表映射

`mwv207_context.c` 只装配 5 个回调，**其余全部由各子模块的 `mwv207_context_*_init()` 装配**（这些函数在 `mwv207_context_create()` 中依次调用，行号见下）。

| `pipe_context` 成员 | 驱动函数 | 赋值位置 | 装配入口（`mwv207_context_create` 调用行） |
|---|---|---|---|
| `destroy` | `mwv207_context_destroy` | `mwv207_context.c:192` | 直接写在 `context_create` 内 |
| `draw_vbo` | `mwv207_draw_vbo` | `mwv207_context.c:208` | 直接 |
| `flush` | `mwv207_flush` | `mwv207_context.c:209` | 直接 |
| `create_fence_fd` | `mwv207_create_fence_fd` | `mwv207_context.c:210` | 直接 |
| `fence_server_sync` | `mwv207_fence_server_sync` | `mwv207_context.c:211` | 直接 |
| `memory_barrier` | `mwv207_memory_barrier` | `mwv207_context.c:212` | 直接 |
| `create_query` / `destroy_query` / `begin_query` / `end_query` / `get_query_result` / `set_active_query_state` | `mwv207_create_query` / `mwv207_destroy_query` / `mwv207_begin_query` / `mwv207_end_query` / `mwv207_get_query_result` / `mwv207_set_active_query_state` | `mwv207_query.c:333-339` | `mwv207_context_query_init`（调用行 `mwv207_context.c:194`） |
| `buffer_map` / `buffer_unmap` / `texture_map` / `texture_unmap` | `mwv207_transfer_map` / `mwv207_transfer_unmap`（同一对函数复用） | `mwv207_resource.c:574-577` | `mwv207_context_resource_init`（`:196`） |
| `flush_resource` | `mwv207_flush_resource` | `mwv207_resource.c:578` | 同上 |
| `transfer_flush_region` | `mwv207_flush_region` | `mwv207_resource.c:579`（**注意**：`mwv207_blit.c:1235` 又把它赋回 `u_default_transfer_flush_region`，谁最后写谁生效——`resource_init` 先于 `blit_init` 调用） | `:196` → 被 `:206` 覆盖（**疑似缺陷**） |
| `create_surface` / `surface_destroy` / `set_framebuffer_state` | `mwv207_create_surface` / `mwv207_surface_destroy` / `mwv207_set_framebuffer_state` | `mwv207_surface.c:360-362` | `mwv207_context_surface_init`（`:198`） |
| `create_rasterizer_state` / `bind_rasterizer_state` / `delete_rasterizer_state` | `mwv207_create_rasterizer_state` / `mwv207_bind_rasterizer_state` / `mwv207_delete_rasterizer_state` | `mwv207_state.c:468-470` | `mwv207_context_state_init`（`:200`） |
| `create_depth_stencil_alpha_state` / `bind_...` / `delete_...` | 对应 `mwv207_*_depth_stencil_alpha_state` | `mwv207_state.c:472-474` | 同上 |
| `set_clip_state` / `set_polygon_stipple` / `set_viewport_states` / `set_scissor_states` / `set_sample_mask` | 对应 `mwv207_set_clip_state` 等 | `mwv207_state.c:476-480` | 同上 |
| `create_stream_output_target` / `stream_output_target_destroy` / `set_stream_output_targets` | 对应 `mwv207_*` | `mwv207_state.c:483-485` | 同上 |
| `set_stencil_ref` | `mwv207_set_stencil_ref` | `mwv207_state.c:487` | 同上 |
| `set_tess_state` | `mwv207_set_tess_state` | `mwv207_state.c:497` | 同上 |
| `create_vertex_elements_state` / `bind_...` / `delete_...` | `mwv207_create_elements` / `mwv207_bind_elements` / `mwv207_delete_elements` | `mwv207_vertex.c:474-476` | `mwv207_context_state_init` 内调 vertex init（未确认独立入口） |
| `set_vertex_buffers` / `set_constant_buffer` | `mwv207_set_vertex_buffers` / `mwv207_set_constant_buffer` | `mwv207_vertex.c:478-479` | 同上 |
| `create_blend_state` / `bind_blend_state` / `delete_blend_state` / `set_blend_color` | `mwv207_create_blend_state` 等 | `mwv207_blend.c:365-368` | `mwv207_context_state_init` 内（与 state.c 的初始化同批） |
| `create_sampler_state` / `bind_sampler_states` / `delete_sampler_state` / `create_sampler_view` / `set_sampler_views` / `sampler_view_destroy` | `mwv207_create_sampler_state` 等 | `mwv207_sampler.c:1064-1070` | `mwv207_context_sampler_init`（`:204`） |
| `create_vs_state` / `bind_vs_state` / `delete_vs_state`、`create_fs_state` ... `delete_tes_state`、`create_compute_state` / `bind_compute_state` / `delete_compute_state` | `mwv207_create_shader_state` / `mwv207_bind_{vs,fs,gs,tcs,tes}_state` / `mwv207_delete_shader_state`；CS 用 `mwv207_create_cs_state` / `mwv207_bind_cs_state` | `mwv207_shader.c:1172-1194` | `mwv207_context_shader_init`（`:202`） |
| `get_compute_state_info` / `get_compute_state_subgroup_size` | `mwv207_get_compute_state_info` / `mwv207_get_compute_state_subgroup_size` | `mwv207_shader.c:1196-1197` | 同上 |
| `blit` / `clear` / `clear_texture` / `clear_buffer` / `clear_render_target` / `clear_depth_stencil` / `generate_mipmap` / `buffer_subdata` / `texture_subdata` / `resource_copy_region` | `mwv207_blit` / `mwv207_clear` / `u_default_clear_texture` / `u_default_clear_buffer` / `mwv207_clear_render_target` / `mwv207_clear_depth_stencil` / `mwv207_gen_mipmap` / `u_default_buffer_subdata` / `u_default_texture_subdata` / `mwv207_resource_copy_region` | `mwv207_blit.c:1226-1238` | `mwv207_context_blit_init`（`:206`） |
| `set_hw_atomic_buffers` / `set_shader_buffers` / `set_shader_images` / `set_compute_resources` / `set_global_binding` / `launch_grid` | 对应 `mwv207_*` | `mwv207_compute.c:313-318` | `mwv207_context_compute_init`（`:214`） |
| `create_video_codec` / `create_video_buffer` | `mwv207_create_codec` / `mwv207_pipe_video_buffer_create` | `video-codec/mwv207_video_codec.c:144-145` | `mwv207_context_video_codec_init`（`:215`） |
| `stream_uploader`（非纯虚表，辅助） | `u_upload_create_default(&ctx->base)` | `mwv207_context.c:58` | `mwv207_context_helper_init`（`:219` 前） |

> `render_condition`、`emit_string_marker`、`set_index_buffer`、`set_vertex_buffers` 之外的 `...` 回调在本驱动**没有被赋值**（上表之外无其他赋值语句；对全目录做 `grep` 只匹配到 `mwv207_` / `util_` / `u_default_` / `NULL` 四类右值）。

### 3.3 脏位与创建/销毁关键行

`enum mwv207_dirty`（`include/mwv207_context.h:53-76`）：

| 位 | 名称 | 位 | 名称 |
|---|---|---|---|
| `1 << 0` | `MWV207_DIRTY_BLEND` | `1 << 12` | `MWV207_DIRTY_SAMPLER_VIEWS` |
| `1 << 1` | `MWV207_DIRTY_SAMPLERS` | `1 << 13` | `MWV207_DIRTY_CONSTBUF` |
| `1 << 2` | `MWV207_DIRTY_RASTERIZER` | `1 << 14` | `MWV207_DIRTY_VERTEX_BUFFERS` |
| `1 << 3` | `MWV207_DIRTY_ZSA` | `1 << 15` | `MWV207_DIRTY_INDEX_BUFFER` |
| `1 << 4` | `MWV207_DIRTY_VERTEX_ELEMENTS` | `1 << 16` | `MWV207_DIRTY_SHADER` |
| `1 << 6` | `MWV207_DIRTY_BLEND_COLOR` | `1 << 17` | `MWV207_DIRTY_TS` |
| `1 << 7` | `MWV207_DIRTY_STENCIL_REF` | `1 << 18` | `MWV207_DIRTY_TEXTURE_CACHES` |
| `1 << 8` | `MWV207_DIRTY_SAMPLE_MASK` | `1 << 19` | `MWV207_DIRTY_DERIVE_TS` |
| `1 << 9` | `MWV207_DIRTY_VIEWPORT` | `1 << 20` | `MWV207_DIRTY_STREAM_OUT` |
| `1 << 10` | `MWV207_DIRTY_FRAMEBUFFER` | — | `MWV207_DIRTY_ALL = 0xFFFFFFFF` |
| `1 << 11` | `MWV207_DIRTY_SCISSOR` | — | （**没有 `1 << 5`**，位 5 空缺） |

`mwv207_context_create()`（`mwv207_context.c:175-232`）关键行：

| 行 | 内容 |
|---|---|
| 178 | `CALLOC_STRUCT(mwv207_context)` |
| 183-184 | `ctx->base.screen = &screen->base; ctx->base.priv = ...` |
| 186 | `ctx->ws = ...` |
| 187 | `ctx->dirty = 0` |
| 188 | `ctx->in_fence_fd = -1` |
| 189 | `ctx->sample_mask = 0xffff` |
| 192 | `ctx->base.destroy = mwv207_context_destroy` |
| 194/196/198/200/202/204/206 | 依次调用 query/resource/surface/state/shader/sampler/blit 的 init |
| 208-212 | `draw_vbo` / `flush` / `create_fence_fd` / `fence_server_sync` / `memory_barrier` |
| 214-215 | `compute_init` / `video_codec_init` |
| 217 | `ctx->cmd = mwv207_create_cmd(ctx)` |
| 219-220 | `mwv207_context_helper_init(ctx)` 失败 → `goto fail` |
| 222 | `slab_create_child(&ctx->transfer_pool, &screen->transfer_pool)` |
| 226-231 | `fail:` 标签，**只 `FREE(ctx)`** —— 已创建的 `cmd`/`dma`/`blitter`/`primconvert`/`uploader` 全部泄漏（**D2**，报告行号 `mwv207_context.c:216-220`） |

`mwv207_flush()`（`mwv207_context.c:68-95`）：先对 `fb_state` 的每个 color buffer 调 `mwv207_flush_resource`（`:75-77`），再 `mwv207_submit(ctx, pfence ? &out_fence_fd : NULL, true)`（`:79`），若 `pixel_128bit` 则对每个 cbuf 调 `mwv207_merge_128bit`（`:89-90`），最后 `fence_create()`（`:94`）。**`:94` 的返回值没有检查即被赋给 `*pfence`** → `ANALYSIS-REPORT.md` 的 **D3**（报告行号 `mwv207_context.c:93`）。

---

## 4. 资源层

### 4.1 结构体字段表

> **命名对照**：本驱动**没有名为 `mwv207_miptree` 的类型**；全目录 `grep miptree` 只命中函数名 `mwv207_set_miptree()`（`mwv207_resource.c:142` 定义、`:283` 调用）。承担「miptree」职责的是 `struct mwv207_level_info level_infos[PIPE_MAX_TEXTURE_LEVELS]` 数组外加 `struct mwv207_tile_info ts`（tile 状态）。

`enum mwv207_tile_mode`（`include/mwv207_resource.h:49-53`）：

```c
/* include/mwv207_resource.h:49-53 */
enum mwv207_tile_mode { MWV207_LINEAR, MWV207_TILED, MWV207_SUPERTILED };
```

| 结构 | 行 | 字段 | 类型 | 作用 |
|---|---|---|---|---|
| `mwv207_transfer` | 55-58 | `base` | `struct pipe_transfer` | 对外 transfer（必须首位） |
| | 57 | `staging_res` | `struct pipe_resource *` | 非 NULL 表示走线性 staging：unmap 时需 blit 回写 |
| `mwv207_tile_info`（`ts`） | 60-67 | `bo` | `struct mwv207_bo *` | tile status BO（TS/压缩状态） |
| | 62 | `layer_offset` | `uint32_t` | 每层 TS 大小（对齐 64） |
| | 63 | `compressed` | `bool` | 是否走硬件压缩 |
| | 64 | `compressed_format` | `int32_t` | 压缩格式码（来自 `mwv207_get_compressed_format()`） |
| | 65 | `clear_values[2]` | `uint32_t` | 两个 32 位的 fast-clear 值 |
| | 66 | `enable` | `bool` | TS 是否启用（`mwv207_flush_resource` 会据它决定是否 `disable_tile`） |
| `mwv207_level_info` | 69-89 | `width` / `height` / `depth` | `uint32_t` | 逻辑尺寸（mip 后的实际层尺寸） |
| | 75-76 | `alloced_width` / `alloced_height` | `uint32_t` | 分配尺寸 = `ALIGN_NP2(width * xscale, align_w)`（非 whandle 走 tile 对齐 64/64；whandle 时按 `ALIGN_NP2(...,1)` 且可能带 MSAA 缩放） |
| | 78 | `stride` | `uint32_t` | 行跨度（字节），非 whandle 时由 `util_format_get_stride(res->base.format, alloced_width)` 算出 |
| | 79 | `offset` | `uint32_t` | 该 level 在资源内的字节偏移 |
| | 80 | `layer_size` | `uint32_t` | 单层字节数（对齐 `align_size = 256`） |
| | 81 | `size` | `uint32_t` | 该 level 全部层的字节数（对齐 `0x1000`） |
| | 83 | `config` | `uint32_t` | **BLT_SRC/DEST_CONFIG 镜像**：stride[20:0]、format[26:21]、MSAA[28:27]、TILING[29]、SUPER_TILING[30]、bit31=DISABLE（见 §6.6） |
| | 84 | `src_configex` | `uint32_t` | BLT_SRC_CONFIG_EX 镜像（cache_mode[17]、size_config[19]、tile_mode[22:21]、msaa_comp[23]） |
| | 85 | `dst_configex` | `uint32_t` | BLT_DEST_CONFIG_EX 镜像（cache_mode[17]、COLOR64[20]、TILE_MODE[27:26]、MSAA_COMP[28]） |
| | 86-87 | `src_compress` / `dst_compress` | `uint32_t` | 压缩位：`compressed << 1`、`compressed_format << 4 & 0xF0` |
| | 88 | `blit_config` | `uint32_t` | blocksize[7:9] 与 sample_mode[12] |
| `mwv207_sample_info` | 91-97 | `is_msaa` | `bool` | `nr_samples > 1` |
| | 94-95 | `xscale` / `yscale` | `uint8_t` | 采样网格：2 倍采样为 (2,2)，4 倍为 (2,2) 配 `nr_samples=4`（`mwv207_format.c:921-953`） |
| | 96 | `nr_samples` | `uint8_t` | 采样数 |
| `mwv207_resource` | 99-124 | `base` | `struct pipe_resource` | 对外资源（必须首位） |
| | 101 | `ctx` | `struct mwv207_context *` | 创建它的上下文（transfer/DMA 用） |
| | 103 | `offset` | `uint32_t` | 资源在 BO 内的起始偏移（`whandle` 导入时非 0） |
| | 104 | `size` | `uint32_t` | 总字节数 |
| | 106 | `mode` | `enum mwv207_tile_mode` | 布局：LINEAR / TILED / SUPERTILED |
| | 107 | `ts` | `struct mwv207_tile_info` | tile/压缩状态 |
| | 108 | `level_infos[]` | `struct mwv207_level_info[PIPE_MAX_TEXTURE_LEVELS]` | 每 level 布局与描述符（≈ miptree） |
| | 109 | `valid_buffer_range` | `struct util_range` | BUFFER 的有效数据区间（transfer 时用） |
| | 111 | `bo` | `struct mwv207_bo *` | 实际显存 |
| | 112 | `need_decompress` | `bool` | 需要把压缩内容解压到线性副本 |
| | 113 | `linear_output` | `bool` | 线性输出标志（未在 `set_miptree` 中置真，见 §4.2） |
| | 115 | `linear` | `struct pipe_resource *` | SCANOUT+SUPERTILED 时另建的线性副本 |
| | 117 | `res_128_bit` | `bool` | blocksize==16 且未压缩 → transfer 时需 128 位拆分/合并 |
| | 118-119 | `src_dirty` / `rgba_dirty` | `bool` | 脏标志（`transfer_unmap` 置 `src_dirty`） |
| | 120-121 | `rg` / `ba` | `struct pipe_resource *` | 128 位格式的两张 R32G32 辅助资源 |
| | 123 | `sample_info` | `struct mwv207_sample_info` | 采样信息 |

相关宏（`include/mwv207_resource.h:30-44`）：`R/G/B/A = 0/1/2/3`（`:33-36`）、`CACHE_MODE 0x1`（`:38`）、`MWV207_RESOURCE_FLAG_VIDEO_BUFFER = PIPE_RESOURCE_FLAG_DRV_PRIV << 0`（`:41`）、`MWV207_RESOURCE_FLAG_SUBALLOC = PIPE_RESOURCE_FLAG_DRV_PRIV << 1`（`:42`）、`ALIGN_NP2(n, align)`（`:44`，向上取整到 align 的**整数倍非 2 幂**对齐）。`resource_create` 里还用到**未命名**的 `PIPE_RESOURCE_FLAG_DRV_PRIV << 2`（“REUSE”，`mwv207_resource.c:291`、`:492`）。

### 4.2 布局决策（LINEAR / TILED / SUPERTILED）

三条判定链，全部返回**可否 tile**：

**(1) `mwv207_tile_supported_format()`（`mwv207_resource.c:38-67`）** —— 格式维度：

```c
/* mwv207_resource.c:38-67 摘要 */
if (util_format_get_blocksize(format) > 8)  return false;   /* :40-41 */
if (util_format_is_compressed(format))      return false;   /* :43-44 */
/* FIX(jm9100-oss) 注释 :46-53 —— 16 位归一化格式必须 LINEAR：
 * transfer 通路经「线性 staging + 2D blit」回写会破坏纹素字节布局 */
case PIPE_FORMAT_R16_UNORM:              /* :55-61 这 6 个显式 return false */
case PIPE_FORMAT_R16_SNORM:
case PIPE_FORMAT_R16G16_UNORM:
case PIPE_FORMAT_R16G16_SNORM:
case PIPE_FORMAT_R16G16B16A16_UNORM:
case PIPE_FORMAT_R16G16B16A16_SNORM:
   return false;
```

**(2) `mwv207_can_tile()`（`mwv207_resource.c:70-92`）** —— 用途/绑定维度：

```c
/* mwv207_resource.c:70-92 摘要 */
const bool enable_scanout_tile = false;                    /* :73 —— 硬编码关死 */
if (!mwv207_tile_supported_format(...)) return PIPE_ERROR_BAD_INPUT;  /* :75-77 */
if (bind & PIPE_BIND_LINEAR)            return PIPE_ERROR_BAD_INPUT;  /* :78-79 */
if (bind & PIPE_BIND_SCANOUT)           return PIPE_ERROR_BAD_INPUT;  /* :81-89：恒成立，scanout 永不走 tile */
return PIPE_OK;
```

**(3) `mwv207_set_miptree()` 的 mode 选择（`mwv207_resource.c:142-234`）**：

| 条件 | `res->mode` | 行 |
|---|---|---|
| `whandle == NULL` 且 `usage == PIPE_USAGE_STAGING` | `MWV207_LINEAR` | `:150-153` |
| `whandle == NULL` 且 `mwv207_can_tile(...) == PIPE_OK` | `MWV207_SUPERTILED` | `:155-156` |
| `whandle == NULL` 其他 | `MWV207_LINEAR` | `:157-158` |
| `whandle != NULL` 且 `modifier == DRM_FORMAT_MOD_VIVANTE_SUPER_TILED` | `MWV207_SUPERTILED` | `:159-160` |
| `whandle != NULL` 其他（含 `DRM_FORMAT_MOD_LINEAR`） | `MWV207_LINEAR` | `:161-162` |
| 压缩格式时的对齐覆盖 | `align_w = (layout==ETC\|\|S3TC) ? 16 : 1`，`align_h = align_size = 1` | `:164-167` |
| `res->mode != MWV207_LINEAR` | 追加调用 `mwv207_get_tile_info(screen, res, 0)` 建 TS BO（第三个形参 `uint32_t size` **从未被使用**，调用点恒传 `0`） | `:212-213` |
| `res->base.bind & PIPE_BIND_SCANOUT` 且 `res->mode == MWV207_SUPERTILED` | 另建 `res->linear`（自己复制一份 `templ` 后 `usage \|= PIPE_USAGE_STAGING`、`bind \|= PIPE_BIND_LINEAR`，再经 `screen->base.resource_create()` 递归创建，`:229-232`） | `:228-233` |
| `depth0 > 1 \|\| array_size > 1 \|\| last_level != 0`（在 `get_tile_info` 内） | `ts.compressed = false` | `:125-126` |

> `MWV207_TILED` 在本流程中**不会被选中**：`get_tile_info()` 只处理 `SUPERTILED`（`mwv207_format.c:685-707` 的 `get_tile_mode` 对 `TILED` 也给出了 tiling=ENABLE/super=DISABLE 的描述，但没有任何调用路径会造出 `MWV207_TILED` 资源）。**未确认**：是否存在外部/未来路径使用 `MWV207_TILED`。
> `res->linear_output` 在 `set_miptree` 与 `get_tile_info` 中都被置为 `false`（`:122`），只有 `mwv207_pipe_info.linear_outmode`（framebuffer 层）才是实际生效的输出模式标志——两者**同名不同物**，勿混。

**(4) `mwv207_get_tile_info()`（`mwv207_resource.c:95-129`）** —— TS（tile status / 压缩）BO 的构造：

| 行 | 内容 |
|---|---|
| 103-104 | `clear_values[0] = clear_values[1] = 0` |
| 107-109 | `layer_offset = align((level_infos[0].size >> (CACHE_MODE == 0x1 ? 9 : 8)) / array_size, 64)`（`CACHE_MODE` 为 `0x1`，即走 **`>> 9`** 分支） |
| 111 | `tile_size = layer_offset * array_size * depth0` |
| 113-114 | `ws->bo_create(tile_size, MWV207_DOMAIN_VRAM, MWV207_FLAG_CPU_ACCESS)` 建 TS BO |
| 116-118 | `bo_map` → `memset 0` → `bo_unmap`（**返回的映射指针未判空**） |
| 120-121 | `compressed_format = mwv207_get_compressed_format(res->base.format)`；`compressed = (compressed_format != -1)` |
| 122-123 | `enable = false`、`linear_output = false` |
| 125-126 | 多深度/多层/多 mip → `compressed = false` |

### 4.3 分配路径

`resource_create()`（`mwv207_resource.c:264-306`，**注意这个内部构造函数没有 `mwv207_` 前缀**；`mwv207_resource_create()` 只是它在 `:309-312` 的入口包装，`mwv207_resource_from_handle()` 是 `:315-321` 的另一层包装）的完整决策链：

| 行 | 决策 |
|---|---|
| 270 | `CALLOC_STRUCT(mwv207_resource)` —— **不做 NULL 检查即继续使用**（**D1**，报告行号 `mwv207_resource.c:230`） |
| 271-272 | 默认 `flag = MWV207_FLAG_CPU_ACCESS`、`domain = MWV207_DOMAIN_VRAM` |
| 274-275 | `res->base = *templ`；`mode = MWV207_LINEAR`（后续由 `set_miptree` 覆盖） |
| 280-283 | `PIPE_BUFFER` → `res->size = templ->width0`；否则 `mwv207_set_miptree(screen, res, NULL)`，失败则 `goto fail` |
| 285-286 | `res->base.flags & (DRV_PRIV << 0)`（=VIDEO_BUFFER）**或** `res->mode != LINEAR` → `flag = MWV207_FLAG_NO_CPU_ACCESS` |
| 288-289 | `res->base.flags & (DRV_PRIV << 1)`（=SUBALLOC）→ `flag \|= MWV207_FLAG_SUBALLOC` |
| 291-292 | `res->base.flags & (DRV_PRIV << 2)`（未命名“REUSE”）→ `flag \|= MWV207_FLAG_REUSE` |
| 294-295 | `res->base.bind & PIPE_BIND_SHARED` → `flag = MWV207_FLAG_2D_ACCESS`（**覆盖**掉前面的 CPU/NO_CPU 标志） |
| 297-298 | `res->base.usage == PIPE_USAGE_STAGING` → `domain = MWV207_DOMAIN_GTT` |
| 300-303 | `whandle != NULL` → `ws->bo_from_handle(..., res->size, flag, domain)`；否则 `ws->bo_create(res->size, flag, domain)` —— **返回值未判空**（同 **D1**） |

其他资源入口：`mwv207_resource_create()`（`:309-312`，即 `resource_create(pscreen, templ, NULL, 0)` 的包装）、`mwv207_resource_from_handle`（`:315-321`）、`mwv207_resource_destroy`（`:237-261`，非 BUFFER 时销毁 `ts.bo`/`rg`/`ba`/`linear`）、`mwv207_resource_get_handle`（`:324-353`，`SHMID` 直接返回 false；非 BUFFER 填 `stride`；`res->linear` 存在时**递归**导出线性副本；modifier 见 §4.6）、`mwv207_screen_can_create_resource`（`:132-139`，`nr_samples > 4` → false）。

### 4.4 `level_infos[]` 的逐 level 计算（`mwv207_resource.c:175-205`）

| 行 | 计算 |
|---|---|
| 175-190 | 逻辑尺寸 `width = u_minify(templ->width0, level)` 等；`depth` 由 `util_num_layers()` 得到 |
| `whandle != NULL` | `alloced_width = ALIGN_NP2(width * xscale, 1)`、`alloced_height = ALIGN_NP2(height * yscale, 1)`、`stride`/`offset` 来自 `whandle->stride` / `whandle->offset + totalBytes` |
| `whandle == NULL` | `alloced_width = ALIGN_NP2(width * xscale, align_w)`、`alloced_height = ALIGN_NP2(height * yscale, align_h)`、`stride = util_format_get_stride(res->base.format, alloced_width)`（`xscale`/`yscale` 来自 MSAA 采样网格，见 `:169-170`） |
| 196 | `offset = totalBytes`（whandle 时为 `whandle->offset + totalBytes`，见 `:190`） |
| 198-199 | `layer_size = stride * nblocksy`，再按 `align_size`（256）对齐 |
| 201-203 | `size = layer_size * depth`，再按 `0x1000` 对齐；`totalBytes += size` |
| 207-210 | `res->offset = 0`（whandle 时 `= whandle->offset`）；`res->size = totalBytes` |
| 215 | `mwv207_get_image_info(level_infos + level, format, ...)` 填 `config` / `src_configex` / `dst_configex` / `blit_config`（见 §6.6） |
| 217-224 | `rg = ba = NULL`；`res_128_bit = (util_format_get_blocksize(format) == 16 && !compressed)` |
| 226 | `linear = NULL` |
| 228-233 | `PIPE_BIND_SCANOUT` 且 `SUPERTILED` → 建 `res->linear`（`templ.usage \|= STAGING`、`bind \|= LINEAR`） |

### 4.5 transfer_map / unmap 通路

`mwv207_transfer_map()`（`mwv207_resource.c:409-521`）：

| 分支 | 行 | 行为 |
|---|---|---|
| 通用 | 420 | `slab_alloc(&ctx->transfer_pool, ...)`（**未判空**） |
| **PIPE_BUFFER** | 433-443 | 若 `PIPE_MAP_READ` 或映射区间与 `valid_buffer_range` 相交 → `mwv207_wait_resource()`；`ws->bo_map(res->bo)`；`stride = layer_stride = 0`；`ptr += res->offset + box->x * stride` |
| 非 BUFFER:等待 | 445-446 | 除非 `PIPE_MAP_UNSYNCHRONIZED`，一律 `mwv207_wait_resource()` |
| 非 BUFFER:`TILED/SUPERTILED` → **staging 路径** | 452 | 新建 `staging` 资源：`usage = PIPE_USAGE_STAGING`（若 CPU 读）、`bind \|= PIPE_BIND_LINEAR` → 该 staging 必为 LINEAR |
| | 459-462 | 若 `PIPE_MAP_READ`：先 `mwv207_blit_resource_box(pctx, &staging->base, pres, level, box)` 把 tile 内容 blit 到 staging，再 wait |
| | 464-472 | `stride`/`layer_stride` 取 staging 的 `level_infos`；`ptr = bo_map(staging->bo) + staging->offset + level offset + stride*box->x + nblocksy*stride*box->y + box->z*layer_size` |
| 非 BUFFER:`LINEAR` | 476-487 | `res_128_bit` 时先 `mwv207_merge_128bit()`；直接映射 `res->bo` 并算 `ptr` |
| **DMA 加速分支** | 490-515 | 附加条件：`PIPE_MAP_READ` 且**不是** `PIPE_MAP_WRITE` 且 `width*height >= 1024` → 新建资源（`templ.flags \|= DRV_PRIV << 2`）并在 BLT 引擎上跑 `mwv207_dma()`：有 staging 时 `mwv207_dma(ctx, staging, pdma, level, box)` 并把 staging 换成 pdma（`:499-502`），否则 `mwv207_dma(ctx, pres, pdma, ...)`（`:504-505`）；wait（`:508`）；`ptr` 改指 pdma（`:510-515`） |

`mwv207_transfer_unmap()`（`mwv207_resource.c:524-555`）：

```c
/* mwv207_resource.c:531-554 摘要 */
if (trans->staging_res) {                    /* :531 */
   ws->bo_unmap(staging->bo);                /* :533 */
   if (map & PIPE_MAP_WRITE)                 /* :535 */
      mwv207_blit_resource_box(pctx, &res->base, &staging->base, level, &box);  /* :535-537 —— 2D BLT 回写 */
   释放 staging 引用;                        /* :539 */
} else {
   ws->bo_unmap(res->bo);                    /* :541 */
   res->src_dirty = true;                    /* :541-542 */
}
if (pres->target == PIPE_BUFFER)             /* :545-547 */
   util_range_add(&res->valid_buffer_range, ...);
if (res->linear) mwv207_flush_resource(pctx, res->linear, ...);  /* :549-550 */
释放资源引用; /* :552 */  slab_free(&ctx->transfer_pool, trans);      /* :554 */
```

**这条回写路径就是 §4.2(1) 里 16 位归一化格式必须 LINEAR 的原因**：`blit_resource_box` 走 `util_format_linear()` + `PIPE_TEX_FILTER_NEAREST` 的 2D blit（`mwv207_blit.c:1142-1161`），会把 16 位纹素的字节序/位置打乱（`reports/ES3-FORMATS-LINEAR-REPORT.md` 的插桩证据：RG16 回读 `00 c3 00 c3`、R16 回读 `00 34 00 34`）。

其余入口：`mwv207_flush_region()`（`:558-569`，仅 BUFFER 时 `util_range_add`）、`mwv207_flush_resource()`（`:356-396`：`ts.enable && !linear` → `mwv207_disable_tile()`；`res->linear && need_decompress` → 组装 `mwv207_region_info` 后 `mwv207_copy_image()` 到线性副本并清标志）、`mwv207_wait_resource()`（`:399-406`：`mwv207_submit()` + `ws->bo_wait(OS_TIMEOUT_INFINITE)`）、`mwv207_context_resource_init()`（`:572-580`：`buffer_map = buffer_unmap = texture_map = texture_unmap = mwv207_transfer_map/unmap`）。

### 4.6 修饰符（dmabuf modifier）与 `pipe_screen` 资源回调

| 项 | 行 | 内容 |
|---|---|---|
| `mwv207_screen_supported_modifiers` | `mwv207_resource.c:582-585` | 只支持 `DRM_FORMAT_MOD_LINEAR` 与 `DRM_FORMAT_MOD_VIVANTE_SUPER_TILED` |
| `mwv207_screen_query_dmabuf_modifiers` | `:588-617` | 上限截到 2；YUV 或 `!mwv207_tile_supported_format()` → `max = 1`；`external_only = util_format_is_yuv(format) ? 1 : 0` |
| `mwv207_screen_resource_create_modifiers` | `:619-646` | 在 modifiers 里找 `VIVANTE_SUPER_TILED`；`DRM_FORMAT_MOD_INVALID` → NULL；`LINEAR` → `templ.bind \|= PIPE_BIND_LINEAR` |
| `mwv207_screen_resource_init` | `:648-660` | 一次装配 7 个 `pipe_screen` 回调：`can_create_resource`（`:651`）、`resource_create`（`:652`）、`resource_destroy`（`:653`）、`resource_get_handle`（`:655`）、`resource_from_handle`（`:656`）、`query_dmabuf_modifiers`（`:658`）、`resource_create_with_modifiers`（`:659`） |

---

## 5. 表面层（`mwv207_surface.c` / `include/mwv207_surface.h`）

### 5.1 结构体字段表

| 结构 | 行 | 字段 | 作用 |
|---|---|---|---|
| `mwv207_tex_info` | 35-42 | `first_level` / `last_level` / `level_count` | 采样的 level 区间 |
| | 38-42 | `first_layer` / `last_layer` / `layer_count` | 采样的层区间 |
| `mwv207_pixel_info` | 44-57 | `peDesc` | `struct mwv207_pixel_desc`（来自 `mwv207_get_pixel_desc()`） |
| | 46-47 | `pixel_config` / `pe_config` | PE 单元主配置（32 位） |
| | 48 | `pixel_config_ex` | PE 扩展配置（stride、super-tile、格式） |
| | 49-50 | `has_stencil` / `depth_config` | 深度/模板标志与深度配置 |
| | 51 | `depth_normalize` | 深度归一化系数（`Z16` → `0xFFFF`，其他 → `0xFFFFFF`） |
| | 52-53 | `memory_config` / `memory_config_ex` | 内存/压缩/TS 配置 |
| `mwv207_surface` | 59-68 | `base` | `struct pipe_surface` |
| | 61-64 | `offset` / `size` / `stride` | 该 surface 在资源中的字节偏移、大小、行跨度 |
| | 65 | `info` | `struct mwv207_tex_info` |
| | 66 | `peInfo` | `struct mwv207_pixel_info` |
| | 67 | `pixel_128bit` | 该表面是否用了 128 位拆分路径（`res->rg`） |
| `mwv207_framebuffer_state` | 70-74 | （未展开） | 帧缓冲状态包装 |
| `mwv207_sampler_desc` | 76-85 | （未展开） | 采样器描述符（采样/寻址/边界色） |
| `mwv207_sampler` | 87-90 | （未展开） | `pipe_sampler_state` 包装 |
| `mwv207_sampler_view` | 92-96 | `base` / `info` / `tex_desc` | 采样器视图：`mwv207_tex_info` + `struct mwv207_texture_desc`（纹理描述符，见 §6） |
| `mwv207_surface()` 内联 | 104-108 | — | 与 `mwv207_screen()` 同型：把 `pipe_surface *` 退回 `mwv207_surface *` |

### 5.2 资源 → 硬件镜像描述符

| 函数 | 行 | 产出 | 关键位域 |
|---|---|---|---|
| `mwv207_update_renderInfo()` | 36-49 | 更新 `peInfo.pe_config` 的 swizzle | 先 `pe_config &= 0xFFF007FF` 清 swizzle，再按 `SWIZZLE_RED/GREEN/BLUE/ALPHA` 写入位 12-13 / 14-15 / 16-17 / 18-19（`ALIGN_BIT(x, 0xC/0xE/0x10/0x12)`，`:42-45`） |
| `mwv207_get_renderInfo()`（static） | 51-124 | 填 `peInfo` 全量 | 见下表 |
| `mwv207_get_depthInfo()`（static） | 126-185 | 填深度/模板相关字段 | 见下表 |

**`mwv207_get_renderInfo()` 位域明细**（`mwv207_surface.c:51-124`）：

| 行 | 字段 | 位 | 来源 |
|---|---|---|---|
| 60-62 | `peDesc` | — | `pixel_128bit` 时改用 `res->rg` 的格式（`mwv207_resource(res->rg)`） |
| 64 | `peDesc` | — | 否则用 `surface->base.format` |
| 66-69 | `super_tile` = true, `tile_config = 0x1` | — | 资源 `mode == MWV207_SUPERTILED` |
| 77-81 | `pixel_config` | format 24-29 (`0x3F000000`)；bit6=0；bit20=super_tile；13-14 (`0x6000`)=tile_config；未用位按 `~(mask\|clear)` 保持 | PE 像素格式 |
| 83-85 | `pe_config` | swizzle 12-19；srgb bit31；bits5-6 = `0x3`（`ALIGN_BIT(0x3, 0x5)`） | PE 单元 |
| 87-92 | `pixel_config_ex` | srgb bit30；format 20-25 (`0x3F00000`)；super_tile bit28；tile_config 26-27 (`0xC000000`)；`stride` 0-17 (`0x3FFFF`) | PE 扩展（线性输出路径） |
| 98-104 | `memory_config`（`res->ts.bo` 存在时） | `msaa_compressed` bit26 (`0x4000000`)；**bit27 = DISABLE**；`ENABLE` bit1；`compressed` bit7；`compressed_format` 8-11 (`0xF00`)；`blocksize == 8` bit30 | 内存/压缩 |
| 106-112 | `memory_config_ex` | `msaa_compressed` bit9 (`0x200`)；**bit10 = DISABLE**；`ENABLE` bit0；`compressed` bit2；`compressed_format` 3-6 (`0x78`)；`blocksize == 8` bit7 | 内存/压缩扩展 |
| 114-123 | 无 TS 时 | `memory_config` 只把 bit1/bit5/bit7 置 DISABLE；`memory_config_ex` 只把 bit0/1/2 置 DISABLE | 关闭压缩 |

`msaa_compressed` 的判定在 `:95-96`：`res->sample_info.is_msaa && res->ts.compressed`。

**`mwv207_get_depthInfo()` 位域明细**（`mwv207_surface.c:126-185`）：

| 行 | 内容 |
|---|---|
| 131 | `depth_normalize` 默认 `0xFFFFFF` |
| 137 | `pe_config` 的 bits20-21 先置 `0x0`（= `STENCIL_FORMAT = D24S8`） |
| 139-143 | `depth_config` 从 `0xFFFFFFFF` 出发按格式逐位收窄 |
| 146-150 | `PIPE_FORMAT_S8_UINT` → `has_stencil = true`、`depth_config` bit12 = DISABLE、`pe_config` bits20-21 = `0x1`（= `S8`，与 `0x529 PE_CONFIG` 的 `STENCIL_FORMAT` 命名取值 D24S8=0/S8=1 对应） |
| 152-156 | `PIPE_FORMAT_Z16_UNORM` → `isZ16 = true`、`depth_normalize = 0xFFFF`、bit4 = `0x0` |
| 158-160 | `PIPE_FORMAT_S8_UINT_Z24_UNORM` → `has_stencil = true` |
| 173-178 | `res->ts.bo` 存在时 `memory_config`：`msaa_compressed` bit26、`ENABLE` bit0、`compressed` bit6、`has_stencil` bit14、`isZ16` bit3 |
| 180-183 | 否则与颜色路径一样把相关位置 DISABLE |

### 5.3 `mwv207_create_surface()` 与表面地址计算（`:188-252`）

| 行 | 内容 |
|---|---|
| 194 | `CALLOC_STRUCT(mwv207_surface)` |
| 200-210 | 填 `base` 基本字段（`u_minify` 尺寸等，见 `:207-208`） |
| 212-218 | `info`：`first_level` / `last_level` / `level_count = 1`；`layer_count = last_layer - first_layer + 1` |
| 220-221 | `PIPE_TEXTURE_CUBE` / `PIPE_TEXTURE_CUBE_ARRAY` → `layer_count *= 6` |
| 227-229 | `blocksize == 16` 且 `res->rg == NULL` → 建 `rg` / `ba` 两个辅助资源（`bind \|= PIPE_BIND_LINEAR`、格式 `mwv207_transfer_128bit(format)`） |
| 232-235 | 资源指针换成 `rg`；`surface->pixel_128bit = true` |
| 237 | `pLevel = &res->level_infos[level]` |
| 239-240 | `surface->offset = res->offset + pLevel->offset + first_layer * pLevel->layer_size` |
| 242-243 | `surface->size = pLevel->layer_size`、`surface->stride = pLevel->stride` |
| 245-246 | `bind` 含 `PIPE_BIND_RENDER_TARGET` 或 `PIPE_BIND_SAMPLER_VIEW` → `mwv207_get_renderInfo(surface)` |
| 248-249 | `bind` 含 `PIPE_BIND_DEPTH_STENCIL` → `mwv207_get_depthInfo(surface)` |

`mwv207_surface_destroy()`（`:255-264`）、`mwv207_swzzile_state_equal()`（`:267-275`，逐个比较 4 路 swizzle）。

### 5.4 `mwv207_set_framebuffer_state()`（`:278-355`）

三轮扫描 + 收尾：

| 轮/阶段 | 行 | 内容 |
|---|---|---|
| 复制 | 289 | `util_copy_framebuffer_state(&ctx->fb_state, state)` |
| 初值 | 291 | `ctx->info.linear_outmode = false` |
| 轮一 | 293-307 | 逐个 cbuf：编号写入 `ctx->info.cbufs_map[]`；**任一 cbuf 非 SUPERTILED → `linear_outmode = true`**；`rast_samples = MAX2(rast_samples, 该表面 sample_info.nr_samples)` |
| 重置 | 309-311 | `pixel_128bit = false`、`frag_rgba_swap = false`、`has_render_arrary = false` |
| 轮二 | 313-335 | 填 `pixel_record[i]`（0 或 `count + pixle_128_offset`）；`pixel_128bit` 时把 `cbufs_map[nr_cbufs + pixle_128_offset]` 也编号、`pixle_128_offset++`、`layer_count != 1` → `has_render_arrary = true`；每 4 路 swizzle 存入 `swizzle_record[i][0..3]` |
| 轮三 | 337-342 | 相邻 cbuf 的 swizzle 不同 → `ctx->info.frag_rgba_swap = true` |
| zsbuf | 344-349 | 有深度表面时更新 `rast_samples` 与 `subsamplez_dirty` |
| 收尾 | 351-354 | `ctx->info.pixel_num = pixle_128_offset`；`ctx->dirty \|= MWV207_DIRTY_FRAMEBUFFER` |

`mwv207_context_surface_init()`（`:358-363`）装配 `create_surface`（`:360`）、`surface_destroy`（`:361`）、`set_framebuffer_state`（`:362`）。

---

## 6. 格式层（`mwv207_format.c` / `include/mwv207_format.h`）

### 6.1 三张格式表总览

| 表 | 定义行 | 条目数 | 表项形状 | 码值去向（消费点） | 码值来源（厂商枚举） |
|---|---|---|---|---|---|
| `texture_infos[]` | `mwv207_format.c:54-177` | **82** | `{mesa_format, TX_FORMAT(old, new), SWIZZLE_SETS(r,g,b,a)}` | `get_texture_desc()`（`:180-231`）→ `mwv207_texture_desc.format` → `mwv207_sampler.c:777-780` 写 `tex_config[17:13]`（取 **old** 5 位）、`:790-795` 写 `tex_config_ext[5:0]`（取 **new** 6 位） | `MWV207REG_TX_EXT_CONFIG_FORMAT_*`（TX 单元，6 位）；old 侧 5 位枚举**未定位** |
| `pixel_infos[]` | `mwv207_format.c:233-321` | **62** | `{mesa_format, 6 位码, SWIZZLE_SETS(...)}` | `get_pixel_desc()`（`:324-406`）→ `peDesc.format` → `mwv207_surface.c:77-81` 写 `pixel_config` bits24-29、`:87-92` 写 `pixel_config_ex` bits20-25 | **第三套编号，未定位**（见 §8） |
| `blitInfos[]` | `mwv207_format.c:408-468` | **37** | `{mesa_format, 6 位码, BLT_SWIZZLE_EX(r,g,b,a)}` | `get_blit_desc()`（`:471-532`）→ `blit_desc.format` → `mwv207_get_image_info()`（`:762`）写 `pLevel->config` bits21-26；BLT 引擎的 `mwv207_blit.c` 也用它 | `MWV207REG_BLT_SRC_CONFIG_FORMAT_*`（BLT config[26:21]，6 位） |

三张表的表项样例（首尾各一，均可核对）：

```c
/* mwv207_format.c:54-56 */
struct mwv207_hardware_Info texture_infos[] = {
   {PIPE_FORMAT_A8_UNORM,  TX_FORMAT(0x1, 0), SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_GREEN, SWIZZLE_BLUE, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_L8_UNORM,  TX_FORMAT(0x2, 0), ...},
/* mwv207_format.c:175-176 末尾 */
   {PIPE_FORMAT_X24S8_UINT, TX_FORMAT(0, 0x22), SWIZZLE_SETS(SWIZZLE_RED, SWIZZLE_ZERO, SWIZZLE_ZERO, SWIZZLE_ALPHA)},
   {PIPE_FORMAT_S8X24_UINT, TX_FORMAT(0, 0x22), ...},

/* mwv207_format.c:233/319-320 */
struct mwv207_hardware_Info pixel_infos[] = {
   {PIPE_FORMAT_B4G4R4X4_UNORM, 0x0, ...},
   ...
   {PIPE_FORMAT_R32G32B32A32_FLOAT, 0x25, ...}

/* mwv207_format.c:408/466-467 */
static const struct mwv207_blit_Info blitInfos[] = {
   {PIPE_FORMAT_B4G4R4X4_UNORM, 0x0, BLT_SWIZZLE_EX(...)},
   ...
   {PIPE_FORMAT_R16G16B16A16_UNORM, 0x30, ...},
   {PIPE_FORMAT_R16G16B16A16_SNORM, 0x30, ...},
```

* `TX_FORMAT(old, new)`（`include/mwv207_format.h:54-56`）= `(old << 0) | (new << 8)`；一个 32 位域里塞了两套码值，分别喂给 TX 单元的两组寄存器。
* 多组 Mesa 格式**共享同一个码值**，靠 swizzle 区分：`B8G8R8A8_UNORM` / `R8G8B8A8_UNORM` / `A8B8G8R8_UNORM` 同为 `0x7`，`B8G8R8X8_UNORM` 为 `0x8`，`R8G8B8X8_UNORM` 为 `0xA`。
* `texture_infos[]` 里 **`PIPE_FORMAT_R9G9B9E5_FLOAT` 出现两次**（`mwv207_format.c:79` 与 `:100`），两行值完全相同（`TX_FORMAT(0x1D, 0)`）——查表是**线性首个匹配**（`:187-196`），因此这条重复无害，但属于冗余数据。
* `texture_infos[]` / `pixel_infos[]` 是**非 static 全局**（`struct mwv207_hardware_Info ...[]`），`blitInfos[]` 是 `static const`。

### 6.2 `get_texture_desc()`（`:180-231`）与 `int_format` 语义

```c
/* mwv207_format.c:185-206 摘要 */
format = util_format_linear(format);              /* 先做 linear 化 */
for (i = 0; i < ARRAY_SIZE(texture_infos); ++i)   /* :187-196 线性扫表 */
   if (texture_infos[i].mesa_format == format) { desc->format = texture_infos[i].format; break; }
if (没找到) {
   if (desc->layout == UTIL_FORMAT_LAYOUT_ASTC)   /* :199-204 */
      desc->format = TX_FORMAT(0, 0x14);          /* ASTC 兜底 */
   else
      return PIPE_ERROR_BAD_INPUT;                /* :206 */
}
```

`int_format` 的判定（`:209-222`）——**只对「纯整数 + 有符号 + 非归一化」的通道置非零**：

```c
/* mwv207_format.c:209-222 摘要 */
desc->int_format = 0x0;
if (desc->channel[0].type == UTIL_FORMAT_TYPE_SIGNED && !desc->channel[0].normalized) {
   if (size == 8)  desc->int_format = 0x1;
   if (size == 16) desc->int_format = 0x2;
}
```

消费点在采样器：`mwv207_sampler.c:809-813` 把它写进 `tex_config2` 的 bits19:18（`INTEGER_SIGN_EXTENSION`），且 `int_format == 0` 时另置 bit16 = DISABLE。源码注释（`mwv207_format.c` 内）说明：**旧判据误把 SNORM 当作 SIGNED16 处理，导致采样返回 0**，这正是本次修复的内容。

另外两个特例（`:224-228`）：`PIPE_FORMAT_R5G6B5_UNORM` / `B5G6R5_UNORM` → `int_format = 0x2`；`PIPE_FORMAT_X8Z24_UNORM` / `S8_UINT_Z24_UNORM` → `0x0`。

### 6.3 `get_pixel_desc()`（`:324-406`）与 `output_mode`

```c
/* mwv207_format.c:334-349 摘要 */
for (每个 pixel_infos 项) if (格式匹配) { desc->format = 6 位码; desc->swizzles[] = ...; found = true; }
if (!found) return PIPE_ERROR_BAD_INPUT;          /* :345-346 */
desc->saturation = false;                         /* :349 */
```

`output_mode` 的判定（`:351-399`）：

| 条件 | `output_mode` | 行 |
|---|---|---|
| `PIPE_FORMAT_R10G10B10A2_UINT` / `B10G10R10A2_UINT` | `0x4` | `:351-357` |
| `PIPE_FORMAT_R10G10B10A2_SINT` / `B10G10R10A2_SINT` | `0x6` | `:358-364` —— 但 `pixel_infos[]` **没有 SINT 表项**，`!found` 已在 `:345` 返回，因此**不可达** |
| 纯整数 `UNSIGNED` 8/16/32 位 | `0x3` / `0x4` / `0x2` | `:366-380` |
| 纯整数 `SIGNED` 8/16/32 位 | `0x5` / `0x6` / `0x2` | `:381-386` |
| 纯整数其他位宽 | `PIPE_ERROR_BAD_INPUT` | `:387` |
| 非整数且 `size == 32` | `0x2` | `:394-398` |
| 非整数其他 | `0x0`，且 `saturation = true` | `:398` |

`srgb`（`util_format_is_srgb`）与 `endian`（`mwv207_get_endian`，恒 0）在 `:402-403` 填。

### 6.4 `get_blit_desc()`（`:471-532`）与 blocksize 兜底

查表未命中时**不报错**，而是按 `util_format_get_blocksize()` 兜出一个码值，并置 `fake_format = true`（`:516`）——这是本驱动格式支持面比表宽的原因：

| blocksize | 兜底码 | 行 |
|---|---|---|
| 1 | `0x23` | `:488-490` |
| 2 | 4 组分 → `0x1`，否则 `0x24` | `:491-497` |
| 3 | `0x22` | `:498-500` |
| 4 | `0x6` | `:501-503` |
| 8 | `0x1C` | `:504-508` |
| 16 | `0x26` | `:509-513` |
| 其他 | `PIPE_ERROR_BAD_INPUT`（走 `:518` 前的分支） | `:514-517` |

命中表项时 `fake_format = false`（`:518`）；`sample_mode = 纯整数 ? 0x1 : 0x0`（`:522-526`）；`srgb`、`pixel_size = util_format_get_blockwidth()`、`endian` 在 `:527-529`。

### 6.5 `get_element_desc()` / `get_format_desc()` / `get_tile_mode()`

`get_element_desc()`（`:535-649`）产出顶点元素描述：`nr_channels` 1/2/3/4 → `size = 0x1/0x2/0x3/0x0`（其他 `PIPE_ERROR_BAD_INPUT`，`:544-559`）；归一化时 `size == 10` → `0x1` 否则 `0x2`（`:561-566`）；纯整数按类型/位宽给 `format`（8/16/32 → `0xE/0xF/0x10`，`weight = 1`，`SIGNED` → `normalized = 0x1`，`:568-584`）；其余 `UNSIGNED` 8/10/16/32 → `0x1/0x7/0x3/0x5`，`SIGNED` → `0x0/0x6/0x2/0x4`，`FLOAT` 16/32 → `0x9/0x8`，`weight = 0x3F800000`（`:586-633`）；`attribue_config` 位域（拼写为驱动原文）在 `:638-646`。

`get_format_desc()`（`:652-682`）返回**绑定位掩码**，是 §2.3.5 判定链的核心：

| 行 | 条件 | 返回位 |
|---|---|---|
| 657-658 | `get_texture_desc()` 成功 | `PIPE_BIND_SAMPLER_VIEW` |
| 662-663 | `get_pixel_desc()` 成功 | `PIPE_BIND_RENDER_TARGET \| PIPE_BIND_BLENDABLE` |
| 668-669 | `get_element_desc()` 成功 | `PIPE_BIND_VERTEX_BUFFER` |
| 671-676 | `Z16_UNORM` / `Z32_FLOAT` / `X8Z24_UNORM` / `S8_UINT_Z24_UNORM` / `S8_UINT` | `PIPE_BIND_DEPTH_STENCIL` |
| 678-679 | `Z32_FLOAT_S8X24_UINT`（特例，直接 return） | `PIPE_BIND_DEPTH_STENCIL \| PIPE_BIND_SAMPLER_VIEW` |

注意：**没有任何格式返回 `PIPE_BIND_SHADER_BUFFER` / `PIPE_BIND_SHADER_IMAGE` / `PIPE_BIND_CURSOR` / `PIPE_BIND_DISPLAY_TARGET`** —— 与 §2.3.5 只在 `PIPE_BUFFER` 分支检查 `PIPE_BIND_VERTEX_BUFFER` 的设计一致。

`get_tile_mode()`（`:685-707`）：`MWV207_TILED` → `tiling = ENABLE`、`super_tile = DISABLE`；`MWV207_SUPERTILED` → 两者都 `ENABLE`；其他 → 两者 `DISABLE`。

### 6.6 `mwv207_get_image_info()`：资源 → 每个 level 的硬件描述符（`:710-788`）

这是把 `mwv207_resource` 编成硬件镜像描述符的地方，**每个 mip level 一套**，写入 `res->level_infos[level]`。

八位域总表（`mwv207_format.c:757-785`）：

| 目标字段 | 位域 | 来源 | 行 |
|---|---|---|---|
| `pLevel->config` | stride bits **0-20** | `pLevel->stride` | `:761` |
| | `blit_desc.format` bits **21-26**（6 位） | `get_blit_desc()` 的 `blitInfos[]` 码值 | `:762` |
| | `msaa_mode` bits **27-28** | `get_blit_msaa_mode()`（2×2 → `0x3`，否则 `0x0`） | `:763`、`:956-962` |
| | `tiling` bit **29** | `get_tile_mode()` | `:764` |
| | `super_tile` bit **30** | `get_tile_mode()` | `:765` |
| | **bit 31 = DISABLE**（恒 0） | 常量 | `:766` |
| `pLevel->src_configex` | bits **0-1** DISABLE | 常量 | `:777-778` |
| | `cache_mode` bit **17** | `CACHE_MODE == 0x1 ? ENABLE : DISABLE`（本驱动恒 ENABLE） | `:718`、`:779` |
| | `size_config` bit **19** | `blocksize >= 8` | `:722-725`、`:780` |
| | `src_tile_config` bits **21-22** = `0x1`（SUPER_TILED_XMAJOR） | `res->mode == MWV207_SUPERTILED` | `:746-748` |
| | bits **23** = MSAA_COMPRESSION ENABLE | `is_msaa && ts.compressed` | `:751-752` |
| | `src_compress` bits 1（`compressed`）、4-7（`compressed_format`） | `ts` | `:737-740`、`:775` |
| `pLevel->dst_configex` | bits **0-1** DISABLE | 常量 | `:772-773` |
| | `cache_mode` bit **17** | 同上 | `:771` |
| | `size_config` bit **20**（COLOR64） | `blocksize >= 8` | `:770` |
| | `dst_tile_config` bits **26-27** = `0x1` | SUPERTILED | `:748` |
| | bit **28** = MSAA_COMPRESSION ENABLE | `is_msaa && ts.compressed` | `:753` |
| | `dst_compress` bits 1、4-7 | `ts` | `:742-744`、`:768` |
| `pLevel->blit_config` | `(blocksize - 1)` bits **7-9**（`0x380`） | 格式 blocksize | `:783` |
| | `sample_mode` bit **12**（`0x1000`） | `get_blit_desc().sample_mode`（纯整数 = 1） | `:784` |

**与厂商寄存器逐一对照**（见 `registers/REGISTERS-FIELDS.md`）：

| 驱动字段 | 厂商寄存器 | 字段名与位域 | 结论 |
|---|---|---|---|
| `pLevel->config` | `0x5002 MWV207REG_BLT_SRC_CONFIG`（BLT_SRC_CONFIG，`:2106`） | `STRIDE 20:0`、`FORMAT 26:21`、`MSAA 28:27`、`TILING 29`、`SUPER_TILING 30`、`MULTI_TILING 31` | **完全一致**：`config` 就是 BLT_SRC/DEST_CONFIG 的镜像；驱动恒清 `MULTI_TILING` |
| `pLevel->config`（作目标用时） | `0x5009 MWV207REG_BLT_DEST_CONFIG`（`:2238`） | 同上（字段布局相同） | 同一份 `config` 被 blit 路径当 src 与 dst 配置复用（`mwv207_blit.c:253/399/550/656` 的写入点） |
| `pLevel->src_configex` | `0x5003 MWV207REG_BLT_SRC_CONFIG_EX`（`:2132`） | `COLOR64 19`、`CACHE_MODE 17`、`TILE_MODE 22:21`、`MSAA_COMPRESSION 23`、`UV_SWIZZLE 18`、`SRGB 8`、`COLOR_FORMAT 7:4` | **一致**：src 侧 `size_config` 落在 bit19（COLOR64）、tile_mode 落在 22:21、MSAA 落在 23 |
| `pLevel->dst_configex` | `0x500a MWV207REG_BLT_DEST_CONFIG_EX`（`:2262`） | `TILE_MODE 27:26`、`MSAA_COMPRESSION 28`、`COLOR64 20`、`CACHE_MODE 17`、`DESTINATION_CONTROL 31` | **一致**：dst 侧布局与 src 侧**不同**（COLOR64 在 20、TILE_MODE 在 27:26、MSAA 在 28）——驱动确实按两套不同位域写入 |
| `src_compress` / `dst_compress` | 同上两个 `_EX` 寄存器 | （本驱动把 `compressed` 放 bit1、`compressed_format` 放 bit4-7；厂商是否有对应独立字段**未确认**，参考文档里 `_EX` 的 bit1 是 `COMPRESSION_EN_C`、bit4-7 是 `COLOR_FORMAT`，二者语义不同 → 疑似**位域错配**） | **未确认/存疑**，见 §8 |

`get_image_info` 的调用点：`mwv207_resource.c:215`（`set_miptree` 内，逐 level 传入 `&res->level_infos[level]`）。`config` 的消费点：`mwv207_blit.c:253/399/550/656`（命令流直接写 `0x5002`/`0x5009`）。

### 6.7 其余格式函数

| 函数 | 行 | 要点 |
|---|---|---|
| `mwv207_get_endian()` | `34-52` | **第 36 行无条件 `return 0x0;`，其后 `:38-51` 的 `switch` 是死代码** → 所有格式的 `endian` 恒为 `0x0`。若硬件确实有大小端/字节序配置位，则此函数等于永远不做设置 |
| `mwv207_get_clearval()` | `791-872` | 颜色：`util_pack_color_union`，blocksize 1/2 时把 `uc.ui[0]` 复制成 `0x01010101` / `0x00010001` 再填两个 32 位字，mask 全 1；深度模板：按格式给 `clear_bits_depth` / `clear_bits_stencil`（`S8_UINT` `0xffffff00/0xff`、`Z16`/`X8Z24` `0xffffffff/0`、`S8_UINT_Z24` `0xffffff00/0xff`、`Z24_UNORM_S8_UINT` `0x00ffffff/0xff000000`、`Z32_FLOAT_S8X24` `0xffffff00/0xff`、default 全 1），未置 `PIPE_CLEAR_DEPTH`/`STENCIL` 则对应部分清零（`:845-853`），`util_pack64_z_stencil()` 打包、`Z16` 再 `\|= 自身 << 16`、`val[1]`/`mask[1]` 复制；其他格式返回 `PIPE_ERROR` |
| `mwv207_get_compressed_format()` | `875-898` | `B8G8R8X8_UNORM/_SRGB`、`R8G8B8X8_UNORM` → `0x3`；`B8G8R8A8_UNORM/_SRGB`、`R8G8B8A8_UNORM` → `0x3`；`X8Z24_UNORM` → `0x6`；`S8X24_UINT`、`S8_UINT_Z24_UNORM` → `0x5`；default 返回 `-1`。函数返回类型是 `uint32_t`，因此调用方 `mwv207_resource.c:120-121` 的「`!= -1`」实际是与 `0xFFFFFFFF` 比较 |
| `mwv207_update_colormask()` | `901-918` | `color_mask == PIPE_MASK_RGBA` 直接返回；否则把 R/G/B/A 的使能位映射到 `swizzle_record` 上对应分量 |
| `mwv207_get_sample_info()` | `921-953` | `0`/`1` → `(1,1,1)`；`2`/`4` → `(2,2,4)`；其他 `return false` |
| `mwv207_get_blit_msaa_mode()` | `956-962` | `xscale == 2 && yscale == 2` → `0x3`，否则 `0x0` |

---

## 7. 已知缺陷条目

`D1`–`D7` 引自 `reports/ANALYSIS-REPORT.md`（针对 Mesa 22.3.7 fork），下表给出**本 24.1.2 快照的实际行号**与复核结论：

| 编号 | 报告描述 | 报告行号（22.3.7） | 本快照行号（24.1.2） | 复核结论 |
|---|---|---|---|---|
| **D1** | `resource_create` 不检查 `CALLOC` / `bo_create` / `bo_from_handle` 失败 | `mwv207_resource.c:230` | `mwv207_resource.c:270`（CALLOC）、`:301`/`:303`（bo 创建/导入） | **仍存在**；另见 `:113-118`（`get_tile_info` 的 `bo_create` + `bo_map` 亦未判空）与 `:420`（transfer 的 `slab_alloc`） |
| **D2** | `context_create` 的 fail 路径只 `FREE(ctx)` | `mwv207_context.c:216-220` | `mwv207_context.c:226-231` | **仍存在**：`cmd`/`dma`/`blitter`/`primconvert`/`stream_uploader` 全部泄漏 |
| **D3** | `flush` 里 `fence_create` 失败泄漏 `out_fence_fd` | `mwv207_context.c:93` | `mwv207_context.c:93-94` | **仍存在**：`:94` 直接赋值，无返回值检查 |
| **D4** | `sample_count_flags` 把 `0` 当「不支持 MSAA」 | `mwv207_screen.c:523-538` | `mwv207_screen.c:522-536` | **仍存在**：`switch` 无 `case 0`，落 `default: false` |
| **D5** | `nir/mwv207_disasm.h` + `meson.build` 缺 `etnaviv/hw/isa.xml.h` include、`INST_CONDITION_MWV207_TRUE` 命名错位 | — | 不在本层文件 | 属于 NIR/编译层，详见 `04-NIR-COMPILER.md` |
| **D6** | `opcode.c` / `emit.c` / `lower.c` 引用不存在的 `INST_EXT_OPCODE_*` | — | 不在本层文件 | 同上 |
| **D7** | `mwv207_drm_bo.c:205` 的 `util_logbace2` 拼写错误 | `mwv207_drm_bo.c:205` | winsys 层 | 详见 `01-WINSYS-KMD.md` |

本层复核过程中另外发现的、**未列入 `D1`–`D7`** 的可疑点（均给出证据行）：

| # | 观察 | 证据 |
|---|---|---|
| O1 | `mwv207_get_endian()` 第一行就 `return 0x0`，其后整个 `switch` 是死代码；所有格式 endian 恒为 0 | `mwv207_format.c:36`（函数体 `34-52`） |
| O2 | `transfer_flush_region` 被装配两次，后写者覆盖前写者：`resource_init` 给 `mwv207_flush_region`（`mwv207_resource.c:579`），随后 `blit_init` 又赋成 `u_default_transfer_flush_region`（`mwv207_blit.c:1235`）——调用顺序 `mwv207_context.c:196` → `:206`，故**最终生效的是默认实现**，`mwv207_flush_region` 实际上永不通过虚表被调用 | `mwv207_resource.c:579`、`mwv207_blit.c:1235`、`mwv207_context.c:196,206` |
| O3 | `get_shader_param` 的 `switch` 之后还有 `debug_printf` + `return 0`，不可达 | `mwv207_screen.c:465-469` |
| O4 | `get_timestamp` 在同一函数里赋值两遍（`screen->base.get_timestamp`） | `mwv207_screen.c:661`、`:663` |
| O5 | `texture_infos[]` 中 `R9G9B9E5_FLOAT` 重复出现且值相同 | `mwv207_format.c:79`、`:100` |
| O6 | `get_pixel_desc` 的 `R10G10B10A2_SINT → 0x6` 分支不可达（`pixel_infos[]` 无 SINT 项，先返回 `PIPE_ERROR_BAD_INPUT`） | `mwv207_format.c:345-346`、`:358-364` |
| O7 | `src_compress`/`dst_compress` 实际写入 `_EX` 寄存器的 bit1/bit4-7，与厂商文档中该位置的 `COMPRESSION_EN_C`/`COLOR_FORMAT` 语义不一致 | `mwv207_format.c:737-744`、`REGISTERS-FIELDS.md:2132,2262` |
| O8 | 「`R16_UNORM` 与 `R16_SNORM` 共用 `0x36`、靠 `int_format` 区分」的说法与代码不符：`int_format` 只对**非归一化且有符号**的通道置位，归一化的 `R16_SNORM` 得到 `0 == R16_UNORM`，两者在 `tex_config2` 上无法区分 | `mwv207_format.c:209-222`、`mwv207_sampler.c:809-813`、`reports/ES3-FORMATS-LINEAR-REPORT.md` |

---

## 8. 未确认清单

| # | 未确认项 | 说明 |
|---|---|---|
| U1 | `pixel_infos[]` 的 6 位码值来自哪套厂商枚举 | `reports/ES3-FORMATS-LINEAR-REPORT.md` 在 `/tmp/dpkg/gg/usr/src/mwv207-1.6.1-v25.u1/jmgpu_neighbor.h` 中只定位到 `TX_EXT_CONFIG_FORMAT_*`（采样器）与 `BLT_SRC_CONFIG_FORMAT_*`（BLT）两套；第三套（`R8G8_UNORM = 0x1F`、`R8_UNORM = 0x23`、`R32G32B32A32_FLOAT = 0x25` … 见 `mwv207_format.c:233-321`）**未能在厂商头中定位** |
| U2 | `TX_FORMAT()` 的 `old` 5 位侧枚举归属 | 采样器 `tex_config[17:13]` 只用到低 5 位（`mwv207_sampler.c:777-780`），对应哪个厂商枚举表未定位 |
| U3 | `pixel_config` / `pixel_config_ex` / `memory_config` 的 PE 侧寄存器归属 | 只与 `0x529 PE_CONFIG` 的 `STENCIL_FORMAT`/`SWIZZLE_*` 对照成功（`REGISTERS-FIELDS.md:1628`）；其余位（如 `pe_config` bits5-6 = `0x3`、`memory_config` bit27 DISABLE）在寄存器文档里未见对应字段命名 → 未确认 |
| U4 | 8×/16× MSAA 是否真的不存在硬件支持 | 驱动一律返回 false（`mwv207_screen.c:529-534`），无从判断是硬件限制还是驱动未实现 |
| U5 | `MWV207_TILED` 是否有实际使用路径 | `set_miptree` 只会产生 `LINEAR` / `SUPERTILED`（`mwv207_resource.c:150-162`），但 `get_tile_mode` 为它写了专门值（`mwv207_format.c:685-707`） |
| U6 | `res->linear_output` 字段的用途 | `set_miptree`/`get_tile_info` 只写 `false`（`mwv207_resource.c:122`），未见读取点；实际生效的是 `mwv207_pipe_info.linear_outmode` |
| U7 | `mwv207_get_compiler_options()` 上报了哪些选项 | 实现位于 `mwv207_shader.c:856`，不在本层范围（见 `04-NIR-COMPILER.md`） |
| U8 | 各 `PIPE_CAP_*` 在 Mesa ST 里的精确消费点 | 本文「门控/消费方」列已尽量给出对应 GL/GLES 扩展，但「哪个 Mesa 函数读了这个 cap」需要跨到 `src/mesa/state_tracker`，**未逐一确认** |
| U9 | `transfer_map` 的 `slab_alloc` / `bo_map` 是否在别处兜底 | 本层看到 `:420`、`:433-443`、`:464-472` 均无判空 |
| U10 | `viewport` / `rasterizer` 子像素位（`PIPE_CAP_VIEWPORT_SUBPIXEL_BITS = 2`、`PIPE_CAP_RASTERIZER_SUBPIXEL_BITS = 4`）的下游消费点 | 未确认与硬件寄存器 `0x` 字段的对应 |
| U11 | `mwv207_update_colormask()` / `get_clearval()` 的实际调用点 | 本层未追到调用者（可能在 `mwv207_draw.c` / `mwv207_blit.c`） |
| U12 | `mwv207_context.h` 中位 `1 << 5` 空缺（`MWV207_DIRTY_*` 从 `1<<4` 直接跳到 `1<<6`）是刻意保留还是遗漏 | 无注释可依 |

---

## 附录：本层函数索引（按文件）

| 文件 | 函数 | 行 |
|---|---|---|
| `mwv207_screen.c` | `mwv207_get_vendor` / `mwv207_get_device_vendor` / `mwv207_get_name` | 42 / 48 / 54 |
| | `mwv207_get_param` / `mwv207_get_paramf` / `mwv207_get_shader_param` / `mwv207_get_compute_param` | 60 / 334 / 366 / 473 |
| | `mwv207_sample_count_flags` / `mwv207_is_format_supported` | 522 / 539 |
| | `mwv207_get_timestamp` / `mwv207_fence_reference` / `mwv207_fence_finish` / `mwv207_fence_get_fd` | 595 / 600 / 610 / 620 |
| | `mwv207_destroy_screen` / `mwv207_create_screen` | 629 / 642 |
| `mwv207_context.c` | `mwv207_context_helper_init` / `mwv207_flush` / `mwv207_memory_barrier` | 44 / 68 / 98 |
| | `mwv207_context_destroy` / `mwv207_create_fence_fd` / `mwv207_fence_server_sync` | 106 / 154 / 167 |
| | `mwv207_context_create` | 175 |
| `mwv207_resource.c` | `mwv207_tile_supported_format` / `mwv207_can_tile` / `mwv207_get_tile_info` | 38 / 70 / 95 |
| | `mwv207_screen_can_create_resource` / `mwv207_set_miptree` | 132 / 142 |
| | `mwv207_resource_destroy` / `mwv207_resource_create` / `mwv207_resource_create` 包装 / `from_handle` | 237 / 264 / 309 / 315 |
| | `mwv207_resource_get_handle` / `mwv207_flush_resource` / `mwv207_wait_resource` | 324 / 356 / 399 |
| | `mwv207_transfer_map` / `mwv207_transfer_unmap` / `mwv207_flush_region` | 409 / 524 / 558 |
| | `mwv207_context_resource_init` / `mwv207_screen_resource_init` | 572 / 649 |
| `mwv207_surface.c` | `mwv207_update_renderInfo` / `mwv207_get_renderInfo` / `mwv207_get_depthInfo` | 36 / 51 / 126 |
| | `mwv207_create_surface` / `mwv207_surface_destroy` / `mwv207_swzzile_state_equal` | 188 / 255 / 267 |
| | `mwv207_set_framebuffer_state` / `mwv207_context_surface_init` | 278 / 358 |
| `mwv207_format.c` | `mwv207_get_endian` / `mwv207_get_texture_desc` / `mwv207_get_pixel_desc` | 34 / 180 / 324 |
| | `mwv207_get_blit_desc` / `mwv207_get_element_desc` / `mwv207_get_format_desc` | 471 / 535 / 652 |
| | `mwv207_get_tile_mode` / `mwv207_get_image_info` / `mwv207_get_clearval` | 685 / 710 / 791 |
| | `mwv207_get_compressed_format` / `mwv207_update_colormask` / `mwv207_get_sample_info` / `mwv207_get_blit_msaa_mode` | 875 / 901 / 921 / 956 |
