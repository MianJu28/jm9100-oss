# 厂商栈一次性探测（2026-11-16）：present 机制、12-bit VP9、10-bit 解码

背景：present 每帧 0.395 ms（自研栈）是 draw 密集场景与厂商 5–14× 差距的主因。
本轮切到厂商栈（`jmgpu` + `mwv207_drv.so` + 闭源 UMD），把能对照的项一次测完。

## 1. present：厂商是"双缓冲扫描出 + 异步交接"，swap 几乎免费

| 项 | 厂商 | 自研 r35 |
|---|---|---|
| `glXSwapBuffers`（`vblank_mode=0`，800×600） | **10.9 µs** | 0.395 ms（帧时间差） |
| 同尺寸 320×200 | 11.4 µs | — |
| swap 后 `glFinish` | 0.1 µs | — |
| Xorg ioctl 次数（glmark2 build 5 s，约 61000 帧） | 6411（≈0.1/帧） | 每帧至少一次提交 |
| Xorg CPU（glmark2 build 10 s） | 2.72 s（27% 单核，≈18.7 µs/帧） | — |

- swap 耗时**与窗口尺寸无关**（800×600 与 320×200 同为 ~11 µs）⇒ swap 里没有逐像素拷贝。
- `0x990438`（扫描出帧缓冲基址）在两个地址间**交替**（`0x6b880` ↔ `0x8b2c0`），桌面空闲时也在交替；`0x991438` 同步交替 ⇒ **整屏双缓冲、按帧翻转**。
- 自研栈：`0x990438 = 0x991438 = 0x49c00` 恒定（单缓冲，每帧拷进去）。
- 显示配置寄存器两边**逐字段相同**（`0x990400=0x00500030`、`0x990408=0x04100820`、`0x99040c=0x00140000`、`0x990410=0x08c20898`、`0x990434=0x1e0`、`0x99043c=0x0437077f`）⇒ 扫描出格式一致，**区分不在硬件格式位，而在"翻转 vs 拷贝"**。
- 结论：自研栈的 present 0.395 ms 花在"两次同步拷贝 + 等待"。修复方向是让拷贝异步化/只做一次，而不是去找平铺扫描出位。

## 2. 12-bit VP9：厂商也不支持 ⇒ 该项关闭

```
./src/jmgpu_decoder_vp9.c:1051 ERROR Vp9DecodeUncompressedHeader failed
./src/jmgpu_decoder_vp9.c:1059 ERROR Vp9DecodeCompressedHeader failed
```
用 `vpxenc --profile=2 --bit-depth=12 --input-bit-depth=12` 生成的 320×180 10 帧样片
（`/tmp/t12.webm`）在厂商 VAAPI 上直接报头部解析失败，输出 0 字节。
⇒ 我们解析器里 `VP9 12-bit stream is not supported` 的拒绝与厂商能力一致，**不再列为缺口**。

## 3. 10-bit VP9：厂商反而是不支持的一方

`p10bit.webm`（Profile 2 / yuv420p10le）在厂商 VAAPI 上：
`Decoder thread returned error: Input/output error`（`-f null -` 也是同样结果）。
⇒ 自研栈（A5 实现、逐位一致验证）在这项上**领先厂商**。

## 4. 10-bit HEVC：厂商正常，差异只在舍入

`h10.mp4`（`libx265 -profile:v main10`，320×180，10 帧）：
- 厂商解码 vs 软件解码（hevc）：10-bit 值差 `max 2`、`mean 0.45`、64.7% 完全相同、**PSNR 62.2 dB**；
- 差异是 ±1–2 LSB 的舍入，不是错误（P010 为 MSB 对齐，比较时先 `>>6`）。

## 5. 其它既有结论（不重复测）

- JPEG baseline：厂商**硬件解码可用**（输出 limited range，`(76-16)*255/219≈70`、`(41-16)*255/219≈29` 与实测吻合），我们仍失败 —— 该项保持"待解决"。
- 解码吞吐：厂商 VP9 142.9 / H.264 258.6 / HEVC 270.0 fps，自研 217.0 / 233.2 / 238.4 ⇒ 各有所长。
- buffer/回读：自研显著更好（SubData 12.59 vs 4.66 GB/s，map READ 10.07 vs 0.34 GB/s，readback 1024² 4.27 vs 2.56 ms 厂商略好）。
- VPP：`scale_vaapi` 两边接近（171.6 vs 167.6 fps）。
- Vulkan：厂商包里无 ICD、无 `vk*` 符号 ⇒ 栈层面不可达（`docs/UNRESOLVED.md` D7）。
- H.264 Multiview/StereoHigh：厂商 VAAPI 声明了，但 Mesa 24.1.2 无对应 `PIPE_VIDEO_PROFILE_*` 枚举、libva 侧 `vaCreateConfig` 也映射不到 ⇒ 无法构造客户端路径，暂不做。

## 6. 回落计划

回自研栈后按 §1 的结论做 present 归因与异步化：
1. 在 `mwv207_copy_image`（`flush_resource` 的影子拷贝）与 Xorg present 路径各打计时，确认 0.395 ms 的构成；
2. 让影子拷贝不阻塞客户端（提交后用 fence 交接，而不是当场等完成）——与厂商 11 µs 的交接对齐；
3. 若仍有一次拷贝不可避免，再评估"屏幕 pixmap 直接用客户端影子面"以减少一次。
