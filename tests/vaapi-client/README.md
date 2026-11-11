
## VP9 superframe

VP9 允许把一个 packet 打包成多个子帧（superframe）：末字节 marker 的低 3 位是
(帧数-1)，3~4 位是 (长度字段字节数-1)，索引 = 1 字节 marker 副本 + 帧数×长度
字段 + 1 字节 marker。`vadec` 会先拆包再逐子帧提交；官方
`vp90-2-16-intra-only.webm` 的首包就是这样（117963 B → 4 个子帧）。

不拆的话驱动会把整包当一帧解，输出与 libvpx 不一致（见
`docs/UNRESOLVED.md` §1.7）。
