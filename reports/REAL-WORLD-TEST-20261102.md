# 真实应用可用性测试报告（2026-11-02）

测试对象：开源 mwv207 栈 —— 用户态 `24.1.2-fullstack-r5`（UMD，`git-b2ab2c9da2`）
+ 内核态自建 KMD `D507FC2774D547E7BEBA182`（社区 6.6 移植 + 我们的 K1/K3–K8/K10/0008 + N15）。
测试环境：`DISPLAY=:0`，已安装栈，**零环境变量覆盖**（与用户桌面完全一致的环境）。

---

## 0. 结论

| 能力面 | 结论 | 关键证据 |
|---|---|---|
| 内核态模块 | **自建版已在跑** | `srcversion = D507FC2774D547E7BEBA182`，`modinfo -n` 指向 `updates/dkms/` |
| 内核稳定性 | **无 GPU 崩溃** | 全 dmesg 无 `kernel BUG at`/`Oops`/`Call trace` 里的 GPU 帧（上次失败 boot 是 16 连崩） |
| 桌面合成 | 正常 | `glamor X acceleration enabled on MWV207`；46 个进程加载我们的库 |
| OpenGL | 正常 | GL 4.0 / GLSL 4.00 / GLES 3.2 |
| 性能 | 正常 | glmark2 **1125**；glxgears 110.6 FPS |
| 视频硬解 | 正常 | VA-API HEVC **2.1×** 提速；mpv GPU 输出画面在动 |
| 计算 (OpenCL) | 正常 | rusticl 上 kernel 跑通、结果逐元素正确 |
| 条件渲染（新实现） | 正常 | 自建 5 用例 5/5 + glmark2 `conditionals` 场景 989~992 FPS |
| 遗留瑕疵 | 2 处非阻塞 | 厂商 EGL ICD 仍启用（实际未生效）；Xorg 服务端 AIGLX 路径未覆盖 |

---

## 1. 内核态

```
$ cat /sys/module/mwv207/srcversion
D507FC2774D547E7BEBA182            # = 我们构建的版本（含 N15 空 entity 修复）

$ modinfo -n mwv207
/lib/modules/6.6.155-arm64-desktop-hwe/updates/dkms/mwv207.ko

$ lsmod | grep mwv207
mwv207               118784  16
```

dmesg 加载序列（`[3.5xx]`，无异常）：

```
mwv207: loading out-of-tree module taints kernel.
mwv207: module verification failed: signature and/or required key missing - tainting kernel   ← 预期（本地自签）
[drm] mwv207: 2048M of VRAM memory size
mwv207_pipe_dma_reset todo        ← 驱动自带 TODO 打印，非错误
mwv207_edp_init TBD                ← 同上（eDP 未实现）
[drm] number of crtc: 2 / encoder: 4 / connector: 4
[drm] Initialized mwv207 0.1.0 20220407 for 0000:07:00.0 on minor 0
mwv207 0000:07:00.0: [drm] fb0: mwv207drmfb frame buffer device
```

**对比失败那次 boot（`b97d4342`）**：当时 16 次 `kernel BUG at drivers/gpu/drm/scheduler/sched_main.c:678`
（`drm_sched_job_arm` 的 `BUG_ON(!entity)`，零重定位 submit 未初始化 job）。
本次启动同一 GL/桌面负载下**零次**，N15 修复生效。

`./switch-mwv207.sh kmd verify` → `[OK] 正在跑自建版`，`/dev/dri/card0`、`renderD128` 齐全。

KMD patch 0008（`MODULE_SOFTDEP("pre: governor_performance")`）也已生效：

```
governor_performance    12288  0
/sys/class/devfreq/devfreq_mwv207_2d0/governor = performance   cur_freq 1200 MHz
/sys/class/devfreq/devfreq_mwv207_3d0/governor = performance   cur_freq 1000 MHz
```

---

## 2. 用户态回归（重启后重编探针）

| 项 | 结果 |
|---|---|
| `DRI_DIR=/usr/local/lib/aarch64-linux-gnu/dri tests/query/run-query-tests.sh` | **PASS=4 FAIL=0** |
| `tests/query/condrender-correct` | **PASS**（5 用例全过） |
| `glxinfo -B` | `Max core profile version: 4.0`、`Video memory: 1024MB`、`OpenGL ES 3.2 (git-b2ab2c9da2)`、`GLSL ES 3.20` |

---

## 3. 真实应用

### 3.1 桌面本身

- 合成器 `/usr/bin/kwin_x11 --replace` 加载我们的 `libGL`/`mwv207_dri.so`；
- **46 个进程**（`grep mwv207-oss /proc/*/maps`）在用我们的栈：`dde-shell`、`dde-lock`、
  `dde-file-manager-daemon`、`deepin-service-manager`、`kglobalacceld`、`mihomo-party`、
  `AskLink`、`QwenWorkCN`、`firefox-bin` 等；
- Xorg：`glamor X acceleration enabled on MWV207 (JingjiaMicro Corporation)`、
  `[DRI2] DRI driver: mwv207`、`VDPAU driver: mwv207`、客户端侧 `Using DRI3 for screen 0`；
- 显示器 HDMI-1：1920×1080 @ **100.00 Hz**（另有 60/75/50 可选）。

### 3.2 glxgears

```
554 frames in 5.0 s = 110.6 FPS
renderer: JingjiaMicro / MWV207
扩展串含 GL_KHR_robustness、GL_ARB_ES3_2_compatibility、
      GL_KHR_blend_equation_advanced、GL_ARB_conditional_render_inverted
```

### 3.3 glmark2（第三方 GL 基准 + 压力）

```
glmark2 --size 800x600      →  Score: 1125      （44 个场景全部跑完，0 error/0 warning）
  [build]            FPS 1362 ~ 1392
  [texture]          FPS 1436 ~ 1529
  [shading]          FPS 1248 ~ 2069
  [bump]             FPS 1545 ~ 3372
  [terrain]          FPS 114
  [shadow]           FPS 658
  [refract]          FPS 99
  [conditionals]     FPS 989 ~ 992     ← 真走了 GL_NV_conditional_render 路径
```

`[conditionals]` 场景本身就是条件渲染 + 分支着色器，能跑完并拿到 ~990 FPS，
是对 0015（条件渲染 CPU 侧实现）的第三方交叉验证。

### 3.4 视频硬解（VA-API）

`vainfo` → `Mesa Gallium driver 24.1.2 for MWV207`，`VAEntrypointVLD`：

```
VAProfileH264ConstrainedBaseline / H264Main / H264High / H264High10
VAProfileHEVCMain / HEVCMain10
```

ffmpeg `-hwaccel vaapi -hwaccel_output_format vaapi` 的 debug 日志确认硬件路径：

```
Initialised VAAPI connection: version 1.22
VAAPI driver: Mesa Gallium driver 24.1.2 for MWV207 (JingjiaMicro Corporation).
Format vaapi chosen by get_format().
Format vaapi requires hwaccel initialisation.
```

1080p30 × 10 s（300 帧）解码耗时：

| 编码 | 软解 | VA-API 硬解 | 比值 |
|---|---|---|---|
| H.264 High | 0.898 s | 0.920 s | ≈1.0×（片源太简单，解码不是瓶颈） |
| HEVC Main | 1.785 s | 0.854 s | **2.1×** |

VDPAU（G3DVL 1.0）同样可用：`H264_BASELINE/MAIN/HIGH/CONSTRAINED_BASELINE` 均支持，8192×8192。

### 3.5 mpv 真实窗口播放（`--vo=gpu --hwdec=vaapi`）

- 5 秒播放 rc=0、无警告；
- 抓屏两帧间隔 1 秒做 PSNR：全屏 **24.24 dB**、只截播放器窗口区（中心 960×540）**27.85 dB**
  → 画面确实在动、且确实由 GPU 输出（不是黑窗或静态帧）。

### 3.6 OpenCL（rusticl）

`/tmp/cl-test.c`：4096 元素向量加法 kernel + 逐元素校验。

```
platform = rusticl
device   = MWV207 (JingjiaMicro Corporation)
version  = OpenCL 3.0
kernel 结果: 正确（全 4096）
OPENCL: PASS
```

`clinfo`：2 个 compute unit。

### 3.7 压力测试

glxgears 连续 25 s 与 `ffmpeg -hwaccel vaapi` 循环解码 25 s **并行**跑完：
dmesg 无任何 GPU 相关崩溃/重置，之后 `glxinfo` 仍 GL 4.0。

---

## 4. 发现的遗留问题

### 4.1 厂商 EGL ICD 仍启用（低危，建议清理）

```
/usr/share/glvnd/egl_vendor.d/10_mwv207.json  → "library_path": "libEGL_mwv207.so.0"
                                               （= /usr/lib/aarch64-linux-gnu/mwv207/libEGL_mwv207.so.1.5.0，闭源）
/usr/share/glvnd/egl_vendor.d/50_mesa.json    → libEGL_mesa.so.0（系统 Mesa）
```

- 厂商 ICD 所需的 `jmgpu.ko` **从未加载**（`lsmod | grep -c jmgpu` = 0），这条 ICD 必然初始化失败；
- 我们的开源 EGL 是 `libEGL_mwv207.so.1.0.0`（SONAME `libEGL_mwv207.so.1`），与厂商的 `.so.0` 名字不冲突；
- Firefox 主进程因此同时映射了厂商 `libEGL_mwv207.so.1.5.0`、系统 `libEGL_mesa.so.0.0.0`
  与我们的 `libGLX_mwv207.so.1.2.0`/`mwv207_dri.so`；
- **实测未造成故障**：`es2_info` → `EGL_VENDOR: Mesa Project`、`eglinfo` → ES 3.2 / GL 4.0，
  说明 EGL 客户端最终仍走我们的 Mesa 栈，厂商那条只是被加载后放弃。

建议（待确认）：把 `10_mwv207.json` 改名为 `.disabled`，让 EGL 只列 `50_mesa`。

### 4.2 Xorg 服务端 AIGLX 路径未覆盖（无害）

```
(EE) AIGLX error: dlopen of /usr/lib/aarch64-linux-gnu/dri/mwv207_dri.so failed (... No such file or directory)
(EE) AIGLX: unable to load driver mwv207
```

我们把 DRI 装在 `/usr/local/lib/aarch64-linux-gnu/dri/`，而 X 服务端编译期路径是 `/usr/lib/...`。
只影响服务端间接渲染（AIGLX），客户端 DRI3 直接渲染正常（glamor 已启用）。
如需消除该报错，可在 `/usr/lib/aarch64-linux-gnu/dri/` 加一条指向我们 DRI 的软链。

### 4.3 与显卡无关的内核缺陷（顺带发现）

本次启动 956 s 处出现一次内核 BRK（`Internal error: BRK handler: 00000000f2000006`）：

```
CPU: 4 PID: 9069 Comm: Cache2 I/O Tainted: G OE
pc : 0xffff800085d45054
lr : __kretprobe_trampoline+0x0/0xc8
Call trace: __arm64_sys_close+0x3c → invoke_syscall → el0_svc
```

- 崩溃栈里 **0 个 GPU/DRM 帧**（`mwv207` 只出现在 "Modules linked in" 清单里）；
- 触发者是 Deepin 的 `vfs_monitor`（VFS change monitor，用于文件索引）在 `close()` 上挂的 kretprobe；
- 由 Firefox 网络缓存线程（`Cache2 I/O`）的文件 IO 触发，与显卡栈无关。
- 若复现频繁，可临时 `modprobe -r vfs_monitor` 或加黑名单。

---

## 5. 仍未决的能力项（与本次测试无关，历史遗留）

- 16 位格式声明 A/B/C 决策（`docs/UNRESOLVED.md` C1/C3）；
- 高级混合 8/15 是否保留超额声明（C2）；
- 路径 B 能力提升（m01903 / m02002）；
- m02560 功能缺口清单。

---

## 6. 复现命令

```bash
export DISPLAY=:0

# 内核态
cat /sys/module/mwv207/srcversion
/home/admin/jm9100-oss/switch-mwv207.sh kmd verify
sudo dmesg | grep -E 'kernel BUG at|Oops|Call trace'

# 用户态回归
DRI_DIR=/usr/local/lib/aarch64-linux-gnu/dri /home/admin/jm9100-oss/tests/query/run-query-tests.sh

# 真实应用
glxgears -info                       # FPS
glmark2 --size 800x600               # 综合 GL
ffmpeg -hwaccel vaapi -hwaccel_output_format vaapi -i in.mp4 -f null -   # 硬解
mpv --hwdec=vaapi --vo=gpu in.mp4    # 窗口播放
/tmp/cl-test                         # OpenCL（源码见报告 3.6 节）
```