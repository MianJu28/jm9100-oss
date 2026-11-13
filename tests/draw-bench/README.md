# draw-bench — 每 draw 的 CPU 开销探针

用法：

```
cc -O2 -o ~/dev-bin/drawbench tests/draw-bench/drawbench.c -lEGL -lGLESv2
DISPLAY=:0 XAUTHORITY=/home/admin/.Xauthority \
  LIBGL_DRIVERS_PATH=/home/admin/dev-dri MESA_LOADER_DRIVER_OVERRIDE=mwv207 \
  ~/dev-bin/drawbench <draws/frame> <frames> [use_vbo]
```

`use_vbo=0`（默认）把顶点数据放在客户端内存里（client arrays），`use_vbo=1` 用 `GL_ARRAY_BUFFER`。
视口/裁剪都固定成 8×8，所以测到的时间基本等于每 draw 的 CPU 开销。

2026-11-13 实测（r31 栈 + 开发驱动）：

| 形态 | 每 draw | 吞吐 |
|---|---|---|
| VBO | 0.45–0.53 µs | ~200 万 draws/s |
| client arrays | 26–44 µs | ~2.5 万 draws/s |

client-array 慢 50 倍的根因是 `mwv207_transfer_map()` 的 `PIPE_BUFFER` 分支每次 map 都
`mwv207_wait_resource()`（强制提交 + 无限 `bo_wait`），详见
`reports/VENDOR-GAP-20261113.md` 的"每 draw 与 present 定位"一节。
