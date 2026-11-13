# 当前驱动 vs 厂商驱动：还剩哪些性能差距（2026-11-13）

> 我方 = r31 安装栈（glamor blit 快路径 + AIGLX + 平铺窗口 + 修法 1/3/4）。
> 厂商 = `jmgpu` 1.8.0（2026-11-11 同日同机实测，`glmark2 -b <scene>:duration=3`，800×600）。
> 我方今天的数字用 `:duration=2`（duration 对 FPS 影响 <2%，但严格对比应同日同参数，见 §4）。

## 1. 总分与典型场景

| 项 | 厂商 | 我方 r31 | 差距 |
|---|---|---|---|
| glmark2 全套 Score | **5209** | **1259–1261**（今天，桌面无 Firefox）；1039–1046（带桌面负载） | **4.1×** |
| glxgears（`vblank_mode=0`） | 15134 / 17621 | **4054** | **3.7–4.3×** |

## 2. 逐场景（FPS）

| 场景 | 厂商 | 我方 r31 | 差距 |
|---|---|---|---|
| `build`（use-vbo=false / true） | 5982 / 14856 | **1581** | 3.8× / 9.4× |
| `texture` | 5986–6506 | 1264 | 4.7–5.1× |
| `shading` | 6494–6917 | 1365 | 4.8–5.1× |
| `bump` | 3701 / 10286 | 1641 | 2.3× / 6.3× |
| `conditionals` | 6126–7589 | 1247 | 4.9–6.1× |
| `pulsar` | 8922 | 1403 | 6.4× |
| `function` | 6882–6990 | 1256 | 5.5–5.6× |
| `loop` | 6875–6962 | 1240 | 5.5–5.6× |
| `effect2d` | 1656 / 3551 | 1146 | 1.4× / 3.1× |
| `desktop` | 868 / 1213 | 281 | 3.1× / 4.3× |
| `buffer` | 1159–1463 | 760 | 1.5–1.9× |
| `jellyfish` | 1175 | 683 | **1.7×** |
| `shadow` | 1241 | 778 | **1.6×** |
| `terrain` | 155 | 111 | **1.4×** |
| `refract` | 123 | 103 | **1.2×** |

规律：**重像素场景（terrain/refract/jellyfish/shadow）只差 1.2–1.7×；便宜、draw 密集场景差 3.8–6.4×。**

## 3. 把每帧时间拆开（今天实测，800×600）

| 场景 | 只渲染（`--off-screen`） | 正常（swap） | present 差 | 厂商只渲染（推算 FPS 倒数） |
|---|---|---|---|---|
| `build` | 0.342 ms | 0.648 ms | **0.306 ms** | 0.167 ms |
| `pulsar` | 0.421 ms | 0.727 ms | **0.306 ms** | 0.112 ms |
| `conditionals` | 0.479 ms | 0.810 ms | **0.330 ms** | 0.132 ms |
| `shading` | 0.452 ms | 0.741 ms | **0.289 ms** | 0.153 ms |

⇒ 便宜场景的时间构成大约 **45% 是 present、55% 是渲染（且渲染本身也比厂商慢 2–4×）**。

## 4. 三类剩余差距

### 4.1 present / 合成：结构性，已无软件手段（占便宜场景约 45%）

- 我方每帧固定 **0.29–0.33 ms**（800×600；300×300 是 0.17 ms，1080p 是 1.54 ms），
  本质是"窗口内容 → 线性屏幕"的一次拷贝，PE 写线性只有 ~4.2 GB/s（每 MB 约 0.24 ms）。
- 厂商走**自有 DDX + EXA + 2D 引擎**：可直接把窗口当硬件 surface 合成，几乎不付这笔。
- 我方三条替代路径都已判死：2D 引擎提交成功但硬件不执行（`679c60d`）、DE 平铺扫描出不可达
  （`3262506`/`3c0d296`）、glamor blit 快路径已用上（`4bb9755`）。

### 4.2 draw 密集场景的渲染本身慢 2–4×（**唯一还值得投入的一块**）

- 今天：`build` 0.342 ms、`pulsar` 0.421、`conditionals` 0.479、`shading` 0.452（只渲染）。
- 厂商：0.112–0.167 ms。
- 这些场景像素量很小，瓶颈是**每 draw 的 CPU/命令提交与状态切换**。我们已经把每 draw 的
  着色器指纹开销从 0.86 µs 降到 0.17 µs（`ccd9d9c`），但仍然是每帧几十万微秒的来源。
- **我方没有启用 `u_threaded_context`**（`src/gallium/auxiliary/util/u_threaded_context.c`
  在树里，但没有任何 mwv207 代码调用 `util_threaded_init()`）；厂商很可能做了线程化提交。
  这是目前唯一"可能显著缩小 4–6× 差距"的杠杆，但它要求驱动侧线程安全（我们的
  `mwv207_transfer_map` 在回调里 `bo_map`、`retired_staging` 环也不是锁保护的）。

### 4.3 像素密集场景 1.2–1.7×：ISA 边界，已定案不算缺陷

`terrain`/`refract` 的差距落在每像素超越函数（`fsin/fexp2/flog2/frcp/frsq` 只有单 lane
`SRC_X_X_0`，无 vec4 形式），厂商同 ISA 也只快 1.2–1.4×，靠的是更紧的寄存器复用
（`503b460` 已把排除表入档）。

## 5. 还没做过对照的部分（不知道差距，不敢下结论）

以下只有我方数字，**厂商侧没有同日测量**：VP9 1080p 解码 220 fps、H.264 234 fps、HEVC 269 fps；
纹理上传 1024² 小块 0.05 ms；`glBufferSubData` 4 MB 0.30 ms（12.9 GB/s）、`glMapBufferRange` 读 0.33 ms；
`glReadPixels` 4 MB 4.8 ms；`scale_vaapi` 1080p→720p 171 fps。

要把这几项也做同期对照，需要 `./switch-mwv207.sh stack vendor` + 重启，跑同一套探针后再切回。

## 6. 结论

- **差距集中在两处**：present 的固定拷贝（约 0.3 ms/帧，软件已到极限）和 draw 密集场景的
  提交开销（2–4×，唯一还有空间的项）。
- 重像素场景（实际游戏/3D 应用的主力）已经只差 1.2–1.7×，接近同代 ISA 的合理范围。
- 若要继续投入，优先级：① 线程化上下文实验（A/B，环境变量门控）② 同日厂商对照补齐
  解码/上传/回读的数字。

## 7. 后续定位：每 draw 与 present 的真实构成（2026-11-13 下午追加）

厂商栈对照（`~/perf-vendor.txt`）显示 draw 密集场景差 5–14×，当时归因为"未启用
`u_threaded_context`"。实际做了三组实验后，这个归因**需要修正**。

### 7.1 每 draw 的 CPU 开销（`tests/draw-bench/drawbench.c`）

| 形态 | 每 draw | 吞吐 |
|---|---|---|
| VBO（真实应用形态） | **0.45–0.53 µs** | ~200 万 draws/s |
| client arrays | **26–44 µs** | ~2.5 万 draws/s |
| client arrays + `MWV207_NOBUFWAIT=1` | 0.52 µs | ~190 万 draws/s |

驱动内部插桩（`mwv207_draw_vbo()` 分段计时）：`config_render_pipeline=0.92 µs`、`submit=0.04 µs`，
说明 pipe 层本身没有问题。client-array 的 50 倍差距来自
`mwv207_transfer_map()` 的 `PIPE_BUFFER` 分支：每次 map 都调用
`mwv207_wait_resource()`（内部 `mwv207_submit(ctx,NULL,true)` + 无限 `bo_wait`），
`strace` 实测约 **2 次 ioctl/draw**。这条等待是为修 WebRender UBO 覆写（P6）加的，
不能简单删除，正确做法是按 BO 的在途栅栏判断。

### 7.2 `u_threaded_context` 试验（已回退）

按标准接法试过（资源结构用 `pipe_resource`/`threaded_resource` 的 union +
`threaded_resource_init/deinit` + `threaded_context_create`，`MWV207_THREADED` 门控）。
`glmark2 -b build` 在 `glDrawArrays` 里堆损坏崩溃（`malloc(): invalid size`，
gdb 停在 `calloc ← _mesa_bufferobj_alloc`，即更早的越界写）。结合 7.3 的结论——
GL 场景的 CPU 不是瓶颈——该项收益有限，已回退，diff 存档 `~/work-note-tc-diff.txt`。

### 7.3 glmark2 build 的每帧构成

`MWV207_STATS=1` 计数（2 秒）：`draws=3065 / submits=3073 / bufmap_wait=2`，
即**每帧 1 次 draw、1 次提交**，帧内 CPU 不是瓶颈。同场景 `--off-screen` 3881 FPS（0.258 ms）
vs 带 swap 1532 FPS（0.653 ms）⇒ **present 约 0.4 ms，占大头**。

### 7.4 present 拷贝的带宽上限

`glBlitFramebuffer`（FBO→FBO，同格式 RGBA8，走 PE 的 `mwv207_copy_image`）：

| 尺寸 | 每次 `glFinish` | 批量提交后一次 `glFinish` |
|---|---|---|
| 800×600（1.92 MB） | 0.372 ms（5.2 GB/s） | **0.215 ms（8.9 GB/s）** |
| 1920×1080（8.29 MB） | 1.084 ms（7.7 GB/s） | **0.881 ms（9.4 GB/s）** |

present 实测（`vblank_mode=0`）：800×600 **0.476 ms/帧**、300×300 **0.172 ms/帧**；
`MWV207_COPYSTATS` 显示每帧约 **1.5 次** `mwv207_copy_image`（800×600 时约 2.9 MB/帧）。
去掉拷贝后的强制提交（`MWV207_COPY_NOFORCE=1`）几乎无变化（0.484 ms）⇒ 瓶颈是拷贝带宽本身，
不是提交次数。

### 7.5 修正后的结论

- **每 draw 的 CPU 开销不是差距来源**（VBO 0.5 µs/draw），因此 `u_threaded_context` 不是主要答案；
  唯一真实的 CPU 异常是 client-array 路径的 buffer-map 等待（50×），已登记待修。
- 廉价场景的 5–14× 差距主要落在 **present**：我们把窗口内容用 PE 拷进线性屏幕，
  上限约 9 GB/s（800×600 约 0.2 ms、1080p 约 0.9 ms）；厂商 0.014 ms 说明它**根本不做这份拷贝**
  （2D 引擎或客户端缓冲直接扫描出）。
- 我方 2D 引擎启动位依赖厂商未发布函数（b163 已判死路）、DC 平铺扫描出在本机不可达
  （`3262506`/`3c0d296`）⇒ 在不改变"客户端缓冲可直接扫描出"这一前提下，present 不可能追平厂商，
  这项归为**结构性差距**，不再作为优化目标。
