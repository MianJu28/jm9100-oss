# 00 · 架构总览

## 1. 分层

```
  应用 (Firefox / glmark2 / FFmpeg / OpenCL 程序 / Xorg)
        │  libGL / libEGL / libGLESv2  (libglvnd 分发)
        ▼
  ┌──────────────────────────────────────────────────────────┐
  │ Mesa 24.1.2                                              │
  │   src/mesa/main/*        GL/GLES 状态机与 API 入口        │
  │   src/mesa/state_tracker ST：把 GL 状态翻译成 pipe 状态   │
  │   src/compiler           GLSL→NIR、NIR 公共优化           │
  └──────────────────────────────────────────────────────────┘
        │  /*include/pipe/p_screen.h 等虚表接口*/
        ▼
  ┌──────────────────────────────────────────────────────────┐
  │ mwv207 UMD（本驱动）        src/gallium/drivers/mwv207  │
  │   screen / context        pipe_screen、pipe_context 实现  │
  │   resource / surface      显存对象与硬件镜像描述符        │
  │   format                  三张格式表（码值空间）          │
  │   state / blend / sampler 状态 → 硬件寄存器               │
  │   vertex / draw / blit    顶点装配、绘制、BLT/DMA         │
  │   nir/                    NIR → MWV207 ISA 编译器         │
  │   video-codec/            H264/HEVC/VP9 硬件解码          │
  └──────────────────────────────────────────────────────────┘
        │  mwv207_winsys（bo_* / cs_* 函数表）
        ▼
  ┌──────────────────────────────────────────────────────────┐
  │ winsys/mwv207/drm         src/gallium/winsys/mwv207/drm  │
  │   BO 分配/映射、命令缓冲、reloc、fence fd                 │
  └──────────────────────────────────────────────────────────┘
        │  6 个 DRM ioctl
        ▼
  ┌──────────────────────────────────────────────────────────┐
  │ KMD  mwv207.ko           src/kernel/drivers/gpu/drm/mwv207│
  │   TTM 显存管理、GEM、调度器 drm_sched、IRQ、CRTC/KMS       │
  └──────────────────────────────────────────────────────────┘
        ▼
  硬件功能块：FE / VS / SH / GS / PS / PE / TX / RA / SE / PA / DE / BLT / DEC / ZSA / AHBDEC
```

## 2. 关键约定

### 2.1 命令流里的寄存器号

驱动在命令缓冲里写寄存器用 `MWV207_EMIT(cmd, reg, value)`，其中 `reg` 是一个 16 位号，
**等于厂商寄存器地址右移 2 位**：

```
reg_id = MWV207REG_<X>_Address >> 2
```

例：`0x530 → MWV207REG_PE_ALPHA_CONFIG (0x014C0)`，`0x50CE → MWV207REG_BLT_CLUSTER_CONTROL (0x14338)`。
推导、验证与 170/219 的映射覆盖率见 [registers/REGISTERS.md](registers/REGISTERS.md)，
字段位域见 [registers/REGISTERS-FIELDS.md](registers/REGISTERS-FIELDS.md)。

### 2.2 寄存器定义被剥离

驱动自带的 `src/gallium/drivers/mwv207/include/hw/mwv207reg_*.h` 是**空壳**
（只有 include guard；`mwv207reg_common.h` 仅定义 `DISABLE 0x0` / `ENABLE 0x1`）。
`include/mwv207_cmd.h` 里的 `VALUE/NAME/MASK/...` 依赖 `reg##_##field##_Offset` 一类宏，
在本发行版中因此不可用；驱动代码于是通篇直接写裸十六进制常量。
本仓库的 `docs/registers/` 用厂商 KMD 头文件补全了这层信息。

### 2.3 三套格式码值空间

| 表 | 位置 | 消费者 | 码值空间 |
|---|---|---|---|
| `texture_infos[]` | `mwv207_format.c` | `mwv207_get_texture_desc()` → `mwv207_sampler.c` | `MWV207REG_TX_EXT_CONFIG_FORMAT_*`（6 位） |
| `blitInfos[]` | `mwv207_format.c` | `mwv207_get_blit_desc()` → `mwv207_blit.c`、`mwv207_get_image_info()` | `MWV207REG_BLT_SRC_CONFIG_FORMAT_*`（6 位） |
| `pixel_infos[]` | `mwv207_format.c` | `mwv207_get_pixel_desc()` → `mwv207_surface.c` | 另一套编号（厂商头文件中未定位，未确认） |

**注意**：资源镜像描述符（`pLevel->config` 的 FORMAT 域）取自 `blitInfos[]`，
而不是 `texture_infos[]`；两者必须同时登记某个格式，该格式才能正确采样。

## 3. 文件清单与职责

| 文件 | 行数 | 职责 |
|---|---:|---|
| `drivers/mwv207/include/hw/mwv207reg_3d.h` | 30 |  |
| `drivers/mwv207/include/hw/mwv207reg_common.h` | 33 |  |
| `drivers/mwv207/include/hw/mwv207reg_shader.h` | 30 |  |
| `drivers/mwv207/include/hw/mwv207reg_texture.h` | 30 |  |
| `drivers/mwv207/include/mwv207_blit.h` | 98 |  |
| `drivers/mwv207/include/mwv207_cmd.h` | 223 |  |
| `drivers/mwv207/include/mwv207_context.h` | 190 |  |
| `drivers/mwv207/include/mwv207_format.h` | 182 |  |
| `drivers/mwv207/include/mwv207_query.h` | 55 |  |
| `drivers/mwv207/include/mwv207_resource.h` | 153 |  |
| `drivers/mwv207/include/mwv207_screen.h` | 56 |  |
| `drivers/mwv207/include/mwv207_shader.h` | 105 |  |
| `drivers/mwv207/include/mwv207_state.h` | 145 |  |
| `drivers/mwv207/include/mwv207_surface.h` | 125 |  |
| `drivers/mwv207/meson.build` | 118 |  |
| `drivers/mwv207/mwv207_blend.c` | 369 | 混合状态打包：alpha_config、alpha_mode_ext、advanced blend、逐 RT 独立混合 |
| `drivers/mwv207/mwv207_blit.c` | 1239 | BLT/DMA：mwv207_blit_resource_box、mwv207_dma、mwv207_can_blit、格式转换与缩放支持矩阵 |
| `drivers/mwv207/mwv207_cmd.c` | 372 | 命令缓冲管理：创建、增长、提交封装、reloc 与 dat 列表 |
| `drivers/mwv207/mwv207_compute.c` | 319 | compute shader 发射路径 |
| `drivers/mwv207/mwv207_context.c` | 232 | pipe_context 创建/销毁，虚表装配，draw_vbo/flush 入口 |
| `drivers/mwv207/mwv207_draw.c` | 1297 | primitive 映射、draw_vbo、clear、遮挡查询、一帧命令发射序列 |
| `drivers/mwv207/mwv207_format.c` | 963 | 三张格式表 texture_infos[]/pixel_infos[]/blitInfos[] 与查询函数；int_format 语义 |
| `drivers/mwv207/mwv207_query.c` | 349 | occlusion / timestamp / pipeline statistics 查询 |
| `drivers/mwv207/mwv207_resource.c` | 661 | BO 与 miptree：布局决策（LINEAR/SUPER_TILED）、transfer_map/unmap、staging 与 DMA 路径 |
| `drivers/mwv207/mwv207_sampler.c` | 1083 | 纹理/采样器描述符构建：tex_config/tex_config2/tex_config_ext 位域、swizzle、过滤、地址模式 |
| `drivers/mwv207/mwv207_screen.c` | 679 | pipe_screen 实现：能力声明（129 个 PIPE_CAP_* + 28 个 PIPE_SHADER_CAP_*），is_format_supported 入口，资源创建入口 |
| `drivers/mwv207/mwv207_shader.c` | 1198 | shader state：NIR 接入、编译产物缓存、bind/delete |
| `drivers/mwv207/mwv207_state.c` | 499 | pipe 状态对象 → 硬件寄存器原子（viewport/scissor/framebuffer/vertex_elements/...） |
| `drivers/mwv207/mwv207_surface.c` | 363 | pipe_surface：硬件镜像描述符（pLevel->config 等）装配，render target 绑定 |
| `drivers/mwv207/mwv207_vertex.c` | 567 | 顶点缓冲、属性格式、索引类型、instance 处理、立即数（IMMEDIATE_*）机制 |
| `drivers/mwv207/nir/mwv207_cmdline.c` | 170 | （nir/）调试开关（环境变量/命令行） |
| `drivers/mwv207/nir/mwv207_common.c` | 326 | （nir/）公用小工具 |
| `drivers/mwv207/nir/mwv207_common.h` | 298 |  |
| `drivers/mwv207/nir/mwv207_compiler.c` | 666 | （nir/）编译管线入口 mwv207_compile_shader()，pass 编排 |
| `drivers/mwv207/nir/mwv207_compiler.h` | 194 |  |
| `drivers/mwv207/nir/mwv207_disasm.c` | 1317 | （nir/）MWV207 ISA 反汇编器（调试用） |
| `drivers/mwv207/nir/mwv207_disasm.h` | 328 |  |
| `drivers/mwv207/nir/mwv207_emit.c` | 2029 | （nir/）NIR → 硬件指令选型与编码（最大文件） |
| `drivers/mwv207/nir/mwv207_emit.h` | 115 |  |
| `drivers/mwv207/nir/mwv207_liveness.c` | 257 | （nir/）活跃性分析 |
| `drivers/mwv207/nir/mwv207_lower.c` | 1687 | （nir/）驱动的 NIR lowering：IO/ALU/global/sprite/ubo-to-uniform |
| `drivers/mwv207/nir/mwv207_lower.h` | 49 |  |
| `drivers/mwv207/nir/mwv207_lower_64bit.c` | 786 | （nir/）64 位整数/浮点拆分与仿真 |
| `drivers/mwv207/nir/mwv207_lower_dual.c` | 269 |  |
| `drivers/mwv207/nir/mwv207_lower_shared_atomic.c` | 194 |  |
| `drivers/mwv207/nir/mwv207_lower_tg4.c` | 111 |  |
| `drivers/mwv207/nir/mwv207_nir_opt.h` | 39 |  |
| `drivers/mwv207/nir/mwv207_nir_opt_peephole_fdiv.c` | 127 |  |
| `drivers/mwv207/nir/mwv207_nir_opt_peephole_ffma.c` | 234 |  |
| `drivers/mwv207/nir/mwv207_nir_opt_peephole_set.c` | 188 |  |
| `drivers/mwv207/nir/mwv207_opcode.c` | 154 | （nir/）NIR opcode → 硬件编码对照表 |
| `drivers/mwv207/nir/mwv207_ra.c` | 856 | （nir/）寄存器分配与延迟/冲突处理 |
| `drivers/mwv207/video-codec/common/mwv207_codec_time.c` | 68 |  |
| `drivers/mwv207/video-codec/common/mwv207_codec_time.h` | 63 |  |
| `drivers/mwv207/video-codec/common/mwv207_decoder_commonvp9.h` | 460 |  |
| `drivers/mwv207/video-codec/common/mwv207_decoder_h264_nal_unit.c` | 149 |  |
| `drivers/mwv207/video-codec/common/mwv207_decoder_h264_nal_unit.h` | 96 |  |
| `drivers/mwv207/video-codec/common/mwv207_decoder_h264_slice_header.c` | 887 |  |
| `drivers/mwv207/video-codec/common/mwv207_decoder_h264_slice_header.h` | 197 |  |
| `drivers/mwv207/video-codec/common/mwv207_decoder_hevc_exp_golomb.c` | 181 |  |
| `drivers/mwv207/video-codec/common/mwv207_decoder_hevc_exp_golomb.h` | 58 |  |
| `drivers/mwv207/video-codec/common/mwv207_decoder_hevc_slice_header.c` | 457 |  |
| `drivers/mwv207/video-codec/common/mwv207_decoder_hevc_slice_header.h` | 107 |  |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_stream.c` | 338 |  |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_stream.h` | 54 |  |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_util.c` | 102 |  |
| `drivers/mwv207/video-codec/common/mwv207_decoder_sw_util.h` | 121 |  |
| `drivers/mwv207/video-codec/common/mwv207_decoder_vp9_slice_header.c` | 1532 |  |
| `drivers/mwv207/video-codec/common/mwv207_decoder_vp9_slice_header.h` | 768 |  |
| `drivers/mwv207/video-codec/common/mwv207_log.c` | 58 |  |
| `drivers/mwv207/video-codec/common/mwv207_log.h` | 62 |  |
| `drivers/mwv207/video-codec/common/mwv207_utils.h` | 39 |  |
| `drivers/mwv207/video-codec/common/mwv207_video_cs.c` | 136 |  |
| `drivers/mwv207/video-codec/common/mwv207_video_cs.h` | 51 |  |
| `drivers/mwv207/video-codec/common/mwv207_video_enums.h` | 102 |  |
| `drivers/mwv207/video-codec/common/mwv207_video_info.h` | 277 |  |
| `drivers/mwv207/video-codec/common/mwv207_video_reg_buf.c` | 131 |  |
| `drivers/mwv207/video-codec/common/mwv207_video_reg_buf.h` | 86 |  |
| `drivers/mwv207/video-codec/common/mwv207_video_state.h` | 765 |  |
| `drivers/mwv207/video-codec/common/mwv207_vp9hwd_bool.c` | 364 |  |
| `drivers/mwv207/video-codec/common/mwv207_vp9hwd_bool.h` | 75 |  |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_api_common.h` | 569 |  |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_h264.c` | 2378 | H264 私有解码器：slice 解析 → 寄存器序列 |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_h264.h` | 39 |  |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_hevc.c` | 1332 | HEVC 私有解码器：slice 解析 → 寄存器序列 |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_hevc.h` | 34 |  |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_ppu.c` | 2088 | 解码后处理（PPU） |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_ppu.h` | 323 |  |
| `drivers/mwv207/video-codec/mwv207/mwv207_dec_register_array_7020.h` | 889 |  |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec.c` | 242 |  |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec.h` | 165 |  |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_8170enum.h` | 830 |  |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_8170g1table.h` | 1344 |  |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_8170g2table.h` | 1344 |  |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_8170table.h` | 1871 |  |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_cfg.h` | 321 |  |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_cs.h` | 65 |  |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_reg.c` | 175 |  |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_reg.h` | 135 |  |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_vpu_features_list.h` | 2148 |  |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_vpufeature.c` | 62 |  |
| `drivers/mwv207/video-codec/mwv207/mwv207_video_dec_vpufeature.h` | 191 |  |
| `drivers/mwv207/video-codec/mwv207_video_buffer.c` | 611 | 视频 surface/buffer 分配与格式（NV12 优先） |
| `drivers/mwv207/video-codec/mwv207_video_buffer.h` | 219 |  |
| `drivers/mwv207/video-codec/mwv207_video_codec.c` | 154 | 视频接入层：profile/entrypoint 能力声明，PIPE_VIDEO_CAP_* 实现 |
| `drivers/mwv207/video-codec/mwv207_video_codec.h` | 43 |  |
| `drivers/mwv207/video-codec/mwv207_video_decoder.c` | 650 | 解码器公共框架：目标 surface、DPB、提交与同步 |
| `drivers/mwv207/video-codec/mwv207_video_decoder.h` | 93 |  |
| `winsys/mwv207/drm/meson.build` | 33 |  |
| `winsys/mwv207/drm/mwv207_drm.h` | 215 | （winsys）UMD 侧 DRM ioctl/结构体定义 |
| `winsys/mwv207/drm/mwv207_drm_bo.c` | 575 | （winsys）BO 创建/映射/哈希表/销毁 |
| `winsys/mwv207/drm/mwv207_drm_bo.h` | 33 |  |
| `winsys/mwv207/drm/mwv207_drm_cs.c` | 460 | （winsys）命令缓冲、reloc 列表、fence fd、SUBMIT ioctl |
| `winsys/mwv207/drm/mwv207_drm_cs.h` | 37 |  |
| `winsys/mwv207/drm/mwv207_drm_public.h` | 37 |  |
| `winsys/mwv207/drm/mwv207_drm_winsys.c` | 276 | （winsys）screen 创建、fd 管理、幂等哈希 |
| `winsys/mwv207/drm/mwv207_drm_winsys.h` | 285 |  |
| **合计 117 个文件** | **50207** | |

## 4. 数据流

### 4.1 一帧绘制

```
glDrawArrays
 → src/mesa/main 状态校验/合并
 → st_draw_vbo  (state_tracker)
 → pipe_context::draw_vbo  → mwv207_draw_vbo()
     ├─ 把脏状态 (atoms) 翻译成寄存器写入命令缓冲
     ├─ mwv207_emit_vertex / emit_index 生成顶点/索引数据
     ├─ 发射 draw 寄存器
     └─ mwv207_submit() → winsys cs → DRM_IOCTL_MWV207_SUBMIT
 → KMD 调度到硬件
 → 出 fence fd，UID 侧 poll/等待
```

### 4.2 纹理上传（glTexImage2D）——**存在已知坑**

```
glTexImage2D
 → st_TexImage → st_TexSubImage
 → pipe_context::texture_subdata = u_default_texture_subdata
 → pipe_context::texture_map   = mwv207_transfer_map()
      非 LINEAR 资源：新建 staging 资源（PIPE_USAGE_STAGING|PIPE_BIND_LINEAR）
      → winsys bo_map → 返回 CPU 可写指针
 → memcpy 用户数据到 staging（CPU，字节精确）
 → pipe_context::texture_unmap = mwv207_transfer_unmap()
      用 2D BLT（mwv207_blit_resource_box）把 staging 拷进真实资源
```

**坑**：最后那一步 BLT 对 16 位多通道布局不能保持字节精确，
必须让这些格式走 LINEAR（见 `mwv207_tile_supported_format()` 与
`patches/0010-mwv207-16bit-formats-linear-layout.patch`）。

### 4.3 着色器编译

```
GLSL → src/compiler/glsl → NIR
 → mwv207_create_shader_state()  (mwv207_shader.c)
 → mwv207_compile_shader()       (nir/mwv207_compiler.c)
     nir_shader_clone → 一系列 mwv207_* lowering / 优化 pass
     → mwv207_ra（寄存器分配）→ 指令编码 (mwv207_emit.c)
 → 生成 MWV207 ISA 二进制，装进 shader 描述符
```

细节见 [04-NIR-COMPILER.md](04-NIR-COMPILER.md)。

### 4.4 视频解码

```
VA-API / VDPAU / FFmpeg  →  pipe_video_codec (st_vdpau/vl)
 → video-codec/mwv207_video_codec.c
 → mwv207_video_decoder.c → mwv207_dec_h264.c / mwv207_dec_hevc.c
 → 寄存器序列（set_regs）→ 命令提交 → DEC 硬件
 → mwv207_dec_ppu.c 后处理 → 输出 surface
```

细节见 [05-VIDEO-CODEC.md](05-VIDEO-CODEC.md)。

## 5. 构建、安装与加载

**目录约定（2026-11-02 起）**：驱动源码在 `src/mwv207-umd/`，上游 Mesa 树在
`ref/oh-mesa/User_source_code/mesa-24.1.2/`（只读参考 + 构建宿主），后者的
`src/gallium/{drivers,winsys}/mwv207` 是指向 `src/mwv207-umd` 的符号链接。

```bash
# 1) 配置（首次；这一套选项是"能装进 switch-mwv207.sh 布局"的唯一组合）
meson setup ref/oh-mesa/User_source_code/mesa-24.1.2/build-full \
  ref/oh-mesa/User_source_code/mesa-24.1.2 \
  -Dgallium-drivers=mwv207 -Dvulkan-drivers= \
  -Dplatforms=x11,wayland -Dglx=dri -Ddri3=enabled -Dgbm=enabled -Degl=enabled \
  -Dglvnd=disabled -Dgles1=enabled -Dgles2=enabled -Dshared-glapi=enabled \
  -Dtools=[] -Dvideo-codecs=all -Dshader-cache=enabled \
  -Dgallium-va=enabled -Dgallium-vdpau=enabled -Dgallium-rusticl=true \
  -Dgallium-xa=disabled -Dgallium-opencl=disabled -Dgallium-omx=disabled \
  -Dllvm=enabled -Ddraw-use-llvm=true -Dshared-llvm=enabled \
  -Dzstd=disabled -Dvalgrind=disabled -Dlibunwind=disabled -Dlmsensors=disabled \
  --buildtype=debugoptimized

# 2) 构建（先保证符号链接在，再编）
./tools/build-umd.sh build

# 3) 免安装试跑：装配一个"模拟安装目录"，所有库戳自洽
./tools/build-umd.sh sim          # → /tmp/mwv207-sim
DISPLAY=:0 LD_LIBRARY_PATH=/tmp/mwv207-sim LIBGL_DRIVERS_PATH=/tmp/mwv207-sim \
  MESA_LOADER_DRIVER_OVERRIDE=mwv207 glxinfo -B

# 4) 出载荷并正式安装
./tools/make-payload.sh -v 24.1.2-fullstack-rN   # → payload-N/
./switch-mwv207.sh umd check                     # 只读预检
sudo ./switch-mwv207.sh umd install              # 会重启 lightdm
```

**两个必须显式指定的选项**（否则产物名字/行为随环境漂移，装不进脚本布局）：

- `-Dglvnd=disabled`：留给 `auto` 时，重配一次就可能变成 glvnd 模式，产物从
  `libGL_mwv207.so.1.2.0` / `libGLESv2_mwv207.so.2.0.0` 变成
  `libGLX_mwv207.so.0` / 不再生成 GLES，`switch-mwv207.sh` 的 `REAL_FILES`
  期待的名字全部对不上。**踩过一次**：`meson setup --wipe` 后整个 GL 栈装不上。
- `-Dgles1=enabled -Dgles2=enabled`：默认 `auto` 在 glvnd 模式下不产 GLES。

**版本戳陷阱**：Mesa 的 `24.1.2 (git-<sha>)` 取自**上游树构建时的 git HEAD**。
驱动加载器会逐字比对 DRI 驱动与 `libGL`/`libEGL` 的戳，不一致就
`DRI driver not from this Mesa build` + 回退软件渲染 + `X Error BadValue
(X_GLXCreateNewContext)`。所以：
- 改了 `src/mwv207-umd/` 只重建 DRI → 戳不变（仍是上游树的 HEAD），可用；
- 一旦上游树有新提交（如"驱动迁出"那次），重建后戳就变了 → 必须
  `./tools/build-umd.sh sim` 或整体出载荷，让所有库用同一个戳。

产物路径：`build-full/src/gallium/targets/dri/libmwv207_dri.so`，
并有一份 `mwv207_dri.so` 符号链接（DRI 加载器按 `<driver>_dri.so` 查找）。

## 6. Mesa 版本与 API 边界

本驱动基线为 **Mesa 24.1.2**。向上游移植时的已知断点：

| 版本 | 变化 | 影响 |
|---|---|---|
| 25.3.0 | 删除 `enum pipe_shader_type` 与 `PIPE_SHADER_*`（改用 `mesa_shader_stage`） | 驱动内 289 处引用需改写 |
| 25.3.0 | 删除 VDPAU / OMX 前端 | 丢失 VDPAU 输出通路 |
| 26.0.0 | `PIPE_CAP_*` 枚举改为 `struct pipe_caps` | `get_param()` 129 处 + `get_shader_param()` 28 处需重写 |
| 26.0.0 | 删除 `nir_def::parent_instr`（改 accessor） | 63 处引用需改写 |
| 26.0.0 | 删除 `pipe_context::create_surface` / `surface_destroy` | 驱动表面层需重写 |
| 26.0.0 | 删除 `vl/vl_decoder.h` | 视频层需自带 `vl_level_supported` 实现 |

## 7. 建议阅读顺序

1. 本文档 + [registers/REGISTERS.md](registers/REGISTERS.md) — 建立整体与寄存器视图
2. [02-CORE-OBJECTS.md](02-CORE-OBJECTS.md) — 对象模型与能力声明
3. [03-DRAW-PIPELINE.md](03-DRAW-PIPELINE.md) — 最常用的绘制通路
4. [01-WINSYS-KMD.md](01-WINSYS-KMD.md) — 向下到内核的契约
5. [04-NIR-COMPILER.md](04-NIR-COMPILER.md) — 编译器（难度最高）
6. [05-VIDEO-CODEC.md](05-VIDEO-CODEC.md) — 视频解码（体量最大）
