# JM9100 (景美 MWV207) 开源驱动适配仓库

Deepin 25 aarch64 / 内核 6.6.155-arm64-desktop-hwe / JingjiaMicro JM9100
[0731:9100] @ PCI 0000:07:00.0 —— 开源驱动 (KMD+Mesa UMD) 逐行分析、缺陷修复、
全功能栈构建与系统切换的完整工作区。

当前系统状态: **oss-24.1.2-fullstack-r13**（UMD + KMD 均为自建带修复版，
零 env 下 GL 4.0 / GLES 3.2 / `Video memory: 2048MB`，`umd verify` 全过；
r13 新增 10bit HEVC 解码面：P010/P016 硬解 + DRM_PRIME_2 导出逐位正确）。

里程碑速览：全功能栈（GL/VAAPI/VDPAU/rusticl OpenCL）→ GLES 3.2 原生点亮 →
自建 KMD 真机运行（srcversion `D507FC2774D547E7BEBA182`）→ Firefox 硬件渲染
修复（WebRender 图元全丢根因 = VS 里 RGBA32F 取样只回 64 位）→ 性能优化
（WebGL 36→42+ FPS，CSS 页到 vsync 上限 97–99 FPS；均约 llvmpipe 的 2 倍）→
GBM/Wayland 平台打通 → 健壮性批次修复 → dual-source blend 崩溃修复。

## 快速使用

```bash
sudo ./switch-mwv207.sh status              # 一次看全: 用户态栈 + KMD 身份
sudo ./switch-mwv207.sh umd install         # 切换到开源用户态栈 (快照+看门狗+自检)
sudo ./switch-mwv207.sh umd verify          # 安装自检 (零 env, 非侵入)
sudo ./switch-mwv207.sh umd rollback        # 一键回滚系统软件栈
sudo ./switch-mwv207.sh kmd status          # KMD: 内建/自建/实际加载 三方对照
sudo ./switch-mwv207.sh kmd install         # KMD 切换为自建带修复版 (需重启)
```

安装后系统级入口: `/usr/local/sbin/mwv207-oss-switch`（自安装副本，载荷走
`/var/lib/mwv207-oss/payload-cache`，自检工具走 `/var/lib/mwv207-oss/tests`）。

## 目录结构

| 目录 | 功能 |
|------|------|
| `switch-mwv207.sh` | 统一切换脚本 (根目录入口, 1200+ 行): `umd` 用户态层 + `kmd` 内核态层; 不带层名的 `status`/`check`/`verify` 同时跑两层。全局选项 `--root`/`--dry-run` 可写在任意位置。EGL ICD 采用**原地改写**（不删厂商 `10_mwv207.json`，保住 `/etc/cdi/jmgpu.yaml` 的 CDI 挂载源 → 玲珑应用可用） |
| `src/mwv207-umd/` | 我们维护的用户态驱动源码（`drivers/mwv207` + `winsys/mwv207`，唯一可改处；上游树 `ref/oh-mesa/...` 里是指向它的符号链接） |
| `src/kernel-6.6/` | 我们维护的内核态驱动（2026-11-03 并入本仓库管理；原独立 git 仓历史在 `ref/kernel-6.6-old-git`。社区 6.6 移植基线 + K1–K8/K10 修复 + N15 零重定位 submit BUG 修复） |
| `payload-36/`, `payload-37/` | 历史与当前活动载荷（`payload-37` = `24.1.2-fullstack-r13`，11 个实体文件含 glvnd ICD `libEGL_jm.so.0`，MANIFEST 校验）。历史载荷已清理，只在 `/var/lib/mwv207-oss/switch-history` 留最近 4 个回滚快照 |
| `tools/` | `make-payload.sh`（从构建树组装载荷: 版本戳一致性自检/rpath/MANIFEST/与上游 md5 差异）；`build-umd.sh`（check/relink/build/sim）；`map-monitor.c`（X Map 死区缓解守护）；`enable-ssh-rescue.sh`（黑屏 SSH 救援通道） |
| `docs/` | 驱动 API 与寄存器参考（19 文件 / 约 1.83 万行）+ `UNRESOLVED.md` 未修问题总账（40 项，含已修索引与批次推进表） |
| `ref/` | 参考材料（不入库大文件除外）：上游 mesa-22.3.7 / oh-mesa(24.1.2) / kernel-5.10 / linux-source-6.6 / 厂商 1.8.0 / 旧 KMD git 仓 / `reverse-source/`（闭源栈反编译: 符号表、反汇编、寄存器映射 JSON、六篇文档） |
| `tests/` | 硬件验证套件: run-all.sh / run-full-regression.sh + query 回归 + 源码与二进制 |
| `reports/` | 阶段报告 15 份（见下索引）+ 火狐取证证据目录 |

> 历史清理：旧脚本与 archive/（2026-10-21）；旧载荷 payload-24…34、14 个回滚
> 快照、/tmp 下全部实验 profile 与旧 DRI 副本（2026-11-03）。同日删除 `patches/`
> 补丁集并停止生成补丁：改动以 git 提交为唯一记录，复现/对外提交直接引用提交哈希。

## 报告索引 (reports/)

1. `ANALYSIS-REPORT.md` — KMD/UMD 逐行分析: 全部缺陷定位与修复
2. `COMPAT-TEST-REPORT.md` — 应用兼容性矩阵实测
3. `HWDECODE-FEASIBILITY-REPORT.md` — OpenHarmony video-codec 移植 + VA 硬解 + 10bit HEVC 黑屏修复 (§7)
4. `FULLFEATURE-REPORT.md` — rusticl OpenCL / VDPAU / 全 codec 全功能栈
5. `PHASE5-SWITCH-REPORT.md` — 切换脚本重构 + glamor 点亮破案 + 冷启动验证
6. `PERFORMANCE-REPORT.md` — glmark2 性能调查: devfreq 启动失败缺陷修复 + 四因子分离
7. `FIREFOX-BLACKSCREEN-INVESTIGATION.md` — Firefox 首帧黑屏: Xorg 合成器死区定位 + 守护缓解
8. `CURRENT-STATE-PROBE.md` — 运行态快照: 多套 UMD 并存 / 加载路径 / X 与多媒体接线
9. `GLES32-ROBUSTNESS-BLEND-REPORT.md` — GLES 3.2 点亮: 鲁棒缓冲访问 + 高级混合 (8/15) 实测
10. `ES3-FORMATS-LINEAR-REPORT.md` — ARB_ES3_compatibility 的 6 个格式: BLT 破坏数据 → 强制 LINEAR
11. `KMD-6.6-PORT-REPORT.md` — KMD 6.6 移植: 社区基线 + K1/K3–K8/K10/softdep, K2 刻意不移植的依据
12. `FF-RENDER-20261102.md` — 火狐 WebRender 图元全丢根因 + 火狐性能优化 (§13) + GitHub 崩溃取证
13. `REAL-WORLD-TEST-20261102.md` — 真实应用验收: glmark2 1125 / HEVC 硬解 2.1× / OpenCL / 46 进程在用
14. `STACK-SWITCH-AUDIT.md` — 栈与切换脚本审查（12 项发现 → 9 项加固落地）

## Firefox 渲染（已定案）

- **首帧黑屏**：Xorg 合成器侧死区（与 GL 栈无关），缓解守护 `tools/map-monitor.c`。
- **WebRender 图元全丢**（2026-11-02 修复）：根因是顶点着色器对 RGBA32F 纹理的
  `texelFetch` 只回 64 位 —— `nir/mwv207_lower_128bit()` 原来只对 FS 生效。
  修复见 `src/mwv207-umd/`（提交 `bda3749` 为 VS 128 位取样，`d7c52a3` 为重编译
  记录重置）；取证过程与最小复现
  （`tests/vstf-128bit/`）见 `reports/FF-RENDER-20261102.md` §10–§12。
- **性能**（2026-11-02 修复，提交 `d7c52a3`）：① 重编译判据误用 `ctx->dirty`
  （每次提交后被重置为全量）→ 改为"128 位模式指纹参与着色器缓存键"；②
  128 位纹理 split/merge 对 WC 显存逐标量拷贝（~100MB/s）→ 改逐行 memcpy。
  结果: WebGL 36→42+ FPS（llvmpipe 同页 28.5），CSS 页 97–99 FPS（vsync 上限）。
- **GitHub 概率性崩溃**（2026-11-03 修复，提交 `40d5d76`）：dual-source blend
  的 RT 视图绑在 `num_sampler_views` 槽而 `sampler_index` 记 `num_samplers`
  （两计数不等），`mwv207_lower_dual.c:115` 无条件解引用 → SIGSEGV 0x48。
  现已判空 + 槽位统一 + 三级回退。
- **浏览器 UI 文字错位**：r8 起可见，暂缓（用户实际使用未受影响），见
  `docs/UNRESOLVED.md`。

## GPU 频率修复 (系统级, 已部署)

`/etc/modprobe.d/mwv207-devfreq.conf`: `softdep mwv207 pre: governor_performance` —
修复 in-tree KMD devfreq 启动即失败的缺陷 (GPU 停 vbios 低频), 重启后自动生效。
源码正解在 `src/kernel-6.6/mwv207_drv.c:543` 的
`MODULE_SOFTDEP("pre: governor_performance")`。

## 路径约定

仓库内所有脚本使用**相对自身位置**的路径 (不硬编码用户主目录):
`switch-mwv207.sh` 以脚本所在目录为仓库根; `tests/*.sh` 以 `..` 为仓库根。
移动仓库目录或换用户调用均不受影响。
