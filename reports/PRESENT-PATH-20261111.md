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

## 4. 可选的改法（按代价排序，均未实施）

1. **让被采样的窗口 pixmap 走平铺**（保持屏幕 pixmap 线性、扫描出仍线性）——
   收益按 2.3 推算：present 0.55 → ≈0.25 ms，`build` 0.95 → ≈0.6 ms。
   风险：`PIPE_BIND_SCANOUT` 与 `PIPE_BIND_RENDER_TARGET` 在 Mesa 里同时置位，
   无法从 bind 位区分"会真被 flip 的缓冲"；需要按 `drw->is_different_buffer` /
   DRI3 的 flip 支持另开判据。
2. **扫描出也走 supertile**（`enable_scanout_tile = true`）——硬件具备：
   厂商头文件有 `MWV207REG_DE_GENERAL_CONFIG_SUPERTILE_VERSION`（V1/V2/V3）
   与能力位 `JM_MINOR_FEATURES4_DE_SUPERTILE_AVAILABLE`；但**我方 KMD 的 dc/
   从不编程 DE supertile**，直接打开会让显示输出花屏。需要 KMD 侧先把 DE
   supertile 版本/布局对齐 UMD 的 4 KB 平铺格式，属中等工程。
3. **不改驱动，改合成路径**：让桌面开合成（kwin 当前是关的）并不会更快——
   合成同样要采样线性窗口纹理；除非把窗口全屏化触发 direct scanout / flip
   （实测全屏 + 无合成时 `present` 变成 vblank 锁定的 9.9 ms/帧，即真 flip）。
4. **仅记录**：present 协议本身的固定开销只有 0.11 ms，没有可压的余地。

## 5. 复现命令

```sh
export DISPLAY=:1 XAUTHORITY=/home/admin/.Xauthority
vblank_mode=0 glmark2 -b build:duration=3              # 1049 FPS
vblank_mode=0 glmark2 -b build:duration=3 --off-screen # 3607 FPS（无 present）
vblank_mode=0 glmark2 -b build:duration=3 --frame-end=finish  # 2447 FPS
qdbus org.kde.KWin /Compositor suspend && vblank_mode=0 glmark2 -b build:duration=3
```

`/tmp/sw.c`（只 swap）、`/tmp/fill2.c`（采样带宽，含 stride 扫描）、
`/tmp/blit.c`（blit 带宽）为本次临时工具，未入库；需要时可按第 2 节表格重写。