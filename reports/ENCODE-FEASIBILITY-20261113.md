# 编码（H.264/HEVC）实现可行性评估 — JM9100 (MWV207) 开源栈

日期：2026-11-13 · 结论：**本 SKU 不可做**（硬件编码块不存在 + 无寄存器文档 + 无可用 HAL）

## 0. 一句话

Mesa VA 前端与 ffmpeg 的 `h264_vaapi`/`hevc_vaapi` 编码通路是通用的，只差**驱动实现**
`create_video_codec(ENCODE)` 与 `encode_bitstream`；但本机是 **lite SKU，编码硬件块
（0x920000 窗口）根本不存在**，即使写出来也没有硬件可跑 —— 因此不是"工程量大"，而是
"目标硬件没有这个功能"。

## 1. 证据链

### 1.1 本机是 lite SKU

- `src/kernel-6.6/mwv207_drv.c:379`：`jdev->lite = jdev_read(jdev, 0x12c0000) == 0x21 ? false : true;`
- 用 KMD 只读口实测：`echo 0x12c0000 > /sys/module/mwv207/parameters/peek` →
  `peek 0x012c0000 = 0x00000000` ⇒ **`jdev->lite = true`**。

### 1.2 lite 模式下 KMD 不建立编码管线

- `src/kernel-6.6/mwv207_pipe_enc.c:251`：`if (jdev->lite) return NULL;`
- `src/kernel-6.6/mwv207_sched.c:400`：`pipe = mwv207_pipe_enc_create(jdev, 0x920000, "mwv207_enc0");`
  → `pipe == NULL` ⇒ `mwv207_sched_create()` 在 `pipe == NULL` 时返回 NULL
  （`mwv207_sched.c:262`）⇒ `jdev->sched_enc[0] == NULL`，用户态拿不到编码引擎。
- 同一 `lite` 判断还影响 2D/3D/DEC/DMA 的位宽与 pending 值（`mwv207_pipe_2d.c:161`、
  `mwv207_pipe_3d.c:573`、`mwv207_pipe_dec.c:245`、`mwv207_pipe_dma.c:637`），
  说明 lite 是硬件裁剪档位，不是软件开关。

### 1.3 编码寄存器窗口在本机不响应（与已知缺席的核对）

用同一个 `peek` 口读各引擎窗口的首字：

| 窗口 | 地址 | 读回 | 判读 |
|---|---|---|---|
| 3D 核 0 | `0x900000` | `0x00040100` | 存在（正在用） |
| DEC 核 0 | `0x930000` | `0x80018000` | 存在（正在用） |
| **ENC** | **`0x920000`** | **`0xffffffff`** | **不响应/不存在** |
| 3D 核 1 | `0x910000` | `0xffffffff` | 不存在（lite 只有一颗 3D） |
| DEC 核 1 | `0x940000` | `0xffffffff` | 不存在 |

`0x920000` 与 `0x910000`/`0x940000` 表现完全一致（全 1），而与在用的 `0x900000`/
`0x930000` 明显不同 ⇒ 编码块未实现。

### 1.4 我们手里没有任何编码寄存器定义

- UMD 侧只有解码寄存器数组：`src/mwv207-umd/drivers/mwv207/video-codec/mwv207/mwv207_dec_register_array_7020.h`
  （1840 条 `JMDEC_*`）与 `mwv207_video_dec_8170table.h`（1840 行位域表）。
- `grep -rln "JMENC\|ENC_REG\|enc_register" src/ ~/vendor-src/usr/include` 只命中
  `src/kernel-6.6/mwv207_pipe_enc.c` 与 `mwv207_pipe_codec_common.h` —— 后者只是
  `JPIPE_ENC_REG_NUM 500` 这种窗口大小常量，不是编码寄存器语义。
- `docs/registers/` 的 `register-db.json` / `REGISTER-MAP.md` 等也没有 ENC 组。

### 1.5 vendor 侧的编码实现是封闭的，而且走它自家的 HAL

- `~/vendor-src/usr/lib/aarch64-linux-gnu/dri/jmgpu_drv_video.so` 的未定义符号里有
  `jmo_HD_GetEncCfg`、`jmo_HD_GetEncCoreNum`、`jmo_HD_GetEncId`、**`jmo_HD_ReadEncReg`**、
  **`jmo_HD_WriteEncReg`**、`jmo_HD_ReserveEnc`、`jmo_HD_WaitEncAnyCore`、`jmo_HD_ReleaseEnc`
  ⇒ 真正的编码器编程在闭源 UMD 里，通过 `libdrm_jmgpu.so.1.0.0` 提供的 HAL 下发。
- vendor KMD 源码（`~/vendor-src/usr/src/mwv207-1.8.0.uos/`）里 `jmgpu_enc.c` 只是
  **引擎管道**：窗口 `#define J9_UNDERCELLARER 0x920000`、`core_array[]` 里
  `{0x920000, 500*4, irq, …}`，加 IRQ 分发与 `slice_idx/core_id` 寻址；
  `J9_HANDLE_J9M_HARBOUROUS`（`_IOW('e', 15, j9_unperforate*)`）就是"写一个编码寄存器"
  （`struct tag_jms_hd_enc_reg { slice_idx, core_id, core_type, offset, val }`）。
  **没有任何编码语义**（该写哪些寄存器、什么顺序、码率控制怎么做，全在闭源 UMD）。
- vendor UMD 的字符串也只暴露**编码器参数**（`Encoded jpeg width. jpgLumWidth`、
  `JPEG CodingMode 0:420 1:422 2:lumaonly`、`hevcencapi.c`、`src/hevc` 等），
  说明"编码能力存在"是家族级描述，具体到本板仍取决于硬件是否带该核。

## 2. 如果硬件真的存在，需要做哪些事（工作量估算）

1. **VA 前端对接**：`mwv207_video_codec.c` 的 `mwv207_create_video_codec()` 需要支持
   `PIPE_VIDEO_ENTRYPOINT_ENCODE`，声明 profile（H.264 Baseline/Main/High、HEVC Main），
   填 `struct pipe_video_codec.encode_bitstream`；Mesa 的
   `src/gallium/frontends/va/picture_h264_enc.c` 已经把
   `VAEncSequenceParameterBufferH264`/`VAEncPictureParameterBufferH264`/slice 参数
   解析好了，可直接用。**约 300–600 行。**
2. **编码器寄存器编程**：SPS/PPS、切片参数、参考帧列表、量化/环路滤波开关、
   码率控制寄存器、bitstream 缓冲地址、完成轮询与 fence。**若没有寄存器文档，
   只能靠 LD_PRELOAD 拦 `jmo_HD_WriteEncReg` 在 vendor 栈上录序列** —— 周期以周计，
   且本板无硬件，无法闭环验证。
3. **码流封装**：硬件通常只输出"切片载荷"，SPS/PPS/切片头/防竞争字节（emulation
   prevention）/CABAC 对齐都要驱动自己拼，或依赖硬件。**约 1000–2000 行。**
4. **码率控制与质量调参**：CBR/VBR、GOP、B 帧（若支持）、参考帧管理、低延迟模式。
   这一块没有厂商数据就只能靠试，**不可估**。
5. **验证**：编码 → 解码回环 PSNR/SSIM、码率准确性、与 x264/x265 对比、
   长跑稳定性。**需要真实硬件。**

量级判断：即便有硬件 + 寄存器文档，H.264 编码器后端也要 **3000–6000 行新代码 +
数周调参**；HEVC 再加 **2000–3000 行**。没有文档时反向工程的不确定性远大于工作量本身。

## 3. 结论与建议

- **不做**。理由不是"难"，而是**这颗 SKU 没有编码硬件**：`0x920000` 窗口全 1、
  KMD 因 lite 不建 enc 管线、三份独立证据（lite 标志、窗口探测、vendor HAL 归属）一致。
- 如果将来在**非 lite**（`0x12c0000 == 0x21`）的板子上要编码，最短路径是：
  ① 先确认 vendor 栈 `vainfo` 出现 `VAEntrypointEncSlice` 且能跑通一路 `h264_vaapi`；
  ② 用 LD_PRELOAD 拦 `jmo_HD_WriteEncReg` 录寄存器序列；
  ③ 再照解码器的结构写 `mwv207_enc_h264.c`。
- 在这块板子上继续投入编码，等于给不存在的硬件写驱动 —— 已判定为**不做**，
  并把证据留在本报告，避免以后重复调研。

### 1.6 决定性证据（2026-11-13 补充）：厂商芯片表与产品规格

**厂商内核驱动自己的芯片表就写明 JM9100 没有编码器。**
`~/vendor-src/usr/src/mwv207-1.8.0.uos/jmgpu_insert.c`：

```c
static j9_precartilage jmgpu_9200_info = {
    .has_3d = { 1, 1, }, .has_decg1 = { 1, 0 }, .has_decg2 = { 1, 1 },
    .has_enc = 1,  ...            /* JM92 / 9200 系列：有编码 */
};
static j9_precartilage jmgpu_9100_info = {
    .has_3d = { 1, },    .has_decg1 = { 1, 0 }, .has_decg2 = { 1, 0 },
    .has_enc = 0,  ...            /* JM9100：无编码 */
};
```
PCI ID 表把 `0x0731:0x9100` 映射到 `jmgpu_9100_info`；本机 `lspci -nn` 读到的正是
**`Jingjia Microelectronics Co Ltd JM9100 [0731:9100]`**。编码器初始化整段挂在
`if (pinfo->has_enc)` 之下（`jmgpu_insert.c:1634`），本卡直接跳过。

这与 1.3 的寄存器探测**逐项吻合**：`.has_3d={1}` ⇒ 3D1(0x910000) 缺席、
`.has_decg2={1,0}` ⇒ DEC1(0x940000) 缺席、`.has_enc=0` ⇒ ENC(0x920000) 缺席。

**官方产品规格也只写解码**：

| 型号 | 官方"编解码/高清编码"一栏 | 来源 |
|---|---|---|
| **JM9100 / JM9100M**（本机） | 只有"高清解码：支持一路 4K@60fps 解码，支持 H.265/264 等常用格式" | [JM9100](https://www.jemoic.com/products_6/381.html)、[JM9100M](https://www.jemoic.com/products_6/723.html) |
| JM92 系列 | **"高清编码：支持 1 路 4K@60fps H.265、H.264"** | [JM92 系列](https://www.jemoic.com/products_6/569.html) |
| JM1100 | **"2 路 4K@60fps 硬件编码，支持 YUV444 编码"** | [JM1100](https://www.jemoic.com/products_6/820.html) |

⇒ 景美的产品线里编码是 **JM92 / JM1100 起**才有的能力；JM9100 这一代只做到
"1 路 4K@60 解码"。先前看到的 `jmgpu_drv_video.so` 里 `hevcencapi.c` / `jmenc/source/hevc`
字符串，是**同一套驱动给整个家族**（含 JM92/9200）编译进去的代码，不代表本卡有此硬件。
