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

## 6. 复现命令

```sh
export DISPLAY=:1 XAUTHORITY=/home/admin/.Xauthority
vblank_mode=0 glmark2 -b build:duration=3              # 1049 FPS
vblank_mode=0 glmark2 -b build:duration=3 --off-screen # 3607 FPS（无 present）
vblank_mode=0 glmark2 -b build:duration=3 --frame-end=finish  # 2447 FPS
qdbus org.kde.KWin /Compositor suspend && vblank_mode=0 glmark2 -b build:duration=3
```

`/tmp/sw.c`（只 swap）、`/tmp/fill2.c`（采样带宽，含 stride 扫描）、
`/tmp/blit.c`（blit 带宽）为本次临时工具，未入库；需要时可按第 2 节表格重写。