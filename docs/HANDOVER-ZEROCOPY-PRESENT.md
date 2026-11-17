# 交接文档 — 移植厂商栈"零拷贝异步翻转"present 架构（2026-11-17）

> 目的：把"厂商 present 0.014 ms vs 我方 0.4–2.7 ms"这件事的**架构差异**讲清楚，
> 并给出可执行的移植方案、所需代码触点、验收判据与止损条件。
> 配套：`docs/HANDOVER.md`（总状态）、`docs/HANDOVER-2D.md`（2D 引擎，已封档）、
> `reports/VENDOR-GAP-20261113.md`、`reports/VENDOR-STACK-PROBE-20261116.md`。

## 0. 结论先行

1. 厂商的 0.014 ms **不是"更快地拷贝"，而是根本不拷贝**：窗口内容由显示引擎的
   **WIN0 窗口（overlay 平面）**直接扫描出，swap 只是"把这块缓冲挂到窗口上 + 等 vblank"。
2. 我方 present 的 0.399 ms（800×600）/ 2.7 ms（1080p）来自**两次整帧拷贝**：
   `UMD flush_resource`（平铺→线性影子）+ X glamor（客户端 pixmap→屏幕 pixmap）。
3. 我方 KMS **已经有 overlay 平面实现**（`dc/mwv207_va.c` 的 `mwv207_overlay_atomic_update`），
   但默认关闭（模块参数 `overlay=N`），且 X 侧没有任何代码会用它做 present。
4. 因此有三条路（§5）：**C1 overlay 直扫**（真零拷贝，需 X 侧补丁 + 硬件验证）、
   **C2 翻转缓冲改线性**（去掉影子拷贝，纯 UMD 改动，实测 fullscreen +24%）、
   **A 维持现状**（平铺 + 影子）。推荐先做 C2（低风险、可量化），再评估 C1。

## 1. 厂商架构 vs 我方架构（对照表）

| 层 | 厂商栈（`jmgpu` 1.8.0） | 我方栈（r38） |
|---|---|---|
| X 驱动 | 闭源 DDX `mwv207_drv.so`（EXA + 私有 2D/overlay） | 上游 **modesetting**（glamor + DRI3 + Present） |
| 窗口 buff | DDX 自己分配的 pixmap（线性、VRAM） | 客户端 DRI3 buff（我们默认**平铺 + 线性影子**） |
| swap 动作 | 把该 buff 的地址写进 WIN0 窗口寄存器 → 等 vblank → 完成 | `flush_resource` 平铺→线性影子拷贝 → glamor 拷到屏幕 pixmap → `drmModePageFlip` |
| 拷贝次数 | **0**（窗口缓冲就是扫描缓冲） | **2**（影子 + glamor） |
| 翻转 | `jmgpuScanOutFlip`（私有 ioctl 直接切扫描地址） | `PAGE_FLIP` ioctl（通用 KMS） |
| 同步 | 异步：提交后不等，靠 fence 记账 | 半同步：`glXSwapBuffers` 内部要等拷贝+fence |

现场证据（r38，同机同场景）：

- 两边**显示窗口寄存器逐字段相同**（`0x990400=0x00500030`、`0x990434=0x1e0` pitch 7680=1920×4、
  `0x99043c=0x0437077f`）⇒ 双方都是**线性扫描出**，厂商没有用 TILE；
- 厂商栈 `0x990438` 在 `0x6b880` ↔ `0x8b2c0` 之间**每帧交替**（空闲时也交替），
  我方恒为 `0x49c00`/`0xa400`（不翻转）；
- 厂商 `glmark2 -b build:duration=10`：**15634 FPS / 0.064 ms / Score 15633**，
  同期 Xorg CPU 2.72 s / 10 s ≈ 27%（≈18.7 µs/帧）—— Xorg 不阻塞在整帧拷贝上。

## 2. 我方 present 路径的实测证据（r38 安装栈）

### 2.1 X 侧行为（strace Xorg，4 s 采样）

| 场景 | `PAGE_FLIP` | `ADDFB2` | `RMFB` | `CRTC_GET_SEQUENCE` |
|---|---|---|---|---|
| 窗口模式 800×600（glmark2 默认窗口） | **0** | 0 | 0 | 4463 |
| 全屏 1920×1080（`--fullscreen`） | **400** | 400 | 400 | 5988 |

判读：
- 普通窗口**从不翻页**——X 把客户端 pixmap `CopyArea` 进屏幕 pixmap，再刷屏（glamor 拷贝）；
- 全屏才走 `PAGE_FLIP`，且**每帧一次 `ADDFB2` + 一次 `RMFB`**（每帧是一个新缓冲，
  X 为它建/删 fb），说明全屏路径已经是"直接翻转客户端缓冲"的形态——**零拷贝的前提已具备一半**；
- `CRTC_GET_SEQUENCE` 每帧多次 ⇒ CPU 在等 vblank（这是"半同步"的代价，厂商是异步的）。

### 2.2 平铺 vs 线性（`MWV207_SCANOUT_TILE`，同构建 A/B）

| 场景 | `=1`（平铺 + 影子拷贝） | `=0`（全线性、无影子） | 判读 |
|---|---|---|---|
| 全屏 1080p glmark2 build | 780 FPS / 1.283 ms | **970 FPS / 1.032 ms** | **去掉影子拷贝净赚 ~0.25 ms/帧**；该场景 fill 少，平铺收益补不回来 |
| 窗口 300×300 present | **0.033 ms** | 0.198 ms | 小窗口平铺收益巨大（PE 写平铺 38 GB/s vs 线性 6.7 GB/s） |
| 窗口 800×600 present | 0.857 ms | 0.854 ms | 打平（影子拷贝被 glamor 拷贝掩盖） |
| 窗口 1920×1080 present | 2.745 ms | 2.730 ms | 打平 |

（`vblank_mode=0`，`pbench <w> <h> 200 1`）

⇒ **策略必须按"这块缓冲会不会被翻转"分流**，一刀切都不对：
- 会被翻转（全屏/未重定向窗口）：线性 + 直接翻转，省 0.25 ms/帧；
- 会被合成（有 compositor 时的窗口）：平铺，保渲染带宽。

## 3. 我方现有实现的代码触点（改动之前先读这几处）

| 位置 | 作用 | 与本议题的关系 |
|---|---|---|
| `src/mwv207-umd/drivers/mwv207/mwv207_resource.c:79` `mwv207_can_tile()` | 决定是否平铺。SCANOUT 资源默认平铺（Xorg 进程除外，`util_get_process_name()` 判定），`MWV207_SCANOUT_TILE=0` 全局关 | **C2 的落点**：把"会被翻转"的资源判成线性 |
| `:246` `set_miptree()` | 平铺资源同时建线性影子 `res->linear` | C2 下这条分支不再进入 |
| `:352` `mwv207_resource_get_handle()` | 有影子时**导出影子**并标 `DRM_FORMAT_MOD_LINEAR`；否则导出本体，平铺时标 `VIVANTE_SUPER_TILED` | 翻转靠这个 handle；线性化后直接导出本体 |
| `:384` `mwv207_flush_resource()` | `sync_needed` 时把整层 `mwv207_copy_image` 到影子（**只排队，不 submit**） | C2 下这次拷贝消失 |
| `src/mwv207-umd/drivers/mwv207/mwv207_cmd.c:380` | `mwv207_submit()` 对带 `res->linear` 的 cbuf 置 `sync_needed` | 同上 |
| `src/mwv207-umd/drivers/mwv207/mwv207_context.c:67` `mwv207_flush()` | 逐 cbuf `flush_resource` → `mwv207_submit(..., true)` 并返回 fence fd | 加"异步/跳过"逻辑的位置 |
| `src/kernel-6.6/dc/mwv207_va.c:292` `mwv207_overlay_atomic_update()` | WIN0 窗口编程（`0x050` 地址、`0x068` pitch、`0x010` 位置、`0x02C` 通道使能、`0x03C/0x0EC` 缩放，`0x0E4/0x00C` 尺寸） | **C1 的硬件基础** |
| `src/kernel-6.6/dc/mwv207_va.c:347` `module_param_named(overlay, …)` | overlay 平面默认 **N**（`/sys/module/mwv207/parameters/overlay` 当前为 `N`） | C1 需打开并验证 |
| `src/kernel-6.6/dc/mwv207_va.c:809` | 只有 `mwv207_overlay_enabled` 时才注册 `DRM_PLANE_TYPE_OVERLAY` | 打开后 X/modesetting 才能看到这块平面 |
| `src/kernel-6.6/dc/mwv207_va.c:704` `.page_flip = drm_atomic_helper_page_flip` | 通用 KMS 翻页已通 | 全屏路径已验证可用 |

## 4. 目标架构（要到达的形态）

```
客户端（GL/WebRender）
   │ 渲染进 DRI3 缓冲（**线性**，或平铺+零拷贝翻转）
   ▼
glXSwapBuffers
   │ ① fence 记账（不等）
   ▼
DRI3/Present ──►  X（modesetting 或我们的补丁）
   │ ② 把这块缓冲挂到 WIN0 overlay（C1）或设为 primary scanout（C2）
   ▼
KMS atomic commit ──► DRM_EVENT_FLIP_COMPLETE（vblank 后通知客户端）
```

三条硬要求：**别拷整帧、别在 swap 里等 vblank、别在每帧重建扫描面**。

## 5. 三条移植方案（含工作量与风险）

### C2（推荐先做，纯 UMD，1–2 天）

**做法**：给"会被翻转"的资源判成线性，跳过影子机制。

判据（从易到难三档，任选其一实现）：
1. 环境/配置档：读 `~/.drirc` 或 `MWV207_SCANOUT_TILE=0`（已有）——但它是全局的，会伤小窗口；
2. **尺寸档**：`PIPE_BIND_SCANOUT` 且 `width*height >= 1280*720` 的客户端资源走线性
   （大窗口/全屏才值得零拷贝，小窗口保留平铺）；
3. **真实判据（推荐）**：在 `mwv207_flush_resource()` 里看该资源是否**被 Present 过**
   （`whandle` 导出次数 / `PIPE_BIND_SCANOUT` + 最近一次 `get_handle` 的 usage 含 `DRM_RDWR|DRM_WRITE`），
   命中就把它迁移到线性 BO（或下一次分配时线性）。

**代价/收益**：全屏 1080p +24%（1.28→1.03 ms）；小窗口可能退化（300² 0.033→0.198 ms）。
**风险**：低（只改分配策略）；需要 `tools/gl-regression.sh` + 桌面观感 + glmark2 全套确认不退化。

### C1（真零拷贝，2–4 周，需要硬件 + X 侧配合）

**做法**：
1. KMD：`overlay=1` 打开 WIN0 平面（`mwv207_overlay_atomic_update` 已实现，需验证
   `pitch & 0x3F == 0`、位置/缩放、与主平面共存、原子提交顺序）；
2. X 侧：给 glamor/modesetting 加"present 走 overlay"的补丁——我们对 xserver 已经有两份
   补丁（`patches/xserver/0001-glamor-blit-fast-path.patch`、`0002-glamor-debug-probe.patch`），
   可延续同一套 `tools/build-xglamor.sh` 流程；
3. 客户端缓冲仍必须**线性**（WIN0 只接受线性地址 + pitch），所以 C2 仍是前置条件。

**验收**：全屏 1080p `pbench` 进入 **< 0.2 ms**；Xorg CPU 明显下降；`strace` 里
`PAGE_FLIP` 仍然每帧一次但**没有 glamor 拷贝的 submit**；桌面/视频/滚动不出撕裂。

**风险**：中高 —— WIN0 当前被厂商用来承载 >64×64 光标（见 `mwv207_va.c` 注释），
需确认不冲突；X 的 plane 使用要过 modesetting 的格式/修饰符检查（我们导出 LINEAR 修饰符，
`is_dmabuf_modifier_supported` 已补上，见 `37918b3`）。

### A（维持现状）

平铺 + 影子拷贝。0.399 ms（800×600）/ 2.7 ms（1080p）。作为 C1/C2 的 fallback 保留，
`MWV207_SCANOUT_TILE` 可随时切回。

## 6. 施工顺序与判据

1. **先量化**（半天）：`vblank_mode=0 pbench {300²,800×600,1080p} 200 1` × `MWV207_SCANOUT_TILE={0,1}`，
   记录基线（上面的表就是 r38 的基线）。
2. **C2 尺寸档**（1 天）：判据放在 `mwv207_can_tile()`；跑 `tools/gl-regression.sh`（9 项）+
   `tools/perf-suite.sh oss-rNN` 全量对照 + 桌面滚动/视频观感。
3. **C2 真实判据**（再 1 天）：如果尺寸档收益不如预期，改 `get_handle`/`flush_resource` 启发式。
4. **C1 硬件验证**（3–5 天）：`overlay=1` 重启后先用 `modetest`/`kmscube` 或自写工具
   手动把一张缓冲挂 WIN0，确认能看到且位置正确，再谈 X 侧补丁。
5. **止损**：若 C1 第 4 步里 WIN0 与光标/主平面冲突且短期无法解，**停在 C2**，
   把结论回填本文档与 `docs/UNRESOLVED.md`。

## 7. 已验证的"不要做"清单（省时间）

- **不要再猜"厂商用 TILE 扫描出"**：两边显示窗口寄存器逐字段相同，都是线性 pitch 7680（`3c0d296`、`163218d`）。
- **不要把 present 拷贝换成 2D 引擎**：2D 数据通路是闭源知识，见 `docs/HANDOVER-2D.md`（封档）。
- **不要开 `u_threaded_context` 追这个差距**：实测 present 固定开销 0.30–0.40 ms，
  渲染线程化最多把 build 从 1532 提到 ~1700，且有堆损坏（见 `reports/THREADED-CONTEXT-20261116.md`）。
- **不要做"消掉影子拷贝"的通用去重**：`MWV207_SCANOUT_TILE=1` 在我们的 A/B 里
  800×600 present 更快（0.399 vs 0.483 ms，提交 `6b685b7`）；要按 C2 的判据分流，不是全局关。
- **不要指望页面翻转频率提升**：`CRTC_GET_SEQUENCE` 每帧多次是 X 的 vblank 等待，改为异步要在 X 侧动手。

## 8. 现场复现命令（都已验证可用）

```bash
# X 驱动与扩展
grep -E "Loading.*modesetting|Present|DRI3" /var/log/Xorg.0.log
DISPLAY=:0 XAUTHORITY=/home/admin/.Xauthority glxinfo -B

# 显示窗口寄存器（只读）
sudo bash tools/scan-dc/dcdump.sh
~/dev-bin/barmap /sys/bus/pci/devices/0000:07:00.0/resource1 0x990438

# 翻转行为
sudo strace -f -e trace=ioctl -p $(pgrep -x Xorg) -o /tmp/xst.txt &
DISPLAY=:0 XAUTHORITY=/home/admin/.Xauthority glmark2 -b build:duration=8 --fullscreen
grep -c "MODE_PAGE_FLIP" /tmp/xst.txt      # 全屏应 ≈ 帧数；窗口模式应 0

# present 计时（关 vsync）
DISPLAY=:0 XAUTHORITY=/home/admin/.Xauthority vblank_mode=0 ~/dev-bin/pbench 800 600 200 1
```

## 9. 相关提交

- `d0b6209`（AIGLX 软链，最大单项收益）、`4bb9755`/`6fa3ad4`/`39a0c35`（glamor blit 快路径）、
  `ccd9d9c`（每 draw CPU 0.86→0.17 µs）、`f486a1b`（窗口平铺 + 线性影子）、
  `7a1fa9a`（Xorg 进程保持线性）、`6b685b7`（present 去重 A/B：不做）、
  `163218d`/`4d56016`（厂商栈显示窗口取证）、`37918b3`（dmabuf modifier 导入钩子，C1 的前置条件之一）。