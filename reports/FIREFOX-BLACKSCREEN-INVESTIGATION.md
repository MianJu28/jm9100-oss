# Firefox 首帧黑屏 —— 缺陷定位报告

- 日期：2026-10-22
- 现象：Firefox 冷启动后首页 / 恢复页 / `about:support` 等 **XUL chrome 页永久黑屏**（100% 纯黑或全白，无内容结构）；HTML 网页、动画页、mpv 等正常。
- 结论：**不是 mwv207 驱动的缺陷，也不是 EGL/GLX/DRI3 用户态代码的缺陷**。根因在 **Xorg 合成器侧的窗口首绘死区**（map 后 0–4ms 内的第一次 X 层绘制内容被永久丢弃），Firefox 只是最容易被它击中的受害者。
- 当前处置：驱动栈回退到 `5e6f2dd` 状态（撤销 `libEGL.so` 无后缀名拦截），Firefox 恢复可用（走系统 llvmpipe 显示正常）。已实现系统级守护 `daemon/map-monitor.c` 作为通用缓解手段，待 Xorg 侧根治后即可退役。

---

## 一、影响面矩阵（实测）

| 场景 | 结果 |
|---|---|
| Firefox 冷启动 + `about:support` / 会话恢复页 | ❌ 100% 纯黑（mean=0，stdev=2.3，dark%=100） |
| Firefox 冷启动 + 外部 URL（`https://example.com`） | ✅ 可见（加载延迟使首呈现晚于死区） |
| Firefox 窗口内后续导航 / 新标签 / 切回标签 | ✅ 可见（窗内任一页绘制过即恢复正常） |
| Firefox 窗口 resize（`xdotool windowsize`） | ✅ 内容恢复（resize → ConfigureNotify + Expose → 重绘） |
| mpv（X11 EGL 窗口）/ glxinfo / glxgears | ✅ 全部正常（demux 延迟使首呈现晚于死区） |
| 动画页（`setInterval` 持续 present） | ✅ 正常（持续 present，跟得上；仅滞后一帧） |
| 纯 X 协议 `XCopyArea`（不经 GL） | ❌ **同样丢失** → 排除 GL 栈，锁定 X 层 |

---

## 二、根因定位链（每一步都有实测支撑）

### 2.1 排除项

| 假设 | 排除依据 |
|---|---|
| mwv207 驱动 present 路径有问题 | 探针 `x11-expose-recovery` / `x11-guard-test`：`glClear`+`glSwapBuffers` 后**内容正确上屏**（254,0,0）；EGL 全套（X11/surfaceless/GBM）、DRI3 dmabuf export、GLX、ES2/ES3/core profile、ARGB visual 全部渲染正确 |
| 探针自身 bug（误报"不上屏"） | **早期结论确为探针 bug**：`glClear(GLbitfield mask)` 被误当 `glClearColor(4 floats)` 调用 → mask≈0.9f 转 int≈0 → 什么都没清。修正后全部正常 |
| 厂商 era `LD_PRELOAD` shim（`libegl-pending-delete.so`） | 绕过 wrapper 直接启动 Firefox（无 LD_PRELOAD）→ 恢复页 **仍 100% 黑** |
| EGL/GLX/DRI3 用户态代码被厂商改坏 | 逐字节比对生产树 `src/oh-mesa/User_source_code/mesa-24.1.2`：`loader_dri3_helper.c` / `platform_x11.c` / `platform_x11_dri3.c` / `platform_x11.h` / `egl_dri2.c` / `egl_dri2.h` 与上游 mesa-24.1.2 **`diff -w` = 0 行**（厂商零改动） |
| loader 层行为差异 | 对生产树 `loader_dri3_helper.c` 插桩（`MWV207_DRI3_DEBUG=1`，6 个打印点：init/CONFIGURE/COMPLETE/swap-SKIP/PRESENT/get_buffers）：**毒化组与健康组日志逐字相同**（同 init、同 get_buffers、同 PRESENT） |
| EGL/Present 路径特有 | 纯 `XCopyArea`（不经 EGL/DRI3）**同样丢失** |
| 合成器（kwin/DDE）不合成 | kwin suspend 合成后 raw window 仍黑 |
| 显示器刷新率 / vsync | IVL=1（vsync）与 IVL=0 行为一致 |
| 窗口尺寸 / 分辨率 | 800x600 与 1920x1080 复现一致 |

> **重要更正**：会话早期曾得出"厂商把整套 22.3.7 的 `src/egl`+`src/loader`+`src/glx` 拖进 24.1.2 树"的结论 —— 那是**在错误的树**（`src/mesa`，jemoic 的 22.3.7 原树）上做的比对，属于**假验证**。生产树是 `src/oh-mesa/User_source_code/mesa-24.1.2`，其 EGL/X11/DRI3 代码与上游 24.1.2 完全一致。旧结论作废。

### 2.2 死区精界

`x11-delta` 探针（`MapNotify` 后 `usleep(Δ)` 再 `XFillRectangle` + `XSync` + `XGetImage` 回读）：

| Δ | 画面 |
|---|---|
| < 3.2 ms | ❌ BLACK |
| ≥ 4.7 ms | ✅ RED |

- **死亡窗口 = 窗口 map 后 0–4ms 内的第一次 X 层绘制**（`XFillRectangle` / `XCopyArea` / EGL present / DRI2 swap 全中）。
- 双重 fill 探针：f1@0.2ms 丢、f2@2ms 丢、f3@30ms 留住 → **一次性丢弃窗口**，后续绘制正常。
- `XSync`（服务器确认处理完）后立刻 `XGetImage` 读窗口仍黑 → 内容**从未写入窗口当前 drawable**，不是"后来被清"。
- 事件时间线（`x11-when-black2`）：`ReparentNotify@+3ms → MapNotify@+14ms → FILL synced → ConfigureNotify@+16ms → Expose@+17ms`。
- 边界依赖：需要 **WM redirect**（`OverrideRedirect` 窗口不丢；无 WM 的 Xephyr 不丢）→ 指向合成器 backing pixmap 分配与 reparent+map 序列的竞态。

### 2.3 X 服务器侧取证

- deepin xorg-server `2:21.1.16-1deepin6` 源码（`/tmp/dxsrc/xorg-server-21.1.16`）：`compwindow.c` `compRealizeWindow()` → `compCheckRedirect()` → `compAllocPixmap()` → `compNewPixmap()`（`CreatePixmap` + **`CopyArea` parent→backing**）；`compReparentWindow()` 在 map 之前执行（WM 标准 reparent-then-map）。
- deepin 自带补丁审查：`screenshot-protection`（只审跨 client `CopyArea`）、`glamor-gles` 系列（shader 语法适配）**均与本现象无关**。
- 结论：合成器在 reparent + map 序列中分配 backing pixmap 时存在竞态，死区内的绘制落进了**即将被替换的旧 drawable**。

---

## 三、缓解方案

### 3.1 首选（已实施）：驱动栈回退，Firefox 走系统路径

`5e6f2dd` 之后新增的 `libEGL.so`（无后缀开发名）胶水拦截会让 Firefox 的 GPU 进程**完全**跑在 mwv207 栈上，从而**必然**踩中死区（首帧 present 落在 map 后 0–4ms）。撤销该拦截后：

- Firefox 的 EGL 请求回到系统 glvnd → 系统 Mesa 24.3 llvmpipe → **Firefox 显示完全正常**；
- 其余全部能力不受影响（GLX/GLX 直渲、VA 硬解、OpenCL 仍走 MWV207；`libEGL.so.1` / `libGL.so.1` / `libGL.so` 拦截保留，仅 Firefox 的 EGL 开发名回落）。

代价：Firefox 的 WebGL / WebRender 暂用 llvmpipe（软件），硬件解码（VA）不受影响。

### 3.2 通用缓解（已实现，待启用观察）：`daemon/map-monitor.c`

**原理**：监听 root 的 `SubstructureNotify`，每当顶层窗口被 `Map`，延迟 50ms 后对其发 **1×1 `XClearArea(exposures=True)`**。X server 向该窗口发 `Expose` 事件 → 客户端重绘 damage 区域 → **死区丢失的内容被补上**。对所有响应 Expose 的 X11 应用透明生效。

**已验证**（`tests/x11-deadzone-guard-test.c` 探针）：

```
EXPOSE 1x1
EXPOSE 400x300
RESULT: (254, 0, 0)   ← 毒化窗口自动恢复
```

部署：`/usr/local/bin/map-monitor` + `/etc/xdg/autostart/map-monitor.desktop`（登录自启）。

> 注意：v1 版本对每个 map 的窗口做全窗 `XClearArea`（400×300）会引发全桌面闪烁，且对已销毁窗口触发 `BadMatch`。v2 改为 **1×1 clear** + 过滤（仅 root 直接子窗、跳过 `InputOnly`、跳过 `override_redirect`）+ 错误处理器吞掉异步错误。

### 3.3 根治方向（待厂商/Xorg 上游）

合成器 `compRealizeWindow`/`compNewPixmap` 与 `compReparentWindow`/map 之间的顺序竞态，需要在 Xorg 侧修复（或改用不 redirect 的路径）。本仓库无法自行修补 xorg-server（deepin 打包），需上游/厂商版本。

---

## 四、复现与验证工具

| 文件 | 用途 |
|---|---|
| `daemon/map-monitor.c` | 死区守护守护进程（Expose 轻推） |
| `tests/x11-deadzone-guard-test.c` | 死区复现 + 守护效果验证探针 |
| `/tmp/x11-premap*.c` 系列 | surface 早于 map 创建的竞态复现（GLX/EGL/GLES 三路径） |
| `/tmp/x11-egl-boundary.c` | 死区精确二分（DELAY 扫描） |
| `/tmp/x11-copyarea-test.c` | 纯 X 协议复现（排除 GL 栈） |
| `/tmp/x11-expose-recovery.c` | Expose 恢复原理验证 |
| `/tmp/egl-name-probe.c` | 指定 SONAME 的 dlopen + EGL 全管线探针（拦截验证） |

---

## 五、遗留

- Xorg 侧死区根因未修（需 xorg-server 补丁），当前依赖 §3.1（Firefox 回落 llvmpipe）与 §3.2（守护轻推）。
- Firefox 走 llvmpipe 时 WebGL/WebRender 为软件渲染，GPU 硬件加速暂不可用于 Firefox。
- 早前调试期间 Firefox 曾出现"起不来 / 主窗永不 map"的异常（反复 `pkill -9` 与 10×10 命令处理窗残留所致），重启会话后状态可恢复；测试时须用 `setsid nohup ... < /dev/null & disown` 并用 `pgrep -x firefox` 判活。
