# u_threaded_context 接入尝试（2026-11-16）：结论是不值得

## 为什么试

`reports/VENDOR-GAP-20261113.md` 记录 draw 密集场景（build/bump/pulsar）与厂商差
5–14×，归因于"我方未启用 `u_threaded_context`，每 draw 的 st/mesa 状态跟踪都在
应用线程上做"。本轮把这条假设验证到底。

## 做了什么（已全部回退，diff 存档见文末）

1. `struct mwv207_resource` 的 `struct pipe_resource base;` 换成
   `union { struct pipe_resource base; struct threaded_resource threaded; }`，
   `resource_create()` 末尾 `threaded_resource_init(&res->base, false)`、
   `mwv207_resource_destroy()` 开头 `threaded_resource_deinit(pres)`。
2. `struct mwv207_transfer` 同样加 `union { struct pipe_transfer base;
   struct threaded_transfer threaded; }`（TC 会往 `threaded_transfer` 里写
   `valid_buffer_range`/`cpu_storage_mapped`，slab 按本结构大小分配，否则越界）。
3. `struct mwv207_query` 起始处加 `struct threaded_query threaded;`
   （TC 在对象开头写 `head_unflushed`/`flushed`，原来那里是 `enum pipe_query_type`）。
4. `mwv207_context_create()` 在 `flags & PIPE_CONTEXT_PREFER_THREADED` 且
   `MWV207_THREADED`（默认开）时调用
   `threaded_context_create(&ctx->base, &screen->transfer_pool, NULL, NULL, &tc)`。
5. **Mesa 侧补丁**：24.1.2 的 `u_threaded_context.c` 没有包装
   `pipe_context::set_hw_atomic_buffers`，而 `st_bind_hw_atomic_buffers()` 会直接
   调 `st->pipe->set_hw_atomic_buffers`（`src/mesa/state_tracker/st_atom_atomicbuf.c:168`）
   ⇒ TC 上下文的该指针是 NULL，`glDrawArrays` 里跳到 0 地址。
   补法：`u_threaded_context_calls.h` 加 `CALL(set_hw_atomic_buffers)`，
   在 `set_shader_buffers` 旁加 `tc_hw_atomic_buffers` 结构 +
   `tc_call_set_hw_atomic_buffers()` + `tc_set_hw_atomic_buffers()`，
   并在 `tc_create` 里 `CTX_INIT(set_hw_atomic_buffers)`。

## 实测结果（`~/dev-bin/drawbench`，EGL + GLES3）

| 阶段 | 现象 |
|---|---|
| 只改 1+4（未子类化 transfer/query） | `malloc(): invalid size (unsorted)`，gdb 停在 `calloc` ← `_mesa_bufferobj_alloc`（堆元数据已被写坏） |
| 加 2+3（子类化） | 改成 `slab_alloc: Assertion '(elt)->magic == 0x7ee01234' failed`（transfer 池的 freelist 元素被覆盖） |
| 加 5（原子缓冲包装）后才走到这里 | 上面那步，崩溃点前移但仍在 TC 路径 |
| `MWV207_THREADED=0`（同一构建，只关 TC 上下文） | 正常：0.84–1.17 µs/draw |

即：**TC 的所有已知契约都补上了，仍有一处把 transfer 池写坏；未见任何性能收益。**

## 为什么不再追（关键：收益本来就很小）

今天在 r35 安装栈补测了"只渲染 vs 带 swap"（`glmark2 -b <scene>:duration=2` 与
`--off-screen`）：

| 场景 | 只渲染 | 带 swap | present 开销 |
|---|---|---|---|
| build | 0.258 ms（3881 fps） | 0.653 ms（1532 fps） | **0.395 ms** |
| pulsar | 0.340 | 0.717 | 0.377 |
| conditionals | 0.470 | 0.811 | 0.341 |
| shading | 0.437 | 0.739 | 0.302 |

厂商 build 全套 14537 fps（0.069 ms/帧），其中"只渲染"部分约 0.167 ms ⇒ 即使把渲染
部分做到厂商水平，**present 的 0.395 ms 仍会让 build 停在约 2400 fps**（现在 1532）。
换句话说，draw 密集场景的 5–14× 差距里 **present 占绝对主导**，而 present 的整帧
拷贝已经确认是结构性差异（2D 引擎收口、DE 平铺扫描出不可达，
见 `docs/UNRESOLVED.md` §4 与 2D 两节）。TC 能把渲染部分从 0.258 ms 压到大约
0.20 ms，最好情况让 build 从 1532 升到 1700 左右，且要承担并发正确性风险（transfer
池、`retired_staging`、video 上下文互斥等全部要审）。
⇒ 决定：**回退，不再启用 TC**；把这条从"性能待办"里删掉，避免以后重复踩坑。

## 存档

- 回退前的工作树 diff：`~/work-note-tc-diff.txt`（含当时的 `[CT]`/`[ST]` 统计插桩，
  那些不是 TC 的一部分）。
- Mesa 侧那 5 号补丁的完整内容见本文 §"做了什么"第 5 条，可直接重放；
  它本身是**上游缺陷**（TC 未包装 set_hw_atomic_buffers），若以后真要启用 TC，
  建议把这条提给 Mesa。
