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
- 进一步实验（2026-11-04 下午，`/tmp/trb.webm` = 640×256、上半 235 下半 16、lossless、`-tile-rows 1`，两个 tile 只有 16/20 字节，tile_off=29）：
  - 1×1 描述符 + `TILE_ENABLE=1`：**不挂**，上半 235 完全正确，下半被填充成 233（"上一行预测/掩盖"），即硬件只解了 tile 0，第二个 tile 行连数据都没读（tile 1 的 20 字节完全没用上）。
  - 网格表 (10,2),(10,2) + `NUM_TILE_ROWS=2`：**挂死**（6 次超时、全 0），即使两个 tile 只有 36 字节也一样挂 ⇒ 不是数据量/长度问题。
  - 逐项排除：`NUM_TILE_COLS/ROWS` 与 legacy 字段（用 id 233/234 写 word10 的 shift20/shift15 位）、`NUM_TILE_ROWS_8K` 1/2/4、tile 行按 8 KiB 对齐、表项换成流字节偏移、`FILT_TILE_BORDER`(178, hantro 的 filter_over_tiles)=1、`DEC_TILE_INT_E` 均不改变"只解第一行"或"挂死"的结果。
  - 观察 `/usr/src/mwv207-1.8.0.uos` 里没有 VP9 解码头文件（只有一个 `jm_dec_tile_mode` 枚举，讲的是图像内存平铺，不是 VP9 tile）；厂商 UMD 在盘上只剩 0 字节 stub，无法反汇编。
  - 结论（暂定）：本 ASIC 的这条寄存器路径很可能**不支持 VP9 tile rows**（tile columns 正常）。保持"跳过 + 占位帧"的防护（`MWV207_VP9_TILEROW=1` 可强制放行，仅受控实验用，有挂死风险）。
  - 追加实验（同日更晚，仍未成功）：
    * **按 tile 行追加命令**已实现并实测：每行一条命令，窗口指向该行在拷贝缓冲里的 `(4 字节前缀 + 负载)`，首条 `DEC_E=1`、后续 `DEC_E=0`、末条 `submit_wait`，行表用真实 SB 增量 —— 结果仍是 6 次 `timeout`、输出全 0。⇒ 追加命令不是答案，粒度换成"行"也一样挂。
    * tile filter / BSD 区从 2 MB 放大到 8 MB / 16 MB：仍挂 ⇒ 不是辅助内存不够。
    * 显式表值 `(0,20) (16,20) (24,20) (20,24) (45,0) (33,0) (20,0)` 等"字节偏移"猜测：**不挂**，但第二行永不解码（row1 恒为 233.4 的掩盖值）⇒ 表里的第二个 u16 被硬件当 SB 增量/行推进量，不是行数据地址。
    * u32 打包表（`row<<16|col`）：挂；`FILT_TILE_BORDER=1`：挂。
  - 稳定的判读：**只要硬件真正"进入"第二个 tile 行就会自旋挂死**（无论一条命令还是每行一条命令），只有 1×1 描述符能避开挂死，但那样第二行根本不参与解码。因此问题不在命令粒度，而在第二行数据的取址/格式，或该 ASIC 修订根本不支持 tile rows。
  - 若要继续，只剩两条路（都有挂死风险，需受控）：(a) 把 tile 表放到单独的 32 B/SB 的 buffer D 里（厂商 `nseg = supercols*superrows*32`），即换掉 scratch 里 4 B/表项的区域；(b) 找一台有完整厂商 UMD 的同型号机器导出 `jmgpu_drv_video.so`，反汇编 `decode_frames` 的 `ctx[0x9cf0]` 分支与 §6 未解码部分（`calc_sur_offset`、buffer C/D 槽位索引、`Vp9AsicAllocateFilterBlockMem`）。

### 6.3 厂商 UMD 静态逆向结果（2026-11-04 傍晚）

盘上其实有**完整的厂商 UMD**：`/usr/lib/aarch64-linux-gnu/dri/jmgpu_drv_video.so`
（1315584 B，aarch64，**未 strip**，mwv207-dev 包提供；linglong 缓存里的同名文件是 0 字节
stub，不要被误导）。反汇编（`objdump -d --no-show-raw-insn`，见 /tmp/vendor.asm）后确认：

- `jmgpu_decoder_vp9_set_tile_register`：tile 表写在 **buffer B（prob/counter BO）内固定偏移
  0x4270**（= prob 3744 B + counter 13264 B），`TILE_BASE`(1022/1021) 指向
  `bufferB + ctx[0x10B0+4*slot]`；`TILE_ENABLE`(245) = `(log2_tile_rows ||
  log2_tile_columns)`；表项为 **（coldelta u16, rowdelta u16）**，行优先，外层行内层列，
  且 **rowdelta 是每行增量** `((i+1)*sr >> l2r) - (i*sr >> l2r)`（不是 hantro 主线的累加修正）；
  随后写 `NUM_TILE_COLS=1<<l2c`（241 或 242）与 `NUM_TILE_ROWS=min(1<<l2r, sr)`（243 或 244）。
  同一函数还写 `DEC_VERT_FILT_BASE`(1049/1047) = bufferC + 0、
  `DEC_BSD_CTRL_BASE`(1057/1055) = bufferC + 系数/filter 缓冲大小。
- `check_and_alloc_asic_buffer`：buffer B 大小 `max(0x5040, 需求)`，counter 常量 **13264
  (0x33D0)**；buffer C = 系数/filter + BSD；buffer D = **supercols*superrows*64** 字节（不是
  早先报告里推测的 32）；segment map = `max(align4k(w*h), 0x1FB000)`。
- `set_stream_register`：流起址 = `bo_base + ctx[0x1360] + ctx[0x9CF0]`（两者在本库里都只被读、
  没有写点 ⇒ 恒 0，即**从帧数据开头**开始），`STRM_START_BIT=(addr&0xf)*8`，
  `STREAM_LEN=(base-addr)+本帧拷贝字节数`；拷贝时按 316 字节的 slice 结构逐块 memcpy。
- 本仓库已按上述布局改好（commit `0324b19`：prob 0、counter 0xEA0/0x33D0、tile 表紧跟 counter、
  行增量公式对齐厂商），单 tile 与多 tile-columns 全量回归仍 13/13 条流 99.00、无超时。

**为什么仍无法得到"真值"对照**：厂商 UMD 需要厂商私有内核字符设备 `/dev/jmgpu` 和私有
ioctl（`_IOC(_IOC_NONE,0x65,0xd,0)`、`_IOC(_IOC_READ,0x6c,0x8,0x8)` …）。本机跑的是 OSS
`mwv207` 内核模块，没有 `/dev/jmgpu`；手工建 `/dev/jmgpu -> /dev/dri/renderD128` 后 open 成功但
所有私有 ioctl 返回 ENOTTY，`vainfo`/`ffmpeg` 初始化失败。厂商 kernel 模块在本机不可加载
（会与 OSS 模块争抢同一 PCI 设备，且桌面显示依赖它）。⇒ 运行时差对照只能另找一台装厂商
内核模块的机器。

用厂商布局 + 厂商行增量跑 `trb`（tile rows）**仍然挂死** ⇒ 布局、表值都已对齐，剩下的差异只
可能在 (i) buffer D 的地址/内容（我方从不分配该缓冲）、(ii) 每槽 1049/1057 指向的 buffer C
内部相对布局（我方用独立的 2 MB filter 区）、(iii) `decode_pictures` 里 `ctx[0x9D38]`
（多命令/多图像）分支的触发条件。

## 7. 验证标准（当前结果）

- `w6`：50/50 帧 Y/U/V 逐位一致（99.00 dB）✅
- 30 帧实拍（单 tile，testsrc2 640×360，realtime 与 crf20）：30/30 帧逐位一致 ✅
- 单 tile 各类合成码流（4×4/8×8/16×16/32×32 TX、棋盘、渐变、噪声）：逐位一致 ✅
- 多 tile-columns：`tc5`（2 tiles）99.00、`tc4`（4 tiles）99.00、`tc30`（30 帧 2 tiles）30/30 帧 99.00、`vp9-kf` 99.00 ✅
- tile rows：❌ 不支持；已防护跳过（不会挂死引擎），占位帧输出（见第 6.2 节）
- 单 tile 无 `job ... timed out`（`dmesg` 清空后跑解码无内核报错）✅；tile rows 会超时挂死（已防护，见第 5 节）