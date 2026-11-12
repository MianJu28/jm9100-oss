# JPEG baseline 硬解调研（2026-11-13）

**结论：在本机（JM9100 / MWV207，`hw_build_id 0x1F76`）上无法实现 JPEG baseline 解码。**
硬件对完全按 mainline Hantro G1 参考编程的 JPEG 作业报 `DEC_ERROR_INT`，不写任何输出
像素，2 s 后由调度器 TDR 复位；厂商 VAAPI 驱动里也只有 JPEG **编码**侧参数。因此收回
`VAProfileJPEGBaseline` 声明并删除实现，只保留两处顺带发现的真修复。

## 1. 为什么先看 JPEG

`video-codec/mwv207/mwv207_video_dec_vpu_features_list.h` 里 `0x1F76` 的能力项：

| 项 | 值 | 含义 |
|---|---|---|
| `jpeg_support` | 1 | 声称支持 |
| `jpeg_esupport` | 1 | 声称支持（编码） |
| `jpeg_prog_support` | 0 | 不支持 progressive |
| `mpeg2_support` / `mpeg4_support` / `vc1_support` / `vp8_support` | **0** | 熔断，硬件没有 |

⇒ MPEG-2/MPEG-4/VC-1/VP8 不必再评估；JPEG 是唯一“硬件声称有、我方没实现”的解码缺口。
我方 `mwv207_video_decoder_end_frame()` 的 `case PIPE_VIDEO_FORMAT_JPEG:` 原本是空 `break`，
`mwv207_profile_supported()` 也没有 JPEG。

## 2. 参考实现与寄存器映射

ST 的 `linux` v6.6-stm32mp 分支有现成的 Hantro G1 JPEG 解码器，取回到
`~/ref-kernel/verisilicon/`：`hantro_g1_jpeg_dec.c`、`hantro_jpeg.c`、`hantro_g1_regs.h`、
`hantro_jpeg.h`。

它给出的关键约定：

- 位流基址 = 整段 JPEG + `ecs_offset`（跳过 SOI…SOS 头），`STREAM_LEN` = **整段**载荷长度；
- 输出：`ADDR_DST`（luma）、`ADDR_DST_CHROMA = dst + w*h`；`JPEG_MODE` 4:2:0→2、4:2:2→3；
- 544 B 辅助表：`qp_tables[3][64]`（按 zig-zag 还原）+ `ac_tables[2][162]` + `dc_tables[2][12]`
  + `padding[4]`，整块按 32 位 `swab32`；
- VLC 码长：AC1/AC2/DC1/DC2 的 `CODE1..16_CNT` 写在 `0x040..0x068`，luma/chroma 按
  `scan->component[0]` 的 `Ta/Td` 交换；
- 寄存器启动序列：`DEC_CTRL0=DEC_MODE(3)|FILTERING_DIS`、`DEC_CTRL1=MB 宽高`、
  `DEC_CTRL2=JPEG_MODE|STREAM_ALL|QTABLES|VLC 选择|STRM_START_BIT`、`DEC_CTRL3=STREAM_LEN`、
  `DEC_CTRL4=垂直补齐`、`JPEG_CTRL=SLICE_H(0)`、`CONFIG`（字节交换、burst）、`INTERRUPT=DEC_E`。

我方寄存器表 `mwv207_dec_register_array_7020.h` + `mwv207_video_dec_8170table.h` 逐项对齐后
（名字数组第 0 项是许可证文本里的 "Software"，须偏移 +1；KMD `mwv207_pipe_dec.c:173`
的 `reg = (cmd & 0xffff) * 4` 印证 word*4 = MMIO 偏移），上述寄存器全部存在且位布局一致。
值得记的三个坑：

1. **地址寄存器是 MSB 在前**：`YBASE_MSB 0x100` / `YBASE_LSB 0x104`；`JPG_CH_OUT_BASE_MSB`
   与 `REFER0_BASE_MSB` 都复用 `0x108`、LSB 复用 `0x10c`。用 `id_lsb + 1` 推 MSB 是错的。
2. `JMDEC_DEC_OUT_YBASE_LSB` 与 `JMDEC_DEC_OUT_BASE_LSB` 同址 `0x104`。
3. JM 的 `DEC_TAB_SWAP` 对非 HEVC/VP9（含 JPEG）默认 `8|16` 字节交换，所以表按自然序
   写即可，不需要像 G1 那样自己 `swab32`。

## 3. 实现内容（已删除，仅记录）

新增 `video-codec/mwv207/mwv207_dec_jpeg.{c,h}`：`get_msg()` 从 `pipe_mjpeg_picture_desc`
取图像参数/量化表/Huffman 表/扫描参数；`set_regs()` 里找 ECS（遍历标记段到 `0xDA`）、
把 ECS 拷进独立 256 B 对齐缓冲（`STRM_START_BIT=0`）、填 544 B 辅助表并按 16 项寄存器
写码长、按 LSB→MSB 映射设 `STREAM/QTABLE/YBASE/JPG_CH_OUT/CBASE`、最后按 vp9 的套路
跑 PPU 把平铺 NV12 转到目标面。

接线：profile 声明、`is_video_format_supported`、`end_frame` 分派、`mwv207_9200_video_dec_picture`
的 JPEG 分支、`meson.build` 源文件。

编译期顺带修掉的真 bug：`mwv207_video_buffer.h` 的 include guard 写成了
`#ifndef mwv207_video_buffer_h` / `#define jmpgu_video_buffer_h`，二次 include 会重定义。

## 4. 实测与失败路径

| 阶段 | 结果 |
|---|---|
| `vainfo` | 出现 `VAProfileJPEGBaseline: VAEntrypointVLD` |
| 首次 ffmpeg | `Failed setup for format vaapi` → `is_video_format_supported` 漏 JPEG → 补后初始化成功 |
| 解码报错 | `jpeg: no SOS marker in bitstream` → Mesa 的 3 缓冲里 buffer0 是合成头，被 `num_buffers==3` 规则丢掉 |
| 修好后 | 不报错，但 `hw.yuv` 全零 |
| 地址回读（KMD `peek`） | `0x930104/0x93010c/0x9302bc` 全是 0 → 发现 MSB 映射错，修后地址正确 patch |
| 仍失败 | DEC0 状态 `0x930004 = 0x00010102`：bit8 `DEC_IRQ` + bit16 `DEC_ERROR_INT`；轮询等 bit12 永远等不到 |
| 后果 | 每帧 2 s 后 `job from 'dec0:0:mjpeg' to 'mwv207_dec0', timed out!` + `irq state reg: 0x00010102` |
| 输出面 | 连解两帧，第二帧开头统计上一帧解码面：`size=2592000 nonzero=0` |
| 错误详情 | `0x414`（ERROR_ADDR_X/Y、SLICE_DATA/HEADER）、`0x0c0`、`0x428` 全 0 |

试过的变体（全部同样 `0x00010102`、零像素）：

1. `PJPEG_E` / `PJPEG_LAST_SCAN_E` / `JPEG_INTERLEAVED_E` + `PJPEG_SS/SE/AH/AL`；
2. 线性输出（跳过 PPU，直接写目标 NV12 面）；
3. 平铺输出（vp9 的 7680 stride 几何）+ PPU；
4. ECS 单独对齐拷贝、`STRM_START_BIT=0`（已用）。

错误状态 `0x10102` 与 **I-4（H.264 High10）** 完全同型：能力表说支持，硬件一启动就
进 error 中断、管道卡到 TDR。

## 5. 厂商侧证据

`~/vendor-src/usr/lib/aarch64-linux-gnu/dri/jmgpu_drv_video.so` 的字符串里：

- `Encoded jpeg width. jpgLumWidth(unit 8 pixels)`
- `JPEG CodingMode 0:420 1:422 2:lumaonly`
- `JPEG encoder supported by HW. 0=not supported. 1=supported`
- `jpgLumHeight(unit 8 pixels):Max width x height is 16384 x 16384`

全部是**编码器**参数，没有任何解码侧字段。结合 `jpeg_support` 与 `jpeg_esupport`
两个独立能力位，最合理的解释是：`jpeg_support` 覆盖的是同一块 JPEG 编解码单元，
但解码路径需要厂商未公开的额外启动条件（类似 high10p 需要的位流预转换）。

## 6. 处理

- 收回 `PIPE_VIDEO_PROFILE_JPEG_BASELINE` 声明，删除 `mwv207_dec_jpeg.{c,h}` 与全部接线；
- 保留：`mwv207_video_buffer.h` 的 guard 修复、`decode_bitstream()` 的“buffer0 以 SOI
  开头则保留”修复（对 H.264/HEVC 无行为变化）；
- 回归：`tools/vp9-regression.sh` 19/19、`tests/query` 4/4。

## 7. 建议

JPEG 解码在拿到厂商文档/固件之前不要再投入。同一张能力表里 **AVS/AVS2
（`avs_support`/`avs_plus_support`/`avs2_support`/`avs2_main10_support` 都是 1）** 是
下一个“硬件有、我方没有”的候选，但国内样片与参考实现都更难找，收益也低于继续打磨
已有通路。