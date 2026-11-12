# 纹理上传路径：5–15 ms 固定开销的根因与修复

日期：2026-11-13 凌晨
驱动：UMD（开发构建，基于仓库 `src/mwv207-umd`）、KMD `C588E9673F984406AF46A2F`
对比基线：安装栈 `oss-24.1.2-fullstack-r28`（`payload-45`）

## 1. 现象

`glTexSubImage2D` 每次调用固定花掉几毫秒，且与纹理总大小成正比、与上传块大小无关：

| 纹理 | 上传块 | ms/次 |
|---|---|---|
| 64² | 64×64 | 1.00 |
| 1024² | 64×64 | 4.87 |
| 1024² | 1024×1024 | 7.05 |
| 2048² | 64×64 | 19.64 |

4 MB 上传只比 16 KB 上传贵 2 ms，说明瓶颈不是带宽而是"每次调用都要做的事"。
浏览器（Firefox/WebRender 的 atlas 更新）与视频渲染都会打在这条路径上。

## 2. 定位过程

1. `[TX]` 分段计时（`MWV207_TXTIME`，已删）：`wait` 占 5–14 ms，`create≈0.02`、`blit=0.04–2.19`。
2. 拆 `mwv207_wait_resource()`：`submit=0.00–0.01 ms`，`bo_wait=3.33–13.43 ms`，
   紧接着再调一次 `bo_wait` 立刻返回 0 ⇒ 等的是一枚真实的、尚未 signal 的 fence。
3. 对照：`glClear+glFinish` 0.65 ms/次 ⇒ 3D 管线排队/完成本身很快。
4. 内核侧 kprobe（`kprobe_events`，非 function_graph）：
   `mwv207_submit_ioctl` 最大 4.25 ms，`drm_gem_dma_resv_wait` 最大 10.50 ms；
   80 次上传中有 78 次 submit ioctl >1 ms（均值 14.8 ms）。
5. 多纹理轮换（8 张，避免同资源序列化）：1024² 仍 3.49 ms/次、2048² 13.87 ms/次
   ⇒ 排除"同一资源的 fence 串行"。
6. 读代码：`mwv207_transfer_map()` 对平铺纹理每次 `resource_create()` 一份全尺寸
   STAGING+LINEAR 影子；`retired_staging[16]` 只延迟释放、从不复用。

结论：内核每次都要为 4–16 MB 的新 BO 分配/填充页，代价 ∝ 纹理大小；随后
`transfer_unmap` 的 blit 又要等这份新 BO 上一次使用结束（其实是同一枚 fence 的等待表现）。

## 3. 修复

`src/mwv207-umd/drivers/mwv207/mwv207_resource.c`：

- 新增 `mwv207_wait_resource_timeout(pctx, res, ns)`，winsys `bo_wait` 返回 bool（true=完成）；
  `mwv207_wait_resource()` 保持无限等待语义。
- `mwv207_transfer_map()`：建暂存前在 `retired_staging` 环里找宽高/深度/array/level/format
  全同的一份，取走后有界等待（100 ms）它上一次 blit 读完；超时则放回槽位、本次新建。
- `mwv207_retire_staging()`：等待上限 2 s；新增 96 MB 缓存预算（`MWV207_STAGING_CACHE_BUDGET`），
  超预算的不进环、等 GPU 读完直接释放。
- `MWV207_STAGING_REUSE=0` 用于 A/B。

## 4. 实测

同一进程、同一纹理集（`:0`，`vblank_mode=0`）：

| 场景 | 修前 | 修后 |
|---|---|---|
| 2048²，64×64 上传，8 张轮换 | 14.95 ms/次 | **0.03 ms/次** |
| 1024²，64×64 上传，8 张轮换 | 3.21 ms/次 | **0.03 ms/次** |
| 1024²，整张 4 MB 上传 | 7.09 ms/次（0.53 GB/s） | **1.54 ms/次**（2.4 GB/s） |

## 5. 正确性

- `tests/tex-upload/texcheck.c`：12 轮"整张 A → 整张 B → 读回校验"，外加 8 张纹理 ×
  4 轮小块背靠背上传（检测复用导致的串数据/撕裂）—— **PASS**。
- `tests/query` 4/4；`tests/fbo-test` PASS；`tools/vp9-regression.sh` 19/19；
  `glmark2 -b :duration=2` 1048（安装栈 1073，噪声内）。
- 视频解码路径不受影响（`bo_wait` 语义未变，只是新增了有界版本）。

## 6. 坑

- winsys `bo_wait()` 返回 **bool（true = 成功）**；第一版按"0=成功"判断，导致复用永远被判超时。
- EGL 不指定 `EGL_CONTEXT_CLIENT_VERSION` 时按规范得到 ES1 上下文（`OpenGL ES-CM 1.1`），
  无 shader 能力；探针必须显式请求 ES2。
- `~/vp9-assets/p1080_sw.yuv` 是旧编码参考，曾让 `p1080.webm` 假 FAIL；
  硬件解码与当场软件解码逐字节一致，测试资产必须同批生成。

## 7. 遗留

- `glReadPixels`（WC 读回）与 VAAPI `vaGetImage` 下载仍在同一条"CPU 读映射 BO"链上（0.28 GB/s），
  属于修法 2（KMD cached 映射）的范畴，风险高，未做。
- 暂存缓存预算 96 MB 是经验值；若后续发现大纹理场景内存吃紧，可下调或按上下文配额分配。
