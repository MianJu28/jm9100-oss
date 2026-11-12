# buffer 的放置与 CPU 侧映射属性（2026-11-13）

## 结论

MWV207 的 buffer 资源原先全部建在 VRAM，而 VRAM 在 `mwv207_ttm_io_mem_reserve()` 里被设成
`ttm_write_combined` + `is_iomem`，所以 CPU 侧映射是 write-combined：写够快、**读极慢**
（4 MB mmap 读 0.18 GB/s）。系统内存 / GTT 的映射是普通 cached 页属性，读 13 GB/s。

把 `PIPE_BUFFER` 改用 `MWV207_DOMAIN_GTT` 后：

| 操作（4 MB buffer） | VRAM（旧） | GTT（新） | 倍率 |
|---|---|---|---|
| `glBufferSubData` | 2.10 ms（1.86 GB/s） | 0.34 ms（11.64 GB/s） | 6.2× |
| `glMapBufferRange(READ)` + memcpy | 24.78 ms（0.16 GB/s） | 0.31 ms（12.44 GB/s） | 80× |
| `glMapBufferRange(WRITE)` + memcpy | 2.21 ms（1.77 GB/s） | 0.34 ms（11.55 GB/s） | 6.5× |

GPU 侧无回归，正确性回归全过。

## 1. 现象

`tests/tex-upload/readback-bench.c` 在安装栈 r29 上：

- `glBufferSubData` 4 MB：72–99 ms/30 次 → **1.12–1.52 ms/次（2.6–3.5 GB/s）**
- `glMapBufferRange(GL_MAP_READ_BIT)` + `memcpy` 4 MB：约 **15 ms（0.28 GB/s）**

读比写慢一个数量级，是 write-combined 映射的典型特征（写入合并成突发，读要逐次往返）。

## 2. 直接测三种放置的 mmap 属性

`tests/bo-mmap/bo-mmap-bench.c` 绕开 GL：`DRM_IOCTL_MWV207_GEM_CREATE`（`preferred_domain` =
0x1 系统 / 0x4 GTT / 0x2 VRAM）→ `DRM_IOCTL_MWV207_GEM_MMAP` → `mmap` → memcpy 读 / 写 / 稀疏读。

```
domain=system 4 MB  读 0.32 ms (12.06 GB/s)  写 0.35 ms (11.23 GB/s)  稀疏读 0.04 ms
domain=gtt    4 MB  读 0.30 ms (13.02 GB/s)  写 0.33 ms (11.73 GB/s)  稀疏读 0.04 ms
domain=vram   4 MB  读 21.67 ms (0.18 GB/s)  写 1.98 ms (1.97 GB/s)  稀疏读 5.58 ms
```

对照 KMD：`src/kernel-6.6/mwv207_ttm.c:102` 的 `mwv207_ttm_io_mem_reserve()` 只对
`TTM_PL_VRAM` 设 `mem->bus.is_iomem = true` 与 `mem->bus.caching = ttm_write_combined`，
`TTM_PL_SYSTEM` / `TTM_PL_TT` 直接 `return 0`，由 TTM 走非 iomem 的默认（cached）页属性。
所以这不是"WC 读慢"的笼统结论，而是**放置选错了**：buffer 全部落在 VRAM。

## 3. 改法

`src/mwv207-umd/drivers/mwv207/mwv207_resource.c` 的 `resource_create()`，在原先
`PIPE_USAGE_STAGING → GTT` 之后加：

```c
if (res->base.target == PIPE_BUFFER &&
    !debug_get_bool_option("MWV207_BUFFER_VRAM", false))
   domain = MWV207_DOMAIN_GTT;
```

`MWV207_BUFFER_VRAM=1` 退回旧行为，便于定点 A/B 或某场景需要 VRAM 时兜底。

## 4. 实测

`tests/tex-upload/buffer-bench.c`（ES3 pbuffer，4 MB `GL_ARRAY_BUFFER`，20 轮，同一进程）：

```
--- GTT (新默认) ---
glBufferSubData   4MB x20:    0.34 ms ( 11.64 GB/s)
map READ + memcpy 4MB x20:    0.31 ms ( 12.44 GB/s)
map WRITE + memcpy 4MB x20:    0.34 ms ( 11.55 GB/s)
--- VRAM (旧行为) ---
glBufferSubData   4MB x20:    2.10 ms (  1.86 GB/s)
map READ + memcpy 4MB x20:   24.78 ms (  0.16 GB/s)
map WRITE + memcpy 4MB x20:    2.21 ms (  1.77 GB/s)
```

GPU 侧（同一驱动 A/B）：

| 场景 | VRAM | GTT |
|---|---|---|
| `glmark2 -b buffer:duration=2` | 655 | 655 |
| `glmark2 -b build:duration=2` | 1526 | 1522 |
| `glmark2 -b jellyfish:duration=2` | 666 | 686 |
| `glmark2 -b :duration=2`（全套） | — | **1031**（安装栈 r29：1041） |

正确性：`tests/query` 4/4、`tests/fbo-test` PASS（`FBO status 0x8cd5 COMPLETE`）、
`tests/tex-upload/texcheck.c` PASS（12 轮 + 8×4 压力）、`tools/vp9-regression.sh` 19/19。

## 5. 坑

- 裸 `ioctl(fd, _IOWR(...), &arg)` 打 GEM_CREATE 时偶发 `EACCES`（render 节点的 `DRM_AUTH`
  检查），用 `drmCommandWriteRead(fd, 0x01, ...)` 一次成功 —— 探针里不要再手搓命令号。
- `printf` 的 stdout 走管道时是全缓冲，探针里要么 `fflush` 要么 `stdbuf -o0`，否则看起来"没有输出"。
- 自建 BO 后 **不要 `close(handle)`**：DRM handle 与 fd 是两套编号，`close` 会误关真实 fd。

## 6. 遗留

1. 重几何场景（顶点数据远超 LLC）从系统内存取顶点可能比 VRAM 慢。本轮 glmark2 的 build/vertex
   类场景没有退化；若将来某个应用退化，用 `MWV207_BUFFER_VRAM=1` 定点关闭即可。
2. buffer 导出成 dmabuf 时 memory type 变成 TT，跨进程共享未做专项测试（现有测试都是同进程）。
3. 纹理仍留在 VRAM（平铺 + TS 需要），其上传路径走 staging 影子（见
   `reports/TEXTURE-UPLOAD-PATH-20261113.md`），本次改动不影响。