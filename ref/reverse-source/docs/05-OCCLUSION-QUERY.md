# 05 · 遮挡查询：从"不可解"到寄存器级定位

## 0. 结论先行

| 项 | 结论 |
|---|---|
| 此前判断 | `0xE09/0xE0C/0xE18` 在厂商库"查不到" → 无法解字段 → 缺陷不可修 |
| 实际情况 | **该判断是 grep 错误**。三个寄存器在厂商库里都有完整定义 |
| 真正缺陷 | 开源驱动的用法与厂商语义**基本正确**，问题在**取值与结果读取路径** |
| 可修性 | **可修**（有明确的候选修复点，需上机验证） |

## 1. 三个寄存器的真实身份

命令号 → 地址：`addr = cmd << 2`（由 `jmo_HARDWARE_LoadState32` 入口 `lsr w1,w1,#2` 证明）

| 命令号 | 地址 | 厂商寄存器名 | 位域 |
|---|---|---|---|
| `0xE09` | `0x03824` | `MWV207REG_OCCLUSION_ADDRESS` | `ADDRESS 30:0`(U31)、`TYPE 31:31`(U01) |
| `0xE0C` | `0x03830` | `MWV207REG_OCCLUSION_DATA` | `COUNTER 31:0`(U32) |
| `0xE18` | `0x03860` | `MWV207REG_OCCLUSION_CONTROL` | `MODE 2:0`(U03) |

`OCCLUSION_CONTROL.MODE` 的 8 个取值：

| 值 | 名称 | 含义 |
|---|---|---|
| `0x0` | `COUNT` | 计数模式 |
| `0x1` | `BOOLEAN` | 布尔模式 |
| `0x2` | `BOOLEAN_CONDITIONAL` | 布尔 + 条件渲染 |
| `0x3` | `COUNT_EARLY` | 计数 + early-Z |
| `0x4` | `BOOLEAN_EARLY` | 布尔 + early-Z |
| `0x5` | `BOOLEAN_CONDITIONAL_EARLY` | 布尔 + 条件 + early-Z |
| `0x6` | `RENDER` | 渲染 |
| `0x7` | `RENDER_IN_RA` | 渲染（RA 内） |

`OCCLUSION_ADDRESS.TYPE`：`0x0 SYSTEM`、`0x1 VIRTUAL_SYSTEM`。
`ReadMask`/`WriteMask` = `0xFFFFFFFC`（低 2 位只读，符合 4 字节对齐地址）。

## 2. 开源驱动的实现（逐行）

`src/gallium/drivers/mwv207/mwv207_query.c`

```c
/* begin — mwv207_query.c:87-101 */
case PIPE_QUERY_OCCLUSION_COUNTER:
case PIPE_QUERY_OCCLUSION_PREDICATE:
case PIPE_QUERY_OCCLUSION_PREDICATE_CONSERVATIVE:
case PIPE_QUERY_SO_OVERFLOW_PREDICATE:
case PIPE_QUERY_SO_OVERFLOW_ANY_PREDICATE:
   if (only_depth) {
      MWV207_EMIT(cmd_buf, 0xE18, (ALIGN_BIT(0x7, 0x0) & (0x7)));   /* MODE = RENDER_IN_RA */
   } else {
      MWV207_EMIT(cmd_buf, 0xE18, (ALIGN_BIT(0x6, 0x0) & (0x7)));   /* MODE = RENDER       */
   }
   mwv207_add_relocs(ctx, &cmd_buf, 0xE09,
                     query->bo, query->query_id * 64, 0x1);          /* 写入目标地址 */
   break;

/* end — mwv207_query.c:190-196 */
   MWV207_EMIT(cmd_buf, 0xE0C, 0x01df5e76);                         /* ← 可疑 */
   query->query_id++;
   break;

/* get — mwv207_query.c:246-251 */
   uint64_t *results = ctx->ws->bo_map(ctx->ws, query->bo);
   for (int i = 0; i <= query->query_id; i++)
      count += results[i];                                            /* ← 可疑 */
```

## 3. 闭源 HAL 的对照（关键证据）

反汇编 `jmo_HARDWARE_SetQuery`(0xdbc00, size 0x960)：

```
   mov  w2, #0x5e76
   movk w2, #0x1df, lsl #16      → w2 = 0x01df5e76
   mov  w1, #0x3830              → OCCLUSION_DATA
   ...
   mov  w1, #0x3824              → OCCLUSION_ADDRESS
   mov  w1, #0x3860              → OCCLUSION_CONTROL
```

**闭源栈也把 `0x01df5e76` 写进 `0x3830`（OCCLUSION_DATA）**。

而 `OCCLUSION_DATA` 的位域是 `COUNTER 31:0`（一个 32 位计数器）、
`WriteMask = 0xFFFFFFFF`。**把一个常数写进"计数器"寄存器**，
最自然的解释是：这是 **"清零/复位计数器" 的伪值**（类似往计数器写任意值触发
latch/clear 的硬件惯例），而不是要写入的计数值。

所以开源驱动这行：

```c
MWV207_EMIT(cmd_buf, 0xE0C, 0x01df5e76);   /* end_query */
```

**与闭源栈逐位相同，是正确的** —— 它不是 bug 所在。

## 4. 真正的可疑点（两处）

### 4.1 `get_query_result` 的求和循环（`:250`）

```c
for (int i = 0; i <= query->query_id; i++)
   count += results[i];
```

`query_id` 在 `end_query` 里 **已经自增**（`:196` `query_id++`），
循环却用 `<=`，**多读一个槽**。而 BO 里每个查询占 64 字节（`query_id * 64`），
`results` 是 `uint64_t*`，即每槽 8 个 u64。这里：
- 按 8 字节步进读（`results[i]`），但**槽间距是 64 字节** → 索引错误；
- `<=` 边界多读一次。

而实测结果正是错得离谱：

| 轮次 | `GL_SAMPLES_PASSED` 实测 | 期望 |
|---|---|---|
| 0 | `0` | ≈ 4096 |
| 1–4 | `4294967295` (`0xFFFFFFFF`) | ≈ 4096 |
| `GL_ANY_SAMPLES_PASSED` | `0` | `1` |

`0xFFFFFFFF` 正是 32 位全 1，与"读错地址 / 读到未初始化的 BO 内容"高度吻合。

### 4.2 `PIPE_QUERY_OCCLUSION_PREDICATE` 的 MODE 选择

`begin_query` 用 `only_depth ? 0x7 : 0x6`（`RENDER_IN_RA` / `RENDER`），
但**没有区分** COUNTER 与 PREDICATE。按厂商枚举：
- 计数应用 `COUNT(0x0)` / `COUNT_EARLY(0x3)`
- 布尔（PREDICATE）应用 `BOOLEAN(0x1)` / `BOOLEAN_EARLY(0x4)`

全部用 `RENDER`/`RENDER_IN_RA` 可能是 `GL_ANY_SAMPLES_PASSED` 恒 0 的原因。

## 5. 候选修复（未实施，需上机验证）

```c
/* 1) 求和循环：按 64 字节槽步进，且用 < 而非 <= */
for (int i = 0; i < query->query_id; i++)
   count += results[i * 8];      /* 每槽 64 字节 = 8 个 uint64 */

/* 2) MODE 按查询类型区分 */
uint32_t mode = (query->type == PIPE_QUERY_OCCLUSION_COUNTER)
                ? 0x0    /* COUNT      */
                : 0x1;   /* BOOLEAN    */
MWV207_EMIT(cmd_buf, 0xE18, mode);
```

⚠ 修复 1 的槽步进与修复 2 的 MODE 值**均为推断**，依据是厂商 `MODE` 枚举与
`OCCLUSION_ADDRESS` 的 64 字节槽间距（`query->query_id * 64`）。
上机前无法确认硬件是否真按 64 字节槽写回，必须先做一个
**dump BO 原始内容的探针**验证（见 §6）。

## 6. 验证方法（版本握手陷阱）

源码树 HEAD 已是 `5499360`/`ba86da9`，而系统装的是 payload-24（`6cb20bd761`）。
直接用 `LIBGL_DRIVERS_PATH` 指向 build-full 会失败：

```
DRI driver not from this Mesa build ('24.1.2 (git-54993602f4)' vs '24.1.2 (git-6cb20bd761)')
failed to load driver: mwv207
X Error of failed request: BadValue ... (X_GLXCreateContext)
```

两条可行路：
1. `git worktree add` 检出 `6cb20bd`，在工作树里插 instrumentation（版本号即匹配）；
2. 用 `tools/make-payload.sh` 产出完整 payload 并 `switch-mwv207.sh umd install`，
   让 libGL 与 DRI 版本一致。

推荐 ①（不触碰系统安装）。

## 7. 与条件渲染缺陷的关系

`PIPE_CAP_CONDITIONAL_RENDER=1` 但 `pctx->render_condition` 从未赋值 →
`glBeginConditionalRender` 直接 SIGSEGV（`cso_context.c:965` 解引用 NULL）。

注意 `OCCLUSION_CONTROL.MODE` 里有 `BOOLEAN_CONDITIONAL(0x2)` 与
`BOOLEAN_CONDITIONAL_EARLY(0x5)` —— **硬件支持条件渲染**，只是开源驱动
没有接上 `render_condition` 钩子。因此长期方案可以是**实现该钩子**
（用 `BOOLEAN_CONDITIONAL` 模式），而不是简单地把 cap 改成 0。
