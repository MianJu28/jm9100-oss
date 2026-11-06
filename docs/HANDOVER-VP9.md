# VP9 硬解（D1）交接文档

2026-11-06 · 状态：**VP9 Profile0 8bit 全部样片（单 tile / 多 tile-columns / tile rows / 隐藏
alt-ref 帧 / 1080p）已与软件解码逐位一致**。修复过程与最终结论见 §8、§9；
**尚未解决的问题、已知限制与后续优先级见 §10（接手先看这一节）。**

目标：VP9 Profile0 8bit 硬解输出与软件解码逐位一致后提交。

## 1. 结论摘要

- 已打通：VA 广播（`vainfo` 显示 `VAProfileVP9Profile0 : VAEntrypointVLD`）、码流头解析、寄存器序列（对照厂商 1.8.0 `jmgpu_drv_video.so` 反汇编 `/tmp/vp9-re.md`）、出图链路（decoder staging → PP → VA 导出）。
- **单 tile 码流现在逐位一致**：128×128 棋盘/渐变、640×360 彩条、640×360 testsrc2 30 帧（realtime 与 crf20 两种编码）、`w6.webm` 50 帧，全部 Y/U/V PSNR 99.00（即逐位相同），涵盖 4×4/8×8/16×16/32×32 变换、intra 与 inter(P) 帧。
- **多 tile-columns 也已逐位一致**（见第 6 节）：关键是驱动把 tile 负载重新拷进 scratch 流缓冲，并在**每个 tile 前（含最后一个）**补 4 字节大端长度；硬件自己按前缀逐个解 tile，不需要"按 tile 追加命令"。
  - `tc5`（512×128，2 tiles，lossless）左右两半均 99.00；`tc4`（1024×128，4 tiles）99.00；`tc30`（640×360，2 tiles，30 帧）30/30 帧 99.00；`vp9-kf`（640×360，2 tiles）99.00。
- **tile rows 也已逐位一致**（§8.1、§9.3）：关键是写 `NUM_TILE_COLS_8K`(242)/`NUM_TILE_ROWS_8K`(244) 这一对，写 legacy 的 241/243 才会挂死；防护代码已在 ad087f5 中删除。
- 逐位一致的样片清单（全部 Y/U/V PSNR 99.00）：w6(50 帧)、cb/blkramp/ramp、kf1t、kf2c/tc4/tc5/tc30(2 tile 列)、trb/tr30(2 tile 行)、tr0testsrc(30 帧)、eB/eF/eI、pan(30 帧带 GOLDEN/ALTREF)、tr_na、1080p `hd.webm`。
- **适用边界（重要）**：以上结论只覆盖 **Profile0 / 8bit 4:2:0 / limited range / 不启用
  segmentation / 编码器为 libvpx（frame_parallel_decoding_mode=1）** 的码流。
  一旦开启分段（常见的 AQ 编码）、全范围色彩、10bit、show_existing_frame 包、>16 个 tile
  等，目前会解错或未验证——完整清单见 **§10**。

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

### 6.2 tile rows：已解决（本节记录的是探索过程，结论见 §8.1 与 §9.3）

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
- tile rows：✅ 2 tile 行逐位一致（`trb` 2 帧、`tr30` 30 帧；不需要任何环境变量，见 §8.1/§9.3）
- 单 tile 无 `job ... timed out`（`dmesg` 清空后跑解码无内核报错）✅；tile rows 会超时挂死（已防护，见第 5 节）
## 8. 2026-11-05 进展（未提交工作树）

### 8.1 tile rows：硬件其实支持，关键是“8K 那对”寄存器

第二轮开机期厂商取证（`jmgpu` 内核 + 厂商 UMD，抓 `SetDecRegister`）得到关键差异：

| 寄存器 | 2 tile 列（kf2c） | 2 tile 行（trb） |
|---|---|---|
| 242 `NUM_TILE_COLS_8K` | **2** | **1** |
| 244 `NUM_TILE_ROWS_8K` | **1** | **2** |
| 1049/1057 `VERT_FILT/BSD` | 非 0 | **0 / 0** |
| 82 `STRM_START_BIT` | 80 | 72 |
| 136 `STREAM_LEN` | 17497 | 49 |

厂商**只写 242/244，从不写 241/243**。把驱动的 tile 计数改到 8K 字段后：

- tile rows 的**关键帧**逐位一致（`trb` 640×256 lossless，行 0/1 都正确）；
- 之前“一进第二 tile 行就挂死”的现象消失；此前挂死是 241/243 落到了别的位域。

结论：`JMDEC_NUM_TILE_COLS_8K`/`NUM_TILE_ROWS_8K` 才是 ASIC 真正读的字段，241/243 不要写。
tile 行帧的 **inter 帧**仍会随复杂内容漂移，但那是 8.3 的通用 inter 问题，不是 tile 特有。

### 8.2 inter 帧头解析：用 VA 给的切分点兜底

- VP9 的 inter 帧压缩头里**没有** 16-bit 的 first-partition-size 字段，原解析器按关键帧布局读，
  读到的是 tile 数据，于是 `part=32768/32794` 之类乱值 → `header parse failed`。
- `mwv207_vp9_parse_frame()` 新增 `hint_frame_header_size` / `hint_first_partition_size` 两个参数，
  解析完未压缩头后，若 VA 的 `frame_header_length_in_bytes` / `first_partition_size` 合理则直接采用。
- `setup_frame_size_with_refs()` 在参考尺寸命中时，只在 `pic_param_passed_*` 非 0 时覆盖，
  否则保留调用方预置的尺寸；解析前先把 `sh->width/height` 置为 VA 给的帧尺寸。
  否则 tile 行列位（依赖宽高）会少读 2 bit，first partition size 随之错位。
- 结果：`w6`（inter 帧 16 字节、part=3）等所有流解析正常。

### 8.3 inter 帧漂移的两个真因（已修，仍有残留）

1. **loop filter delta 未跨帧继承**（已修）。VP9 的 `ref_deltas/mode_deltas` 只在 header
   要求更新时改写，其余帧沿用上一帧（libvpx 存在 `cm->lf` 里）。我方每帧 `memset(sh)`，
   于是帧 1 起 delta 变成 0，滤波只差边缘十几个像素——正好是“帧 1 差 18 像素”。
   修法：`st->lf_ref_delta[]`/`lf_mode_delta[]` 持久化，解析前后 memcpy。修后
   `tr0testsrc`/`tr30` 帧 1 都变成 99.00。
2. **时间 MV 预测（use_prev_frame_mvs）**（已修，见 8.4）。

修完这两项后（640×360 testsrc2，30 帧，单/多 tile 均同）：

| 流 | 帧 0 | 帧 1 | 帧 2 | 帧 3 | 帧 4 |
|---|---|---|---|---|---|
| `tr0testsrc` | 99.00 | 99.00 | 31.42 | 30.34 | 30.11 |
| `tr30`（2 tile rows） | 99.00 | 99.00 | 47.53 | 40.72 | 38.37 |
| `sq640`（方块平移） | 99.00 | 99.00 | 99.00 | 99.00 | 99.00 |
| `trb`（2 tile rows，lossless） | 99.00 | 99.00 | | | |

### 8.4 ASIC 的“上一帧 MV 图像”机制

依据 libvpx `vp9_decodeframe.c:3000`：`use_prev_frame_mvs = !error_resilient &&
width==last_width && height==last_height && !last_intra_only && last_show_frame &&
last_frame_type != KEY_FRAME`；`vp9_mvref_common.c:25-77` 用 `prev_frame->mvs`（8×8 网格）当候选。

厂商寄存器语义（静态反汇编 + 开机 trace 交叉验证）：

- 每帧把 MV 写到 `DEC_OUT_DBASE` 指向的独立区域（**不是** Y 平面附近）；
- 下一帧通过 `REFER0/1/4/5_DBASE` 读回**上一帧**那块区域；
- `WRITE_MVS_E`(57) 开写、`TEMPOR_MVP_E`(88) 开读；实测**两个都要置 1** 才生效
  （只开一个时 `sq` 6 帧仍从帧 2 崩）。

实现：`st->mv_offset` 起两块 `mv_size = align4k(w*h*3/2+64)` 双缓冲，`st->mv_cur` 在
`submit_done()` 里 `vp9_refresh_slots()` 之后翻转；inter 帧写 `WRITE_MVS_E=1` +
`DIR_MV_BASE`/`DEC_OUT_DBASE` = 当前块，`REFERx_DBASE` = 上一块，`TEMPOR_MVP_E` 按上式算。

MV 图像实测格式（`MWV207_VP9_MVDUMP` 抓 HW 写出的区域，与 libvpx `cm->cur_frame->mvs` 对照）：

- 16 B/记录，**按 64×64 superblock 分组**：`idx = ((sb_y*sw + sb_x) * 64) + iy*8 + ix`
  （`sw = ceil(w/64)`，ix/iy 是 SB 内 8×8 索引）；
- 记录：`byte6 = ref0`、`byte7 = ref1`（无第二参考时为 0，libvpx 是 -1）、
  `offset8: u16be 列`、`offset10: u16be 行`、`offset12..15` 第二组 (列,行)；单位 1/8 像素；
- 关键帧帧 0 与 libvpx **3600/3600 逐条一致**；帧 1 起有约 90/3600 的 ref 差、
  394/3600 的 MV 差（4×4 分区块 ASIC 只存整块 MV 等），但 ASIC 读写自洽，
  简单运动流的输出仍逐位一致。

### 8.5 多 tile 无需重拷（2026-11-05 晚）

早先为了迁就 “每 tile 前 4 字节大端长度” 的假设，多 tile 帧会把整个 payload 重打到
scratch 里再加长度前缀。改用 8K tile 计数寄存器（242/244）后这个重拷**反而是错的**：
ASIC 按 tile 表逐块回到**原始码流**取数据。现在默认直接指向原始码流（`trb`、`tc5`、
`kf2c` 等 8 条多 tile 流全部 99.00，不再需要任何环境变量）；旧的拷贝路径保留在
`MWV207_VP9_COPY=1` 后面仅供实验。

### 8.5.1 固定变换尺寸（fixed tx-mode）关键帧失步 —— 已修复

- 现象：`vpxenc --rt`、`ffmpeg -deadline realtime` 或 `VX_TX_MODE=1/2/3` 产出的固定 tx-mode
  码流，复杂内容（testsrc2）的关键帧会在中途（约 SB 3）失步，输出 ~15 dB；平滑渐变则正常。
- 根因：**符号计数器缓冲区越界**。厂商布局只给计数器留 13264 B（`0x33d0`），紧跟其后就是我们
  的 tile 尺寸表；系数密集的帧里熵引擎写的计数超过 13264 B，直接踩坏 tile 表，ASIC 随后按
  坏表找 tile → 半帧后熵解码失步。select-mode 流因为每帧 tile 表用得少/写得早，之前侥幸没暴露。
- 修复：`VP9_COUNTER_SIZE` 从 `0x33d0` 放大到 `0x20000`（128 KB），tile 表、段表、滤波区、
  流拷贝区依次后移。
- 验证：`ffrt.ivf`（ffmpeg realtime、tx=0）关键帧 99.00；`txm1/txm2/txm3`（8x8/16x16/32x32-only）
  关键帧与第 1 帧均 99.00；`w6`/`trb` 等原有流不受影响。
- 仍存在：固定 tx-mode 的**第 2 个 inter 帧**起仍随复杂内容下降（29-48 dB），属于下面 8.5.2
  的时间 MV 语义问题，与 tx-mode 无关。

### 8.5.2 MV 图像记录格式（2026-11-05 晚补测）

用 `MWV207_VP9_MVDUMP` 与 libvpx `VX_MVDUMP` 逐记录对比（`/tmp/sq640.ivf`、`/tmp/pan.ivf`）：

- 每 64×64 superblock 64 条记录，16 B/条：`idx = ((sb_y*sw + sb_x) * 64) + iy*8 + ix`；
- 记录内：`byte0..5` 保留、`byte6 = ref0`、`byte7 = ref1`、`offset8` 起两对 **大端 int16**，
  顺序是 **(col, row)** 而不是 (row, col)；`ref` 就是 VP9 的 `ref_frame` 值
  （0=INTRA、1=LAST、2=GOLDEN、3=ALTREF），和 libvpx 的 `MV_REF` 一致，不需要换算：
  `pan` 帧 2 的 `byte6` 分布 (0:118, 1:2658, 2:824) 对上 libvpx 的 (0:118, 1:2654, 2:828)。
- 简单流（方块平移、`sq640`）逐记录一致（3600/3600）。
- **关键定位**：复杂流崩坏的块全部是 **GOLDEN 参考块**。`pan` 帧 2 的 828 个 GOLDEN 块
  平均误差 7.18（最大 99.3）、118 个 intra 块 5.12（被相邻坏块带坏），而 2654 个 LAST 块
  平均误差 **0.0**（最大 1.7）。把编码器限成只用 LAST（libvpx 探针 `VX_REFS=1`，
  `cpi->ref_frame_flags &= 1`）重编同一内容后，6 帧全部逐位一致。
- GOLDEN 块在 MV 图像里的记录仍与 libvpx 不一致（169/828 块，例：`(0,3)` 16×8 NEWMV
  写 `(col=32,row=0)`，libvpx 是 `(col=64,row=0)`），LAST 块也有 342/2654 被带偏，
  即**硬件的 MV 依赖链在出现 GOLDEN 后用错了预测值**。已排除：`ref_frame_sign_bias`
  的索引（我们原来少加 1，已按规范改成 `[k+1]`，对输出无影响）、把 `REFER1/2/3`
  别名到同一内容、`AREF` 缩放、`REFERx_*BASE`、双缓冲寄存器 `805`。
- **残差丢失是主要签名**：把 `pan` 帧 2 的 GOLDEN 块（16×16、整数 MV，共 721 块）按
  "硬件输出 vs 运动补偿预测（同一 MV、参考=上一帧内容）"分类：184 块硬件输出**正好等于
  预测值**（残差整块丢失，误差>3 像素）、48 块部分错、489 块正常。即硬件对这些块解出的
  系数全为 0/skip，而规范要求解码残差。这个签名与"参考槽选错"不同（选错槽会整块错，
  不会恰好等于同 MV 预测值），更像是 **ref != LAST 时块级语法（skip/eob 或系数上下文）
  的解码路径不同**。
- 已确认 `active_ref_idx` 映射与 libvpx 一致（`ref_frame_map` 恒等 + 刷新时按 bit 直接
  写槽位），`pan` 帧 2 的 bitstream 参考索引就是 0/0/0（三参考同一槽），所以两侧参考
  内容相同，差异只能来自块级语法解释。
- 下一步方向：(a) 造"GOLDEN 内容与 LAST 不同"的码流（需编码器显式刷新 GOLDEN 槽）确认
  是残差丢失还是参考选错；(b) 对比 ref=LAST 与 ref=GOLDEN 块在硬件寄存器/系数上下文上
  的差异（`COMP_PRED_*`、`REFERx_VALID`、`single_ref` 概率下标）。

### 8.5.3 剩余问题（历史快照，已被 §10 取代）

> 本节是 2026-11-05 的记录。其中"复杂内容第 2 个 inter 帧起误差"已在 §9.1/§9.2 修完
> （`ref_frame_map` 清零、dword38 共享）；loop filter 也在全部样片逐位一致后确认无误。
> **当前仍开放的问题以 §10 为准。**

- 复杂内容的**第 2 个 inter 帧**起仍有明显误差（31-47 dB）：MV 图像与 libvpx 语义不完全一致，
  且 ASIC 对时间候选的使用与规范有出入。候选方向：
  (a) 4×4 分区块的 MV 存法（整块 MV vs 子块 MV），(b) `ref_frame` 编码（HW 用 1..3，
  libvpx 用 0..3/INTRA=0 的差异），(c) `TEMPOR_MVP_E` 是否还依赖 “上一帧也是 inter 且尺寸相同”
  之外的 `ref_frame_map` 状态，(d) 参考缩放（`REFERx_HOR/VER_SCALE`）在时间候选下的作用。
- loop filter 细节：`FILTERING_DIS` 与 `FILT_LEVEL` 的交互还没完全摸清
  （`MWV207_VP9_NOLF=1` 时硬件的输出与 libvpx `VX_NOLF=1` 仍有差距，需再对一次）。

### 8.6 调试开关（这些历史开关已在 9.7 中全部删除）

调试期用过的 `MWV207_VP9_*` 环境变量（`DBG`/`TILEROW`/`COPY`/`CLEAR`/`REGSET`/`REGD`/
`CDUMP`/`FCDUMP`/`MVDUMP`/`NOLF`/`TMODE`/`SLEN`/`SLENADD`/`SB`/`SOFF`/`NOREF`/`NOHINT`/
`PROBBUMP`/`ZCTR` 等）在 2026-11-06 已整体移除，当前驱动不需要任何环境变量。

### 8.7 测试配方（重启会清空 /tmp）

```
ninja -C ref/oh-mesa/User_source_code/mesa-24.1.2/build-full \
      src/gallium/targets/va/libmwv207_drv_video.so
cp ref/oh-mesa/.../build-full/src/gallium/targets/va/libmwv207_drv_video.so \
   /tmp/gd3/mwv207_drv_video.so      # 注意：文件名没有 lib 前缀！
LIBVA_DRIVERS_PATH=/tmp/gd3 LIBVA_DRIVER_NAME=mwv207 ffmpeg ...
```

## 9. 2026-11-06 收尾：剩余三个根因与最终状态

### 9.1 GOLDEN/ALTREF：`ref_frame_map` 被每帧清零

`set_regs` 开头 `memset(sh, 0, sizeof(*sh))` 会把 `ref_frame_map[]` 一起清成 0，
于是 `active_ref_idx` 里的 raw 0/1/2 全部映射到 slot 0 —— GOLDEN/ALTREF 块实际拿
LAST 的内容做预测。修复：memset 之后立刻填恒等映射（我们按 `refresh_frame_flags`
把每帧写进它自己的槽，槽号即帧号）。

判据：`VP9REFIDX raw=%d mapped=%d` 打印（临时调试）显示 raw=1/2 而 mapped=0。
修好后 `pan` 从"帧 0-1 逐位一致"变成"帧 0-10 逐位一致"。

### 9.2 ALTREF 预测恒为 clear 值：`REFER_LTERM_E` 与 AREF 缩放共用一个 dword

`REFER_LTERM_E` 与 `AREF_HOR/VER_SCALE` 在 `hw_dec_reg_spec[]` 里映射到**同一个 hw
dword 38**（LTERM 是整 32 位置位，缩放是 16 位两个字段）。原来的顺序是先写参考寄存器
（含 AREF 缩放 16384）再写 `REFER_LTERM_E=0`，后者的位域写会把 AREF 缩放清零，导致
所有 ALTREF 块的预测恒为 clear 值 0x11（画面里表现为一块块 17 的灰色），而 LAST/GOLDEN
不受影响。

修复：把 `JMDEC_NO_FWD_REF_E` / `JMDEC_REFER_VALID_E` / `JMDEC_REFER_LTERM_E` 三连写
移到 `vp9_write_ref_regs()` **之前**，让缩放值最后落盘。修好后 `pan` 30/30 帧逐位一致，
tr30/tc30 的隐藏 alt-ref 帧也正常。

教训：写寄存器前先查 `hw_dec_reg_spec[]` 的 `(hw_dword, width, shift)`，同一 dword 的
字段互相踩是静默的。

### 9.3 时间 MV 图像：16 字节/8x8 块、按 64x64 超级块排列

- 记录格式（大端）：`[0..5]=0`、`[6]=ref_frame[0]`、`[7]=ref_frame[1]`、
  `[8..9]=mv[0].col`、`[10..11]=mv[0].row`、`[12..13]=mv[1].col`、`[14..15]=mv[1].row`，
  都是 1/8 像素的 i16；`ref_frame` 0=INTRA、1=LAST、2=GOLDEN、3=ALTREF。
- 索引：`((sb_y*sbw + sb_x)*64) + iy*8 + ix`（`sbw = ceil(w/64)`），即每帧只有
  `sbw*sbh*1024` 字节会被写。
- VP9 的时间 MV 候选恒为"上一解码帧"的 MV 图像（`cm->prev_frame->mvs`），ASIC 通过
  **每个参考的 `REFERx_DBASE`** 去读，所以被本帧引用的每个参考槽都必须持有上一帧的
  MV 图像。实现上不必每帧镜像 16 份：在 `vp9_write_ref_regs()` 里（此时 slot 0 还是
  上一帧的 MV）给三个 active 参考各拷一份 `sbw*sbh*1024` 字节即可。

### 9.4 性能

scratch 是 STAGING（非缓存）内存，memcpy 只有 ~60MB/s：曾按整幅 `luma*3/2` 每帧镜像
16 份，640×360 每帧 95ms、1080p 0.7s/帧。改成"只拷 `sbw*sbh*1024` 字节 + 只拷 3 个
active 参考"后：640×360 30 帧 0.57s（19ms/帧）、1080p 20 帧 1.8s（90ms/帧）。

### 9.5 最终状态与验证

- 提交：`ad087f5`（GOLDEN/ALTREF + 时间 MV + 去掉 tile-row 防护）、`2a2c663`（MV 拷贝按需）。
- 回归脚本 `/tmp/reg2.sh`（遍历 `/home/admin/vp9-assets/*.webm`）、`/tmp/reg.sh`
  （w6/kf2c/trb/tr30/tc30/tr0testsrc/pan/tr_na）：全部 `bad=0`。
- 交叉验证：H.264（libx264 crf23）与 HEVC（libx265 crf28）用同一驱动解码也逐位一致，
  替换系统驱动是安全的。
- 1080p 样片 `/tmp/hd.webm`（1920×1080、20 帧、realtime crf33）逐位一致。

### 9.6（原“仍未清理的调试开关”，已被 9.7 取代）

### 9.7 调试开关清理（2026-11-06）

`mwv207_dec_vp9.c` 与 `mwv207_decoder_vp9_slice_header.c` 里所有 `MWV207_VP9_*` /
`MWV207_PP_*` 环境变量、`vp9_trace()` / `vp9_dbg_enabled()`、以及全部 `fprintf(stderr)`
调试打印都已删除（净减 444 行，编译 0 warning）。驱动现在不需要任何环境变量。

等价性验证：清理前后的驱动在 17 个码流（14 个 `vp9-assets` 样片、`pan.webm`、
1080p `hd.webm`、`tr_na.ivf`）上输出逐字节相同；对软件参考的 Y PSNR 全部 99.00；
H.264/HEVC 解码与桌面渲染不受影响。

## 10. 尚未解决的问题清单（2026-11-06）

本节是接手后要做的全部已知工作。§8.5.3 是历史快照，**以本节为准**。
排序原则：会不会让用户直接看到错误画面 → 兼容性 → 性能。

| 编号 | 问题 | 现状 | 优先级 |
|---|---|---|---|
| A1 | 分段编码（segmentation）不支持 | 实测开 AQ 的流 12-15 dB | **P0** |
| A2 | `show_existing_frame` 包未处理 | 会走普通解码路径 | P1 |
| A4 | 全范围色彩（`color_range=1`）未传 | 恒按 limited 处理 | P1 |
| B1 | tile 数 > 16 静默截断 | 4K 多 tile 会缺内容 | P1 |
| A3 | `frame_parallel_decoding_mode=0` 的概率自适应缺失 | 未实现，现有样片恰好都是 1 | P2 |
| B2 | 中途改分辨率 / 参考尺寸与当前不同 | 未验证 | P2 |
| A5 | VP9 10bit（Profile 2）、4:2:2/4:4:4、alpha | 未实现、未广播 | P3 |
| B3 | intra-only（非关键帧） | 未验证 | P3 |
| B4 | 损坏/截断码流鲁棒性 | 未验证 | P3 |
| B5 | 极小/非 16 对齐尺寸、4K | 未测 | P2 |
| B6 | 多实例并发 | 未测 | P4 |

### 10.1 已验证范围（边界）

| 维度 | 已验证 | 说明 |
|---|---|---|
| Profile | VP9 Profile0（8bit 4:2:0） | `vainfo` 也只广播 Profile0 |
| 帧类型 | key / inter(P) / 隐藏 alt-ref(show=0) / show=1 | 30 帧长流验证 |
| 参考 | LAST / GOLDEN / ALTREF / 时间 MV | §9.1–9.3 |
| tile | 单 tile / 多 tile-columns / 多 tile-rows / 2×2 混合 | 最多 4 个 tile |
| 编码器 | ffmpeg(ffmpeg 内建 VP9 编码器与 libvpx-vp9)、vpxenc | realtime 与 good/cpu-used 0–8 |
| 色彩 | limited range（`color_range=0`） | 全范围见 A4 |
| 分段 | **关闭** | 开 AQ 即错，见 A1 |
| 分辨率 | 128×128 – 1920×1080（最长 120 帧） | 4K 见 B5 |

验收脚本 `/tmp/full_test.sh`：20 个码流（14 个 `vp9-assets` 样片 + `pan`/`hd`/`m720`/
`m_er`/`m_noise`/`m_mandel`）逐流 `cmp`，全 `OK`；另跑 1080p60 120 帧长片逐字节一致，
`dmesg` 无新增 "timed out"。

### 10.2 功能性缺口

#### A1（P0）分段编码 segmentation 不支持

- 现象：任何启用 8 段分段的码流都解错。实测（`/tmp/aq{1,2,3}.webm`，ffmpeg
  `-aq-mode 1/2/3` —— libvpx 的 AQ 就是靠 segmentation 实现）：
  `aq1` 30/30 帧错（Y/U/V ≈ 12.85/12.84/12.83 dB）、`aq2` 30/30 帧错（12.21/12.14/12.08）、
  `aq3` 帧 0 逐位一致、其后 29 帧错（14.93/15.26）。
- 原因：`set_regs` 恒写 `JMDEC_SEGMENT_E=0`、`JMDEC_SEGMENT_UPD_E=0`
  （`src/mwv207-umd/drivers/mwv207/video-codec/mwv207/mwv207_dec_vp9.c`），把分段全关掉。
  解析器其实已经解出 `segment_feature_mode`（abs/delta）与 `segment_feature_data[3][8]`
  （每段 REFPIC/SKIP/FILT/QUANT 四个特性，见
  `common/mwv207_decoder_vp9_slice_header.c` 的 `vp9_decode_segment_header()`），只是没
  程序化到硬件。
- 硬件侧已有寄存器（`mwv207_video_dec_8170enum.h`）：`SEGMENT_TEMP_UPD_E`、
  `SEGMENT_UPD_E`、`SEGMENT_E`，以及每段的 `REFPIC_SEG0..7`、`SKIP_SEG0..7`、
  `FILT_LEVEL_SEG0..7`、`QUANT_SEG0..7`；厂商 UMD 的
  `jmgpu_decoder_vp9_set_segment_register` 就是写这一组（§6.3）。
- 难点：
  1. `QUANT_SEG0`(315) 现在被当作 base_qindex 写死（§5 量化模型）。开分段后
     abs 模式每段是绝对 qindex、delta 模式是 base+delta，`INIT_QP`(288) 与 294 的位域
     重叠（§5）要重新梳理。
  2. ASIC 还有 `SEGMENT_READ_BASE` / `SEGMENT_WRITE_BASE` 两个 scratch 区（驱动已按
     `2 × align4k(w*h)` 分配），推测是逐块 segment id 的读写缓冲，时序未验证。
  3. `FILT_LEVEL_SEG*`/`SKIP_SEG*` 与普通 LF/skip 的交互（`segment_feature_mode`
     abs 时覆盖 base）需要对着 libvpx 的 `vp9/decoder/vp9_detokenize.c`、
     `vp9_decodeframe.c` 逐项对齐。
- 影响：真实世界 VP9（YouTube、Chrome 系编码器、绝大多数转码流水线）默认开 AQ，
  **不解决就无法作为桌面/浏览器解码器使用**。建议作为下一个任务。

#### A2（P1）show_existing_frame 未处理

- `vp9_decode_uncompressed_header()` 解析了 `show_existing_frame` 与
  `show_existing_frame_index`，但 `set_regs` 只在计算 `prev_was_inter` 时读了一下，
  **没有改变解码流程**。
- 后果：这类包（规范上只需把某个参考槽重新显示，负载常常只有 1 字节）会走普通解码
  路径，`DEC_E=1` 后硬件没有有效码流可解，输出是 clear 值或上一帧，但 VA 时间戳照常推进。
- 建议：`set_regs` 开头判断 `sh->show_existing_frame`，跳过解码并直接输出
  `ref_store[show_existing_frame_index]`（或复制上一帧）。需要先造含该类包的样片
  （vpxenc `--auto-alt-ref=1 --lag-in-frames=N` 通常会产生）。

#### A4（P1）全范围色彩未传下去

- 解析器解出 `sh->color_range`，但 VP9 路径没有传给 PP：
  `mwv207_dec_pp_params_init()`（`mwv207_video_dec.c`）设置 `enabled/planar/tiled/cr_first/
  shaper/align`，**不设 `video_range`**；调用处是
  `mwv207_pp_unit_config ppu_ext_cfg[DEC_MAX_PPU_COUNT] = {0};`
  ⇒ `video_range` 恒为 0（limited）。而 `mwv207_dec_ppu.c` 只在
  `ppu_ext_cfg->video_range` 非 0 时做 full-range 转换。
- 后果：`-color_range pc` 的流（HDR 风格素材、部分剪辑软件导出）会被当 limited 处理，
  黑/白电平错（16..235 vs 0..255），表现为发灰或过曝。
- 建议：给 `mwv207_dec_pp_params_init()` 增加 range 参数（或读
  `dec->dec_param.video_range`），VP9 调用处传 `sh->color_range`；H.264/HEVC 路径同样
  有这个字段，可一并检查。

#### B1（P1）tile 数 > 16 被静默截断

- `mwv207_dec_vp9.c` 的 tile 表构造：`uint16_t cinfo[16], rinfo[16]; if (n > 16) n = 16;`，
  且 `st->tile_off[16]` / `st->tile_len[16]`；`n = ncol * nrow`。
- 4K 常见配置 `-tile-columns 3`（8 列）或 `8 列 × 2 行 = 16` 刚好卡在边界；再高
  （如 8×4=32）只解前 16 个 tile，其余 tile 不写，画面出现整块旧内容。
- 建议：上限提到 32（甚至 64），并把截断改成显式拒绝（`DEC_E=0` + 日志），杜绝静默错误。

#### A3（P2）frame_parallel_decoding_mode=0 的概率自适应缺失

- 规范 + libvpx：`if (!error_resilient && !frame_parallel_decoding_mode)` 时，解码器必须用
  **本帧符号计数**更新下一帧概率（`vp9/decoder/vp9_decodeframe.c` 末尾
  `vp9_adapt_coef_probs` / `vp9_adapt_mode_probs` / `vp9_adapt_mv_probs`）。
- 我们的驱动从不读 ASIC 计数区（`VP9_COUNTER_OFFSET 0xea0`；ASIC 确实会写，640×360
  一帧后该区有 856 个非零 `u32`、总和 11163），也不做 CPU 侧自适应。
- 现有样片没暴露问题，是因为 libvpx/ffmpeg 产出的流 `frame_parallel_decoding_mode=1`
  （逐流 dump 确认 `par=1`）。其它编码器（部分硬件编码器、`--frame-parallel=0`）的流会漂移。
- 实现路径：读计数区 → 按 libvpx 的 `vp9_adapt_*` 算法（或厂商 UMD 的
  `AdaptProb`/`UpdateCoefProbs`/`Vp9StoreAdaptProbs`）更新 scratch 概率表。

#### B2（P2）中途改分辨率 / 参考尺寸与当前不同

- 参考缩放恒写 `1<<14`（§5），即假定参考与当前同尺寸。VP9 允许参考帧与当前帧尺寸不同
  （按比例缩放取参考）以及流中途改分辨率（`render_and_frame_size_different`）。
- `sh->width/height` 每帧刷新、参考 w/h 也按当帧写，所以"改尺寸后续解"大概率能用，但
  未测；且 scratch 只增不减、staging/参考槽跨尺寸内容的行为未验证。
- 建议：造"前 15 帧 640×360、后 15 帧 320×180"的流，以及用 `-resize`/`-force_key_frames`
  切换尺寸的样片。

#### A5（P3）10bit（Profile 2）、4:2:2/4:4:4（Profile 1/3）、alpha

- `vainfo` 只广播 `VAProfileVP9Profile0`（H.264High10/HEVCMain10 有 10bit 路径），
  所以 10bit VP9 目前根本不会进这个驱动。
- 要支持 Profile 2 需要：广播 profile、P010 输出（`mwv207_dec_pp_params_init()` 已有 10bit
  分支且 H.264/HEVC 走通）、解码侧 `BIT_DEPTH_Y/C_MINUS8`(201/202)、反量化/扫描表按
  bit_depth 重算、`out_cut_8bits` 之外的真 10bit 输出路径。
- Profile 1（4:2:2）/ Profile 3（4:4:4）需要色彩格式寄存器（chroma 采样、SAS/BSD 表），
  完全未探索。
- VP9 alpha（webm alpha）需要第二平面与 alpha 混合寄存器，未做。

#### B3（P3）intra-only（非关键帧）未单独验证

- `IDR_PIC_E` 已按 `key_frame || intra_only` 写，但手头没有"intra_only=1 且非关键帧"的
  样片（`eI` 全关键帧流）。这类帧多见于 seek/入场，建议补样例（vpxenc 用
  `--kf-max-dist` + 强制 intra-only 的构造，或直接改 IVF 的 frame_type）。
  
#### B4（P3）损坏/截断码流鲁棒性

- 解析失败时统一 `DEC_E=0`、输出保留上一帧，没有与 libvpx 的错误隐藏行为对比，
  `vaEndPicture` 仍返回成功。播放器/浏览器遇到坏包可能"冻结"而不是黑帧或丢帧。

#### B5（P2）极小尺寸、非 16 对齐、4K

- 最小测过 128×128；VP9 最小 64×64、宽度可为偶数非 16 对齐（如 176×144）。`out_w_y`
  的 align 逻辑与 PP 的 `DEC_ALIGN_64B` 组合需要单独验证。
- 4K（3840×2160）没跑过，会同时暴露 B1（tile 上限）、scratch 大小与性能问题。

#### B6（P4）多实例并发

- 每个 `mwv207_video_dec` 有自己的 CS/reg_buf，但 semaphore、job 提交路径的并发行为
  没测过（两个 VA 上下文同时解 VP9，或一边 VP9 一边 H.264）。

### 10.3 性能/架构（不算错误，但值得做）

- **关键帧 16 槽参考拷贝**：`refresh_frame_flags=0xff` 时 `vp9_refresh_slots()` 会
  memcpy 16 份 `luma*3/2`（1080p 每份 3.1MB、合计 ≈50MB；staging 内存实测 ~60MB/s
  ⇒ 按该带宽估算单个关键帧的开销约 0.8s）。1080p60 120 帧整片 11.4s（含 hwdownload 落盘）里，关键帧那
  几帧明显比 P 帧慢，主要就是这笔拷贝。
- **单线程 + 同步等待**：`mwv207_video_decoder.c` 用 `mwv207_video_cs_submit_wait()`，
  1080p ≈90ms/帧；后续可做异步 fence + 多实例流水。
- **PP 每帧全量重配**：`mwv207_pp_set_regs()` 每帧写 ~40 个寄存器 × PP unit 数，
  帧间参数不变时可跳过。
- **MV 图像拷贝**：已从"每帧 16×整幅"优化到"每帧 3×`sbw*sbh*1024`"（§9.4），
  1080p ≈510KB×3；若以后做 GPU blit 可进一步省。

### 10.4 复现素材与工具（/tmp 重启会清空）

- 验收：`/tmp/full_test.sh`（20 流逐流 `cmp`）。
- A1 复现：`/tmp/aq{1,2,3}.webm` + `aq*_sw.yuv`（重建命令见 §8.7 的 testsrc2 素材配方，
  加 `-aq-mode N`）。
- libvpx 探针：`/tmp/libvpx-src`（1.12.0）+ `/home/admin/vp9-tools/libvpx-probes.py`
  （`VX_MIDUMP`；编解码需要 `--enable-examples`，`MODE_INFO` 字段是 `sb_type`）。
- 厂商证据：`/home/admin/vendor-boot.log`（寄存器真值）、`/tmp/jm.asm`（UMD 反汇编）、
  `/tmp/jmnames.txt`（枚举 id → 名字）。
- 历史样片：`/home/admin/vp9-assets/`（14 个 webm + `_sw.yuv`）。
