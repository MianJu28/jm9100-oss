# JM9100 (景美 MWV207) 开源驱动

Deepin 25 aarch64 / Linux `6.6.155-arm64-desktop-hwe` / JM9100 `[0731:9100]` 的
开源内核驱动（KMD）+ Mesa 24.1.2 用户态驱动（UMD），含全功能图形/视频栈、
系统切换脚本与硬件验收套件。

当前版本：**用户态 `oss-24.1.2-fullstack-r36`**（DRI `78eb18044645d6084f4be4f373a15b1d`、
VA `d847624c2dd9c21048853f536b59fd4a`），**内核态 `BABA169D1C80304D666559D`**。
零环境变量下：GL 4.0（Core/Compat）、GLES 3.2、OpenCL 3.0、VA-API 1.22、VDPAU、
Vulkan 无。

## 能力

| 类别 | 支持 | 说明 |
|---|---|---|
| GL / GLES | 4.0 / 3.2 | 含 GBM/Wayland、DRI3、Present、`GL_EXT_blend_func_extended`（次像素文字用）；glamor blit 快路径 |
| dmabuf | 导出 + 导入往返 | `linear` 与 `VIVANTE_SUPER_TILED` 均可导入（Firefox 的 dmabuf 传递依赖它）；`tests/gl-probe/dmabuf.c` 验收 |
| VA-API 解码 | H.264 ConstrainedBaseline/Main/High、HEVC Main/Main10、VP9 Profile0/Profile2（10-bit） | 全部与软件解码逐字节一致（含跨尺寸参考、`show_existing` 重放帧） |
| VA-API 处理 | `VAProfileNone` + `VAEntrypointVideoProc`（VPP/缩放） | 走 Mesa 通用 compositor，无需驱动侧 codec |
| OpenCL | Rusticl 3.0 | 走同一 Mesa 栈 |
| 编码 | 不支持 | 本 SKU 无编码硬件块（`has_enc=0`） |
| JPEG / VP9 12-bit / H.264 High10/422/444 | 明确拒绝 | 提交前报错或 `vaSyncSurface` 失败，不挂死、不出脏图 |

## 安装与切换

```bash
sudo ./switch-mwv207.sh status        # 用户态栈 + KMD 身份
sudo ./switch-mwv207.sh umd install   # 安装/切换到开源用户态栈（快照 + 自检）
sudo ./switch-mwv207.sh umd verify    # 安装自检（零 env、只读）
sudo ./switch-mwv207.sh umd rollback  # 回滚到上一份快照
sudo ./switch-mwv207.sh kmd stage     # 自建 KMD 放入 DKMS（需重启生效）
sudo ./switch-mwv207.sh stack oss     # 从厂商栈切回（重启）
```

安装后系统级入口为 `/usr/local/sbin/mwv207-oss-switch`；载荷与自检工具放在
`/var/lib/mwv207-oss/`。

## 验收

```bash
tests/verify-installed.sh <srcversion>                      # 21 项：版本/GL/引擎/VAAPI/解码/TDR
DRI_DIR=/usr/local/lib/mwv207-oss tools/gl-regression.sh    # 9 项：GL/GLES + 双源混合 + dmabuf + glmark2
LIBVA_DRIVERS_PATH=/usr/local/lib/mwv207-oss tools/vp9-regression.sh   # 20 个码流逐字节比对
LIBVA_DRIVERS_PATH=/usr/local/lib/mwv207-oss tests/vp9-12bit/run-vp9-12bit.sh  # 12bit 拒绝行为
```

r36 实测：`verify-installed` **PASS=21 FAIL=0 SKIP=3**、`gl-regression` **pass=7 fail=0**
（glmark2 1077）、`vp9-regression` **pass=20 fail=0**、12-bit 测试 **PASS**。

## 性能基线（1920×1080、关掉浏览器）

| 项目 | 数值 |
|---|---|
| glmark2（`:duration=2`） | 1077–1290 |
| present 300² / 800×600 / 1080p | 0.068 / 0.352 / 1.388 ms |
| 纹理上传 1024² 小块 / 全张 | 0.02 / 1.27 ms |
| `glBufferSubData` 4 MB / `glMapBufferRange` 读 | 0.30 ms（13.2 GB/s）/ 0.26 ms（14.9 GB/s） |
| `glReadPixels` 4 MB | 3.54 ms（受 DMA 引擎约 2 GB/s 限制） |
| 硬件解码 1080p（不下载帧） | VP9 161 / H.264 304 / HEVC 311 fps |
| `scale_vaapi` 1080p→720p | 145 fps |

## 已知限制

- **编码**、**JPEG 硬件解码**、**Vulkan**、**H.264 Multiview/StereoHigh**、
  **AVS/AVS2**：本 SKU 或闭源栈不可达（证据见 `docs/UNRESOLVED.md`）。
- **2D 引擎**：命令编码与提交链路已解出，draw 数据面寻址属闭源知识，未打通；
  present 因此无法走 2D。
- **DE 平铺扫描出**：本机镜像不可达，窗口平铺只能靠 UMD 侧“平铺渲染 + 线性影子”。
- **draw 密集场景**与厂商仍有 5–14× 差距，主因是 present 固定拷贝（厂商为
  零拷贝异步翻转）与每帧固定路径；像素密集场景差距 1.2–1.7×，属 ISA 边界。
- VP9 1080p 解码在不同构建间有 165–220 fps 的波动，尚未完全归因。

## 目录

| 路径 | 内容 |
|---|---|
| `switch-mwv207.sh` | 统一安装/切换/回滚脚本（用户态 + 内核态 + 厂商栈切换） |
| `src/mwv207-umd/` | Mesa 24.1.2 UMD（`drivers/mwv207`、`winsys/mwv207`、视频编解码） |
| `src/kernel-6.6/` | KMD（DKMS 打包，唯一维护内核 6.6.155） |
| `tools/` | 构建（`build-umd.sh`）、载荷（`make-payload.sh`）、回归（`gl/vp9-regression.sh`）、基准（`perf-suite.sh`） |
| `tests/` | 硬件验收套件（解码逐字节比对、present/上传/回读基准、健壮性、VAAPI 逐位一致） |
| `docs/` | 交付说明 `DELIVERY.md`、未解问题总账 `UNRESOLVED.md`、交接 `HANDOVER.md`、寄存器参考 |
- [docs/HANDOVER-2D.md](docs/HANDOVER-2D.md)：2D 引擎数据通路的完整交接（协议/寄存器/命令编码/未解缺口/工具）
| `reports/` | 各项分析与对照报告 |
| `payload-46/` | 当前载荷（r36） |

## 文档入口

- **[交付说明](docs/DELIVERY.md)** —— 版本校验、安装/回滚、验收矩阵、维护方法
- **[未解问题总账](docs/UNRESOLVED.md)** —— 已修索引、残余问题与边界证据
- **[交接文档](docs/HANDOVER.md)** —— 系统/仓库布局、方法学、历史提交索引