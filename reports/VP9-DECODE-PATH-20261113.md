# VP9 硬解为什么只有 8 fps —— 参考槽零拷贝改造（2026-11-13）

## 结论

VP9 1080p 硬件解码 **7.1–8.5 fps → 224 fps**（`ffmpeg -hwaccel_output_format vaapi`，
不下载帧）；若命令不带 `-hwaccel_output_format vaapi`，ffmpeg 会把每帧下载回系统内存，
走 0.28 GB/s 的 WC 映射读（1080p 约 10 ms/帧），测出来是 73–80 fps —— 那是另一条链。
360p **63.4 → 160 fps**（同一口径）。解码输出与改动前逐字节相同。

## 根因链（全部用 `MWV207_DECTIME` 插桩证实，测完已删除）

1. `mwv207_dec_vp9_submit_done()` 每帧调用 `vp9_refresh_slots()`，后者把解码输出面
   (`buf->res`，普通 GEM BO、WC 映射) 用 **CPU `memcpy`** 拷进每张被刷新参考帧的槽位。
   1080p 每帧最多 8 槽 × 3.1 MB ≈ **25 MB**，实测 **70–90 ms/帧** ⇒ 7–8 fps。
   槽级实测：`memcpy` 345 KB = 1.02 ms ⇒ **0.34 GB/s**，正是 WC 映射读的典型带宽。
2. 拷完之后紧接着一个全范围 `__builtin___clear_cache(scratch, scratch + mv_offset)`：
   1080p 覆盖约 4.4 MB（2×2 MB 段映射 + 2 MB 滤波系数表，本帧根本没写），
   ARM 上 **30–45 ms/帧**（每 64 B cache line 一条 `dc civac`）。
3. `vp9_write_ref_regs()` 给三路参考各拷一份上一帧的 MV 图（1080p 每份 522 KB、三份），
   读写都在同一个 WC 映射里 ⇒ **25–30 ms/帧**。

三者相加 ≈ 130–165 ms/帧，但客户端并行度与同步把它压到约 120 ms，即 8 fps。

## 改法

| 改动 | 位置 | 说明 |
|---|---|---|
| 参考槽别名 | `mwv207_dec_vp9.c`：`vp9_ref_reserve()` / `vp9_pic_pick()` / `vp9_refresh_slots()` | 解码单元输出直接写进参考环 `ref_store`；`refresh_slots` 只更新 `ref_off[i]`，不拷像素。VP9 参考内容不可变 ⇒ 别名安全；16 个环位置 > 8 个逻辑槽，长寿命参考不会被覆盖 |
| 定向刷 cache | 同上，`set_regs()` | 只冲刷概率表/计数区/tile 表；段映射只在 `intra_only \|\| error_resilient` 的 memset 之后单独冲刷 |
| MV 双缓冲 | `vp9_write_ref_regs()` / `submit_done()` | 两个 MV 槽轮换：本帧写 `mv_write_off`，参考 `DBASE` 指向另一半；`VP9_MV_SLOTS` 3→1 |

## 验证

- `tools/vp9-regression.sh`：**18/18**。
- `tests/vp9-resize/run-vp9-resize.sh`：`vp90-2-21-resize_inter_320x180_5_1-2.webm` **IDENTICAL**（B2 跨尺寸参考未回退）。
- `tests/vp9-intraonly/run-vp9-intraonly.sh`：4/5/6/7 帧 **IDENTICAL**（I-5/N18 未回退）。
- 改动前后 120 帧输出 `cmp`：360p、1080p 均**逐字节相同**。

## 遗留

- `glTexSubImage2D` 0.51 GB/s、`glReadPixels` 2.8 ms、HEVC/H.264 的 `buf->res` 路径仍在
  "WC 映射读慢"这条链上；KMD 侧让系统内存 BO 走 cached 映射（并处理一致性）是下一步候选。
- 与厂商驱动的同口径对照（同一 ffmpeg 命令、vendor 栈）尚未做，需要切栈重启。
