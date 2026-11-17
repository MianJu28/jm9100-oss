# 交接文档 — JM9100 (MWV207) 开源驱动适配（2026-11-17 15:30 更新）

> 目的：把当前进度、环境、方法学与"下一步做什么"完整交给下一个会话。
> 本文档随进度更新；未修问题权威清单在 `docs/UNRESOLVED.md`（含 2026-11-10 的 §0.1 速览）。
> VP9 专项见 `docs/HANDOVER-VP9.md`；2D 引擎专项见 `docs/HANDOVER-2D.md`；
> present 零拷贝翻转架构专项见 `docs/HANDOVER-ZEROCOPY-PRESENT.md`。

## 1. 当前状态（2026-11-17 15:30）

**r38 已部署并验收通过**：活动栈 `oss-24.1.2-fullstack-r38`（`payload-48`，2026-11-17 15:22
安装，快照 `20261117-152237`，安装目录 219M）；DRI `fed71d54deb79d6e6944f0f8e3abd2a7`、
VA `667937d55e9e1ed346804c79fa2546db`，与 `payload-48/` 内文件一致；安装后实测
`gl-regression` **pass=9 fail=0**、`vp9-regression` **pass=20 fail=0**、
`verify-installed` **PASS=21 FAIL=0 SKIP=3**；用户实机确认火狐不再崩溃、文字正常。自建 KMD 在跑
（srcversion `BABA169D1C80304D666559D`，含 2026-11-16 的 2D 提交链 4 KB 对齐修复与
2D `diag` 日志；**运行内核已升到 `6.6.159-arm64-desktop-hwe`**，6.6.155 的 headers 只能编
旧版，DKMS 已为 155/159 各建一份，6.6.143 停止维护）。

**r38 内容（仅 UMD 变化，两条）**：
1. 补 `pipe_screen::is_dmabuf_modifier_supported` / `get_dmabuf_modifier_planes`，让"导出的
   dmabuf 能被导回"（此前必然 `EGL_BAD_MATCH`；Firefox 的 dmabuf 传递依赖它）；
2. **修着色器 arena 只放得下 8 个变体**（越界写显存 + `assert` abort = 火狐"打开网页就
   崩溃 / 文字整页乱码"的根因），起步 2 MB 并把越界改成一次性告警。
新增探针 `tests/gl-probe/{dmabuf,dualtex,dualarena}.c`，回归扩到 9 项。详见
`docs/UNRESOLVED.md` 的 dmabuf 与"着色器 arena"两节。

桌面零 env 下 GL 4.0 / GLES 3.2 / `Video memory: 2048MB`；
OpenCL/VA-API/VDPAU 全走本栈。

验收命令（2026-11-13 实跑，全绿）：

| 命令 | 结果 |
|---|---|
| `tests/verify-installed.sh BABA169D1C80304D666559D` | PASS=21 FAIL=0 SKIP=3（SKIP 需 root） |
| `DRI_DIR=/usr/local/lib/mwv207-oss tools/gl-regression.sh` | pass=8 fail=0（glmark2 793–1077；第 7 项是 dmabuf 往返） |
| `tools/vp9-regression.sh` | pass=20 fail=0 |
| `tests/vp9-intraonly/run-vp9-intraonly.sh` | 7/7 IDENTICAL（含 3 个 `show_existing` 重放帧） |
| `tests/vp9-resize/run-vp9-resize.sh` | 1 IDENTICAL + 1 KNOWN-DIFF（滤波核差异，§1.9） |

**性能基线（r35，关掉 Firefox 的干净环境，`~/perf-oss-r35-clean.txt`）**：
`glmark2 -b :duration=2` **1290**；glxgears **6527 FPS**；present 300×300 **0.068 ms/帧**、
800×600 0.352 ms、1080p 1.388 ms；纹理上传 1024²/小块 **0.02 ms/次**（修法 3 前 5–15 ms）、
1024² 全张 1.27 ms；buffer `glBufferSubData` 4 MB **0.30 ms（13.2 GB/s）**、`glMapBufferRange`
读 **0.26 ms（14.9 GB/s）**（修法 4 前 2.10 / 24.78 ms）；`glReadPixels` 4 MB 3.54 ms（DMA 引擎
约 2 GB/s 的下限）；解码 1080p（不下载帧）**VP9 161 / H.264 304 / HEVC 311 fps**、
`scale_vaapi` 1080p→720p **145 fps**。同口径厂商（`reports/VENDOR-BASELINE-20261113.md`）：
glmark2 5075、build 14537 vs 我方 1930、present 0.014 ms（异步口径，不可直接比）、
H.264 259 / HEVC 270 / VP9 143 —— **解码已持平或反超**，差距集中在 draw 密集场景的
present 与每帧固定路径（详见下）。

已关闭的实质项（每一项都有提交、探针与文档，细节在 `docs/UNRESOLVED.md`）：
- 功能：VP9 Profile0/Profile2（含 show_existing 重放帧、跨尺寸参考）、HEVC Main/Main10、
  H.264 8bit（含 High10 明确拒绝）、VPP（`scale_vaapi` 原生可用）、12bit VP9 明确报错拒绝、
  JPEG baseline 判定为需闭源 HAL（见 §1.10）。
- 健壮性：N1/N2/N4/N9/N14/N16/N17/N19、20 处 `BUG_ON` 收敛、K9/K11/K12。
- 性能：VP9 解码 8.4→220 fps（修法 1）、纹理上传 5–15 ms→0.03 ms（修法 3）、
  buffer 6–80×（修法 4）、client-array 每 draw 26–44 µs→0.42 µs（N19）、
  present 1.9–2.1×（glamor blit 补丁）、窗口平铺/线性影子（jellyfish 2.9×）。
- 判定为硬件/闭源边界、不再投入：2D 引擎（启动位依赖未发布函数）、DE 平铺扫描出
  （`DCREG_FRAME_BUFFER_CONFIG` 在本机镜像不可达）、JPEG baseline（`DEC_E` 之外还有未公开
  启动条件）、编码（本 SKU `has_enc=0`）、Vulkan（无驱动无 ICD）、MVC/AVS（Mesa/libva 无
  profile 可达）、多视口（硬件能力未知）、12bit VP9（P010 路径止于 10bit）、
  Firefox WebGL 帧时间（WebRender 调度，不在 GL 侧）。

**唯一仍在清单上的性能疑点**：VP9 1080p 解码在 r31 记 217 fps、r35 干净环境 165 fps
（同文件同命令，`REPLAY_LINEAR` A/B 无差别，H.264/HEVC 反而变快）——尚未解释，见
`reports/VENDOR-BASELINE-20261113.md` 末尾。

## 2. 系统与仓库布局

| 项 | 值 |
|---|---|
| 机器 | Deepin 25 aarch64，内核 `6.6.155-arm64-desktop-hwe`（143 也装了 DKMS 包，但不是启动内核），GPU `[0731:9100]` PCI 07:00.0，HDMI-1 1920×1080@100Hz，无免密 sudo（用 `printf '666\n' \| sudo -S`） |
| 仓库 | `/home/admin/jm9100-oss`（git，远端 `git@github.com:MianJu28/jm9100-oss.git`，已推送同步） |
| 源码 | `src/mwv207-umd/`（UMD，唯一可改处；上游树 `ref/oh-mesa/.../src/gallium/{drivers,winsys}/mwv207` 是指向它的符号链接）、`src/kernel-6.6/`（KMD，已并入本仓库） |
| 已装栈 | `/usr/local/lib/mwv207-oss/`（运行中 **r37**：DRI md5 `0f0851ed955c…`、VA md5 `6c455b9a2160…`），`/lib/modules/…/updates/dkms/mwv207.ko`（自建 KMD `C588E9673F984406AF46A2F`，`kmd stage` 会同时刷新 `/var/lib/dkms/mwv207-oss/1.0.0/$KVER/$arch/module/mwv207.ko` 以免 `dkms status` 报 Diff）；历次安装快照与 md5 在 `./switch-mwv207.sh status` 与 `/var/lib/mwv207-oss/switch-history` |
| 载荷 | 运行中 `payload-46`（`VERSION = 24.1.2-fullstack-r37`，DRI `0f0851ed…`、VA `6c455b9a…`；在 r36 基础上修好**双源混合 lower**（缓存键、输出改写、texelFetch、fadd+fneg 因子、scissor 裁剪 RT 拷贝、拷贝纹理缓存）并让**发射器支持 `nir_op_fsub`**，新增 `tests/gl-probe/{dualblend.c,dualbench.c}`）。`tools/make-payload.sh` 每次会 `rm -rf payload-46` 重建并覆盖同名编号，回滚点只能靠 `switch-history` 里的旧目录 |
| 快速回归副本 | `~/dev-dri/`（驱动：`mwv207_dri.so`、`libmwv207_drv_video.so`、`mwv207_drv_video.so`，后两个必须 `rm -f` 再 `cp`——原名是指向 `/usr/lib` 的符号链接）与 `~/dev-bin/`（探针：`bufbench`/`texbench`/`rbbench`/`rbscale`/`pbench`/`drawbench`/`peep`）。**不要放 `/tmp`**：重启即空，2026-11-13 已被清空三次。流程：改 `src/mwv207-umd` → `./tools/build-umd.sh` → `cp build-full/.../libmwv207_dri.so ~/dev-dri/mwv207_dri.so` → `LIBGL_DRIVERS_PATH=$HOME/dev-dri MESA_LOADER_DRIVER_OVERRIDE=mwv207 …`；视频侧用 `LIBVA_DRIVERS_PATH=$HOME/dev-dri` |
| 出包安装 | `./tools/make-payload.sh -v 24.1.2-fullstack-rN` → 用户在自己终端 `sudo ./switch-mwv207.sh umd install`（脚本自动 `systemd-run --scope` 脱离 lightdm，安装过程会重启 lightdm；agent 直接调 sudo 时工具调用常显示 interrupted，**实装可能已成功——必须用 `status` + md5 核对，勿盲目重跑**） |
| 构建陷阱 | 性能实验必须用 `build-full`（`build-glvnd` 是旧二进制）；`meson setup --wipe` 会把 glvnd 从 auto 变 enabled → 必须显式 `-Dglvnd=false -Dgles1/2=enabled` |

## 2.9 r37（2026-11-17 12:46）：双源混合修好，火狐 LCD 文字恢复

- 症状：火狐部分页面文字被彩色竖条覆盖；`soft`（llvmpipe）不复现、`wr0` 仍复现 ⇒ 我们执行火狐 GL 调用的路径的问题。
- 根因：硬件混合单元没有 `SRC1` 因子，驱动把双源混合降级进片元着色器（`nir/mwv207_lower_dual.c`），原实现四处错：混合状态未进着色器缓存键、多写一个输出、`texop_txb` 带过滤取 dst、因子按未定义通道取值。
- 修法：混合参数进 `fp_dual_blend` 指纹；只写一条最终颜色（并摘 src use_link）；`texop_txf` + `floor(gl_FragCoord)`；因子统一 vec4；发射器新增 `nir_op_fsub`（复用 fadd + 源取负，否则 `nir_opt_algebraic_late` 折叠后 assert）；RT 拷贝按 scissor 裁剪并按尺寸缓存。
- 实测：`dualblend` 7/7 PASS；`gl-regression` 8/8；`vp9-regression` 20/20；`verify-installed` PASS=21；用户实机确认文字恢复正常；`dualbench` 每 dual draw 0.079（basline 0.053）ms。
- 提交：`a219e03`、`5c50b46`、`72c056a`、`4d1f9b9`。

## 2.10 火狐"打开网页就崩溃 / 文字整页乱码"（2026-11-17 15:00）：着色器 arena 太小

- 症状：Firefox 159 打开 `https://cnb.cool/...` 3.6 s 内崩溃（排障模式正常）；不崩时整页文字被裁边/串字，几何与图标正常。
- 崩溃取证：崩溃报告里的 `StackTraces` JSON → 按模块基址算偏移 → `addr2line` 到 r37 的 `libmwv207_dri.so`：
  `SIGABRT` 落在 `mwv207_record_code`（`nir/mwv207_compiler.c:405` 的 `assert(state->bo_offset < bo->size)`），调用链 `mwv207_set_current_shader ← mwv207_update_shader ← _mesa_DrawElements…`；另有 `SIGSEGV` 落在 `src/mesa/main/get.c:find_value`（堆已坏的次生现象）。
- 根因：着色器变体的 GPU 二进制放在 context 的 arena BO 里，容量 `code_size*4*16` 而每变体占 `code_size*4*2` ⇒ **只放得下 8 个变体**。双源混合把混合状态烘焙进着色器后变体暴涨，第 9 个变体 `memcpy` 越界（先污染别的显存 → 文字乱码）随后 `assert` abort。
- 修法：arena 起步 2 MB；越界改一次性告警不再 abort；顺带把 lower_dual 用到的"渲染目标读回槽号"并进 `fp_dual_blend` 指纹。
- 复现：`tests/gl-probe/dualarena.c`（每 draw 换 `glBlendColor`）在修复前驱动上 **abort 于同一条断言**（rc=134），修复后 PASS；`tools/gl-regression.sh` 增为 9 项，dev 上 **pass=9 fail=0**。
- 实机：开发驱动打开原崩溃页渲染完全正常、无崩溃、无 arena 告警。提交 `90ebc12`。

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

- **present 快路径（2026-11-12，`4bb9755`/`1cc0ba4`/`6fa3ad4`）**：X 的 glamor 21.1
  拷贝只有着色器路径，在本机（线性目标 6.7 GB/s）每帧吃 0.65 ms。补丁给
  `glamor_copy_gl()` 加了 `glBlitFramebuffer` 快路径（逐 box，`GLAMOR_NO_BLIT=1` 可关），
  并配 `tools/build-xglamor.sh`（下载/补丁/构建/备份/安装/回滚，模块 ABI 与发行版一致）。
  实测：present 800×600 0.485–0.567 ms（关掉 blit 1.040 ms，**1.9–2.1×**）；`glmark2 build`
  1031 → 1504 FPS，全套 723 → 933（厂商 5209）。剩余差距的定量结论：一次 800×600 blit
  的下限 0.210 ms，厂商 0.167 ms/帧说明它几乎不整帧拷贝 ⇒ 继续提升只能走
  "扫描出平铺+TS"或"overlay 零拷贝"，详见 `reports/PRESENT-PATH-20261111.md` §12。
  回滚：`sudo /usr/local/sbin/xglamor-rollback` 或 `sudo ./tools/build-xglamor.sh --rollback`。

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

## 5. 剩余工作（2026-11-13 16:00 复核）

### 当前清单（只有一条技术疑点 + 若干外部依赖）

1. ~~VP9 1080p 解码吞吐的口径差异~~ **已定性（2026-11-13 16:20）**：加临时
   `MWV207_DECTIME` 分段计时测得驱动侧每帧 `setregs` **0.03–0.04 ms**、`submit`（含等
   DEC 完成）**1.88–1.93 ms**（1080p）/ 0.89–0.93 ms（360p），端到端 6.0 ms 里的其余
   约 4 ms 是 ffmpeg 自己的 VP9 头/tile 解析（user 时间 3.2 ms/帧 @1080p、
   1.2 ms @360p）。即 217↔165 fps 是客户端解析占比/当次 CPU 状态差异，驱动无变化；
   硬件上限约 **530 fps @1080p、1080 fps @360p**。详见
   `reports/VENDOR-BASELINE-20261113.md` 末尾。
2. **JPEG baseline 解码**：需要厂商闭源 KMD/UMD 的启动条件（`DEC_E` 之外），不做；
   实现与开关在 `~/jpeg-parked/`，证据在 `reports/JPEG-VENDOR-TRACE-20261113.md`。
3. **多视口**：`PIPE_CAP_MAX_VIEWPORTS=1`、视口只有一份（`mwv207_draw.c:185`），
   需要硬件文档确认 0x1F76 是否支持多份，否则保持现状。
4. **外部依赖项**（需用户/硬件资料）：Vulkan（无驱动、无 ICD；KMD 的 syncobj 字段注释
   "reserved for vulkan"）、编码（本 SKU `has_enc=0`）、**2D 引擎**（2026-11-16 收口：
   命令语义已由厂商 DDX 反汇编 100% 解出、手工链能被引擎完整执行到最后一个同步写，
   但 draw op 的数据通路不写显存，与 JPEG 同型缺闭源门控；`docs/UNRESOLVED.md` 的
   "2D 引擎"两节记录了全部证据与 `tools/j2d/README.md` 里的编码表）。
   **2026-11-16 傍晚补记**：在收口后又找到了"引擎为何永不执行"的真正条件——
   waitlink 的 `link` 目标地址**必须 4 KB 对齐**（同一条链放 `0x41000` 会执行、
   放 `0x40030` 静默不动）。KMD 已改为按 4 KB 槽分配并提交（`PIPE_2D_SLOT_SIZE`、
   `pipe_2d_slot_align()`，提交 `e0f7eb9`，模块 `BABA169D…`），修复后 `adstat` 能看到
   `2DBUSY`、整条链（含 fill opcode）执行到最后的同步写；但目标像素仍是 0（三种寄存器
   布局、颜色 alpha=0xff、以及使能 2D 私有 MMU 后都一样），所以 **present 仍不能走 2D**，
   收口结论不变。
5. **可选的交付性工作**：README/报告索引刷新、把 `reports/` 里 11-11~11-13 的 8 份报告
   串成一条演进线（present→VP9→上传→回读→vendor 对照→JPEG）——不影响功能，属于工程收尾。

### 以下是 2026-11-10 的历史复核内容（保留作索引，结论已并入上一节）

### 已完成（不再需要做）
- **VP9 Profile0 8bit 硬解**：单 tile / 多 tile 列 / tile rows / 分段 / show_existing /
  概率自适应 / 时间 MV / ALTREF 全部逐位一致；`tools/vp9-regression.sh` 18/18；
  Big Buck Bunny 720p/1080p 整条逐字节一致。提交链 `5c6d05e`→`f8f693a`，
  细节见 `docs/HANDOVER-VP9.md`。
- **Firefox 渲染两处根因**：字形/图标撕裂（`f8f693a`，上传暂存 BO 延迟释放）、
  标签栏"微缩 chrome"（`6ec1e17`，buffer 写入前等待 GPU）；mesa 侧
  `loader_dri3_query_buffer_age()` 恒 0（`ref/oh-mesa` `248659c`，仅本地提交）。
- **KMD 自建 + DKMS**：`mwv207-oss/1.0.0` 已注册，运行中 srcversion 与构建一致；
  切换脚本三态（builtin/oss/vendor）已实现，失败回滚 builtin。
- **N14（KMD，`22145e9`）**：3D/2D/DEC/ENC 的 `reset`/`destroy` 不再丢弃在飞 fence ——
  统一 `dma_fence_set_error(-EIO)` + signal；DEC/ENC 用 `xchg(&current_fence, NULL)`
  与轮询线程抢槽，杜绝双重 put。已在 2026-11-10 重启后生效（运行中即
  `039F972EFE3279A155EE225`），重启后 glxinfo / query 4·4 / VP9 18·18 / glxgears
  117 FPS 全过；reset 通路已用 `tests/n14-reset/` 探针实测：6 轮 TDR、
  `pipe_3d_force_complete` 命中 6/6、用户 fence 12.3 s 完成、复位后 GPU 正常。
  2D/DEC/ENC 与 destroy 路径仍未被探针覆盖，见 `docs/UNRESOLVED.md` §2.1。
- **KMD 加固批次（`53b3229` + `c01af40`，已重启验证生效）**：20 处 `BUG_ON` 收敛——用户
  提交路径上的（`mem_type` 非 VRAM/TT、`reloc->idx` 越界、DMA `cmd_size` 不符、3D 事件
  槽耗尽）返回 `-EINVAL`/`-EIO`/`-EBUSY`，寄存器访问器与 3D ring 描述符改
  `WARN_ON_ONCE` + 跳过动作（受影响的 job 由 2 s TDR 回收，N14 已保证在飞 fence 会被
  signal）；写配套测试时又抓到 **N16**——畸形 submit 会让 `mwv207_job_fini()` 对未分配的
  `cmd_dats` 空指针解引用（实测内核 oops `[#11]/[#12]`），已修。新增
  `tests/bad-submit/`（9 个用例）。2026-11-10 16:21 重启后运行中即为
  `5A5B401802B79D560151210`，`tests/verify-installed.sh … --bad-submit` 全过
  `PASS=25 FAIL=0 SKIP=1`；畸形提交 9/9 干净返回 errno、dmesg 零 oops。
- **N17（`1fc9b21`，已重启验证生效）**：`5A5B4018…` 上普通桌面负载即触发
  `refcount_t: addition on 0; use-after-free`（`mwv207_pipe_dma_submit+0x77c`）——
  N4 的异步 DMA 把给调度器的 `dma_fence_get()` 放在 `queue_work()` 之后，worker 抢跑时
  fence 已被回收。轻则引用计数污染/泄漏，重则 parent fence 被回收、job 永不完成，用户态
  停在不可中断等待（与 16:30 整屏卡死、Xorg 只能 SIGKILL 的症状吻合）。两内核与两
  initramfs 已装 `8DFC520FB8FCDECFBC911AA`；17:55 重启后按 §2.6 复核通过（PASS=25、kprobe 压测 submit/complete 2052·2052 平衡、`refcount_t` 告警 0、X 最大延迟 5 ms）。
- **KMD 结构/边界批次（`6502f19`，已重启验证生效）**：N1 sched 槽位撞车（6 槽放 7 个
  引擎，enc0 覆盖 dec1）→ `MWV207_MAX_SCHED_NR 7` + 显式下标；N2 3D `hw_submission`
  30→28（等于事件槽数会命中 `BUG_ON(event >= 30)`）；K12 超时路径 devfreq 多减 →
  新增 `mwv207_devfreq_reset_busy()` 清零；K9-② ISR 影子寄存器改
  `READ_ONCE`/`WRITE_ONCE`；N9 显示侧两处 `BUG_ON(irq == 0)` 改可回退错误。
  两内核已装、initramfs 已刷（srcversion `D5EEF36…`）。2026-11-10 13:29 重启后
  运行中即 `D5EEF36617A4D3FBF4AFD1D`，glxinfo / query 4·4 / VP9 18·18 /
  H.264 8bit 逐位一致 / `tests/n14-reset/` 6·6 / dmesg 无 WARN·BUG 全过；
  引擎计数用新增的 `tests/engines/run-engines.sh` 核对（本机 lite：
  3d=1 dec=1 enc=0 2d=1 dma=1，见 `docs/UNRESOLVED.md` §2.2）。
- **KMD 批次 3（`fbed27f`，2026-11-10 14:36 重启后验证生效）**：N4 —— `pipe_dma_commit()` 原来在 drm_sched 的
  `run_job` 线程里 `cpu_relax()` 自旋最多 5 s，再返回 `dma_fence_get_stub()`（提交即已
  signal）。改成 submit 只启动引擎 + 返回真实 fence，完成/清通道/失效缓存/signal 交给本管线
  自己的 `WQ_MEM_RECLAIM` 有序队列睡眠式轮询（上限 1500 ms < 调度器 2000 ms 超时），
  reset/destroy 按 N14 范式 `cancel_work_sync()` + xchg 以 `-EIO` 收尾；K11 —— `irq_fini()`
  在 `free_irq()` 前把 `0x009A8020/0x24` 使能位清零、按 W1C 清挂起状态（关掉 nobody-cared
  窗口），`pci_remove()` 在 `pci_clear_master()` 后补 `pci_disable_device()`。K9-① 经与前
  一版厂商源码对照确认是 W1C 标准清位、**不是缺陷**；N10 决定只文档化（见
  `docs/UNRESOLVED.md` §2.3）。
- **UMD 视频批次（`385ed81`/`a5a938b`/`4ecca0d`，载荷 r20 已安装）**：I-1 HEVC tile 缓冲按真实帧高
  分配（原先拿 `bit_depth_chroma_minus8` 当高度，8bit 时高度为 0）；I-2 H.264 路径删掉
  `set_reg(BITPLANE1_E, 0)`（与原厂 1840 行表核对：BITPLANE0/1/2 与
  CABAC/BLACKWHITE/DIR_8X8_INFER 在 word7 上逐位同名同址，是按 codec 复用的物理位，
  该句把单色标志清掉）；I-3 收回 H.264 High422/High444 声明 —— 寄存器/原厂驱动/libva
  三重证据表明本机只有 4:0:0 与 4:2:0（细节 `docs/UNRESOLVED.md` §1.5）。
  验证：HEVC 8bit/Main10 2 tile 列逐位一致、H.264 High 420 逐位一致、VP9 18/18、
  query 4/4。**I-4：H.264 High10 已定案并收回声明** —— 实测 10bit 样片会让
  DEC 管线连报 6 次 `job from 'av:h264:df0' to 'mwv207_dec0', timed out!`
  （TDR 回收、输出全 0），缺 Hantro 的 high10p 位流预转换；原厂闭源驱动也不
  声明 High10。收回后 ffmpeg 对该类流直接回退（`hwaccel initialisation
  returned error`），DEC 零超时；`end_frame` 另加 `bit_depth_luma_minus8 > 0`
  跳过分支。见 `docs/UNRESOLVED.md` §1.2。
- **I-6：被跳过的帧不再静默返回脏图（2026-11-10，`bb5cd9e`）**：VP9/H.264 里所有
  "这一帧没法解"的提前返回（参考缺失、尺寸不符、参数不支持、分配失败、分区尺寸异常）
  以前只清 `JMDEC_DEC_E` 就返回，PPU 输出面根本没编程，而 `vaSyncSurface()` 照样
  成功 —— 应用拿到的是目标 surface 的旧内容（B2 的 resize_inter 向量就是这样）。
  现在 codec 层置 `frame_skipped`，`end_frame()` 不提交、改置 `frame_failed` 并给
  `*picture->fence` 一个 fd=-1 的桩 fence，新增的 `get_decoder_fence()` 在失败时
  返回 0，Mesa 的 va 前端据此让 `vaSyncSurface()` 回 `VA_STATUS_ERROR_TIMEDOUT`
  （ffmpeg 侧 `Failed to sync surface`）。回归：VP9 P2 10bit / show_existing /
  res14 / HEVC 8 / H.264 8 全部逐位一致。
- **I-5：VP9 intra-only 之后的 inter 帧解码错（2026-11-11，`8f6e964`）**：官方
  `vp90-2-16-intra-only.webm` 上 4 个真实解码帧与 libvpx 差 15–22 阶（64×64 块级
  损坏）。三处根因：① `vp9_setup_past_independence()` 只对 key/error-resilient 生效，
  漏了 intra-only（libvpx 对 `frame_is_intra_only` 同样重置概率表、lf delta、段特征
  并把 `frame_context_idx` 归零）；② 自适应基准应取"刚重置成默认表的
  `frame_contexts[fci]`"，我们固定用帧头更新前的保存表；③ `show_existing_frame` 的
  1 字节包被解析器早返回后，调用方又把它当压缩头"解"了一遍，函数尾的
  `saved_ctx[fci] = sh->entropy` 把垃圾写进保存的概率表（重放帧也不清系数计数）。
  修后官方向量 **7 个真实解码帧全部与 libvpx 逐字节一致**，本机 20 帧 `-g 5`、
  `-frame-parallel 0` 样本 20/20 一致；只剩 3 个 `show_existing` 重放帧仍是黑图
  （独立限制：整条解码+PPU 由 `JMDEC_DEC_E` 启动，重放不启动 PPU）。
  定位工具：`tests/vaapi-client/vadec.c`（直连 libva）+ `tests/vp9-intraonly/`。
  2026-11-11 12:07 随 **r23** 装进安装栈并复核：完整 ffmpeg 路径
  （`-hwaccel vaapi -hwaccel_output_format vaapi … -vf hwdownload,format=nv12`）解官方
  intra-only 向量 7 帧全部与 `-c:v libvpx-vp9` **逐字节一致（0 差异帧）**，
  `tests/verify-installed.sh` PASS=25、`tools/vp9-regression.sh` 18/18。
- **B2：VP9 跨尺寸参考已完成（2026-11-11，`351e6ed` → `21a15db`）**：
  第一版只删掉"参考尺寸必须等于当前帧"的跳过判据，实测仍跳过 9 帧，日志
  `reference 0 (slot 0) has no decoded picture` —— 真相是客户端在分辨率变化时重建
  VA context，Mesa 随之重建解码器实例，我们私有的参考槽副本（ASIC 内部布局，
  无法从客户端 surface 反推）跟着归零。`21a15db` 新增进程级 `vp9_ref_carry`：
  `vp9_destroy_state()` 把 `ref_store` 与 16 个槽的几何/长度移交出去，新实例
  `vp9_ref_carry_adopt()` 接手。实测 `vp90-2-21-resize_inter_320x180_5_1-2.webm`
  由 `SKIPPED-AS-ERROR`（hw=864000）变为 **IDENTICAL 2592000 B、跳过帧 0**；
  `vp90-2-14`/`vp90-2-16-intra-only`/`vp90-2-10-show-existing-frame`/`fp0g5` 全一致，
  `vp9-regression` 18/18、query 4/4，`dmesg` 无 TDR —— 跨尺寸参考并不会让 ASIC 自旋。
- **B2 第一版（2026-11-11，`351e6ed`，已被 `21a15db` 取代）**：厂商栈 `jmgpu` 能解
  官方 `vp90-2-21-resize_inter_320x180_5_1-2.webm`（逐字节正确），我方此前因"参考尺寸
  必须等于当前帧"整帧跳过。改动：删掉该跳过分支（硬件本来就按每个参考槽自己的
  `ref_w_reg`/`ref_h_reg`/`ref_y_stride_reg` 与 14 位定点 `HOR/VER_SCALE` 编程），保留
  "槽从未解码过就跳过"；staging 缓冲（`buf->res`，本地暂存）在帧变大时释放重建，避免
  越界写。**尚未实测**（切换在 vendor 栈）：需要切回自研栈后跑
  `tests/vp9-resize/run-vp9-resize.sh`，期望 `跳过帧=0` 且 IDENTICAL。
  若仍跳过，`docs/UNRESOLVED.md` §1.8 已列出三条待查：客户端重建 VA context 导致槽空
  （厂商直接用 `pic_param->ref[]` 客户端 surface，没有第二份副本）、PPU 恒 1:1 的输出
  尺寸、命令流缓冲按最大帧分配（曾见 `submit failed: -34 ERANGE`）。
- **一键验收脚本（2026-11-10，`tests/verify-installed.sh`）**：把 KMD 版本/来源、
  glxinfo/query/引擎计数、VAAPI profile 归位、H.264-High/HEVC 8/10bit 逐位一致、
  High10 回退与 DEC 零超时、TDR 探针、dmesg 新增告警（带基线，只查本次运行新增行）
  串成一条命令；`kernel.dmesg_restrict=1` 时明确 SKIP 而非假 PASS。已在 r19 + `D5EE…`
  上预跑通过（root 全跑 `PASS=21 FAIL=0 SKIP=1`）。

- **`present` 性能根因（2026-11-11，仅定位未修）**：glamor 每帧用 3D 着色器把窗口
  pixmap 合成到屏幕 pixmap，而两者都被 `mwv207_can_tile()` 的
  `enable_scanout_tile = false` 强制成线性；本驱动对线性表面的采样只有
  6.7 GB/s、且**写入线性目标同样只有 10.4→3.3 GB/s**，于是 800×600 的 present 要
  ≈0.55 ms（纯 swap 0.65 ms）。
  **2026-11-11 追加取证推翻了"厂商用平铺扫描出"**：反汇编厂商 `jmgpu.ko` 的
  plane atomic update（`j9_handle__attribute_chalkstone`）只写与我们完全相同的
  5 个寄存器（`0x434/0x43C/0x4F8/0x438/0x430`），无任何平铺位；其 DDX 也不 import
  `drm_jmgpu_bo_set_tiling`。厂商快在**用 2D 引擎（EXA）做合成拷贝**，而 glamor
  21.1 的 `glamor_copy.c` 根本没有 blit 路径（只有着色器）。
  实测同一对 FBO：`glBlitFramebuffer` 线性目标 **0.222 ms（17.3 GB/s）**、
  着色器 0.576 ms ⇒ **blit 快 2.6×**。
  再补一层（把平铺与 tile-status 分开）：平铺+TS 的 blit 0.100 ms / 着色器 0.180，
  **平铺无 TS 0.205 / 0.300**，线性 0.222 / 0.576 ⇒ blit 引擎写线性并不慢，
  慢的是 PE 写线性；TS 压缩另给 2×，但 TS 只分配给平铺资源
  （`mwv207_resource.c:212`），线性资源双重劣势。PE 写线性慢属硬件行为，无便宜修法。
  因此改法按性价比重排：**A. 给 glamor 加 blit 快路径（X server 补丁，不动驱动，
  ≈0.25 ms/≈1800 FPS）→ D. 窗口 present 走 overlay plane（KMD 新增 plane，厂商有
  `jmgpu_crtc_set_overlay`，上限最高）→ B. 扫描出平铺（证据基础被削弱、风险最高）**；
  C（查 UMD 侧 3D 写线性）已排除。
  数据见 `reports/PRESENT-PATH-20261111.md` 与 `docs/UNRESOLVED.md` 同名小节。

- **2D 引擎（2026-11-12，未打通）**：KMD 侧的 2D 管线本身完好（描述符 dump 逐字正确、
  `0x4200` 特殊全局寄存器可写、`0x4800` 复位状态正常），但**启动位没找到**：
  `0x4C00` 与 `0x4200` 在地址解码上别名，8 个候选偏移写 1 后完成序号 `0x6000` 全为 0、
  2D 中断一次未到。启动位由厂商未发布的 `j9_handle_j9m_incumbency()` 表计算，
  `jmgpu.ko` 里也无相应立即数 ⇒ 需要更大范围的偏移扫描或更深的表定位，暂不推进。
  证据与限制见 `docs/UNRESOLVED.md` 的 present 小节末两条。诊断参数 `mwv207.diag`
  （默认 0）已随代码保留，供以后复查。

- **UMD 每 draw CPU 开销（2026-11-12）**：`mwv207_update_shader()` 每次绘制都重算
  三组着色器指纹（≈200 次哈希），实测占 config 阶段 70%。改成 `fp_dirty` 脏标记后
  端到端提交 **0.86 → 0.17 µs/draw**；`/tmp/swapsh` 交替纹理/FBO 格式的读回哈希
  新旧驱动一致，`tests/query` 4/4。`glmark2` 不受影响（GPU-bound）。
- **窗口渲染改平铺 + 线性影子（`f486a1b`，载荷 r26）**：`can_tile` 放开 SCANOUT 平铺，
  `flush_resource` 按 `sync_needed` 把平铺内容拷到 `res->linear` 影子，X/显示仍读线性。
  实测 `jellyfish` 2.98×、`buffer` 1.61×、`pulsar` 1.35×、`conditionals` 1.22×，
  `build`/`texture`/`bump`/`terrain` 慢 2–5%，全套 1109–1236 → 1252–1254；
  代价是窗口缓冲约 2× 显存。详见 `reports/PRESENT-PATH-20261111.md` §19。
### 待办（按价值排序，2026-11-13 复核）

1. **编码（D3，H.264/HEVC encoder）**：唯一剩下的"大功能"。UMD 侧 `base.encode_bitstream`
   从未安装，需从零接入 `mwv207_pipe_enc.c`（KMD 已有）。工程量最大，且需要重新做
   码率/质量/参考帧管理的验证。
2. **Vulkan（D7）**：不可达 —— 官方 1.8.0 也没有（无 libvulkan / icd.d；KMD 的
   syncobj/vm_bind/gpuva/sparse 全 0）。
3. **多视口（D6）**：`PIPE_CAP_MAX_VIEWPORTS=1`；视口状态寄存器只有一份（`mwv207_draw.c:193-198`
   的 `0x280–0x285`），硬件是否支持多份无文档可查 ⇒ 维持现状。
4. **12bit VP9**：代码只放行 10bit；12bit 既无样片也无硬解证据，维持"未验证"。
5. **`show_existing` 重放帧仍是黑图**：PPU 不经 `JMDEC_DEC_E` 无法启动，属独立课题
   （不影响 B3 判据，`tests/vp9-intraonly/` 已按 SKIPPED-AS-ERROR 处理）。
6. **vendor 态端到端验证**：`stack vendor` 会重启，需人工实跑；热切换已证不可行
   （`mwv207` refcnt 恒 2）。
7. **Firefox WebGL**：剩余帧时间在 WebRender 调度，不在 GL 侧；已不作为驱动缺陷。
8. **DKMS 一致性**：改 KMD 源码后必须 `tools/dkms-sync.sh`；`kmd stage` 覆盖 .ko 后
   `dkms status` 会报 `Diff between built and installed module!`，用
   `dkms install --force` + `depmod` + `update-initramfs` 恢复。

### 不建议投入（沿用 2026-11-03 结论）
C-2 16 位采样编码（硬件边界）、D7 Vulkan、C-5/C-6、N10、openuuyc 单实例 UX。
原先"chrome 文字错位（用户已搁置）"一条已随 `f8f693a`/`6ec1e17` 修复，可删除。

## 6. C 类三项（2026-11-03 已拍板：全部保留声明 + 注明）

| # | 问题 | 选项 |
|---|---|---|
| C1/C3 | R16G16_UNORM/R16_SNORM/R16G16_SNORM 采样恒 0。**2026-11-03 已定位为硬件边界**：把正常纹理强制换码 0x36 也读 0，而码 7/24–26 正常，tiling/align/NORM/new-mode/bit23/31 全无效 → 0x1F76 不解码归一化 16 位码 0x32–0x36。10bit **解码面**已随 `f0c7605` 启用（P010/P016 + DRM_PRIME_2 导出逐位验证） | A 保留声明+文档注明 / B 回退只留 R16_UNORM / C 继续逆向（已证伪：非编码问题，2026-11-03） |
| C2 | 高级混合 8/15（7 种方程静默不混合） | 保留声明 = 原生 GLES 3.2 的必要代价 / 撤销则 GLES 3.2 退回 3.1 |
| ~~I-3~~ | ~~H264 High422/444 声明 vs `chroma_format_idc=1` 硬置~~ | **已定案（2026-11-10，`a5a938b`）**：硬件只有 4:0:0/4:2:0（寄存器表无采样格式选择器、原厂驱动只声明 ConstrainedBaseline/Main/High、libva 无对应 VAProfile）→ **不可实现，已收回声明**；细节见 `docs/UNRESOLVED.md` §1.5。另新增 **I-4**：H.264 High10 输出全 0（硬件 `h264_high10_support=1`，属本驱动缺陷） |

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
- KMD 热切换**可行**（2026-11-06 实测）：`sudo ./switch-mwv207.sh kmd install`
  会停 lightdm → `rmmod`（refcnt 到 0 后能卸）→ 装入自建 `.ko` → 重新拉起 lightdm，
  桌面约几秒后恢复，桌面会话会重新登录一次。脚本另有 `kmd stage`（只放置，重启生效）
  与 `refresh_initramfs`。
- **自建 KMD 用 DKMS 注册成 `mwv207-oss` 包，随内核自动重建**（2026-11-06）：
  * 源目录 `/usr/src/mwv207-oss-1.0.0`（由 `tools/dkms-sync.sh` 从仓库
    `src/kernel-6.6` 同步，剔除 `*.o/*.ko/*.mod.c` 等编译产物）；`dkms.conf` 里
    `BUILT_MODULE_NAME=mwv207`、`DEST_MODULE_LOCATION=/updates/dkms`、`AUTOINSTALL=YES`，
    `MAKE` 用 `${kernelver}` + `${build_dir}`。
  * 产物 `/lib/modules/<kver>/updates/dkms/mwv207.ko`；因为模块名仍是 `mwv207`，
    depmod 会把它排在 `kernel/drivers/gpu/drm/mwv207/` 的内建版之前，重启即生效。
  * 内核升级后 `dkms autoinstall` 自动重编这个包；仓库源码有改动时跑一次
    `sudo ./tools/dkms-sync.sh [版本号]` 同步并重建。
  * 坑：`make -C /lib/modules/$(uname -r)/build M=`（M 为空）会触发 syncconfig，
    而 Deepin 的 headers 包没有 `scripts/kconfig`，失败时会删掉
    `include/generated/autoconf.h`，之后所有模块编译都报找不到该文件。可从 ostree
    部署快照还原（`rsync -a /ostree/deploy/deepin/deploy/<当前部署>/usr/src/linux-headers-$(uname -r)/ /usr/src/linux-headers-$(uname -r)/`）；
    `tools/dkms-sync.sh` 已在开头检查该文件并给出提示。
  * 厂商那条 DKMS 包（`mwv207/1.8.0.uos`）构建出来的是 `jmgpu.ko`，不会覆盖我们的
    模块；`jmgpu` 仍由内核命令行 `modprobe.blacklist=jmgpu` 拦住。
- 上游枚举陷阱：`p_format.h` 只在 `ref/mesa-22.3.7/...`（`find` 要 `-L`）。
- **三态"整套驱动"切换**（2026-11-06 新增 `stack` 子命令）：把内核态(KMD)与用户态(UMD)
  当成一个整体，只有三种合法状态：

  | 状态 | 内核态 | 用户态 |
  |---|---|---|
  | `builtin` | 内核内建 `mwv207.ko` | 开源载荷（Mesa 24.1.2 oss） |
  | `oss` | 自建修复 `mwv207.ko`（DKMS） | 开源载荷（VA-API 带全部 VP9 修复） |
  | `vendor` | 厂商 `jmgpu.ko` | 厂商 UMD/EGL/OpenCL |

  `sudo ./switch-mwv207.sh stack status|builtin|oss|vendor|menu`；任一步失败（含桌面未点亮、
  自检不过）自动回滚到 `builtin`（内核内建驱动）并点亮桌面；切 `vendor` 会拉看门狗，超时自动回滚到 `builtin`。
  状态检测靠 `srcversion` 三方比对（已加载 / 内建 .ko / 自建 .ko），自建版 srcversion
  另存 `/var/lib/mwv207-oss/oss-srcversion`，因此 updates 里的 .ko 被删掉时仍能认出来。

- **停 lightdm 必须放到流程最后、且与调用终端脱钩**（2026-11-06 踩坑）：本机图形会话本身
  就跑在 lightdm 下的 X 里，脚本一停 lightdm，调用它的终端/对话窗口立即被杀，切换只做
  到一半。`stack` 因此拆成两段：
  1. **阶段 1（前台）**：只做文件级动作（UMD 载荷、KMD 模块放置、depmod、initramfs），
     全程不碰桌面；失败直接退出，桌面毫发无损。
  2. **阶段 2（detached）**：`setsid nohup` 起后台进程，`sleep ${STACK_DELAY:-3}` 后
     停 lightdm → rmmod/modprobe → 起 lightdm → 自检；日志 `/var/log/mwv207-stack.log`，
     结果 `/run/mwv207-stack-result`（`ok|failed target=...`）。
  可选 `--delay <秒>` 或 `STACK_DELAY=N` 让阶段 2 更晚启动（把最后的确认/说明先输出完）。

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
