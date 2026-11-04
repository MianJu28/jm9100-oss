# VP9 硬解（D1）交接文档

2026-11-04 · 状态：**单 tile 码流已与软件逐位一致**；多 tile（tile columns / tile rows）仍有缺陷。

目标：VP9 Profile0 8bit 硬解输出与软件解码逐位一致后提交。

## 1. 结论摘要

- 已打通：VA 广播（`vainfo` 显示 `VAProfileVP9Profile0 : VAEntrypointVLD`）、码流头解析、寄存器序列（对照厂商 1.8.0 `jmgpu_drv_video.so` 反汇编 `/tmp/vp9-re.md`）、出图链路（decoder staging → PP → VA 导出）。
- **单 tile 码流现在逐位一致**：128×128 棋盘/渐变、640×360 彩条、640×360 testsrc2 30 帧（realtime 与 crf20 两种编码）、`w6.webm` 50 帧，全部 Y/U/V PSNR 99.00（即逐位相同），涵盖 4×4/8×8/16×16/32×32 变换、intra 与 inter(P) 帧。
- 仍未解决：**多 tile 码流**。tile columns 的 640×360（`vp9-kf.webm`）只有第 0 个 tile 正确（tile 1 是噪声）；带 tile rows 的 `vp9-640.webm` 整帧全 0。

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

## 6. 下一步：多 tile

现象与证据：

- `vp9-kf.webm`（640×360，l2c=1）：tile 0（x<320）逐位正确，tile 1（x≥320）是噪声，整帧 15.5 dB。
- 把流指针指到 tile 1 的数据（`tile_off=236`，tile0 size=2823 → 下一 tile 在 3063；`STRM_BASE=3056`、`STRM_START_BIT=56`）并关闭 tile 标志时，tile 1 **能正确解出**（内容与软件右半一致）→ 硬件具备解任意 tile 的能力，问题是“一帧一条命令”时它不去解第二个 tile。
- 厂商 `decode_frames` 里有 `if (ctx[0x9cf0] == 1) 首条命令` 分支（`DEC_E` 只在首条命令置 1），并且有 `ctx[0x10d0 + 48*idx]` 之类的**每槽/每 tile 状态**，暗示多 tile 需要**按 tile 追加命令**（同一帧多条命令，首条 `DEC_E=1`，后续 `DEC_E=0`），当前驱动每帧只提交一条。
- 已排除：tile 表项顺序（`(coldelta,rowdelta)` 正确，交换后更差）、表项步长（4 字节 / 8 / 16）、`NUM_TILE_COLS/ROWS` 取值（0…3 无差别）、`NUM_TILE_ROWS_8K`（0/1/4/6/10378 全部无差别，疑似该字段在本芯片无效）、`DEC_TILE_INT_E`、legacy `NUM_TILE_COLS_V0/ROWS_V0`、`VERT_FILT/BSD` 基址、`DEC_MODE`、tile 表地址。
- 也试过（无效）：`STRM_START_OFFSET`(1128) 当 tile 字节偏移用（整帧不再出图）、把 `STREAM_LEN` 设为精确 tile 长度 / tile0 长度 / tile0+前缀长度（tile 0 仍正确、tile 1 仍噪声）、把 tile 负载拷贝到 16 字节对齐的 scratch 流缓冲（单 tile 也失败，说明该实验实现有误或 scratch 不能当流源）。

建议实现：解析每个 tile 的 4 字节大端长度前缀，算出各 tile 的起始字节/长度；为第 1 个 tile 之后的每个 tile 追加一条命令（`STRM_BASE` 对齐到 16 字节、`STRM_START_BIT` 为余数、`STREAM_LEN` 为该 tile 长度+余量、`DEC_E=0`），并在首条命令之后只更新流相关寄存器。需要给 codec 增加“提交额外命令”的能力（现在只有 `dec->decoder->reg_buf` 一份，`end_frame` 只提交一次）。

## 7. 验证标准（当前结果）

- `w6`：50/50 帧 Y/U/V 逐位一致（99.00 dB）✅
- 30 帧实拍（单 tile，testsrc2 640×360，realtime 与 crf20）：30/30 帧逐位一致 ✅
- 单 tile 各类合成码流（4×4/8×8/16×16/32×32 TX、棋盘、渐变、噪声）：逐位一致 ✅
- 多 tile：❌（见第 6 节）
- 无 `job ... timed out`（`dmesg` 清空后跑解码无内核报错）✅