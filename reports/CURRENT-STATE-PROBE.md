# 当前运行栈实测快照（重新探测）

探测时间：2026-10-30 22:2x（Asia/Shanghai）　主机：`admin-PC`

---

## 一、内核侧（KMD）

| 项 | 实测值 |
|---|---|
| 内核 | `6.6.155-arm64-desktop-hwe`，`#25.01.00.33 SMP PREEMPT Fri Aug 28 17:02:55 CST 2026 aarch64` |
| 已加载模块 | `mwv207`（refcnt 36，依赖 ttm/drm_kms_helper/gpu-sched/drm_display_helper/i2c-algo-bit/drm_ttm_helper） |
| 模块文件 | `/lib/modules/6.6.155-arm64-desktop-hwe/kernel/drivers/gpu/drm/mwv207/mwv207.ko`，**250 177 B，2026-08-28 16:50** |
| 属性 | `license: GPL`、`author: shanjinkui`、`description: MWV207 Graphics Driver`、**`intree: Y`**、内核构建密钥签名（PKCS#7） |
| `srcversion` | `90738AD2C689AD8A81F74D6` |
| `/sys/module/mwv207/version` | **空**（未定义 MODULE_VERSION） |
| `DRM_IOCTL_VERSION` | name `mwv207`，version **0.1.0**，date `20220407`，desc `mwv207 gpu driver` |
| 厂商闭源 KMD | `/lib/modules/…/updates/dkms/jmgpu.ko`，1 246 061 B，**2026-10-14 13:27**，`JingjiaMicro Graphics Driver` —— **已编译、未加载** |

**关键结论**：运行的是 **Deepin 内核自带的 in-tree mwv207**，本机**没有它的源码**（内核头文件包只有 `.ko`；仓库 `src/kernel` 是 5.10 基线，其 `linux-6.6` 分支无 mwv207 文件）。它构建于 2026-08-28，**早于**仓库修复提交 `0ec4816`(10-20)/`d94c106`(10-21) → **不含本仓库任何 KMD 修复**。

`dmesg` 不可读（无免密 sudo），故没有内核日志侧证据。

---

## 二、用户态（UMD）—— 本机并存 **三套**

| 栈 | 位置 | 版本 | md5 | 状态 |
|---|---|---|---|---|
| ① 22.3.7 构建 | `src/mesa/build-mwv207/src/gallium/targets/dri/mwv207_dri.so`（68 MB，10-26 22:37） | `Mesa 22.3.7 (git-1a7d8c2636)` | `d3554ca5…` | **只被 `tests/run-all.sh` 使用** |
| ② 24.1.2 payload | `/usr/local/lib/mwv207-oss/mwv207_dri.so` ← `/usr/local/lib/aarch64-linux-gnu/dri/mwv207_dri.so` | `Mesa 24.1.2 (git-6cb20bd761)` | `28ffca76…` | **桌面会话实际在用**（GLES 3.1） |
| ③ 24.1.2 + 补丁 0009/0010 | `src/oh-mesa/…/mesa-24.1.2/build-full/…/mwv207_dri.so` | `Mesa 24.1.2` + 补丁 | `20c4710d…` | **未安装**（GLES 3.2） |

加载实况（`LIBGL_DEBUG=verbose`）：`using driver mwv207 for 4` → `pci id for fd 4: 0731:9100, driver mwv207` → `MESA-LOADER: dlopen(/usr/local/lib/aarch64-linux-gnu/dri/mwv207_dri.so)` → `Using DRI3 for screen 0`。

**为什么②会赢**：`/etc/ld.so.conf.d/00-mwv207-oss.conf` 只有一行 `/usr/local/lib/mwv207-oss`，排序在系统之前，`ldd glxinfo` 显示 `libGL.so.1 => /usr/local/lib/mwv207-oss/libGL.so.1`（这个 libGL/libEGL/libGLESv2/libgbm 都是 **24.1.2** 自建，不是系统 glvnd）。

X / 显示：

- Xorg 用 **modesetting + glamor**（`glamor X acceleration enabled on MWV207`，`[DRI2] DRI driver: mwv207`），**未用**厂商 `mwv207_drv.so`。
- connector：`HDMI-1 connected primary 1920x1080`（当前 `1920x1080@100.00*`，首选 60.00），`HDMI-2`/`DVI-I-1`/`VGA-1` disconnected。

多媒体：

| 接口 | 实测 |
|---|---|
| VA-API | libva 1.22.0 / libva 2.19.0，驱动 `Mesa Gallium driver 24.1.2 for MWV207`，加载 `/usr/lib/aarch64-linux-gnu/dri/mwv207_drv_video.so` → `/usr/local/lib/mwv207-oss/libmwv207_drv_video.so`；profile：H264 ConstrainedBaseline/Main/High/High10、HEVC Main/Main10（皆 VLD）、None(VideoProc) |
| VDPAU | API 1，`G3DVL VDPAU Driver Shared Library version 1.0`，surface 420/422/444/420_16 均 8192×8192；`/usr/lib/aarch64-linux-gnu/vdpau/libvdpau_mwv207.so.1` → `/usr/local/lib/mwv207-oss/libvdpau_mwv207.so.1.0.0`（25 MB） |
| OpenCL | Platform `rusticl`，Device `MWV207`，`OpenCL 3.0`，2 CU；ICD `/etc/OpenCL/vendors/*.icd` → `/usr/local/lib/mwv207-oss/libmwv207OpenCL.so.1`（69 MB） |

**厂商 1.8.0 包是装着的**：`dpkg -l` 有 `mwv207-dev 1.8.0.uos`、`mwv207-dkms 1.8.0`；因此存在 `/usr/lib/aarch64-linux-gnu/dri/jmgpu_dri.so`(4.1 MB)、`jmgpu_drv_video.so`、`/usr/src/mwv207-1.8.0.uos`、`jmgpu.ko` —— 但**整体未启用**（GL 走 OSS，KMD 走内建 mwv207）。

⚠ **EGL 名字陷阱**：`/usr/share/glvnd/egl_vendor.d/10_mwv207.json` 指向 `libEGL_mwv207.so.0`，实体是 `/usr/lib/aarch64-linux-gnu/libEGL_mwv207.so.0` → 厂商闭源 `…/mwv207/libEGL_mwv207.so.1.5.0`；而 OSS 自己的是 `/usr/local/lib/mwv207-oss/libEGL_mwv207.so.1.0.0`（**`.so.1`**）。两者名字近似、版本不同源，混栈风险点。

---

## 三、能力实测对比（决定性）

| 项 | ② 已安装 payload | ③ 我们未安装的构建 |
|---|---|---|
| GL core / compat | 4.0 / 4.0 | 4.0 / 4.0 |
| GLSL | 4.00 | 4.00 |
| **GLES** | **3.1** | **3.2** |
| **GLES GLSL** | **3.10** | **3.20** |
| `GL_` 扩展行数 | 231 | **240** |
| `GL_KHR_robustness` | ✗ | ✓ |
| `GL_ARB_ES3_compatibility` | ✗ | ✓ |

即：本轮 GLES 3.2 / 鲁棒访问 / 16 位格式的成果**只存在于 §二③ 那份未安装的构建里**。

---

## 四、本次探测新发现

### 1. 显存上报错 1024 倍（新，1 字符可修）

`src/gallium/drivers/mwv207/mwv207_screen.c:175-176`：

```c
case PIPE_CAP_VIDEO_MEMORY:
   return 1024 << 20;
```

`PIPE_CAP_VIDEO_MEMORY` 的语义是 **MB**，这里返回的是**字节**（1 GiB）。因此 `glxinfo -B` 报
`Video memory: 1073741824MB`（≈1024 TiB）。正确值应为 `return 1024;`。
影响面：所有按显存决定画质/纹理预算的应用与游戏。`/sys/class/drm/card0/device/mem_info_vram_total` 不存在，无法交叉核对真实显存。

### 2. 仓库回归套件测的不是"正在跑的那套"

`tests/run-all.sh:6-8` 把 `LIBGL_DRIVERS_PATH` 固定到 `src/mesa/build-mwv207`（**22.3.7**），
所以 `run-all.sh` 实测输出是 `OpenGL ES 3.1 Mesa 22.3.7 (git-1a7d8c2636)`。
即：**桌面在跑 24.1.2 payload，回归在跑 22.3.7，最新补丁那套两边都没覆盖**。

### 3. 记录更正

- 内核版本：此前记为 `6.6.143-arm64-desktop-hwe`，**实为 `6.6.155-arm64-desktop-hwe`**。
- 运行模块性质：此前只知"早于修复提交"，现确认 **in-tree: Y + 内核签名 + 本机无源码**，因此**无法在本机重建**。

---

## 五、一句话结论

**在跑的是**：Deepin 内建 `mwv207` KMD（2026-08-28，不含本仓库修复）+ 24.1.2 payload UMD（GLES 3.1）。
**已构建未安装**：24.1.2 + 补丁 0009/0010（GLES 3.2），且改动仍在工作区未提交（6 文件 / +140 −3）。
**厂商 1.8.0 全家桶装好但整体闲置**（`jmgpu_dri.so` / `jmgpu.ko` 均未被使用）。