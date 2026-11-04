# VP9 硬解（D1）交接文档

2026-11-04 · 状态：未完成（WIP，未提交代码）。目标：VP9 Profile0 8bit 硬解输出与软件解码逐位一致后提交。

## 1. 结论摘要

- 已打通：VA 广播（vainfo 显示 `VAProfileVP9Profile0 : VAEntrypointVLD`）、码流头解析、寄存器序列（对照厂商 1.8.0 `jmgpu_drv_video.so` 反汇编）、出图链路（decoder staging → PP → VA 导出）。
- 关键帧解码**空间正确**。EC_BYPASS 修复后 staging 整帧连续写入；纯色/黑白分屏内容可辨。
- 当前唯一阻塞：**inter 帧（P 帧）输出为空**。w6（白→黑循环）结果 `[151, 0, 151, 0]`；30 帧真实视频 PSNR 11.8。参考帧内容已验证有效（ref 槽首字节 `97`=151），所以问题不是参考数据，而是硬件 inter 命令未产出（既无预测也无残差写入 staging）。

## 2. 测试环境

构建（VA/B 目标）：

```sh
ninja -C ref/oh-mesa/User_source_code/mesa-24.1.2/build-full \
      src/gallium/targets/va/libmwv207_drv_video.so
cp src/gallium/targets/va/libmwv207_drv_video.so /tmp/gd3/   # dlshim 重定向目标
```

测试（/tmp/dlshim.so 把 dlopen 的 mwv207_drv_video.so 指到 /tmp/gd3）：

```sh
MWV207_VP9_CLEAR=1 mwv207_debug=video LIBVA_DRIVER_NAME=mwv207 \
LD_PRELOAD=/tmp/dlshim.so ffmpeg -v error -y -hwaccel vaapi \
  -hwaccel_device /dev/dri/renderD128 -hwaccel_output_format vaapi \
  -i /tmp/w6.webm -vf hwdownload,format=nv12 -pix_fmt yuv420p -f rawvideo /tmp/hw6.yuv
```

- `mwv207_debug=video`（小写）让视频提交同步等待，否则读 staging 是竞态。
- 素材：`/tmp/w6.webm`（白/黑 2 帧循环 50 帧）、`/tmp/vp9-640.webm`（30 帧实拍）、`/tmp/sw-vp9.yuv`（软件参考）、`/tmp/s-half.webm`（左白右黑）。
- 软件参考生成：`ffmpeg -i x.webm -f rawvideo -pix_fmt yuv420p sw.yuv`。

## 3. 已实现的代码（工作树未提交）

- `video-codec/mwv207/mwv207_dec_vp9.c`（新建，约 700 行）：`mwv207_dec_vp9_set_regs()`、scratch 布局、参考帧自持存储、PP 配置、env 调试钩子。
- `video-codec/mwv207/mwv207_dec_vp9.h`（新）：`set_regs` / `submit_done` / `destroy_state`。
- `common/mwv207_decoder_vp9_slice_header.c`：新增 `mwv207_vp9_parse_frame(sh, data, size, fallback_w, fallback_h)` wrapper（解析函数仍 static）；bool 解码器尾溢容忍（strm_len 传整帧剩余）。
- `mwv207_video_dec.h/.c`：dec 增加 `void *vp9_state`，destroy 路径清理。
- `mwv207_video_decoder.c/.h`：`get_vp9_msg`、end_frame 分派、`CTA` 提交后调用 `mwv207_dec_vp9_submit_done()`。
- `meson.build`：源列表加入 dec_vp9.c。
- `mwv207_video_codec.c`：profile allowlist 加入 VP9 Profile0。

WIP 备份（每轮 `git checkout` 前保存）：

```
/home/admin/vp9-wip-20261103/tracked-v28.patch
/home/admin/vp9-wip-20261103/mwv207_dec_vp9.c.v28
/home/admin/vp9-wip-20261103/mwv207_dec_vp9.h.v28
```

## 4. 寄存器序列（已对齐厂商）

每帧顺序：解析 → 写 prob 表/tile 表 → DEC_MODE=VP9 → common config → 尺寸 → DEC_OUT → slice header → loop filter → segment → prob/counter/tile/filter 基址 → PP → TILE_ENABLE → 流窗口 → DEC_E=1。

关键 id（与厂商 `SetDecRegister` 一致，1840 项名表下标）：

- 流：82 STRM_START_BIT、136 STREAM_LEN、1023/1025 STREAM_BASE、1127/1128；
- tile：245 TILE_ENABLE、241/243 行列、1021/1022 TILE_BASE；
- 尺寸：69/70（CBS）、423/424（4X4）、279/280 MIN/MAX_CB_SIZE（3/6，缺了会全 0）；
- 输出：52 OUT_DIS、61 EC_BYPASS（必须 1）、824/826 YBASE、951/952 CBASE、1161/1162 stride；
- 参考：829/832 REFER0 Y、953/954 C；855/857/961/962 REFER4；861/863/963/964 REFER5；605/606 宽高；640…662 缩放；
- 切片头：201/202、294/295/296、298、196、261/271/272/273、301/300/299、57 WRITE_MVS_E、16；
- probe 表 memcpy 3744 B（= sizeof(mwv207_vp9_entropy_probs)）。

厂商报告全文：`/tmp/vp9-re.md`（842 行）。

## 5. 已知坑

- 初始化必须：scratch 创建时 memset 一次；不要每帧清（prob/计数器跨帧）。
- `mwv207_video_dec_destroy_buffer()` 每帧调用；vp9_state 销毁必须放在 `mwv207_video_dec_destroy()`，否则参考槽每帧丢。
- 参考帧不能指向 VA target buffer（会释放），必须驱动自持 staging BO。
- 调试打印不要在 bo_map 指针上按 640×360 固定偏移读，小分辨率会越界 SIGSEGV。
- 8bit 核幅度：白 QP 上限 221（软件 235），关系约 `Yhw = 128 + 0.868·(Ysw-128)`；PP rangemap 可补偿但 5bit coef 达不到逐位。

## 6. 下一步（建议按序）

1. **判别 inter 命令是否被执行**：inter 帧加临时日志/寄存器快照（或临时关 DEC_E、对比 dmesg）。当前现象是 staging 全空，连残差都没有。
2. **对齐 HEVC 参考路径**：HEVC 用全 16 槽 `ref_ybase[]/ref_cbase[]/ref_dbase[]`，并写 `WRITE_MVS_E=1`、`DIR_MV_BASE`、每槽 DBASE 与 dir_mv 区域（staging 尾部 `luma*3/2+64` 起，`mbw*mbh*16`）。VP9 路径目前没有 dir_mv 区、`WRITE_MVS_E=0`。
3. 若 2 后仍 0：检查 `REFERx_VALID_E/REFERx_LTERM_E`、参考 tiled/stride 位，以及 PP 输出对 inter 帧是否被覆盖。
4. 幅度问题最后处理（QP 映射或 PP rangemap）。

## 7. 验证标准

- `w6`：期望 `[151, 16, 151, 16]`（黑 16、白 151，crf 流）。
- 30 帧实拍：Y 全帧 PSNR > 35（第 1 帧可先只比关键帧）。
- 不新增 dmesg `job ... timed out`；`umd status` 仍通过。