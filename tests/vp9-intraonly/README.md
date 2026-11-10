# VP9 intra-only 隐藏帧（B3）诊断

B3 的目标是验证驱动能否解码 **intra-only 非关键帧**（`show_frame = 0` 且
`intra_only = 1`，Profile 0 时未压缩头不带 color config）。本机 libvpx 编码器产不出
这种帧（`auto-alt-ref` 产生的隐藏帧全是 inter 帧，见下），因此改用 WebM 官方测试
向量。

## 样片

| 文件 | 说明 |
| --- | --- |
| `vp90-2-16-intra-only.webm` | <https://storage.googleapis.com/downloads.webmproject.org/test_data/libvpx/vp90-2-16-intra-only.webm>，175807 B，md5 `70d4afd231a41dd0f0364fec8121fa09`，352×288，Profile 0 8bit |
| `vp90-2-10-show-existing-frame.webm` | 同一批次的对照样片（隐藏帧 + 单字节 `show_existing_frame`），硬件与软件解码逐位一致 |

`ffprobe -show_packets` 可见 7 个包；`ffmpeg -c copy -f ivf` 对这类文件会输出空文件
（VP9 parser 侧限制），所以脚本自带 EBML 解析把包抽出来：

```
117963, 25801, 1, 1, 1, 21400, 10293   （包 2/3/4 是单字节 show_existing）
```

用驱动自带解析器（`common/mwv207_decoder_vp9_slice_header.c` 编译成的小工具）解析
包 0：

```
res=0 used=19 key=0 intra=1 show=0 showex=0 err=0 prof=0 352x288 refresh=0x01
tilecols=0 tilerows=0 seg=0 lossless=0 txmode=3 fpar=0 lf=6 sharp=0 qyac=53
```

即确实是 intra-only 隐藏帧，单 tile、无分段、`frame_parallel_decoding_mode = 0`
（对比：本机 libvpx 编出的关键帧是 `fpar=1`）。

## 结论：硬件解码结果与 libvpx 不一致（待定性的缺陷，UNRESOLVED I-5）

- 软件侧 `-c:v libvpx-vp9` 与 `-c:v vp9` 解码 7 帧逐帧完全相同，可作为参考。
- 硬件侧（r20 载荷，`LIBVA_DRIVER_NAME=mwv207`）也是 7 帧，但 **4 个真正解码的帧
  （0/1/5/6）全部与软件不同**（Y 平面 maxdiff 254/255，mean 19.6–38.9），而 3 个
  单字节 `show_existing_frame`（2/3/4）逐字节一致。
- 最小复现：只留包 0（单帧、无任何参考）分别解码 → 都是 1 帧 152064 B，但**不一致**，
  说明差异发生在 intra-only 帧自身的重建，与参考帧管理无关。
- 目视对比（`evidence/`）：布局、色调、位置都对，但一批 64×64 级区块是乱的 ——
  不是色度/范围变换，也不是帧偏移，更像部分超块的重建/预测读到了错误数据。

### 已排除

- 比对方法：对照样片 `vp90-2-10-show-existing-frame.webm` 硬件与软件 **IDENTICAL**
  （7 帧 2585088 B），说明 `hwdownload` 通路、隐藏帧、`show_existing` 都正常。
- 尺寸因素：本机 libvpx 编的 352×288 **关键帧**硬件与软件 **IDENTICAL**。
- 解析层：`common/mwv207_decoder_vp9_slice_header.c:958-1000` 的 intra_only 分支与
  libvpx `read_uncompressed_header()` 一致（`bit_depth` 仅 profile ≥ 2 时读，
  color config 仅 profile > 0 时读，Profile 0 用默认 BT.601/studio/4:2:0/8bit）。
- 关键帧位流不能直接改写成 intra-only：把真实关键帧的压缩头 + tile 套进 intra-only
  未压缩头后，解析器与 libvpx 都接受，但解码结果与关键帧不同；穷举 header 位数
  （`H_orig∈[130,190)`、tile 起点）都没有出现"等于关键帧"的候选。

### 受控实验：自产 intra-only 样片（`make-intra-only.py`）

既然关键帧的压缩头 + tile 数据与 intra-only 帧是同一套编码工具，就可以把关键帧的
**未压缩头**换成 intra-only 形态，得到内容等价、位流合法的自产样片（改写规则见
脚本头部注释；Profile 0 的 intra-only 头不带 color config，`reset_frame_context`
要取 3，否则首帧会被 libvpx 以 `Uninitialized entropy context` 拒绝）。

```sh
./build-probe.sh                    # 编译解析探针（链接驱动自带的 VP9 头解析器）
ffmpeg -f lavfi -i testsrc2=size=352x288:rate=30 -frames:v 1 \
       -c:v libvpx-vp9 -g 1 -f ivf /tmp/kf.ivf
./make-intra-only.py /tmp/kf.ivf /tmp/io.ivf [--refresh 0x01]
```

用这批自产样片做的对照（硬件 r20 载荷 vs libvpx 软件解码，全部逐位比对）：

| 样片 | 关键参数 | 结果 |
| --- | --- | --- |
| 64×64 testsrc2 | refresh 0xFF / 0x01 / 0x02 / 0x80，rfc=3，fpar=1 | IDENTICAL |
| 352×288 testsrc2 | refresh 0xFF 与 0x01，rfc=3，fpar=1 | IDENTICAL |
| 352×288 testsrc2 CRF 50/55 | qyac 123/145，fpar=1 | IDENTICAL |
| 352×288 testsrc2 `-frame-parallel 0` | fpar=0 | IDENTICAL |

⇒ **intra-only 帧类型本身、部分刷新、高低 QP、`frame_parallel_decoding_mode`
都不是触发条件**：驱动对自产 intra-only 帧的解码是正确的。与官方样片唯一剩下的
差异是它特有的参数组合（`txmode=3`、`qyac=53`、`lf=6`、单帧 118 KB 的真实画面
内容），I-5 只在这类流上出现，范围已收窄到该组合。

### 待查方向

1. ~~`frame_parallel_decoding_mode = 0`~~ —— 已用 `-frame-parallel 0` 的自产样片排除
   （见上表）。剩下的入口是官方样片的 `txmode=3`（本机编码器给 4=SELECT）与
   `lf=6`/大体积 tile 组合。
2. `refresh_frame_flags = 0x01`（关键帧是 0xFF）时 `REFRAME`/`REFSLOT` 寄存器的写法，
   以及 `JMDEC_IDR_PIC_E = 1` 与"intra-only 但刷新部分槽位"的组合。
3. 单帧只有 1 个 tile 时 `JMDEC_TILE_BASE/`tile 长度与 `frame_tag_size + first_partition`
   的边界（包 0 的 `used=19`，需要与 libvpx 的实际 partition 大小对账）。
4. 硬件寄存器回读：以 `dump_state` 的 `current stat/addr/cmdl/cmdh` 对照 `only0` 与
   本机 352×288 关键帧，找出 intra-only 路径下被改写的字段。

## 用法

```sh
./run-vp9-intraonly.sh              # 需要能联网下载样片（第一次）
./run-vp9-intraonly.sh --local DIR  # 用已下载的样片目录
```

脚本会打印每帧的比对结果，并在 `${TMPDIR:-/tmp}/vp9-intraonly/evidence/` 留下
软件/硬件首帧的 PNG，便于目视。当前它是个**诊断工具**而不是通过/失败检查：
在 I-5 修好之前，"解码帧不一致"就是已知状态。

## 本机 libvpx 产不出 intra-only 的证据

`ffmpeg -c:v libvpx-vp9 -b:v 1M -pass 1/2 -lag-in-frames 25 -auto-alt-ref 1` 编
320×240 40 帧，按 IVF 包解析未压缩头：40 个包里只有 4 个隐藏帧（包 1/13/25/32），
且**全部 `intra_only = 0`** —— alt-ref 是 inter 隐藏帧。