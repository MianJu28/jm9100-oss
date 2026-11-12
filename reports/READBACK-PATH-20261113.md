# 回读路径（glReadPixels / VAAPI 下载）分析

日期：2026-11-13
环境：JM9100（MWV207），`oss-24.1.2-fullstack-r30`，KMD `C588E9673F984406AF46A2F`，内核 6.6.155，X11 `:0`

## 1. 现象

`glReadPixels(0,0,1024,1024,GL_RGBA,GL_UNSIGNED_BYTE)` 每次约 **3.8–4.4 ms**（4 MB，≈0.9–1.0 GB/s）。
`readback-scale` 的尺寸标定：

| 尺寸 | 字节 | ms/次 | 有效 GB/s |
|---|---|---|---|
| 128² | 0.06 MB | 0.576 | 0.11 |
| 256² | 0.25 MB | 0.877 | 0.28 |
| 512² | 1.00 MB | 1.860 | 0.53 |
| 1024² | 4.00 MB | 4.846 | 0.81 |
| 2048² | 16.0 MB | 11.181 | 1.40 |

边际斜率：(16−4) MB / (11.18−4.85) ms ≈ **1.9 GB/s**。

## 2. 时间构成（驱动侧插桩，`MWV207_RBTIME`）

在 `mwv207_transfer_map()` 里分段：

| 段 | 实测 |
|---|---|
| staging 分支（建 LINEAR 影子 + blit + wait） | 从未打印 ⇒ pbuffer 表面本身是 `MWV207_LINEAR`，走线性分支 |
| 回读 DMA 缓冲创建 | 0.009–0.021 ms |
| DMA 拷贝 + `bo_wait` | **1.4–2.0 ms（4 MB）**，约 2.5 GB/s |
| CPU 侧逐行拷贝（Mesa 打包成用户缓冲） | **≈2.3 ms（4 MB）** |
| `transfer_map` 相邻调用的间隔 | 1.9–3.9 ms（即每次 readback 的全部时间都在 map 内） |

`GL_IMPLEMENTATION_COLOR_READ_FORMAT = GL_BGRA(0x80e1)`、`TYPE = GL_UNSIGNED_BYTE`；
分别用 `GL_RGBA`、`GL_BGRA`、PBO 读回：4.19 / 3.77 / 4.26 ms ⇒ **格式与 PBO 都不是瓶颈**。

## 3. 机器内存基准（关键）

`/tmp/mc2.c`（64 MB 缓冲，读结果参与校验）：

```
memcpy 64MB: 33.01 ms   1.94 GB/s
memset 64MB:  2.61 ms  24.48 GB/s
```

即本机 CPU 侧**大工作集读**只有约 **1.9 GB/s**，写远快于读。
4 MB 工作集能落进缓存，重复读可以达到 12 GB/s（`buffer-bench` 的 GTT 场景），但回读的数据是
GPU 刚 DMA 写入 DRAM 的，CPU 读到的是**冷数据**，只能按 1.9 GB/s 走 ⇒ 4 MB ≈ 2.1–2.3 ms 的下限。

## 4. 结论

- `3.8–4.4 ms ≈ 1.4–2.0 ms DMA + 2.3 ms 冷 DRAM 拷贝`，已是该路径的硬件下限；
- 唯一可省的分配开销已改为复用 `ctx->retired_staging[]` 中同尺寸条目（`MWV207_READBACK_REUSE`，
  默认开，`=0` 关闭）：**5–8%**，噪声级，但省掉每次 4–16 MB 的 BO 分配与页操作；
- 想进一步加速只能让 DMA 与 CPU 拷贝重叠，这与 `glReadPixels` "返回前必须拿到像素"的语义冲突；
- VAAPI 的下载路径（`vaDeriveImage`/`vaGetImage`/`-f null` 不带 `-hwaccel_output_format vaapi`）
  共用同一条"GPU 写 DRAM → CPU 冷读"链，结论相同。

## 5. 探针坑（写给以后的自己）

1. `memcpy` 的结果如果没有被读出来，编译器会把循环里的 `memcpy` 提成一次（甚至只拷被读的那一个字节），
   测出"12 GB/s"这类假值。`buffer-bench.c` 已加 `volatile sink` 校验；`bo-mmap-bench.c` 待补。
2. 4 MB 级工作集受缓存影响极大，引用任何带宽数字都要注明工作集大小与冷热状态。
3. `bo_wait` 返回 bool（`true` = 成功），按 `0` 判断会把"已完成"当成超时。
