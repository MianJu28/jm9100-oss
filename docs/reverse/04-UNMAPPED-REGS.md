# 04 · 开源驱动 49 个"厂商库查不到"的命令号：闭源 HAL 实证

## 背景

开源驱动 `drivers/mwv207` 共使用 **219** 个命令寄存器号。其中 170 个可用
`地址 = 命令号 << 2` 在厂商公开寄存器库（1353 个寄存器）里解出名字与位域，
剩下 **49 个解不出**。

已排除的三种解释：

| 假设 | 结论 |
|---|---|
| 1.8.0 的头比 1.6.1 新，可能新增了这些 | ❌ 1.8.0（146 个 `.h`）与 1.6.1 的**地址集合完全相同**（1353，交集 1353，差集 0） |
| 命令号 <<2 映射不适用于这些 | ❌ 该映射由 `jmo_HARDWARE_LoadState32` 入口的 `lsr w1,w1,#2` 直接证明，对所有号一视同仁 |
| 开源驱动自己编造的号 | ❌ 闭源 HAL 里大量使用同一批号（下节） |

**真正的原因**：这 49 个寄存器**不在随包发布的公开头里**，但硬件与闭源栈确实在用。
因此只能靠"哪个 HAL 函数用了它"来推断语义。

## 方法

对闭源 `libdrm_jmgpu.so` 全量反汇编，扫描每条 `mov wN, #imm`，若 `imm` 落在
这 49 个号中，则记录**使用该号的函数名**。景美的 HAL 函数命名是自解释的
（`jmo_HARDWARE_FlushDepth` 显然与深度刷新有关），故函数名即语义线索。

⚠ **置信度声明**：下表"推测功能"是**推断**，不是厂商文档。判断依据是
①使用它的函数名，②开源驱动自身对该号的写入值是否与该功能自洽（已逐条核过）。
凡两处不能互相印证的，标为 未确认。

## 结果：49 个中 32 个有 HAL 实证

| 命令号 | HAL 引用数 | 推测功能 | 使用该号的 HAL 函数 |
|---|---|---|---|
| `0x191` | 3 | ProgramIndex | ProgramIndex, ProgramIndexEx, FastProgramIndex |
| `0x282` | 1 | FlushDepth | FlushDepth |
| `0x285` | 1 | FlushDepth | FlushDepth |
| `0x286` | 1 | FlushPA | FlushPA |
| `0x287` | 1 | FlushPA | FlushPA |
| `0x28A` | 5 | SURF_Blit | SURF_Blit, 2D_MonoBlit, 2D_SetMonochromeSource, 2D_SetTransparencyAdvanced, 2D_SetTransparencyAdvancedEx |
| `0x28C` | 1 | FlushShaders | FlushShaders |
| `0x28D` | 1 | FlushPA | FlushPA |
| `0x401` | 3 | WriteCounters | WriteCounters, 2D_Construct, PROFILER_probeInitialize |
| `0x402` | 3 | WriteCounters | WriteCounters, FlushShaders, PROFILER_probeInitialize |
| `0x403` | 3 | WriteCounters | WriteCounters, FlushPipe, PROFILER_probeInitialize |
| `0x404` | 3 | WriteCounters | WriteCounters, PROFILER_probeInitialize, FillFromTileStatus |
| `0x500` | 3 | SetTarget | SetTarget, FlushDepth, FastFlushDepthCompare |
| `0x501` | 1 | FlushDepth | FlushDepth |
| `0x503` | 1 | FlushDepth | FlushDepth |
| `0x505` | 3 | FlushDepth | FlushDepth, HzClearValueControl, FastFlushDepthCompare |
| `0x506` | 1 | FlushStencil | FlushStencil |
| `0x508` | 1 | FlushAlpha | FlushAlpha |
| `0x50B` | 1 | FlushTarget | FlushTarget |
| `0x50D` | 1 | FlushTarget | FlushTarget |
| `0x594` | 4 | FlushTileStatusCache | FlushTileStatusCache, ProgramResolve, 3DBlitTileFill, FillFromTileStatus |
| `0x595` | 5 | DisableTileStatus | DisableTileStatus, DisableTileStatusMRT, FlushAndDisableTileStatus, PauseTileStatus, EnableTileStatus |
| `0x596` | 5 | FlushTileStatusCache | FlushTileStatusCache, FlushAndDisableTileStatus, ResetTileStatusAddrs, EnableTileStatus, FillFromTileStatus |
| `0x598` | 1 | EnableTileStatus | EnableTileStatus |
| `0x599` | 1 | EnableTileStatus | EnableTileStatus |
| `0x59B` | 1 | EnableTileStatus | EnableTileStatus |
| `0xE00` | 29 | DoBlitOrClear | DoBlitOrClear, 2D_MonoBlitEx, Blit, DoMultiSourceBlit, 2D_FilterBlitEx |
| `0xE02` | 7 | Semaphore | Semaphore, 3DBlitBlt, MultiGPUSync, 2DMultiGPUSync, 3DBlitYUVTiler |
| `0xE03` | 19 | DrawRectangle | DrawRectangle, FlushTileStatusCache, FlushPipe, FlushDepth, DrawPattern |
| `0xE05` | 11 | DrawPattern | DrawPattern, DrawPrimitives, DrawPrimitivesEx, ResetBaseInstance, DrawPrimitivesCount |
| `0xF00` | 3 | WritePixelTo_X4R4G4B4 | WritePixelTo_X4R4G4B4, Semaphore, ClearSoftware |
| `0xFFFF` | 30 | CheckSurface | CheckSurface, ComputeClear, 2D_SetSource, WritePixelTo_D16, WritePixelTo_R16 |

### 与开源驱动自身用法的一致性核验（关键）

推断不能只看函数名，必须回到开源驱动看它**往这个号里写了什么**：

| 命令号 | 开源驱动的写入（`file:line`） | 与推断是否自洽 |
|---|---|---|
| `0x401` | `MWV207_EMIT(cmd_buf, 0x401, output_reg[0])` — `mwv207_shader.c:661` | ✅ 着色器输出寄存器个数 → `WriteCounters` |
| `0x402` | `num_varyings + 1 + msaa_extra_input`（`mwv207_shader.c:707`） | ✅ varying 计数 |
| `0x403` | `fs_input + msaa_extra_temp`（`mwv207_shader.c:670`） | ✅ FS 输入/临时寄存器计数 |
| `0x404` | 成对出现于 `mwv207_draw.c:421,424` | ✅ 同为计数类 |
| `0x594` | `MWV207_EMIT(cmd_buf, 0x594, ENABLE & 0x1)` — `mwv207_cmd.c:87,147` | ✅ tile status 使能位 |
| `0x595` | `memory_config`（`mwv207_draw.c:325,466`） | ✅ tile status 内存配置 |
| `0x596` | 带 `mwv207_add_relocs`（`mwv207_draw.c:311`） | ✅ tile status 地址（需重定位） |
| `0x505` | `surface->stride`（`mwv207_draw.c:473`） | ✅ 与 depth/target 尺寸相关 |
| `0x28A` | `0x1 | 0x10`（`mwv207_draw.c:511`） | ✅ 位掩码，2D blit 控制 |
| `0xE05` | `0x1`（`mwv207_cmd.c:53`） | ✅ 绘制使能 |
| `0x191` | 带 `mwv207_add_relocs`（`mwv207_draw.c:158,171`） | ✅ 程序索引（写地址） |

## 剩余 17 个：闭源栈也不使用

`NOTFOUND: `0x192 0x281 0x283 0x284 0x28b 0x301 0x302 0x303 0x304 0x305 0x306 0x502 0x507 0x509 0x50a 0x6d0 0xe13

这 17 个号在闭源 HAL 的 `mov` 立即数里**完全没有出现**，可能的解释：
① 该功能是开源驱动特有的写法；② 走字面量池而非常见的立即数编码；
③ 开源驱动里的**无效/遗留写入**。**均标为 未确认，不做断言。**

## 对"遮挡查询失真"缺陷的直接意义

缺陷定位时用到了三个号：`0xE09`（`OCCLUSION_ADDRESS`）、`0xE0C`（`OCCLUSION_DATA`）、
`0xE18`（`OCCLUSION_CONTROL`）。这三个**不在**本节的 49 个里 —— 它们此前被判为
"厂商库查不到"是**我自己的 grep 错误**（厂商头里写的是大写 5 位 `0x03824`，
我用小写 4 位 `0x3824` 检索）。纠正后三个全部命中，且位域完整：

- `0x03824 OCCLUSION_ADDRESS`：`ADDRESS 30:0`(U31)、`TYPE 31:31`(SYSTEM=0 / VIRTUAL_SYSTEM=1)
- `0x03830 OCCLUSION_DATA`：`COUNTER 31:0`(U32)
- `0x03860 OCCLUSION_CONTROL`：`MODE 2:0`(U03)，取值 `COUNT 0x0`、`BOOLEAN 0x1`、
  `BOOLEAN_CONDITIONAL 0x2`、`COUNT_EARLY 0x3`、`BOOLEAN_EARLY 0x4`、
  `BOOLEAN_CONDITIONAL_EARLY 0x5`、`RENDER 0x6`、`RENDER_IN_RA 0x7`

闭源 `jmo_HARDWARE_SetQuery`(0xdbc00) 写这三个号的方式与开源驱动**一致**
（`0x3830` 的值同为 `0x01df5e76`）。详见 `05-OCCLUSION-QUERY.md`。

