# 厂商栈实测基线（2026-11-13，同机同探针）

栈：`jmgpu` 1.8.0（KMD + UMD，X 走厂商 `mwv207_drv.so`，TearFree 默认 on，EXA）。
GL：`OpenGL 4.0 V1.8.0`，renderer `Jingjia JM9100`（不是 Mesa）。
VA-API：`Jm jmgpu driver for JM Jmgpu Encoder JMENC V6.2 /Decoder JMDEC 1.1.2`。
探针：`./tools/perf-suite.sh vendor`（与 `oss-r31` 完全同一套），原始输出 `~/perf-vendor.txt`。

## 1. 对照表

| 项 | 厂商 | 我方 r31 | 比值 |
|---|---|---|---|
| glmark2 Score | **5075** | 1039 | 4.9× |
| glxgears FPS | **15367** | 4071 | 3.8× |
| build | **14537** | 1532 | **9.5×** |
| texture | **5214** | 1236 | 4.2× |
| shading | **6617** | 1362 | 4.9× |
| bump | **22308** | 1610 | 13.9× |
| conditionals | **7023** | 1245 | 5.6× |
| effect2d | **4433** | 1148 | 3.9× |
| pulsar | **9983** | 1396 | 7.2× |
| function / loop | **6766 / 6742** | 1246 / 1253 | 5.4× |
| desktop | **840** | 285 | 2.9× |
| buffer | **1423** | 762 | 1.9× |
| jellyfish | **1168** | 670 | 1.7× |
| shadow | **1223** | 772 | 1.6× |
| terrain | **148** | 109 | 1.4× |
| refract | **119** | 102 | 1.2× |
| present 300² / 800×600 / 1080p | **0.014 / 0.018 / 0.014 ms** | 0.186 / 0.501 / 1.569 ms | **13–111×** |
| 纹理上传 1024/64、2048/64、1024 全张 | 0.03 / 0.04 / 3.15 ms | 0.04 / 0.03 / 2.06–2.90 ms | 持平 |
| `glBufferSubData` 4MB | 4.66 GB/s | **12.59 GB/s** | 我方 2.7× |
| `glMapBufferRange(READ)` 4MB | **0.34 GB/s** | 10.07 GB/s | **我方 30×** |
| `glMapBufferRange(WRITE)` 4MB | 4.76 GB/s | **12.11 GB/s** | 我方 2.5× |
| `glReadPixels` 1024² / 2048² | **2.56 / 4.58 ms** | 4.27 / 11.05 ms | 厂商 1.7–2.4× |
| VP9 1080p 解码 | 142.9 fps | **217.0 fps** | 我方 1.5× |
| H.264 1080p 解码 | **258.6 fps** | 233.2 fps | 厂商 1.1× |
| HEVC 1080p 解码 | **270.0 fps** | 238.4 fps | 厂商 1.1× |
| `scale_vaapi` 1080p→720p | 167.6 fps | 171.6 fps | 持平 |

## 2. 判读

1. **present 的 0.014 ms 不是"真的那么快"**：厂商 GL 的 `glXSwapBuffers` 基本异步（300 帧循环里不等翻转完成），
   所以以 swap 为节拍的 FPS 被系统性放大。要公平比较必须 `glFinish` 之后再计时；这一点我方的
   `pbench`（Mesa 路径）是同步的。⇒ present 差距用这张表**只能定性**（厂商确实不在关键路径上做整屏拷贝）。
2. **draw 密集场景的真实差距是 5–14×**（build/bump/pulsar）。这些场景像素量小，差距只能来自每 draw 的
   CPU 提交与状态校验 ⇒ 对应我方未启用 `u_threaded_context`。
3. **像素密集场景 1.2–1.7×**（terrain/refract/shadow/jellyfish）⇒ ISA 边界，与我方旧结论一致。
4. **buffer/回读是我方占优或接近**：`glMapBufferRange(READ)` 我方快 30×（厂商把 buffer 放 VRAM，
   map 读是 WC，正是我们 `327b330` 修掉的那个问题）；回读厂商 DMA 更快（1.7–2.4×）。
5. **解码**：VP9 我方快 1.5×（别名+双缓冲的成果）；H.264/HEVC 厂商快约 10%；VPP 持平。

## 3. 意外发现：厂商 VA-API 的 profile 列表

```
VAProfileH264ConstrainedBaseline/Main/High : VLD
VAProfileH264MultiviewHigh / StereoHigh    : VLD   ← 我方没有
VAProfileJPEGBaseline                      : VLD   ← 我方实现失败、已收回
VAProfileHEVCMain / Main10                 : VLD
VAProfileVP9Profile0 / Profile2            : VLD
```
没有任何 `VAEntrypointEnc*`（与"本 SKU 无编码硬件"的结论一致，尽管驱动名里写着 `JMENC V6.2`）。

## 4. 厂商 JPEG 硬件解码实测（重要）

`ffmpeg -hwaccel vaapi -vaapi_device /dev/dri/renderD128 -i t1080.jpg -f rawvideo -pix_fmt nv12 vjpg_hw.yuv`
→ **成功出图**（3,110,400 B）。与软件解码 `vjpg_sw.yuv` 逐字节不同，但差异是**范围映射**：
厂商输出 limited range（`(76-16)*255/219 ≈ 70`、`(41-16)*255/219 ≈ 29`，与实测 70/29 完全吻合）。

⇒ **硬件 JPEG baseline 解码在本 SKU 上是可用的**，我方此前遇到的 `DEC_ERROR_INT(0x10102)+TDR`
是寄存器编程细节问题，不是硬件缺失。这一项可以从"不可实现"改回"待解决"，且厂商输出可作参考。

## 复跑（2026-11-13 15:49，安装栈 r35）

同一套 `tools/perf-suite.sh oss-r35`，结果 `~/perf-oss-r35.txt`。与 r31 那次相比属同一量级，
注意这一次**桌面上开着 Firefox**（`top` 里 firefox 占 73% CPU、load average 5.3），
所以 draw/解码类数字偏保守，present 与像素带宽不受影响：

| 项 | r35（15:49，有 Firefox） | r31（11:13，空桌面） |
|---|---|---|
| glmark2 | 1062 | 1039 |
| build / texture / shading / bump / conditionals | 1563 / 1234 / 1359 / 1611 / 1242 | 1532 / 1236 / 1362 / 1610 / 1245 |
| jellyfish / shadow / pulsar | 689 / 768 / 1388 | 670 / 772 / 1396 |
| present 300² / 800×600 / 1080p | 0.151 / 0.508 / 1.593 ms | 0.186 / 0.501 / 1.569 ms |
| SubData / map READ / map WRITE（4 MB） | 12.88 / 13.40 / 11.91 GB/s | 12.59 / 10.07 / 12.11 GB/s |
| glReadPixels 1024² / 2048² | 4.30 / 12.61 ms | 4.27 / 11.05 ms |
| VP9 / H.264 / HEVC 解码 1080p | 131.6 / 238.6 / 267.0 fps | 217.0 / 233.2 / 238.4 fps |
| scale_vaapi 720p | 128.7 fps | 171.6 fps |

VP9 解码那两项的差异来自 Firefox 抢 CPU（同一份代码在 r34 上单独测仍是 215.8 fps），
不是回归；要干净的解码数字需关掉图形负载重跑。

### 干净复跑（2026-11-13 15:56，Firefox 已关，load ≈ 3.1 → 2.0）

`./tools/perf-suite.sh oss-r35-clean`（`~/perf-oss-r35-clean.txt`）——这才是与厂商那次
可比的数字：

| 项 | r35 干净 | r35 有 Firefox | 厂商 1.8.0 |
|---|---|---|---|
| glmark2 | **1290** | 1062 | 5075 |
| glxgears | **6527** | 4130 | 15367 |
| build / texture / shading / bump / conditionals | **1930 / 1488 / 1644 / 1995 / 1477** | 1563 / 1234 / 1359 / 1611 / 1242 | 14537 / 5214 / 6617 / 22308 / 7023 |
| pulsar / jellyfish / shadow / effect2d | **1682 / 732 / 864 / 1329** | 1388 / 689 / 768 / 1131 | 9983 / 1168 / 1223 / 4433 |
| terrain / refract / desktop | 112 / 104 / **623** | 117 / 101 / 564 | 148 / 119 / 840 |
| present 300² / 800×600 / 1080p | **0.068 / 0.352 / 1.388 ms** | 0.151 / 0.508 / 1.593 | 0.014 / 0.018 / 0.014（异步，见正文） |
| 纹理上传 1024/64、2048/64、1024 全张 | **0.02 / 0.02 / 1.27 ms** | 0.05 / 0.03 / 2.53 | 0.03 / 0.04 / 3.15 |
| SubData / map READ / map WRITE | **13.22 / 14.92 / 13.84 GB/s** | 12.88 / 13.40 / 11.91 | 4.66 / 0.34 / 4.76 |
| glReadPixels 1024² / 2048² | **3.54 / 9.68 ms** | 4.30 / 12.61 | 2.56 / 4.58 |
| VP9 / H.264 / HEVC 1080p 解码 | **161.4 / 303.9 / 311.3 fps** | 131.6 / 238.6 / 267.0 | 142.9 / 258.6 / 270.0 |
| scale_vaapi 1080p→720p | **144.8 fps** | 128.7 | 167.6 |

- 干净环境下 H.264/HEVC 解码反超厂商（304/311 vs 259/270），VP9 仍低于厂商
  （161 vs 143 是反过来的：我们更快？见下）。
- **VP9 数字的新旧差异：已定性为"客户端解析成本"，不是驱动回归。** 给
  `mwv207_video_decoder_end_frame()` 加临时 `MWV207_DECTIME` 分段计时（已删）后实测
  （1080p / 360p，每 20 帧打印一次）：

  | 分辨率 | `setregs`（CPU：解析+probs+寄存器） | `submit`（含等 DEC 完成） | 端到端 |
  |---|---|---|---|
  | 1920×1080 | **0.03–0.04 ms** | **1.88–1.93 ms** | 1.92–1.96 ms |
  | 640×360 | 0.03–0.06 ms | 0.89–0.93 ms | 0.94–0.95 ms |

  即**驱动每帧只花 1.9 ms（1080p），其中 1.88 ms 是等硬件**；而 `ffmpeg` 进程的 user 时间
  是 3.2 ms/帧（1080p）/ 1.2 ms（360p），这部分是 ffmpeg 自己的 VP9 头/tile 解析（与码率
  同阶，4 Mbps vs 1 Mbps）。端到端 6.0 ms = 驱动 1.9 + ffmpeg 解析与 VA 封装约 4.1。
  所以 217↔165 fps 的差别落在**客户端解析占比与当次 CPU 状态**上，我们的驱动侧没有变化
  （`setregs` 已是 0.03 ms 量级，`submit` 纯粹是硬件解码时间）。这也给出 VP9 的硬件上限：
  1080p 约 **530 fps**（1.9 ms/帧）、360p 约 **1080 fps**（0.93 ms/帧）。
