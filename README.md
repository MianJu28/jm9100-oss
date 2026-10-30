# JM9100 (景美 MWV207) 开源驱动适配仓库

Deepin 25 aarch64 / 内核 6.6.143-arm64-desktop-hwe / JingjiaMicro JM9100
[0731:9100] @ PCI 0000:07:00.0 —— 开源驱动 (KMD+Mesa UMD) 逐行分析、缺陷修复、
全功能栈构建与系统切换的完整工作区。

当前系统状态: **oss-24.1.2-fullstack-r1** (开源全功能栈已安装并冷启动验证)。

## 快速使用

```bash
sudo ./switch-mwv207-oss.sh status     # 查看当前栈状态
sudo ./switch-mwv207-oss.sh verify     # 14 项安装自检 (零 env, 非侵入)
sudo ./switch-mwv207-oss.sh rollback   # 一键回滚系统软件栈
sudo ./switch-mwv207-oss.sh install    # 切换到开源栈 (自动快照+看门狗+自检)
```

安装后系统级入口: `/usr/local/sbin/mwv207-oss-switch` (自安装副本, 载荷走
`/var/lib/mwv207-oss/payload-cache`, 自检工具走 `/var/lib/mwv207-oss/tests`)。

## 目录结构 (按功能分类)

| 目录 | 功能 |
|------|------|
| `switch-mwv207-oss.sh` | 切换脚本 (根目录入口): check/install/rollback/verify/status/watchdog |
| `reports/` | 阶段报告 14 份: 逐行分析 / 兼容性 / VA 硬解移植 / 全功能栈 / 切换脚本 / 性能调查 / 黑屏定案 / 运行态快照 / GLES 3.2 与鲁棒访问 / 16 位格式 / KMD 6.6 移植 |
| `patches/` | 缺陷修复补丁集 0001-0012 (KMD 2 个 + UMD 4 个 + 硬解移植 1 个 + 10bit HEVC 黑屏修复 1 个 + GLES 3.2/鲁棒访问/16位格式 1 个 + 显存单位与 winsys O2/O3 1 个 + KMD 6.6 移植 1 个) |
| `payload-24/` | Mesa 24.1.2 全功能栈载荷源 (GL+VA+VDPAU+OpenCL, MANIFEST 校验) —— **不含** GLES 3.2 等后续修复 |
| `payload-25/` | 由 `tools/make-payload.sh` 从 build-full 组装的 r2 载荷 (含 GLES 3.2 / 鲁棒访问 / 16位格式 / 显存修正), 待部署 |
| `tools/` | `make-payload.sh`: 从构建树组装新载荷 (改 rpath/SONAME 自检/重算 MANIFEST/打印与上游 md5 差异) |
| `docs/` | 驱动 API 与寄存器参考 (17 文件 / 约 1.77 万行): 架构 / winsys / 核心对象 / 绘制 / NIR / 视频 / 已知问题 / 可行性 / 寄存器四表 |
| `tests/` | 硬件验证套件: run-all.sh / run-full-regression.sh + 源码与二进制 |
| `compat-env/` | 兼容运行环境 (libva 2.22 deb + 解包根, 供旧 libva 系统临时定向) |
| `src/` | 源码树: kernel (KMD 5.10 原始+修复) / **kernel-6.6 (社区 6.6 移植基线 + 我们的 K 修复, 独立 git 仓)** / mesa (22.3.7 修复版) / oh-mesa (OpenHarmony 金矿: 24.1.2 基线+6.6 内核 patch) |

> 历史遗留已按用户要求清理 (2026-10-21): 旧脚本 switch-to-oss.sh (690 行, 缺陷
> 记录于 PHASE5-SWITCH-REPORT.md §2) 及 archive/ (22.3 载荷/早期实验/旧测试)。

## 报告索引 (reports/)

1. `ANALYSIS-REPORT.md` — KMD/UMD 逐行分析: 全部缺陷定位与修复
2. `COMPAT-TEST-REPORT.md` — 应用兼容性矩阵实测
3. `HWDECODE-FEASIBILITY-REPORT.md` — OpenHarmony video-codec 移植 + VA 硬解 + 10bit HEVC 黑屏修复 (§7)
4. `FULLFEATURE-REPORT.md` — rusticl OpenCL / VDPAU / 全 codec 全功能栈
5. `PHASE5-SWITCH-REPORT.md` — 切换脚本重构 + glamor 点亮破案 + 冷启动验证
6. `PERFORMANCE-REPORT.md` — glmark2 性能调查: devfreq 启动失败缺陷修复 + 四因子分离
7. `FIREFOX-BLACKSCREEN-INVESTIGATION.md` — Firefox 首帧黑屏: Xorg 合成器死区定位 + 守护缓解
8. `CURRENT-STATE-PROBE.md` — 运行态快照: 三套 UMD 并存 / 加载路径 / X 与多媒体接线 / 能力对比
9. `GLES32-ROBUSTNESS-BLEND-REPORT.md` — GLES 3.2 点亮: 鲁棒缓冲访问 + 高级混合 (8/15) 实测
10. `ES3-FORMATS-LINEAR-REPORT.md` — ARB_ES3_compatibility 的 6 个格式: BLT 破坏数据 → 强制 LINEAR
11. `KMD-6.6-PORT-REPORT.md` — KMD 6.6 移植: 社区基线 + K1/K3–K8/K10/softdep, K2 刻意不移植的依据

## Firefox 首帧黑屏 (已定案 + 缓解)

根因在 **Xorg 合成器侧**: 窗口 map 后 0–4ms 内的第一次 X 层绘制被永久丢弃
(纯 `XCopyArea` 同样复现, 与 GL 栈无关; 生产树 EGL/X11/DRI3 与上游 mesa-24.1.2
`diff -w` = 0 行, 厂商零改动)。Firefox 冷启动首页/恢复页的首帧 present 恰在
死区内 → 永黑; 外部 URL/动画页/mpv 因首呈现较晚而不受影响。

当前处置: 驱动栈回退至 `5e6f2dd` (撤销 `libEGL.so` 无后缀名拦截) → Firefox
走系统 llvmpipe **显示正常**; 其余能力 (GLX 直渲 / VA 硬解 / OpenCL) 不受影响。
通用缓解: `daemon/map-monitor.c` — 监听窗口 Map, 延迟 50ms 发 1×1 `XClearArea`
触发 Expose, 客户端自动补画死区内容 (探针实测恢复 254,0,0)。

## GPU 频率修复 (系统级, 已部署)

`/etc/modprobe.d/mwv207-devfreq.conf`: `softdep mwv207 pre: governor_performance` —
修复 in-tree KMD devfreq 启动即失败的缺陷 (GPU 停 vbios 低频), 重启后自动生效。
源码正解为 `patches/0008-KMD-devfreq-governor-softdep.patch` (MODULE_SOFTDEP)。

## 路径约定

仓库内所有脚本使用**相对自身位置**的路径 (不硬编码用户主目录):
`switch-mwv207-oss.sh` 以脚本所在目录为仓库根; `tests/*.sh` 以 `..` 为仓库根。
移动仓库目录或换用户调用均不受影响。
