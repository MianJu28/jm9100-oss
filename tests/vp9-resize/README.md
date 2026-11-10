# VP9 跨尺寸参考（B2）诊断

`docs/UNRESOLVED.md` §1.8 的可复跑版本：拿两族 WebM 官方测试向量，用 libvpx 软件
解码与 mwv207 硬件解码逐字节比对，并统计驱动打印的"参考缺失 → frame skipped"行数。

| 向量 | 期望 |
| --- | --- |
| `vp90-2-14-resize-10frames-fp-tiles-1-2.webm` | 普通 resize + tile，硬件与软件 **IDENTICAL** |
| `vp90-2-21-resize_inter_320x180_5_1-2.webm` | 参考尺寸变化，驱动每帧报 `reference 0 (slot 0) has no decoded picture; frame skipped`，但输出字节数仍与软件相同、内容 DIFF（I-6 候选） |

```sh
./run-vp9-resize.sh                # 第一次需要联网下载向量
./run-vp9-resize.sh --local DIR    # 复用已下载的向量目录
```

脚本是**诊断工具**：第二族出现 DIFF 就是当前已知状态（拦截没有传到应用），
不判 PASS/FAIL。真正要修的两条路：支持跨尺寸参考，或让"跳过"对上层的
surface/fence 可见（见 §1.8）。