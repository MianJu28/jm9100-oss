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

## 10. 移植进展（2026-11-17，C2 尺寸档已落地）

### 10.1 已实现：C2 尺寸档（`mwv207_can_tile()`）

按 §5/§6 的推荐顺序先做 C2 尺寸档，纯 UMD 改动，落在
`src/mwv207-umd/drivers/mwv207/mwv207_resource.c` 的 `mwv207_can_tile()`：

- 新增 `MWV207_SCANOUT_LINEAR_MIN` 调试选项（默认 `1280*720` 像素，`debug_get_num_option`）。
  客户端 `PIPE_BIND_SCANOUT` 资源若 `width0*height0 >= 阈值` 且为单层面/单 mip，
  直接判为**线性**（返回 `PIPE_ERROR_BAD_INPUT` 即不走平铺分支）。
- 线性化后 `mwv207_set_miptree()` 不再为它建 `res->linear` 影子，
  `mwv207_flush_resource()` 的整帧 `mwv207_copy_image()` 消失，
  `mwv207_resource_get_handle()` 直接导出本体并标 `DRM_FORMAT_MOD_LINEAR`。
- Xorg 进程（`is_server`）与阈值以下的小窗口仍走原"平铺 + 影子"路径，避免伤
  `300²` 这种平铺带宽收益远大于一次影子拷贝的场景。
- `MWV207_SCANOUT_LINEAR_MIN=0` 可一键关掉分流做 A/B；
  原 `MWV207_SCANOUT_TILE` 语义不变（全局开关保留作为 fallback）。

### 10.2 实测（r38 安装栈 + 新构建，同机 1920×1080，vblank_mode=0）

**全屏 glmark2 build:duration=8 --fullscreen**（复现 §2.2 的 C2 收益，render-heavy）：

| 配置 | FPS | 帧时 | 判读 |
|---|---|---|---|
| C2 ON（默认尺寸档，线性 scanout） | **943** | **1.061 ms** | 跳过整帧影子拷贝 |
| C2 OFF（`MWV207_SCANOUT_LINEAR_MIN=0`，平铺+影子） | 769 | 1.301 ms | 基线 |
| 全局 `MWV207_SCANOUT_TILE=0` | 937 | 1.068 ms | 与默认尺寸档等效（1080p 下都线性） |

⇒ **+22.6% FPS / −0.24 ms/帧**，与 §2.2 表中"去掉影子拷贝净赚 ~0.25 ms"吻合。
默认尺寸档在 1080p 下与全局线性等效，但保住了小窗口的平铺收益（见下）。

**present 基准（`pbench 200 1`，窗口模式，受合成器 glamor 拷贝掩盖）**：

| 场景 | C2 ON | C2 OFF | 说明 |
|---|---|---|---|
| 300×300 | 0.22 ms | 0.22 ms | 阈值下，均平铺，打平 |
| 800×600 | 0.81 ms | 0.82 ms | 阈值下（48 万像素 < 92 万），均平铺，打平 |
| 1920×1080 | 2.77 ms | 2.83 ms | 线性化只省 UMD 侧一次拷贝，X 合成拷贝仍在 |

（窗口模式收益小是预期的：§2.2 已说明窗口 present 被 glamor 合成拷贝掩盖；
真正的零拷贝收益要在全屏 PAGE_FLIP 路径上看，见上表。）

**全屏 override-redirect 窗口 clear+swap（`pbench-fs 1920 1080 400 1`）**：
C2 ON 0.56 ms vs C2 OFF 0.63 ms（约 −0.07 ms）。窗口模式与全屏模式之间的
差值就是合成器拷贝的固定开销，C2 只负责消掉 UMD 侧那一次。

### 10.3 回归验证

- 编译：`tools/build-umd.sh build` 成功，`libmwv207_dri.so` 正常产出。
- `tools/gl-regression.sh`（DRI_DIR=sim）：**pass=10 fail=0**（query/fbo/texcheck/
  texbench/bufbench/dualblend/dmabuf/rbbench/dualarena/glmark2），无正确性退化。

### 10.4 C1 评估：KMD 侧已就绪，卡在手动上屏验证与 X 侧补丁

第一轮 §10.4 的止损理由在第二轮已逐条推进（2026-11-17）：

1. **C2 已正式部署系统栈**：`payload-49` / active-stack `oss-24.1.2-fullstack-r39`，
   C2 marker=1。全屏 glmark2 956 FPS/1.047ms vs OFF（LINEAR_MIN=0）788 FPS/1.270ms
   （+21%/−0.22ms），`GL_DURATION=1 ./tools/gl-regression.sh` → 10/10 pass。
2. **KMD 切换成功**：`switch-mwv207.sh kmd install` 后加载 srcversion
   `C3A62E5C00106C402380ADE`（6.6.159 新构建），`/sys/module/mwv207/parameters/overlay`=Y，
   lightdm/Xorg/DISPLAY 正常，Xorg 日志 `glamor initialized` + `AIGLX Loaded mwv207`。
   `modetest`/`kmscube` 已 apt 安装。`modetest -M mwv207 -p` 确认每个 CRTC 各多一个
   `type=0`(Overlay) plane：id 35（CRTC 37）与 id 42，format AR24/XR24 均 LINEAR。
3. **WIN0 与 >64×64 光标冲突风险已排除（仅针对 OSS 栈）**：KMD 中 cursor 走独立寄存器——
   `mwv207_cursor_switch()` 写 0x478，`mwv207_cursor_atomic_update()` 写 0x4B4/0x454/0x4A8；
   WIN0 overlay 走 0x990000+ 系列（0x02C enable/0x010 pos/0x050 addr>>6/0x068 pitch/0x0E4 size）。
   两套寄存器完全独立，`mwv207_va.c:50/260` 注释中的"共用"仅指厂商 jmgpu 驱动，
   OSS 栈不存在该冲突。

**上屏硬件验证已通过（2026-11-18）**：用户按上述命令在文本 VT 手动运行 `overlay-test`，
屏幕出现品红矩形 ⇒ overlay plane 可见、位置正确（(100,100) 800×600）、与主平面共存。
结合"OSS KMD cursor 走独立寄存器、无 WIN0 冲突"，**C1 的 KMD 硬件链路全部闭环**。

**X 侧 present→overlay 补丁路径评估（只读，2026-11-18）：结论不实施**，理由：

1. **Xorg 用 modesetting DDX**（`xserver-xorg-core 2:21.1.16`，非闭源 `mwv207_drv.so`）。
   `nm -D /usr/lib/xorg/modules/drivers/modesetting_drv.so` 确认它**不导入 `drmModeSetPlane`**——
   没有把任意客户端缓冲挂到 overlay plane 的代码路径。其 Present flip 只走
   `drmModePageFlip`（翻 PRIMARY scanout）+ `drmModeSetCrtc`；`drmModeGetPlaneResources /
   GetPlane` 仅用于 PRIMARY/CURSOR 枚举。要让窗口 flip 挂 WIN0，必须改 xserver 的 present
   路径（`ms_present.c` / `drmmode_display.c`），即自编并替换整个 DDX，而非改 UMD/KMD。
2. **单 WIN0 窗口的结构性限制**：每 CRTC 只有一个 overlay plane，只能服务**一个**窗口，
   桌面多窗口场景无解。走 overlay 的窗口绕过 X 合成，被遮挡或不在最上层时会显示错误内容，
   还需 damage / 可见性 / z-order 跟踪——不是"多打一次 SetPlane"能解决。
3. **厂商也没走通**：`reports/PRESENT-PATH-20261111.md` §13.1 证实厂商 KMD 的
   `DRM_PLANE_TYPE_OVERLAY` 是空分支，其 DDX 同样只翻 PRIMARY。
4. **工程量/风险**：2–4 周 + 自编 xserver + 单窗口/遮挡/合成处理，中高风险；
   而 C2 已落地并实测 +21%/−0.22ms、回归 10/10 PASS。

**最终结论**：C1 的 KMD 硬件链路已验证可用（overlay 作为"能力原型"保留，模块参数默认 N）；
但 X 侧 present→overlay 全链路受"DDX 无 SetPlane 路径 + 单 WIN0 只能服务一个窗口 +
厂商亦未走通"三重限制，投入产出比远低于 C2。按 §6 第 5 条止损条件 **不实施 X 侧补丁，
维持 C2 为最终交付**。若日后重试 C1，起点是自编 xserver（可复用 `tools/build-xglamor.sh`
的构建/回滚基础设施）改 modesetting present 路径。

### 10.5 待办（C2 真实判据）

尺寸档是 §5 的"中间档"。若要进一步逼近厂商"只翻转的缓冲才线性"的语义，
可在 `mwv207_flush_resource()`/`get_handle()` 里按该资源是否被 Present 过
（`whandle` 导出次数 / usage 含 `DRM_RDWR|DRM_WRITE`）做真实判据迁移；
本轮未做——尺寸档在关键场景已拿到全部可测收益（+22.6%），真实判据的边际
收益不确定且风险更高。
### 10.6 present 路径现状（2026-11-18，Xorg 重启后）

本文件前文"我方 present 0.4–2.7ms 来自两次整帧拷贝"是 11 月中上旬的状态。
11-18 查明其中最关键的一次拷贝走的是 glamor **着色器路径**而不是
`glBlitFramebuffer`：`patches/xserver/0001-glamor-blit-fast-path.patch` 被
ostree 部署切换冲掉（运行中的 `libglamoregl.so` md5 与较新 deploy 一致、不含
`GLAMOR_NO_BLIT`）。重新构建并装载后实测：

- present 800×600 clear+swap：**0.500 ms**（原 0.931 ms）；300×300：0.104 ms（原 0.344 ms）
- `glmark2 -b build`：**1477–1520 FPS**（原 945）；全套 Score 784 → **1215**；glxgears 2375 → 4220
- `GL_DURATION=1 ./tools/gl-regression.sh`：pass=10 fail=0

即"第二次拷贝"已经降到 blit 理论值附近（3.8 MB / 17.3 GB/s ≈ 0.22 ms，余下是
swap/合成/同步），present 不再是轻载场景的唯一瓶颈；剩余轻载差距（vs 厂商
5982 FPS）主要是每帧 fixed overhead 与本文件 §10.4 已止损的结构性零拷贝。
详见 `docs/UNRESOLVED.md` 的"性能回归根因"与"glamor blit 快路径生效后的实测"。
每次 X 重启/部署切换后跑一次 `tools/verify-glamor-patch.sh` 确认补丁还在。

### 10.7 厂商"结构性零拷贝"到底是什么（2026-11-18 取证结论）

回答"厂商 0.014 ms present / glmark2 5982 FPS 是怎么来的、要不要自编 xserver"。
取证对象：`ref/vendor-1.8.0/mwv207-dev_1.8.0.uos_arm64.deb` 解包后的
`/tmp/vendor-extract/usr/lib/xorg/modules/drivers/mwv207_drv.so`（未 strip、含 DWARF）
与 `/usr/src/mwv207-1.8.0.uos` 的 KMD 源码。

**结论一：厂商没有用 DRM overlay 平面，也不需要自编整个 xserver。**
DDX 导入符号里**没有 `drmModeSetPlane`**；只有 `drmModePageFlip` /
`drmModeSetCrtc` / `drmModeAddFB`(v1) / `drmWaitVBlank` /
`PixmapStartDirtyTracking` / `PixmapSyncDirtyHelper` / `present_screen_init` /
`dri3_screen_init`。NEEDED 里**没有 libglamoregl**，DWARF 文件表里
`jmgpu_glamor.c` 只有 **1 行**（即完全没实现 glamor）。真正的大头是自研
**EXA DDX**：`jmgpu_exa.c` 2349 行、`jmgpu_drmmode_display.c` 5091 行、
`jmgpu_present.c` 492 行、`jmgpu_dri3.c` 176 行。

**结论二：它做的是"2D 引擎按 damage 拷贝 + scanout pixmap 直接 page flip"。**
- `jmgpuDrmModeCrtcScanOutCreate/Destroy/Free` 维护一块专用 scanout BO；
- `jmgpuScanOutFlip`(0xf600) / `jmgpuScanOutUpdate`(0xf3f0) /
  `jmgpuScanOutDoUpdate` / `jmgpuSyncScanOutPixmaps` 在 dirty 区域上做拷贝；
- `jmgpuPresentFlip` 反汇编确认结构为：`jmgpuPresentCheckFlip` 放行 →
  `jmgpuFlushInDirect` → `jmgpuDoPageFlip`（带 abort/flip 双回调）→ 清 pending；
- `jmgpuPresentCheckFlip` 只对"覆盖整屏的单窗口"放行，与 X 核心
  `present_check_flip()` 的门禁一致；
- 拷贝走 **2D 引擎**：`jmgpu2dCopyAreaHW`(0x14ad0) 取 `exaGetPixmapDriverPrivate`
  两侧 priv、按 `[priv+72]/[priv+76]` 判断源尺寸，配套
  `jmgpu2dCopyArea/jmgpu2dFillRect/jmgpuFlush2dCmd/jmgpuWait2dIdle`。

**结论三：2D 命令编码完全在用户态，内核只当搬运工。**
厂商 KMD `jmgpu_garbage.c:950-975` 的 `j9_handle_j9m_muriculate()` 收到
`struct drm_jm_j2d_send_cmd` 后直接 `j9_handle_insurrecto(gdev->p2d, args->cmd,
args->size, nowait)`（`jmgpu_define.c:449`），后者只做 `size & 0x3` 校验、加
`enginelock`，然后 `p2d->engine->ops->input_cmds(engine, pcmds, size, no_wait,
from_user=1)`。**内核不解析 2D 命令字**。这就是我方缺的那块：我方 KMD 有
ring/fence 框架（`src/kernel-6.6/mwv207_pipe_2d.c`），但 UMD 侧没有任何 2D
命令流（`docs/HANDOVER-2D.md` 记录"draw 数据面寻址"仍未解出）。

**结论四：厂商 DDX 二进制不可直接复用。** `nm -D --undefined-only` 显示它强依赖
厂商 KMD 私有 ioctl：`drm_jmgpu_bo_create/destroy/mmap/export_to_fd/
import_from_fd/get_handle/invalidate/query/read_rect/write_rect/xfer_to_dev/
xfer_from_dev/clean`、`drm_jmgpu_create/close`、**`drm_jmgpu_j2d_send_cmd` /
`drm_jmgpu_j2d_wait_idle`**。我方 KMD 没有这些 ioctl，直接装 `mwv207_drv.so`
不可能起来；要复刻就得写一个等价 DDX + 补 2D 命令通路。

**结论五：我方 present 其实已经走 page flip，差距不在"翻转"。**
kprobe 实测（`drm_mode_page_flip_ioctl` / `drm_mode_atomic_ioctl` /
`drm_mode_setcrtc` / `drm_mode_addfb2` / `drm_mode_dirtyfb_ioctl` 五个探针）：
1920×1080 全屏 override-redirect 连渲 2 s（4862 帧）→ `pgflip=202`、
`addfb2=202`、`atomic=0`、`setcrtc=0`、`dirtyfb=0`，即**每帧一次 page flip**；
静止时 `pgflip=0`。800×600 窗口同样测法 `pgflip=0` —— 小窗口走的是
`present_copy_region()` 的整块 `CopyArea`，X 核心
`present/present_scmd.c:61 present_check_flip()` 要求
`RegionEqual(&window->clipList, &root->winSize)`，**非全屏窗口在 X 核心里
根本不允许 flip**（这一条改 DDX 也没用，必须改 X 核心或自研 DDX）。

**结论六：present 的 0.29–0.33 ms 就是"一次全屏纹理采样绘制"。**
自写探针 `/tmp/filltex.c`（全屏 textured quad 画到 FBO）：800×600
NEAREST = **0.2481 ms**（1.93 Gpx/s）、纯色几何 0.1273 ms、LINEAR 0.2779 ms；
1920×1080 NEAREST = 0.8939 ms。这个数字与窗口态 present 净成本
（windowed 0.482 ms − offscreen 0.189 ms ≈ 0.29 ms）基本相等。对照：
`glBlitFramebuffer` 800×600 = 0.2053 ms（18.70 GB/s）；`glReadPixels` 走
DMA 回读只有 **2.7440 ms（0.70 GB/s）** —— 所以"DMA 引擎做 present 拷贝"
这条路可以直接排除。

**结论七：真正的大头是渲染吞吐，不是 present。**
- 通用 draw 填充率：800×600 全屏三角形 **5.9 Gpx/s（23.6 GB/s）**，
  1920×1080 6.2 Gpx/s；对照 `glClear` RGBA8 是 **458.9 Gpx/s（1835 GB/s）**。
- 片元着色边际成本极大：300 次 sin 的重片元从 5.90 掉到 **0.32 Gpx/s**；
  而 `discard` / colorMask off（不写色）只省 20%（7.36 Gpx/s）。
- 深度测试在高 overdraw 下贵 **2.8×**（20000 小三角形：2.5153 ms vs 0.9069 ms）；
  blending 免费。
- 顶点吞吐饱和 ~810 Mverts/s，顶点着色不是瓶颈。
- 我方 glxgears 6392 FPS vs 厂商 17621 FPS（2.8×）；glmark2 offscreen
  800×600 build 5250 FPS 已接近厂商 windowed 5982 FPS（说明小尺寸下渲染
  差距不大），差距随分辨率/overdraw 放大。

**结论八：early-Z 两个候选位在 JM9100 上实测无效（已回滚）。**
按厂商头文件补了 `AQ_DEPTH_CONFIG.EARLY`（0x1400 bit16，普通 LESS 也开）与
`RA_CONTROL.EX_EARLY_DEPTH_IN_APP`（0x382 bit5），并做了 discard 反例保护。
5 轮交替 A/B（`glmark2 -b build:duration=2`）：2072/2074、2070/2070、
2073/2072、2072/2075、2070/2073 —— **完全无差异**；`/tmp/earlyz`（近→远
全被深度拒绝）与 `/tmp/layers 1`（高 overdraw）也无差异，bit5 甚至略慢。
`GL_DURATION=1 ./tools/gl-regression.sh` pass=10 fail=0（无回归）。
既然零收益又带正确性风险（discard/alpha 交互），改动已 `git checkout` 回滚，
结论写进 `docs/UNRESOLVED.md`，避免以后重复试。

**要不要自编 xserver？** 不需要自编 xserver 核心，但需要写一个**新的 DDX 模块**
（EXA 或 glamor 皆可），并补上 2D 引擎命令通路；工作量主体在 2D 命令编码
（闭源知识）而非 X。按当前证据，**性价比最高的仍是已经拿到的两层**：
C2 线性翻转（+21–22%）与 glamor blit 快路径（+57%），二者均已上系统栈。

## 11. 自研 X DDX（mwv207oss）+ WIN0 overlay 零拷贝 present（2026-11-18）

§10.7 的结论是"厂商不需要自编 xserver，但需要一个新 DDX"。本节记录把这条路
真正做出来的第一版：**不动 X 核心的翻转门禁就不可能有窗口态零拷贝**，所以采用
"X 核心加一组版本 2 钩子 + 自研 DDX 用 WIN0 平面直扫"的组合。

### 11.1 为什么必须同时改 X 核心

窗口态 Present 走 `present_check_flip()`（`present/present_scmd.c:61`），它按顺序
拦掉：无 DDX flip 钩子 → window pixmap 三项比对 → **`window->clipList != root->winSize`
（即窗口不覆盖整屏，含 WM 装饰）** → `x_off/y_off` 非 0 → `valid != 整屏` →
窗口 x/y 非 0 或尺寸不等于 pixmap。真实窗口在带装饰的 WM 下**永远被第三条拦掉**，
到不了 DDX。实测佐证：把屏幕改成 800×600、窗口盖满整屏，kprobe `pgflip` 仍为 0；
只有 override-redirect 真全屏才 pgflip=126/3s。

因此核心加了版本 2 钩子（`patches/xserver/0003-present-overlay-zero-copy.patch`，253 行）：

- `present/present.h`：`PRESENT_SCREEN_INFO_VERSION` 1→2，`present_screen_info_rec`
  末尾追加 `present_check_overlay_ptr check_overlay` 与
  `present_overlay_flip_ptr overlay_flip`；
- `present/present_priv.h`：`present_vblank.overlay`、`present_screen_priv.flip_overlay`；
- `present/present_vblank.c`：flip 判定**先问 overlay**，命中即标记
  `overlay + flip + sync_flip`（簿记复用 flip 队列，不引入新队列）；
- `present/present_scmd.c`：`present_execute()` 的 flip 分支里 overlay 走
  `overlay_flip()`，成功直接 return（**不** `present_set_tree_pixmap`、**不**
  `DamageDamageRegion`，因为屏幕 pixmap 根本没有被写过）；失败则清标志回落到拷贝路径；
  `present_flip_notify()` 记 `flip_overlay`；`present_restore_screen_pixmap()` 在
  overlay 场景下按**窗口偏移**把最后一帧拷回屏幕 pixmap；`present_check_flip_window()`
  的 pending/current/queued 三处改为走 `present_check_overlay`；
  `present_scmd_can_window_flip()` 在 overlay 钩子存在时直接返回 TRUE（让客户端
  拿到可直扫的缓冲区/modifier）。

### 11.2 自研 DDX（`src/xddx-mwv207/`）

从 xserver 21.1.16 的 `modesetting` 原样 fork（10 文件约 328 KB），符号重命名
`modesettingRec→mwv207Rec`、`modesettingPtr→mwv207Ptr`、`ms_*→mwx_*`（`ms->`
结构体指针保持不变），模块身份统一为 `mwv207oss`
（`DriverRec mwv207oss` / `XF86ModuleData mwv207ossModuleData` / `scrn->driverName`），
所以能与系统的 modesetting 并存、互不干扰。在此基础上加：

1. **overlay 平面枚举**（`src/xddx-mwv207/drmmode_display.c`）：
   `drmmode_crtc_create_planes()` 原来对非 PRIMARY 平面直接丢弃，现在把
   `DRM_PLANE_TYPE_OVERLAY` 记录为 `overlay_plane_id` + `props_overlay_plane[]`，
   并把主平面命中时的 `break` 改 `continue`（否则扫不到后面的 overlay）；
   `is_plane_assigned()` 同时比对 `plane_id` 与 `overlay_plane_id`。
2. **`drmmode_overlay_plane_set()` / `drmmode_overlay_plane_disable()`**：
   直接 atomic 提交 WIN0 的 `FB_ID/CRTC_ID/SRC_*/CRTC_*`，`NONBLOCK`；换帧时释放
   上一个 FB（内核在平面状态被替换前仍持有引用，安全）。
3. **Present 钩子**（`src/xddx-mwv207/present.c`）：
   - `mwx_present_check_overlay()` 放行条件：CRTC 开着、`vtSema`、glamor 可用、
     已枚举到 overlay 平面、窗口是 root 的直接子窗口且**未被重定向**、
     窗口尺寸等于 pixmap、窗口坐标非负且不越出 CRTC、
     **`clipList` 等于窗口矩形（即没有任何窗口遮挡它）**、depth 24/32、
     gbm stride 满足 **`stride & 0x3f == 0`**（KMD 要求）、modifier 为
     `DRM_FORMAT_MOD_LINEAR`、格式为 ARGB/XRGB/ABGR/XBGR8888。
   - `mwx_present_overlay_flip()` 提交平面更新后，用
     `mwx_drm_queue_alloc()+mwx_queue_vblank(MS_QUEUE_RELATIVE,0)` 在下一个 vblank
     回 `present_event_notify()`；**任何失败路径都必须补一次
     `present_event_notify(event_id,0,0)`**，否则 `flip_pending` 永久占位、Present 卡死。
   - `mwx_present_unflip()` 开头新增 overlay 分支：只关平面、不发 flip。
4. `mwv207Rec` 增加 `overlay_active` / `overlay_window`。

### 11.3 构建（关键坑）

`tools/build-xddx.sh` 一把梭（`--build/--install/--rollback/--status/--test`）：

- 构建目录必须是 **开了 GLX 的 `build-glx`**。系统 Xorg 会加载
  `/usr/lib/xorg/modules/extensions/libglx.so`，它需要 `glxServer` 符号；
  `-Dglx=false` 编出来的 Xorg 没有该符号，GLX 会整体不可用。
- `-Dglx=true` 要 `dri.pc`，而 mesa 24 起不再提供 ⇒ 脚本在 `/tmp/pkgconfig/dri.pc`
  造一个最小 shim（只为取 `dridriverdir` 与链接 `dl`）。
- 符号面核对：我们的 Xorg 导出 1947 个符号、系统 2008 个，差的 62 个**全是 DRI1**
  （`DRICreateContext` 等）；逐个 `nm` 检查已装模块，**没有任何模块依赖它们**。
- DDX 模块 dlopen 验证（先 `dlopen libxorgserver.so RTLD_GLOBAL` 再 `dlopen` 模块）：
  `mwv207ossModuleData` / `mwv207oss` 均可解析。

### 11.4 安装/回滚与验证

```bash
./tools/build-xddx.sh --build        # 只构建
sudo ./tools/build-xddx.sh --install # 装 DDX + 打了补丁的 Xorg + OutputClass 配置
sudo systemctl restart lightdm       # 手动执行(会杀掉 pty 里的会话)
./tools/verify-overlay-present.sh    # 一条命令判定是否真的走了 overlay
sudo ./tools/build-xddx.sh --rollback # 一键回滚(恢复旧 Xorg/删模块/删配置)
```

`--install` 会同时替换 `/usr/lib/xorg/modules/drivers/mwv207oss_drv.so` 与
`/usr/lib/xorg/Xorg`（**必须成对换**：旧核心不认识版本 2 钩子，新核心缺 DDX 也没有
overlay），并写 `/usr/share/X11/xorg.conf.d/10-mwv207-oss.conf`
（`MatchDriver "mwv207"` + `Driver "mwv207oss"`）。两者的 `.mwv207-bak` 是回滚备份。

### 11.5 当前状态（2026-11-18 13:20）

已构建并安装完毕，**尚未重启 X**（重启会经 pty SIGHUP 终止本会话，按用户要求由
用户手动执行）。预期收益：窗口态 present 从 0.49 ms（glamor blit 拷贝）
降到 0.1 ms 量级；glmark2 build 从 1477 FPS 向离屏基线 5396 FPS 靠近
（厂商窗口态 5982 FPS ⇒ 差距从 2.87× 收窄到约 1.11×）。
