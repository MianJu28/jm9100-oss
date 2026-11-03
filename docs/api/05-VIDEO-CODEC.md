# 05 · 视频编解码（video-codec/）

> 范围：`src/gallium/drivers/mwv207/video-codec/` 全部 56 个文件、**26099 行**（`wc -l` 实测）。
> 方法：逐文件 `read` 通读；表文件（`*table.h`）按表格语义与索引方式解析（用脚本还原全量映射）。
> 结论均给 `文件:行号`；读不出来的在 [§13 未确认](#13-未确认与疑点清单) 明列。
> 相关文档：[00-ARCHITECTURE.md](00-ARCHITECTURE.md)（§4.4）、[../registers/REGISTERS.md](../registers/REGISTERS.md)、
> [01-WINSYS-KMD.md](01-WINSYS-KMD.md)、[06-KNOWN-ISSUES.md](06-KNOWN-ISSUES.md)、
> 报告：[`../reports/HWDECODE-FEASIBILITY-REPORT.md`](../../reports/HWDECODE-FEASIBILITY-REPORT.md)、
> [`../reports/FULLFEATURE-REPORT.md`](../../reports/FULLFEATURE-REPORT.md)。

---

## 0. 结论速览

| 项目 | 结论 | 依据 |
|---|---|---|
| 硬件解码 | **H.264 与 HEVC 可用**；H.264 8bit/High10（走 `DEC_MODE_H264_HIGH10P`）、HEVC Main/Main10 | `mwv207_dec_h264.c:2312-2377`、`mwv207_dec_hevc.c:1224-1331` |
| VP9 | **解析器齐全但完全没有寄存器序列**，且无任何调用者（纯死代码，随库一起编译） | `meson.build:77-78`；全树 grep 无调用 |
| 编码 | **完全没有**。`encode_bitstream` 从未赋值，profile 开关直接排除 ENCODE 入口 | `mwv207_video_decoder.c:618-623`、`mwv207_video_codec.c:51` |
| MPEG2/MPEG4/VC1/JPEG | 有 `end_frame` 分支但**函数体为空**，不产生任何命令流；硬编码探测的芯片能力表里这些标志均为 0 | `mwv207_video_decoder.c:538-549`、`mwv207_video_dec_vpu_features_list.h:40-49` |
| 命令提交 | DEC 引擎独立命令缓冲，`MWV207_SUBMIT_ENGINE_TYPE_DEC=1` | `common/mwv207_video_cs.c:40` |
| fence 同步 | **正常路径不建 fence、不等待**；只靠下一帧 `begin_frame()` 的 `bo_wait` 间接串行化 | `common/mwv207_video_cs.c:133`、`mwv207_video_decoder.c:419-428` |
| 寄存器可读名 | 视频块（JMDEC）**没有厂商可读寄存器名/地址**，只有逻辑名 + `hw_dec_reg_spec` 表 | `docs/registers/register-db.json` 无 JMDEC 项 |

---

## 1. 分层结构

### 1.1 分层图

```
   应用（ffmpeg / mpv / gstreamer / XvBA）
        │  libva（VA-API）
        ▼
  ┌──────────────────────────────────────────────────────────────────────┐
  │ Mesa VA 前端  src/gallium/frontends/va/                              │
  │   config.c:56-76   vaQueryConfigProfiles → vl_codec_supported()       │
  │   config.c:84-110  vaQueryConfigEntrypoints → VAEntrypointVLD        │
  │   picture_h264.c / picture_hevc.c  → 填充 pipe_h264/h265_picture_desc│
  └──────────────────────────────────────────────────────────────────────┘
        │  vl/vl_video_buffer.c、vl_codec_supported() → screen->get_video_param()
        │  pipe_video_codec / pipe_video_buffer 虚表
        ▼
  ┌──────────────────────────────────────────────────────────────────────┐
  │ 驱动接入层  video-codec/*.c/.h                                       │
  │   mwv207_video_codec.c     profile / cap / format 声明 + 钩子安装     │
  │   mwv207_video_buffer.c    解码目标 buffer（多 plane 资源、sampler）  │
  │   mwv207_video_decoder.c   pipe_video_codec 实现（帧流程 + ES 搬运）  │
  └──────────────────────────────────────────────────────────────────────┘
        │  mwv207_9200_video_dec_picture()  （按 video_format 分派）
        ▼
  ┌──────────────────────────────────────────────────────────────────────┐
  │ 私有解码器  video-codec/mwv207/                                      │
  │   mwv207_video_dec.c       公共配置寄存器 + 分派 + PPU 参数初始化      │
  │   mwv207_dec_h264.c        H.264 set_regs（2378 行，103 处 set_reg）  │
  │   mwv207_dec_hevc.c        HEVC  set_regs（1332 行，109 处 set_reg）  │
  │   mwv207_dec_ppu.c         PPU 后处理寄存器（9 处直接 + 全走 ppu_regs）│
  │   mwv207_video_dec_reg.c   逻辑寄存器号 → regbuf 字/位域 的间接层      │
  │   mwv207_video_dec_vpufeature.c  按 hw_build_id 选芯片能力记录         │
  └──────────────────────────────────────────────────────────────────────┘
        │  逻辑名 set_reg(reg_buf, JMDEC_XXX, v)
        ▼
  ┌──────────────────────────────────────────────────────────────────────┐
  │ 寄存器数组  uint32_t regbuf[DEC_X170_REGISTERS=503]                   │
  │   common/mwv207_video_reg_buf.c   分配/清零/BO 排序/dump              │
  └──────────────────────────────────────────────────────────────────────┘
        │  bos_sort() 后按 swreg 升序重排 BO 表
        ▼
  ┌──────────────────────────────────────────────────────────────────────┐
  │ 命令提交  common/mwv207_video_cs.c                                   │
  │   cs_submit(): 503 字寄存器镜像 → 4 条 opcode=0x01 写寄存器命令        │
  │   copy_one_cmd() 在同地址处插入 reloc（LOW32 / HIGH32）               │
  │   → winsys cmdbuf_commit(engine_type=DEC)                            │
  └──────────────────────────────────────────────────────────────────────┘
        │  DRM_IOCTL_MWV207_SUBMIT（cmd ≤ 16K + reloc 表 + bo 表）
        ▼
  ┌──────────────────────────────────────────────────────────────────────┐
  │ KMD  src/kernel/drivers/gpu/drm/mwv207/mwv207_pipe_dec.c              │
  │   opcode 0x01 → 逐字 MMIO 写；写到字节偏移 4 时取 bit0 作为 hw_run    │
  │   opcode 0x02 → 软件回写（sw_pp），本 UMD **不使用**                  │
  │   poll 线程轮询 0x4 的 0x1100 位 → dma_fence_signal                  │
  └──────────────────────────────────────────────────────────────────────┘
        │
        ▼
  DEC 硬件（JMDEC，寄存器窗口 503×4 = 2012 字节）+ PPU 后处理
```

### 1.2 三个"层"之间只有一张虚表

驱动**没有**为视频单独定义 pipe 接口；它实现的是 gallium 公共虚表：

| 虚表成员 | 安装点 | 实现 |
|---|---|---|
| `screen->get_video_param` | `mwv207_video_codec.c:141` | `mwv207_get_video_param()` |
| `screen->is_video_format_supported` | `mwv207_video_codec.c:142-143` | `mwv207_is_video_format_supported()` |
| `context->create_video_codec` | `mwv207_video_codec.c:144` | `mwv207_create_codec()` → `mwv207_create_decoder()` |
| `context->create_video_buffer` | `mwv207_video_codec.c:145` | `mwv207_pipe_video_buffer_create()` |
| `codec->begin_frame / decode_bitstream / end_frame / flush / destroy` | `mwv207_video_decoder.c:618-623` | 见 §4 |
| `codec->decode_macroblock` | `mwv207_video_decoder.c:621` | **`assert(0)`（`438-444`），未实现** |
| `codec->encode_bitstream` | **从未赋值** | 编码不存在 |

钩子总入口在 context 创建时调用一次：

```c
/* mwv207_context.c:215 */
mwv207_context_video_codec_init(&ctx->base);
```

`mwv207_video_codec.c:136-146` 中先 `mwv207_log_init()`（`:139`），再装 4 个钩子。
`mwv207_video_codec.h:36-42` 只导出两个符号：`mwv207_context_video_codec_init()` 与 `mwv207_create_codec()`。

### 1.3 编译接入

`src/gallium/drivers/mwv207/meson.build`：include 目录在 `:23`（三个子目录），
源文件在 `:58-78`，**19 个 .c 全部编入 `libmwv207_dri.so`**——包括 VP9 的两个文件（`:77-78`）：

| 行 | 文件 | 行 | 文件 |
|---|---|---|---|
| 58 | `video-codec/mwv207_video_buffer.c` | 69 | `mwv207/mwv207_video_dec_vpufeature.c` |
| 59 | `video-codec/mwv207_video_codec.c` | 70 | `mwv207/mwv207_dec_ppu.c` |
| 60 | `video-codec/mwv207_video_decoder.c` | 71 | `mwv207/mwv207_dec_h264.c` |
| 61 | `common/mwv207_log.c` | 72 | `mwv207/mwv207_dec_hevc.c` |
| 62 | `common/mwv207_codec_time.c` | 73 | `common/mwv207_decoder_h264_nal_unit.c` |
| 63 | `common/mwv207_video_reg_buf.c` | 74 | `common/mwv207_decoder_h264_slice_header.c` |
| 64 | `common/mwv207_video_cs.c` | 75 | `common/mwv207_decoder_hevc_exp_golomb.c` |
| 65 | `common/mwv207_decoder_sw_stream.c` | 76 | `common/mwv207_decoder_hevc_slice_header.c` |
| 66 | `common/mwv207_decoder_sw_util.c` | 77 | `common/mwv207_decoder_vp9_slice_header.c` |
| 67 | `mwv207/mwv207_video_dec.c` | 78 | `common/mwv207_vp9hwd_bool.c` |
| 68 | `mwv207/mwv207_video_dec_reg.c` | | |

> 关键推论：**VP9 的解析器与 bool 解码器是编进库里的**，缺的只是寄存器序列（与
> `FULLFEATURE-REPORT.md` §4.1 一致）。

---

## 2. 能力声明

### 2.1 profile / entrypoint 能力表

`mwv207_profile_supported(screen, profile, entrypoint)`（`mwv207_video_codec.c:34-55`）：

| profile | 行号 | 判定 |
|---|---|---|
| `PIPE_VIDEO_PROFILE_MPEG4_AVC_BASELINE` | 41 | ✅ |
| `PIPE_VIDEO_PROFILE_MPEG4_AVC_CONSTRAINED_BASELINE` | 42 | ✅ |
| `PIPE_VIDEO_PROFILE_MPEG4_AVC_MAIN` | 43 | ✅ |
| `PIPE_VIDEO_PROFILE_MPEG4_AVC_EXTENDED` | 44 | ✅ |
| `PIPE_VIDEO_PROFILE_MPEG4_AVC_HIGH` | 45 | ✅ |
| `PIPE_VIDEO_PROFILE_MPEG4_AVC_HIGH10` | 46 | ✅ |
| `PIPE_VIDEO_PROFILE_MPEG4_AVC_HIGH422` | 47 | ⚠️ 声明支持，但解码路径强制 4:2:0（`mwv207_dec_h264.c:1184`） |
| `PIPE_VIDEO_PROFILE_MPEG4_AVC_HIGH444` | 48 | ⚠️ 同上 |
| `PIPE_VIDEO_PROFILE_HEVC_MAIN` | 49 | ✅ |
| `PIPE_VIDEO_PROFILE_HEVC_MAIN_10` | 50 | ✅ |
| 其它（含 HEVC_MAIN_12/444、VP9、AV1、VC1…） | 54 | ❌ `return false` |

返回值为（`:51`）：

```c
return entrypoint != PIPE_VIDEO_ENTRYPOINT_ENCODE;
```

即：**只要 profile 命中，就支持除 ENCODE 以外的任何入口**——`PIPE_VIDEO_ENTRYPOINT_IDCT` /
`PIPE_VIDEO_ENTRYPOINT_MC` 也被"宣称"支持，但 `decode_macroblock` 是 `assert(0)`
（`mwv207_video_decoder.c:434-444`）。VA 前端只问 BITSTREAM（见 §2.5），所以实际暴露 6 个 VLD 入口。

### 2.2 `PIPE_VIDEO_CAP_*` 实现

`mwv207_get_video_param()`（`mwv207_video_codec.c:57-85`）：

| CAP | 行号 | 返回值 |
|---|---|---|
| `PIPE_VIDEO_CAP_SUPPORTED` | 64-65 | `mwv207_profile_supported(...)` |
| `PIPE_VIDEO_CAP_NPOT_TEXTURES` | 66-67 | `1`（非 2 的幂纹理支持 → buffer 对齐走 64 而非 POT） |
| `PIPE_VIDEO_CAP_MAX_WIDTH` | 68-69 | `vl_video_buffer_max_size(pscreen)` = `PIPE_CAP_MAX_TEXTURE_2D_SIZE` |
| `PIPE_VIDEO_CAP_MAX_HEIGHT` | 70 | 同上 |
| `PIPE_VIDEO_CAP_PREFERED_FORMAT` | 71-73 | `PIPE_FORMAT_NV12` |
| `PIPE_VIDEO_CAP_PREFERS_INTERLACED` | 74-75 | `false` |
| `PIPE_VIDEO_CAP_SUPPORTS_INTERLACED` | 76-77 | `false` |
| `PIPE_VIDEO_CAP_SUPPORTS_PROGRESSIVE` | 78-79 | `true` |
| `PIPE_VIDEO_CAP_MAX_LEVEL` | 80-81 | `vl_level_supported(pscreen, profile)` |
| 其它全部（`STACKED_FRAMES`、`ENC_*`、`ROI_CROP_DEC`…） | 82-83 | `0` |

> 注意 `SUPPORTS_INTERLACED=false` 与解码器**内部**大量场/MBAFF 代码（`dec_h264_set_picparam_regs`
> 的 `JMDEC_FIELDPIC_FLAG_E` 等，`mwv207_dec_h264.c:582-590`）并存——即**场解码逻辑在，但对外声明不支持**，
> 上层不会构造场图。

### 2.3 视频格式支持

`mwv207_is_video_format_supported()`（`mwv207_video_codec.c:87-145`）：

- 格式开关接受 `PIPE_FORMAT_NV12`（`:97-99`）与 `PIPE_FORMAT_P010`/`P016`
  （`:100-112`；2026-11-03 新增：仅当 `R16_UNORM` / `R16G16_UNORM` 的
  `SAMPLER_VIEW` 受支持时才接受 10bit，理由见 `:102-105` 注释）；
- `:117-125` 注释记录上一版“OR 语义谎报 10bit 支持”的教训（见报告 §7）；
- `:126-143` 用 **AND** 语义合并 profile 与格式两个开关，`PIPE_VIDEO_PROFILE_UNKNOWN` 例外放行
  （VPP/image 通路只需要格式、不带 profile）。

另有 `mwv207_video_buffer_is_format_supported()`（`mwv207_video_buffer.c:102-130`）按 plane 检查
`SAMPLER_VIEW` / `RENDER_TARGET`，但**screen 钩子用的是前者**，该函数在 `mwv207_video_buffer.h:60` 只是导出。

### 2.4 缓冲区格式与 plane

| 函数 | 行号 | 行为 |
|---|---|---|
| `mwv207_get_video_buffer_formats` | `mwv207_video_buffer.c:50-67` | 按 plane 取 `util_format_get_plane_format`；YUYV→`R8G8_R8B8_UNORM`、UYVY→`G8R8_B8R8_UNORM` |
| `mwv207_video_buffer_plane_order` | `:69-88` | YV12→YVU；NV12/P010/P016/YUYV/UYVY→YUV |
| `mwv207_video_buffer_surface_format` | `:90-100` | SUBSAMPLED 一律映射到 `R8G8B8A8_UNORM` |
| `mwv207_video_buffer_create` | `:366-405` | 因 NPOT=1，宽高按 `MWV207_MACROBLOCK_WIDTH/HEIGHT=64` 对齐（`:386-389`） |
| `mwv207_pipe_video_buffer_create` | `:545-563` | 真正钩子；强制 `PIPE_BIND_LINEAR`（`:550`） |

### 2.5 VA 前端如何消费这些声明（跨层验证）

`src/gallium/auxiliary/vl/vl_codec.c:74`（`vl_codec_supported()` 整体位于 `31-75`，`74` 即其中的 `return screen->get_video_param(...)`）：

```c
return screen->get_video_param(screen, profile,
        encode ? PIPE_VIDEO_ENTRYPOINT_ENCODE : PIPE_VIDEO_ENTRYPOINT_BITSTREAM,
        PIPE_VIDEO_CAP_SUPPORTED);
```

- `vaQueryConfigProfiles`（`frontends/va/config.c:56-70`）用 `vl_codec_supported(...,false)` 遍历
  `PIPE_VIDEO_PROFILE_MPEG2_SIMPLE … AV1_MAIN`，命中者才有 VA profile，最后无条件追加 `VAProfileNone`；
- `vaQueryConfigEntrypoints`（`config.c:84-110`）：`false` 分支通过 → `VAEntrypointVLD`；
  `true`（encode）分支因 `:51` 恒 false → **不出现 `VAEntrypointEncSlice`**。
- 结果与实测一致：`VAProfileH264{ConstrainedBaseline,Main,High,High10}` +
  `VAProfileHEVC{Main,Main10}` 共 6 个 VLD 入口（报告 §"回归结果 C"）。

### 2.6 芯片能力记录（**硬编码探测**）

两个入口函数都用**固定**的 `hw_build_id` 去查能力表：

```c
mwv207_video_dec_get_hw_features_by_id(0x00001F76, &dec->hw_feature);  /* dec_h264.c:2321-2322 */
mwv207_video_dec_get_hw_features_by_id(0x00001F76, &dec->hw_feature);  /* dec_hevc.c:1231-1232 */
```

查表实现 `mwv207_video_dec_vpufeature.c:36-56`：线性扫描 `feature_list[]`，
`(hw_build_id & id_mask) == id` 匹配后 `memcpy`；循环上界是 `sizeof(...)-1`，**最后一行是兜底**
（该行的 `.id = 0x00000000`）。`feature_list[]` 由 `mwv207_video_dec_vpu_features_list.h` 文本包含
（`:32-34`），共 15 条目。

命中的第一条即 `0x1F76`（`mwv207_video_dec_vpu_features_list.h:30`，`id_mask=0xFFFFFFFF`）：

| 字段 | 值 | 影响 |
|---|---|---|
| `h264_support` / `h264_high10_support` | 3 / 1 | 走 HIF10P 专用模式 |
| `hevc_support` / `hevc_main10_support` | 1 / 1 | HEVC 8/10bit |
| `vp9_support` / `vp9_profile2_support` | 1 / 1 | **硬件有能力，驱动没实现** |
| `mpeg4` / `custom_mpeg4` / `mpeg2` / `vc1` | 0 / 0 / 0 / 0 | 解释 `end_frame` 空分支（§4.1） |
| `jpeg_support` / `avs/avs2` | 1 / 1 | 未接 |
| `*_max_dec_pic_*`（h264/hevc/vp9） | 4096×4096 | 上限 |
| `ref_buf_support` | 0 | 不用硬件参考缓冲管理 |
| `tiled_mode_support` / `field_dpb_support` | 1 / 1 | → `tiled_stride_enable=1`（`dec_h264.c:592-593`） |
| `rfc_support` / `rfc_stride_support` | 1 / 1 | 视频压缩（RFC）能力在，但驱动 `use_video_compressor=0` |
| `pic_size_reg_unified` | 1 | HEVC 走 `*_8K` 变体寄存器（`dec_hevc.c:608-622`） |
| `max_ppu_count` | 1 | 只有 PPU0 有效（`dec_ppu.c:398-400` 关掉其余） |
| `pp_version` | `UNFIED_PP` | 统一 PPU |
| `fmt_p010_support` | 1 | 10bit 直出 P010 的硬件前提 |
| `pp_up_lanczos` / `pp_down_lanczos` | {0} / {0} | **Lanczos 通路永不启用**（`dec_ppu.c:403-410`） |
| `rotation_support` / `dither_support` | 0 / 0 | 旋转/抖动寄存器仍会被写，值来自配置 |
| `crop_step_unit` / `crop_step_rshift` | 2 / 1 | PPU 裁剪坐标右移 1（`dec_ppu.c:1322-1339`） |

编译期配置另有一套 `mwv207_video_dec_cfg.h`（321 行）：`DEC_X170_USING_IRQ 1`（`:65-68`）、
`DEC_MAX_PPU_COUNT 5`（→ `DEC_X170_REGISTERS 503`、`PP_UNIFIED_REGS 183`，`:249-281`）、
`MWV207DEC_STREAM_SWAP` 展开为 `0`（`:170-179`）、`MWV207DEC_TIMEOUT_OVERRIDE 0x500000`（`:189-191`）。

---

## 3. 关键数据结构

### 3.1 接入层

| 结构 | 位置 | 要点 |
|---|---|---|
| `struct mwv207_video_decoder` | `mwv207_video_decoder.h:47-76` | `base`（pipe_video_codec）、`ws`、`cs`、`picture_desc`（h264/h265 union）、`bs_buffers[4]`、`bs_ptr/bs_size`、`streams[16]/streams_len[16]/streams_num`、`decoder`（私有 `mwv207_video_dec*`）、`video_format`、`codec_time` |
| `MWV207_NUM_BUFFERS` | `mwv207_video_decoder.h:45` | `4`（bitstream staging 环形缓冲） |
| `struct mwv207_video_buffer` | `mwv207_video_buffer.h:71-85` | `num_planes`、`resources[VL_NUM_COMPONENTS]`、`sampler_view_planes/components`、`surfaces[VL_MAX_SURFACES]`、`res`、`virtual_addr`、`mwv207_offset_info`、`tiled_pic_size` |
| `struct mwv207_offset` | `mwv207_video_buffer.h:43-65` | 单帧内存布局：`luma_size`、`pic_buff_size`、`pp_buff_size`、`sync_mc_offset`、`dir_mv_offset`、`cbs_tbl_offset*`、`chroma_base_offset`、`fake_tbl*` |

### 3.2 私有解码器

| 结构 | 位置 | 要点 |
|---|---|---|
| `struct mwv207_video_dec` | `mwv207/mwv207_video_dec.h:119-150` | `dec_param`、`hw_feature`、`reg_buf`、`ppu_cfg[5]`、`cabac_buffer[4]`、`slice_header`、`list0/list1/list_p[34]`、`ref_frames[16]`、`ref_frames_h265[16]`、`short_ref[32]`、`long_ref[32]`、`poc` |
| `struct mwv207_video_dec_param` | `mwv207_video_dec.h:66-100` | `high10p_mode`、`w_align`、`rlc_mode`、`bit_depth`、`pp_enabled`、`dpb_mode`、`tiled_stride_enable`、`use_video_compressor`、`tile_info_offset`、`sao_mem_offset`、`bsd_control_mem_offset`… |
| `H264POCContext` | `mwv207_video_dec.h:102-113` | POC 递推状态（`prev_poc_msb` 初值 `1<<16`，`mwv207_video_dec.c:137`） |
| `struct mwv207_picture_h264` | `common/mwv207_video_state.h:309-317` | DPB 槽位：`picture`、`frame_num`、`is_long_term`、`top/bottom_is_reference`、`top/bottom_field_order_cnt` |
| `struct mwv207_picture_h265` | `common/mwv207_video_state.h:571-581` | 同上 + `PicOrderCntVal` |

> `common/mwv207_video_state.h`（765 行）是驱动自编的 VA 风格参数集，**不是** VA 头文件
> （`#include <va/va.h>` 全部被注释掉）。其中 `mwv207_h264_enc_*` / `mwv207_h265_enc_*`
> （`:352-492`）与 `mwv207_video_info.h`（277 行）的 `mwv207_video_h264/h265/vc1/mpeg2/mpeg4`
> 结构在**本树内无任何使用者**，属遗留代码。

---

## 4. 解码管线（bitstream → 硬件寄存器）

### 4.1 帧生命周期

```
VA: vaBeginPicture → vaRenderPicture(数据) → vaEndPicture
      │                     │                  │
      ▼                     ▼                  ▼
 codec->begin_frame()   codec->decode_bitstream()  codec->end_frame()
  mwv207_video_decoder.c:406    :449                     :521
```

| 阶段 | 实现 | 关键动作 |
|---|---|---|
| 创建 | `mwv207_create_decoder()` `mwv207_video_decoder.c:592-649` | `bs_buf_size = width*height*3`（`:633`）；建 4 个 `PIPE_USAGE_STAGING` 的 bitstream 缓冲（`:634-643`）；`mwv207_video_dec_create(ws)`（`:629`）建私有对象与 `reg_buf`；`mwv207_video_cs_init(&dec->cs, ws)`（`:630`）建 DEC 命令缓冲 |
| `begin_frame` | `:406-429` | ① 记录 codec_time 起点（`:417`）；② `ws->bo_wait(bs_buffers[cur_buffer]->res->bo, OS_TIMEOUT_INFINITE)`（`:419-421`）——**等这一号 bitstream 缓冲上一轮用完**；③ `bs_size=0` + `bo_map` 得 `bs_ptr`（`:422-424`）；④ 若 target 有 `res` 也 `bo_wait`（`:426-428`） |
| `decode_bitstream` | `:449-516` | 累积 ES 到 `streams[0]/streams_len[0]` 与 `bs_ptr/bs_size`；`num_buffers==3` 时跳过 `i==0`（VA 前端的 slice-data 布局，`:456-490`）；检测 3 字节起始码 `00 00 01` 后拷贝 `buffers[i]+3`（`:474-486`）；多 buffer 时每段另存 `streams[streams_num++]`（`:504-514`） |
| `end_frame` | `:521-582` | 见下 |

`end_frame` 的精确顺序：

```c
switch (u_reduce_video_profile(picture->profile)) {       /* :530 */
case PIPE_VIDEO_FORMAT_MPEG4_AVC: get_h264_msg(dec, pic); break;   /* :531-533 */
case PIPE_VIDEO_FORMAT_HEVC:      get_h265_msg(dec, pic); break;   /* :535-537 */
case VC1: case MPEG12: case MPEG4: case JPEG: break;      /* :538-549 空体 */
default: return;                                          /* :550-552 直接不解码！ */
}
mwv207_9200_video_dec_picture(dec, target);               /* :555  → §4.2 分派 */
mwv207_video_cs_submit(&dec->cs, dec->decoder->reg_buf);  /* :563  → §5.2 提交 */
dec->streams_num = 0; (各 streams_len 清零)                /* :565-566 */
dec->decoder->reg_buf->bos_clear(...);                    /* :567 清寄存器镜像 */
codec_time 收尾 / 计算                                  /* :569-570 */
mwv207_video_dec_destroy_buffer(dec->decoder);          /* :572 释放 tile_edge/misc_buffer */
/* POC / 帧号推进 */                                      /* :574-578 */
++dec->cur_buffer; dec->cur_buffer %= MWV207_NUM_BUFFERS; /* :580-581 */
```

> **性能与正确性的关键点**：`begin_frame()`（下一帧）里的 `bo_wait` 是**唯一的输出同步**。
> `end_frame` 自身**不建 fence、不等待**（§5.4）。也就是说，帧 N 的结果由帧 N+1 的 `begin_frame`
> 隐式等待——单帧结束时若应用立即 `vaGetImage`，同步由 VA 前端/vl 层的 buffer 等待兜住。

### 4.2 分派：`mwv207_9200_video_dec_picture()`

`mwv207/mwv207_video_dec.c:180-193`：

| `dec->video_format` | 调用 | 行号 |
|---|---|---|
| `MWV207_VIDEO_FORMAT_MPEG4_AVC` | `mwv207_dec_h264_set_regs(decoder, target)` | `:183-184` |
| `MWV207_VIDEO_FORMAT_HEVC` | `mwv207_dec_hevc_set_regs(decoder, target)` | `:185-187` |
| **VP9（无分支）** | **什么都不做**，随后 `reg_buf->bos_sort(reg_buf)`（`:192`）提交一坨全零/陈旧寄存器 | — |

`video_format` 由 `get_h264_msg()`（`mwv207_video_decoder.c:62`）和 `get_h265_msg()`（`:194`）设置；
VP9 分支不存在，`dec->video_format` 也**没有任何地方被赋值为 `MWV207_VIDEO_FORMAT_VP9`**
（全树 grep 仅枚举定义处出现）。

### 4.3 H.264 与 HEVC 的管线差异

| 环节 | H.264 | HEVC |
|---|---|---|
| 入口 | `mwv207_dec_h264_set_regs` `dec_h264.c:2312-2377` | `mwv207_dec_hevc_set_regs` `dec_hevc.c:1224-1331` |
| `JMDEC_DEC_MODE` | 在 `dec_h264_set_common_regs` 内按 `high10p_mode` 写 `DEC_MODE_H264_HIGH10P`(`dec_h264.c:463`) 或 `DEC_MODE_H264`(`:468`) | 显式写 `DEC_MODE_HEVC`(`dec_hevc.c:1249`)，**且必须在 `mwv207_dec_set_common_config_regs()` 之前**（后者读 DEC_MODE 决定 `DEC_TAB_SWAP`，`mwv207_video_dec.c:42-49`） |
| 本帧语法解析 | `dec_h264_set_sh_regs`（`:1172-1316`）**真正解析** slice header：NAL→slice，置 `is_reference`、IDR 检测→`idr()`、POC 计算（`:1205-1243`） | `dec_hevc_set_skip_length_reg`（`:668-709`）**只为算跳过长**：重解析首 slice 得 `hw_skip_bits` → `JMDEC_HDR_SKIP_LENGTH` |
| 解析产物 | `dec->slice_header` + `pic_param->my_field_order_cnt[]` + DPB 更新 | 仅 `JMDEC_HDR_SKIP_LENGTH`（HEVC 的 DPB 信息来自 VA 前端传入的 `pic_param`） |
| 参考列表 | `dec_h264_set_rpl_regs`（`:1716-1851`），含 directB 硬件限制的槽位重映射 workaround（`:1737-1786`） | `dec_hevc_set_rpl_reg`（`:895-1048`），`qsort`+`CompareDeltaPoc` 按 POC 差排序构造 list0/list1（`:998-1042`） |
| 输出地址 | `dec_h264_set_cur_out_buffer_info`（`:1854-1962`）+ `dec_h264_set_ref_buffer_info`（`:1964-2084`） | `dec_hevc_set_out_reg`（`:711-863`）+ 参考地址写在 `set_rpl_reg` 内（`:923-954`） |
| CABAC 初值表 | `dec_h264_set_cabac_data`（`:2128-2235`）：920 word 常量表（`:296-450`）+ 缩放矩阵 + POC → `JMDEC_QTABLE_BASE` | 无（HEVC CABAC 初值内置） |
| 缩放矩阵 | 塞进 CABAC buffer 尾部（`:2187-2218`） | 塞进 `misc_buffer` 偏移 0（`:1149-1190`） |
| Tile | 无 | 网格表写进 `misc_buffer + tile_info_offset`（`:1106-1147`）→ `JMDEC_TILE_BASE` |
| 第二输出（10bit P010） | 无 | G2 才有的 `DEC_RSY/RSC_BASE`（`:797-832`） |
| DPB 维护 | `mwv207_h264_execute_ref_pic_marking` + `fill_my_ref_frames`（`:2373-2376`） | **无** |
| 寄存器 dump | `reg_buf->dump_regs()`（`:2371`） | **无** |
| 10bit 表达 | `high10p_mode=1` + `bit_depth=10` | `bit_depth=10` + `use_p010_output=1`（`:1243-1245`） |

---

## 5. 寄存器机制与命令提交

### 5.1 三层寄存器表示

驱动用**三套名字**描述同一个寄存器，转换发生在 `mwv207/mwv207_video_dec_reg.c`：

```
逻辑名（源码里写的 JMDEC_*）
   │  enum 序号 id  ← mwv207_video_dec_8170enum.h 第 id 行（1840 项）
   ▼
hw_dec_reg_spec[id] = {word, width, shift, flag}   ← mwv207_video_dec_8170table.h 第 id 行（1840 项，下标严格对齐）
   │
   ▼
regbuf[word] 的 bit[shift+width-1 : shift]         ← uint32_t regbuf[503]，DEC_X170_REGISTERS
   │
   ▼ cs_submit 打包成 opcode=0x01 命令
命令寄存器号 = word（KMD: word*4 = MMIO 字节偏移）
```

字段写入公式（`mwv207/mwv207_video_dec_reg.c:53-82`）：

```c
tmp  = reg_base[spec[id][0]];
tmp &= ~(reg_mask[spec[id][1]] << spec[id][2]);
tmp |=  (value & reg_mask[spec[id][1]]) << spec[id][2];
```

- `reg_mask[33]`（`:36-42`）= 0,1,3,7,…,0xFFFFFFFF，即 N bit 掩码；
- **`spec[id][0] == 0` 直接 `return;`（`:67-70`）**：1840 项中 **225 项 `word==0`**，即这些逻辑寄存器
  在本代芯片上不存在——所有针对它们的写都是**静默空操作**（如 `JMDEC_DEC_LATENCY`、`JMDEC_DEC_OUT_ENDIAN`、
  `JMDEC_OUTPUT_8_BITS`、`JMDEC_DEC_OUT_SWAP` 等，`dec_h264.c:471-480` 里就有一批）；
- 三项特殊追加：`JMDEC_DEC_IRQ_STAT = {1,13,11,0}`、`JMDEC_PP_IRQ_STAT = {60,2,12,0}`（`reg.c:48-49`）。

名字表 `registername[JMDEC_LAST_REG][50]`（`reg.c:32-34`）由 `mwv207_dec_register_array_7020.h`（1840 个字符串）
文本初始化，**只用于 trace 日志**（`MWV207_LEVEL_TRACE_REG`）。

### 5.2 regbuf → 4 条硬件写命令（`common/mwv207_video_cs.c:81-135`）

`mwv207_video_cs_submit()` 把 503 字镜像拆成 **4 条 opcode=1（写寄存器）命令**递交：

| # | 命令字 | 行号 | opcode | 字数 | 起始寄存器 | 覆盖 register word |
|---|---|---|---|---|---|---|
| 1 | `0x08FF0003` | `:90-92` | 1 | `0xFF`=255 | 3 | 3 … 257 |
| 2 | `0x08F50102` | `:101-105` | 1 | `0xF5`=245 | 258 | 258 … 502 |
| 3 | `0x08010002` | `:107-109` | 1 | 1 | 2 | 2 |
| 4 | `0x08010001` | `:111-114` | 1 | 1 | 1 | 1 |

合计 502 字（word 1…502），**word 0 永不发送**（与 `spec[id][0]==0` 的"占位"语义一致）。
顺序刻意安排为**大块 → 尾块 → word2 → word1**：`JMDEC_DEC_E` 落在 **word 1 bit 0**（`regmap` 已验证），
因此"启动解码"是最后写入的寄存器；KMD 也在看到"写 word 1"这条命令时读取 bit0 作为 `hw_run`
（`mwv207_pipe_dec.c:176-180`）。

命令字编码（`../registers/REGISTERS.md` §1.1）：`bits[31:27]` 操作码、`bits[26:16]` 字数、`bits[15:0]` 寄存器号；
这里 `0x08` 前缀 = 操作码 1（写寄存器）。

### 5.3 BO 地址回填（reloc）

寄存器里存的是 GPU 地址，不能提前写死。驱动流程：

1. 寄存器序列阶段调用
   `reg_buf->set_addr_reg(reg_buf, JMDEC_XXX_MSB, bo, offset, flags)` / `set_addr_reg_hi(...)`
   （`mwv207/mwv207_video_dec_reg.c:84-122`）写高位低 32 位，并登记一条
   `MWV207_SUBMIT_RELOC_LOW32` / `_HIGH32` 记录，`swreg = spec[id][0]`；
2. `end_frame` 里 `reg_buf->bos_sort(reg_buf)`（`mwv207_video_dec.c:192`）按 `swreg` 升序 qsort
   （`common/mwv207_video_reg_buf.c:42-54`）；
3. 提交时 `copy_one_cmd()`（`common/mwv207_video_cs.c:52-79`）逐字拷贝，一旦
   `offset == cur_bo->swreg` 就用 `cmdbuf_add_bo()` + `cmdbuf_add_relloc(cmd, idx, cmd_offset, bo_offset, type, pad)`
   追加 BO 与重定位项（`:64-73`），`bo_offset = cmdbuf_cur[0]`（即刚写下的低 32 位）。

BO 表容量 500（`mwv207_video_reg_buf.h:45-80`），寄存器镜像 503 字（`reg_buf.c:105-106`）。

### 5.4 命令缓冲与 fence

`mwv207_video_cs_init()`（`common/mwv207_video_cs.c:35-42`）：

```c
cs->cmd = ws->cmdbuf_create(ws);
cs->cmd->engine_type = MWV207_SUBMIT_ENGINE_TYPE_DEC;   /* :40 */
```

提交（`cs.c:118-133`）分两条路：

| 条件 | 行为 |
|---|---|
| `ws->debug & MWV207_DEBUG_VIDEO`（`:118`） | `cmdbuf_commit(..., &out_fence_fd)`（`:123`）→ `fence_create` → **`fence_wait(OS_TIMEOUT_INFINITE)`（`:124-129`）**，并统计 codec_wait_hw_use_time |
| 正常 | `cmdbuf_commit(ws, cmd, -1, NULL)`（`:133`），**不建 fence、不等待** |

KMD 侧最终以 `dma_fence` 返回：`mwv207_pipe_dec_submit()` 分配 `struct pipe_dec_fence`，
若检测到 `hw_run` 则挂到 `pipe->current_fence`；硬件完成由 **1ms 轮询线程**
（`mwv207_pipe_dec.c:74-104`，读 0x4 判断 `(stat & 0x1100) == 0x1100`）触发 `dma_fence_signal()`。
调试开关 `MWV207_DEBUG_VIDEO` 打开时，`reg_buf->dump_reg_stream` 会把寄存器流写入 `./mwv207_video_reg.dump`
（`common/mwv207_video_reg_buf.c:108-112`）。

---

## 6. H.264 寄存器序列（`mwv207/mwv207_dec_h264.c`）

> 全部寄存器号 = `hw_dec_reg_spec` 的 **word 索引**，也就是 KMD 的 `reg*4` 中的 `reg`
> （即 DEC MMIO 字节地址 = word×4）。**厂商可读名不存在**：`docs/registers/register-db.json`
> 里没有任何 JMDEC 条目（只有 106 个 `MWV207REG_DEC400_*`，那是 2D/3D 的压缩块），
> 因此下文"厂商名"列一律为 `—`；名称只能用驱动逻辑名。

### 6.1 入口 `mwv207_dec_h264_set_regs()`（`:2312-2377`）的调用顺序

| 步 | 函数 | 行号 | 作用 | 涉及逻辑寄存器数 |
|---|---|---|---|---|
| 0 | 能力探测 | `:2321-2322` | `get_hw_features_by_id(0x1F76)`（硬编码） | — |
| 0 | 参数初始化 | `:2323-2334` | `bit_depth` 8/10、`w_align=6`、`pp_enabled=1`、`use_video_compressor=0`、`allow_dpb_field_ordering=1`、`cur_pic=target` | — |
| 1 | `dec_h264_set_common_regs` | `452-490` | codec 模式 + 公共配置 + tap 系数 | 18 |
| 2 | `dec_h264_set_picparam_regs` | `492-655` | PPS/SPS → 画面尺寸、QP、场、步长 | 32 |
| 3 | `dec_h264_set_sh_regs` | `1172-1316` | **解析 slice header**（NAL→slice）、IDR、POC、ref-idx | 10 |
| 3.5 | `my_ref_frames` 回填 | `:2342-2350` | 首帧 `ref_frames[0]` 为空时用解析出的参考帧 | — |
| 4 | `dec_h264_set_rpl_regs` | `1716-1851` | 参考列表（含 directB 槽位重映射） | 经索引数组 |
| 5 | `dec_avc_calc_sur_offset` | `1318-1388` | 计算本帧 `tiled_pic_size` 与各段偏移（**不写寄存器**） | 0 |
| 6 | `dec_ensure_surface_res` | `1390-1403` | 按需分配输出 surface | 0 |
| 7 | `dec_h264_set_cur_out_buffer_info` | `1854-1962` | 输出/色度/运动矢量地址 + DPB 模式 | 14 |
| 8 | `dec_h264_set_ref_buffer_info` | `1964-2084` | 16 个参考槽地址 + 有效/长参考位图 | 4 + 槽族 |
| 9 | `dec_h264_avc_set_pp_regs` | `1405-1457` | PPU 参数初始化与寄存器（→ §9） | 1 + PPU |
| 10 | `dec_avc_set_matrix_flag` | `2086-2126` | 缩放矩阵启用位 + 位深寄存器 | 3 |
| 11 | `dec_h264_set_cabac_data` | `2128-2235` | 组装 CABAC/缩放/POC 表缓冲 | 2 |
| 12 | `dec_h264_set_stream_data` | `2237-2276` | bitstream 基址与长度 | 8 |
| 13 | `JMDEC_DEC_E = 1` | `:2370` | **最后写启动位**（word 1 bit0） | 1 |
| 14 | `dump_regs()` | `:2371` | 调试 dump | — |
| 15 | `execute_ref_pic_marking` + `fill_my_ref_frames` | `:2373-2376` | 仅当 `is_reference`，**DPB 维护在提交之后** | — |

### 6.2 `dec_h264_set_common_regs()`（`:452-490`）

H.264 特有部分（`:459-480`）：

| 寄存器（逻辑名） | word / 字节 | 位域 | 值 | 行 |
|---|---|---|---|---|
| `JMDEC_DEC_MODE` | 3 / 0x00C | w5 s27 | `DEC_MODE_H264_HIGH10P`=**15** 或 `DEC_MODE_H264`=**0** | `463` / `468` |
| `JMDEC_RLC_MODE_E` | 3 / 0x00C | — | 0 | `464` / `469` |
| `JMDEC_BUFF_ID` | **word 0** | — | 0 → **空操作** | `465` |
| `JMDEC_DEC_LATENCY` | **word 0** | — | 0 → **空操作** | `471` |
| `JMDEC_DEC_CLK_GATE_E` | 2 / 0x008 | w1 s10 | 0（注意：公共块稍后又写 1） | `472` |
| `JMDEC_DEC_OUT_ENDIAN` | **word 0** | — | 1 → **空操作** | `473` |
| `JMDEC_DEC_MAX_BURST` | 58 / 0x0E8 | w8 s0 | 16 | `474` |
| `JMDEC_DEC_DATA_DISC_E` | **word 0** | — | 0 → **空操作** | `475` |
| `JMDEC_SERV_MERGE_DIS` | 58 / 0x0E8 | — | 0 | `476` |
| `JMDEC_DEC_OUT_DIS` | 3 / 0x00C | — | 0 | `477` |
| `JMDEC_CH_8PIX_ILEAV_E` | 7 / 0x01C | — | 0 | `478` |
| `JMDEC_PP_OUT_E_U` | 320 / 0x500 | w1 s0 | `pp_enabled`（=1） | `479` |
| `JMDEC_PP_CR_FIRST` | 320 / 0x500 | w1 s1 | `cr_first` | `480` |
| `JMDEC_PRED_BC_TAP_0_0/1/2` | 49 / 0x0C4 | — | 1 / (uint32)-5 / 20 | `485-487` |
| `JMDEC_BUFFER_EMPTY_INT_E` | 3 / 0x00C | — | 1 | `488` |
| `JMDEC_BLOCK_BUFFER_MODE_E` | 3 / 0x00C | — | 0 | `489` |

> ⚠️ **模式选择的实际含义**：`:459` 的条件是 `dec->dec_param.rlc_mode == 0 && frame_mbs_only_flag`。
> 全树 grep 确认 **`rlc_mode` 从未被赋非零值**（`mwv207_video_dec.h:75` 定义、`dec_h264.c:459` 唯一读取处），
> 所以对**所有逐行（frame_mbs_only）H.264 流**驱动都会走 `DEC_MODE_H264_HIGH10P`(=15) 这条路，
> 与 `DWL_CLIENT_TYPE_H264_MAIN10`(=15) 同值；真正的 8bit/10bit 由 `bit_depth` 参数区分
> （`:2323-2327`：仅当 `bit_depth_luma/chroma_minus8 != 0` 才是 10）。是否所有 8bit 流都适合这条
> "HIGH10P" 流水线，**未确认**（实测 1080p 解码正确、CPU -82%，见报告）。

随后调用公共块 `mwv207_dec_set_common_config_regs()`（`mwv207/mwv207_video_dec.c:33-114`）：

| 寄存器 | word / 字节 | 值 | 行 |
|---|---|---|---|
| `DEC_STRM_SWAP` / `DEC_PIC_SWAP` / `DEC_DIRMV_SWAP` | 2 / 0x008（s28 / s24 / s20） | `MWV207DEC_STREAM_SWAP` 展开为 **0** | `video_dec.c:38-40`、`cfg.h:170-179` |
| `DEC_TAB_SWAP` | 2 / 0x008（s12） | HEVC/VP9 → 0；否则 `SWAP_8\|SWAP_16`（**H.264 → 非 0**） | `video_dec.c:42-49` |
| `DEC_BUSWIDTH` | 58 / 0x0E8（w3 s8） | `DEC_X170_BUS_WIDTH` = `BUS_WIDTH_128` = **2** | `:51-52`、`cfg.h:50,167` |
| `DEC_MAX_BURST` | 58 / 0x0E8（w8 s0） | `DEC_X170_BUS_BURST_LENGTH` = 16 | `:51-52`、`cfg.h:37,90` |
| `DEC_REFER_DOUBLEBUFFER_E` | 58 / 0x0E8（w1 s15） | 0 | `:57` |
| `APF_DISABLE` / `APF_SINGLE_PU_MODE` / `APF_THRESHOLD` | 55 / 0x0DC（s31 / s30 / s0..15） | 0 / 0 / 0 | `:60-76` |
| `DEC_IRQ_DIS` | 1 / 0x004（w1 s4） | `DEC_X170_USING_IRQ==0` 才置 1；实测 **=0（IRQ 使能）** | `:78-82`、`cfg.h:65-68` |
| `DEC_OUT_EC_BYPASS` | 3 / 0x00C（w1 s8） | 0 | `:84` |
| `APF_ONE_PID` | 3 / 0x00C（w1 s7） | 0 | `:85` |
| `DEC_CLK_GATE_E` | 2 / 0x008（w1 s10） | `DEC_X170_INTERNAL_CLOCK_GATING` = 1 | `:87-88` |
| AXI RD/WD ID 使能 + ID | 未确认 | `DEC_X170_AXI_ID_R/W_E`=0、`_R/W`=0 | `:90-97`、`cfg.h:141-160` |
| `EXT_TIMEOUT_OVERRIDE_E` / `EXT_TIMEOUT_CYCLES` / `TIMEOUT_OVERRIDE_E` / `TIMEOUT_CYCLES` | 318 / 0x4F8、319 / 0x4FC（另两个未确认） | 由 `MWV207DEC_TIMEOUT_OVERRIDE`=0x500000 与 `DEC_X170_TIMEOUT_CYCLES`=0xFFFFFF 决定 | `:99-107`、`cfg.h:189-191,307` |
| `AXI_RD_OSTD_THRESHOLD` / `AXI_WR_OSTD_THRESHOLD` | 265 / 0x424（RD，w10 s18；WR 相邻位） | `DEC_X170_MAX_WR_OUTSTANDING_THRESHOLD` = 64 | `:108-113`、`cfg.h:310-311` |

### 6.3 `dec_h264_set_picparam_regs()`（`:492-655`）

| 寄存器 | word / 字节 | 来源 | 行 |
|---|---|---|---|
| `DEC_MULTICORE_E` / `DEC_WRITESTAT_E` | 58 / 0x0E8（s30 / s29） | `dec_param.mc_enabled` | `:507-508` |
| `MIN_CB_SIZE` / `MAX_CB_SIZE` | 12 / 0x030（s13 / s10） | 3 / 4（H.264 = 16×16） | `:514-515` |
| `PIC_WIDTH_IN_CBS` / `PIC_HEIGHT_IN_CBS` | 4 / 0x010（w13 s19 / s6） | `align(w,16)/16<<1` | `:517-519` |
| `PARTIAL_CTB_X/Y` | 20 / 0x050 | 0 | `:521-522` |
| `PIC_WIDTH_4X4` / `PIC_HEIGHT_4X4` | 20 / 0x050（s16 / s0） | `mbs<<2` | `:523-525` |
| `IGNORE_SLICE_ERROR_E` | 266 / 0x428 | 0 | `:528` |
| `INIT_QP` | 13 / 0x034（w7 s24） | `pic_init_qp_minus26 + 26` | `:531` |
| `REF_FRAMES` | 4 / 0x010（w5 s0） | `num_ref_frames` | `:535` |
| `FRAMENUM_LEN` / `FRAMENUM` | 7 / 0x01C（s16 / s0） | `ceil(log2(max_frame_num))` / `frame_num & ~0x1000` | `:538-544` |
| `CONST_INTRA_E` / `FILT_CTRL_PRES` / `RDPIC_CNT_PRES` | 8 / 0x020 | PPS 对应位 | `:546-552` |
| `CH_QP_OFFSET` / `CH_QP_OFFSET2` | 5 / 0x014 | `chroma_qp_index_offset` / `second_chroma_qp_index_offset` | `:555-557` |
| `DIR_8X8_INFER_E` / `WEIGHT_PRED_E` / `WEIGHT_BIPR_IDC` | 7 / 0x01C | SPS/PPS | `:561-566` |
| `FIELDPIC_FLAG_E` | 5 / 0x014 | `!frame_mbs_only_flag` | `:569` |
| `PIC_INTERLACE_E` / `PIC_FIELDMODE_E` / `SEQ_MBAFF_E` | 3 / 0x00C | 场/MBAFF | `:572-580` |
| `8X8TRANS_FLAG_E` | 8 / 0x020 | `transform_8x8_mode_flag` | `:584` |
| `BLACKWHITE_E` / `CABAC_E` | 7 / 0x01C | `chroma_format_idc==0` / `entropy_coding_mode_flag` | `:587-590` |
| `DEC_OUT_Y_STRIDE` / `DEC_OUT_C_STRIDE` | 314 / 0x4E8（s16 / s0） | `NEXT_MULTIPLE(...,ALIGN(w_align)*8)` 系列；`tiled_stride_enable = tiled_mode_support && field_dpb_support`（`:592-593`） | `:595-654` |

### 6.4 `dec_h264_set_sh_regs()`（`:1172-1316`）— 这里的解析最要紧

| 寄存器 | word / 字节 | 值 | 行 |
|---|---|---|---|
| `REFPIC_MK_LEN` | 8 / 0x020（w11 s17） | `slice_header.dec_ref_pic_marking.strm_len` | `:1248` |
| `IDR_PIC_E` | 8 / 0x020（w1 s16） | `IS_IDR_NAL_UNIT(&nal_unit)` | `:1251` |
| `IDR_PIC_ID_H10` | 12 / 0x030（w16 s16） | high10p 路径的 `idr_pic_id` | `:1254` |
| `IDR_PIC_ID` | 8 / 0x020（w16 s0） | 普通路径的 `idr_pic_id` | `:1257` |
| `PPS_ID` | 9 / 0x024（w8 s24） | `pic_parameter_set_id` | `:1260` |
| `POC_LENGTH` | 9 / 0x024（w8 s0） | `slice_header.poc_length_hw` | `:1262` |
| `PIC_TOPFIELD_E` | 3 / 0x00C（w1 s19） | `!bottom_field_flag` | `:1265` |
| `WRITE_MVS_E` | 3 / 0x00C | `nal_ref_idc!=0 \|\| NAL_CODED_SLICE_EXT` | `:1267` |
| `REFIDX0_ACTIVE` | 9 / 0x024（w5 s14） | P/SP 且 override=0 时 `l0_active_minus1+1` | `:1302` |
| `REFIDX1_ACTIVE` | 9 / 0x024（w5 s19） | B 且 override=0 时 `l1_active_minus1+1` | `:1311` |

同时这里做了三件"帧级"的事（**不是寄存器**）：置 `pic_param->is_reference`（`:1206-1210`）、
IDR 时调 `idr()` 清空 DPB 并复位 POC（`:1214-1227`，`idr()` 在 `:1163-1170`）、
算 `my_field_order_cnt[0..1]`（`:1232-1243`）。对 `streams_num > 1` 的每一段额外 stream 都会重新解析
一次以取 `num_ref_idx_active`（`:1272-1315`）。**`pic_param->pps.sps.chroma_format_idc` 被硬置为 1**
（`:1184`）——所以 4:2:2/4:4:4 声明（`mwv207_video_codec.c:47-48`）实际跑不出正确结果。

### 6.5 参考槽寄存器族（`:1964-2084` 与 `RPL`）

16 个槽的寄存器名不在 `set_reg()` 里字面出现，而是通过索引数组间接写
（`ref_base[16]` `:49-64`、`ref_base_msb` `:65-80`、`ref_cbase` `:82-97`、`ref_dbase` `:116-131`、
`ref_tybase` `:150-165`、`ref_tcbase` `:184-199` 等）：

| 族 | word 公式（n=0..15） | 槽0 / 槽15 | 位域 |
|---|---|---|---|
| `REFERn_BASE_MSB` / `_LSB`（=`YBASE`） | `66+2n` / `67+2n` | 66/0x108 · 67/0x10C → 96/0x180 · 97/0x184 | 32 位 |
| `REFERn_CBASE_MSB/LSB` | `100+2n` / `101+2n` | 100/0x190 · 101/0x194 → 130/0x208 · 131/0x20C | 32 位 |
| `REFERn_DBASE_MSB/LSB` | `134+2n` / `135+2n` | 134/0x218 · 135/0x21C → 164/0x290 · 165/0x294 | 32 位 |
| `REFERn_TYBASE_MSB/LSB` | `191+2n` / `192+2n` | 191/0x2FC · 192/0x300 → 221/0x374 · 222/0x378 | 32 位 |
| `REFERn_TCBASE_MSB/LSB` | `225+2n` / `226+2n` | 225/0x384 · 226/0x388 → 255/0x3FC · 256/0x400 | 32 位 |
| `REFERn_FIELD_E` | LSB 字 | 67/0x10C bit1（`TOPC_E` 为 bit0） | 1 位 |
| `REFERn_NBR` | `30 + n/2` | 30/0x078 s0（n=1 → s16；n=15 → 37/0x094 s16） | 16 位 |
| `REFER_LTERM_E` / `REFER_VALID_E` | 38/0x098 · 39/0x09C | 同左 | 32 位位图（驱动 `<<=16` 后写入，`:2077-2080`） |
| `CUR_POC_n` | `46 + n/4`，shift `24-8*(n%4)` | 46/0x0B8 s24 → 49/0x0C4 s0 | 8 位 |
| `INIT_RLIST_Fn`（list0）/ `_Bn`（list1）/ `PINIT_RLIST_Fn` | 14/0x038、47/0x0BC 起 | `INIT_RLIST_F0@14 s0`、`INIT_RLIST_B0@14 s5`、`PINIT_RLIST_F0@47 s10` | 5 位/项 |

### 6.6 输出缓冲与地址（`:1854-1962`）

| 寄存器 | word / 字节 | 指向 | 行 |
|---|---|---|---|
| `DEC_OUT_BASE_MSB/LSB`（=`YBASE`） | 64/0x100 · 65/0x104 | `target->res->bo` + 0 | `:1880-1889` |
| `DEC_OUT_CBASE_MSB/LSB` | 98/0x188 · 99/0x18C | 同 bo + `chroma_base_offset` | `:1906-1912` |
| `DIR_MV_BASE_MSB/LSB` | 132/0x210 · 133/0x214 | 同 bo + `dir_mv_offset` | `:1918-1923` |
| `DEC_OUT_TYBASE_MSB/LSB` | 189/0x2F4 · 190/0x2F8 | `resources[0]`（仅 `use_video_compressor`） | `:1931-1936` |
| `DEC_OUT_TCBASE_MSB/LSB` | 223/0x37C · 224/0x380 | `resources[1]` | `:1942-1947` |
| `DEC_OUT_EC_BYPASS` | 3 / 0x00C（w1 s8） | 压缩机开→0，否则 **1** | `:1952` |
| `DPB_ILACE_MODE` | 65 / 0x104（w1 s1，= `_LSB` 字） | `DEC_DPB_INTERLACED_FIELD`/`DEC_DPB_FRAME` | `:1958` |
| `TILED_MODE_MSB/LSB` | 2 / 0x008（w1 s17 / w1 s7） | `TILED_REF_8x4` 位拆写 | `:1960-1961` |

### 6.7 CABAC/缩放表与码流（`:2128-2276`）

| 寄存器 | word / 字节 | 指向/值 | 行 |
|---|---|---|---|
| `QTABLE_BASE_MSB/LSB` | 174/0x2B8 · 175/0x2BC | CABAC 缓冲（3680 B 初值表 + 224 B 缩放矩阵 + 136/144 B POC 表） | `:2220-2231` |
| `BIT_DEPTH_Y_MINUS8` / `_C_MINUS8` | 8 / 0x020 | `bit_depth-8`（仅 high10p） | `:2119-2121` |
| `TYPE1_QUANT_E` | 5 / 0x014 | `scaling_matrix_present_flag` | `:2124` |
| `START_CODE_E` | 13 / 0x034（w1 s31） | 1 | `:2248` |
| `STRM_START_OFFSET` | 259 / 0x40C | 0 | `:2249` |
| `STREAM_LEN` | 6 / 0x018（w32 s0） | `decoder->bs_size` | `:2250` |
| `STRM_BUFFER_LEN` | 258 / 0x408 | 缓冲总字节数 | `:2251` |
| `STREAM_BASE_MSB/LSB`（high10p） | 168/0x2A0 · 169/0x2A4 | `bs_buffers[cur_buffer]->res->bo` | `:2255-2260` |
| `RLC_VLC_BASE_MSB/LSB`（普通） | 168/0x2A0 · 169/0x2A4（**同一对寄存器**） | 同上 | `:2266-2271` |

> `DEC_TAB_SWAP` 在公共块里被置为"非 0"（H.264 分支），而 `JMDEC_DEC_TAB_SWAP` 正好落在 **word 2**——
> 这就是提交时为什么要把 word 2 单独作为一条命令（§5.2 第 3 条）紧随 word 1 之前发送。

---

## 7. HEVC 寄存器序列（`mwv207/mwv207_dec_hevc.c`）

### 7.1 入口 `mwv207_dec_hevc_set_regs()`（`:1224-1331`）的调用顺序

| 步 | 函数/动作 | 行号 | 说明 |
|---|---|---|---|
| 0 | 能力探测 + 参数 | `:1231-1245` | 同一个 `0x1F76`；`bit_depth>8` 时 `use_p010_output=1`（`:1243-1245`） |
| 1 | `dec_hevc_calc_sur_offset` | `279-375` | 算 `luma_size`/`dir_mv_offset`/`tiled_pic_size`，必要时分配 surface（`:371-374`） |
| 2 | `dec_hevc_set_pp_reg` | `377-419` | PPU（→ §9） |
| 3 | `JMDEC_DEC_MODE = DEC_MODE_HEVC` | `:1249` | **必须在下一步之前** |
| 4 | `mwv207_dec_set_common_config_regs` | `video_dec.c:33-114` | 读 DEC_MODE 决定 `DEC_TAB_SWAP`（HEVC → 0） |
| 5 | `dec_hevc_set_picparam_reg` | `421-666` | 62 个寄存器，SPS/PPS 直填 |
| 6 | `HDR_SKIP_LENGTH=0`、`APF_ONE_PID=0`、`MULTICORE_E=0`、`WRITESTAT_E=0` | `:1252-1258` | — |
| 7 | `dec_hevc_set_skip_length_reg` | `668-709` | 重解析首 slice → `HDR_SKIP_LENGTH` |
| 8 | `dec_hevc_set_out_reg` | `711-863` | 输出/色度/MV/步长 (+ G2 RS 死路径) |
| 9 | `dec_hevc_set_rpl_reg` | `895-1048` | 参考槽地址、POC 差、list0/list1 |
| 10 | `dec_hevc_check_and_alloc_asic_buffer` | `1050-1104` | 分配 `tile_edge` / `misc_buffer`，定各段 mem offset |
| 11 | `dec_hevc_set_misc_value_reg` | `1106-1191` | tile 网格 + 缩放矩阵写进 `misc_buffer` |
| 12 | `dec_hevc_set_stream_data_reg` | `1193-1222` | 码流地址/长度 |
| 13 | `TILE_BASE` / `SCALE_LIST_BASE` | `:1265-1288` | 指向 `misc_buffer` 的不同偏移 |
| 14 | `DEC_VERT_FILT_BASE` / `DEC_VERT_SAO_BASE` / `DEC_BSD_CTRL_BASE` | `:1290-1324` | 指向 `tile_edge` 的不同偏移（仅 tile 列数 >1） |
| 15 | `TILE_ENABLE` 重写 + `JMDEC_DEC_E=1` | `:1326-1328` | 启动 |

> 与 H.264 的两点差别值得强调：**HEVC 没有 `dump_regs()`，也没有任何 DPB 记账**
> （参考帧状态完全依赖 VA 前端传入的 `pic_param`，`get_h265_msg()` 在
> `mwv207_video_decoder.c:189-379` 把 RPS/POC/list 全部拷进来）。

### 7.2 `dec_hevc_set_picparam_reg()`（`:421-666`）寄存器表（62 项）

| 寄存器 | word / 字节 | 值（PPS/SPS 字段） | 行 |
|---|---|---|---|
| `BUFFER_EMPTY_INT_E` / `BLOCK_BUFFER_MODE_E` | 3 / 0x00C | 1 / 0 | 450-451 |
| `PIC_WIDTH_IN_CBS` / `PIC_HEIGHT_IN_CBS` | 4 / 0x010 | `>> (log2_min_cb+3)` | 452-453 |
| `PARTIAL_CTB_X/Y` | 20 / 0x050 | 非 CTB 对齐时为 1 | 454-455 |
| `PIC_WIDTH_4X4` / `PIC_HEIGHT_4X4` | 20 / 0x050 | `cbs*min_cb>>2` | 458-460 |
| `BLACKWHITE_E` | 7 / 0x01C | 0 | 462 |
| `BIT_DEPTH_Y_MINUS8` / `_C_MINUS8` | 8 / 0x020 | `bit_depth_minus8` | 465-468 |
| `PCM_E` / `PCM_BITDEPTH_Y/C` | 12/0x030 · 8/0x020 | PCM 使能与位深 | 472-479 |
| `REFPICLIST_MOD_E` | 12 / 0x030 | `lists_modification_present_flag` | 483 |
| `MIN_CB_SIZE` / `MAX_CB_SIZE` | 12 / 0x030 | `log2_min_cb+3` / `+log2_diff` | 487-492 |
| `MIN_TRB_SIZE` / `MAX_TRB_SIZE` | 13 / 0x034 | 变换块 log2 | 496-501 |
| `MIN_PCM_SIZE` / `MAX_PCM_SIZE` | 12 / 0x030 | PCM CTB log2 | 506-511 |
| `MAX_INTER_HIERDEPTH` / `MAX_INTRA_HIERDEPTH` | 13 / 0x034 | SPS hier depth | 517-520 |
| `ASYM_PRED_E` / `SAO_E` / `PCM_FILT_DISABLE` | 7 / 0x01C | SPS 标志 | 524-530 |
| `TEMPOR_MVP_E` / `SIGN_DATA_HIDE` | 5 / 0x014 | `temporal_mvp && !IDR` / `sign_data_hiding` | 534-539 |
| `CABAC_INIT_PRESENT` | 7 / 0x01C | PPS | 543 |
| `REFIDX0_ACTIVE` / `REFIDX1_ACTIVE` | 9 / 0x024 | PPS 默认值 | 547-550 |
| `INIT_QP` | 13 / 0x034 | `init_qp_minus26+26` | 554 |
| `CONST_INTRA_E` | 8 / 0x020 | PPS | 557 |
| `TRANSFORM_SKIP_E` | 12 / 0x030 | PPS | 561 |
| `CU_QPD_E` / `MAX_CU_QPD_DEPTH` / `CH_QP_OFFSET` / `CH_QP_OFFSET2` | 5 / 0x014 | PPS QP delta | 565-573 |
| `SLICE_CHQP_FLAG` / `WEIGHT_PRED_E` / `WEIGHT_BIPR_IDC` | 7 / 0x01C | PPS | 576-582 |
| `TRANSQ_BYPASS_E` | 12 / 0x030 | PPS | 585 |
| `DEPEND_SLICE_E` / `SLICE_HDR_EBITS` / `STRONG_SMOOTH_E` | 7 / 0x01C | PPS/SPS | 589-597 |
| `TILE_ENABLE` / `NUM_TILE_COLS` / `NUM_TILE_ROWS` | 10 / 0x028 | `tiles_enabled_flag`、列/行数 | 601-613 |
| `NUM_TILE_COLS_8K` / `NUM_TILE_ROWS_8K` | 10 / 0x028 | `pic_size_reg_unified==0` 时改用这两个（`:608-622`） | 617-620 |
| `ENTR_CODE_SYNCH_E` | 10 / 0x028 | `entropy_coding_sync_enabled_flag` | 625 |
| `FILT_SLICE_BORDER` / `FILT_TILE_BORDER` | 7 / 0x01C | PPS loop-filter 跨边界 | 630-634 |
| `FILT_CTRL_PRES` / `FILT_OVERRIDE_E` / `FILTERING_DIS` | 8/0x020 · 7/0x01C · 3/0x00C | deblocking 三态 | 638-649 |
| `FILT_OFFSET_BETA` / `FILT_OFFSET_TC` | 7 / 0x01C | `pps_beta/tc_offset_div2` | 653-655 |
| `PARALLEL_MERGE` | 13 / 0x034 | `log2_parallel_merge_level_minus2+2` | 658 |
| `IDR_PIC_E` | 8 / 0x020 | `pic_param->RAPPicFlag` | 661 |
| `SLICE_HDR_EXT_E` | 7 / 0x01C | `slice_segment_header_extension_present_flag` | 664 |

### 7.3 `dec_hevc_set_out_reg()`（`:711-863`）与 `set_rpl_reg()`（`:895-1048`）

| 寄存器 | word / 字节 | 指向/值 | 行 |
|---|---|---|---|
| `DEC_OUT_YBASE_MSB/LSB` | 64/0x100 · 65/0x104 | target bo + 0 | 731-736 |
| `DEC_OUT_CBASE_MSB/LSB` | 98/0x188 · 99/0x18C | target bo + `luma_size` | 742-747 |
| `WRITE_MVS_E` | 3 / 0x00C | **1**（HEVC 恒写运动矢量） | 752 |
| `DIR_MV_BASE_MSB/LSB` | 132/0x210 · 133/0x214 | + `dir_mv_offset` | 755-760 |
| `DEC_OUT_Y_STRIDE` / `DEC_OUT_C_STRIDE` | 314/0x4E8（s16/s0） | `NEXT_MULTIPLE(4*out_w*pixel_width, ALIGN(w_align)*8)/8` | 793-794 |
| `DEC_RSY_BASE_*` / `DEC_RSC_BASE_*` | **word 0 → 全为空操作** | G2 专用第二输出，且 `IS_G2_LEGACY(asic_id)` 因 `asic_id` 恒为 0 而**永不成立** | 812-828 |
| `DEC_OUT_TYBASE/TCBASE` | 189/0x2F4 · 223/0x37C（及 LSB） | `resources[0]/[1]`（仅压缩机） | 837-853 |
| `DEC_OUT_EC_BYPASS` | 3 / 0x00C | 压缩机开→0，否则 1 | 857 |
| `REFERn_YBASE_MSB/LSB` | `66+2n` / `67+2n` | 参考帧 bo + 0 | 923-954 |
| `REFERn_CBASE_MSB/LSB` | `100+2n` / `101+2n` | + `luma_size` | 923-954 |
| `REFERn_DBASE_MSB/LSB` | `134+2n` / `135+2n` | + `dir_mv_offset` | 923-954 |
| `REFER_LTERM_E` | 38 / 0x098 | `<<(15-i)` 长参考位图 | 956 |
| `CUR_POC_n`（`ref_poc_regs[16]`） | `46+n/4` | `CLIP3(-128,127, CurrPOC - RefPOC)` | 958-965 |
| `REF_FRAMES` | 4 / 0x010 | `NumPocStCurrBefore+After+NumPocLtCurr` | 996 |
| `INIT_RLIST_Fn`/`_Bn` | 14/0x038 起 | list0/list1（qsort `CompareDeltaPoc` 后按 POC 差拼装） | 1044-1047 |

### 7.4 HEVC 的两块辅助缓冲与两个疑点

| 缓冲 | 分配点 | 用途 | 寄存器 |
|---|---|---|---|
| `dec->tile_edge` | `:1074-1086`（仅 `num_tile_cols>1`） | 垂直滤波/SAO/BSD 控制 | `DEC_VERT_FILT_BASE`、`DEC_VERT_SAO_BASE`、`DEC_BSD_CTRL_BASE`（`:1290-1324`） |
| `dec->misc_buffer` | `:1088-1103` | 偏移 0：缩放矩阵；`tile_info_offset`：tile 网格 | `SCALE_LIST_BASE`（170/0x2A8）、`TILE_BASE`（166/0x298） |

- 疑点 1（**已确认的代码缺陷，后果未确认**）：`dec_hevc_check_and_alloc_asic_buffer()` 里
  `uint32_t pic_height = pic_param->pps.sps.bit_depth_chroma_minus8;`（`:1060`）与
  `uint32_t height64 = (pic_height + 63) & ~63;`（`:1067`）——变量名 `pic_height` 但取的**是位深**，
  tile 缓冲尺寸与真实图像高度无关。代入：8bit 时 `(0+63)&~63 = 0` → `tile_buffer_size = 0`，
  三个 mem offset 也全为 0，于是 `mwv207_video_buffer_create_2(ctx, 0, STAGING)`（`:1082-1085`）
  分配**零字节缓冲**；10bit 时 `(2+63)&~63 = 64`。只有 `num_tile_cols > 1`（码流带 tile）才进入该分支，
  所以实测的 720p HEVC 很可能是无 tile 流而未暴露。**真实片源是否会因此出错未确认。**
- 疑点 2：`dec_hevc_set_picparam_reg()` 用 `pic_size_reg_unified` 在 `NUM_TILE_COLS/ROWS` 与
  `..._8K` 之间二选一（`:608-622`），而 `0x1F76` 的该位为 1。
- 疑点 3：`dec_hevc_set_skip_length_reg()` 里 `rb.strm_curr_pos = slice_address + 2;`（`:689`）而
  `rb.strm_data_size = decoder->bs_size - 3;`（`:692`，无符号）——`bs_size < 3` 时下溢；
  且 `strm_buff_size` 取 `streams_len[0]`（`:682`）与 `strm_data_size` 取 `bs_size` 两者来源不同，
  对多段 buffer 的帧可能不一致。**未确认是否会在真实码流上触发。**

---

## 8. 参考帧 / DPB / surface 管理

### 8.1 三层"队列"要分清

| 队列 | 归属 | 位置 | 说明 |
|---|---|---|---|
| **bitstream 环形缓冲**（4 个） | 驱动 | `mwv207_video_decoder.h:45`、`:634-643` | `bs_buffers[4]`（`PIPE_USAGE_STAGING`，`width*height*3` 字节）+ 4 份 `streams[16]` 影子拷贝；`cur_buffer` 每帧 ++（`end_frame:580-581`） |
| **DPB 槽**（16 个） | 驱动内部 | `dec->ref_frames[16]` / `ref_frames_h265[16]`（`mwv207_video_dec.h:119-150`） | H.264 由 `short_ref[32]`/`long_ref[32]` 维护；HEVC 无维护 |
| **输出 surface** | VA 前端 / vl | `pipe_video_buffer *target` | 由应用创建；解码时作为 `cur_pic`（`:2334`）与 PPU 输出面 |

**关键点：驱动自己不分配 DPB surface。** 参考帧指针全部来自 VA 前端传入的
`pipe_video_desc.ref[i]`：

- H.264：`h264_desc->ref_frames[i].picture = (struct mwv207_video_buffer *)pic->ref[i]`
  （`mwv207_video_decoder.c:164-165`，循环 `:163-166`）
- HEVC：`:366-378` 同样把 `pic->ref[i]` 与 `PicOrderCntVal/IsLongTerm` 填进 `h265_desc->ref_frames[i]`

### 8.2 surface 结构（`struct mwv207_offset`，`mwv207_video_buffer.h:43-65`）

| 字段 | H.264（`dec_avc_calc_sur_offset` `:1318-1388`） | HEVC（`dec_hevc_calc_sur_offset` `:279-375`） |
|---|---|---|
| 亮度/色度缓冲 | `out_w = NEXT_MULTIPLE(4*width_mbs*16*bit_depth, ALIGN(w_align)*8)/8`；`out_h = height_mbs*4` | `out_w = NEXT_MULTIPLE(4*pic_width*bit_depth, ALIGN(w_align)*8)/8`；`out_h = height/4` |
| `pic_buff_size` / `luma_size` | `out_w*out_h*(mono?2:3)/2 + mb_count*(high10p?80:64) + 32` | `NEXT_MULTIPLE(luma_size*3/2,16) + dmv_mem_size`，再 `+=32` |
| 运动矢量 | `dir_mv_offset` | `dir_mv_offset`（`dmv_mem_size = num_ctbs*(1<<(2*(min_cb+3+diff-4)))*16`） |
| 同步/写回 | `sync_mc_offset`（high10p/普通两种算法） | — |
| CBS/RFC 表 | `cbs_tbl_offsety/offsetc`、`fake_tbly/fake_tblc_size` | 同 |
| 结果 | `buf->tiled_pic_size = pic_buff_size`（`:1374-1387`） | `buf->tiled_pic_size`（`:371-374`） |

输出 surface 的 `res`（**解码器内部重建面**）是**按需分配**的：

```c
/* dec_h264.c:1390-1403（HEVC 在 :371-374 内联） */
if (!buf->res)
    mwv207_video_buffer_alloc_buffer(decoder->base.context, buf,
                                     buf->tiled_pic_size, PIPE_USAGE_STAGING);
```

也就是说，一个 VA 输出 buffer 实际带两套资源：`res`（tiled 重建 + MV，解码核写入）
与 `resources[0..1]`（NV12 平面，PPU 写入）；`mwv207_video_buffer_create_2()` 走的则是
"一次分配固定字节的裸缓冲"（`mwv207_video_buffer.c:565-588`），专供 bitstream / CABAC / tile 等内部缓冲。

### 8.3 H.264 DPB 管理（**驱动内最完整的一块**）

| 机制 | 实现 | 行号 |
|---|---|---|
| 短/长参考表 | `short_ref[32]`、`long_ref[32]`、`long_ref_count`、`short_ref_count` | `mwv207_video_dec.h:119-150` |
| 滑动窗口 | `generate_sliding_window_mmcos()`：`short+long >= num_ref_frames` 时给最老短参考发 `MMCO_SHORT2UNUSED` | `dec_h264.c:770-799` |
| MMCO 执行 | `mwv207_h264_execute_ref_pic_marking()`：`MMCO_SHORT2UNUSED/SHORT2LONG/LONG2UNUSED/LONG/SET_MAX_LONG/RESET` 全实现 + 溢出时按 `MAX(num_ref_frames,1)` 丢最老 | `:921-1104` |
| 工具函数 | `pic_num_extract`（`:811-823`）、`remove_short_at_index`（`:831-841`）、`find_short`（`:851-867`）、`remove_short`（`:872-889`）、`remove_long`（`:896-914`） | 同上 |
| IDR | `idr()` → `remove_all_refs()` + POC 复位 | `:1163-1170`、`:1134-1158` |
| 回填 VA 描述 | `fill_my_ref_frames()` 把 short/long 合成 `pic_param->my_ref_frames[16]` | `:1106-1132` |
| 与硬件的桥 | `dec_h264_set_ref_buffer_info()` 把 16 槽地址/帧号/有效位写进 `REFERn_*`、`REFER_VALID_E`、`REFER_LTERM_E` | `:1964-2084` |

⚠️ `my_ref_frames` 是 jm9100-oss 移植时的**自造**通路，源码里的注释说明原因
（`common/mwv207_video_state.h:344-345`）：

> `/*add by zhihong.deng, 2024-07-16, since openmax will not pass ref_frames to us, and our hardware need this, so create it myself*/`

配合 `dec_h264.c:2342-2347`（首帧 `ref_frames[0]` 为空时用 `my_ref_frames` 顶上）与
`:2373-2376`（`is_reference` 时才回填）使用。

### 8.4 HEVC 的参考帧管理

HEVC 路径**完全信任 VA 前端**：

- `get_h265_msg()`（`mwv207_video_decoder.c:189-379`）把 RPS 计算结果（
  `CurrRpsIdx`、`NumPocStCurrBefore/After`、`RefPicSetStCurrBefore/After[8]`、
  `RefPicSetLtCurr[8]`、`RefPicList[2][15]`、`UseRefPicList`、`NumPocTotalCurr` 等）整包拷入；
- `dec_hevc_set_rpl_reg()`（`dec_hevc.c:895-1048`）只做"地址回填 + 按 POC 差排序"，
  用 `qsort` + `CompareDeltaPoc`（`:888-893`）重建 list0/list1；
- **没有** short/long ref 表、没有 MMCO/RPS 演进、没有跨帧状态。

> 源码里还有一段被注释掉的 `memcpy(&pps, pic->pps, sizeof(pipe_h265_pps))`
> （`mwv207_video_decoder.c:195-204`），注释说明结构体大小/布局不一致，所以改成逐字段拷贝。

---

## 9. PPU 后处理（`mwv207/mwv207_dec_ppu.c`，2088 行）

### 9.1 PPU 在管线中的位置与"永远开启"

- 两个入口都把 `dec->dec_param.pp_enabled = 1` 写死（`dec_h264.c:2316`、`dec_hevc.c:1235`），
  `dec_h264_set_common_regs()` 立刻把它写进 `JMDEC_PP_OUT_E_U`（word **320 / 0x500** bit0，`dec_h264.c:479`）；
- PPU 的**输入**是解码核写入 `target->res`（tiled 重建）的结果，**输出**写进
  `target->resources[0..1]`（NV12 平面）：`mwv207_dec_pp_params_init()` 里
  `ppu_ext_cfg->outsurface = target;`（`mwv207_video_dec.c:237`），
  `mwv207_pp_set_regs()` 再据此写 `PP_OUT_LU_BASE_U_*` / `PP_OUT_CH_BASE_U_*`；
- 所以**"解码出来的帧"必须先经 PPU 才能变成上层可见的 NV12**。这也是 10bit 修复方案
  （报告 §7）能成立的原因：解码核输出 10bit 到 staging，PPU 在输出级把位深降到 8bit。

### 9.2 `ppu_regs[]`：5 个 PPU 实例的寄存器名间接表

`mwv207/mwv207_dec_ppu.c` 顶部定义 `struct mwv207_pp_unit_regs ppu_regs[DEC_MAX_OUT_COUNT]`（5 项），
每项 58 个字段（`mwv207/mwv207_dec_ppu.h:105-163`）：

| 索引 | 覆盖寄存器基址 | 命名风格 |
|---|---|---|
| 0 | `PP_OUT_E_U` = 320 / **0x500**（= `PP_START_UNIFIED_REGS`） | 统一名（无实例后缀） |
| 1 | 342 / **0x558** | `JMDEC_PP1_*` |
| 2 | 359 / **0x59C** | `JMDEC_PP2_*` |
| 3 | 376 / **0x5E0** | `JMDEC_PP3_*` |
| 4 | 451 / **0x70C** | `JMDEC_PP4_*` |

`mwv207/mwv207_dec_ppu.c` 内只有 **9 处** `set_reg(reg_buf, JMDEC_…)` 字面调用，全部在 RGB 分支
（`JMDEC_COLOR_COEFFA1/A2/B/C/D/E_U` = word 412/413/414，`JMDEC_YCBCR_RANGE_U` = 323/0x50C，
`JMDEC_RGB_RANGE_MAX_U` = 320/0x500 的 bit18..29，`JMDEC_RGB_RANGE_MIN_U` = 321/0x504，`:1285-1301`）；
**其余 250 余个 PPU 字段全部经 `ppu_regs[i].字段名` 间接**——与 H.264/HEVC 直接用逻辑名形成两种风格。
（`ppu_regs[0]` 里 `WSCALE_INVRA_EXT_U` / `HCALE_INVRA_EXT_U` 指向 `JMDEC_PP0_*` 是唯一带实例号的例外。）
注意 `DEC_MAX_PPU_COUNT` 在三个头文件里被重复定义（`mwv207_video_dec_cfg.h:249-281`、
`mwv207_video_dec_vpufeature.h:55`、`mwv207_dec_ppu.h:169`，均为 5）。

### 9.3 `mwv207_pp_set_regs()`（`:1248-1553`）写了什么

| 组 | 寄存器 | 行 |
|---|---|---|
| 使能/格式 | `PP_OUT_E_U`（未使能即 continue）、`PP_CR_FIRST`、`PP_OUT_TILE_E_U`、`OUT_FORMAT_U`、`PP_OUT_RGB_FMT_U`、`PP_RGB_PLANAR_U`、`PP_OUT_ALPHA_U`、`DITHER_SELECT_R/G/B_U`、`PP_OUT_P010_FMT_U` | `:1261-1306` |
| RGB 色彩转换 | `JMDEC_COLOR_COEFFA2/A1/B/C/D/E_U`、`JMDEC_YCBCR_RANGE_U`、`JMDEC_RGB_RANGE_MAX_U`、`JMDEC_RGB_RANGE_MIN_U` | `:1285-1302` |
| 几何/裁剪 | `CROP_STARTX/Y_U`、`PP_IN_WIDTH/HEIGHT_U`、`PP_CROP2_STARTX/STARTY/OUT_WIDTH/OUT_HEIGHT_U`、`SCALE1_OUT_WIDTH/HEIGHT_U`（仅 `uscale_support_big_3[i]`）、`PP_OUT_WIDTH/HEIGHT_U` | `:1326-1364` |
| 缩放（水平） | `HOR_SCALE_MODE_U`（1=放大/2=缩小/0=等比）、`SCALE_WRATIO_U`、`WSCALE_INVRA_U(_EXT)`、`PP_DUP_HOR_U` | `:1371-1429` |
| 缩放（垂直） | `VER_SCALE_MODE_U`、`SCALE_HRATIO_U`、`HSCALE_INVRA_U`、`HCALE_INVRA_EXT_U`、`PP_DUP_VER_U` | `:1437-1496` |
| Lanczos 表 | `PP_OUT_LANCZOS_TBL_BASE_U_MSB/LSB`、`Y_FILTER_SIZE_U`、`X_FILTER_SIZE_U`（仅在 `pp_lanczos` 分支；0x1F76 下不可达） | `:1501-1516` |
| 输出面 | `PP_OUT_LU_BASE_U_MSB/LSB`、`PP_OUT_CH_BASE_U_MSB/LSB`（`set_addr_reg*` 指向 `outsurface->resources[0]/[1]`，`MWV207_BO_ACC_FLAG_WRITE`）、`PP_OUT_Y_STRIDE`、`PP_OUT_C_STRIDE`（后者取 `mwv207_resource(buf_res_uv)->level_infos[0].stride`） | `:1525-1550` |

RGB 转换系数是内嵌常量表 `coeff[6][5]`（`:348-353`，例如 `{16384,22970,11700,5638,29032}`、`{19077,26149,13320,6419,33050}`），
由 `ppu_cfg->rgb_stan` 选择行。

### 9.4 PPU 的能力校验与格式映射

| 函数 | 行号 | 作用 |
|---|---|---|
| `mwv207_check_pp_unit_config()` | `:1006-1247` | 按硬件特性校验输入尺寸/缩放比例（`PP_SCALE_IN_MAX_WIDTH 4096`、`PP_SCALE_IN_MAX_HEIGHT 2304`、`PP_CROP_MIN_WIDTH/HEIGHT 48`、`PP_SCALE_OUT_MAX` 等，`mwv207_dec_ppu.h:36-43`）；`i >= max_ppu_count` 时关掉该实例（`:398-400`）；调用 `InitPpUnitLanczosData()`（`:385-…`） |
| `InitPpUnitLanczosData()` | `:385-…` | 仅当 `pp_up_lanczos[i] \|\| pp_down_lanczos[i]` 才置 `pp_lanczos=1`（`:403-410`）——**0x1F76 两者皆为 0，故 Lanczos 永不启用，`lanczos_table` 永不分配** |
| `CalOutTableIndex()` / `getSplineCoeff()` | `:356-384` | 表索引折叠与样条系数 |
| `mwv207_cal_second_up_scale_ratio()` | `:947-1005` | 二次放大比例 |
| `mwv207_pp_unit_set_int_config()` | `:1652-1850` | `mwv207_pp_unit_config`（外部）→ `mwv207_pp_unit_int_config`（内部） |
| `mwv207_trans_unit_config_2_format()` | `:1851-1955` | 内部配置 → `enum mwv207_dec_picture_format` |
| `mwv207_check_output_format()` | `:1956-2088` | 依 `tiled_e`/`monochrome`/`cr_first`/`rgb`/`planar`/`out_p010\|out_I010\|out_L010`/`out_1010` 映射到 `DEC_OUT_FRM_*`（47 个，`mwv207_dec_api_common.h:262-319`） |
| `mwv207_calc_pp_unit_buffer_size()` / `mwv207_calc_each_pp_buffer_size()` | `:1613-1651` / `:1568-1612` | 计算 PPU 工作缓冲大小（写入 `mwv207_offset.pp_buff_size`） |
| `CalcDec400TableSize()` | `:1557-1567` | DEC400 压缩表尺寸（`DEC400_YUV_TABLE_SIZE 4*2304` 等，`mwv207_dec_ppu.h:315-320`） |

### 9.5 10bit 输出的两种去向（报告 §7 修复点）

`mwv207_dec_pp_params_init()`（`mwv207/mwv207_video_dec.c:195-241`）：

| 条件 | 结果 | 行 |
|---|---|---|
| `target` 是 P010/P016 且位深 > 8 | `out_p010 = 1`（硬件 `fmt_p010_support=1`），直出 P010 | `:229-232` |
| `target` 是 8bit NV12 且位深 > 8 | `out_cut_8bits = 1`：PPU 输出 `PP_OUT_FMT_YUV420_8BIT`/`pixel_width=8`，把 10bit 降到 8bit | `:233-235` |
| 其它 | 默认 8bit | — |

同时固定 `enabled=1`、`planar=0`、`tiled_e=0`、`cr_first=0`、`shaper_enabled=1`、`align=DEC_ALIGN_64B`（`:203-209`）；
`has_shaper` 被写死 0（`:201`，`:211-215` 分支不可达）。两个调用点：
`dec_h264.c:1416-1417` 与 `dec_hevc.c:387-388`（都传入 `target`）。

> 【2026-11-03 更新】P010/P016 解码面已启用：`mwv207_is_video_format_supported()`
> 按 `R16_UNORM`/`R16G16_UNORM` 的 `SAMPLER_VIEW` 支持取 AND。实测 HEVC Main10
> 硬解后用 `vaExportSurfaceHandle`（DRM_PRIME_2，fourcc=P010）导出，640×360/30 帧
> 与软件解码逐位一致。**但 GL 侧 16 位采样仍恒 0（`UNRESOLVED.md` C1；2026-11-03 已定位为
> 0x1F76 不解码 `TX_EXT_CONFIG.FORMAT` 码 0x32–0x36，16 位整数/浮点正常）**，所以
> 10bit 帧在 GL 里采样/缩放仍然不正确；当目标是 8bit NV12 面（如 GL 渲染管线）时
> `out_cut_8bits` 仍是默认路径。`out_cut_8bits` 直接丢 2 个 LSB。

---

## 10. 表文件（`*table.h`）到底是什么表

> **结论先行：视频编解码目录里没有任何 VLC 表、也没有 QP 表。** 六个 `*table.h` 只有两类：
> ①**逻辑寄存器 → 硬件寄存器字/位域 的映射表**；②**逐芯片能力记录表**。
> 真正像"码表"的数据（H.264 CABAC 初值、VP9 默认概率、RGB 系数）都内嵌在 `.c/.h` 里，不在 `*table.h`。

| 文件 | 行数 | 内容 | 规模 | 索引方式 | 是否参与编译 |
|---|---|---|---|---|---|
| `mwv207_video_dec_8170enum.h` | 830 | `JMDEC_*` 逻辑寄存器**名**的枚举体 | **1840** 项 | 枚举序号 = 逻辑寄存器 id | ✅ 被 `mwv207_video_dec_reg.h:75-107` 文本包含进 `enum mwv207_9200_hw_if_name`，末尾追加 `JMDEC_DEC_IRQ_STAT`、`JMDEC_PP_IRQ_STAT`、`JMDEC_LAST_REG` → **`JMDEC_LAST_REG = 1842`** |
| `mwv207_video_dec_8170table.h` | 1871 | 每行 `{word, width, shift, flag}` | **1840** 行 | 行号 = 逻辑 id（与上一个文件严格对齐） | ✅ `mwv207_video_dec_reg.c:44-46` |
| `mwv207_video_dec_8170g1table.h` | 1344 | 同格式，**G1 代**（`0x6731`）寄存器图 | 1344 行 | 行号 | ❌ **死代码**：全树无 `#include` |
| `mwv207_video_dec_8170g2table.h` | 1344 | 同格式，**G2 代**（`0x6732`） | 1344 行 | 行号 | ❌ **死代码** |
| `mwv207_dec_register_array_7020.h` | 889 | 1840 个寄存器名字符串字面量 | **1840** 项 | 行号 = 逻辑 id | ✅ `mwv207_video_dec_reg.c:32-34`（仅 trace 日志用） |
| `mwv207_video_dec_vpu_features_list.h` | 2148 | `mwv207_video_dec_hw_features` 的初值 | **17 条** | 线性扫描 `(hw_build_id & id_mask) == id` | ✅ `mwv207_video_dec_vpufeature.c:32-34` |

### 10.1 映射表的语义（唯一权威）

- 每行 `{word, width, shift, flag}`：**word** = `regbuf[]` 下标（= 命令寄存器号 = MMIO 字节偏移/4）、
  **width** = 位域宽度、**shift** = 左移。
- `flag` 字段在 `mwv207_9200_set_dec_register()` 里**未被使用**（`reg.c:53-82` 只取前三项）。
- 全表里 **225 行 `word == 0`**，表示"该逻辑寄存器在本代芯片上不存在"，写入被 `reg.c:67-70` 丢弃。
- `word 0` 本身从不被发送（§5.2），两者自洽。
- 完整映射的还原方式（本报告采用的脚本化方法）：把上述三个文件按行号对齐，得到
  `id ↔ 名字 ↔ {word,width,shift}`；再对 `mwv207_video_dec_reg.c:48-50` 追加的 3 行特判。
  本仓库 `docs/registers/register-db.json`（1392 个厂商寄存器）**不包含** JMDEC 视频块，
  所以 **word 号是这套映射唯一可用的"寄存器号"**，拿不到厂商名（详见 §13-1）。

### 10.2 内嵌在 .c/.h 里的数据表

| 表 | 位置 | 规模 | 用途 |
|---|---|---|---|
| `cabac_init_values[]` | `mwv207/mwv207_dec_h264.c:296-450` | `uint32_t[4*460*2/4]` = **920 字 / 3680 B** | H.264 CABAC 上下文初值（对应 `ASIC_CABAC_INIT_BUFFER_SIZE 3680`，`:40`），每次解码 `memcpy` 到 CABAC 缓冲（`:2158`） |
| `coeff[6][5]` | `mwv207/mwv207_dec_ppu.c:348-353` | 30 个系数 | PPU 的 RGB↔YCbCr 转换矩阵 |
| `reg_mask[33]` | `mwv207/mwv207_video_dec_reg.c:36-42` | 33 项 | N 位掩码（0,1,3,7,…,0xFFFFFFFF） |
| `default_coef_probs_4x4/8x8/16x16/32x32[]` | `common/mwv207_decoder_vp9_slice_header.h:33/196/359/522` | 4 张大表（占 `.h` 前 686 行） | VP9 系数概率默认值（**死代码**，§11.4） |
| `ppu_regs[5]` | `mwv207/mwv207_dec_ppu.c`（`DEC_MAX_OUT_COUNT` 项 × 58 字段） | 290 个名字映射 | PPU 寄存器间接（§9.2） |

---

## 11. 公共解析块（`common/`）

### 11.1 模块总览

| 模块 | 文件（行数） | 导出接口 | 说明 |
|---|---|---|---|
| 位读取器 | `mwv207_decoder_sw_stream.h/.c`（54/338） | `mwv207_sw_get_bits()`、`show_bits()`、`flush_bits()`、`get_bits_unsigned_max()`、`is_byte_aligned()` | `struct mwv207_strm_data`；`show_bits` 内部处理 `00 00 03` 防竞争字节并累加 `emul_byte_count`（`:124-134`）；超界返回 `END_OF_STREAM`（`:212-220`） |
| 位工/工具 | `mwv207_decoder_sw_util.h/.c`（121/102） | `mwv207_sw_count_leading_zeros()`、`mwv207_sw_num_bits()`、`mwv207_sw_turn_around()` | `MWV207_OK/NOK`、`NEXT_MULTIPLE`、`MIN/MAX/CLIP3`、`MEMORY_ALLOCATION_ERROR` 等宏 |
| Exp-Golomb | `mwv207_decoder_hevc_exp_golomb.h/.c`（58/181） | `mwv207_hevc_decode_exp_golomb_unsigned()`、`..._signed()` | 名字是 hevc，但 **H.264 解析器也用它** |
| H.264 NAL | `mwv207_decoder_h264_nal_unit.h/.c`（96/149） | `mwv207_h264_bsd_decode_nal_unit()` | 解析 1 字节 NAL 头（`nal_unit_type`/`nal_ref_idc`）+ SVC 扩展（`svc_extension_flag`、`non_idr_flag`、`view_id`、`anchor_pic_flag`…） |
| H.264 slice | `mwv207_decoder_h264_slice_header.h/.c`（197/887） | `mwv207_h264_bsd_decode_slice_header()`、`mwv207_h264_bsd_Ref_idx_override()`（**无调用者**） | §11.2 |
| HEVC slice | `mwv207_decoder_hevc_slice_header.h/.c`（107/457） | `mwv207_hevc_decode_slice_header()` | §11.3 |
| VP9 slice + bool | `mwv207_decoder_vp9_slice_header.h/.c`（768/1532）、`mwv207_vp9hwd_bool.h/.c`（75/364）、`mwv207_decoder_commonvp9.h`（460） | `vp9_decode_uncompressed_header()`、`vp9_decode_compressed_header()`、`mwv207_vp9_bool_start()` 等 | **全部无调用者**（§11.4） |
| 参数结构 | `mwv207_video_state.h`（765） | — | 驱动自编的 VA 风格 SPS/PPS/picture_desc（§3.2） |
| 日志/计时 | `mwv207_log.h/.c`（62/58）、`mwv207_codec_time.h/.c`（63/68） | `mwv207_log_init()`、`MWV207_LOG()`、codec_time 三项 | |

### 11.2 H.264 slice header 解析（`…h264_slice_header.c:91-367`）

| 语法元素 | 行号 | 备注 |
|---|---|---|
| `first_mb_in_slice` | `106-109` | Exp-Golomb |
| `slice_type` | `111-114` | `IS_P_/B/I/SP/SI_SLICE` 接受 +5 的取值（`.h:48-58`） |
| `pic_parameter_set_id` | `116-119` | |
| `frame_num` | `121-133` | 位宽由 `log2_max_frame_num_minus4` 现算 |
| `field_pic_flag` / `bottom_field_flag` | `137-155` | 仅 `!frame_mbs_only_flag`；产出 `picture_structure` |
| `curr_pic_num` / `max_pic_num` | `157-165` | 场图折半 |
| `idr_pic_id` | `167-172` | 仅 IDR |
| `pic_order_cnt_lsb` / `delta_pic_order_cnt_bottom` / `delta_pic_order_cnt[2]` | `176-213` | 按 `pic_order_cnt_type` 分支 |
| `poc_length` / `poc_length_hw` | `215-217` | **`poc_length_hw = poc_length - 8*emul_byte_count`** —— 供 `JMDEC_POC_LENGTH` |
| `redundant_pic_cnt` | `218-223` | |
| `direct_spatial_mv_pred_flag` | `225-231` | B |
| `num_ref_idx_active_override_flag` + L0/L1 active | `232-263` | 回落到 PPS 默认值 |
| `ref_pic_list_reordering` L0/L1 | `265-288` | 实现 `:394-463`：idc 0/1→`abs_diff_pic_num`、2→`long_term_pic_num`、4/5→`abs_diff_view_idx`、3 结束；越界返回 `MWV207_NOK` |
| `pred_weight_table` | `290-300` | 语法跳过（`:700-753`） |
| `dec_ref_pic_marking` | `302-309` | 实现 `:489-608` + `:610-684`，内产 MMCO 列表；`strm_len = strm_buff_read_bits - strm_pos - 8*emul_byte_count`（`:604-605`）→ `JMDEC_REFPIC_MK_LEN` |
| `cabac_init_idc` | `310-320` | |
| `slice_qp_delta` | `322-328` | |
| 去块滤波字段 | `330-350` | `disable_deblocking_filter_idc`、alpha/beta offset |
| ~~`slice_group_change_cycle`~~ | `351-365` | **`#if 0`** |

**产出**：`mwv207_h264_slice_header`（`.h:136-175`，40+ 字段，含 `mmco[66]`、`nb_mmco`、`mmco_reset`、
`explicit_ref_marking`、`picture_structure`）。使用方：`dec_h264_set_sh_regs()`（§6.4）与 MMCO 引擎（§8.3）。
另有 `printSliceType()`（`:659` 起）整段在 `#if 0` 里。

### 11.3 HEVC slice header 解析（`…hevc_slice_header.c:159-456`）

| 语法元素 | 行号 | 备注 |
|---|---|---|
| `first_slice_segment_in_pic_flag` | `:159+` | 非首 slice 才继续 |
| RAP 附加位 | — | `pic_param->RAPPicFlag` 时 |
| `slice_segment_address` | — | 位宽 = `ctb_width*ctb_height` 的位数 |
| `num_extra_slice_header_bits` 跳过 | — | |
| `slice_type`、`output_flag_present_flag` | — | `HEVC_B/P/I_SLICE`（`.h:42-46`） |
| POC lsb | — | 非 IDR |
| `short_term_ref_pic_set_sps_flag` + ST-RPS | — | 内联时存 `st_ref_pic_set + num_short_term_ref_pic_sets`；delta 预测路径 `hevc_decode_short_term_ref_pic_set()` `:33-157` |
| 长参考段（`num_long_term_sps/pics`、`lt_idx_sps`、`poc_lsb_lt`、`used_by_curr_pic_lt`…） | — | 超 32 拒绝 |
| **`hw_skip_bits`** | `:367-369` | `= strm_buff_read_bits - read_bits - emul_byte_count*8` → `JMDEC_HDR_SKIP_LENGTH` |
| temporal MVP / SAO 位 | — | |
| `num_ref_idx_l0/l1_active` + RPL 修改段 | — | `rpl_modification_flag[2]`、`list_entry_l0/l1[16]` |

**产出**：`mwv207_hevc_slice_header`（`.h:60-87`，含 `st_ref_pic_set[65]`、`lt_idx_sps[32]`、
`delta_poc_msb_preset_flag[32]`、`hw_skip_bits`、RPL 列表项）。
注释里保留了"SW 只解首 slice"的 TODO（`:192-199`）与被移除的 I-slice/RAP 校验（`:239-248`）。

### 11.4 VP9：完整但**没有任何调用者**（死代码）

| 事实 | 证据 |
|---|---|
| 解析器与 bool 解码器被编进 `libmwv207_dri.so` | `meson.build:77-78` |
| `vp9_decode_uncompressed_header` / `vp9_decode_compressed_header` / `mwv207_vp_decode_mv_update` / `mwv207_vp9_bool_start` | 全树 grep **零调用者** |
| `mwv207_decoder_vp9_slice_header.h` / `mwv207_vp9hwd_bool.h` / `mwv207_decoder_commonvp9.h` | **零外部 include**（三者只互相引用） |
| `.h` 里没有导出任何函数原型（文件以 struct 结尾） | `mwv207_decoder_vp9_slice_header.h:686-767` |
| 硬件有 VP9 能力 | `mwv207_video_dec_vpu_features_list.h:35-36`：`vp9_support=1`、`vp9_profile2_support=1`、4096×4096 |
| 缺失环节 | **`mwv207_dec_vp9_set_regs()`**（`mwv207_9200_video_dec_picture()` 无 VP9 分支，`mwv207_video_dec.c:180-193`），以及 `dec->video_format` 从不被置为 `MWV207_VIDEO_FORMAT_VP9` |
| 报告口径 | [`../reports/FULLFEATURE-REPORT.md`](../../reports/FULLFEATURE-REPORT.md) §4.1 原文："VP9 set_regs: video_dec.c already has a DEC_MODE_VP9 branch, vp9_slice_header.c/vp9hwd_bool.c are already compiled into the library, only mwv207_dec_vp9_set_regs() is missing (H264 has 94 set_reg call sites as a template); PPU post-processing path is already present."（"94 处"当初按**单行** `set_reg(reg_buf, JMDEC_…` 形式统计；本次把跨行书写也算进来，实测 `dec_h264.c` 为 **103** 处，见 §14.2） |

`vp9_decode_uncompressed_header()`（`:855-…`）解析 frame marker/version/profile、show_existing_frame、
frame_type、color_space、`setup_frame_size(_with_refs)`、render size、error_resilient、
refresh flags、frame_context_idx、loop filter、量化 delta、segmentation、tile 信息，
并算出 `frame_tag_size`；`vp9_decode_compressed_header()`（`:1276-…`）走 bool 解码器做
分割概率、tile size、系数概率更新与 MV 更新。
`mwv207_vp9hwd_bool.c` 实现了完整的 VP9 算术解码器（`decode_bool`、`decode_bool_128`、
`decode_sub_exp`、`read_prob_diff_update`、概率重映射 `InvRemapProb` 等）。

> 也就是说：**VP9 缺的不是解析，而是"解析结果 → 寄存器"这一层**。以 H.264 的 103 处 `set_reg`
> 为模板（`mwv207_dec_h264_set_regs()` 的 15 步，§6.1）即可补齐。

---

## 12. 编码路径现状

**结论：完全没有编码实现，也不是"部分实现"，而是零。**

| 检查项 | 结果 | 依据 |
|---|---|---|
| `pipe_video_codec::encode_bitstream` | **从未赋值**（`mwv207_create_decoder()` 只装 destroy/begin_frame/decode_macroblock/decode_bitstream/end_frame/flush） | `mwv207_video_decoder.c:606-624` |
| profile 支持 | 命中 profile 后 `return entrypoint != PIPE_VIDEO_ENTRYPOINT_ENCODE;` → ENCODE 恒 false | `mwv207_video_codec.c:51` |
| VA 入口暴露 | 因上式恒 false，`vaQueryConfigEntrypoints` 不会追加 `VAEntrypointEncSlice` | `frontends/va/config.c:84-110` |
| 编码 CAP | `PIPE_VIDEO_CAP_ENC_*` 全部落 `default: return 0` | `mwv207_video_codec.c:82-83` |
| 编码结构体 | `mwv207_h264_enc_*`（`common/mwv207_video_state.h:352-416`）、`mwv207_h265_enc_*`（`:418-492`）仅有**声明**，无使用者 | 全树 grep |
| 源码里的 encode 字样 | 只有 `mwv207_video_codec.c:51`、`common/mwv207_video_enums.h:94`（枚举值）、`mwv207_dec_h264.c:1053`（注释） | grep |
| KMD | DEC 与 ENC 是两条独立通路，KMD 侧**有** `mwv207_pipe_enc.c` 与 `JPIPE_ENC_REG_NUM 500` | `mwv207_pipe_codec_common.h:19-22` |
| 报告口径 | "Hardware encoding — no ENC userspace in the public source at all" | `../reports/FULLFEATURE-REPORT.md` 能力缺口表 |

若要实现编码，需要：①一个 `encode_bitstream` 回调 + 一整套 `ENC` 能力声明；
②一套与 `mwv207_dec_*_set_regs` 对应的编码寄存器序列（**当前无任何参考实现**）；
③`mwv207_video_buffer` 侧的输出面/重建面绑定；④把 KMD 的 ENC 引擎（`MWV207_SUBMIT_ENGINE_TYPE_ENC=2`）接进 winsys。

---

## 13. 未确认与疑点清单

| # | 事项 | 现状 |
|---|---|---|
| 1 | **JMDEC 视频块的厂商寄存器名/地址** | `docs/registers/register-db.json`（1392 个寄存器）里只有 `MWV207REG_DEC400_*`（2D/3D 压缩块），**没有视频解码块**；所以 `word` 号无法反查厂商名，也无法验证 `_Address>>2` 规则（该规则在 REGISTERS.md §1.1 只对 3D/2D 块用实测样例证明过）。间接佐证：KMD `reg*4` 与 `iosize = 503*4`、`DEC_E@word1 → KMD 读字节 4 取 hw_run`。**厂商名：未确认** |
| 2 | `DEC_X170_BUS_WIDTH_128 = 2` 的编码语义 | 只知道常量值（`cfg.h:50`），不知道 0/1/2/3 分别代表 64/128/256 bit 还是别的 |
| 3 | ~~AXI ID / EXT_TIMEOUT 的 word 号~~ | **已确认**：`DEC_AXI_RD_ID_E`=58/0x0E8 s14、`DEC_AXI_WD_ID_E`=58 s13、`DEC_AXI_RD_ID`=60/0x0F0 s0、`DEC_AXI_WR_ID`=60/0x0F0 s16、`EXT_TIMEOUT_OVERRIDE_E`=318/0x4F8 s31、`EXT_TIMEOUT_CYCLES`=318 s0、`TIMEOUT_OVERRIDE_E`=319/0x4FC s31、`TIMEOUT_CYCLES`=319 s0 |
| 4 | 为什么所有逐行 H.264 都走 `DEC_MODE_H264_HIGH10P`(=15) | `rlc_mode` 恒 0（全树只在 `dec_h264.c:459` 读取），条件退化为 `frame_mbs_only_flag`。**是否符合硬件语义未确认**（实测解码正确） |
| 5 | tile 缓冲零字节的后果（HEVC） | 见 §7.4 疑点 1，**代码事实已确认，实际影响未确认** |
| 6 | `bs_size - 3` 无符号下溢（HEVC） | 见 §7.4 疑点 3，触发条件未确认 |
| 7 | 场（interlaced）解码 | 对外 `SUPPORTS_INTERLACED=false`（`mwv207_video_codec.c:76-77`），但内部有 MBAFF/场寄存器与 `DEC_DPB_INTERLACED_FIELD` 逻辑；实测未覆盖 |
| 8 | `mwv207_h264_bsd_Ref_idx_override()` | `.h:194` 声明、`.c:755-886` 实现，**全树无调用者**（是否留给未来/外部调用未确认） |
| 9 | `common/mwv207_video_info.h`（277 行） | **无任何 include 者** → 整文件遗留；`mwv207_video_h264/h265/vc1/mpeg2/mpeg4` 未使用 |
| 10 | `dump_referece_frames()`（`dec_h264.c:2278-2310`） | 死调试函数，且硬编码 1920×1080 尺寸 |
| 11 | `mwv207_9200_dump_register()`（`reg.c:143-174`） | 函数体开头 `return;`（`:151`）→ dump 被禁用 |
| 12 | `MWV207DEC_STREAM_SWAP` 展开为 0 | 名字暗示字节序交换，但三个 SWAP 寄存器全写 0。`cfg.h:170-179` 里有一个 `#if 0` 的"原本意图"版本：`SWAP_8\|SWAP_16\|SWAP_32`（= 1\|2\|4 = 7），最终 `#else` 分支写死 0；H.264 的 `DEC_TAB_SWAP` 则取 `SWAP_8\|SWAP_16` = **3**（`video_dec.c:42-49`，`cfg.h:53-55`）。大小端设置实际由 `JMDEC_DEC_OUT_ENDIAN` 承担，而**该寄存器 word=0（空操作）** |
| 13 | PPU 的 rotation/dither 寄存器 | 驱动会写（`dec_ppu.c` 的 `ROTATION_MODE_U`/`DITHER_SELECT_*_U`），但 `0x1F76` 的 `rotation_support=0`、`dither_support=0` |
| 14 | `JMDEC_BITPLANE1_E` 与 `JMDEC_BLACKWHITE_E` | 二者都映射到 word 7 的 **bit30**（`BLACKWHITE_E` 由 H.264 在 `:587` 写入，随后 `:2083` 被 `BITPLANE1_E=0` 覆盖）——**疑似寄存器表串行/重复，影响未确认** |
| 15 | `MWV207_DEBUG_VIDEO` 调试通路 | 打开时会同步等 fence 并 dump 寄存器流（`cs.c:118-129`），但对解码结果的影响未测 |
| 16 | `-DUSE_FAKE_RFC_TABLE`（`dec_hevc.c:50`） | 宏定义存在但未见条件编译使用点，作用未确认 |

---

## 14. 附录

### 14.1 文件清单（56 个文件，`wc -l` 合计 26099 行）

**接入层（6）**：`mwv207_video_codec.c 154`、`mwv207_video_codec.h 43`、`mwv207_video_buffer.c 611`、
`mwv207_video_buffer.h 219`、`mwv207_video_decoder.c 650`、`mwv207_video_decoder.h 93`

**`mwv207/`（21）**：`mwv207_dec_api_common.h 569`、`mwv207_dec_h264.c 2378`、`mwv207_dec_h264.h 39`、
`mwv207_dec_hevc.c 1332`、`mwv207_dec_hevc.h 34`、`mwv207_dec_ppu.c 2088`、`mwv207_dec_ppu.h 323`、
`mwv207_dec_register_array_7020.h 889`、`mwv207_video_dec.c 242`、`mwv207_video_dec.h 165`、
`mwv207_video_dec_8170enum.h 830`、`mwv207_video_dec_8170table.h 1871`、`mwv207_video_dec_8170g1table.h 1344`、
`mwv207_video_dec_8170g2table.h 1344`、`mwv207_video_dec_cfg.h 321`、`mwv207_video_dec_cs.h 65`、
`mwv207_video_dec_reg.c 175`、`mwv207_video_dec_reg.h 135`、`mwv207_video_dec_vpufeature.c 62`、
`mwv207_video_dec_vpufeature.h 191`、`mwv207_video_dec_vpu_features_list.h 2148`

**`common/`（29）**：`mwv207_codec_time.c 68`、`.h 63`、`mwv207_decoder_commonvp9.h 460`、
`mwv207_decoder_h264_nal_unit.c 149`、`.h 96`、`mwv207_decoder_h264_slice_header.c 887`、`.h 197`、
`mwv207_decoder_hevc_exp_golomb.c 181`、`.h 58`、`mwv207_decoder_hevc_slice_header.c 457`、`.h 107`、
`mwv207_decoder_sw_stream.c 338`、`.h 54`、`mwv207_decoder_sw_util.c 102`、`.h 121`、
`mwv207_decoder_vp9_slice_header.c 1532`、`.h 768`、`mwv207_log.c 58`、`.h 62`、`mwv207_utils.h 39`、
`mwv207_video_cs.c 136`、`.h 51`、`mwv207_video_enums.h 102`、`mwv207_video_info.h 277`、
`mwv207_video_reg_buf.c 131`、`.h 86`、`mwv207_video_state.h 765`、`mwv207_vp9hwd_bool.c 364`、`.h 75`

### 14.2 附录 A：字面 `set_reg()` 写到的寄存器（180 个逻辑名，按 word 排序）

`H` = `mwv207_dec_h264.c`（103 处调用）、`E` = `mwv207_dec_hevc.c`（109 处）、
`P` = `mwv207_dec_ppu.c`（9 处，RGB 通路）、`C` = `mwv207_video_dec.c`（31 处，公共配置块）。
`word` = 命令寄存器号（MMIO 字节 = word×4）；标 **空操作** 者在该芯片映射里 word=0，写入被丢弃。

| word | 字节 | 逻辑寄存器名 | w | s | H | E | P | C |
|---|---|---|---|---|---|---|---|---|
| 0 | 0x000 | `JMDEC_BUFF_ID` <sub>空操作</sub> | 32 | 0 | ✔ |  |  |  |
| 0 | 0x000 | `JMDEC_DEC_DATA_DISC_E` <sub>空操作</sub> | 32 | 0 | ✔ |  |  |  |
| 0 | 0x000 | `JMDEC_DEC_LATENCY` <sub>空操作</sub> | 32 | 0 | ✔ |  |  |  |
| 0 | 0x000 | `JMDEC_DEC_OUT_ENDIAN` <sub>空操作</sub> | 32 | 0 | ✔ |  |  |  |
| 0 | 0x000 | `JMDEC_DEC_RSC_BASE_LSB` <sub>空操作</sub> | 32 | 0 |  | ✔ |  |  |
| 0 | 0x000 | `JMDEC_DEC_RSC_BASE_MSB` <sub>空操作</sub> | 32 | 0 |  | ✔ |  |  |
| 0 | 0x000 | `JMDEC_DEC_RSY_BASE_LSB` <sub>空操作</sub> | 32 | 0 |  | ✔ |  |  |
| 0 | 0x000 | `JMDEC_DEC_RSY_BASE_MSB` <sub>空操作</sub> | 32 | 0 |  | ✔ |  |  |
| 1 | 0x004 | `JMDEC_DEC_E` | 1 | 0 | ✔ | ✔ |  |  |
| 1 | 0x004 | `JMDEC_DEC_IRQ_DIS` | 1 | 4 |  |  |  | ✔ |
| 2 | 0x008 | `JMDEC_DEC_CLK_GATE_E` | 1 | 10 | ✔ |  |  | ✔ |
| 2 | 0x008 | `JMDEC_DEC_DIRMV_SWAP` | 4 | 20 |  |  |  | ✔ |
| 2 | 0x008 | `JMDEC_DEC_PIC_SWAP` | 4 | 24 |  |  |  | ✔ |
| 2 | 0x008 | `JMDEC_DEC_STRM_SWAP` | 4 | 28 |  |  |  | ✔ |
| 2 | 0x008 | `JMDEC_DEC_TAB_SWAP` | 4 | 12 |  |  |  | ✔ |
| 2 | 0x008 | `JMDEC_TILED_MODE_LSB` | 1 | 7 | ✔ |  |  |  |
| 2 | 0x008 | `JMDEC_TILED_MODE_MSB` | 1 | 17 | ✔ |  |  |  |
| 3 | 0x00C | `JMDEC_APF_ONE_PID` | 1 | 7 |  | ✔ |  | ✔ |
| 3 | 0x00C | `JMDEC_BLOCK_BUFFER_MODE_E` | 1 | 1 | ✔ | ✔ |  |  |
| 3 | 0x00C | `JMDEC_BUFFER_EMPTY_INT_E` | 1 | 2 | ✔ | ✔ |  |  |
| 3 | 0x00C | `JMDEC_DEC_MODE` | 5 | 27 | ✔ | ✔ |  |  |
| 3 | 0x00C | `JMDEC_DEC_OUT_DIS` | 1 | 15 | ✔ |  |  |  |
| 3 | 0x00C | `JMDEC_DEC_OUT_EC_BYPASS` | 1 | 8 | ✔ | ✔ |  | ✔ |
| 3 | 0x00C | `JMDEC_FILTERING_DIS` | 1 | 14 |  | ✔ |  |  |
| 3 | 0x00C | `JMDEC_PICORD_COUNT_E` | 1 | 9 | ✔ |  |  |  |
| 3 | 0x00C | `JMDEC_PIC_FIELDMODE_E` | 1 | 22 | ✔ |  |  |  |
| 3 | 0x00C | `JMDEC_PIC_INTERLACE_E` | 1 | 23 | ✔ |  |  |  |
| 3 | 0x00C | `JMDEC_PIC_TOPFIELD_E` | 1 | 19 | ✔ |  |  |  |
| 3 | 0x00C | `JMDEC_RLC_MODE_E` | 1 | 24 | ✔ |  |  |  |
| 3 | 0x00C | `JMDEC_SEQ_MBAFF_E` | 1 | 10 | ✔ |  |  |  |
| 3 | 0x00C | `JMDEC_WRITE_MVS_E` | 1 | 12 | ✔ | ✔ |  |  |
| 4 | 0x010 | `JMDEC_PIC_HEIGHT_IN_CBS` | 13 | 6 | ✔ | ✔ |  |  |
| 4 | 0x010 | `JMDEC_PIC_WIDTH_IN_CBS` | 13 | 19 | ✔ | ✔ |  |  |
| 4 | 0x010 | `JMDEC_REF_FRAMES` | 5 | 0 | ✔ | ✔ |  |  |
| 5 | 0x014 | `JMDEC_CH_QP_OFFSET` | 5 | 19 | ✔ | ✔ |  |  |
| 5 | 0x014 | `JMDEC_CH_QP_OFFSET2` | 5 | 14 | ✔ | ✔ |  |  |
| 5 | 0x014 | `JMDEC_CU_QPD_E` | 1 | 4 |  | ✔ |  |  |
| 5 | 0x014 | `JMDEC_FIELDPIC_FLAG_E` | 1 | 0 | ✔ |  |  |  |
| 5 | 0x014 | `JMDEC_MAX_CU_QPD_DEPTH` | 6 | 5 |  | ✔ |  |  |
| 5 | 0x014 | `JMDEC_SCALING_LIST_E` | 1 | 24 |  | ✔ |  |  |
| 5 | 0x014 | `JMDEC_SIGN_DATA_HIDE` | 1 | 12 |  | ✔ |  |  |
| 5 | 0x014 | `JMDEC_STRM_START_BIT` | 7 | 25 |  | ✔ |  |  |
| 5 | 0x014 | `JMDEC_TEMPOR_MVP_E` | 1 | 11 |  | ✔ |  |  |
| 5 | 0x014 | `JMDEC_TYPE1_QUANT_E` | 1 | 24 | ✔ |  |  |  |
| 6 | 0x018 | `JMDEC_STREAM_LEN` | 32 | 0 | ✔ | ✔ |  |  |
| 7 | 0x01C | `JMDEC_ASYM_PRED_E` | 1 | 23 |  | ✔ |  |  |
| 7 | 0x01C | `JMDEC_BITPLANE1_E` | 1 | 30 | ✔ |  |  |  |
| 7 | 0x01C | `JMDEC_BLACKWHITE_E` | 1 | 30 | ✔ | ✔ |  |  |
| 7 | 0x01C | `JMDEC_CABAC_E` | 1 | 31 | ✔ |  |  |  |
| 7 | 0x01C | `JMDEC_CABAC_INIT_PRESENT` | 1 | 31 |  | ✔ |  |  |
| 7 | 0x01C | `JMDEC_CH_8PIX_ILEAV_E` | 1 | 24 | ✔ |  |  |  |
| 7 | 0x01C | `JMDEC_DEPEND_SLICE_E` | 1 | 19 |  | ✔ |  |  |
| 7 | 0x01C | `JMDEC_DIR_8X8_INFER_E` | 1 | 29 | ✔ |  |  |  |
| 7 | 0x01C | `JMDEC_FILT_OFFSET_BETA` | 5 | 12 |  | ✔ |  |  |
| 7 | 0x01C | `JMDEC_FILT_OFFSET_TC` | 5 | 7 |  | ✔ |  |  |
| 7 | 0x01C | `JMDEC_FILT_OVERRIDE_E` | 1 | 18 |  | ✔ |  |  |
| 7 | 0x01C | `JMDEC_FILT_SLICE_BORDER` | 1 | 25 |  | ✔ |  |  |
| 7 | 0x01C | `JMDEC_FILT_TILE_BORDER` | 1 | 24 |  | ✔ |  |  |
| 7 | 0x01C | `JMDEC_FRAMENUM` | 16 | 0 | ✔ |  |  |  |
| 7 | 0x01C | `JMDEC_FRAMENUM_LEN` | 5 | 16 | ✔ |  |  |  |
| 7 | 0x01C | `JMDEC_PCM_FILT_DISABLE` | 1 | 21 |  | ✔ |  |  |
| 7 | 0x01C | `JMDEC_SAO_E` | 1 | 22 |  | ✔ |  |  |
| 7 | 0x01C | `JMDEC_SLICE_CHQP_FLAG` | 1 | 20 |  | ✔ |  |  |
| 7 | 0x01C | `JMDEC_SLICE_HDR_EBITS` | 3 | 3 |  | ✔ |  |  |
| 7 | 0x01C | `JMDEC_SLICE_HDR_EXT_E` | 1 | 6 |  | ✔ |  |  |
| 7 | 0x01C | `JMDEC_STRONG_SMOOTH_E` | 1 | 17 |  | ✔ |  |  |
| 7 | 0x01C | `JMDEC_WEIGHT_BIPR_IDC` | 2 | 26 | ✔ | ✔ |  |  |
| 7 | 0x01C | `JMDEC_WEIGHT_PRED_E` | 1 | 28 | ✔ | ✔ |  |  |
| 8 | 0x020 | `JMDEC_8X8TRANS_FLAG_E` | 1 | 28 | ✔ |  |  |  |
| 8 | 0x020 | `JMDEC_BIT_DEPTH_C_MINUS8` | 2 | 4 | ✔ | ✔ |  |  |
| 8 | 0x020 | `JMDEC_BIT_DEPTH_Y_MINUS8` | 2 | 6 | ✔ | ✔ |  |  |
| 8 | 0x020 | `JMDEC_CONST_INTRA_E` | 1 | 31 | ✔ | ✔ |  |  |
| 8 | 0x020 | `JMDEC_FILT_CTRL_PRES` | 1 | 30 | ✔ | ✔ |  |  |
| 8 | 0x020 | `JMDEC_IDR_PIC_E` | 1 | 16 | ✔ | ✔ |  |  |
| 8 | 0x020 | `JMDEC_IDR_PIC_ID` | 16 | 0 | ✔ |  |  |  |
| 8 | 0x020 | `JMDEC_PCM_BITDEPTH_C` | 4 | 8 |  | ✔ |  |  |
| 8 | 0x020 | `JMDEC_PCM_BITDEPTH_Y` | 4 | 12 |  | ✔ |  |  |
| 8 | 0x020 | `JMDEC_RDPIC_CNT_PRES` | 1 | 29 | ✔ |  |  |  |
| 8 | 0x020 | `JMDEC_REFPIC_MK_LEN` | 11 | 17 | ✔ |  |  |  |
| 9 | 0x024 | `JMDEC_HDR_SKIP_LENGTH` | 14 | 0 |  | ✔ |  |  |
| 9 | 0x024 | `JMDEC_POC_LENGTH` | 8 | 0 | ✔ |  |  |  |
| 9 | 0x024 | `JMDEC_PPS_ID` | 8 | 24 | ✔ |  |  |  |
| 9 | 0x024 | `JMDEC_REFIDX0_ACTIVE` | 5 | 14 | ✔ | ✔ |  |  |
| 9 | 0x024 | `JMDEC_REFIDX1_ACTIVE` | 5 | 19 | ✔ | ✔ |  |  |
| 10 | 0x028 | `JMDEC_ENTR_CODE_SYNCH_E` | 1 | 0 |  | ✔ |  |  |
| 10 | 0x028 | `JMDEC_NUM_TILE_COLS` | 5 | 19 |  | ✔ |  |  |
| 10 | 0x028 | `JMDEC_NUM_TILE_COLS_8K` | 7 | 17 |  | ✔ |  |  |
| 10 | 0x028 | `JMDEC_NUM_TILE_ROWS` | 5 | 14 |  | ✔ |  |  |
| 10 | 0x028 | `JMDEC_NUM_TILE_ROWS_8K` | 5 | 12 |  | ✔ |  |  |
| 10 | 0x028 | `JMDEC_TILE_ENABLE` | 1 | 1 |  | ✔ |  |  |
| 12 | 0x030 | `JMDEC_IDR_PIC_ID_H10` | 16 | 16 | ✔ |  |  |  |
| 12 | 0x030 | `JMDEC_MAX_CB_SIZE` | 3 | 10 | ✔ | ✔ |  |  |
| 12 | 0x030 | `JMDEC_MAX_PCM_SIZE` | 3 | 4 |  | ✔ |  |  |
| 12 | 0x030 | `JMDEC_MIN_CB_SIZE` | 3 | 13 | ✔ | ✔ |  |  |
| 12 | 0x030 | `JMDEC_MIN_PCM_SIZE` | 3 | 7 |  | ✔ |  |  |
| 12 | 0x030 | `JMDEC_PCM_E` | 1 | 3 |  | ✔ |  |  |
| 12 | 0x030 | `JMDEC_REFPICLIST_MOD_E` | 1 | 0 |  | ✔ |  |  |
| 12 | 0x030 | `JMDEC_TRANSFORM_SKIP_E` | 1 | 2 |  | ✔ |  |  |
| 12 | 0x030 | `JMDEC_TRANSQ_BYPASS_E` | 1 | 1 |  | ✔ |  |  |
| 13 | 0x034 | `JMDEC_INIT_QP` | 7 | 24 | ✔ | ✔ |  |  |
| 13 | 0x034 | `JMDEC_MAX_INTER_HIERDEPTH` | 3 | 4 |  | ✔ |  |  |
| 13 | 0x034 | `JMDEC_MAX_INTRA_HIERDEPTH` | 3 | 7 |  | ✔ |  |  |
| 13 | 0x034 | `JMDEC_MAX_TRB_SIZE` | 3 | 10 |  | ✔ |  |  |
| 13 | 0x034 | `JMDEC_MIN_TRB_SIZE` | 3 | 13 |  | ✔ |  |  |
| 13 | 0x034 | `JMDEC_PARALLEL_MERGE` | 4 | 0 |  | ✔ |  |  |
| 13 | 0x034 | `JMDEC_START_CODE_E` | 1 | 31 | ✔ | ✔ |  |  |
| 20 | 0x050 | `JMDEC_PARTIAL_CTB_X` | 1 | 31 | ✔ | ✔ |  |  |
| 20 | 0x050 | `JMDEC_PARTIAL_CTB_Y` | 1 | 30 | ✔ | ✔ |  |  |
| 20 | 0x050 | `JMDEC_PIC_HEIGHT_4X4` | 12 | 0 | ✔ | ✔ |  |  |
| 20 | 0x050 | `JMDEC_PIC_WIDTH_4X4` | 12 | 16 | ✔ | ✔ |  |  |
| 38 | 0x098 | `JMDEC_REFER_LTERM_E` | 32 | 0 | ✔ | ✔ |  |  |
| 39 | 0x09C | `JMDEC_REFER_VALID_E` | 32 | 0 | ✔ |  |  |  |
| 49 | 0x0C4 | `JMDEC_PRED_BC_TAP_0_0` | 10 | 22 | ✔ |  |  |  |
| 49 | 0x0C4 | `JMDEC_PRED_BC_TAP_0_1` | 10 | 12 | ✔ |  |  |  |
| 49 | 0x0C4 | `JMDEC_PRED_BC_TAP_0_2` | 10 | 2 | ✔ |  |  |  |
| 55 | 0x0DC | `JMDEC_APF_DISABLE` | 1 | 31 |  |  |  | ✔ |
| 55 | 0x0DC | `JMDEC_APF_SINGLE_PU_MODE` | 1 | 30 |  |  |  | ✔ |
| 55 | 0x0DC | `JMDEC_APF_THRESHOLD` | 16 | 0 |  |  |  | ✔ |
| 58 | 0x0E8 | `JMDEC_DEC_AXI_RD_ID_E` | 1 | 14 |  |  |  | ✔ |
| 58 | 0x0E8 | `JMDEC_DEC_AXI_WD_ID_E` | 1 | 13 |  |  |  | ✔ |
| 58 | 0x0E8 | `JMDEC_DEC_BUSWIDTH` | 3 | 8 |  |  |  | ✔ |
| 58 | 0x0E8 | `JMDEC_DEC_MAX_BURST` | 8 | 0 | ✔ |  |  | ✔ |
| 58 | 0x0E8 | `JMDEC_DEC_MULTICORE_E` | 1 | 30 | ✔ | ✔ |  |  |
| 58 | 0x0E8 | `JMDEC_DEC_REFER_DOUBLEBUFFER_E` | 1 | 15 |  |  |  | ✔ |
| 58 | 0x0E8 | `JMDEC_DEC_WRITESTAT_E` | 1 | 29 | ✔ | ✔ |  |  |
| 58 | 0x0E8 | `JMDEC_SERV_MERGE_DIS` | 1 | 31 | ✔ |  |  |  |
| 60 | 0x0F0 | `JMDEC_DEC_AXI_RD_ID` | 16 | 0 |  |  |  | ✔ |
| 60 | 0x0F0 | `JMDEC_DEC_AXI_WR_ID` | 16 | 16 |  |  |  | ✔ |
| 64 | 0x100 | `JMDEC_DEC_OUT_BASE_MSB` | 32 | 0 | ✔ |  |  |  |
| 64 | 0x100 | `JMDEC_DEC_OUT_YBASE_MSB` | 32 | 0 |  | ✔ |  |  |
| 65 | 0x104 | `JMDEC_DEC_OUT_BASE_LSB` | 32 | 0 | ✔ |  |  |  |
| 65 | 0x104 | `JMDEC_DEC_OUT_YBASE_LSB` | 32 | 0 |  | ✔ |  |  |
| 65 | 0x104 | `JMDEC_DPB_ILACE_MODE` | 1 | 1 | ✔ |  |  |  |
| 98 | 0x188 | `JMDEC_DEC_OUT_CBASE_MSB` | 32 | 0 | ✔ | ✔ |  |  |
| 99 | 0x18C | `JMDEC_DEC_OUT_CBASE_LSB` | 32 | 0 | ✔ | ✔ |  |  |
| 132 | 0x210 | `JMDEC_DIR_MV_BASE_MSB` | 32 | 0 | ✔ | ✔ |  |  |
| 133 | 0x214 | `JMDEC_DIR_MV_BASE_LSB` | 32 | 0 | ✔ | ✔ |  |  |
| 166 | 0x298 | `JMDEC_TILE_BASE_MSB` | 32 | 0 |  | ✔ |  |  |
| 167 | 0x29C | `JMDEC_TILE_BASE_LSB` | 32 | 0 |  | ✔ |  |  |
| 168 | 0x2A0 | `JMDEC_RLC_VLC_BASE_MSB` | 32 | 0 | ✔ |  |  |  |
| 168 | 0x2A0 | `JMDEC_STREAM_BASE_MSB` | 32 | 0 | ✔ | ✔ |  |  |
| 169 | 0x2A4 | `JMDEC_RLC_VLC_BASE_LSB` | 32 | 0 | ✔ |  |  |  |
| 169 | 0x2A4 | `JMDEC_STREAM_BASE_LSB` | 32 | 0 | ✔ | ✔ |  |  |
| 170 | 0x2A8 | `JMDEC_SCALE_LIST_BASE_MSB` | 32 | 0 |  | ✔ |  |  |
| 171 | 0x2AC | `JMDEC_SCALE_LIST_BASE_LSB` | 32 | 0 |  | ✔ |  |  |
| 174 | 0x2B8 | `JMDEC_QTABLE_BASE_MSB` | 32 | 0 | ✔ |  |  |  |
| 175 | 0x2BC | `JMDEC_QTABLE_BASE_LSB` | 32 | 0 | ✔ |  |  |  |
| 178 | 0x2C8 | `JMDEC_DEC_VERT_FILT_BASE_MSB` | 32 | 0 |  | ✔ |  |  |
| 179 | 0x2CC | `JMDEC_DEC_VERT_FILT_BASE_LSB` | 32 | 0 |  | ✔ |  |  |
| 180 | 0x2D0 | `JMDEC_DEC_VERT_SAO_BASE_MSB` | 32 | 0 |  | ✔ |  |  |
| 181 | 0x2D4 | `JMDEC_DEC_VERT_SAO_BASE_LSB` | 32 | 0 |  | ✔ |  |  |
| 182 | 0x2D8 | `JMDEC_DEC_BSD_CTRL_BASE_MSB` | 32 | 0 |  | ✔ |  |  |
| 183 | 0x2DC | `JMDEC_DEC_BSD_CTRL_BASE_LSB` | 32 | 0 |  | ✔ |  |  |
| 189 | 0x2F4 | `JMDEC_DEC_OUT_TYBASE_MSB` | 32 | 0 | ✔ | ✔ |  |  |
| 190 | 0x2F8 | `JMDEC_DEC_OUT_TYBASE_LSB` | 32 | 0 | ✔ | ✔ |  |  |
| 223 | 0x37C | `JMDEC_DEC_OUT_TCBASE_MSB` | 32 | 0 | ✔ | ✔ |  |  |
| 224 | 0x380 | `JMDEC_DEC_OUT_TCBASE_LSB` | 32 | 0 | ✔ | ✔ |  |  |
| 258 | 0x408 | `JMDEC_STRM_BUFFER_LEN` | 32 | 0 | ✔ | ✔ |  |  |
| 259 | 0x40C | `JMDEC_STRM_START_OFFSET` | 32 | 0 | ✔ | ✔ |  |  |
| 265 | 0x424 | `JMDEC_AXI_RD_OSTD_THRESHOLD` | 10 | 18 |  |  |  | ✔ |
| 265 | 0x424 | `JMDEC_AXI_WR_OSTD_THRESHOLD` | 10 | 8 |  |  |  | ✔ |
| 266 | 0x428 | `JMDEC_IGNORE_SLICE_ERROR_E` | 1 | 31 | ✔ |  |  |  |
| 314 | 0x4E8 | `JMDEC_DEC_OUT_C_STRIDE` | 16 | 0 | ✔ | ✔ |  |  |
| 314 | 0x4E8 | `JMDEC_DEC_OUT_Y_STRIDE` | 16 | 16 | ✔ | ✔ |  |  |
| 318 | 0x4F8 | `JMDEC_EXT_TIMEOUT_CYCLES` | 31 | 0 |  |  |  | ✔ |
| 318 | 0x4F8 | `JMDEC_EXT_TIMEOUT_OVERRIDE_E` | 1 | 31 |  |  |  | ✔ |
| 319 | 0x4FC | `JMDEC_TIMEOUT_CYCLES` | 31 | 0 |  |  |  | ✔ |
| 319 | 0x4FC | `JMDEC_TIMEOUT_OVERRIDE_E` | 1 | 31 |  |  |  | ✔ |
| 320 | 0x500 | `JMDEC_PP_CR_FIRST` | 1 | 1 | ✔ |  |  |  |
| 320 | 0x500 | `JMDEC_PP_OUT_E_U` | 1 | 0 | ✔ |  |  |  |
| 320 | 0x500 | `JMDEC_RGB_RANGE_MAX_U` | 12 | 18 |  |  | ✔ |  |
| 321 | 0x504 | `JMDEC_RGB_RANGE_MIN_U` | 9 | 0 |  |  | ✔ |  |
| 322 | 0x508 | `JMDEC_PP_IN_FORMAT_U` | 5 | 27 | ✔ | ✔ |  |  |
| 323 | 0x50C | `JMDEC_YCBCR_RANGE_U` | 1 | 29 |  |  | ✔ |  |
| 412 | 0x670 | `JMDEC_COLOR_COEFFA1_U` | 16 | 0 |  |  | ✔ |  |
| 412 | 0x670 | `JMDEC_COLOR_COEFFA2_U` | 16 | 16 |  |  | ✔ |  |
| 413 | 0x674 | `JMDEC_COLOR_COEFFB_U` | 16 | 0 |  |  | ✔ |  |
| 413 | 0x674 | `JMDEC_COLOR_COEFFC_U` | 16 | 16 |  |  | ✔ |  |
| 414 | 0x678 | `JMDEC_COLOR_COEFFD_U` | 16 | 0 |  |  | ✔ |  |
| 414 | 0x678 | `JMDEC_COLOR_COEFFE_U` | 16 | 16 |  |  | ✔ |  |


> 说明：本表是 `set_reg(reg_buf, JMDEC_…)` **字面出现**的全部名字（含多行书写形式）。
> 参考槽族（`REFER0..15_*`、`CUR_POC_00..15`、`INIT_RLIST_*`、`PINIT_RLIST_*`）经**索引数组**间接写（§6.5），
> PPU 的 250 余个字段经 **`ppu_regs[i].字段名`** 间接写（§9.2），故都不在本表内。
> 另有 225 个逻辑寄存器在映射表里 word=0（本代芯片不存在），其中只有 4 个（`BUFF_ID`、
> `DEC_DATA_DISC_E`、`DEC_LATENCY`、`DEC_OUT_ENDIAN`）以及 HEVC 的 4 个 `DEC_RS*_BASE_*`
> 被驱动写到——均为空操作。

### 14.3 关键函数签名速查

```c
/* 接入层 */
void  mwv207_context_video_codec_init(struct pipe_context *context);            /* video_codec.c:136 */
struct pipe_video_codec *mwv207_create_codec(struct pipe_context *context,
                                             const struct pipe_video_codec *codec);/* :148 */
bool  mwv207_profile_supported(struct pipe_screen *, enum pipe_video_profile,
                               enum pipe_video_entrypoint);                       /* :34 */
void *mwv207_get_video_param(struct pipe_screen *, enum pipe_video_profile,
                             enum pipe_video_entrypoint, enum pipe_video_cap);    /* :57 */
bool  mwv207_is_video_format_supported(struct pipe_screen *, enum pipe_format,
                                       enum pipe_video_profile,
                                       enum pipe_video_entrypoint);               /* :87 */
struct pipe_video_buffer *mwv207_pipe_video_buffer_create(struct pipe_context *,
                                        const struct pipe_video_buffer *tmpl);    /* video_buffer.c:545 */
void  mwv207_video_buffer_resources(struct pipe_video_buffer *,
                                    struct pipe_resource **r);                    /* :459（jm9100 修复点） */
struct pipe_video_codec *mwv207_create_decoder(struct pipe_context *,
                                        const struct pipe_video_codec *codec);    /* video_decoder.c:592 */

/* 私有解码器 */
struct mwv207_video_dec *mwv207_video_dec_create(struct mwv207_winsys *ws);        /* video_dec.c:131 */
void  mwv207_9200_video_dec_picture(struct mwv207_video_decoder *decoder,
                                    struct pipe_video_buffer *target);             /* :180 分派 */
void  mwv207_dec_set_common_config_regs(struct mwv207_video_dec *dec);             /* :33 */
int   mwv207_dec_pp_params_init(struct mwv207_video_dec *dec,
                                mwv207_pp_unit_config *ppu_ext_cfg,
                                uint32_t bit_depth, struct pipe_video_buffer *target); /* :195 */
void  mwv207_dec_h264_set_regs(struct mwv207_video_decoder *, struct pipe_video_buffer *); /* dec_h264.c:2312 */
void  mwv207_dec_hevc_set_regs(struct mwv207_video_decoder *, struct pipe_video_buffer *); /* dec_hevc.c:1224 */

/* 寄存器间接层 / 提交 */
struct mwv207_video_reg_buf *mwv207_video_reg_buf_create(struct mwv207_winsys *ws); /* video_reg_buf.c:90 */
void  mwv207_9200_set_dec_register(struct mwv207_video_reg_buf *, uint32_t id, uint32_t value); /* reg.c:53 */
void  mwv207_9200_set_addr_reg(struct mwv207_video_reg_buf *, uint32_t id,
                               struct mwv207_bo *bo, uint64_t offset, uint32_t flags);      /* reg.c:84 */
void  mwv207_9200_set_addr_reg_hi(struct mwv207_video_reg_buf *, uint32_t id,
                                  struct mwv207_bo *bo, uint64_t offset, uint32_t flags);   /* reg.c:104 */
void  mwv207_video_cs_init(struct mwv207_video_cs *cs, struct mwv207_winsys *ws);  /* video_cs.c:35 */
void  mwv207_video_cs_submit(struct mwv207_video_cs *cs,
                             struct mwv207_video_reg_buf *regbuf);                  /* video_cs.c:81 */

/* 公共解析 */
uint32_t mwv207_h264_bsd_decode_slice_header(struct mwv207_strm_data *,
        struct mwv207_h264_picture_desc *, mwv207_h264_slice_header *,
        mwv207_h264_nal_unit *);                                                   /* h264_slice_header.c:91 */
uint32_t mwv207_hevc_decode_slice_header(struct mwv207_strm_data *,
        struct mwv207_h265_picture_desc *, uint32_t *hw_skip_bits,
        struct mwv207_hevc_slice_header *, uint32_t st_rps_bits);                  /* hevc_slice_header.c:159 */
uint32_t mwv207_sw_get_bits(struct mwv207_strm_data *stream, uint32_t num_bits);   /* sw_stream.c:36 */
uint32_t mwv207_hevc_decode_exp_golomb_unsigned(struct mwv207_strm_data *, uint32_t *); /* exp_golomb.c:60 */

/* PPU */
void  mwv207_pp_set_regs(struct mwv207_video_reg_buf *,
        struct mwv207_video_dec_hw_features *, mwv207_pp_unit_int_config *,
        uint32_t mono_chrome, uint32_t bottom_field_flag);                          /* dec_ppu.c:1248 */
void  mwv207_pp_unit_set_int_config(mwv207_pp_unit_int_config *, mwv207_pp_unit_config *,
        uint32_t pixel_width, uint32_t frame_only, uint32_t mono_chrome);           /* :1652 */
uint32_t mwv207_check_pp_unit_config(struct mwv207_video_dec_hw_features *,
        uint32_t in_width, uint32_t in_height, uint32_t interlace,
        mwv207_pp_unit_int_config *);                                              /* :1006 */

/* 跨层 */
struct pipe_context *ctx;  ctx->create_video_codec / create_video_buffer;         /* 安装于 codec.c:144-145 */
bool vl_codec_supported(struct pipe_screen *, enum pipe_video_profile, bool encode); /* vl_codec.c:31-75 */
```

### 14.4 交叉引用

- [00-ARCHITECTURE.md](00-ARCHITECTURE.md) §4.4（视频分层草图，本篇为其细化）
- [01-WINSYS-KMD.md](01-WINSYS-KMD.md)（`cmdbuf_commit` / DRM ABI / KMD 通路）
- [../registers/REGISTERS.md](../registers/REGISTERS.md) §1.1（命令字编码与 `_Address>>2` 规则）
- [06-KNOWN-ISSUES.md](06-KNOWN-ISSUES.md)（本篇 §13 中的疑点在此汇总）
- [`../reports/HWDECODE-FEASIBILITY-REPORT.md`](../../reports/HWDECODE-FEASIBILITY-REPORT.md)（10bit 黑屏修复、性能数据）
- [`../reports/FULLFEATURE-REPORT.md`](../../reports/FULLFEATURE-REPORT.md)（VP9 缺口、编码缺口）