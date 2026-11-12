# `present` 每帧多花 ~0.55 ms：根因定位（2026-11-11）

本机：景美 JM9100 / MWV207，Debian 25 aarch64，内核 `6.6.155-arm64-desktop-hwe`，
KMD `8DFC520FB8FCDECFBC911AA`，UMD 载荷 `oss-24.1.2-fullstack-r25`
（DRI md5 `e8570c2c8673bd6617bdda272717385c`），Xorg 在 **`:1`**：
`DISPLAY=:1 XAUTHORITY=/home/admin/.Xauthority`。

## 1. 结论先行

`glmark2` 里"便宜帧"落后厂商 4–10×（`build` 1049 vs 5982、`texture` 960 vs 6506、
`conditionals` 855 vs 7589），差距几乎全部来自 **`present`/合成这一路**，而不是 3D 渲染：

- 同一场景 `glmark2 -b build:duration=3`：`--off-screen` **3607 FPS（0.277 ms）** →
  `--frame-end=finish` **2447 FPS（0.409 ms）** → 默认（swap）**1049 FPS（0.953 ms）**
  ⇒ 交换/合成 **≈0.55 ms/帧**，占整帧 58%。
- **根因**：X 的 glamor 用 **3D 采样着色器**把窗口 pixmap 合成到屏幕 pixmap，
  而窗口/屏幕 pixmap 都是**线性（未平铺）内存**；本驱动的 3D 采样对线性表面
  只有平铺表面的 **1/3 带宽**（实测 6.7 GB/s vs 20.9 GB/s）。
- 这不是 present 协议本身的问题：纯 `glXSwapBuffers`（不画）800×600 就要
  **0.65 ms**，而 GPU 纯 blit（不采样）同样数据量只要 **0.20 ms**。

## 2. 测量方法与数据

### 2.1 纯 present 成本（`/tmp/sw.c`：只 swap，不画）

| 尺寸 | swap-only | 说明 |
|---|---|---|
| 800×600 | **0.653–0.661 ms** | 与 glmark2 `build` 的 0.55 ms 吻合 |
| 1920×1080 | **2.304 ms** | 面积 ×4.3，时间 ×3.5 |
| 400×300 | 0.305 ms | |
| 128×96 | 0.148 ms | |
| 32×32 | 0.109 ms | 固定开销（协议 + X 往返）≈0.11 ms |
| 1×1 | 0.125 ms | 同上 |

⇒ 成本 = 0.11 ms 固定 + 与像素数成正比（800×600 时约 0.55 ms = 1.9 MB 读 + 1.9 MB 写，
折合 **≈6 GB/s**）。

### 2.2 谁是瓶颈：不是 kwin

- `qdbus org.kde.KWin /Compositor suspend` 前后各测一次：**0.770 → 0.767 ms（无变化）**；
  `org.kde.kwin.Compositing.active` = **false** ⇒ 当前桌面根本没开合成，
  这一路就是 X 自己的 glamor 拷贝。
- CPU 账（`/proc/<pid>/stat` 3 秒采样，800×600 循环）：
  **Xorg 0.057 ms/帧、kwin_x11 0.000 ms/帧、客户端 0.026 ms/帧** —— 合计 0.08 ms，
  远小于 0.65 ms ⇒ 时间花在 GPU 与等待上，不是 CPU 拷贝。

### 2.3 GPU 侧能力（`/tmp/blit.c`、`/tmp/fill2.c`）

| 测试 | 每秒像素带宽 | 备注 |
|---|---|---|
| `glBlitFramebuffer` 800×600（RGBA8→RGBA8） | **19.0 GB/s**（0.202 ms / 3.8 MB） | 与平铺/线性无关 |
| `glBlitFramebuffer` 1920×1080 | 18.5 GB/s | 线性目标同样快 |
| 采样+绘制，源纹理**平铺** | **20.9 GB/s**（0.184 ms） | |
| 采样+绘制，源纹理**线性** | **6.7 GB/s**（0.576 ms） | **3.1× 慢** |
| 线性源，stride 3072 / 3200 / 3328 / 3840 / 4096 / 4352 | 5.9–6.8 GB/s | 与 stride 对齐无关 |
| **只写不读**（clear + 不采样的三角形）→ **平铺**目标 | **10.4 GB/s**（0.185 ms） | |
| **只写不读**（clear + 不采样的三角形）→ **线性**目标 | **3.3 GB/s**（0.582 ms） | **写入端同样慢 3.1×** |

对照实现：把 `mwv207_can_tile()` 临时改成"一律线性"（`MWV207_FORCE_LINEAR=1` 门控，
测量后已删除该临时改动），同一支程序的结果即是上表最后一行；恢复后回到 20.9 GB/s。

### 2.4 为什么窗口 pixmap 是线性的

`src/mwv207-umd/drivers/mwv207/mwv207_resource.c` 的 `mwv207_can_tile()`：

```c
const bool enable_scanout_tile = false;      /* 故意关闭 */
if (res->base.bind & PIPE_BIND_LINEAR)  return PIPE_ERROR_BAD_INPUT;
if (res->base.bind & PIPE_BIND_SCANOUT) return PIPE_ERROR_BAD_INPUT;
```

GLX 的窗口缓冲（DRI3 present 源）与 X 的屏幕 pixmap 都带 `PIPE_BIND_SCANOUT`，
于是一律落在 `MWV207_LINEAR`。glamor 合成时把它们当纹理采样 —— 于是每帧都吃
6.7 GB/s 的线性采样速率。

## 3. 与厂商栈的差距构成

| 环节 | 自研 | 厂商 | 倍率 |
|---|---|---|---|
| `build` 整帧（800×600） | 0.953 ms | 0.167 ms | 5.7× |
| 其中 present | ≈0.55 ms | ≈0.05 ms（估） | ≈10× |
| 纯渲染（`--off-screen`） | 0.277 ms | ≈0.05 ms | ≈5× |

即：**present 与渲染两条都慢，present 更突出**。厂商 DDX 用 **EXA + 2D 引擎**
做合成拷贝（不经过 3D 采样），其窗口缓冲也可以走 supertile；我方走 glamor 的
着色器合成，且缓冲被强制线性。

## 4. 关键修正：线性慢的不只是"读"，写也一样

先前的推测"只把窗口 pixmap 平铺、屏幕 pixmap 保持线性"**不成立**。决定性测量
（同一支程序，仅切换目标缓冲的布局）：

| 场景 | 平铺 | 线性 |
|---|---|---|
| clear + 不采样的三角形（纯写 1.9 MB） | **0.185 ms（10.4 GB/s）** | **0.582 ms（3.3 GB/s）** |
| 采样 + 绘制（读 1.9 MB + 写 1.9 MB） | 0.184 ms | 0.576 ms |

⇒ 3D 单元访问线性表面的**读写两端都是 3.3 GB/s 量级**，只有平铺表面能到 10–21 GB/s。
glamor 的合成是"读窗口 pixmap + 写屏幕 pixmap"，两端都是 3D 访问：

- 只平铺窗口 pixmap（屏幕仍线性）⇒ 写入端仍是 0.58 ms 级，present 几乎不改善；
- 两端都平铺 ⇒ present 降到 ≈0.20 ms（≈2.7×）。

## 5. 两种改法与厂商栈的距离

| 方案 | 改动范围 | present（800×600） | `glmark2 build` 估算 | 与厂商（0.167 ms / 5982 FPS）的差距 |
|---|---|---|---|---|
| 现状 | — | 0.55 ms | 0.95 ms（1049 FPS） | 5.7× |
| ①只平铺窗口 pixmap，屏幕保持线性 | 仅 UMD（`mwv207_can_tile`） | ≈0.5 ms（写入端未改善） | ≈0.9 ms | ≈5×（基本无收益） |
| ②窗口 + 屏幕（扫描出）都平铺 | UMD + **KMD 编 DE supertile** | ≈0.20 ms | ≈0.30 ms（≈3300 FPS） | ≈1.8× |
| ③全屏 direct scanout（真 flip） | 仅对全屏应用有效 | — | — | 视场景 |

**结论：只有②能明显接近厂商栈**；①的收益被"写线性目标同样慢"抵消。
②剩下的 1.8× 差距来自 3D 渲染效率本身（我方 offscreen `build` 0.185–0.277 ms
vs 厂商 ≈0.05–0.09 ms），那是另一条线（着色器/光栅化开销），与 present 无关。

②的落地前提：硬件有 DE supertile —— 厂商头文件里
`MWV207REG_DE_GENERAL_CONFIG_SUPERTILE_VERSION`（`SUPER_TILE_V1/V2/V3`）与
`JM_MINOR_FEATURES4_DE_SUPERTILE_AVAILABLE`；当前 `src/kernel-6.6/dc/` 完全不碰这个
寄存器，而 UMD 的 4 KB 平铺布局必须与 DE 侧的版本/布局严格一致，否则显示花屏。
需要：① 从厂商 DDX/DE 侧确认布局与版本号（本项目只有 KMD 源码，DDX 闭源，
可用 `ref/vendor-1.8.0` 的 KMD 中 `DRM_JM_GEM_TILING_SUPERTILED` 路径反推）；
② KMD 在 plane 更新时写 supertile 版本与 stride/高度对齐；③ UMD 允许
`PIPE_BIND_SCANOUT` 走 `MWV207_SUPERTILED`。


## 6. 进一步取证：厂商栈是不是真的用"平铺扫描出"？

对厂商 KMD 做的对照（`/lib/modules/6.6.155-arm64-desktop-hwe/updates/dkms/jmgpu.ko`，
stripped 但有函数名；源码 `ref/vendor-1.8.0` 的 75 个 `.c`）：

- 厂商的主平面更新函数（`j9_handle__attribute_chalkstone`，即 plane atomic update）
  **只写 5 个寄存器**，与我们的 `mwv207_primary_atomic_update()`
  （`src/kernel-6.6/dc/mwv207_va.c:170-206`）完全一一对应：
  `0x434 = pitch>>4`、`0x43C = ((h-1)<<16)|(w-1)|0x3`、`0x4F8 = (w-1)<<16`、
  `0x438 = addr>>6`、`0x430` 按 fourcc 置格式位（`XR30`/`AR30` → `|1`，
  `AR24`/`XR24` → `&= ~3`，`RG16`/`RGB565` → `|2`）。
  **没有任何平铺相关位**。
- 厂商 KMD 确实有 `DRM_IOCTL_JM_GEM_SET_TILING`（`DRM_JM_GEM_TILING_SUPERTILED`），
  但发布源码里 `tilingMode` 只在 set/get ioctl 里出现，**没有消费者**；
  厂商 DDX `mwv207_drv.so` 也没有 import `drm_jmgpu_bo_set_tiling`。
- 结论：**没有证据表明厂商用平铺缓冲扫描出**。厂商的 `present` 更快，更可能是
  它的 DDX 用 2D 引擎（EXA）做合成拷贝，而不是 glamor 的 3D 着色器采样。

## 7. 关键补充测量：blit 写线性目标远快于 3D 写线性目标

同一对 FBO（800×600），只改驱动布局（`MWV207_FORCE_LINEAR=1` 的临时构建）：

| 操作 | 平铺 | 线性 |
|---|---|---|
| `glBlitFramebuffer`（FBO→FBO） | **0.100 ms（38.3 GB/s）** | **0.222 ms（17.3 GB/s）** |
| 着色器采样拷贝（纹理→FBO） | 0.184 ms（20.9 GB/s） | 0.576 ms（6.7 GB/s） |

⇒ 线性目标上，**blit 比着色器快 2.6×**（0.222 vs 0.576）。这解释了厂商的快：
2D/EXA 路径是 blit 级别，glamor 是着色器级别。

## 8. 由此得出的四个可实施项（按性价比）

| 方案 | 改动 | present（800×600） | `glmark2 build` | 备注 |
|---|---|---|---|---|
| A. 让 glamor 用 blit 而不是着色器 | **X server 补丁**（glamor 21.1 只有着色器路径） | 0.65 → ≈0.25 ms | ≈0.55 ms（≈1800 FPS） | 不动驱动与 KMD，风险最低；需自编 xorg-server |
| B. 扫描出平铺（UMD+KMD） | UMD 放开 SCANOUT 平铺 + KMD 新增平铺显示支持 | ≈0.10–0.15 ms | ≈0.45 ms（≈2200 FPS） | 但见第 5 节：厂商也没这么做，KMD 侧寄存器序列无公开证据，风险高 |
| C. 查 3D "写线性"慢在哪 | 仅 UMD | 已查明：PE 写线性 3.3 GB/s 属硬件行为（平铺无 TS 也只有 12.8），**无便宜修法** | — | 结论：没有可修的配置项 |
| D. 窗口 present 走 overlay plane（零拷贝） | **KMD 新增 overlay plane + DDX/Present 对接** | ≈0 ms | ≈0.32 ms（≈3100 FPS） | 厂商 KMD 有 `jmgpu_crtc_set_overlay`，我方 KMD 无 overlay 面；工程量最大但上限最高 |

建议顺序：**A（glamor blit 补丁，改动最小、收益确定 ≈0.25 ms）→ D（overlay plane 零拷贝 present，上限最高）→ B（扫描出平铺，证据已被反汇编削弱）**；C 已排除（第 9 节）。

## 9. 再补一层：把"平铺"和"tile status（压缩）"分开测

为了判断线性慢到底是"布局"还是"缺 tile-status/压缩"，用 `MWV207_NO_TS=1`
（临时改 `mwv207_set_miptree()`：强制 `MWV207_SUPERTILED`、但跳过
`mwv207_get_tile_info()` 的 TS 缓冲分配；测完已 `git checkout` 回退）做了三方对照：

| 目标布局 | `glBlitFramebuffer` 800×600 | 着色器写 1.9 MB |
|---|---|---|
| 平铺 + TS | **0.100 ms（38.4 GB/s）** | **0.180 ms（21.3 GB/s）** |
| 平铺、无 TS | 0.205 ms（18.7 GB/s） | 0.300 ms（12.8 GB/s） |
| 线性 | 0.222 ms（17.3 GB/s） | 0.576 ms（6.7 GB/s） |

三点结论：

1. **blit 引擎对线性目标并不慢**（0.222 ms，与"平铺无 TS"的 0.205 基本同档），
   慢的是 **PE（3D）写线性**（0.576 ms）。blit 快 2.6× 的原因就在这里。
2. **TS/压缩带来另外 2×**（平铺 0.205 → 0.100；着色器 0.300 → 0.180），
   而且 **TS 只对平铺资源分配**（`mwv207_resource.c:212`
   `if (res->mode != MWV207_LINEAR) mwv207_get_tile_info(...)`）⇒ 线性资源既没有
   平铺也没有 TS，是双重劣势。
3. 因此"让 glamor 改走 blit"能拿到的是 **0.65 → ≈0.25 ms**（不是 0.65 → 0.10），
   剩下那 2× 必须靠平铺 + TS。

另外，厂商同条件 `build` 场景 5982 FPS（0.167 ms/帧）**已经低于我们 blit 一次
800×600 线性拷贝的 0.222 ms**，说明厂商的窗口 present 很可能压根不做整帧拷贝
（其 KMD 有 overlay plane，`jmgpu_crtc_set_overlay` 只写 4 个寄存器），
而不是靠"更快的拷贝"。这条也是 B 方案（扫描出平铺）的又一条反证。


## 10. 方案 A 已实现并安装（2026-11-11）

- 补丁：`patches/xserver/0001-glamor-blit-fast-path.patch`（对 xorg-server 21.1.16 的
  `glamor/glamor_copy.c`），新增 `glamor_copy_fbo_fbo_blit()`：当拷贝是
  单 box、GXcopy、无 reverse/upsidedown/bitplane、两个 pixmap 都在单块 FBO 内、
  GL 纹理格式一致且不重叠时，直接 `glBlitFramebuffer`，否则回退原有着色器路径。
  可用 `GLAMOR_NO_BLIT=1` 关闭做 A/B。
- 构建/安装脚本：`tools/build-xglamor.sh`（下载 21.1.16 源码 → 打补丁 → meson 构建
  `libglamoregl.so` → 备份系统模块为 `libglamoregl.so.mwv207-orig` 后替换）；
  撤销：`sudo ./tools/build-xglamor.sh --rollback` 或 `/usr/local/sbin/xglamor-rollback`
  （供 VT 下自救用，无需进入仓库）。
- ABI 校验：从同一上游版本 21.1.16 构建；导出符号是系统版本的超集（系统有而我们缺的
  为 0）；未定义符号里除 libc/libepoxy/pixman/libdrm 外全部由 `/usr/lib/xorg/Xorg`
  导出。glamor 的结构体布局只受 `GLAMOR_HAS_GBM` 影响，本构建已启用。
- 当前状态：模块已装（`md5 cbf0e4af…`，原文件 `03fe0a6e…`），**需重启图形会话生效**。
- 第一轮实测（2026-11-11 16:50，重启后加载第一版补丁模块；`vblank_mode=0`，
  800×600）：swap-only **0.065 ms**（修前 0.643）、clear+swap **0.642 ms**（修前 0.927）。
  ⇒ 纯 present 已经是零拷贝；但"画了一帧再 present"仍要 0.577 ms，说明还有一次整帧拷贝。
- 原因已查明：第一版构建漏了 `-Ddri3=true`，`glamor_enable_dri3()` 没编进去
  （Xorg 日志 `modeset(0): DRI2: glamor lacks support for pixmap import/export`），
  Present 因此退回拷贝路径。已重新按发行版配置（`-Ddri2=true -Ddri3=true`）构建并安装，
  `build/include/dix-config.h` 里 `#define DRI3 1`，导出符号仍为系统版超集（无缺失）。
- 第二轮实测（2026-11-12 09:18，加载了带诊断计数的构建）：swap-only 0.016 ms、
  clear+swap 0.705 ms —— 比第一版没有改善。原因由诊断日志直接给出：
  ```
  MWV207DBG: blit rejected: nbox (nbox=4)
  MWV207DBG: shader copy #5 nbox=4 gc=0x… alu=3 pm=0xffffffff
  ```
  即 present 的 `present_copy_region()` 走 `miCopyArea` 后交给 glamor 的是 **4 个 box**
  （GC 是 scratch GC：alu=GXcopy、planemask 全 1，都满足条件），而第一版要求 `nbox == 1`，
  于是全部回退到着色器路径。**结论：瓶颈不在"用不用 blit"，而在"只支持单 box"。**
- 第三版（最终）：去掉 `nbox == 1` 限制，改为**逐 box 各做一次 `glBlitFramebuffer`**
  （上限 64 个 box），其余判定不变。已重新构建安装（md5 `73b9622834…`，
  备份 `.mwv207-orig` 不动，导出符号仍为系统版超集）。
- 待验证（重启图形会话后）：`tests/present-bench/run-present-bench.sh 800 600 300`
  期望 clear+swap 从 0.705 降到 **≈0.30–0.35 ms**（一次 900×600 面积分摊到 4 个 blit，
  每个 blit 引擎约 17 GB/s），`glmark2 -b :duration=3` 期望从 723 升到 ≈1300–1600。
  若异常：`sudo /usr/local/sbin/xglamor-rollback` 后重启会话即可回到旧模块。

## 12. 第四轮：多 box 快路径实测（2026-11-12 09:52，X 在 :0）

重启图形会话后，加载的是"逐 box blit"版本（md5 `c7fdcb66…`，含文件旗标诊断：
`/tmp/mwv207-xg-dbg` 打印计数、`/tmp/mwv207-xg-noblit` 运行时关掉快路径）。

### 12.1 快路径确实命中，且是 present 的主拷贝

`/var/log/Xorg.0.log` 每 50 次拷贝一行，示例：

```
MWV207DBG: blit=3250 shader=0 | last: BLIT 800x600 -> 1920x1080 nbox=1
```

`shader=0` ⇒ present 的每次拷贝都走了 `glBlitFramebuffer`；`nbox=1`（每次
`present_copy_region()` 只带一个 box），所以"逐 box"实现已经把画布全覆盖。

### 12.2 同一 X 会话内的 A/B（800×600，`vblank_mode=0`）

| 设置 | swap-only | clear+swap |
|---|---|---|
| blit 快路径开 | 0.036 ms | **0.485–0.567 ms** |
| blit 快路径关（`/tmp/mwv207-xg-noblit`） | — | **1.040 ms** |

⇒ 快路径把 present 拷贝成本砍掉 **1.9–2.1×**。

### 12.3 glmark2 端到端

| 指标 | 无 blit | 有 blit | 厂商栈 |
|---|---|---|---|
| `build` 场景 | 1031 FPS（0.970 ms） | **1504 FPS（0.665 ms）** | 5982 FPS |
| 全套 Score（duration=1） | 723（旧模块） | **933** | 5209 |

廉价场景（`conditionals`/`function`/`loop`）全部升到 **1025–1038 FPS（≈0.97 ms/帧）**；
重场景不变（`terrain` 115、`refract` 93、`jellyfish` 240）—— 说明提升全部来自 present。

### 12.4 剩余差距的定量分解

- 单次 800×600 blit 进 1920×1080 FBO（客户端实测）：**0.210 ms（9.1 GB/s）**，
  进 800×600 FBO 0.225 ms ⇒ **一次拷贝的硬件下限 ≈0.21 ms**。
- 现在每帧 present ≈0.5 ms，≈2.4 倍于一次 blit 的下限；`strace`/计数不足以判定
  是否两次 GPU 拷贝，但天花板已由上面这条决定：**即使把拷贝压到 1 次、0.21 ms，
  800×600 也只能到 ≈0.5 ms/帧（≈2000 FPS），仍与厂商 0.167 ms 差 3×**。
- 厂商能到 0.167 ms 说明它**几乎不拷贝**（或拷贝走 TS 压缩的平铺缓冲，带宽 38 GB/s
  ⇒ 1.9 MB 只需 0.05 ms）。我方线性目标只有 9–17 GB/s，且 TS 只分配给平铺资源。

### 12.5 结论与下一步

1. **blit 快路径应当保留**（1.9–2.1× 拷贝提速、全套 +29%），补丁与构建脚本已入库。
2. 想再接近厂商，只有两条路：
   - **扫描出平铺 + TS**：让屏幕 pixmap 也成为 supertile + TS 资源，拷贝带宽 9→约 38 GB/s；
     难点在 KMS 侧（DE 平面寄存器里没有任何平铺位，厂商 KMD 也没用），需要硬件文档或试验。
   - **overlay 平面零拷贝 present**：窗口内容走 overlay 平面直接上屏；厂商 KMD 有
     overlay 支持而我们没有，属于 DC/KMS 扩展，工作量最大但上限最高（present→0）。
3. 廉价场景现在的 0.97 ms/帧里，拷贝 ≈0.5、3D ≈0.28、其余为同步开销；即使 present 归零
   （0.167 ms 级），`build` 也只能到 ≈3000 FPS，与厂商 5982 仍差 2×，那部分属于 3D 管线效率。


## 13. overlay 零拷贝（方案 D）取证与试验平面（2026-11-12）

### 13.1 厂商的 "overlay" 其实是"大光标窗口"

`/tmp/vk/usr/src/mwv207-1.8.0.uos/`（`dpkg-deb -x ref/vendor-1.8.0/mwv207-dev_1.8.0.uos_arm64.deb`）里：

- `jmgpu_crtc_set_overlay()`（`jmgpu_package.c:202`）只在 **光标图 > 64×64**
  （`JMGPU_CURSOR_ENGINE_MAX`，`jmgpu_console.h:24`）时被走到，它把光标位置换算成
  **主帧缓冲内的地址**再写窗口寄存器 ⇒ 本质是"用 WIN 窗口画一个大光标"，不是通用平面。
- 真正的窗口编程在 `jmgpu_plane_set_cursor_win0()`（`jmgpu_console.c:401`），它写的是
  `J9_NEUROPARALYTIC(chan)` 基址的一组寄存器：地址 `0x050 = fbaddr>>6`、步长 `0x068 =
  pitch<<16 | pitch`、尺寸 `0x0E4/0x00C`、位置 `0x010`、使能 `0x02C = 1<<chan`。
  这组寄存器支持**任意线性缓冲**，因此可以当作通用 overlay 平面使用。
- `DRM_PLANE_TYPE_OVERLAY` 在厂商 KMD 里是 **空分支**（`jmgpu_console.c` 的 `switch
  (plane->type)` 里 `case DRM_PLANE_TYPE_OVERLAY: break;`）⇒ 厂商也没把 WIN0 暴露成
  DRM overlay 平面，X 的 modesetting DDX 只用 PRIMARY/CURSOR。

### 13.2 我方试验平面（默认关闭）

`src/kernel-6.6/dc/mwv207_va.c` 新增（`module_param_named(overlay, …, bool, 0644)`，
默认 0）：

- `struct mwv207_va.overlay` + `overlay_formats[] = { ARGB8888, XRGB8888 }`；
- `mwv207_overlay_atomic_update()` 按上面的窗口寄存器布局编程 WIN0（地址/步长/尺寸/
  位置/使能），`mwv207_overlay_atomic_disable()` 清 `0x02C`；
- 仅当 `mwv207.overlay=1` 加载模块时才 `drm_universal_plane_init(... DRM_PLANE_TYPE_OVERLAY)`。

`tests/overlay/overlay-test.c`（`cc -O2 -o /tmp/overlay-test tests/overlay/overlay-test.c
-I/usr/include/libdrm -ldrm`）用 `drmModeSetPlane` 往该平面放一块纯色/半透明缓冲，
用来判定窗口是否真的上屏。

### 13.3 结论

- 硬件确有可编程窗口（格式/alpha/线性或 TILE4X4 模式，`OVERLAY_CONFIG 0x04B0` 的
  `FORMAT/MODE/ALPHA_*` 字段），但**语义未文档化**，且它同时被"大光标"路径占用；
- **即使平面能用，X 的 modesetting DDX 也不会用它**（它只翻 PRIMARY）；所以"overlay
  零拷贝 present"要真正收益，需要自写 DDX 或让合成器（kwin）用该平面，工作量远大于
  一个 KMD 平面注册；
- 因此本方案只作为**能力原型**保留（默认关闭、可随时弃用），不计入 present 收益。

## 11. 复现命令

```sh
export DISPLAY=:1 XAUTHORITY=/home/admin/.Xauthority
vblank_mode=0 glmark2 -b build:duration=3              # 1049 FPS
vblank_mode=0 glmark2 -b build:duration=3 --off-screen # 3607 FPS（无 present）
vblank_mode=0 glmark2 -b build:duration=3 --frame-end=finish  # 2447 FPS
qdbus org.kde.KWin /Compositor suspend && vblank_mode=0 glmark2 -b build:duration=3
```

`/tmp/sw.c`（只 swap）、`/tmp/fill2.c`（采样带宽，含 stride 扫描）、
`/tmp/blit.c`（blit 带宽）、`/tmp/blittest.c`（FBO→FBO blit）为本次临时工具，未入库；
需要时可按第 2/6 节表格重写。

## 14 2D 引擎（方案 A2）现状：opcode 编码在闭源库，尚未复刻

### 14.1 我方 KMD 的 2D 通路是完整的

- `mwv207_pipe_2d_create(jdev, 0, 0x968000, "mwv207_2d0")`，基址与厂商
  `J9MATHS_BANDCUTTER (0x968000)` 一致；环形缓冲、`0x89000000` 链接、
  `write(0x4C00, 1)` 触发、`0x80000000` NOP 都与厂商
  `j9_gelatinizable()` 一一对应。
- 段头布局经厂商源码 `jmgpu_accessible.c:46-52` 的
  `struct tag_jms_wl_header { u32 dont_care0; u32 byte16_cnt; … }` 证实：
  计数在**第二个**字，与我们的 `pipe_2d_submit()` 相同。此前"厂商把
  计数写在第一个字"的猜测被证伪（把计数前移还会破坏 16 字节对齐，
  KMD 直接返回 `-EIO`），实验已回退（`807fe4b`）。

### 14.2 缺的是 draw opcode

- 抓包（166 个命令流，厂商 DDX + `libdrm_jmgpu.so`）得到两种绘制命令：
  - `0x82000054` + 4 字：填充 `[?, dst_addr, dst_desc, (w<<16|h)]`
  - `0x8200001c` + 7 字：拷贝 `[0x000200f0, dst, dst_desc, wh, src, src_desc, wh]`
- 但把这两种命令原样送进我们的 2D 环：提交成功、fence 正常置位，
  **目标缓冲全 0**。描述字取遍 `0 / pitch / pitch/8 / pitch/64 /
  0x00010080 / 0x00040600 / 0x00050680` 等候选仍无输出
  （`tests/j2d-copy/j2dtest.c`，候选已入档 `d43628e`）。
- 原因：这些 opcode 由闭源 `libdrm_jmgpu.so`（1.5 MB、符号未 strip，
  导出 `jmo_2D_Clear/Blit/Construct/Commit` 整套 API）生成，DDX 只是
  调用者（二进制里查不到这两个立即数，`.text` 里也没有 `0x82000000`）。

### 14.3 结论

方案 A2（用 2D 引擎做 present 拷贝）要真正落地，需要先反汇编
`libdrm_jmgpu.so` 解出 opcode 组合规则（或伪造其 device/surface 结构、
直接调 API 生成样本做差分）。这是独立的一块逆向工作，不属于"再调几个
参数"的范围。当前 present 收益仍以 14.1 之外的 glamor blit 快路径为准
（1.9–2.1×，见 §12）。


## 15. 2026-11-12：AIGLX 软链补齐后 glamor 走硬件 2D，present 再降 2×

### 15.1 发现

`/var/log/Xorg.0.log` 一直有：

```
(EE) AIGLX error: dlopen of /usr/lib/aarch64-linux-gnu/dri/mwv207_dri.so failed (No such file or directory)
(EE) AIGLX: unable to load driver mwv207
```

Xorg 的 AIGLX 只在 `/usr/lib/aarch64-linux-gnu/dri` 找驱动，而我们的 DRI 实体
在 `/usr/local/lib/mwv207-oss/`（客户端经 glvnd/`LIBGL_DRIVERS_PATH` 走
`/usr/local/lib/aarch64-linux-gnu/dri/mwv207_dri.so` 软链）。缺这一条时
glamor 只能退回 llvmpipe → `Refusing to try glamor on llvmpipe` →
`ShadowFB` 软件合成（**每帧 CPU 拷贝整窗**）。

`switch-mwv207.sh` 的 `AIGLX_LINK` 已经覆盖这条软链（`umd_deploy_files`
里 `ln -sfn "$INST_DIR/mwv207_dri.so" "$AIGLX_LINK"`），补上后：

```
(II) modeset(0): glamor X acceleration enabled on MWV207 (JingjiaMicro Corporation)
(II) modeset(0): glamor initialized
(II) AIGLX: Loaded and initialized mwv207
```

### 15.2 实测（800×600，`vblank_mode=0`，同一个 `tests/present-bench`）

| 阶段 | swap-only | clear+swap |
|---|---|---|
| 原始（glamor 在 llvmpipe） | 0.643 ms | 0.927 ms |
| + glamor blit 快路径（§12） | 0.036 ms | 0.485–0.567 ms |
| **+ AIGLX 软链（本节）** | **0.016 ms** | **0.248 ms** |
| 厂商栈 | — | 0.167 ms |

1920×1080 clear+swap：**1.328 ms**（厂商同条件未测单帧，`build` 场景 0.167 ms 是 800×600）。

`glmark2 -b :duration=1`：**Score 1225**（今天上午 933、原始 723，厂商 5209）。
廉价场景 `conditionals` 1230–1254、`function` 1252–1419、`loop` 1423–1745 FPS
（上午 1025–1038）；重场景不变（`terrain` 118、`refract` 94、`jellyfish` 248）。

### 15.3 结论

- 800×600 的一次全窗合成拷贝从 0.9 ms 降到 0.248 ms（3.7×），与厂商 0.167 ms 只差 1.5×。
- 剩余差距集中在**重场景**（GPU 本身）与**多窗合成**（`desktop` 467–488 FPS），
  不再是 present 的固定开销。
- 这条修复不需要改代码，只要保证安装流程建好 `AIGLX_LINK`（脚本已覆盖），
  以及**装完载荷后重启 X**（glamor 在启动时选 EGL provider）。

## 16. 每 draw 的 CPU 开销：着色器指纹缓存（2026-11-12）

### 16.1 测量

`/tmp/drawcpu`（2000 次相同 `glDrawArrays`，无 clear / 无 swap，`vblank_mode=0`）
的提交侧耗时：**0.86–1.04 µs/draw**，而 `glFinish` 尾部只有 ~0.005 ms ⇒ 瓶颈在
用户态驱动。`strace -f -c -e trace=ioctl` 显示 2000 次 draw 只有 1165 次 ioctl
（合计 1.5 ms），所以不是"每 draw 一次内核往返"。

在 `mwv207_draw_vbo()` 与 `mwv207_config_render_piepline()` 里临时插入
`clock_gettime`（环境变量门控，测完已删除）得到：

```
[DRAWTIME] cfg=1.09 us/draw, sub=0.09 us/draw
[CFG] dualblend=0.04 shader=0.76 vertex=0.08 other=0.06 draw+rest=0.06 us/draw
```

⇒ `mwv207_update_shader()` 独占 **0.76 µs / 1.09 µs（70%）**：它每次绘制都要

1. 按 `PIPE_MAX_SHADER_SAMPLER_VIEWS` × `PIPE_SHADER_TYPES` 重算 FNV 指纹
   `fp_tex_128bit[st]`（单次 ≈200 次哈希），
2. 重算 `fp_z32_shadow[st]` 与 `fp_pixel_128bit`，
3. 再用这些指纹去 `shader_list` 里查 VS/FS/GS/TCS/TES 五份缓存。

### 16.2 修法

给这三组指纹加"脏"标记（`struct mwv207_context::fp_dirty`）：

- `mwv207_set_sampler_views()`、`mwv207_bind_sampler_states()`、
  `mwv207_set_framebuffer_state()` 置脏；
- `mwv207_update_shader()` 只在脏时重算，算完清脏；
- `mwv207_split_128bit()` 之后也置脏（拆分把视图换成新纹理，指纹必须失效）。

### 16.3 结果

| 指标 | 修前 | 修后 |
|---|---|---|
| 驱动内 config 阶段 | 1.09 µs/draw | 0.41 µs/draw |
| 其中 `update_shader` | 0.76 µs/draw | 0.08 µs/draw |
| 端到端提交（`/tmp/drawcpu`，去掉插桩后） | 0.86–1.04 µs/draw | **0.17 µs/draw** |

正确性对照：`/tmp/swapsh`（同一帧内交替绑定 RGBA8/RGBA32F 纹理、切换
RGBA8/RGBA32F FBO 附件，读回 24 次绘制结果做哈希）在旧/新驱动下输出
**完全相同**（`md5like=0b12ed05`）；`tests/query/run-query-tests.sh` 4/4 PASS；
`glmark2 -b :duration=1` 939 → 943（±0.5%，因为这些场景是 GPU-bound，不吃
每 draw 开销）。

结论：这条改的是**多小 draw 场景（浏览器/UI/桌面合成）**的 CPU 上限，
`glmark2` 这种以重场景为主的基准看不出收益。

## 17. 修后整体测量与剩余瓶颈归因（2026-11-12）

同一 `:0` 会话、`vblank_mode=0`：

| 项目 | 修前/安装栈 | 修后（dev 驱动） | 说明 |
|---|---|---|---|
| `glxgears` | 5894 FPS | **6737 FPS** | 每 draw CPU 开销受益 +14% |
| `glmark2 -b :duration=1` 总分 | 939 | **1205** | 厂商 5209 |
| `conditionals` 廉价场景 | 1017 | **1309–1348** | 已接近 present 下限 |
| `shadow` | 673–823 | 777 | 厂商 1241 |
| `terrain` | 109 | 115 | 厂商 155 |
| `refract` | 88 | 93 | 厂商 123 |
| `jellyfish` | 238 | 245 | 厂商 1175 |

单项能力实测（dev 驱动）：

- `glClear`（800×600，fast-cleared 走 TS）：**0.004 ms** ⇒ clear 不是瓶颈。
- `glXSwapBuffers`（800×600 窗口，`tests/present-bench` swap-only）：**0.257 ms**；
  clear+swap **0.659 ms**（同一会话内会随窗口是否可见/被遮挡波动，§15 那次是 0.248 ms）。
- 顶点吞吐（`/tmp/vtxbench`，30k 顶点 × 100 帧，64×64 FBO 无填充压力）：**515 Mvert/s**。
- 缓冲上传（`/tmp/upbench`，4 MB `glBufferSubData` 与 `GL_MAP_INVALIDATE` 映射）：**≈4.9 GB/s**（两种写法一致）。
- 纯填充（`/tmp/fill`）：1920×1080 const **839 Mpx/s**；alu64 **2.8 Gops/s**。

归因：

1. **廉价场景现在受 present 限制**：1300 FPS = 0.76 ms/帧，其中 present ≈0.66 ms。
   厂商同场景 7589 FPS（0.13 ms）说明它整帧不拷贝（flip 或 overlay）。
   要让 X 走 flip 需要 DDX 支持（`modesetting` DDX 只翻 PRIMARY，且合成器未用 overlay），
   不是 UMD 内能解决的，属结构性差距。
2. **重场景（terrain/refract）差 1.35×，与填充率/ALU 吞吐同量级**，属 GPU 能力。
3. **jellyfish 差 4.8×**：该场景每帧大量小批次绘制 + 顶点着色器，参考数字
   515 Mvert/s、4.9 GB/s 上传，怀疑是顶点吞吐与每批次固定开销叠加，
   下一步可对"每批次固定开销"再做一次插桩（本次未做）。
4. `[buffer]` 场景（subdata 475 / map 644 FPS）主要吃上传带宽，4.9 GB/s 已接近
   本机 DRAM 写带宽量级。

## 18. 为什么"廉价场景/重场景"仍慢：窗口缓冲是线性表面（2026-11-12）

### 18.1 先否掉"向量超越函数"这条

`nir/mwv207_opcode.c` 里 `fsin/fcos/frcp/frsq/flog2/fexp2/fsqrt` 都只有一个操作码
（`0x22/0x23/0xC/0xD/0x12/0x11`），源操作数形式统一是 `SRC_X_X_0`（单个标量 X），
`emit` 里对这些 op 还会强制 `swiz_scalar`。也就是说这套 ISA 的超越函数就是
**单 lane** 执行的，没有 vec4 形式可放行；`mwv207_alu_to_scalar_filter_cb` 把它们
列进标量化名单是**正确的**，不是遗漏。⇒ 这条路没有收益，不做。

### 18.2 真正的原因：窗口 drawable 是线性、无 TS

同一段"全屏三角形 + 平凡片元着色器"（`/tmp/wfill2.c`，800×600，`glFinish`）：

| 渲染目标 | 每帧 |
|---|---|
| 离屏 FBO（tiled + TS） | **0.124 ms** |
| 窗口 front/back buffer（linear，无 TS） | **0.949 ms**（含 swap 0.257 ms） |

⇒ PE 写线性窗口表面约 **5.7×** 于写平铺+TS 表面。这与 §9 的带宽测量一致
（PE 写平铺无 TS 12.8 GB/s、写线性 6.7 GB/s，TS 再乘 2）。

`glmark2` 的 jellyfish 每帧把整个窗口重绘一遍（还带混合读），所以它的差距
几乎全部来自这里：

| 场景 | on-screen | off-screen | 差值 |
|---|---|---|---|
| jellyfish | 4.39 ms（228 FPS） | 1.15 ms（873 FPS） | +3.24 ms |
| terrain | 8.6 ms | 8.3 ms | +0.3 ms |
| refract | 10.5 ms | 9.8 ms | +0.7 ms |
| shadow | 1.25 ms | 0.87 ms | +0.38 ms |
| desktop | 2.3 ms | 1.8 ms | +0.5 ms |

旁证：`glmark2 --visual-config samples=4` 时它拿不到多重采样配置、退化成
"渲染到 FBO 再上屏"，同一场景立刻从 228 涨到 **360 FPS**。

### 18.3 结论与可选路径

1. **窗口缓冲改平铺（B 路径）**：UMD 侧只需放开 `mwv207_can_tile()`
   里 `enable_scanout_tile = false`（`mwv207_resource.c`），但**前提是显示端能扫描出
   平铺/带 TS 的帧缓冲**：当前 KMD 的 plane 只声明 `DRM_FORMAT_MOD_LINEAR`
   （`src/kernel-6.6/dc/mwv207_va.c:68`）且主平面更新不写任何 tile 位
   （与厂商 KMD 一致，见 §8）。要做必须先在 KMD 里试探 DE 的
   `SUPERTILE_VERSION`/tile 尺寸并验证显示正确性，风险是花屏。
   建议用 `mwv207.overlay=1` 的 WIN0 试验平面做这项试探（只影响 overlay，坏了不黑主屏）。
2. **驱动内"先渲染到平铺 FBO 再 blit 上屏"**：等价于在 UMD 里做一层小型合成器，
   代码量和风险都远大于收益，暂不做。

因此 present/廉价场景的剩余差距属**结构性**，不是可以通过调参或补一条快路径解决的。
