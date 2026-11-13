# JPEG Baseline 硬件解码：厂商寄存器序列取证（2026-11-13）

## 1. 为什么做这件事

`docs/UNRESOLVED.md` 的 JPEG 一条此前判为"本 SKU 不可实现"（我方实现总是
`DEC_ERROR_INT(0x10102)` + 2 s TDR，输出面零字节）。但 2026-11-13 在厂商栈上实测：

```
ffmpeg -hwaccel vaapi -vaapi_device /dev/dri/renderD128 -i t1080.jpg \
       -f rawvideo -pix_fmt nv12 /tmp/vjpg_hw.yuv
```
成功输出 3,110,400 B，与软解仅差 limited→full range 映射
（`(76-16)*255/219≈70`、`(41-16)*255/219≈29` 与实测一致）。
⇒ **硬件能做 JPEG baseline 解码**，我方失败是编程细节。于是去取厂商到底写了什么。

## 2. 取证方法

`vainfo` 显示厂商 VA-API 是 `Jm jmgpu driver … Encoder JMENC V6.2 / Decoder JMDEC 1.1.2`，
解码在闭源 `jmgpu_drv_video.so` + 厂商 KMD `jmgpu.ko` 里。

1. 先试 `tools/vendor-capture/vcap.c`（LD_PRELOAD 拦 `ioctl`/`drmCommandWrite*`）：
   只抓到 `req=c0406400 sz=64` 的小结构 —— 厂商把寄存器数组放在指针里
   （`struct core_desc { u32 id; u32 type; u32 *regs; u32 size; u32 reg_id; }`），
   拦截 ioctl 看不到载荷。
2. 改走内核侧：`jmgpu.ko` 未 strip，符号表里有 `dec_write_reg.isra.0`；
   `~/vendor-src/.../jmgpu_remote.c` 的 `DecWriteRegs()` 解析 `core_desc` 后逐字调用它。
   kprobe（**5 个参数**：`a1=jdev, a2=id, a3=type, a4=offset, a5=value`）：

   ```
   sudo mount -t debugfs none /sys/kernel/debug
   echo 'p:dw dec_write_reg.isra.0 a1=$arg1 a2=$arg2 a3=$arg3 a4=$arg4 a5=$arg5' \
        | sudo tee /sys/kernel/debug/tracing/kprobe_events
   echo 1 | sudo tee /sys/kernel/debug/tracing/events/kprobes/dw/enable
   # 在厂商栈上跑一次 JPEG 解码，然后读 trace
   ```
   一次解码产生 **502 条**写入 = 整个 DEC 窗口 0x000–0x7d0 逐字写完。

产物：`tools/vendor-capture/vendor-jpeg-regs.txt`（502 行 `offset value`）、
`tools/vendor-capture/capture-jpeg.sh`（切到厂商栈后一键抓取）。

## 3. 厂商 1920x1080 单帧的非零寄存器（偏移=MMIO 字节）

| 偏移 | 值 | 含义 |
|---|---|---|
| 0x004 | 0x1 | `DEC_E`（最后写） |
| 0x00c | 0x1800c000 | `DEC_MODE=3`、`FILTERING_DIS`(bit14)、**`DEC_OUT_DIS=1`(bit15)** |
| 0x010 | 0x07802200 | `PIC_MB_WIDTH=15`（= (w+127)/128）、`PIC_MB_HEIGHT_P=68`（= (h+15)/16） |
| 0x014 | 0x1a7c | `JPEG_MODE=2`、`STREAM_ALL`、`QTABLES=3`、CB/CR 的 DC+AC VLC 全 1；`STRM_START_BIT=0`、**`HUFFMAN_E=0`** |
| 0x018 | 0x1c724 | `STREAM_LEN=116516`（= ECS 长度，流起点即 ECS） |
| 0x01c | 0x0400003f | `PJPEG_FILDOWN_E`(bit26) + `PJPEG_SE=63`、`SS=0`、`AH=AL=0` |
| 0x040–0x05c | 见文件 | AC1 码长（AC2 全 0） |
| 0x064 | 0x0132201 | DC1 码长（DC2 全 0） |
| 0x0dc | 0x8 | `APF_THRESHOLD=8` |
| 0x0e8 | 0x210 | `DEC_BUSWIDTH=2`、`DEC_MAX_BURST=16` |
| 0x2a4 | 0xce23000 | `STREAM_BASE`（绝对 GPU 地址） |
| 0x2bc | 0xd24000 | `QTABLE_BASE`（544 B 辅助表） |
| 0x408 | 0x1c724 | `STRM_BUFFER_LEN` |
| 0x424 | 0x01004000 | AXI RD/WR OSTD 阈值各 64 |
| 0x4f8/0x4fc | 0x80500000 | `EXT_TIMEOUT_OVERRIDE_E` / `TIMEOUT_OVERRIDE_E` + 0x500000 周期 |
| 0x500 | 0x1 | `PP_OUT_E_U`（不开 tile） |
| 0x518 | 0xcb26000 | **PPU 输出 Y 面地址** |
| 0x520 | 0xcd24000 | **PPU 输出 C 面地址** |
| 0x524 | 0x07800780 | PPU 输出 Y/C stride 都是 1920 |
| 0x52c | 0x3c0021c | **PPU 输入 960x540**（= 宽高各一半） |
| 0x530 | 0x07800438 | PPU 输出 1920x1080 |
| 0x628 | 0x01010000 | `PP0_DUP_HOR_U=1` / `PP0_DUP_VER_U=1` |
| 0x630 | 0x07800438 | `PP0_SCALE1` 输出 1920x1080 |

**最关键的发现**：厂商**完全不写** `0x104`(`DEC_OUT_YBASE`)、`0x108/0x10c`
(`REFER0_BASE`/`JPG_CH_OUT_BASE`)、`0x18c`(`DEC_OUT_CBASE`) —— 全为 0。
即 JPEG 解码不落显存面，输出经内部通路进 PPU；PPU 再按"半尺寸输入 + 2x2 复制"
直接写进应用 surface。这解释了我方"输出面永远是零"：我们一直在给解码器指显存面，
而它根本不往那里写。

## 4. 我方实现与结果

按上表逐项复刻（`mwv207_dec_jpeg.c` 的 `jpeg_program_vendor()`，`MWV207_JPEG_VENDOR`），
含 `DEC_OUT_DIS=1`、`(w+127)/128` 的 MB 宽度、`SS/SE/AH/AL`、PPU 半尺寸输入 + DUP、
超时 override 等。编译、`vainfo` 均正常，帧也能提交。

结果仍是 **`irq state 0x00010102`（`STRM_CORRUPTED`+`DEC_IRQ`+`DEC_ERROR_INT`）+ 2 s TDR、
输出全零**。已排除的变量：

- `DEC_MODE` 3 是对的（换 1/2/4/5/6/7 都是 `0x10102`，3 是唯一给出 `0x00000009`/`0x10102`
  两种状态的模式）；
- 表字节序：`MWV207_JPEG_AUXSWAB` × `MWV207_JPEG_TABSWAP` 4 组全试，无变化；
- 流字节序/起点：`MWV207_JPEG_STRMSWAP` 0/1/2/3/8、`STREAM_ALL=0`、整段 JPEG 上传 + ST
  的 256B 页/32 位交换位偏移公式，全部无变化；
- 寄存器写入确实生效（KMD `peek` 在作业期间读到 `0x0c/0x10/0x14/0x18/0x2a4/0x2bc` 都是我们写的值）。

⇒ 剩下的差异只可能在**厂商送进去的缓冲内容/布局**：ECS 里是否带额外填充、
544 B 辅助表的确切排布（尤其 QP 的 zigzag 方向与 AC/DC 的交换条件），
以及 `STREAM_BASE`/`QTABLE_BASE` 的放置（厂商是显存绝对地址）。
这些从寄存器 trace 看不到，需要在厂商栈上用 kprobe 抓 `copy_from_user`/`jmo_HD_*`
的缓冲载荷（或在 `jmo_HD_WriteEncReg` 之外再挂 `DecWriteRegs` 的 `core->regs` 指针读取）。

## 5. 处置

- 本次不实现 JPEG 解码：`PIPE_VIDEO_PROFILE_JPEG_BASELINE` 声明不启用（保持 client 看不到该 profile），
  避免客户端拿到的全是零帧或 2 s 卡顿。
- **保留取证产物**：`tools/vendor-capture/{vcap.c,capture-jpeg.sh,vendor-jpeg-regs.txt}` —— 502 行
  完整寄存器序列是本项目最有价值的参考资料之一，未来要重开这项时不必再切栈抓一次。
- 被搁置的实现代码放在 `~/jpeg-parked/`（`mwv207_dec_jpeg.{c,h}`），未进仓库。
- 下一步优先级更高的替代项：VP9 `show_existing` 重放帧（决定 PPU 独立启动或 CPU tiled→线性），
  它影响的是已声明功能，用户可见。
