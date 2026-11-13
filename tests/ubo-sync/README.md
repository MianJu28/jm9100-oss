# ubo-sync — 常量缓冲（UBO）同步正确性探针

两个探针，用来验证 buffer map 的等待策略放宽之后没有丢同步：

| 探针 | 做法 | 用途 |
|---|---|---|
| `ubosync.c` | 每帧 `glBufferSubData` 改 UBO 颜色 → 画 → `glReadPixels` 校验 | 逐帧写读都生效（有同步点，验证内容正确） |
| `uborace.c` | 连续 N 帧，每帧改 UBO 颜色并画进 16×16 的独立图块，**全程不 `glFinish`**，最后一次性读回 | 覆盖竞争：若常量上传与在途 GPU 读取之间丢了同步，后面的颜色会盖掉前面的图块 |

编译与运行：

```
cc -O2 -o ~/dev-bin/ubosync tests/ubo-sync/ubosync.c -lEGL -lGLESv2
cc -O2 -o ~/dev-bin/uborace tests/ubo-sync/uborace.c -lEGL -lGLESv2
DISPLAY=:0 XAUTHORITY=/home/admin/.Xauthority \
  LIBGL_DRIVERS_PATH=/home/admin/dev-dri MESA_LOADER_DRIVER_OVERRIDE=mwv207 \
  ~/dev-bin/ubosync 200 ; ~/dev-bin/uborace 128
```

2026-11-13 实测：两者均 `PASS`（`0/200 frames mismatched`、`0/128 tiles mismatched`）。
