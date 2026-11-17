# JM9100 (MWV207) 开源驱动 — 交付说明

本文件描述**交付物、安装、验收、能力边界与维护方法**。所有内容对应当前稳定版本，
不含过程记录（过程见 `reports/`，未解问题见 `docs/UNRESOLVED.md`）。

## 1. 交付物与版本

| 项 | 值 |
|---|---|
| 用户态栈 | `oss-24.1.2-fullstack-r36` |
| 载荷目录 | `payload-46/`（`VERSION = 24.1.2-fullstack-r36`） |
| Mesa UMD | `mwv207_dri.so` md5 `78eb18044645d6084f4be4f373a15b1d`；`libmwv207_drv_video.so` md5 `d847624c2dd9c21048853f536b59fd4a` |
| 内核态 | srcversion `BABA169D1C80304D666559D`（DKMS 包 `mwv207-oss/1.0.0`） |
| 内核 | `6.6.155-arm64-desktop-hwe`（唯一维护；6.6.143 headers 残缺，不再支持） |
| 硬件 | JM9100 `[0731:9100]` @ `0000:07:00.0`，VRAM 2048 MB，HDMI-1 1920×1080@100Hz |
| 源码 | `src/mwv207-umd/`（Mesa 24.1.2 分支）、`src/kernel-6.6/`（KMD） |

栈组成（载荷 11 个实体文件）：DRI 驱动、VA-API 驱动（`libmwv207_drv_video.so` +
`mwv207_drv_video.so`）、glvnd ICD `libEGL_jm.so.0`、VDPAU 驱动、OpenCL（Rusticl）配置、
`MANIFEST` 校验清单。

配套系统组件（非载荷，随主机安装）：

- `libglamoregl.so`：`patches/xserver/0001-glamor-blit-fast-path.patch` 构建版
  （`tools/build-xglamor.sh`，可 `--rollback` 还原）。缺失时 present 慢约 2×。
- `/usr/lib/aarch64-linux-gnu/dri/mwv207_dri.so`：指向安装栈的软链（AIGLX 必需，
  缺失会让 glamor 退回 llvmpipe → 软件合成）。

## 2. 环境要求

| 依赖 | 版本/说明 |
|---|---|
| 发行版 | Deepin 25 aarch64（Debian 系） |
| 内核 | 6.6.155-arm64-desktop-hwe + 对应 headers（编译 DKMS 模块） |
| Mesa | 24.1.2（`ref/oh-mesa/User_source_code/mesa-24.1.2` 构建树） |
| libva | 1.22（`vainfo` 报 libva 2.19.0） |
| 其他 | `drm` headers、Xorg 21.1（glamor）、`systemd`（scope/看门狗） |

## 3. 安装

```bash
# 用户态（会建快照、重启 lightdm，脚本自动脱离 lightdm cgroup）
sudo ./switch-mwv207.sh umd install
./switch-mwv207.sh status           # 核对：栈号、DRI/VA md5、快照 ID

# 内核态（写入 DKMS + 刷新 initramfs，重启后生效）
sudo ./switch-mwv207.sh kmd stage
sudo depmod -a 6.6.155-arm64-desktop-hwe
sudo update-initramfs -u -k 6.6.155-arm64-desktop-hwe
sudo reboot
```

注意：`kmd stage` 只更新 `/lib/modules/.../updates/dkms/mwv207.ko`，而冷启动优先加载
`/boot/initrd.img-*` 里的同名副本 —— **必须刷新 initramfs**，否则重启后仍是旧模块。

安装后系统级入口：`/usr/local/sbin/mwv207-oss-switch`；载荷与自检工具缓存在
`/var/lib/mwv207-oss/`；历史快照在 `/var/lib/mwv207-oss/switch-history`（保留最近 4 个）。

## 4. 回滚与栈切换

```bash
sudo ./switch-mwv207.sh umd rollback   # 回到上一份用户态快照
sudo ./switch-mwv207.sh stack vendor   # 切厂商栈（重启；桌面会停，看门狗超时自动回滚）
sudo ./switch-mwv207.sh stack oss      # 切回开源栈（重启）
sudo ./switch-mwv207.sh kmd install    # 恢复发行版自带 KMD（重启）
```

## 5. 验收矩阵（r36 实跑结果）

| 命令 | 判据 | r36 结果 |
|---|---|---|
| `tests/verify-installed.sh <srcversion>` | 版本/GL/引擎/VAAPI 能力/三类解码逐位一致/TDR/dmesg | **PASS=21 FAIL=0 SKIP=3**（SKIP 为需 root 的 dmesg/重启类） |
| `DRI_DIR=/usr/local/lib/mwv207-oss tools/gl-regression.sh` | query 4/4、fbo、texcheck、texbench、bufbench、rbbench、glmark2 | **pass=7 fail=0**（glmark2 1077） |
| `LIBVA_DRIVERS_PATH=/usr/local/lib/mwv207-oss tools/vp9-regression.sh` | 20 个码流与 libvpx 逐字节一致 | **pass=20 fail=0** |
| `tests/vp9-intraonly/run-vp9-intraonly.sh` | 7/7 帧（含 3 个重放帧） | **IDENTICAL** |
| `tests/vp9-resize/run-vp9-resize.sh` | 跨尺寸参考 | 1 IDENTICAL + 1 KNOWN-DIFF（滤波核差异，平均 <2 级） |
| `tests/vp9-12bit/run-vp9-12bit.sh` | 10bit 正常、12bit 优雅拒绝 | **PASS** |
| `tools/perf-suite.sh <label>` | 全量性能基线 | 结果写 `~/perf-<label>.txt` |

## 6. 能力矩阵

| 类别 | 支持 |
|---|---|
| OpenGL / GLES | 4.0（Core+Compat）/ 3.2；EGL、GLX、GBM、Wayland、DRI3 |
| VA-API 解码 | H.264 ConstrainedBaseline/Main/High（8bit）、HEVC Main/Main10、VP9 Profile0/Profile2（10-bit） |
| VA-API 处理 | `VAProfileNone` + `VAEntrypointVideoProc`（缩放/格式转换，`scale_vaapi`/`format=nv12`） |
| VDPAU | 解码 + 呈现 |
| OpenCL | Rusticl 3.0 |
| 视频输出下载 | `hwdownload`（NV12/P010）、`hwupload`、`vaDeriveImage` |
| 明确拒绝（不挂死） | JPEG baseline、VP9 12-bit、H.264 High10/High422/High444 |
| 不支持 | 编码（H.264/HEVC/JPEG）、Vulkan、H.264 Multiview/StereoHigh、AVS/AVS2 |

## 7. 性能基线（1920×1080，关闭浏览器，r35/r36 同机）

| 项目 | 数值 |
|---|---|
| glmark2（`:duration=2`） | 1077–1290 |
| glxgears（`vblank_mode=0`） | ~6500 FPS |
| present 300² / 800×600 / 1080p | 0.068 / 0.352 / 1.388 ms |
| 纹理上传 1024² 小块 / 全张 | 0.02 / 1.27 ms |
| `glBufferSubData` 4 MB / `glMapBufferRange` 读 | 0.30 ms（13.2 GB/s）/ 0.26 ms（14.9 GB/s） |
| `glReadPixels` 1024² / 2048² | 3.54 / ~11 ms（DMA 引擎约 2 GB/s） |
| 硬件解码 1080p（不下载帧） | VP9 161–220 / H.264 304 / HEVC 311 fps |
| `scale_vaapi` 1080p→720p | 145 fps |

## 8. 已知限制（均为已定性边界，非待办）

1. **编码**：本 SKU 无编码硬件（厂商芯片表 `jmgpu_9100_info.has_enc = 0`，`0x920000` 引擎窗口读回 `0xffffffff`），官方规格只在 JM92/JM1100 提供编码。
2. **JPEG 硬件解码**：厂商 VA-API 可用，但需要闭源 KMD/UMD 的未公开启动条件；我方实现只进 `DEC_ERROR_INT`（`0x10102`）+ TDR，输出面零字节。
3. **Vulkan**：厂商包无 ICD、无 `vk*` 入口；Mesa 树无 mwv207 Vulkan 驱动；KMD 的 syncobj 字段仅注释占位。
4. **2D 引擎**：命令编码与 waitlink 链路已解出并可执行，draw 数据面寻址属闭源知识，未能落像素。
5. **DE 平铺扫描出**：`DCREG_FRAME_BUFFER_CONFIG(0x1518)` 在本机镜像不可达，窗口平铺改为 UMD 侧“平铺渲染 + 线性影子”。
6. **draw 密集场景差距**：与厂商 5–14×，主因是 present 固定拷贝（厂商为零拷贝异步翻转）与每帧固定路径；像素密集场景 1.2–1.7× 属 ISA 边界（超越函数只有单 lane）。
7. **VP9 解码帧率**：不同构建间 165–220 fps 波动，未完全归因。
8. **多视口 / 12bit VP9**：硬件能力未知、P010 路径止于 10bit。

## 9. 维护

```bash
./tools/build-umd.sh                                   # 重建 UMD（check/relink/build/sim）
rm -rf payload-46 && ./tools/make-payload.sh -v 24.1.2-fullstack-rNN   # 打载荷
./tools/dkms-sync.sh                                   # KMD 源码同步到 DKMS 目录
sudo ./tools/build-xglamor.sh                          # 重建/安装 glamor blit 补丁版
sudo ./tools/build-xglamor.sh --rollback               # 还原系统 glamor
```

改动流程：源码改动 → `build-umd.sh` → 拷到 `~/dev-dri/`（`mwv207_dri.so`、
`libmwv207_drv_video.so`、`mwv207_drv_video.so`，后者需先 `rm -f` 符号链接）→
用 `LIBGL_DRIVERS_PATH` / `LIBVA_DRIVERS_PATH` 指向它做回归 → 打载荷 → 用户安装 → 安装栈复验收。

内核侧改动：改 `src/kernel-6.6/` → `./tools/dkms-sync.sh` → `kmd stage` → `depmod` →
`update-initramfs` → 重启 → `tests/verify-installed.sh <srcversion>`。

## 10. 文档索引

| 文件 | 内容 |
|---|---|
| `README.md` | 概览、能力、快速使用、性能基线、限制 |
| `docs/DELIVERY.md` | 本文件：交付、安装、验收、维护 |
| `docs/UNRESOLVED.md` | 未解问题与边界证据（含各专项实证） |
| `docs/HANDOVER.md` | 系统/仓库布局、方法学、提交索引 |
| `reports/` | 各阶段分析（present、回读、VP9、JPEG、编码、厂商对照等） |
| `docs/registers/` | 寄存器与命令编码参考 |
| `tests/*/README.md` | 各验收套件用法 |