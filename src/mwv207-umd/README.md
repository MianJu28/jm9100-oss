# `src/mwv207-umd` — 我们维护的 MWV207 用户态驱动源码

这是 **唯一可改** 的驱动源码位置。以前动的是 `src/oh-mesa/User_source_code/mesa-24.1.2`
里的同名目录，等于把上游 oh-mesa 仓当工作目录、还往它自己的 git 上提交 —— 那些提交
（`cd90d9f` / `5499360` / `b2ab2c9`）属于我们，不应该长在上游历史里。2026-11-02 已
把驱动源码整体搬到这里，上游树只留两个符号链接指回来。

## 目录

| 路径 | 内容 |
|---|---|
| `drivers/mwv207/` | Gallium 驱动本体：screen/context/resource/surface/format/draw/blend/sampler/vertex/blit/query/cmd/compute + `nir/`（编译器后端）+ `video-codec/`（H.264/HEVC/VP9 硬解、编码框架） + `include/` |
| `winsys/mwv207/` | 命令流与 BO 管理（DRM ioctl 封装、`cmd` 打包、reloc） |

117 个文件 / 约 5 万行（其中 `video-codec/` 约 2.6 万行）。

## 与上游的关系

`drivers/mwv207` 与 `winsys/mwv207` **不是 Mesa 上游代码**，是景美（JingJiaMicro）为
MWV207/JM9100 写的驱动，由 oh-mesa 移植接到 Mesa 24.1.2 上。所以这里的“上游参考”指的是：

- `src/oh-mesa/User_source_code/mesa-24.1.2/` —— 整个 Mesa 24.1.2 树（只读参考 + 构建宿主）。
  本目录通过符号链接挂进它的 `src/gallium/drivers/mwv207` 与 `src/gallium/winsys/mwv207`，
  meson 构建时无感。
- `patches/0009..0013` —— 本轮改动作为补丁留存，可用于对照/复现（内容已在本树里）。

## 构建

```bash
# 1) 确保链接在（若上游树被重新克隆过，需要重建）
ls -l src/oh-mesa/User_source_code/mesa-24.1.2/src/gallium/drivers/mwv207   # 应为符号链接
./tools/build-umd.sh relink        # 不在或指错时修复

# 2) 增量编译（配置已在 build-full 里）
./tools/build-umd.sh               # = ninja -C <mesa>/build-full

# 3) 出载荷并部署
./tools/make-payload.sh -v <版本>
sudo ./switch-mwv207.sh umd install
```

验证时注意版本握手：构建树产物的版本戳（`24.1.2 (git-<sha>)`）必须与已安装的
`/usr/local/lib/mwv207-oss/libGL.so.1` 一致，否则 DRI 会拒绝加载
（`DRI driver not from this Mesa build`）。改代码后要一起重建 libGL/libEGL 等，
`tools/make-payload.sh` 已强制校验载荷内版本戳唯一。

## 改动约定

- 我们的修改一律用 `FIX(jm9100-oss):` 注释起头，便于和原始厂商代码区分：
  `grep -rn 'FIX(jm9100-oss)' src/mwv207-umd | wc -l`
- 每项修复同步写进 `docs/UNRESOLVED.md`（未修）或 `docs/api/06-KNOWN-ISSUES.md`（已修），
  并附 `file:line`；`docs/tools/validate_doc_citations.py` 会校验引用不越界。
- 驱动侧寄存器语义对照见 `docs/registers/`，闭源驱动反编译结论见 `docs/reverse/`。