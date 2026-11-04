# VP9 硬解（D1）交接文档

2026-11-04 · 状态：**单 tile 与多 tile-columns 码流已与软件逐位一致**；tile rows 不支持（会挂死，已防护跳过）。

目标：VP9 Profile0 8bit 硬解输出与软件解码逐位一致后提交。

## 1. 结论摘要

- 已打通：VA 广播（`vainfo` 显示 `VAProfileVP9Profile0 : VAEntrypointVLD`）、码流头解析、寄存器序列（对照厂商 1.8.0 `jmgpu_drv_video.so` 反汇编 `/tmp/vp9-re.md`）、出图链路（decoder staging → PP → VA 导出）。
- **单 tile 码流现在逐位一致**：128×128 棋盘/渐变、640×360 彩条、640×360 testsrc2 30 帧（realtime 与 crf20 两种编码）、`w6.webm` 50 帧，全部 Y/U/V PSNR 99.00（即逐位相同），涵盖 4×4/8×8/16×16/32×32 变换、intra 与 inter(P) 帧。
- **多 tile-columns 也已逐位一致**（见第 6 节）：关键是驱动把 tile 负载重新拷进 scratch 流缓冲，并在**每个 tile 前（含最后一个）**补 4 字节大端长度；硬件自己按前缀逐个解 tile，不需要"按 tile 追加命令"。
  - `tc5`（512×128，2 tiles，lossless）左右两半均 99.00；`tc4`（1024×128，4 tiles）99.00；`tc30`（640×360，2 tiles，30 帧）30/30 帧 99.00；`vp9-kf`（640×360，2 tiles）99.00。
- 仍不支持：**tile rows**（`log2_tile_rows > 0`）。这类码流会让 DEC 引擎挂死（第 5 节），当前全部跳过并返回占位帧；探索记录见第 6.2 节。

## 2. 测试环境

构建（VA/B 目标）：

```sh
ninja -C ref/oh-mesa/User_source_code/mesa-24.1.2/build-full \
      src/gallium/targets/va/libmwv207_drv_video.so
cp ref/oh-mesa/User_source_code/mesa-24.1.2/build-full/src/gallium/targets/va/libmwv207_drv_video.so /tmp/gd3/
```

测试（`/tmp/dlshim.so` 把 dlopen 的 `mwv207_drv_video.so` 指到 `/tmp/gd3`）：

```sh
MWV207_VP9_CLEAR=1 LIBVA_DRIVER_NAME=mwv207 LD_PRELOAD=/tmp/dlshim.so \
ffmpeg -v error -y -hwaccel vaapi -hwaccel_device /dev/dri/renderD128 \
  -hwaccel_output_format vaapi -i IN -vf hwdownload,format=nv12 \
  -pix_fmt yuv420p -f rawvideo OUT
```

- 回归脚本 `/tmp/reg.sh`：对 cb4/cb/blkramp/ramp128c10/ramp640c10/kf1t/vp9-kf/noise/w6/vp9-640 逐码流打印 Y PSNR。
- 30 帧对比脚本 `/tmp/p30.sh`。
- 素材：`/tmp/w6.webm`（128×128，50 帧，白→黑，含 inter 帧）、`/tmp/kf1t.webm`（640×360 彩条，单 tile）、`/tmp/rt30.webm`、`/tmp/q30.webm`（testsrc2 640×360 30 帧，单 tile）、`/tmp/cb.webm`、`/tmp/cb4.webm`、`/tmp/blkramp.webm`、`/tmp/noise.webm`、`/tmp/ramp128c10.webm`、`/tmp/ramp640c10.webm`；多 tile 素材 `/tmp/vp9-kf.webm`（l2c=1）、`/tmp/vp9-640.webm`（l2c=1 / l2r=1）。
- 软件参考：`ffmpeg -i x.webm -f rawvideo -pix_fmt yuv420p sw.yuv`。

## 3. 已实现的代码（本提交）

- `video-codec/mwv207/mwv207_dec_vp9.c`（新建）：`mwv207_dec_vp9_set_regs()`、scratch 布局、参考帧自持存储、PP 配置、submit_done、destroy。
- `video-codec/mwv207/mwv207_dec_vp9.h`（新）：`set_regs` / `submit_done` / `destroy_state`。
- `common/mwv207_decoder_vp9_slice_header.c`：新增 `mwv207_vp9_parse_frame(sh, data, size, fallback_w, fallback_h, saved_ctx)` wrapper；`remove_emul3_byte = 1`；帧上下文（`entropy_last`）跨帧继承；调试打印全部用 `MWV207_VP9_DBG` 门控。
- `mwv207_video_dec.h/.c`：dec 增加 `void *vp9_state`，destroy 路径清理。
- `mwv207_video_decoder.c/.h`：`get_vp9_msg`、end_frame 分派、VP9 用 `mwv207_video_cs_submit_wait()` 同步提交后调用 `mwv207_dec_vp9_submit_done()`。
- `common/mwv207_video_cs.c/.h`：`mwv207_video_cs_submit_internal(cs, regbuf, wait)` + `mwv207_video_cs_submit()` / `mwv207_video_cs_submit_wait()`。
- `meson.build`：源列表加入 dec_vp9.c；`mwv207_video_codec.c`：profile allowlist 加入 VP9 Profile0。

## 4. 关键修复（按发现顺序）

1. **emulation-prevention 字节**：VP9 码流不存在 `00 00 03` 转义，但位读取器在 `remove_emul3_byte == 0` 时会主动跳过该模式，导致头解析漂移（inter 帧交替 `tag=11 part=32` → `vp9: header parse failed`）。wrapper 里置 `rb.remove_emul3_byte = 1`。
2. **量化模型**：ASIC 的块量化来自 segment 0 的特征字（`JMDEC_QUANT_SEG0`，id 315）= `base_qindex`，而 294/295/296 是**带符号的每平面增量**（`qp_ydc/qp_ch_dc/qp_ch_ac`，`& 0x3f`），不是绝对值。此前把 294 当绝对量导致增益 0.365 的“对比度压缩”。
3. **帧上下文概率跨帧继承**：每帧都从默认概率表起步会让所有 inter 帧预测错误。`saved_ctx[4]` 由状态保存，key/error_resilient/reset=3 时重置全部，reset=2 只重置当前 index，解析完写回。
4. **参考帧缩放 = 14 bit 定点**：`1.0 == 1<<14`（`(w<<14)/scaled_w`），原来写 `1<<16`。
5. **`JMDEC_STREAM_LEN` 必须留 look-ahead 余量**：布尔解码器会超前读几个字节，寄存器等于精确 tile 长度时硬件会丢弃**帧的最后一个块**（棋盘最后一格=0）。现在 `stream_len = tile_len + min(64, buffer 剩余)`。
6. **同步提交**：参考槽的 CPU 侧拷贝必须在硬件完成后进行，否则非 debug 模式下读到未完成的参考帧。

## 5. 已知坑

- 初始化必须：scratch 创建时 memset 一次；不要每帧清（prob/计数器跨帧）。
- `mwv207_video_dec_destroy_buffer()` 每帧调用；vp9_state 销毁必须放在 `mwv207_video_dec_destroy()`，否则参考槽每帧丢。
- 参考帧不能指向 VA target buffer（会释放），必须驱动自持 staging BO。
- 调试打印不要按 640×360 固定偏移读，小分辨率会越界 SIGSEGV。
- `JMDEC_DEC_TILE_INT_E`（16）置 1 会导致整帧全 0，必须 0。
- 输出 staging 是 **4×4 块平铺、块按光栅序**：`off(px,py) = ((py>>2)*(w>>2) + (px>>2))*16 + (py&3)*4 + (px&3)`（仅调试用）。
- 寄存器表 `mwv207_video_dec_8170table.h` 存在 word 位域重叠（288 与 294 同属 word 13），不要同时写 288 和 294。
- **tile rows（`log2_tile_rows > 0`）的码流会挂死 DEC 引擎**（2026-11-04 实测，桌面因此卡死）。内核每 2 s 打印
  `mwv207: job from 'av:vp9:dfN' to 'mwv207_dec0', timed out!` + `irq state reg: 0x00040100`（DEC_E 一直为 1、DEC_IRQ=0，
  完成中断永不触发），并反复重试同一条命令缓冲；该 GPU 同时驱动显示，命令队列被堵住后整屏停止刷新。
  `MWV207_VP9_REGSET="82=0,1128=<非0偏移>,136=..."` 这类把 `STRM_START_OFFSET` 当字节偏移用的非法探针同样会挂死。
  现在 `mwv207_dec_vp9_set_regs()` 对 `log2_tile_rows != 0` 的帧直接 `DEC_E=0` 跳过（staging 填 16，打 ERROR 日志），
  避免再次把机器挂住；`MWV207_VP9_TILEROW=1` 可强制放行（有挂死风险，仅在受控实验时使用）。

## 6. 多 tile

### 6.1 tile columns：已解决（逐位一致）

实现（`mwv207_dec_vp9.c`）：

1. 按 VP9 规范解析 tile 负载：tile 网格按行优先排列，**除最后一个 tile 外，每个 tile 的数据前都有 4 字节大端长度**（即 `size,data,size,data,…`，不是把 n-1 个长度集中放在前面）。解析必须顺序进行：读长度 → 跳过该 tile 数据 → 读下一个长度；最后一个 tile 取帧剩余字节。早期按 `tile_off + 4*(n-1)` 的"长度表"解释在 n=4 时立即失败（`bad tile size 2142148336`），可作为反面教材。
2. 把整帧拷进 scratch 的流缓冲（`st->stream_offset`），在**每一个** tile（包括最后一个）前写入 4 字节大端长度；`STREAM_BASE`/`STRM_START_BIT` 指向该副本，`STREAM_LEN = 副本长度 - tile_off + 最少 64 字节余量`。硬件随后自己沿前缀逐个解 tile。
3. 寄存器：`JMDEC_TILE_ENABLE=1`，`JMDEC_NUM_TILE_COLS=1<<log2_tile_columns`，`JMDEC_NUM_TILE_ROWS=1<<log2_tile_rows`，`JMDEC_DEC_TILE_INT_E=0`；tile 表仍写 `(coldelta,rowdelta)` 超级块增量对（表本身对 columns 不是关键，但保持主线格式）。单 tile 帧不走拷贝路径，行为与之前完全一致。

为什么不是"按 tile 追加命令"：厂商 `decode_frames` 的 `ctx[0x9cf0]` 分支确实暗示多命令，但实测**每 tile 追加命令会直接挂死引擎**（`'av:vp9:df0'` 超时、`irq state 0x00040100`，6 次重试后放弃），且首条 `DEC_E=1`/后续 `DEC_E=0`、`TILE_ENABLE=0`、每命令都 `DEC_E=1` 等变体都挂。真正缺的是"最后一个 tile 也要有长度前缀"，硬件按前缀自走，一条命令即可。

### 6.2 tile rows：不支持，已防护

- tile rows 的帧（`log2_tile_rows>0`，例如 `vp9-640.webm` 的 inter 帧、`-tile-rows 1` 编码的 `tr2/trm`）只要 `TILE_ENABLE=1` 就会挂死 DEC 引擎（第 5 节）；`DEC_TILE_INT_E`、`NUM_TILE_ROWS_8K`(244) 取值、tile 行 8 KiB 对齐、把表项换成流字节偏移等都试过，均无用（要么仍挂，要么只有第一行正确）。
- 用 1×1 描述符（vendor 在 `ncol==1` 时走的分支）+ `TILE_ENABLE=1` 可以不挂，但**只有第一 tile 行正确**（`trm` 640×256：row0 99.00、row1 12 dB），第二行不参与解码；`NUM_TILE_ROWS` 设为 2、`244` 设 1/2/4 都不改变结果。
- 结论：本芯片/本寄存器序列下没有找到 tile rows 的正确打法，保持"跳过 + 占位帧"的防护（`MWV207_VP9_TILEROW=1` 可强制放行，仅受控实验用，有挂死风险）。

## 7. 验证标准（当前结果）

- `w6`：50/50 帧 Y/U/V 逐位一致（99.00 dB）✅
- 30 帧实拍（单 tile，testsrc2 640×360，realtime 与 crf20）：30/30 帧逐位一致 ✅
- 单 tile 各类合成码流（4×4/8×8/16×16/32×32 TX、棋盘、渐变、噪声）：逐位一致 ✅
- 多 tile-columns：`tc5`（2 tiles）99.00、`tc4`（4 tiles）99.00、`tc30`（30 帧 2 tiles）30/30 帧 99.00、`vp9-kf` 99.00 ✅
- tile rows：❌ 不支持；已防护跳过（不会挂死引擎），占位帧输出（见第 6.2 节）
- 单 tile 无 `job ... timed out`（`dmesg` 清空后跑解码无内核报错）✅；tile rows 会超时挂死（已防护，见第 5 节）