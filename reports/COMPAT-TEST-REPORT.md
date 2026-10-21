# JM9100 开源驱动（修复版）应用兼容性测试报告

- 日期：2026-10-20
- 方法：全程 **env 非侵入式定向**（LD_LIBRARY_PATH / LIBGL_DRIVERS_PATH / LIBVA_* / QT_* / SDL_*），未改系统配置、未切换系统驱动
- 被测栈：`src/mesa/build-mwv207`（mwv207 DRI + EGL + GBM + glapi 22.3.7 修复版）与 `build-va`（含 VA 前端 + get_video_param 修复）
- 联测库：`compat-env/va222-root`（deepin 仓库 libva 2.22 解包，仅 LD_LIBRARY_PATH 加载，未安装）

---

## 一、结果总表

| 维度 | 工具/框架 | 结果 | 说明 |
|------|-----------|------|------|
| **GL 基础** | EGL/GBM surfaceless | ✅ PASS | renderer=MWV207（真硬件），ES 3.1 |
| **GL 基础** | FBO 硬件渲染 | ✅ PASS | clear 精确回读 |
| **GL 基础** | 完整管线（shader+draw+readback） | ✅ PASS | 三角形像素校验 R=255 |
| **GL 框架** | SDL2 + GLES2 | ✅ PASS | renderer=MWV207（surfaceless/offscreen） |
| **GL 框架** | Qt6 (QOpenGLContext) | ⚠️ 部分 | eglfs 需 DRM master（被 X server 占用）；offscreen 平台硬编码 GLX → env 模式无法测 EGL 路径 |
| **视频播放** | mpv render API（视频帧→GL FBO） | ✅ PASS | mpv 全管线在 MWV207 上渲染帧 |
| **视频播放** | mpv DRM 直连输出 | ❌ 被占 | KMS master 被 X server 持有，仅系统级切换后可用 |
| **硬件解码** | VA-API (vainfo) | ✅ 不崩溃* | 修复 NULL deref 后正常报告 VAProfileNone |
| **硬件解码** | ffmpeg `-hwaccel vaapi` | ✅ 优雅回退 | VA 连接成功；H264 profile 不存在 → 自动软解，无崩溃 |
| **硬件解码** | GStreamer libgstva | ⚠️ | 系统插件按 libva 2.20 编译，加载 2.22 后段错误（版本 ABI） |
| **WebGL** | Firefox headless | ❌ env 不可达 | Firefox 按系统 glvnd ICD 选库，env 定向无效；实际命中系统 llvmpipe |
| **VDPAU** | vdpauinfo | ➖ 未测 | payload 有 libvdpau_mwv207，但同为闭源 VA 栈的镜像问题（无公开源码），意义有限 |

*开源 UMD 不含视频 codec 实现 → 硬件编解码本身不可用（见 §三-1）

---

## 二、逐项详情

### 1. GL/加速层 —— 全部通过

修复后的 mwv207 用户态在真实硬件上的状态：

```
gbm backend: drm                      ← glamor 硬要求（厂商栈是 "jm"，会被 glamor 拒绝）
EGL vendor: Mesa Project, EGL 1.4
GL renderer: MWV207 (Jingjia Corporation)   ← 真硬件，非 llvmpipe
GL version : OpenGL ES 3.1 Mesa 22.3.7 (git-4197d4a)
```

- SDL2（游戏框架常用 EGL 路径）：`SDL_VIDEODRIVER=offscreen` + surfaceless → **PASS**
- mpv render API（视频应用 GL 输出层的事实标准）：解码帧渲染进 mwv207 EGL FBO → **PASS**
- GLX 路径：**env 模式不可用**（原因见 §三-2），但 glX 查询显示 X server 有 GLX visual（版本 1.4）

### 2. 硬件编解码 —— 开源栈的"硬缺口"

测试链条：vainfo（libva 2.22 + 我们的 VA driver）→ ffmpeg `-hwaccel vaapi` → GStreamer。

发现并修复了一个崩溃级缺陷（本次新增，commit 待打）：
- **`mwv207` 从未注册 `screen->get_video_param`**，而 vl 层 `vl_codec_supported()` 无 NULL 检查直接调用 → 任何 VA-API 消费者在 `vaQueryConfigProfiles` 必然 SIGSEGV（vainfo 实测段错误，gdb 定位 `vl_codec.c:60` NULL 调用）。
- 修复后 vainfo 正常退出并诚实报告：`VAProfileNone + VAEntrypointVideoProc`。

**根本事实**：jemoic 公开的 mesa 树**不包含视频 codec 前端**（无 pipe_video_codec/解码器实现；KMD 的 pipe_dec/pipe_enc 存在但用户态未公开对接）。因此：

| 场景 | 行为 |
|------|------|
| ffmpeg `-hwaccel vaapi` | VA 设备初始化成功（1.22 连接 OK），H264 无 profile → **自动回退软解，正常播完** |
| 期望硬解的应用 | 拿不到硬解（VAProfileNone），会走各自的软解回退 |
| **崩溃风险** | 修复前任何 VA-API 消费者首查 profile 即崩；修复后全部优雅降级 |

另外两个环境性事实：
1. **libva 版本钉死**：payload VA 驱动只导出 `__vaDriverInit_1_22`，系统 libva 2.20 装不上（报 no `__vaDriverInit_1_0`）。deepin 仓库有 2.22 但厂商依赖锁 2.20。开源栈启用时要么用仓库 2.22、要么上游同步。
2. vainfo 默认 X11 display 拿不到 DRM fd 会报 init failed——**必须 `--display drm`**（或 LIBVA 指定 renderD128），这是 libva 使用姿势不是缺陷。

### 3. GStreamer —— 系统插件 ABI 限制

系统 `libgstva.so`（gstreamer1.0-vaapi）按 libva 2.20 编译；我们 env 注入 libva 2.22 后插件加载段错误。**开源栈系统级启用时需重编 gstreamer1.0-vaapi 对齐 libva 2.22**（或用回 2.20 并向景美索要 1.20 接口的 VA 驱动）。这是打包问题，不是驱动代码缺陷。

### 4. Firefox/WebGL（Chromium/Electron 同机制）—— env 定向不可达

实测矩阵：

| env 组合 | 结果 |
|----------|------|
| 我们全套 22.3 库注入 | `ALL_FAIL`（22.3 libEGL 与系统 glvnd 的 vendor 机制不兼容） |
| 仅 LIBGL_DRIVERS_PATH（系统库） | `OK: llvmpipe` —— Firefox 走**系统 ICD**（`/usr/share/glvnd/egl_vendor.d/*.json`），env 覆盖不了它的选择 |
| `__EGL_VENDOR_LIBRARY_FILENAMES` 指到我们的 libEGL | glvnd 加载成功但 `eglInitialize` 失败（22.3 standalone EGL 作 glvnd vendor 的入口差异） |

结论：**浏览器/Electron 类应用只能通过系统级 ICD 接入开源驱动**（把 mwv207 的 ICD JSON 装进 `/usr/share/glvnd/egl_vendor.d/` 并配置排序——即工作区 `switch-to-oss.sh` 已实现的方案，含 09_ 前缀优先与厂商 ICD 停用）。env 模式测不了它们是机制限制，不是驱动缺陷。

### 5. Qt —— 平台插件机制限制

- Qt 的 offscreen 平台插件**硬编码 GLX** → 撞上 §三-2 的混载崩溃（系统 24.3 GLX driver + 注入的 22.3 glapi）
- eglfs 需要 DRM master → 被正在运行的 X server 占用（KMS 独占），headless 下不可用
- **Qt 应用（含 Deepin 桌面自身）要吃到硬件加速，需要系统级启用**（lightdm/X 拿到我们的 EGL vendor 后 glamor 即可初始化——本报告验证的 `gbm backend=drm` 正是 glamor 的硬前提，此前厂商栈卡在 "jm" backend + llvmpipe）

### 6. 系统环境发现（顺带）

- `/dev/dri/card0` 无 ACL、用户不在 video 组：KMS master 类应用（mpv drm、eglfs）以普通用户会失败；`renderD128` 有 ACL 可用。
- X server 当前 modesetting+ShadowFB（软渲染桌面）——本会话未改变该状态。

---

## 三、关键结论

1. **开源 UMD 没有视频编解码实现**。GL/3D 加速完整可用（且比当前桌面用的 llvmpipe 强），但硬解/硬编必须等厂商开源 VA 前端，或继续用厂商闭源 VA（二者不互斥：厂商 VA 走自己的 KMD 接口，开源 GL 走 mwv207 KMD——但同一时间只能加载一个 KMD，厂商 VA 在 mwv207 KMD 下不可用，之前实验已证实）。**这是"开源栈适配上限"的核心事实**：切换开源栈 = 获得可维护的开源 GL 加速，失去硬解（视频播放退软解，CPU 占用升高）。
2. **EGL/GLX 的版本一致性是硬约束**：22.3 与系统 24.3 任何交叉加载（LD_LIBRARY_PATH 半注入）都会 `_glapi_add_dispatch` 段错误。系统级启用必须整套替换 + ICD 指向（switch-mwv207-oss.sh 的私有目录 + 公开名胶水 + RUNPATH 设计正是为此）。
3. **本次新增修复（应合入补丁序列）**：`mwv207_get_video_param` 实现（防 VA 消费者全体崩溃）。vainfo 从段错误 → 正常枚举。
4. **对"是否能用"的判定**：
   - 作为 **3D/GL 加速栈**：可用（硬件渲染已验证，SDL/mpv 级别应用即插即用）
   - 作为**桌面全栈**：需系统级切换（X+glamor 才能让 Qt/GTK/浏览器受益），本会话按约束未做
   - 作为**视频硬解栈**：不可用（无 codec 前端），但不会拖垮宿主应用（全部优雅回退）

## 四、建议的后续（如需真正启用）

> 状态注记 (2026-10-21 12:15): 本节为 22.3 时代所写。以下各项已被后续阶段
> 完成或超越——①已合入补丁序列; ②系统级切换已由 switch-mwv207-oss.sh 实现
> 且 glamor 点亮+冷启动验证 (见 PHASE5-SWITCH-REPORT.md); ③无需厂商 VA 前端,
> OpenHarmony video-codec 移植已完成硬解 (见 HWDECODE-FEASIBILITY-REPORT.md);
> ④VA 驱动已入系统 dri 目录自动命中。原建议保留如下, 仅作历史记录:

1. 合入本次 `get_video_param` 修复，重打 patch 序列
2. 用旧 switch-to-oss.sh 的既有机制做系统级切换（含看门狗/回滚），使 X/glamor、Qt、浏览器全部吃到 mwv207 硬件 GL
3. 向景美反馈/索取：开源 VA 前端（pipe_dec/pipe_enc 的用户态）+ libva 2.20 接口的驱动变体（或推动系统升级 libva 2.22）
4. 重编 gstreamer1.0-vaapi 对齐 libva 2.22

---

## 追加：验收能力矩阵复核（2026-10-21 12:05，终态安装 oss-24.1.2-fullstack-r1 + 冷启动后）

用户验收表三项待复核（EGL GBM ❌ / OpenCL ⚠️"软件栈" / Vulkan ❌），逐项取证如下。

### 一、EGL GBM 平台 —— ✅ 正常（验收 ❌ 系探针方式所致，非驱动缺陷）

**现象**：eglinfo GBM 平台报 `mwv207: failed to create kernel context, fd=N err=-13`（EACCES）。

**根因（证据链）**：
- strace：eglinfo GBM 探针硬编码 `open("/dev/dri/card0")`（**主节点**）且不做任何 DRM 认证；surfaceless 平台则开 renderD128 → 通过。
- 内核实锤：mwv207 KMD 全部 ioctl 均为 `DRM_AUTH|DRM_RENDER_ALLOW`（5.10 树 mwv207_drv.c:111-116；6.6 官方移植 mwv207_gpu.patch:5072/5075，即当前在跑的 in-tree 驱动）——教科书式正确标志：
  - `DRM_RENDER_ALLOW` → render 节点免认证放行；
  - `DRM_AUTH` → 主节点要求 authenticated（drm_ioctl.c:537）。
- `drm_file.c:167`：`file->authenticated = capable(CAP_SYS_ADMIN)` —— root 开主节点自动认证。

**四方互证（GBM 全通，仅"未认证普通用户+主节点"被拒）**：

| 使用方式 | 结果 |
|---|---|
| renderD128 + 普通用户（uid 1000，零 env） | ✅ egl-smoke GBM 全管线 OK（gbm backend: mwv207） |
| card0 + root（CAP_SYS_ADMIN 自动认证） | ✅ eglinfo -B -p gbm：MWV207 / GL 4.0 双 profile |
| card0 + Xorg（DRM master） | ✅ 当前 glamor 硬件加速本身就是 GBM+card0 路径点亮的 |
| card0 + 普通用户未认证（eglinfo 探针） | ❌ EACCES —— 标准 DRM 安全语义，Intel/AMD 同标志同行为 |

**结论**：真实 GBM 消费者（compositor、Wayland 会话）要么以 master 开 card0，要么用 renderD128，均不受影响。eglinfo 探针失败与驱动无关。Wayland 平台探针失败同因（card0 未认证），且本机为 lightdm+X11 无 Wayland 会话，属基础设施缺位。

### 二、OpenCL —— ✅ GPU 硬件执行（"软件栈"系误解）

- 本栈 OpenCL 走 **rusticl**（Mesa 官方开源实现，建于 Gallium compute）：kernel 经 LLVM 编译为 GPU 可执行码，经 mwv207 Gallium 驱动**在 GPU 上执行**——cl-test 65536 元素向量核 0 失配即 GPU 执行结果。非 CPU 软件渲染。
- "厂商 OpenCL"（闭源 ICD libmwv207OpenCL 原厂版）在 mwv207 KMD 下 `clinfo` devices=0，根本不可用（它只匹配厂商 jmgpu KMD）。
- 结论：rusticl 是本硬件**唯一可用**的 OpenCL，且跑在 GPU 上——开源栈反超闭源，而非降级。

### 三、Vulkan —— ❌ 确认硬边界

- JM9100 无任何公开 Vulkan 驱动：KMD 无 Vulkan 专用 ioctl；Mesa 树无 mwv207 Vulkan 驱动；系统唯一 ICD 为 lvp（llvmpipe 纯软件）。
- 从零实现 Vulkan 需寄存器级完整文档，厂商未公开——超出开源适配范围，维持硬边界结论。

### 复核后的能力矩阵

| 能力 | 验收表 | 复核结论 |
|---|---|---|
| OpenGL/GLX | ✅ | ✅ MWV207 硬件（glxgears 真渲染） |
| EGL X11/surfaceless | ✅ | ✅ MWV207 |
| **EGL GBM** | ❌ | **✅ 正常（探针方式问题：未认证 card0，DRM_AUTH 标准语义）** |
| VA-API | ✅ | ✅ H264×4+HEVC×2 VLD 硬解 |
| **OpenCL** | ⚠️"软件栈" | **✅ GPU 硬件执行（rusticl/Gallium compute；厂商闭源 ICD 反而 devices=0）** |
| Vulkan | ❌ | ❌ 硬边界（无公开驱动，维持） |
