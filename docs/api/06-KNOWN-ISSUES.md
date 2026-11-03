# 06 · 已知问题与观察（含本树复核结果）

本文件汇总：(A) 既有分析报告中的缺陷清单在本 checkout 的**复核状态**；
(B) `01-WINSYS-KMD.md` 作者新发现的 O1–O12；(C) 本轮能力开发中定位并修复/仍未解决的问题。
所有条目均给 `文件:行号`；标注【已复核】的表示本次直接读源码确认。

---

## A. 既有缺陷清单复核

来源：`reports/ANALYSIS-REPORT.md`（原始描述针对另一棵树）。

### UMD winsys W1–W12

| 编号 | 原描述 | 本树状态 | 依据 |
|---|---|---|---|
| W1 | CTX 创建 ioctl 失败只 printf | 已修复 | `mwv207_drm_winsys.c` |
| W2 | `fcntl(F_DUPFD_CLOEXEC)` 未检查 | 已修复 | `mwv207_drm_winsys.c` |
| W3 | screen 为 NULL 仍缓存进 fd 哈希 | 已修复 | `mwv207_drm_winsys.c` |
| W4 | 哈希表键不一致（插入 vs 查询传址） | 已修复 | 见 B 节 O1 的键语义说明 |
| W5 | `co->name` 笔误 | 已修复 | `mwv207_drm_bo.c` |
| **W6** | **`CALLOC` 后无 NULL 检查** | **仍存在** | `mwv207_drm_bo.c:382-383` 分配、`:405-406` 解引用 |
| **W7** | **`GEM_MMAP` ioctl 失败仅 printf 后继续 mmap** | **仍存在【已复核】** | `mwv207_drm_bo.c:283-284` `printf` 后未返回，继续用可能为 0 的 `args.out.offset` 调 `os_mmap()` |
| W8 | `add_bo` 把访问标志写进共享 `bo->flags` | 已修复 | `mwv207_drm_cs.c` |
| W9 | 两次 `realloc` 覆盖原指针（泄漏+悬空） | 已修复 | `mwv207_drm_cs.c` |
| W10 | `add_dat` 扩容固定 `*2` | 已修复 | `mwv207_drm_cs.c` |
| W11 | `fence_wait` ns→ms 除后存入 int，INFINITE 溢出 | 已修复【已复核】 | `mwv207_drm_cs.c:88-104`：`OS_TIMEOUT_INFINITE` → `-1`；其余钳到 `INT_MAX` |
| **W12** | **`cmdbuf_create` 依赖 `assert`** | **仍存在** | `mwv207_drm_cs.c:125-147` |

### KMD K1–K12

K1/K2/K3/K4/K5/K6/K7/K10 已修复；**K11 仍在**（`mwv207_drv.c:445` 的 `pci_clear_master` 位于移除流程最后）。
K8/K9/K12 引自原报告，本树未复核（KMD 的 `irq.c`/`sched.c`/`ttm.c` 本轮未逐行读；另注：系统实际加载的
6.6 版 `mwv207.ko` 源码不在本仓库）。

---

## B. 新观察 O1–O12（来自 `01-WINSYS-KMD.md`）

> 本节为 winsys/drm 层作者在逐行通读中新记录的问题。下面给出其中最需关注的 5 条。

| 编号 | 位置 | 问题 | 复核 |
|---|---|---|---|
| O2 | `mwv207_drm_bo.c:276-277` vs `:291-293` | `mwv207_drm_bo_map()` 有**两种返回语义**：`bo->logical` 已存在时返回 `bo->logical`（= `ptr + bo->offset`）；首次调用返回**不含** `bo->offset` 的 `ptr`。子分配（slab）BO 会因此指错地址 | 【已复核】代码确认 → **已修复 2026-10-30**：`return ptr;` 改为 `return bo->logical;`（commit 5499360） |
| O3 | `mwv207_drm_cs.c:270` | `add_dat` 拼接 dat 时**硬编码 `dest_dat->size += 0x100`**，而非用 `src_dat->size`。只有当每个 `struct cmd_dat` 恰好 256 字节时才自洽 | 【已复核】`struct cmd_dat { uint32_t data[64]; … }`（`mwv207_drm_winsys.h:144-151`）确为 256 字节；当前唯一调用点是 `mwv207_sampler.c:578/598` 传 `&tex_desc->dat`（完整结构体），故**当前不会出错**，但属脆弱假设；后果见 G.2 → **已修复 2026-10-30**：拼接前 `memset(dest_dat->data + dest_dat->size, 0, 0x100)`，memcpy 收敛为 `MIN2(src_dat->size, 0x100u)`（commit 5499360） |
| O4 | `mwv207_drm_cs.c` commit 的 `-EBUSY` 重试 | 无上限 busy-loop；若内核已 `fd_install` 出 fence fd 再失败，重试路径可能**泄漏 fd**（`mwv207_submit.c:591-599` 失败路径不回收） | 未复核（需读 KMD submit.c） |
| O5 | `mwv207_drm_cs.c:89-101` | `fence->fd < 0` 时 `fence_wait()` **恒返回 true** → 提交失败被上层静默当作"已完成" | 【已复核】代码确认：`if (fence->fd >= 0) { … } return true;` |
| O1/O6–O12 | 见 `01-WINSYS-KMD.md` §13 | ioctl 号实测、engine_id 恒 0、in/out fence 共用 `fence_fd`、`DRM_AUTH\|DRM_RENDER_ALLOW` 门控等 | 见原文 |

`O5` 的实际影响面取决于 `pipe_context::flush` 之后上层是否依赖 fence 语义，
建议在 `02-CORE-OBJECTS.md`/`03-DRAW-PIPELINE.md` 的 fence 章节交叉阅读。

---

## C. 本轮能力开发中定位的问题

### C1 纹理上传路径破坏 16 位多通道数据（已修复）

- 机制：`mwv207_transfer_unmap()` 把 CPU 写好的 staging 资源用 **2D BLT**
  （`mwv207_blit_resource_box()`）拷进真实资源，而该 BLT 对这些 16 位布局
  不能保持字节精确（实测 `00 40 00 c0` 被读成 `00 c3 00 c3`）。
- 修复：`mwv207_tile_supported_format()` 对 6 个 16 位格式返回 `false`，强制 LINEAR，
  走逐字节精确的 CPU 路径。
- 报告：`reports/ES3-FORMATS-LINEAR-REPORT.md`。
- **遗留**：数据已字节精确，但 `R16G16_UNORM` / `R16_SNORM` / `R16G16_SNORM`
  采样仍恒为 0；6 位采样器格式码 `0x00–0x36` 全扫描无一组能同时读通两个通道
  （厂商文档中的 RG16 码 `0x34/0x35` 读出全 0）。属**信息边界**，非驱动笔误。

### C2 高级混合方程只实现 8/15（已确认硬件限制）

- `mwv207_blend.c` 的 `alpha_mode_ext[]` 只映射 8 个方程；用 `0x9–0xF`
  探测其余 7 个（COLORDODGE/COLORBURN/SOFTLIGHT/HSL_*）时硬件把帧缓冲清零，
  说明**混合单元只实现 8 个扩展模式**，是真实硬件缺口。
- 代价：`PIPE_CAP_BLEND_EQUATION_ADVANCED = 1`（解锁 GLES 3.2 所需）对那 7 个方程
  属**超额声明**——它们会静默地"不混合"。已如实写入注释与
  `reports/GLES32-ROBUSTNESS-BLEND-REPORT.md`。

### C3 `ARB_ES3_compatibility` 建立在部分失真的格式声明上（待决策）

- `ARB_ES3_compatibility` 要求 6 个格式可采样（用于 ETC2 模拟），驱动现已全部声明，
  但其中 `R16G16_UNORM`/`R16_SNORM`/`R16G16_SNORM` 实际采样恒为 0（见 C1 遗留）。
- 影响：使用 ETC2/EAC 的应用会**渲染错误**（不会崩溃）。
- 三个选项（A 保留声明并注明 / B 回退到只保留 R16_UNORM / C 继续追查），
  目前**尚未由用户决定**。

### C4 遮挡查询结果失真（已修复 2026-10-31）

- **现象**：`GL_SAMPLES_PASSED` 第 0 轮恒 `0`、之后恒 `0xFFFFFFFF`（应约等于被覆盖像素数）；
  `GL_ANY_SAMPLES_PASSED` 恒 `0`。22.3.7 原栈同症状 → **非本轮引入**。
- **根因**：`mwv207_get_query_result()` 用 `uint64_t *results` 按 **8 字节**步进累加
  （`for (i = 0; i <= query->query_id; i++) count += results[i];`），
  而硬件实际布局是**每槽 64 字节、槽内前 `cluster_count` 个 u32 才是各簇计数**
  （BO 为 `bo_create(64*100)`，`ws->cluster_mask=0xf` → `cluster_count=4`，
  4×1024=4096 与 64×64 全屏吻合）。原循环既步进错又用 `<=` 越界。
- **修复**（`mwv207_query.c:246-261`，commit `b2ab2c9`）：

  ```c
  uint32_t *w = (uint32_t *)results;
  for (int i = 0; i < query->query_id; i++) {
     uint32_t *slot = w + i * 16;              /* 每槽 64 字节 = 16 个 u32 */
     for (int c = 0; c < ctx->ws->cluster_count; c++)
        count += slot[c];
  }
  ```

- **破译依据**：反编译闭源 `libdrm_jmgpu.so` 的 `jmo_HARDWARE_SetQuery`(0xdbc00, size 0x960)
  得到寄存器常量 `0x3824 / 0x3830 / 0x3860`，正是开源 `0xE09 / 0xE0C / 0xE18` 的 `<<2`，
  对应厂商 `MWV207REG_OCCLUSION_ADDRESS / _DATA / _CONTROL`（`MODE` 取值含 `COUNT 0x0`、
  `BOOLEAN 0x1`、`RENDER 0x6`…）。详见 `ref/reverse-source/docs/05-OCCLUSION-QUERY.md`。
  ⚠ 此前判"厂商库查不到"是 **grep 假象**：厂商头地址是**大写 5 位** `0x03824`，
  早先按小写 4 位 `0x3824` 检索故零命中。
- **实测**：`GL_SAMPLES_PASSED=4096`、`GL_ANY_SAMPLES_PASSED=1`，且计数随几何面积单调
  （1/4 屏 120 < 半屏 496 < 全屏 2016）→ 证明硬件真在计数，非巧合值。

### C5 条件渲染 SIGSEGV（已彻底修复 2026-11-02：实现钩子）

- **现象**：`glBeginConditionalRender()` 段错误，gdb 显示
  `#0 0x0000000000000000 in ?? ()` / `#1 cso_set_render_condition (...)` at
  `../src/gallium/auxiliary/cso_cache/cso_context.c:965`，`pc 0x0`。
- **根因**：`mwv207_screen.c` 声明 `PIPE_CAP_CONDITIONAL_RENDER = 1`，
  但驱动**从未赋值** `pipe_context::render_condition`（全树 grep **0 处**）；
  调用链上 `src/mesa/main/condrender.c:44-88` 的 `BeginConditionalRender()` 无条件调用
  `cso_set_render_condition()`，后者直接 `pipe->render_condition(...)` → 解引用 NULL。
  `st->has_conditional_render`（`st_context.c:624`）**只**在 `src/mesa/main/blit.c:478`
  用作守卫，不保护 GL 入口。对照：llvmpipe / softpipe 都实现了该钩子
  （`lp_context.c:234`、`sp_context.c:233`）。
- **第一次修法（2026-10-31，cap 回退，已废弃）**：把 cap 改 `return 0` 止住崩溃，
  但**连锁把桌面 GL 从 4.0 砍到 2.1** —— `src/mesa/main/version.c:260-289` 的
  桌面 `ver_3_0` 门控里点名要 `NV_conditional_render`，而该扩展只由
  `st_extensions.c:860` 的 `{ o(NV_conditional_render), PIPE_CAP_CONDITIONAL_RENDER }`
  决定。实测 `Max core profile version: 0.0` / `OpenGL version string: 2.1` /
  `GLSL 1.20`。**教训：cap 回退不是"免费"的，它会沿扩展依赖链回噬版本号。**
- **最终修法（2026-11-02，patch `0015`）**：真正实现钩子，CPU 侧判断：
  - `mwv207_query.c` 新增 `mwv207_render_condition()`（记录 query/cond/mode 并清缓存）
    与 `mwv207_check_render_cond()`（首次调用时 `get_query_result()` 读回结果，
    `pass = ok && result.u64 != 0`，按 `cond` 决定是否取反，之后缓存在上下文里）；
    `mwv207_context_query_init()` 里注册 `pctx->render_condition = mwv207_render_condition;`。
  - `mwv207_draw.c` 的 `mwv207_draw_vbo()` 开头 `if (!mwv207_check_render_cond(pctx)) return;`；
    `mwv207_blit.c` 的 `mwv207_clear_render_target()` / `mwv207_clear_depth_stencil()`
    在 `render_condition_enabled` 时同样提前返回。
  - `mwv207_screen.c`：`PIPE_CAP_CONDITIONAL_RENDER` 恢复 `return 1`，
    并新增 `case PIPE_CAP_CONDITIONAL_RENDER_INVERTED: return 1;`（反相谓词直接由
    `render_cond_cond` 分支支持）。
  - 为什么不用硬件的 `BOOLEAN_CONDITIONAL`：要把谓词 BO 地址喂进 `0xE0C` 并让命令流
    条件跳转，改动面大且难自证；CPU 侧判断与 llvmpipe
    （`lp_query.c:545 llvmpipe_check_render_cond`）/ softpipe 同构，语义合法
    —— `GL_QUERY_*_NO_WAIT` 只表示实现**允许**不等待，实现选择等待是允许的。
- **验证**（`tests/query/condrender-correct.c`，GLX 桌面 GL，遮挡物画红、
  受约束绘制画绿，读中心像素绿通道判断是否真的画上去）：5/5 PASS ——
  `WAIT 谓词成立→绘制` G=255、`WAIT 谓词不成立→丢弃` G=0、
  `INVERTED 谓词不成立→绘制` G=255、`INVERTED 谓词成立→丢弃` G=0、
  `NO_WAIT 谓词不成立→丢弃` G=0；且 `Max core profile version` 回到 **4.0**、
  `GLES 3.2` 与 `Video memory: 1024MB` 均未受影响。
- **硬件仍支持**：`MWV207REG_OCCLUSION_CONTROL.MODE` 有
  `BOOLEAN_CONDITIONAL 0x2` 与 `BOOLEAN_CONDITIONAL_EARLY 0x5`，
  将来若把谓词下推到命令流，CPU 侧这套逻辑可以整体删掉。

### C4/C5 的 A/B 对照（`tests/query/run-query-tests.sh`）

| 检查项 | 修复栈（`/tmp/wt-build`） | 已安装栈（payload-24） |
|---|---|---|
| `GL_SAMPLES_PASSED` | **4096** ✅ | 0 ❌ |
| `GL_ANY_SAMPLES_PASSED` | **1** ✅ | 0 ❌ |
| 面积单调（1/4 < 半 < 全） | 120 < 496 < 2016 ✅ | 0 / 496 / 4294967295 ❌ |
| condrender 退出码 | **0** ✅ | 139 (SIGSEGV) ❌ |

> 注：worktree 产物 `libmwv207_dri.so` 的版本戳为 `24.1.2 (git-6cb20bd761)`，
> 与系统 `/usr/local/lib/mwv207-oss/libGL.so.1`、`libEGL.so.1` **完全一致**，
> 故修复栈一列是走真实系统 X/GLX 栈的端到端验证，不是孤立的 build 目录测试。

---

## D. 与寄存器文档的衔接

- `W7` 涉及 `args.out.offset`，其字段定义见 `01-WINSYS-KMD.md` §9；
- C1/C2 涉及的寄存器（`PE_ALPHA_CONFIG`、`BLT_*`、`TX_EXT_CONFIG_FORMAT`）见
  `registers/REGISTERS-FIELDS.md` 与 `registers/REGISTER-ARRAYS.md`。

---

## E. 绘制管线通读中记录的 10 条可疑点

来源：`03-DRAW-PIPELINE.md` §13.3（**未修复，仅记录**；本节只做索引，细节见原文）。

| # | 位置 | 可疑点 |
|---|---|---|
| E1 | `mwv207_draw.c:751-754` | `color_8` 四个分量都做 `<< 8`，高位互相覆盖 |
| E2 | `mwv207_draw.c:888,907` | `draw_count` 是函数内 `static` → **跨 context 共享**状态 |
| E3 | `mwv207_blit.c:568-584` | 压缩格式分支里算好的 `task_info` 被随后无条件赋值覆盖 |
| E4 | `mwv207_draw.c:1012` | 无深度模板时解引用 `FS shader_states` 未判空 |
| E5 | `mwv207_state.c:326-331` | `stencil_back_reference` 与正面索引交叉（背面取到正面值） |
| E6 | `mwv207_query.c:108-112` | `TIMESTAMP` 查询用 CPU `os_time_get_nano()`，不是 GPU 时间戳 |
| E7 | `mwv207_query.c:156-163,279-282` | `PIPELINE_STATISTICS` 与 `SO_STATISTICS`：begin/end 不发射寄存器，`get_result` 走 `unreachable()` → 实际不可用 |
| E8 | `mwv207_state.c:335-354` | 视口/裁剪状态只保留单份（`mwv207_set_viewport_states` 忽略 `num_viewports`） |
| E9 | `include/mwv207_state.h:143` | `mwv207_context_zsa_init` 只有声明，无定义、无调用 |
| E10 | `mwv207_query.c:342` / `include/mwv207_query.h:53` | `destory` 拼写错误（函数名） |

> E2/E6/E7/E8 会直接影响可观测行为（跨上下文污染、时间戳不可信、查询不可用、多视口失效），
> 优先级高于纯拼写类问题。这些均**未经硬件验证**，仅由代码阅读得出。
>
> **2026-11-03 状态**：E2/E4/E5/E9/E10 已修（`841e300` / `5dee412`）；E1 复核为 1 LSB 差异（不改）；
> E6 复核不改（与 llvmpipe 同为 CPU 时钟语义）；E8 复核不改（`PIPE_CAP_MAX_VIEWPORTS=1`，
> `ARB_viewport_array` 未暴露）；E3 复核不改（分支不可达，ETC2 拷贝实测正确）。E7 仍余"无真实硬件计数"。

### 附：命名/拼写类历史遗留（保留原样以便检索）

`cmd_dat_arrary`（`mwv207_drm_winsys.h:153`）、`MWV207_IMMEDIATE_GLOBAL_MEMEORY_OFFSET`、
`CACHE_TEXUTRE_L1`、`set_active_query_state` 中的 `destory`。

---

## F. NIR 编译器通读中记录的问题

来源：`04-NIR-COMPILER.md`（**未修复，仅记录**）。

### F.1 硬件 ISA 常量头缺失（信息边界，非缺陷）

`INST_OPCODE_*` / `INST_TYPE_*` / `INST_EXT_OPCODE_*` 在本开源树**无定义**
（`include/hw/*.h` 全是空壳，见 `02-CORE-OBJECTS.md`）。

- 所有引用点都落在**从未被调用的宏体**内：`nir/mwv207_opcode.c:42-63`（15 个）、
  `nir/mwv207_disasm.c:1022-1052`（31 个）——所以不影响编译，也不影响运行。
- 后果：`mwv207_ops[]` 的 **101 条 opcode 数值**只能靠反汇编名称表互证，
  **没有厂商文档依据**。这也是 `disasm` 表能反过来验证 `emit` 表的唯一手段。

### F.2 `mwv207_cmdline.c` 不是调试开关

`nir/mwv207_cmdline.c`（170 行）是**独立可执行文件 `mwv207_compiler` 的 main**，
只支持 `compile` 子命令（`disasm` 分支是 `unreachable()`）。

关键警告：它自带的 NIR 选项集**与运行时不一致**（缺 `fuse_ffma*`、`lower_fdiv`、
`lower_uniforms_to_ubo`）→ **离线编译的产物不能直接等同于运行时产物**。

真正的调试开关是环境变量：`mwv207_debug`（位掩码，`nir=0x1`、`nir_all=0x201`
编译后打印 NIR、`tgsi=0x2`、`spill=0x100` 无对应宏）、`OPT_MOV:true`、
`OPT_IF_BAIL:true`、`OPT_DIV/OPT_FFMA/OPT_SET:false`、`mwv207_count`、`MWV207_LOG_LEVEL`。

### F.3 反汇编器缺陷（疑似）

| # | 位置 | 问题 |
|---|---|---|
| F.3.1 | `nir/mwv207_disasm.c:141-158` vs `nir/mwv207_emit.c:205-232` | `_str_inst_type[]` 与 `get_alu_inst_type()`/`ops[].type` 的 type 编码**互相矛盾**（1 到底是 `.f16` 还是 `i32`）→ `type != 0` 时打印的类型名基本全错 |
| F.3.2 | `nir/mwv207_disasm.c` | `mwv207_disasm()` 断言 `sizedwords % 2 == 0`，但实际步长为 **4** |
| F.3.3 | `nir/mwv207_disasm.c` | FS color 输出打印遇到 0 时截断 |

> 影响面：仅影响**反汇编输出可读性**（调试/验证工具），不影响编译与渲染。
> 但因为 F.1 的存在，反汇编表是 opcode 数值的唯一交叉验证来源，所以修它有意义。
>
> **2026-11-03 状态**：F.3.1 已修（`5dee412`：按汇编器语义 `type = (bit0 << 2) | bit1_2`），
> F.3.2/F.3.3 已修（`841e300`）。本节保留修复前的判断依据作历史记录。

### F.4 其他 7 条待确认（详见 `04-NIR-COMPILER.md` §12）

`link_shader` 恒 `return false`；`bo_size = code_size * 4 * 16` 与
`bo_offset += code_size * 4 * 2` 的倍数关系无注释；`shared_atomic` 形状匹配失败即
`assert(0)`；三个窥孔（`OPT_DIV`/`OPT_FFMA`/`OPT_SET`）默认全关；`spill` 位无宏；
离线/运行时 NIR 选项不一致；`reg_get_base` 各 stage 固定偏移无依据、RA 溢出循环无
迭代上限、变体命中只查 4 个 dirty 位、dual blend 采样槽硬编码 4、
`get_ssbo_size` 常量描述符是否漏更新、key 位域宽度、liveness 的 `~0u` 处理。

> **2026-11-03 状态**：`link_shader` 恒 `return false`、`shared_atomic` 失败即
> `assert(0)`、RA 溢出循环无上限这三条已修（`5dee412`）；其余条目继续在
> [UNRESOLVED.md §1.4 F.4](../UNRESOLVED.md) 跟踪。

---

## G. 两处结论修正（本轮交叉验证得出）

### G.1 ISA 扩展操作码**有**厂商依据（修正 `04-NIR-COMPILER.md` §12）

`04-NIR-COMPILER.md` 记录"101 条 opcode 数值无厂商文档依据"。交叉验证发现：
厂商头文件里存在 `MWV207REG_SH_EXTENDED_OPCODE`（U08，bits 0–7，**16 个命名取值**）、
`SH_CMPLX_OPCODE`（U04，MUL/MAD/ADD）、`SH_IMG_ATOM_OPCODE`（U03，8 个原子操作）、
`SH_VISION_OPCODE`（U06，32 个视觉操作）。

驱动反汇编表把扩展指令编在 `0x7f + n`，其 `n` **就是** `SH_EXTENDED_OPCODE` 的取值，
逐项一致（`0x4 → LODQ`、`0xF → HALFMIX`、`0xB → BIT_FINDLSB`、`0x13 → MOVBIX` …）。

- 结论：**扩展操作码部分应视为有厂商依据**；基础 ALU 操作码仍未找到厂商枚举。
- 完整对照表见 [../registers/ISA-OPCODES.md](../registers/ISA-OPCODES.md)。
- 缺口：厂商定义但驱动未实现/未解码的有 4 个 —— `WAIT_GROUP_EVENT`(0x11)、
  `TD_OP`(0x12)、`TEX_INVALIDATE`(0x14)、`PRE_NORM`(0x15)。

### G.2 O3 后果修正：不是"当前无害"，而是**每槽尾部 0x64 字节未初始化并发往 GPU**

`07-WINSYS-AUDIT.md` 的结论（修正本节 B 部分原先的"当前不会出错"）：

- `sizeof(struct mwv207_texture_config)` **实测 = 156 = 0x9C**（不是 0x100）。
- 槽步进、reloc 记账、返回值三处统一用 0x100（`mwv207_drm_cs.c:262/267/270`）
  → **布局不错位、不越界**；但也**从不写入** `[0x9C, 0x100)` 这 100 字节。
- 该区间保留 MALLOC/realloc 的未初始化堆内容，跨帧只复位 `size`（`:438`），
  经 KMD 逐字节读走写进 dat BO（`submit.c:262`、`:485-508`）→ **GPU 可见的未初始化数据**。
- 触发条件极低：**任何一次带纹理采样的 draw**。
- 最小修复：`mwv207_drm_cs.c:267` 之前加一行
  `memset(dest_dat->data + dest_dat->size, 0, 0x100);`（不动槽宽与记账）。
  **不要**把 `:268` 改成 `+= src_dat->size`（会破坏 0x100 槽宽假设）。

### G.3 O2 判定：移植引入的"哑弹"，当前不可触发

- `MWV207_FLAG_SUBALLOC` 全树仅 5 处命中，**没有任何地方**把它写进
  `pipe_resource.flags` → 整条 slab 路径是**死代码**。
- 即使将来启用：`size <= 4096` 才触发，且 `PIPE_BIND_SHARED` 会把 flag 覆盖为
  2D_ACCESS；slab entry 创建时已预置 `logical`，会命中 `:276` 早返回。
- 对照厂商 22.3.7 树：其 `bo_map` 恒 `return bo->logical`，且同样无人写 SUBALLOC
  → **O2 是 24.1.2 移植引入的分歧**，非厂商本意。
- 最小修复：`mwv207_drm_bo.c:293` 的 `return ptr;` 改成 `return bo->logical;`。

> G.2/G.3 均为**静态审计结论**，未做运行态验证；修复建议未落地到代码。

---

## H. KMD 层复核结论与新增发现（来源：`08-KMD.md`）

### H.0 树的前提更正

本树是 **5.10 基线 + 修复 commit（0ec4816）+ softdep（d94c106）**，`wc -l` 共 **10 260 行**
（不是任务书所称 2 万行）；**仓库内不存在 6.6 版源码**。因此"6.6 移植点"在本树
的实际写法就是**未移植**，仍用 `ttm_bo_device` / `ttm_bo_driver` / `ttm_dma_tt` /
`ttm_bo_init` / `fault_reserve_notify` / `verify_access` /
`gem_free_object_unlocked` / `drm_device.pdev` / `ttm_bo_mmap`。
6.6 替换项清单见 `reports/ANALYSIS-REPORT.md` §四。

### H.1 K 系列复核（在本树）

| 缺陷 | 结论 | 证据 |
|---|---|---|
| **K8** | **已修** | `irq.c:146-153` 已检查 `irq_create_mapping` 返回值并回退；`kms.c:172` 的 `BUG_ON(va_irq==0)` 由上游保证不触发 |
| **K8 残留** | 仍在 | `kms.c:172`、`hdmi.c:1057` 保留 `BUG_ON`（3D/2D 已是可回退范式）→ N9 |
| **K9** | **原机制不成立** | demux 清位是**逐位 W1C** `jdev_write(reg, 1UL<<j)`（`irq.c:44`），不会清掉读后新置位的 bit |
| **K9 等价风险** | 两处真实 | ① VA vblank 处理器**整字回写** `0x9907f4`（`va.c:634-655`，影响 bit12–15）；② `irq_enable_reg[]` 被 ISR 无锁读（`irq.c:31`）而 mask/unmask 在 `irq_lock` 内写 |
| **K11** | 仍在 | 与 `01-WINSYS-KMD.md` 一致；耦合解释：**`kms_fini` 必须先于 `irq_fini`** |
| **K12** | 仍在 | `sched.c:203-204` 循环体只有 `devfreq_record_idle`、循环变量未使用；`drm_sched_resubmit_jobs`（`:205`）前无 job 完整性校验 |

### H.2 新增发现 N1–N14（择要）

| # | 级别 | 位置 | 问题 |
|---|---|---|---|
| **N1** | **高** | `sched.c:310-314` | 指针算术使 `sched_dec[1]` 与 `sched_enc[0]` **同为 `&sched[3]`** → enc0 的 scheduler 覆盖 dec1，而 `INFO` 仍报 DEC=2；`engine_id=1` 的 DEC 提交会被 **ENC 调度器**执行（丢失 hang_karma 隔离）。当前 UMD `engine_id` 恒 0，故可能尚未触发 |
| **N2** | 中 | 3D 管线 | `hw_submission = 30` 与硬件事件数 30 **相等**，与源码注释 "must be lower than 30" 冲突 → 事件位图满时存在越界/`BUG_ON` 风险 |
| **N4** | 中 | `pipe_dma.c:177-186,366` | DMA 提交在 `run_job` 工作线程内忙等最多 **5 s** 并返回 **stub fence** → 调度器工作线程被长占、fence 语义退化 |
| **N14** | 中 | `pipe_3d.c:301-307`、`pipe_2d.c:212-227`、`pipe_dec.c:136-141` | 三条管线 `reset` 都只 `dma_fence_put` 在飞 fence **而不 signal**；2D 的 `destroy` 还不排空 `fence_queue`（`pipe_2d.c:424-448`）→ **用户态可能永久等待** |
| **N9** | 低 | `kms.c:172`、`hdmi.c:1057` | 仍保留 `BUG_ON`（K8 残留） |
| **N10** | 低 | HDMI | `mode_valid` 首个条件 `clock >= 384000` 先命中 → **384–594 MHz 全部被拒**；`phy_configure` 恒传 `bpp=8`，PHY 表 10/12 bpp 项不可达 |

### H.3 结构澄清（回答任务书疑问）

- **`mwv207_db.c` 不是寄存器读写接口**，也不是 debugfs/sysfs：它是纯内存
  key/value 库（2048 项）。**整个 KMD 无任何 debugfs / sysfs 节点**。
- `mwv207_dma.h` 的 `mwv207_dma_*` API 在本树**无实现文件**（死声明）；
  驱动实际使用的是 `pipe_dma`。
- KMD 侧 ioctl 实现细节（01 只列 ABI 字段，08 补实现）：`args->out.resv` 不回写、
  INFO 复用 `nr_recs` 作 key、6 个 ioctl 的 `copy_*_user` 与返回值语义。

---

## I. 视频编解码层的通读发现（来源：`05-VIDEO-CODEC.md`）

### I.1 两个新缺陷（本树确认，未修复）

| # | 位置 | 问题 | 影响 |
|---|---|---|---|
| **I-1** | `video-codec/mwv207/mwv207_dec_hevc.c:1060` | `pic_height` 误取 `bit_depth_chroma_minus8` 而非图像高 → 8 bit 时 `height64 = 0` → `tile_buffer_size = 0` | HEVC 8 bit 路径的 tile 缓冲为 0 字节，与 `bs_size-3` 下溢叠加，实际影响**未实测** |
| **I-2** | word7 bit30 复用 | `JMDEC_BITPLANE1_E` 与 `JMDEC_BLACKWHITE_E` **同占 word7 bit30**；H264 先写后者（`mwv207_dec_h264.c:587`），随后被前者清零（`mwv207_dec_h264.c:2083`）**覆盖** | 两开关互相踩，后写的生效 |

### I.2 同步机制的意外事实（需注意）

- **正常视频提交路径不发 out fence、也不同步等待**：`cs_submit` 只拼
  4 条硬件写命令（`0x08FF0003` 255 字 / `0x08F50102` 245 字 / `0x08010002` /
  `0x08010001`，覆盖 word 1..502），只有 `MWV207_DEBUG_VIDEO` 打开时才
  `fence_create/wait`（`video-codec/common/video_cs.c:118-129`）。
- 完成判定改由 **KMD 轮询**：对 `0x4` 以 1 ms 间隔查 `(stat & 0x1100) == 0x1100`
  （KMD `mwv207_pipe_dec.c`）。这与 3D 路径的 fence 语义不同，是解码路径
  独有的同步模型。

### I.3 能力声明与实现的偏差

- 能力表宣告 H264 含 **High422 / High444**，但解码器**硬置 `chroma_format_idc = 1`**
  （`video-codec/mwv207/mwv207_dec_h264.c:1184`）→ 实际只走 4:2:0。
  与 C2/C3 同类，属**过度声明**（不会崩，但色度格式与声明不符）。
- **编码完全未实现**：`base.encode_bitstream` 从未安装，全树 `encode` 相关只有
  入口排除 / 枚举 / 注释 3 处（见 `05-VIDEO-CODEC.md` §12）。
- **VP9 的准确表述**：切片解析器与 bool 解码器**已编入库但零调用者**，
  连 `.h` 都未导出原型；唯一缺口是 `mwv207_dec_vp9_set_regs()`
  （`video-codec/mwv207/mwv207_video_dec.c:180-193` 无 VP9 分支）。
  硬件侧 `mwv207_video_dec_vpu_features_list.h:35-36` 宣告 `vp9_support=1`。

### I.4 寄存器映射（解码块）

- 三张表 **`8170enum.h`(830) / `8170table.h`(1871) / `dec_register_array_7020.h`(889)
  共 1840 行严格按行号对齐**，逻辑名 → `{word, width, shift}`，已全量还原。
- **225 行 `word == 0` = 本芯片不存在的空操作寄存器**（含 `DEC_OUT_ENDIAN`、
  `DEC_DATA_DISC_E`、`DEC_LATENCY`），写入无害但无意义。
- **覆盖缺口**：厂商寄存器库 1392 条里**完全没有视频解码块**
  （`MWV207REG_DEC*` 只有 2D/3D 的 `DEC400`），因此 `word → 厂商名` **无法反查**，
  `REGISTER-MAP.md` 对解码块是无能为力的（这不是遗漏，是数据源本身没有）。

### I.5 未确认项

见 `05-VIDEO-CODEC.md` §13（16 条），要点：`rlc_mode` 恒 0 使所有逐行 H264 走
`DEC_MODE_H264_HIGH10P = 15` 是否符合硬件语义；`BUS_WIDTH_128 = 2` 的位宽语义；
隔行解码未实测；`Ref_idx_override`、`video_info.h`、`dump_referece_frames`、
`dump_register`、`USE_FAKE_RFC_TABLE` 为死代码；PPU rotation/dither 与 bit30 重叠。

---

## J. 本轮（2026-10-30）已修复项

复审后当场修掉的 3 个源码缺陷。均为几行内改动，已提交、已重建、已用硬件实测验证
（但**尚未部署**，见 `README.md` 的载荷说明）。

| 项 | 位置 | 修复 | 验证 |
|---|---|---|---|
| 显存上报放大 1024× | `mwv207_screen.c` `PIPE_CAP_VIDEO_MEMORY` | `return 1024 << 20;` → `return 1024;`（该 cap **单位是 MB**，原值让 `glxinfo -B` 报 1024 TiB） | `glxinfo -B` → `Video memory: 1024MB` |
| O2 | `mwv207_drm_bo.c` `mwv207_drm_bo_map()` 末尾 | `return ptr;` → `return bo->logical;`，与函数开头缓存分支语义统一 | `bo->offset` 恒 0，故当前不可触发，属潜伏缺陷 |
| O3 | `mwv207_drm_cs.c` `mwv207_add_dat()` | 拼接前 `memset(dest_dat->data + dest_dat->size, 0, 0x100)`；memcpy 长度收敛为 `MIN2(src_dat->size, 0x100u)`；`+= 0x100` 不变 | 不再把每槽尾部 100 字节未初始化内存发往 GPU |

- 提交：`cd90d9f`（GLES 3.2 + 鲁棒缓冲访问 + 16 位格式，6 文件）、`5499360`（上述 3 项，3 文件）。
- 回归：`tests/tri-test` → `HARDWARE TRIANGLE RENDER: PASS`；`tests/fbo-test` →
  `FBO status: 0x8cd5 COMPLETE` + `FBO HARDWARE RENDER: PASS`；`glxinfo` 仍为
  GL core 4.0 / **GLES 3.2 (git-6cb20bd761)**。
- 仍未修：`W6`/`W7`/`W12`、`O5`、`E1`–`E10`、`N1`/`N14`、`I-1`/`I-2` 等 —— 分级与
  批次见 `11-FIX-FEASIBILITY.md`。
