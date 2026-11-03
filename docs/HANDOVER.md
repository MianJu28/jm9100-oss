# 交接文档 — JM9100 (MWV207) 开源驱动适配（2026-11-03）

> 目的：把当前进度、环境、方法学与"下一步做什么"完整交给下一个会话。
> 本文档随进度更新；未修问题权威清单在 `docs/UNRESOLVED.md`（343 行）。

## 1. 当前状态（一句话）

**r13 已部署并验收通过**。桌面零 env 下 GL 4.0 / GLES 3.2 / `Video memory: 2048MB`，
glxgears 110 FPS，Firefox 硬件渲染正常（内容+性能都修完），OpenCL/VA-API/VDPAU
全走我们的栈，玲珑应用正常，自建 KMD 在跑（srcversion `D507FC2774D547E7BEBA182`）。
r13 在 r12 基础上新增 **10bit HEVC 解码面**（P010/P016，见 §3 末条）。

## 2. 系统与仓库布局

| 项 | 值 |
|---|---|
| 机器 | Deepin 25 aarch64，内核 `6.6.155-arm64-desktop-hwe`，GPU `[0731:9100]` PCI 07:00.0，HDMI-1 1920×1080@100Hz，无免密 sudo（用 `printf '666\n' \| sudo -S`） |
| 仓库 | `/home/admin/jm9100-oss`（git，远端 `git@github.com:MianJu28/jm9100-oss.git`，已推送同步） |
| 源码 | `src/mwv207-umd/`（UMD，唯一可改处；上游树 `ref/oh-mesa/.../src/gallium/{drivers,winsys}/mwv207` 是指向它的符号链接）、`src/kernel-6.6/`（KMD，已并入本仓库） |
| 已装栈 | `/usr/local/lib/mwv207-oss/`（r13，DRI md5 `f80fa8e0…`），`/lib/modules/…/updates/dkms/mwv207.ko`（自建 KMD） |
| 载荷 | `payload-37`（`24.1.2-fullstack-r13`）；历史快照在 `/var/lib/mwv207-oss/switch-history`（留 5 个） |
| 快速回归副本 | `/tmp/gd3/`（与已装栈同 md5）。实验构建流程：改 `src/mwv207-umd` → `ninja -C ref/oh-mesa/User_source_code/mesa-24.1.2/build-full src/gallium/targets/dri/libmwv207_dri.so` → `cp` 到 `/tmp/gd3/` → 用 `LIBGL_DRIVERS_PATH=/tmp/gd3 MESA_LOADER_DRIVER_OVERRIDE=mwv207` 测 |
| 出包安装 | `./tools/make-payload.sh -v 24.1.2-fullstack-rN` → `printf '666\n' \| sudo -S ./switch-mwv207.sh umd install --force`（**install 调用常显示 interrupted，实装已完成——必须用 `status` + md5 比对确认，勿盲目重跑**） |
| 构建陷阱 | 性能实验必须用 `build-full`（`build-glvnd` 是旧二进制）；`meson setup --wipe` 会把 glvnd 从 auto 变 enabled → 必须显式 `-Dglvnd=false -Dgles1/2=enabled` |

## 3. 已完成里程碑（提交索引）

- 全功能栈（GL/VAAPI/VDPAU/rusticl）、GLES 3.2 原生点亮、自建 KMD 真机运行、
  切换脚本统一（`switch-mwv207.sh`，umd/kmd/all 三层）。
- **Firefox 图元全丢**：根因 = VS 里 RGBA32F `texelFetch` 只回 64 位
  （`nir/mwv207_lower_128bit()` 只对 FS 生效）→ 提交 `bda3749`（取样）+
  `d7c52a3`（重编译记录），最小复现
  `tests/vstf-128bit/`（`/tmp/wrreplay` 运行器，判据 = 与 llvmpipe FB_HASH 逐字节一致）。
- **Firefox 性能**：① 重编译判据误用 `ctx->dirty`（每次 submit 被重置为全量）→
  "128 位模式指纹参与着色器缓存键"（`mwv207_shader.c:985-1015`，`built_fp_*`）；②
  128 位纹理 split/merge 对 WC 显存逐标量拷贝（~100MB/s）→ 逐行 memcpy。WebGL 36→42+
  FPS（llvmpipe 28.5），CSS 页 97-99 FPS（vsync 上限）→ 提交 `d7c52a3`。
- **GitHub 概率性崩溃**：dual-source blend 的 `sampler_index`（`num_samplers`）与
  视图槽位（`num_sampler_views`）不一致 + 无判空 → SIGSEGV `get_rt_texture_coord`
  → 提交 `40d5d76`。
- **健壮性批次 1/2 + GBM/Wayland**：W6/W7/W12/E2/E4/E7/E9/E10/F.3.2/F.3.3/O4 +
  GBM EACCES（render 节点回退）→ 提交 `841e300`。玲珑 CDI 冲突修复（EGL ICD
  原地改写，**不要删厂商 `10_mwv207.json`**）。
- **A 类修复（`5dee412`，r12 已部署）**：E5 双面模板双重互换（0x528 恒取
  `stencil_back_reference[0]`）、F.3.1 反汇编 type 位序（`(bit0<<2)|bit1_2`）、
  F.4 三条（link_shader 返回 true / shared_atomic 判空 / RA 收敛上限 32）、
  O5 失败 fence（`video_cs.c` 对 fd<0 不创建 fence）。E6 复核结论：**不改**（与
  llvmpipe 同为 CPU 时钟语义）。
- 显存修正 `PIPE_CAP_VIDEO_MEMORY` = 2048（KMD 实测 2GB）；显存单位历史教训：
  该 cap 单位是 MB。
- **10bit HEVC 解码面（`f0c7605`，r13 已部署）**：`mwv207_is_video_format_supported()`
  对 P010/P016 改为按 `R16_UNORM`/`R16G16_UNORM` 的 `SAMPLER_VIEW` 支持取 AND（仍与
  profile 白名单取 AND）。真机 640×360/30 帧 Main10 解码 + `vaExportSurfaceHandle`
  （DRM_PRIME_2）导出与软件解码逐位一致；8bit HEVC/H264 回归同样逐位一致。**GL 侧
  16 位采样恒 0（C1）仍在**：2026-11-03 已定位为 0x1F76 不解码
  `TX_EXT_CONFIG.FORMAT` 码 0x32–0x36（详见 UNRESOLVED C1），10bit 帧在 GL 里
  采样/缩放仍不正确。

## 4. 关键方法学（重复踩过的坑，务必遵守）

1. **探针必须先在 llvmpipe 上跑出"符合预期"**才能用来判定我们的驱动；判据始终是
   "两驱动逐条/逐字节一致"，不要用自己手算的期望值。
2. 桌面 GL 2.0+ 入口必须 `eglGetProcAddress`（`PFNGL##U##PROC` 拼名要与 GL 头逐字
   一致；GL 1.1 入口直接调用）；`glUniform4f` 不加载会因 double 传参出垃圾 uniform；
   纹理必须设 `MIN_FILTER`（默认 mipmap → 不完整 → 采样恒 0）；探针缓冲区按格式
   尺寸分配（`uint4` 这类 typedef 在该上下文不存在）。
3. 驱动回调内**禁止**调 `texture_map/buffer_map`（SIGSEGV）；BO 读回用
   `ws->bo_wait + ws->bo_map`（见 `mwv207_query.c:245-248`）。
4. 测 FPS 前必须 kill 所有 `/usr/lib/firefox/firefox` 并用 `xprop -id <win>
   _NET_WM_PID` 杀 `Firefox Crash Reporter`，标题匹配用 `/tmp/title.sh`；直方图裁切
   用 `xwininfo -id` 的真实几何（历史教训：固定 crop 量错窗口）。
5. `pkill -f` 会杀自己的 shell；按 `readlink -f /proc/<pid>/exe` 判定。
6. core dump：`echo "/tmp/coredir/core.%e.%p" > /proc/sys/kernel/core_pattern`
   （崩溃在 GPU/Renderer 子进程，gdb 跟父进程抓不到）。
7. `glmark2` Score 波动 704-1299，**不能当回归判据**；回归用
   `tests/query/run-query-tests.sh`（需 `DRI_DIR=/tmp/gd3
   LD_LIBRARY_PATH=/usr/local/lib/mwv207-oss`）+ wrreplay 三组 + glxinfo + 火狐
   diagpage 直方图（红 2.3%/绿 3.0%/蓝 2.5%/黄 2.0%/白 9.3%）。
8. `docs/UNRESOLVED.md` 与 `api/11-FIX-FEASIBILITY.md` 的历史结论会过期，采信前实测复核。
9. 测试装置：`/tmp/fps.html`（FPS 写进 document.title）、`/tmp/css.html`、
   `/tmp/diagpage.html`、`/tmp/ffp4/user.js`、`/tmp/hist2.py`、`/tmp/ts4.sh`、
   `/tmp/swapbench`、`/tmp/wrshade/`、`/tmp/wrreplay`、`/tmp/tsprobe`。清理时保留。

## 5. 剩余工作（按优先级）

### 已拍板待办
- **10bit HEVC/VP9 解码面（已完成 2026-11-03，`f0c7605`）**：`P010/P016` 在
  `R16_UNORM`/`R16G16_UNORM` 可采样时接受，VA 前端开始广播 `YUV420_10BPP`；
  真机 Main10 硬解 + `DRM_PRIME_2` 导出与软件解码逐位一致。GL 侧 16 位采样
  仍恒 0（C1：0x1F76 不解码 0x32–0x36，2026-11-03 定位），渲染路径仍受限，
  出厂前需真机装一次验证。
- **C 类诚实性取舍（需用户拍板，见 §6）**。
- **D1 VP9 硬解**（最有价值功能项）：硬件档位 `0x00001F76` 确认 `vp9_support=1`，
  解析器/bool 解码器已编入库（零调用），缺 `mwv207_dec_vp9_set_regs()` 分支；
  H264/HEVC 的 set_regs 可作模板（`mwv207_dec_h264.c:2321`、`mwv207_dec_hevc.c:1231`）。
- **N14 KMD reset 不 signal 在飞 fence**（唯一可能卡死应用的 KMD 缺陷，需重启验证）。

### B 类（中成本，可做）
E8/D6 多视口（先确认硬件是否有多组 viewport 寄存器，无则把 cap 降为 1）、
E3 压缩纹理 blit 的 `task_info` 覆盖（`mwv207_blit.c:568-584`）、K9-①/K9-②/K12
（KMD 竞态类，需重启）、N2（`hw_submission=30` 改小）、N4（DMA run_job 忙等 5s）、
K11（suspend 次序）。

### 不建议投入
C-2 16 位采样编码（硬件边界）、D7 Vulkan（官方闭源也没有）、C-5/C-6（厂商库无
来源）、N10/N9（实测无影响）、火狐 WebGL 42-44 FPS 剩余瓶颈（在 WebRender 帧调度，
总 CPU 仅 2.4-3.6%）、chrome 文字错位（用户已搁置）、openuuyc 单实例 UX（用户已确认没问题）。

## 6. C 类三项待拍板（用户没选就不动）

| # | 问题 | 选项 |
|---|---|---|
| C1/C3 | R16G16_UNORM/R16_SNORM/R16G16_SNORM 采样恒 0。**2026-11-03 已定位为硬件边界**：把正常纹理强制换码 0x36 也读 0，而码 7/24–26 正常，tiling/align/NORM/new-mode/bit23/31 全无效 → 0x1F76 不解码归一化 16 位码 0x32–0x36。10bit **解码面**已随 `f0c7605` 启用（P010/P016 + DRM_PRIME_2 导出逐位验证） | A 保留声明+文档注明 / B 回退只留 R16_UNORM / C 继续逆向（已证伪：非编码问题，2026-11-03） |
| C2 | 高级混合 8/15（7 种方程静默不混合） | 保留声明 = 原生 GLES 3.2 的必要代价 / 撤销则 GLES 3.2 退回 3.1 |
| I-3 | H264 High422/444 声明 vs `chroma_format_idc=1` 硬置 | 修 set_regs 传真实 chroma_format（硬件能力需实测）/ 删两行 vainfo 声明 |

## 7. 每次交付的标准流程

1. 改 `src/mwv207-umd`（每处带 `FIX(jm9100-oss)` 注释说明"为什么"）。
2. `ninja -C ref/oh-mesa/User_source_code/mesa-24.1.2/build-full src/gallium/targets/dri/libmwv207_dri.so`
   → `cp` 到 `/tmp/gd3/` → 跑回归四件套。
3. `./tools/build-umd.sh check`（FIX 文件计数应递增）。
4. `git commit`；需要时 `tools/make-payload.sh -v …-rN` 出包 → `umd install --force`
   → **status+md5 确认实装** → 零 env 验收（glxinfo/glmark2/query tests）。
5. 更新 `docs/UNRESOLVED.md` 与 README 版本号，push。**不再导出补丁**（用户
   2026-11-03 决定，`patches/` 已删除）：改动以 git 提交为唯一记录。

## 8. 杂项备忘

- openuuyc（UU 远程被控端）：后台服务 `app-openuuyc.service` 登录即跑
  （`OpenUUYC gui --background`），持"控制中心"单实例锁
  `/run/user/1000/openuuyc-control-center-1000.lock`；桌面点击的第二实例按设计退出
  （**非故障**，用户已确认没问题）。切栈重启 lightdm 会断它的远程会话——切换前可
  先 `systemctl --user stop app-openuuyc`。
- `EGL_ICD` 原地改写逻辑在 `switch-mwv207.sh`（`VENDOR_EGL_ICD_ORIG` 备份 +
  `library_path` 指向 `libEGL_jm.so.0`）；`/etc/cdi/jmgpu.yaml` 依赖该文件存在。
- KMD 热切换不可行（refcnt 恒 1，内核内部引用），只能重启；脚本有 `kmd stage`
  （只放置）与 `refresh_initramfs`。
- 上游枚举陷阱：`p_format.h` 只在 `ref/mesa-22.3.7/...`（`find` 要 `-L`）。
- **切换脚本的 Xorg 日志要按实际显示号解析**（2026-11-03 修）：display 不一定是
  `:0`——`:0` 的 Xorg 崩溃后 lightdm 会起在 `:1`，日志写到 `Xorg.1.log`。原先写死
  `/var/log/Xorg.0.log`，install 等不到新日志 → 误判"软件渲染"→ 自动回滚 + 黑屏。
  现在 `xorg_log()` 从 `pgrep -a Xorg` 解析 `:N`（等待循环内每轮重解析），
  `umd verify`/`umd status`/watchdog 同样走它。
- **install 前把安装目录恢复成干净快照**：`umd install` 的快照取自"当前安装目录"，
  如果之前手工 `cp` 过库，失败回滚会"复原"这份脏状态（2026-11-03 10:44 实证：
  回滚后仍是新库）。先 `umd rollback` 回最近一次干净快照（或从备份恢复）再 install。
- **补丁流程已废止**（2026-11-03 用户决定）：`patches/` 目录与"导出补丁"步骤一并
  删除，之后不再生成任何 `*.patch` 归档；版本记录只靠 git 提交，需要对外提交时
  直接引提交哈希（或 `git format-patch` 临时生成、不入库）。
