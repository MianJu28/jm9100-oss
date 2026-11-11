# glmark2：厂商栈实测 vs 开源栈记录（2026-11-11）

> 目的：用户要求把厂商驱动（`jmgpu` 1.8.0）的 glmark2 实测与开源修复驱动对比。
> 本次只测了**厂商栈**（机器当时在 vendor 态）；开源栈数字来自仓库里已有的两次记录，
> 条件不完全相同，表下已注明。逐场景原始输出：`reports/evidence-glmark2/`。

## 一、测量条件

| 项 | 厂商栈（本次实测） | 开源栈（历史记录） |
|---|---|---|
| 内核/UMD | `jmgpu` 1.8.0 + `jmgpu_dri.so` / `jmgpu_drv_video.so` | `mwv207` KMD（`8DFC520F…`）+ Mesa 24.1.2 `libmwv207_dri.so` |
| 命令 | `glmark2 -b :duration=3`（默认窗口 800×600） | `glmark2 --size 800x600`（默认 duration） |
| 后台负载 | Firefox（本会话 GUI）在跑 | 记录 A：46 进程在跑（含 Firefox）；记录 B：干净环境 |
| Xorg 合成 | 厂商 DDX `mwv207_drv.so` + **EXA 硬件 2D** | `modesetting` + glamor **失败**（`Refusing to try glamor on llvmpipe`）→ **ShadowFB 软件 2D** |
| 日期 | 2026-11-11 13:55 | 2026-11-02 / 2026-10-21 |

## 二、总分

| | 厂商栈 | 开源栈 |
|---|---|---|
| glmark2 Score | **5209** | **1125**（2026-11-02，44 场景）／**851**（2026-10-21，干净环境） |
| glxgears（vsync 关，`vblank_mode=0`） | **15134 / 17621 FPS** | **1808 FPS**（2026-10-21 同一命令） |

## 三、逐场景（同场景、不同次运行；数字为 FPS）

| 场景 | 厂商栈 | 开源栈 | 倍数 |
|---|---|---|---|
| `build`（use-vbo true/false） | 14856 / 5982 | 1362~1392 | **10.7× / 4.3×** |
| `texture` | 5986 / 5788 / 6506 | 1436~1529 | **4.2×** |
| `shading` | 6855 / 6917 / 6494 / 6710 | 1248~2069 | **3.7×** |
| `bump` | 3701 / 11682 / 10286 | 1545~3372 | **2.4×** |
| `conditionals` | 7589 / 6126 / 7580 | 989~992 | **7.6×** |
| `jellyfish` | 1175 | 229（干净环境） | **5.1×** |
| `shadow` | 1241 | 658 | **1.9×** |
| `terrain` | 155 | 114~118 | **1.3×** |
| `refract` | 123 | 98~99 | **1.25×** |
| `glxgears`（无 vsync） | 15134 | 1808 | **8.4×** |

## 四、结论（可定量）

1. **GPU 吞吐本身没有差到这个程度**：纯 GPU-bound 的 `terrain` / `refract` 只差
   25–35%，与 `reports/PERFORMANCE-REPORT.md` 测得的硬件光栅上限（~48–53 Gpx-eq/s）一致。
2. **差距集中在"便宜帧"**：`build`/`texture`/`conditionals`/`glxgears` 差 4–10 倍，
   而它们每帧的 GPU 工作量极小 ⇒ 是**每帧固定开销**，不是逐像素能力。
3. **固定开销约 0.5 ms/帧**：`glxgears` 开源栈 1808 FPS = 0.553 ms/帧，厂商 15134 FPS
   = 0.066 ms/帧，差 **≈0.49 ms/帧**；`build` 场景 0.73 vs 0.17 ms（差 0.57 ms）同量级。
4. **最可能的来源是 Xorg 的软件 2D 路径**：开源栈的 Xorg 日志是
   `modeset(0): Refusing to try glamor on llvmpipe` → `glamor initialization failed` →
   `ShadowFB`，即每帧都要在 CPU 里把 800×600×4 ≈ 1.9 MB 合成/拷贝一次；按 ~4 GB/s
   的 memcpy 吞吐正好 ≈0.48 ms，与上面的差值吻合。厂商栈用 EXA（硬件 2D），没有这笔开销。
5. 这解释了为什么"重场景差得少、轻场景差很多"，也解释了 `reports/PERFORMANCE-REPORT.md`
   里"合成器挂起可给简单场景 +66%"的旧观测——那是同一个瓶颈的另一面。

## 五、下一步（待切回开源栈时执行）

1. 用**同一条命令**复测开源栈：`glmark2 -b :duration=3`，同时抓
   `glxgears`（`vblank_mode=0`）与 `/var/log/Xorg.0.log` 的 glamor 结论 —— 才能把
   "历史 1125/851 vs 5209" 变成严格同条件对比。
2. 查 glamor 为什么落到 llvmpipe：
   - `/usr/share/glvnd/egl_vendor.d/10_mwv207.json` 是否指向 `libEGL_jm.so.0`；
   - `eglinfo` / `es2_info` 在开源栈下的 vendor 是否为 `Jingjia`；
   - 我方 `libEGL` 是否暴露 `EGL_EXT_platform_device`（glamor 走
     `eglGetPlatformDisplay(EGL_PLATFORM_DEVICE_EXT)`）。
   若 json 在切栈时被写成厂商库（切到 vendor 时 `switch-mwv207.sh` 会把该 json 改成
   `libEGL_mwv207.so.0`），开源栈的 Xorg 就会退到 Mesa 的 `50_mesa.json` → llvmpipe。
3. glamor 修好后重测：预期简单场景提升接近"每帧省 0.5 ms"（`glxgears` 1808 → 数千、
   `conditionals` ≈1k → 5–7k），`terrain`/`refract` 不变。

## 六、复现命令

```bash
# 厂商栈（本次）
DISPLAY=:0 XAUTHORITY=/home/admin/.Xauthority glmark2 -b :duration=3
DISPLAY=:0 XAUTHORITY=/home/admin/.Xauthority vblank_mode=0 glxgears -info

# 开源栈（切回后，与上面逐字相同）
DISPLAY=:0 XAUTHORITY=/home/admin/.Xauthority glmark2 -b :duration=3
```
