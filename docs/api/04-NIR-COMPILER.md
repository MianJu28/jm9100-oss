# 04 — MWV207 (景美 JM9100) NIR 编译器后端 API 文档

> 被测驱动：Mesa gallium 驱动 `mwv207`（景美 JM9100 / MWV207 GPU 的开源 UMD）。
> 源码根：`/home/admin/jm9100-oss/src/oh-mesa/User_source_code/mesa-24.1.2/`
> 本文范围：`src/gallium/drivers/mwv207/nir/` 全部文件，外加 shader state 接合层
> `src/gallium/drivers/mwv207/mwv207_shader.c` 与 `src/gallium/drivers/mwv207/include/mwv207_shader.h`。
>
> **所有结论均给出 `文件:行号` 依据。行号基于 mesa-24.1.2 快照的实际文件内容。**
> 读代码无法确定的点，统一在文中显式标注“**未确认**”。
>
> 寄存器参考：`/home/admin/jm9100-oss/docs/registers/REGISTERS.md`
> 约定：驱动命令流里的 16 位寄存器号 = 厂商寄存器 `_Address >> 2`。

---

## 目录

1. [范围与文件清单](#1-范围与文件清单)
2. [核心数据结构](#2-核心数据结构)
3. [编译管线全景](#3-编译管线全景)
4. [驱动自定义 lowering / 优化 pass 逐个说明](#4-驱动自定义-lowering--优化-pass-逐个说明)
5. [指令编码（mwv207_emit.c）](#5-指令编码mwv207_emitc)
6. [指令表（mwv207_opcode.c）](#6-指令表mwv207_opcodec)
7. [寄存器分配（mwv207_ra.c）](#7-寄存器分配mwv207_rac)
8. [活跃性分析（mwv207_liveness.c）](#8-活跃性分析mwv207_livenessc)
9. [反汇编器（mwv207_disasm.c 1317 行 / mwv207_disasm.h 328 行）](#9-反汇编器mwv207_disasmc-1317-行--mwv207_disasmh-328-行)
10. [Shader state 层（mwv207_shader.c 1198 行 + include/mwv207_shader.h 105 行）](#10-shader-state-层mwv207_shaderc-1198-行--includemwv207_shaderh-105-行)
11. [调试开关（环境变量 + 独立命令行工具）](#11-调试开关环境变量--独立命令行工具)
12. [未能确认之处汇总](#12-未能确认之处汇总)

---

## 1. 范围与文件清单

| 文件 | 行数 | 角色 |
|---|---|---|
| `nir/mwv207_compiler.h` | 194 | 编译器公共结构与入口声明（`mwv207_shader_key`、`mwv207_shader`、`mwv207_compile`） |
| `nir/mwv207_compiler.c` | 666 | 顶层编译入口 `mwv207_compile_shader`、I/O 记录、常量/立即数收集、汇编产出、shader link |
| `nir/mwv207_lower.h` | 48 | 跨文件 lowering 入口声明 |
| `nir/mwv207_lower.c` | 1687 | NIR pass 编排 `mwv207_lower()` + 大量驱动自定义 lowering |
| `nir/mwv207_common.h` | 298 | 指令字段宏、`mwv207_inst*` 结构、立即数类型枚举、公共原型 |
| `nir/mwv207_common.c` | 326 | swizzle 复合、`real_dest`/`src_index` 等 SSA→寄存器名映射、`mwv207_assemble` |
| `nir/mwv207_emit.h` | 115 | 发射层宏（`SRC_REG`/`SRC_CONST`/立即数构造）与原型 |
| `nir/mwv207_emit.c` | 2029 | NIR 指令 → `mwv207_inst` 的选择/编码 |
| `nir/mwv207_opcode.c` | 154 | NIR opcode → 硬件编码属性表 `mwv207_ops[]` |
| `nir/mwv207_ra.c` | 856 | 寄存器分配（线性扫描 + 特殊处理） |
| `nir/mwv207_liveness.c` | 257 | `live_defs()` 活跃区间计算 |
| `nir/mwv207_lower_64bit.c` | 786 | 64 位（fp64/int64）拆分与 vec2 化 |
| `nir/mwv207_lower_dual.c` | 268 | dual-source blend 拆分 |
| `nir/mwv207_lower_tg4.c` | 110 | `tg4`（textureGather）处理 |
| `nir/mwv207_lower_shared_atomic.c` | 194 | shared memory atomic 处理 |
| `nir/mwv207_nir_opt_peephole_ffma.c` | 234 | FMA 融合窥孔 |
| `nir/mwv207_nir_opt_peephole_fdiv.c` | 127 | 除法窥孔 |
| `nir/mwv207_nir_opt_peephole_set.c` | 188 | `set`/比较结果窥孔 |
| `nir/mwv207_nir_opt.h` | 39 | 三个窥孔 pass 的声明 |
| `nir/mwv207_disasm.h` | 328 | 指令字 `struct instr` 位域定义、反汇编原型 |
| `nir/mwv207_disasm.c` | 1317 | 反汇编器实现 |
| `nir/mwv207_cmdline.c` | 170 | 驱动自定义命令行/环境变量开关 |
| `mwv207_shader.c` | 1198 | shader state 创建/绑定/删除、二进制缓存、编译调度 |
| `include/mwv207_shader.h` | 105 | `mwv207_shader_state` 公开结构 |

---

## 2. 核心数据结构

### 2.1 `mwv207_compile` —— 一次编译的全部状态

`nir/mwv207_compiler.h:154-172`：

```c
struct mwv207_compile {
   nir_shader *nir;                  // 克隆自 shader->info->nir 的工作副本
   nir_function_impl *impl;

   struct mwv207_shader *shader;     // 输出的 shader（持有 code/constants）

   uint32_t block_ptr[0x10000];      // 指令下标 -> 基本块入口地址 映射
   int inst_ptr;
   struct mwv207_inst code[0x20000]; // 未编码的指令缓冲（每条 1 项；汇编后 16 字节）

   int const_count;
   uint64_t consts[0x800];           // 立即数/常量池：(contents<<32)|value

   struct ra_graph *g;
   struct ra_regs *regs;
   uint32_t *live_map;
   uint32_t num_nodes;
};
```

| 字段 | 含义 | 依据 |
|---|---|---|
| `block_ptr` | 一维数组按基本块号索引（`:160` 大小 `0x10000`），存该块在 `code[]` 中的起始下标；发射跳转时以索引占位，汇编前回填 | `mwv207_compiler.c:364-365` |
| `code[0x20000]` | 未编码中间表示，每项 `struct mwv207_inst` | `:163` |
| `consts[0x800]` | 每条 64 位：低 32 位值 + 高 32 位 `enum mwv207_immediate_contents`（见 §5.4） | `mwv207_compiler.c:270`、`:246` |
| `const_count` | 常量池条目计数（注意：`copy_uniform_state_to_shader` 把它乘 4 后当作组件数） | `mwv207_compiler.c:457`、`:239` |

### 2.2 `struct mwv207_shader` —— 编译产物与链接元数据

`nir/mwv207_compiler.h:102-152`。关键字段分组：

| 分组 | 字段（行号） | 说明 |
|---|---|---|
| 阶段 | `enum pipe_shader_type stage` (`:103`) | 当前 stage |
| I/O | `infile` / `outfile` (`:105`,`:107`) | `struct mwv207_shader_io_file`，`reg[0x10]` |
| 链接 | `link` (`:109`) | `struct mwv207_shader_link_info` |
| 常量 | `uniforms` (`:111`) | `struct mwv207_shader_uniform_info` |
| 代码 | `code`/`code_size`/`num_temps` (`:113-115`) | `code` 为 32 位字数组 |
| VS 专用 | `vs_id_in_reg`,`vs_pos_out_reg`,`vs_pointsize_out_reg`,`vs_load_balancing` (`:117-120`) | load balancing 见 §3.1 |
| GS 专用 | `gs_use_primitive_id`,`gs_primitive_id_location`,`gs_use_invocation_id`,`gs_use_restart`,`gs_use_layer` (`:122-126`) | |
| TCS/TES | `output_num`,`patch_num`,`inner_outer_count` (`:128-131`) | |
| FS 专用 | `ps_color_out_reg[8]`,`ps_depth_out_reg`,`useFragCoord`,`usePointCoord`,`useClipDistance` (`:133-137`) | |
| CS | `cs_shared_size` (`:139`) | |
| spill | `spill_offset`,`spill_buffer` (`:141-142`) | |
| 缓存链 | `next` (`:144`) | 同一 state 的多份变体（按 key 串成链表） |
| 回指 | `info` (`:146`)、`key` (`:147`) | `struct mwv207_shader_state *`、`struct mwv207_shader_key` |
| 其它 | `z32_shadow[][]` (`:148`)、`bo_offset` (`:150`)、`ubo_mask[8]` (`:151`) | |

### 2.3 `struct mwv207_shader_key` —— shader 变体键

`nir/mwv207_compiler.h:33-55`。位域打包进一个 `uint32_t global`：

| 位域 | 宽度 | 行号 |
|---|---|---|
| `frag_rgba_swap` | 1 | `:37` |
| `front_ccw` | 1 | `:39` |
| `tex_128bit` | 1 | `:40` |
| `pixel_128bit` | 1 | `:41` |
| `z32_shadow` | 1 | `:42` |
| `render_arrary` | 1 | `:43` |
| `has_tess` | 1 | `:44` |
| `tcs_input_vertries_regs` | 2 | `:46` |
| `is_tg4_lower` | 1 | `:47` |
| `is_dual` | 1 | `:48` |
| `sprite_coord_enable` | `MAX_TEXTURE_COORD_UNITS` | `:50` |
| `sprite_coord_yinvert` | 1 | `:51` |

比较用 `mwv207_shader_key_equal()`（`:57-63`）——直接比较打包后的 `global`，并有 `STATIC_ASSERT(sizeof(struct mwv207_shader_key) <= sizeof(a->global))`。

### 2.4 I/O 记录结构

`struct mwv207_shader_inout`（`:65-73`）：`reg`、`slot`、`driver_location`、`is_array`、`arry_index`、`num_components`、`interpolation`。
`struct mwv207_shader_io_file`（`:75-78`）：`size_t num_reg` + `reg[0x10]`（最多 16 个寄存器槽）。
`struct mwv207_shader_link_info`（`:93-100`）：`num_varyings`、`num_varyings_fixed`、`varyings[0x10]`、`pcoord_varying_comp_ofs`。

### 2.5 指令中间表示 `struct mwv207_inst`

`nir/mwv207_common.h:213-234`：

```c
struct mwv207_inst {
   uint32_t opcode;          // 7 位有效（:270 assert）
   uint32_t ext_opcode;      // opcode==0x7F 时的扩展码
   uint32_t type;            // 数据类型（2 位 + bit2）
   uint32_t cond: 5;
   uint32_t sat: 1;
   uint32_t round: 2;
   uint32_t sel_bit0: 1;
   uint32_t sel_bit1: 1;
   uint32_t dst_full: 1;
   struct mwv207_inst_dst dst;
   struct mwv207_inst_tex tex;
   struct mwv207_inst_src src[0x3];   // 最多 3 个源操作数
   uint32_t imm;                      // 常量池下标（汇编时回填 src2 reg）
   enum kinds_of_inst kind_of_inst;
   uint32_t flags;
   uint32_t src_mask;
   uint32_t src_count;
   struct mwv207_inst_src sampler_aux_src;
   struct mwv207_inst_ctrl inst_ctrl; // 由 disasm.h 定义
};
```

- `struct mwv207_inst_dst`（`:158-165`）：`use:1`、`amode:3`、`reg:7`、`reg_bit7:1`、`reg_bit8:1`、`write_mask:4` → **目的寄存器共 9 位（7+bit7+bit8）**。
- `struct mwv207_inst_src`（`:193-211`）：`use:1`、`rgroup:3`，其后是 union：
  - 寄存器形态：`reg:9`、`swiz:8`、`neg:1`、`abs:1`、`amode:3`、`is_const_reg:1`；
  - 立即数形态：`imm_val:20`、`imm_type:2`、`reverse:1`。
- `struct mwv207_inst_tex`（`:167-191`）：`id:5` + union（`amode:3`/`swiz:8`；或 `pack_mode`/`attri_client`/`attri_layout`/`is_local`；或 `restart`/`no_jump_end`）。

### 2.6 `struct mwv207_op_info`

`nir/mwv207_common.h:238-247`：

```c
struct mwv207_op_info {
   uint8_t opcode;
   uint8_t src;
   uint8_t cond;
   uint8_t type;
   uint8_t round;
   uint8_t ext_opcode;
};
extern struct mwv207_op_info mwv207_ops[nir_num_opcodes];
```

### 2.7 类型/立即数枚举

`enum mwv207_pass_flags`（`:78-88`）：`BYPASS_DST`/`BYPASS_SRC`（bit0/1）与 `SRC{0,1,2}_MOD_{NEG,ABS}`（bit2..7），用于在 `nir_instr.pass_flags` 上打标记；`set_src_mod_abs/neg`、`is_src_mod_abs/neg`、`is_dead_instruction` 为其访问器（`:94-131`）。`static_assert` 校验位段（`:90-92`）。

`enum mwv207_immediate_contents`（`:133-156`）共 **22** 个值：

| 值 | 名称 | 用途（从命名与 §4/§5 用法推断） |
|---|---|---|
| 0 | `MWV207_IMMEDIATE_UNUSED` | 空槽 |
| 1 | `MWV207_IMMEDIATE_CONSTANT` | 普通立即数（`CONST(x)`） |
| 2 | `MWV207_IMMEDIATE_UNIFORM` | UBO 常量（`UNIFORM(x)`） |
| 3-6 | `MWV207_UNIFORM_TEXTURE_WIDTH/HEIGHT/DEPTH`、`MWV207_UNIFORM_PATCH_VERTEX_IN` | 纹理尺寸/补丁顶点数（`TEXSIZE`，`emit.h:44`） |
| 7-8 | `MWV207_IMMEDIATE_TEXRECT_SCALE_X/Y` | RECT 纹理缩放（`TEXSCALE`，`emit.h:43`） |
| 9-10 | `MWV207_IMMEDIATE_WORK_GROUP_COUNT/SIZE` | CS 工作组参数 |
| 11-19 | `MWV207_IMMEDIATE_UBO/SSBO/SSIMAGE/SSIMAGE_STRIDE/SIMAGE_SIZE/SSIMAGE_INFO/SSIMAGE_LAYER_SIZE/SSBO_SIZE/ATOMIC_BO` | 资源描述符超载（见 §4/§5） |
| 20-21 | `MWV207_IMMEDIATE_LOCAL_MEMORY_OFFSET`、`MWV207_IMMEDIATE_GLOBAL_MEMEORY_OFFSET` | 地址空间偏移 |

驱动侧若全部为 `MWV207_IMMEDIATE_CONSTANT` 则 `no_unifrom = true`（提示无需常量缓冲区，`mwv207_compiler.c:242,248`）。

### 2.8 指令字位域（`struct instr`）

由 `nir/mwv207_disasm.h` 定义（`:328` 行的头文件，位域 `opc`/`cond`/`sat`/`dst_*`/`tex_*`/`src0_*`/`src1_*`/`src2_*`/`sel_0`/`sel_1`/`dst_full`/`opcode_bit6`/`type_bit01`/`type_bit2`，见 §9）。`mwv207_assemble()` 把 `struct mwv207_inst` 逐字段填入该位域（`mwv207_common.c:277-323`）。

---

## 3. 编译管线全景

### 3.1 顶层入口：`mwv207_compile_shader`

签名（`nir/mwv207_compiler.h:183-184`）：

```c
bool
mwv207_compile_shader(struct mwv207_context *ctx,
                      struct mwv207_shader_state *shader, bool offline);
```

实现 `nir/mwv207_compiler.c:410-478`。**实际调用顺序**：

| # | 行号 | 调用 | 作用 | 输入 / 输出 IR 形态 |
|---|---|---|---|---|
| 0 | `:413-416` | `shader = state->current`；`if (!shader) return false` | 取当前变体 | — |
| 1 | `:418-423` | `CALLOC_STRUCT(mwv207_compile)`；`c->shader = shader` | 分配编译上下文 | — |
| 2 | `:424` | `c->nir = nir_shader_clone(NULL, shader->info->nir)` | **克隆**一份可破坏的 NIR 副本 | NIR（含变量/deref，SSA 混合） |
| 3 | `:439-442` | `nir_lower_robust_access(c->nir, &robust_opts{.lower_ssbo=true})` | 把 SSBO 访问偏移 clamp 到界内（支撑 `PIPE_CAP_ROBUST_BUFFER_ACCESS_BEHAVIOR`） | NIR → NIR |
| 4 | `:444` | `mwv207_lower(ctx, c)` | **驱动全部 lowering/优化 pass**（详见 §3.2） | NIR → NIR（已全标量化、64 位拆分、vec 化、bool 降级完毕） |
| 5 | `:446` | `mwv207_record_input(c)` (`:47-136`) | 建立 `shader->infile`：为每个输入变量分配寄存器槽、记录 `slot`/`interpolation`/`num_components` | NIR 元数据 → `mwv207_shader_io_file` |
| 6 | `:448` | `mwv207_reset_output_index(c)` (`:138-232`) | 重排输出 `driver_location`（GS 特殊顺序；TCS 顶点/补丁区分离） | 改写 NIR 变量 `driver_location` |
| 7 | `:450` | `mwv207_record_uniform(c)` (`:253-348`) | 常量/常量 UBO 立即数化：`load_const` 打标、`load_uniform` 替换为 `nir_build_imm`；末尾 `nir_opt_dce` + `nir_convert_from_ssa` + `nir_trivialize_registers` | NIR SSA → **寄存器 IR（`nir_convert_from_ssa`，`:346-347`）** |
| 8 | `:452-453` | `if (ctx->ws->debug == 0x201) nir_print_shader(c->nir, stdout)` | 调试打印（magic 值 `0x201`） | — |
| 9 | `:455` | `mwv207_emit_shader(ctx, c)` | 指令选择与编码，填充 `c->code[]`、`c->regs/g`（RA 等） | 寄存器 IR → `struct mwv207_inst[]` |
| 10 | `:457` | `copy_uniform_state_to_shader(c->shader, c->consts, c->const_count)` (`:234-251`) | 常量池 → `shader->uniforms`（`imm_data`/`imm_contents`） | |
| 11 | `:459` | `mwv207_record_code(ctx, c, state, offline)` (`:350-408`) | 汇编 `code[]`→`shader->code`；非 offline 时拷进 BO（详见 §5.5） | `mwv207_inst[]` → 16 字节/条 的硬件指令流 |
| 12 | `:461-473` | VS：计算 `shader->vs_load_balancing` | 用 `VS_BALANCE()`（`common.h:71`）与 `ALIGN_BIT` 打包 a/b 及常数 `0x3f`/`0x0f` | |
| 13 | `:475-476` | `ralloc_free(c->nir)`；`FREE(c)` | 释放 | |

要点：

- **`nir_lower_robust_access` 只开 `.lower_ssbo`**，代码注释（`:426-438`）给出理由：驱动实现了 `get_ssbo_size()` 但**没有** `get_ubo_size()`，故 `.lower_ubo` 会产生未实现 intrinsic；24.1.2 的该 pass 只匹配非 deref 的 image intrinsic，而驱动看到的是 `image_deref_*`，`.lower_image` 在此为空操作；`.lower_shared` 属于 Vulkan/调试范畴。
- 编译失败条件只有 `!shader`（`:415`）与 `!c`（`:420`）两处；`mwv207_link_shader` 在末尾 **`return false`**（`:665`），即链接函数恒返回 false（调用方需注意，未确认是否有意为之）。

### 3.2 `mwv207_lower()` —— NIR pass 的完整顺序

签名（`nir/mwv207_compiler.h:180-181`）：

```c
void
mwv207_lower(struct mwv207_context *ctx, struct mwv207_compile *c);
```

实现 `nir/mwv207_lower.c:1557-1685`。`nir` 为 `c->nir`，`shader` 为 `c->shader`。**逐 pass 列表**（严格按源码行号顺序；`OPT_*` 宏见 §11）：

| # | 行号 | pass | 作用 | IR 形态变化 |
|---|---|---|---|---|
| 1 | `:1563` | `mwv207_early_lower(nir, c)` | FS + `sprite_coord_enable` 时做 `nir_lower_texcoord_replace`，否则 `sprite_coord_yinvert` 时反转 PNTC 的 y | NIR → NIR |
| 2 | `:1565` | `nir_remove_dead_variables(nir_var_shader_in, NULL)` | 删除未使用的输入变量 | 变量层 |
| 3 | `:1567-1574` | `nir_lower_io(nir_var_shader_in\|nir_var_uniform, mwv207_glsl_type_size, nir_lower_io_lower_64bit_to_32)`；**TCS/TES 只传 `nir_var_uniform`** | I/O 变量 → `load_input`/`load_uniform` intrinsic | 变量 → intrinsic |
| 4 | `:1576` | `nir_lower_vars_to_ssa` | 变量转 SSA | 变量 IR → SSA |
| 5 | `:1578-1580` | `nir_lower_indirect_derefs(nir_var_all, UINT32_MAX)`（**TCS/TES 跳过**） | 间接寻址 deref 展开 | SSA/deref |
| 6 | `:1582-1588` | `nir_lower_tex{lower_txp=~0u, lower_txf_offset=true, lower_txd_3d=true, lower_txd=true, lower_txs_lod=true}` | 纹理指令降级 | tex intrinsic 形态 |
| 7 | `:1590` | `nir_lower_alu_to_scalar(mwv207_alu_to_scalar_filter_cb, NULL)` | 按白名单把 ALU 标量化（白名单见 §4.3） | ALU 向量→标量 |
| 8 | `:1592-1593` | **`OPT_DIV` 为真时** `mwv207_nir_opt_peephole_fdiv` | 除法窥孔（默认 **关**，`debug_get_bool_option("OPT_DIV", false)`，`:1532`） | ALU → ALU |
| 9 | `:1595` | `nir_lower_flrp(16\|32\|64, false)` | `flrp` 降级 | ALU |
| 10 | `:1596` | `nir_lower_frexp` | `frexp` 降级 | ALU |
| 11 | `:1598` | `mwv207_optimize_loop(nir)` | 主优化不动点循环（§3.3） | — |
| 12 | `:1600-1611` | **仅 `MESA_SHADER_KERNEL`**：`nir_lower_mem_access_bit_sizes`（回调 `mem_access_size_align_cb`，`:1534-1549`）→ `nir_lower_alu_width(lower_vec816_alu→4)` → `nir_lower_alu_vec8_16_srcs` → `nir_lower_phis_to_scalar` | CS/内核内存访问规范化 | |
| 13 | `:1612` | `nir_opt_dce` | 死代码删除 | |
| 14 | `:1613` | `nir_lower_alu_to_scalar(mwv207_fp64_to_scalar_filter_cb, NULL)` | 64 位 FP 标量化（回调在 `mwv207_lower_64bit.c`） | |
| 15 | `:1614` | `mwv207_nir_split_64bit_var` | 64 位变量拆分（§4.6） | 变量 |
| 16 | `:1615` | `mwv207_lower_double_store_global` | 64 位 `store_global` 多分量拆分（`:731-782`：`>2` 分量 64 位时拆成 2 条，`write_mask` 3，第二条地址 `+16`） | intrinsic |
| 17 | `:1616` | `nir_split_64bit_vec3_and_vec4` | 64 位 vec3/vec4 拆分 | |
| 18 | `:1617` | `nir_lower_int64` | int64 全降级 | |
| 19 | `:1618` | `mwv207_nir_lower_64bit_uniforms_and_ubo` | 64 位 uniform/UBO 处理（§4.6） | |
| 20 | `:1619` | `nir_copy_prop` | | |
| 21 | `:1620` | `nir_opt_algebraic` | | |
| 22 | `:1621` | `nir_opt_dce` | | |
| 23 | `:1622` | `mwv207_nir_64_to_vec2` | 64 位 → vec2 形式（§4.6） | |
| 24 | `:1623` | `nir_opt_combine_stores(nir_var_shader_out)` | 输出 store 合并 | |
| 25 | `:1624` | `nir_lower_discard_or_demote(true)` | discard 降级 | |
| 26 | `:1626` | `mwv207_optimize_loop(nir)` | | |
| 27 | `:1627-1629` | **非 KERNEL**：`if (OPT(nir, mwv207_nir_lower_ubo_to_uniform)) mwv207_optimize_loop(nir)` | 常量 UBO → uniform（§4.2） | |
| 28 | `:1631` | `mwv207_lower_io(nir, shader)` | 驱动自定义 I/O 修正（front_face 符号、tex 坐标/lod/offset 等，§4.3） | |
| 29 | `:1633-1634` | FS + `sprite_coord_enable`：`mwv207_lower_sprite_coord_late(nir, c)` | 删除已死的 TEX0 变量并 `num_inputs--`（§4.3） | |
| 30 | `:1636` | `nir_lower_pack` | pack/unpack 降级 | |
| 31 | `:1637` | `mwv207_optimize_loop(nir)` | | |
| 32 | `:1639-1642` | `nir_lower_idiv(&{.allow_fp16 = true})` | 整数除法降级 | |
| 33 | `:1643` | `nir_lower_alu` | ALU 合法性降级 | |
| 34 | `:1645` | `mwv207_optimize_loop(nir)` | | |
| 35 | `:1647-1648` | **`OPT_FFMA`**（默认关）`mwv207_nir_opt_peephole_ffma` | FMA 融合（§4.7） | |
| 36 | `:1650-1651` | **`OPT_SET`**（默认关）`mwv207_nir_opt_peephole_set` | 比较结果窥孔（§4.7） | |
| 37 | `:1653-1655` | **仅 KERNEL**：`mwv207_lower_global(nir)` | `load/store_global` → `*_global_amd`（§4.4） | |
| 38 | `:1657` | `while (OPT(nir, nir_opt_vectorize, NULL, NULL));` | 向量化至不动点 | |
| 39 | `:1659-1660` | FS：`mwv207_lower_dual_blend(nir, ctx, shader)` | dual-source blend 拆分（§4.5） | |
| 40 | `:1662` | `nir_lower_alu_to_scalar(mwv207_alu_to_scalar_filter_cb, NULL)`（**第二次**） | 再次标量化 | |
| 41 | `:1664` | `nir_remove_dead_variables(nir_var_function_temp, NULL)` | | |
| 42 | `:1665` | `nir_opt_algebraic_late` | | |
| 43 | `:1666` | `nir_opt_constant_folding` | | |
| 44 | `:1668` | `nir_move_vec_src_uses_to_dest(false)` | | |
| 45 | `:1669` | `nir_copy_prop` | | |
| 46 | `:1671` | `nir_opt_dce` | | |
| 47 | `:1672` | `nir_opt_cse` | | |
| 48 | `:1673` | `nir_lower_bool_to_int32` | bool → int32 | |
| 49 | `:1674` | `nir_lower_bool_to_bitsize` | bool → 位宽 | |
| 50 | `:1675` | `mwv207_lower_alu(nir, c)` | sin/cos 缩放、fdiv/flog2 复制成 2 分量、bitfield_insert 打包（§4.3） | ALU |
| 51 | `:1676` | `mwv207_lower_roundEven(nir, ctx)` | `fround_even` 展开（`:1338-1374`） | |
| 52 | `:1677` | `mwv207_lower_txs(nir, ctx, shader)` | `texop_txs` → `load_uniform`（base=`~sampler_index`，§4.3） | tex → uniform load |
| 53 | `:1678` | `mwv207_lower_texelFetch(nir, ctx, shader)` | `txf` 分量 <2 时补成 vec2 坐标（`:1326-1332`） | |
| 54 | `:1679` | `mwv207_lower_128bit(nir, ctx)` | FS：`tex_128bit` / `pixel_128bit` 拆分（§4.3） | |
| 55 | `:1680` | `mwv207_lower_shadow_tex(nir, ctx, shader)` | `z32_shadow` 阴影采样降级（§4.3） | |
| 56 | `:1681` | `mwv207_nir_lower_tg4(nir, shader, ctx)` | gather4 处理（§4.8） | |
| 57 | `:1682` | `mwv207_lower_rgba_swap(nir, ctx)` | FS `frag_rgba_swap` 重排写入（`swizzle_record`） | |
| 58 | `:1683` | `mwv207_lower_vecn(nir, c)` | vec2/3/4 常量折叠与 mov 插入（§4.3） | |
| 59 | `:1684` | `mwv207_lower_shared_atomic(nir, c, ctx)` | shared atomic 处理（§4.9） | |

> 说明：第 1 步之前没有任何“TGSI→NIR”的转换出现在本文件内——`mwv207_lower` 的输入已经是 `nir_shader`（`nv50_ir`/`tgsi` 字样只出现在 include 的 `tgsi/tgsi_strings.h`，`:27`）。前端的 TGSI→NIR 由 `mwv207_shader.c` 一侧负责（§10）。

### 3.3 `mwv207_optimize_loop()` 内部的 pass 顺序

`nir/mwv207_lower.c:1494-1530`，`do { ... } while (progress)` 的优化不动点循环。轮内顺序（行号）：

| 行号 | pass |
|---|---|
| `:1502` | `nir_lower_vars_to_ssa`（`NIR_PASS_V`，不计 progress） |
| `:1503` | `nir_opt_copy_prop_vars` |
| `:1504` | `nir_opt_shrink_stores(true)` |
| `:1505` | `nir_opt_shrink_vectors(false)` |
| `:1506` | `nir_copy_prop` |
| `:1507` | `nir_opt_dce` |
| `:1508` | `nir_opt_cse` |
| `:1509` | `nir_opt_peephole_select(16, true, true)` |
| `:1510` | `nir_opt_intrinsics` |
| `:1511` | `nir_opt_algebraic` |
| `:1512` | `nir_opt_constant_folding` |
| `:1513` | `nir_opt_dead_cf` |
| `:1515-1519` | `nir_opt_loop`，若进步则再跑 `nir_copy_prop` + `nir_opt_dce` |
| `:1521` | `nir_opt_loop_unroll` |
| `:1522` | `nir_opt_if(nir_opt_if_optimize_phi_true_false)` |
| `:1523` | `nir_lower_64bit_phis` |
| `:1524` | `nir_opt_remove_phis` |
| `:1525` | `nir_opt_undef` |
| `:1526` | `nir_opt_conditional_discard`（用 `NIR_PASS`，非 `OPT`） |
| `:1527-1528` | FS：`nir_opt_move_discards_to_top` |

辅助回调：
- `mem_access_size_align_cb`（`:1534-1549`）：返回 `num_components = MIN2(bytes/(bit_size/8), 4)`、`bit_size`、`align = bit_size/8`；`align` 先经 `nir_combined_align`，并 assert 为 2 的幂。
- `lower_vec816_alu`（`:1551-1555`）：恒返回 `4`（把 ALU 宽度限制到 4）。

### 3.4 编译后的 shader 链接

`mwv207_link_shader`（`nir/mwv207_compiler.h:186-189` 声明，`mwv207_compiler.c:588-666` 实现）：

```c
bool
mwv207_link_shader(struct mwv207_context *ctx,
                   struct mwv207_shader *shader,
                   struct mwv207_shader_link_info *info);
```

- 先由 `mwv207_get_next_shader()`（`:563-586`）按 stage 链求下游 shader：TES→(GS?)；TCS→(TES?GS?)；VS→(TCS?→TES?→GS?)；否则默认 `PIPE_SHADER_FRAGMENT`。
- `assert(next_shader->infile.num_reg < 0x10)`（`:597`）。
- 遍历下游 `infile.reg[]`，用 `mwv207_output_lookup()`（`:544-561`，数组按 `is_array`+`slot`+`arry_index` 匹配，非数组按 `slot`）找上游输出，填充 `info->varyings[input->reg + link_offset]`（插值、分量数、寄存器号），累计 `comp_ofs`；`VARYING_SLOT_PNTC` 记录 `pcoord_varying_comp_ofs`（`:634-635`）。
- 特判：`input->reg==0 && slot==POS` 跳过（`:609-610`）；`reg==0` 且有输出时重置 `link_offset=0`、`num_varyings=0`、`varyings_offset=1`，非 FS 下游把 `vs_pos_out_reg=-1`（`:612-618`）；`VARYING_SLOT_LAYER` 跳过（`:620-621`）；下游为 FS 且 `input->slot==VARYING_SLOT_PRIMITIVE_ID` 且无匹配输出时跳过（`:638-639`）。
- 找不到语义输出时 **`printf("Semantic value not found in vertex shader outputs, please check if this is a fake input\n")`**（`:641`）——只打印，不中断（**未确认**是否为临时调试代码）。
- GS 使用 layer 时把 `link_offset` 置 0（`:601-602`），并追加一条 `interpolation=0x2`、`num_components=1` 的 varyings 项（`:654-658`）。
- 末尾 `mwv207_fix_stream_out()`（`:504-542`）处理 stream-out 的 `register_index` 重映射（`vs_pos_out_reg`/`vs_pointsize_out_reg` 特判、`register_index==63` 二次修正），随后若下游为 FS 则 `next_shader->link = *info`（`:662-663`）。
- 函数返回 `false`（`:665`）。

---

## 4. 驱动自定义 lowering / 优化 pass 逐个说明

（本节按 §3.2 的 pass 编号给出实现细节。）

### 4.1 `mwv207_early_lower` / sprite coord 两条路径

`mwv207_early_lower(nir, c)`：`nir/mwv207_lower.c:333-345`。

| 条件 | 动作 | 行号 |
|---|---|---|
| stage==`MESA_SHADER_FRAGMENT` 且 `key.sprite_coord_enable` | `nir_lower_texcoord_replace(nir, sprite_coord_enable, false, sprite_coord_yinvert)` 然后 `mwv207_lower_sprite_coord()` | `:338-341` |
| 否则 `key.sprite_coord_yinvert` | `mwv207_invert_point_coord()` | `:342-343` |

- `mwv207_lower_sprite_coord(nir, shader)`（`:267-282`）：遍历输入变量，`VARYING_SLOT_TEX0` 记 `idx = driver_location`；若 `VARYING_SLOT_PNTC` 且 `idx != -1`，把 PNTC 的 `driver_location` 改成同一个 `idx`（让点坐标与 sprite 坐标共用寄存器）。
- `mwv207_invert_point_coord(nir)`（`:305-331`）：找 `VARYING_SLOT_PNTC` 变量，对每个 `nir_intrinsic_load_deref` 结果构造 `vec2(x, 1.0 - y)` 并 `nir_def_rewrite_uses_after`。
- `mwv207_lower_sprite_coord_late(nir, c)`（`:291-303`）：用 `nir_remove_dead_variables(nir_var_shader_in, opts)`，`opts.can_remove_var = can_remove_dead_varying`（`:284-289`，仅当 `*is_sprite_coord` 且 `location==VARYING_SLOT_TEX0` 可删），成功则 `nir->num_inputs -= 1`。

### 4.2 `mwv207_nir_lower_ubo_to_uniform`

| 项 | 内容 | 行号 |
|---|---|---|
| 声明 | `static bool mwv207_nir_lower_ubo_to_uniform(nir_shader *shader)` | `:256-265` |
| 谓词 | `is_const_ubo(instr, data)`：`*uniform_count > 0xF0` 即返回 false；要求 `nir_intrinsic_load_ubo`、`src[0]`/`src[1]` 均常量、`block == 0` | `:201-223` |
| 改写 | `lower_ubo_to_uniform()`：`uniform_count += (num_components>1 ? 4 : 1)`；`uniform_offset = (ubo_offset - range_base) >> 4`；生成 `nir_load_uniform(num_components, bit_size, uniform_offset, .base = range_base/16, .range = range/16, .dest_type = nir_type_float32)` | `:225-254` |
| 调用点 | 非 KERNEL：`if (OPT(nir, mwv207_nir_lower_ubo_to_uniform)) mwv207_optimize_loop(nir)` | `:1627-1629` |

注意：`uniform_count` 局部变量在 `mwv207_nir_lower_ubo_to_uniform` 内定义并传地址，**改写结果并未回写 `c->consts`/`c->const_count`**（`:259` 局部 `int uniform_count = 0`）——计数只用于限制 pass 匹配次数上限（`0xF0`）。**未确认**该计数是否有意超出局部作用域使用。

### 4.3 `mwv207_lower_io` 及三个 ALU/vec 处理

**(a) `mwv207_lower_io(nir, shader)`**：`nir/mwv207_lower.c:347-473`。

| 处理对象 | 动作 | 行号 |
|---|---|---|
| `nir_intrinsic_load_front_face` | `def.bit_size = 32`，之后 `nir_ine(def, 0)`；若 `key.front_ccw` 把该 ALU 指令 op 改成 `nir_op_ieq` | `:359-370` |
| `nir_intrinsic_load_frag_coord` | `shader->useFragCoord = true` | `:372-374` |
| `nir_intrinsic_load_vertex_id` / `_instance_id` | `shader->vs_id_in_reg = shader->infile.num_reg` | `:375-378` |
| `nir_intrinsic_load_primitive_id` | `shader->gs_use_primitive_id = true` | `:380-382` |
| `nir_intrinsic_load_invocation_id` | `shader->gs_use_invocation_id = true` | `:384-386` |
| `nir_intrinsic_end_primitive` | `shader->gs_use_restart = true` | `:388-390` |
| `assert(tex->sampler_index == tex->texture_index)` | 前置断言 | `:405` |
| `nir_tex_src_lod` | float：clamp 到 `[0,100]`；int：clamp 到 `[0,100]` 再 `i2f32` | `:416-433` |
| `nir_tex_src_offset` | `mwv207_tex_offset()`（见下） | `:435-436` |
| `nir_tex_src_ms_index`/`comparator`/`sampler_deref`/`texture_deref`/`texture_offset`/`sampler_offset` | 忽略 | `:439-445` |
| 其它 src_type | `printf("unsupport io type %d\n", ...)` + `assert(0)` | `:447-450` |
| `GLSL_SAMPLER_DIM_RECT` | 生成 `load_uniform`（`base = ~sampler_index`，2 分量，`src[0]=0.0f`），坐标乘该值 | `:454-469` |

`mwv207_tex_offset(b, tex, shader)`（`:126-199`）：
- 无 offset 源时 `assert(0)`（`:131-132`）。
- lod：float 则 clamp `[0,100]` 后 `+0.5`，int 则 `i2f32 + 0.5`，再 `ffloor` 回写（`:144-153`）；无 lod 时默认 0；FS 且非 `txb` 时用 `compute_Lod()`（`:154-161`）。
- 坐标：float 且非 RECT 时用 `txsRsh = f2i32(txs) >> f2i32(lod)`、`scale = frcp(i2f32(txsRsh))`，`offset_coord = coord + i2f32(offset)*scale`；RECT 时直接 `coord + i2f32(offset)`；int 坐标则 `iadd`（`:167-181`）。
- `is_array`：2 分量时保留 `coord.y`，3 分量时第 3 分量做 `ffloor(+0.5)`（`:183-194`）。
- 最后 `nir_tex_instr_remove_src(tex, offset_index)`（`:198`）。

`compute_Lod()`（`:93-124`）：`Dx=fddx(coord)`、`Dy=-fddy(coord)`、`txs=get_texture_size()`，`fdot2` 后 `fmax`、`flog2`、`*0.5`、`+393216.0`、mask `0xffff`/`0x8000`/`|0xffff0000`、`b32csel`、再 `/32`、`+0.5`、`ffloor`。属驱动侧手写 LOD 近似。
`get_texture_size()`（`:39-91`）：构造一条 `nir_texop_txs`（`dest_type = nir_type_int`，srcs 复制 texture/sampler deref 等 + lod=0），包装为 `nir_i2f32`。

**(b) ALU 标量化白名单 `mwv207_alu_to_scalar_filter_cb`**（`:475-521`）：返回 true 的 op 有
`frsq, frcp, flog2, fexp2, fsqrt, fcos, fsin, fdiv, imul, imul_high, umul_high, fdph, fround_even`（`:484-497`），以及
`b32all_fequal2/3/4`、`b32any_fnequal2/3/4`、`b32all_iequal2/3/4`、`b32any_inequal2/3/4`（`:499-511`）。
`fdot2` 显式 `break` 返回 false（`:513-514`）。

**(c) `get_alu_cv(nir_alu_src*)`**（`:523-548`）：`nir_src_as_const_value` 失败且父指令是 `fabs`/`fneg` 时，取 `parent->src[0]` 的常量值，但要求每个分量高 32 位为 `MWV207_IMMEDIATE_CONSTANT`，否则判为空。

**(d) `mwv207_lower_alu(nir, c)` / `mwv207_lower_alu_impl`**（`:550-612`）：
- `fsin`/`fcos`：`src[0] *= 1.0/M_PI`（`:564-570`）。
- `fdiv`/`flog2`/`fsin`/`fcos`：`assert(num_components == 1)`（`:576`），把 `def` 改成 2 分量，插入 `fmul(def, def.y)` 且对后续使用改写（`:572-590`）。
- `bitfield_insert`：`src[2] = src[2] | (src[3] << 8)`（`:592-599`）。

**(e) `mwv207_lower_vecn` / `_impl`**（`:861-956`）与 `insert_vec_mov`（`:817-859`）、`vec_dest_has_swizzle`（`:784-815`）：
- 统计 vec2/3/4 各源里可解析的常量；`num_components > 1` 时用 `nir_build_imm` 合成常量并改写/删除（`:893-911`）。
- 对每个分量判断是否需要插入 `mov`：`need_mov = list_length(&ssa->uses) != 0`，任一 use 是 if 条件或非本 vec 指令则置 true；源为 `load_input` 时用 `vec_dest_has_swizzle()` 决定；`tex`/`alu` 默认不插入，其余类型强制 `need_mov = true`（`:915-944`）。
- `insert_vec_mov()` 把同源同修饰的连续分量合并为一条多分量 `mov`，返回 `write_mask`（`:817-859`）。

**(f) `mwv207_lower_roundEven`**（`:1338-1374`）：把 `nir_op_fround_even` 改成 `mov`，然后用 `fsign`/`fabs`/`floor`/`ffract`/`f2i32 & 1`/`fcsel` 组合重建“四舍六入五成双”，最后乘符号。

**(g) `mwv207_lower_txs`**（`:1276-1312`）：`texop_txs` → `load_uniform`（`base = ~sampler_index`，`range = 1`，分量数由 `sampler_dim`/`is_array` 决定 2 或 3，`dest_type = nir_type_int`），并删除原 tex 指令（`:1306-1307`）。

**(h) `mwv207_lower_texelFetch`**（`:1314-1336`）：`texop_txf` 且 `coord_components < 2` 时补成 `vec2(coord, 0)` 并把 `coord_components` 置 2。

**(i) `mwv207_lower_128bit` 与子实现**（`:1183-1202`）：仅 FS；`ctx->info.tex_128bit` → `mwv207_lower_tex_128_impl`；`ctx->info.pixel_128bit` → `mwv207_lower_fb_128_impl`。
- `mwv207_lower_tex_128_impl`（`:1013-1081`）：跳过 `tg4`，按 `ctx->info.tex_record[FS][sampler_index]` 复制出第二条纹理采样 `tex_ba`（sampler/texture index 换成 record 值），再用 `mwv207_exchange_texture_swizzle()`（`:958-1011`，按采样视图 `swizzle_r/g/b/a` 选择 RG（`tex`）或 BA（`tex_ba`）通道，0x4→0.0，0x5→1.0）构造 `vec4`。
- `mwv207_lower_fb_128_impl`（`:1083-1181`）：对 `store_deref`，若目标 location 在 `ctx->info.pixel_record[]`（FRAG_RESULT_COLOR 视为 DATA0），构造 BA 分量写入新建的 "fix_var"（location = `FRAG_RESULT_DATA0 + pixel_record[]`），原值改写为 RG 分量（缺分量补 0/1）。

**(j) `mwv207_lower_shadow_tex`**（`:1376-1486`）：仅 `ctx->info.z32_shadow` 开启时；对 `tex->is_shadow` 且采样视图格式为 `PIPE_FORMAT_Z32_FLOAT` 的采样打 `shader->z32_shadow[stage][texture_index] = true`，然后按 `sampler->state.compare_func` 处理：

| compare_func | 处理 | 行号 |
|---|---|---|
| `NEVER` | 结果替换为 `0.0f`，删除 tex | `:1436-1440` |
| `ALWAYS` | 结果替换为 `1.0f`，删除 tex | `:1442-1446` |
| `EQUAL` | 移除 comparator，`nir_seq(tex, compare)` | `:1448-1449` |
| `NOTEQUAL` | `nir_sne(tex, compare)` | `:1452-1453` |
| `GREATER` | `nir_slt(tex, compare)` | `:1456-1457` |
| `GEQUAL` | `nir_sge(compare, tex)`（swap=true） | `:1460-1461` |
| `LESS` | `nir_slt(compare, tex)`（swap=true） | `:1464-1465` |
| `LEQUAL` | `nir_sge(tex, compare)` | `:1468-1469` |

`mwv207_nir_remove_shadow_tex()`（`:1376-1404`）负责移除 `nir_tex_src_comparator` 源并在 tex 之后按 `swap` 方向插入比较。

**(k) `mwv207_lower_rgba_swap`**（`:1204-1274`）：FS 且 `ctx->info.frag_rgba_swap` 时，对 `store_deref`（`deref_type == nir_deref_type_var`）按 `ctx->info.swizzle_record[location - FRAG_RESULT_DATA0]` 重排分量；若 `SWIZZLE_STATE(...)` 已满足则跳过（`:1234-1235`）。3 分量时第 4 通道补 `0.0f`（`:1251-1252`）。

### 4.4 `mwv207_lower_global`（KERNEL）

- `mwv207_lower_global(nir)`（`:722-729`）→ `mwv207_lower_global_impl(impl)`（`:614-720`）。
- 只处理 `load_global`/`load_global_constant`/`store_global`（`:626-633`）。
- 用 `nir_scalar_chase_movs` 剥离 mov，把地址拆成 `base + off`（`iadd` 形式，否则 off=0，`:640-647`）；若 base 是 `load_const` 则交换 base/off（`:653-657`）；若 base 本身是 ALU `iadd`，则把地址进一步拆成 `base + (off1 + off2)` 并重建 `iadd`（`:659-681`）。
- `store_global` → `nir_intrinsic_store_global_amd`（src[0]=values, src[1]=base, src[2]=off，`:683-703`）；load → `nir_intrinsic_load_global_amd`（src[0]=base, src[1]=off，`:704-716`）。
- 断言：store 的 `src[1]` 分量数必须为 1（`:684`），load 的 `src[0]` 分量数必须为 1（`:705`）。

### 4.5 `mwv207_lower_dual_blend`（`nir/mwv207_lower_dual.c`，268 行）

```c
void mwv207_lower_dual_blend(nir_shader *nir, struct mwv207_context *ctx, struct mwv207_shader *shader); /* :260-269 */
```

**核心思路**：硬件不支持双源混合（dual-source blending）的即时混合，驱动改为**在 NIR 里手工完成一次混合**再写单一输出。

| 项 | 内容 | 行号 |
|---|---|---|
| 生效条件 | 仅当 `ctx->info.dsbi.is_dual` 为真 | `:260-262` |
| 入口 | 对 shader 内每个 function impl 调 `mwv207_lower_dual_blend_impl` | `:263-268`、`:180-258` |
| 识别两个源 | 扫描 `store_deref` 计数：**第 1 条**的 src 作为 `blend_src`，**第 2 条**作为 `blend_src1` | `:203-212` |
| 副作用标记 | 置 `shader->useFragCoord = true`（需要 frag coord 采样 RT） | `:213` |
| RT 坐标 | `get_rt_texture_coord`：`nir_load_frag_coord` + `ctx->sampler_views[4][ctx->info.dsbi.sampler_index]->texture` 的 `width0/height0` 归一化，`ctx->info.dsbi.y_invert` 决定 y 翻转（`fdiv`） | `:109-129` |
| RT 取值 | `create_rt_texture`：`nir_texop_txb`、`GLSL_SAMPLER_DIM_2D`、texture/sampler index 均为 `dsbi.sampler_index` | `:131-151` |
| 因子 | `get_factor_def` 覆盖全部 `PIPE_BLENDFACTOR_*`，`CONST_COLOR` 用 `ctx->blend_color.color[]`，`SRC_ALPHA_SATURATE` 用 `min(srcA, 1-dstA)` | `:29-107` |
| 混合函数 | `handle_blend_func`：`PIPE_BLEND_ADD`/`SUBTRACT`/`REVERSE_SUBTRACT`/`MIN`/`MAX` | `:153-179` |
| 写回 | 新建 `nir_var_shader_out` 变量 `"final_out"`（`location = FRAG_RESULT_DATA0`、`driver_location = 2`、`nir->num_outputs++`），用 `nir_op_vec4` 组 rgb+alpha，`store_deref` 的 `write_mask = 0xf` | `:228-254` |

> 采样槽位硬编码为 **4**（`ctx->sampler_views[4]`）；`dsbi.sampler_index` 由 `mwv207_context` 在创建 shader 时填好。**未确认** slot 4 与 4 号槽位是否有其它用途冲突。

### 4.6 64 位相关 lowering（`nir/mwv207_lower_64bit.c`，786 行）

声明（`nir/mwv207_lower.h`）：

```c
bool mwv207_fp64_to_scalar_filter_cb(const nir_instr *instr, const void *data);  /* :33-37 */
bool mwv207_nir_split_64bit_var(nir_shader *shader);
bool mwv207_nir_64_to_vec2(nir_shader *shader);
bool mwv207_nir_lower_64bit_uniforms_and_ubo(nir_shader *shader);
```

调用点（`mwv207_lower.c`）：`nir_lower_alu_to_scalar(mwv207_fp64_to_scalar_filter_cb)` `:1613`；`mwv207_nir_split_64bit_var` `:1614`；`mwv207_lower_double_store_global` `:1615`；`nir_split_64bit_vec3_and_vec4` `:1616`；`nir_lower_int64` `:1617`；`mwv207_nir_lower_64bit_uniforms_and_ubo` `:1618`；`mwv207_nir_64_to_vec2` `:1622`。

| 子 pass | 谓词/范围 | 变换 | 行号 |
|---|---|---|---|
| `mwv207_fp64_to_scalar_filter_cb` | 仅 `alu` 且 op ∈ `{unpack_64_2x32_split_x, unpack_64_2x32_split_y, pack_64_2x32_split}` | 返回 `true` → 交给上游 `nir_lower_alu_to_scalar` 拆成标量 | `:41-59` |
| `mwv207_nir_split_64bit_var` | `need_split_64bit_var`：`load_uniform/load_ubo/load_ssbo/load_global/load_deref`、`store_deref`、`load_const`，且 `bit_size == 64 && num_components >= 3` | 用 `nir_shader_lower_instructions` 把 64 位 **vec3/vec4** 变量拆成 `xy`(dvec2) + `zw`(dvec2 或 dvec1) 两个变量对（`get_var_pair` 用哈希表 `split_vars` 缓存；`shader_in/out` 时 `zw` 的 `driver_location++`/`location++`；`function_temp` 时把 `zw` 变量插进 `impl->locals`）。加载侧：`split_double_load_uniform/ubo/ssbo/global/deref`（`:154-368`）把一次 dvec3/4 加载拆成两次，再用 `merge_64bit_loads`（`:135-152`）重组；存储侧 `split_store_deref_var/array`（`:231-307`）；常量侧 `split_load_const`（`:370-378`） | `:415-427`、`:101-133`、`:61-99`、`:380-413` |
| `mwv207_lower_double_store_global` | 同名实现也在 `mwv207_lower.c`（KERNEL 路径） | 见 §4.4 与 `mwv207_lower.c:731` | — |
| `mwv207_nir_lower_64bit_uniforms_and_ubo` | `is_64bit_uniforms_and_ubo`：`load_uniform`/`load_ubo`/`load_ubo_vec4` 且 `bit_size == 64` | 把 `bit_size` 64→2×32、`num_components` 翻倍、`component` 乘 2，再用 `nir_pack_64_2x32_split` 重组出 64 位结果（断言 `old_components <= 2`） | `:472-479`、`:429-442`、`:444-470` |
| `mwv207_nir_64_to_vec2` | `need_lower_to_vec2`（`:542-585`） | ① 预扫描：把「源中含 64 位值」的 `alu` 记入链表 `instr64bit`（`:699-735`），并对 `store_global` 令 `num_components *= 2`、`write_mask = (1<<n)-1`；② `nir_shader_lower_instructions` 逐类改写：`store_deref`→`store_64_to_vec2`（变量类型改 `glsl_vec_type(components*2)` 或数组，`write_mask` 1→0x3 否则 0xf，`:492-521`）、`load_deref`→`load_deref_64_to_vec2`（`:587-616`）、`load_global(_constant)`→`load_64_to_vec2`（`:481-490`）、`alu`（`bit_size=32`、`num_components*=2`；`pack_64_2x32_split`→`nir_op_vec2`、`pack_64_2x32`→`mov`、`vec2`→`vec4`、`vec4`→`vec8` 仅 KERNEL 且 `:654` 断言）、`load_const` 拆成两个 32 位常量（`:669-680`）、`undef`（`:681-686`）；③ 修正链表里 alu 的 swizzle：`unpack_64_2x32_split_x`→`swizzle*2`、`_y`→`swizzle*2+1`、`unpack_64_2x32`/`bcsel` 特例、默认偶奇配对（`:741-782`） | `:692-786`、`:618-690` |

### 4.7 三个窥孔优化

| pass | 声明 | 默认 | 调用点 |
|---|---|---|---|
| `mwv207_nir_opt_peephole_fdiv` | `mwv207_nir_opt.h` | 关（`OPT_DIV`） | `mwv207_lower.c:1592-1593` |
| `mwv207_nir_opt_peephole_ffma` | `mwv207_nir_opt.h` | 关（`OPT_FFMA`） | `mwv207_lower.c:1647-1648` |
| `mwv207_nir_opt_peephole_set` | `mwv207_nir_opt.h` | 关（`OPT_SET`） | `mwv207_lower.c:1650-1651` |

开关宏 `#define do_by_default(OPT) (debug_get_bool_option("OPT_"#OPT, false))`（`mwv207_lower.c:1532`，文件末尾 `:1687` `#undef`）——**默认 false，即三个窥孔默认全部不跑**。

| pass | 文件/行数 | 匹配模式 | 改写 |
|---|---|---|---|
| `fdiv` | `mwv207_nir_opt_peephole_fdiv.c`(127) | `fdiv`（非 exact）且**除数**是单分量 `load_const`（`get_load_const_for_src` `:32-39`） | `frcp(除数)` + `fmul(被除数, frcp)`（`:67-90`），有进展则 `nir_metadata_preserve(block_index\|dominance)` |
| `ffma` | `mwv207_nir_opt_peephole_ffma.c`(234) | `fadd`（非 exact、两源不同 `:136-140`）的某个源可回溯成 `fmul`：`get_mul_for_src`（`:65-120`）递归穿透 `mov/fneg/fabs`（`fneg` 翻转 negate、`fabs` 置 abs 并清 negate）并累乘 swizzle；要求 `are_all_uses_fadd(mul->def)`（`:31-63`，所有使用者都是 `fadd/mov/fneg/fabs`） | 发射 `ffma`：src0/src1 = mul 的两源（abs 时对两者 `nir_fabs`，negate 时对 src0 `nir_fneg`），swizzle 逐分量复合 `ffma->src[i].swizzle[j] = mul->src[i].swizzle[swizzle[j]]`，src2 = add 的另一源；`nir_def_rewrite_uses` + 断言无残余 use + 插在 add 前 + 删 add（`:170-199`） |
| `set` | `mwv207_nir_opt_peephole_set.c`(188) | `b2f32`（非 exact）的源经 `get_cmp_for_src`（`:31-73`，可穿透 mov）命中 `feq32/ine32/fge32/flt32/flt` | 改写为整数比较 `set`：`fge32→sge`、`flt32/flt`→`slt`、`feq32→seq`、`ine32→sne`（`:107-127`），swizzle 逐分量复合 |

三者骨架一致：`mwv207_nir_opt_peephole_X(nir_shader*)` → `_impl` → `_block`，用 `nir_metadata_preserve(block_index | dominance)` 保留元数据。

### 4.8 `mwv207_nir_lower_tg4`（`nir/mwv207_lower_tg4.c`，110 行）

```c
void mwv207_nir_lower_tg4(nir_shader *nir, struct mwv207_shader *shader, struct mwv207_context *ctx); /* :94-111 */
```

- 仅当 `shader->key.is_tg4_lower` 时生效（`:94-97`）；遍历 function/block/instr，对 `instr->type == nir_instr_type_tex && op == nir_texop_tg4` 调 `lower_instr`（`:31-92`）。
- **动机**：硬件 gather 只支持 2 个分量。`lower_instr` 按 `tg4->component` 把请求重映射为两次采样，并**同时改写原 tg4 的 `sampler_index`**：

| `component` | 重映射后 | sampler_index |
|---|---|---|
| 0 | 0 | 不变 |
| 1 | 2 | 不变 |
| 2 | 0 | **+1** |
| 3 | 2 | **+1** |

（`:41-68`）

- 克隆出 `new_tg4`：`component = 重映射后的 +1`，继承 `num_srcs/sampler_dim/dest_type/coord_components/is_array/is_shadow/is_new_style_shadow/texture_index/texture_(non_)uniform/tg4_offsets[4][2]/src[]`，`dest` 为 4×32 位（`:36-84`）。
- 合并：`temp = nir_ishl(new_tg4_dst, 16)`、`temp2 = nir_ior(temp, tg4_dst)`，再用 `nir_def_rewrite_uses_after(tg4_dst, temp2, temp2->parent_instr)` 把原结果 4 个 16 位分量拼成一个 32 位值给后续使用者（`:85-91`）。

### 4.9 `mwv207_lower_shared_atomic`（`nir/mwv207_lower_shared_atomic.c`，194 行）

```c
void mwv207_lower_shared_atomic(nir_shader *nir, struct mwv207_compile *c, struct mwv207_context *ctx); /* :169-194 */
```

- 先 `check_has_shared_atomic`（`:146-167`，扫 `shared_atomic`/`shared_atomic_swap`）为假则直接返回。
- 逐 function 调 `lower_instr`（`:115-144`）：把 `load_shared`/`shared_atomic`/`shared_atomic_swap` 的 `src[0]`（`store_shared` 用 `src[1]`）改写为 `offset + base_offset + nir_intrinsic_base(instr)`（`nir_iadd`/`nir_iadd_imm`，`:138-141`）；有进展时置 `c->shader->cs_shared_size = nir->info.shared_size`（`:192-193`）。
- `base_offset` 由 `set_base_offset`（`:92-113`）一次性构造 —— `set_base_offset_impl`（`:29-90`）做**指令形状匹配**：要求序列严格为 `load_num_workgroups → load_workgroup_id → imul → imul → imul → iadd → iadd`（`intrinsic_lists`/`base_op_lists` `:32-42`），命中后取最后一条 alu 的结果，再按 workgroup 大小换算：
  - `local_invocation_index_bits = ffs(util_next_power_of_two(MAX2(work_group_size, 64))) - 1`（断言 `<= 8`，`:73-89`）；
  - 若 workgroup 不是 1×1×1，先 `& ((1 << (16 - bits)) - 1)`，再 `* nir->info.shared_size`。
- **形状匹配失败则 `assert(0)`**（`:109-111`）——即该 lowering 强依赖 `nir_lower_compute_system_values` 等上游 pass 产出的固定代码形状；上游若改变工作组的展开方式，此处会直接崩溃。**未确认**该假设在其它 workgroup 形状下是否总成立。

---

## 5. 指令编码（mwv207_emit.c）

> 本节先给出编码基础设施（`mwv207_common.h` / `mwv207_common.c` / `mwv207_emit.h` 已通读部分）；
> `mwv207_emit.c` 的选择逻辑见 §5.6 起（下轮补充）。

### 5.1 汇编器 `mwv207_assemble()`

签名 `nir/mwv207_common.h:269-270`：

```c
int mwv207_assemble(uint32_t *out, const struct mwv207_inst *inst);
```

实现 `nir/mwv207_common.c:263-326`：

- **前置条件**：`if (inst->imm && inst->src[2].use) return 1;`（`:267-268`）——常量池下标与第三源操作数互斥，冲突则返回 1（失败）。`assert(!(inst->opcode & ~0x7f))` 限定 opcode 7 位（`:270`）。
- `opcode == 0x7F` 时用 `inst->ext_opcode`（`:274-275`）。
- 把 `struct mwv207_inst` 逐字段写入 `struct instr *code = (struct instr *)out`（`:277-320`），映射关系：
  `opc = opcode & 0x3f`、`opcode_bit6 = (opcode & 0x40) >> 6`、`cond`、`sat`、`dst_use`、`dst_amode`、`dst_reg`、`dst_comps = dst.write_mask`、`tex_id`、`tex_amode`、`tex_swiz`、
  `srcN_use/reg/swiz/neg/abs/amode/rgroup`（N=0,1,2）、`sel_0 = sel_bit0`、`sel_1 = sel_bit1`、`dst_full`、
  `type_bit2 = (type & 0x4) >> 2`、`type_bit01 = type & 0x3`（`:295,:308`）。
- `out[1] |= inst->round;`（`:297`）——round 只写第 2 个字。
- `out[3] |= (extcode << 4) | (inst->imm << 7);`（`:322-323`）——扩展码与常量池下标打包在第 4 个字。
- **每条硬件指令固定 16 字节（4 个 32 位字）**，调用者按 `&code[i*4]` 逐条落位（`mwv207_compiler.c:366`）。

### 5.2 立即数（源操作数）机制

`nir/mwv207_emit.h:53-84`：立即数走源操作数的 **`rgroup == 0x7`** 形态（union 的 `imm_val:20 / imm_type:2 / reverse:1`，`mwv207_common.h:205-209`）。

| 构造函数 | 行号 | `imm_type` | 约束 |
|---|---|---|---|
| `mwv207_immediate_src(type, bits)` | `:53-62` | 参数 | `.use=1, .rgroup=0x7, .imm_val=bits` |
| `mwv207_immediate_float(float x)` | `:64-70` | `0x0` | `assert((bits & 0xfff) == 0)`；存 `bits >> 12`（低 12 位必须为 0） |
| `mwv207_immediate_int(int x)` | `:72-77` | `0x1` | `assert(-0x80000 <= x < 0x80000)` |
| `mwv207_immediate_uint(int x)` | `:79-84` | `0x2` | `assert(-0x80000 <= x < 0x80000)` |

因此单条指令内嵌立即数只有 **20 位**；float 立即数可用位只有 20 位（隐含 float 的低 12 位尾数为 0，即只支持 12 位尾数精度的浮点常量）。

### 5.3 寄存器源/目的宏与 rgroup 语义

`nir/mwv207_emit.h:34-44`：

| 宏 | 展开 | 行号 |
|---|---|---|
| `DST_REG(idx, m)` | `{ .use=1, .reg=idx, .write_mask=m }` | `:34` |
| `ALU_SWIZ(s)` | `INST_SWIZ(swizzle[0..3])` | `:35` |
| `SRC_DISABLE` | 全 0 | `:36` |
| `SRC_CONST(idx, s)` | `{ .use=1, .rgroup=0x2, .reg=idx, .swiz=s }` | `:37` |
| `SRC_REG(idx, s)` | `{ .use=1, .rgroup=0x0, .reg=idx, .swiz=s }` | `:38` |
| `CONST_VAL(a,b)` | `(nir_const_value){.u64 = (uint64_t)(a)<<32 \| (uint64_t)(b)}` | `:40` |
| `CONST(x)` | `CONST_VAL(MWV207_IMMEDIATE_CONSTANT, x)` | `:41` |
| `UNIFORM(x)` | `CONST_VAL(MWV207_IMMEDIATE_UNIFORM, x)` | `:42` |
| `TEXSCALE(x, i)` | `CONST_VAL(MWV207_IMMEDIATE_TEXRECT_SCALE_X + i, x)` | `:43` |
| `TEXSIZE(x, i)` | `CONST_VAL(MWV207_UNIFORM_TEXTURE_WIDTH + i, x)` | `:44` |

`mwv207_rgroup_is_uniform(rgroup)`：`rgroup == 0x2 || rgroup == 0x3`（`:46-51`）——**0x2 = 常量缓冲（constant）源，0x3 也被视为 uniform 类**；0x0 = 普通寄存器；0x7 = 立即数。

`#define emit(type, args...) mwv207_emit_##type(c, args)`（`:32`）——发射层统一隐式传 `c`。

### 5.4 swizzle / write-mask 复合工具

`nir/mwv207_common.h:40-67` 定义 2 位/分量的 swizzle 编码：`INST_SWIZ_COMP_X/Y/Z/W = 0..3`；`INST_SWIZ_X/Y/Z/W` 分别位于 bit0-1/2-3/4-5/6-7；`INST_SWIZ_BROADCAST(x)`；`INST_SWIZ_IDENTITY`；`SWIZZLE(c0,c1,c2,c3)` 按 X/Y/Z/W 名字组装。

`nir/mwv207_common.c`：

| 函数 | 签名 | 行为 | 行号 |
|---|---|---|---|
| `inst_swiz_compose` | `uint32_t inst_swiz_compose(uint32_t swz1, uint32_t swz2)` | 以 `swz2` 为索引对 `swz1` 重排（2 位一组） | `:29-36` |
| `inst_write_mask_compose` | `uint32_t inst_write_mask_compose(uint32_t wm1, uint32_t wm2)` | 按 `wm2` 的置位顺序压缩 `wm1` 的写掩码 | `:38-53` |
| `src_swizzle` | `struct mwv207_inst_src src_swizzle(struct mwv207_inst_src src, uint32_t swizzle)` | `rgroup != 0x7` 时才复合（立即数有 `reverse` 位而非 swizzle） | `:55-62` |
| `const_add` | `int const_add(uint64_t *c, uint64_t value)` | 在 4 项窗口内去重/占位，命中返回下标，满则 `-1` | `:250-261` |

### 5.5 汇编产物落地 `mwv207_record_code()`

`nir/mwv207_compiler.c:350-408`，签名：

```c
static void mwv207_record_code(struct mwv207_context *ctx,
                               struct mwv207_compile *c,
                               struct mwv207_shader_state *state,
                               bool offline);
```

| 行号 | 行为 |
|---|---|
| `:358-359` | 若 `inst_ptr == 0`，补一条 `opcode = 0x0` 的空指令（保证非空二进制） |
| `:361` | `code = MALLOC(c->inst_ptr * 16)` —— **每条指令 16 字节** |
| `:362-367` | 逐条：`opcode == 0x16` 时把 `inst->imm = c->block_ptr[inst->imm]`（跳转目标回填）；`mwv207_assemble(&code[i*4], inst)` |
| `:369` | `shader->code_size = c->inst_ptr * 4` —— **`code_size` 单位是 32 位字（dword）**，非字节 |
| `:371-404` | `!offline` 时把二进制写进 `state->bo`：首次创建 BO（`bo_size = shader->code_size * sizeof(uint32_t) * 16`，且不小于 `0x1000`；domain `MWV207_DOMAIN_VRAM`，flag `MWV207_FLAG_CPU_ACCESS`）；已有 `shader->code` 时先 flush 并 `bo_wait(OS_TIMEOUT_INFINITE)` 再覆盖，拷贝长度 `state->current->code_size * sizeof(uint32_t)`；否则分配 `shader->bo_offset = state->bo_offset` 并推进 `state->bo_offset += code_size * sizeof(uint32_t) * 2` |
| `:403` | `assert(state->bo_offset < state->bo->base.base.size)` |
| `:406` | `shader->code = code` |

**待确认点（未确认）**：
1. `bo_size = code_size * 4 * 16` 字面上是“所需字节数的 16 倍”，而 `bo_offset` 步进是“字节数的 2 倍”。两者都不等于 `code_size*4`，可能与硬件指令缓冲对齐/双缓冲需求有关，但源码无注释说明。
2. `opcode == 0x16` 被当作跳转指令、其 `imm` 是 `block_ptr` 下标（`:364-365`）——该假设的具体硬件语义需对照 `mwv207_opcode.c`（§6）确认。
3. offline 分支不写 BO，仅产出 `shader->code`。

### 5.6 `mwv207_emit.c` 总流程

签名（`nir/mwv207_emit.h:112-113`、实现 `nir/mwv207_emit.c:2013-2029`）：

```c
void mwv207_emit_shader(struct mwv207_context *ctx, struct mwv207_compile *c);
```

| 步骤 | 行号 | 内容 |
|---|---|---|
| 1 | `:2016` | `nir_shader *shader = c->nir` |
| 2 | `:2018` | `c->impl = nir_shader_get_entrypoint(c->nir)`（**只处理入口函数**，不支持多函数） |
| 3 | `:2020` | `mwv207_init_reg_state(ctx, c, shader)` —— 实现在 `mwv207_ra.c:799`，内含寄存器分配 |
| 4 | `:2022` | `mwv207_fix_start(c)` |
| 5 | `:2024` | `emit_cf_list(c, &entrypoint->body)` —— 主体发射 |
| 6 | `:2026` | `mwv207_fix_end(c)` |
| 7 | `:2028` | `mwv207_free_reg_state(c)`（`mwv207_ra.c:840`） |

**注意**：发射阶段不再调用任何 RA 函数——RA 在 `mwv207_init_reg_state()` 中一次性完成（§7）；发射期 `mwv207_dest()`/`mwv207_src()`（`mwv207_ra.c:459` 起）只做查询。

`emit_inst()`（`:32-37`）：`assert(c->inst_ptr < 0x20000); c->code[c->inst_ptr++] = *inst;`。

### 5.7 控制流发射

| 函数 | 行号 | 硬件编码 |
|---|---|---|
| `emit_block(c, block)` | `:1844-1856` | 先 `emit(block_start, block->index)` 记 `block_ptr[block]=inst_ptr`（`:1387-1392`）；逐条 `emit_instr`；若块以 jump 结束**或**后继块号小于本块（回边），追加 `emit(jump, successors[0]->index, SRC_DISABLE)` |
| `emit_cf_list` | `:1935-1958` | `nir_cf_node_block` → `emit_block`；`_if` → `emit_if`；`loop` → 直接展开 body（`:1950`，**不做循环回边特判**，回边由 `emit_block` 处理）；其它 → `compile_error("Unknown NIR node type")` |
| `emit_if` | `:1916-1933` | 若 `OPT_IF_BAIL`（默认 true，`:1919`）且命中 “then 只有一条 break/return 且 else 为空” 模式 → 发一条 `jump_with_nz` 直跳 then 的后继；否则 `jump` 到 else 起始 + then_list + （then 不以 jump 结尾且 else 非空时）跳 else 后继 + else_list |
| `has_only_one_jump_instr` | `:1861-1878` | 判定块是 CF 末块、指令表恰有一条且为 `nir_instr_type_jump` |
| `is_block_empty` | `:1880-1885` | CF 末块且指令表为空 |
| `is_then_bail_else_empty_if` | `:1887-1914` | then/else 各只有一块、else 空、then 仅一条 `nir_jump_break`/`nir_jump_return` |

跳转/丢弃指令构造：

| 函数 | 行号 | opcode | cond | 说明 |
|---|---|---|---|---|
| `mwv207_emit_jump` | `:508-527` | `0x16` | `0xA`（有条件时） | `type=0x6`；`src[0].swiz = INST_SWIZ_BROADCAST(src[0].swiz & 3)`；无条件时仅 `{.opcode=0x16, .imm=block}` |
| `mwv207_emit_jump_with_nz` | `:487-506` | `0x16` | `0xB` | “非零跳转”，其余同上 |
| `mwv207_emit_discard` | `:529-547` | `0x17` | `0xB`（有 ko 条件时） | 无条件时 `{.opcode=0x17}` |
| `collective barrier` | via `emit_intrinsic` | `0x2A` | — | `nir_intrinsic_barrier`（`:1518-1520`） |

> `emit_block` 的后继选择 `block->successors[0]`（`:1852`）在 NIR 中 CF 末块的重排语义下与 `block->successors[1]` 的关系依赖 NIR 已做 CF 规范化；**未确认**是否存在只取 `successors[0]` 而丢分支的情况（含 `nir_if` 两分支时由 `emit_if` 分别处理）。

### 5.8 ALU 选择与编码

`emit_alu()`（`:1394-1440`）→ `mwv207_emit_alu()`（`:282-425`）。

**`emit_alu` 的准备工作**：

| 行号 | 行为 |
|---|---|
| `:1399-1400` | `is_dead_instruction()` 为真直接丢弃（该宏来自 `mwv207_common.h`） |
| `:1402` | `assert(!(alu->op >= nir_op_vec2 && alu->op <= nir_op_vec4))` —— vecN 必须已被 lowering 消掉 |
| `:1405` | `mwv207_dest(c, &alu->def, &dst_swiz)`（RA 结果 → 目的寄存器 + swizzle） |
| `:1408-1413` | `fdot2/3/4` 强制 `dst_swiz = INST_SWIZ_IDENTITY` |
| `:1420-1437` | 遍历 `info->num_inputs`（`bitfield_insert` 跳过第 4 个，`:1421-1423`）：`src = src_swizzle(mwv207_src(c,&asrc->src), ALU_SWIZ(asrc))` 再 `src_swizzle(src, dst_swiz)`；若 `src.rgroup != 0x7` 则 `neg = (op==fneg)`、`abs = (op==fabs)` |
| `:1438-1439` | `src_bitsize = nir_src_bit_size(alu->src[0].src)`；`saturate = (op == nir_op_fsat)` |

**`mwv207_emit_alu` 的编码**（签名 `:282-284`）：

```c
static void mwv207_emit_alu(struct mwv207_compile *c, nir_op op, struct mwv207_inst_dst dst,
                            struct mwv207_inst_src src[3], uint32_t src_bitsize, bool saturate);
```

| 行号 | 行为 |
|---|---|
| `:286` | `struct mwv207_op_info ei = mwv207_ops[op]` —— **查表取硬件编码**（表见 §6） |
| `:287` | `swiz_scalar = INST_SWIZ_BROADCAST(ffs(dst.write_mask) - 1)` —— 取目的写掩码的最低分量作为标量广播 |
| `:289-293` | `OPT_MOV`（默认 true）时，`mov` 且同寄存器、同 swizzle、无 abs/neg → **直接不发射**（MOV 消除） |
| `:295-298` | `ei.opcode == 0xff` 时 `printf(" not support nir opcode %s ")` 并 `assert` 失败 |
| `:300-307` | 由 `ei` 填 `opcode/type/cond/round`，加 `dst`、`sat=saturate` |
| `:309-310` | `opcode == 0x7F` 时带 `ext_opcode = ei.ext_opcode` |
| `:312-331` | `fdiv/flog2/fsin/fcos` 先置 `inst.tex.amode=1`，与 `frsq/frcp/fexp2/fsqrt/imul` 一起把 `src[0]/src[1]`（非立即数时）与 `swiz_scalar` 复合 |
| `:333-409` | 逐 op 的特殊化（见表下） |
| `:415-416` | `opcode == 0x31` 且 `src[2]` 未用时补 `imm int -1` |
| `:417-422` | **源操作数重排**：`i = (ei.src >> (j*2)) & 3`；`if (i < 3) inst.src[j] = src[i]` —— `mwv207_op_info.src` 每 2 位编码一个硬件源槽对应哪个 NIR 源，`3` 表示该槽空 |
| `:424` | `emit_inst` |

op 特化表（`mwv207_emit_alu` 内 switch）：

| NIR op | 处理 | 行号 |
|---|---|---|
| `b2f32` | `src[2] = imm_float(1.0f)` | `:333-335` |
| `b2i32`/`b2i16`/`b2i8` | `src[2] = imm_int(1)` | `:337-341` |
| `ineg` | `src[0] = imm_int(0)` 且 `src[0].neg = 1` | `:343-346` |
| `fround_even` | `src[0] = imm_float(0.5f)` | `:348-350` |
| `extract_u16` | `assert(false)`（不支持） | `:352-355` |
| `isub_sat` | `type = get_alu_inst_type(int, src_bitsize)`；`src[1].neg = 1`（**要求 src[1] 非立即数**） | `:357-362` |
| `usub_sat` | 同上，type 用 uint | `:364-369` |
| `u2u32`/`u2u16`/`u2u8` | `type = uint(src_bitsize)`；`src[1] = imm_uint(32)` | `:371-376` |
| `i2i32` | `type = int(src_bitsize)`；`src[1] = imm_int(32)` | `:378-381` |
| `i2f32` | `type = int(src_bitsize)`；`src[1] = imm_float(32.0f)` | `:383-386` |
| `u2f32` | `type = uint(src_bitsize)`；`src[1] = imm_float(32.0f)` | `:388-391` |
| `iabs`/`imin`/`imax`/`iadd_sat`/`ieq32`/`ine32`/`ilt32`/`ige32` | `type = int(src_bitsize)` | `:393-402` |
| `iand`/`uadd_sat`/`ult32`/`uge32` | `type = uint(src_bitsize)` | `:404-409` |

类型编码 `get_alu_inst_type(dest_type, bitsize)`（`:205-232`，按 `dest_type | bitsize` switch）：

| 类型 | 编码 |
|---|---|
| `float32` | `0x0` |
| `float16` | `0x4` |
| `int32` | `0x1` |
| `int16` | `0x5` |
| `int8` | `0x2` |
| `bool32`/`uint32` | `0x6` |
| `bool16`/`uint16` | `0x3` |
| `bool8`/`uint8` | `0x7` |

`same_swiz(swiz_src, dst_write_mask)`（`:234-280`）：把写掩码（1..14）映射到 swizzle 常量表后逐分量比较，判定 `mov` 是否可与自身目的等价；写掩码 0 或 15+ 直接返回 false。

### 5.9 纹理指令发射

`emit_tex(c, tex)`（`:1442-1498`）：

| 行号 | 行为 |
|---|---|
| `:1449-1487` | 遍历 `tex->num_srcs`：`coord`、`bias`/`lod`（断言只有一个）、`comparator`、`ms_index`、`texture_offset`、`sampler_offset` 分别记录；`sampler_deref`/`texture_deref` 忽略；其它 → `compile_error("Unhandled NIR tex src type")` |
| `:1489-1490` | 若同时有 `tex_offset` 与 `sampler_offset`，断言二者 `ssa->index` 相同 |
| `:1492-1497` | 调 `mwv207_emit_tex(..., tex->texture_index, dst, dst_swiz, coord, lod_bias?:SRC_DISABLE, compare?:…, ms_index?:…, tex_offset?:…, tex->component)` |

`mwv207_emit_tex()`（`:427-485`，签名见 `:428-431`）：

| 行号 | 行为 |
|---|---|
| `:433-438` | 基础 inst：`dst`、`tex.id = texid`、`tex.swiz = dst_swiz`、`src[0] = coord` |
| `:440-444` | `lod_bias.use` → `src[1]`；`compare.use` → `src[2]` |
| `:446-455` | 有 `tex_offset` 时**先额外发射一条 `opcode 0x56`**（`type=0x6`，`dst=DST_REG(0,0x1)`，`src[2]=tex_offset`），并置 `inst.tex.amode = 0x1`（即 alu 结果作为地址偏移） |
| `:457-482` | 按 `nir_texop` 选 opcode：`tex→0x18`、`txb→0x19`、`txl→0x6F`、`txf→0x49`（并 `src[2]=imm_int(gen_coorddata(0x0,0x1))`）、`tg4→0x7D`（`src[1]=imm_uint(component)`）、`lod→0x7F` + `ext_opcode=0x4`、`txf_ms→0x7F` + `ext_opcode=0xE`（`src[1]=ms_index`）；其它 `assert(0)` |

`gen_coorddata(dataType, mode)`（`:113-126`）：把 `dataType`（bit0-1）、`mode<<12`（bit12-13）、`addressingType=0x1<<8`（bit8-9）、`magFilter=0`（bit4）、`minFilter=0`（bit5）用 `ALIGN_BIT` 拼成一个 20 位内嵌立即数。

### 5.10 intrinsic 发射总表

`emit_intrinsic()`（`:1500-1811`）。分发宏 `emit(kind, args...)`（`mwv207_emit.h:32`）展开为 `mwv207_emit_<kind>(c, args)`。默认分支：`compile_error(c, "Unhandled NIR intrinsic type: %s\n", nir_intrinsic_infos[intr->intrinsic].name)`（`:1807-1809`）。

| NIR intrinsic | 处理函数 / 编码 | 行号 |
|---|---|---|
| `store_deref` | `emit(output, intr, src[1])` → 记录输出寄存器 | `:1504-1506` |
| `demote_if`/`discard_if` | `emit(discard, src[0])` | `:1508-1511` |
| `demote`/`discard` | `emit(discard, SRC_DISABLE)` | `:1513-1516` |
| `barrier` | `opcode 0x2A` | `:1518-1520` |
| `load_uniform` | **两条指令**：`opcode 0xA`（`dst.write_mask=0x1`，`src[2]=src[0]`，即把偏移写进 A0）；`opcode 0x9`（dst 为真目的，`src[2]={rgroup=0x2, reg=nir_intrinsic_base, swiz=dst_swiz, amode=0x1}`） | `:1521-1541` |
| `load_ubo` | `opcode 0x32`，`type=0x6`，`src[0]=src[1]`，`src[1]=const(UBO, idx)`；`idx` 取 `src[0]` 常量，非常量时 `printf("no const ubo idx, just use 0, fix me later")` 并用 0 | `:1543-1559` |
| `load_front_face`/`load_frag_coord` | 空（由 `mwv207_lower_io` 处理） | `:1561-1563` |
| `load_primitive_id` | 仅 TES：`opcode 0x78`，写入 reg1 的 `write_mask=0x4`，`src[1]=imm_int(1)`，`src[2]=imm_int(1)` | `:1565-1583` |
| `load_vertex_id`/`load_instance_id`/`load_invocation_id`/`load_patch_vertices_in`/`load_input` | 空 | `:1585-1590` |
| `load_per_vertex_input` | `emit(load_gs, intr)` | `:1592-1594` |
| `load_deref` | `emit(load_deref, intr)` → 按 `var->data.patch` 分派 `load_perVertex`/`load_patch` | `:1596-1598` |
| `emit_vertex` | `opcode 0x7F` + `ext_opcode 0x1`，`tex.restart=gs_use_restart`、`no_jump_end=true`，`src[2]=imm_uint(1)` | `:1600-1617` |
| `end_primitive` | `opcode 0x7F` + `ext_opcode 0x2`，`tex.restart=true`，`src[2]=imm_uint(0)` | `:1619-1635` |
| `load_tess_coord` | 空 | `:1637-1638` |
| `load_num_workgroups`/`get_ssbo_size`/`load_workgroup_id`/`load_workgroup_size`/`load_local_invocation_id`/`load_global_invocation_id`/`load_global_invocation_id_zero_base` | 空（由其它层处理；`get_ssbo_size` 见 §7） | `:1640-1647` |
| `load_ssbo`/`store_ssbo` | `emit(load_buffer/store_buffer, intr, false)` | `:1649-1655` |
| `load_global_constant`/`load_global_amd` | `opcode 0x32`，`type=get_alu_inst_type(uint, def.bit_size)`，`src[0]=const(GLOBAL_MEMEORY_OFFSET, index)`，`src[1]=src[1]`；`index = nir_src_as_int(src[1]->parent load 的 src[1]) / bytesize` | `:1657-1672` |
| `store_global_amd` | `opcode 0x33`；`src[0]=const(GLOBAL_MEMEORY_OFFSET, index)`，`src[1]=src[2]`（地址），`src[2]=src[0]`（数据）；`dst.use = (src[2].rgroup != 0x0)`；非立即数时 `src[2].swiz &= (1 << (num_components*2)) - 1` | `:1674-1694` |
| `image_deref_load`/`image_deref_store` | `emit(load_image/store_image)` | `:1696-1702` |
| `ssbo_atomic`/`ssbo_atomic_swap` | `emit(atom, intr, false, false)` | `:1703-1706` |
| `shared_atomic`/`shared_atomic_swap` | `emit(atom, intr, true, false)` | `:1707-1710` |
| `image_deref_atomic`/`_swap` | `emit(atom, intr, false, true)` | `:1711-1714` |
| `load_shared` | `c->shader->cs_shared_size` 非 0 → `load_buffer(true)`，否则 `load_shared` | `:1715-1720` |
| `store_shared` | 同理 → `store_buffer(true)` / `store_shared` | `:1721-1726` |
| `atomic_counter_read` | `opcode 0x65`，`type=0x6`，`src[0]=const(ATOMIC_BO, base)`，`src[1]=src[0]`，`src[2]=imm_int(0)` | `:1727-1738` |
| `atomic_counter_inc` | `opcode 0x65`，`src[2]=imm_int(1)`；dst 未用时强制 `use=1, write_mask=1` | `:1740-1758` |
| `atomic_counter_pre_dec` | `opcode 0x65`，`src[2]=imm_int(-1)`；若 dst 有用再补 `opcode 0x1`（`src[0]=mwv207_src_from_dest`，`src[2]=imm_int(-1)`） | `:1760-1783` |
| `image_deref_size` | `opcode 0x9`，`type=0x6`，`src[2]=const(SSIMAGE_SIZE, binding)` | `:1785-1802` |
| `decl_reg`/`load_reg`/`store_reg` | 空（寄存器/SSA 由 RA 层处理） | `:1803-1806` |

`emit_instr()`（`:1813-1842`）：`alu→emit_alu`、`tex→emit_tex`、`intrinsic→emit_intrinsic`、`jump→assert(nir_instr_is_last)`、`load_const`/`undef`/`deref` → 空、其它 → `compile_error("Unhandled NIR instruction type: %d")`。

### 5.11 存储/原子/图像/共享内存的发射细节

**(a) 顶点/图元属性加载 `opcode 0x78`**：

| 函数 | 行号 | 关键点 |
|---|---|---|
| `mwv207_emit_load_patch` | `:549-574` | `offset = driver_location (+ get_deref_index 若非常量)`；`remapIndex = (stage==TCS ? 0 : 1)`；`tex{pack_mode=1, attri_client=1, attri_layout=1}`；`src[0]=REG(0, W 广播)` |
| `mwv207_emit_load_gs` | `:576-631` | 无 primitive_id/invocation_id/restart 的 GS → `src_swiz=(Z,W,X,Y)`、`load_reg=0`；常量 src[0]：`>=8` 时 `-8` 且 `load_reg=2`；非常量仅支持 `load_invocation_id`（`REG(0, Y 广播)`），否则 `assert(0)` |
| `mwv207_emit_load_perVertex` | `:633-793` | TES 时 `attri_layout=1`；数组下标来自 `load_invocation_id` 或任意 ALU 时进入 **select-map** 路径：`tcs_input_vertries_regs+1 >= 2` 才走（`:666`），选 `select_map_reg = patch_outputs_read ? 9 : 5`（`:668`），`map_start = (var 为 shader_out) ? 5 : 1`（var 为 out 时用 `mwv207_get_LS_attr_client`），发射两条 `opcode 0x43`（`type=0x3`，`tex.swiz=0x80`/`0x81`，`dst=select_map_reg`，写掩码 0x1，源为 map_start..+3），再发射 `opcode 0x78`（`src[0]=REG(select_map_reg, X 广播)`，`src[1]/src[2]=imm_int(0)`）；常量下标 → `load_reg = idx/8 + 1`、`src = imm_int(idx % 8)` |

**(b) 输出存储 `opcode 0x42`**：

| 函数 | 行号 | 关键点 |
|---|---|---|
| `mwv207_emit_store_gs` | `:810-829` | `dst.write_mask = nir_intrinsic_write_mask`；`tex{attri_client=store client, attri_layout=1}`；`src[0]=REG(0,(X,Y,Y,Y))`；`src[1]=imm_int(var->data.driver_location)` |
| `mwv207_emit_store_tess` | `:831-904` | `patch_offset = driver_location*4 + location_frac`；TESS_LEVEL_OUTER/INNER 加 `arr.index`，否则加 `get_deref_index(deref)*4`；**patch 变量逐分量发一条**（`write_mask = 1<<(patch_offset%4)`，源按分量广播）；非 patch 一次发一条（`src.swiz=XYZW`，写掩码取 intrinsic）；分别更新 `shader->patch_num` / `shader->output_num` |
| `mwv207_emit_output` | `:1283-1385` | 记录 FS 输出寄存器（`ps_color_out_reg`、`ps_depth_out_reg`）、VS 的 `vs_pos_out_reg`/`vs_pointsize_out_reg`，并把其它 stage 的输出登记进 `shader->outfile.reg[]`（`slot`/`driver_location`/`is_array`/`arry_index`/`num_components`）；VA 的 CLIP_DIST0/1 强制 `is_array=true`；FS 遇到非 COLOR/DATAx/DEPTH 输出 `unreachable("Unsupported fs output")` |

**(c) 共享内存 / SSBO / UBO `opcode 0x32`（load）、`0x33`（store）**：

| 函数 | 行号 | 关键点 |
|---|---|---|
| `mwv207_emit_load_shared` | `:906-929` | `type=0x6`，`tex.is_local=1`，`src[0]=REG(1, W 广播)`（固定本地地址寄存器），`src[1]=src[0]` |
| `mwv207_emit_store_shared` | `:1125-1156` | `src[1]=src[1]`，`src[2]=data`；`dst.use = (data.rgroup != 0x0)` |
| `mwv207_emit_load_buffer` | `:931-951` | `uniform = is_local ? MWV207_IMMEDIATE_LOCAL_MEMORY_OFFSET : MWV207_IMMEDIATE_SSBO`；`buffer_index = is_local ? 0 : src[0]`；`src[0]=const(uniform, idx)`，`src[1]= is_local ? src[0] : src[1]` |
| `mwv207_emit_store_buffer` | `:1158-1185` | 同上，`src[1]= is_local ? src[1] : src[2]`，`src[2]=data` |

**(d) 原子 `mwv207_emit_atom`（`:1021-1123`）**：

- `get_atomic_op`（`:953-983`）：`iadd`/`fadd→0x65`；`umin`/`imin`/`fmin→0x68`；`umax`/`imax`/`fmax→0x69`；`iand→0x6B`；`ior→0x6A`；`ixor→0x6C`；`xchg→0x66`；`cmpxchg`/`fcmpxchg→0x67`；默认 `assert(0)`。
- 源索引映射 `index[] = {0,1,2,3}`：local 全 `-1`，image 全 `+1`（`:1032-1039`）。
- local/ssbo：`src0 = const(uniform(LOCAL_MEMORY_OFFSET|SSBO), index)`（`:1076-1079`）。
- image：默认 `opcode 0x37`；`dim > GLSL_SAMPLER_DIM_2D || is_array` 时 `opcode 0x38`，并先发 `opcode 0x4C`（`type=0x1`，`dst=coord.reg` 且 `write_mask=0x4`，`src[0]=coord` 的 Z 广播，`src[1]=const(SSIMAGE_LAYER_SIZE)`，`src[2]=const(SSIMAGE)`）计算层地址；然后 `emit(image_addr, src0, ssimage_index, opcode)`，最后 `src0.swiz = broadcast(src0.swiz & 3)`（`:1045-1074`）。
- `imin`/`imax` → `type = 0x1`（`:1083-1086`）。
- `cmpxchg`：先把 `src[index[3]]` 的低位搬到 `src[index[2]]` 的寄存器（`opcode 0x9`，`type=0x6`，`write_mask=0x2`），再令 `src2 = src[index[3]]`（`:1088-1101`）。
- dst 未用时置 `use=1, write_mask=0x1`（`:1107-1110`）；最终 `tex.pack_mode=0x1`（`:1112-1122`）。

**(e) 图像访问**：

| 函数 | 行号 | 默认 opcode | 多维/数组时 |
|---|---|---|---|
| `mwv207_emit_load_image` | `:1187-1230` | `0x79` | `0x34` + 先发 `0x4C` 计算层（同 atomic 的写法） |
| `mwv207_emit_store_image` | `:1232-1281` | `0x7A`（`dst.write_mask=0xf`） | `0x35` + 先发 `0x4C` |
| `mwv207_emit_image_addr` | `:998-1019` | 参数 opcode（供 atomic 用） | `dst={use=1, reg=src.reg, write_mask=0xf}`，断言 `src.swiz` 为 identity |
| `get_image_info` | `:985-996` | `mwv207_const(c, {SSIMAGE, SSIMAGE_STRIDE, SSIMAGE_SIZE, SSIMAGE_INFO}, 4)` —— 4 个资源描述符常量打包 |

**(f) 前后缀修正**：

| 函数 | 行号 | 条件 | 发射 |
|---|---|---|---|
| `mwv207_fix_start` | `:1960-1990` | TCS 且 `patch_num != 2` | `opcode 0x42`，写掩码 `0x8`，`src[1]=imm_int(1)`，`src[2]=REG(0,W 广播)` |
|  |  | FS 且 `key.render_arrary` | `opcode 0x9`，`type=0x6`，`dst={reg=0, write_mask=0x8}`，`src[2]=REG(1, X 广播)` |
| `mwv207_fix_end` | `:1992-2011` | TCS | `opcode 0x42`，写掩码 `0x4`，`src[1]=imm_int(1)`，`src[2]=REG(0,Z 广播)` |

### 5.12 emit 层涉及的调试开关

| 开关 | 默认 | 位置 | 作用 |
|---|---|---|---|
| `OPT_MOV` | `true` | `mwv207_emit.c:289` | 关掉（=false）则不消除自赋值 `mov` |
| `OPT_IF_BAIL` | `true` | `mwv207_emit.c:1919` | 关掉则不使用 “then 只有 break/return” 的短路发码形态 |

---

## 6. 指令表（mwv207_opcode.c，154 行）

### 6.1 表结构

`struct mwv207_op_info`（`nwv207_common.h` 实为 `mwv207_common.h:238-245`）——6 个 `uint8_t`：

```c
struct mwv207_op_info {
   uint8_t opcode;     /* 硬件 opcode（0x7F = EXTENDED，需配合 ext_opcode）*/
   uint8_t src;        /* 源操作数重排编码，见 §5.8 */
   uint8_t cond;       /* 条件/功能子码 */
   uint8_t type;       /* 数据类型编码 */
   uint8_t round;      /* 舍入模式 */
   uint8_t ext_opcode; /* 扩展 opcode（仅 opcode==0x7F 有效）*/
};
extern struct mwv207_op_info mwv207_ops[nir_num_opcodes];   /* mwv207_common.h:247 */
```

表本体 `mwv207_opcode.c:40-154`。默认值填 `{0xff}`（`:41` `[0 ... nir_num_opcodes - 1] = {0xff}`）——即**未在表中列出的 NIR opcode，其 `opcode` 为 `0xff`**，会命中 `mwv207_emit_alu` 的 “not support nir opcode” → `printf` + `assert`（`mwv207_emit.c:295-298`）。

### 6.2 `src` 字段的源槽重排编码（`mwv207_opcode.c:29-38`）

每 2 bit 对应一个硬件源槽（槽位序号 j = 0/1/2），值为该槽应取的 **NIR 源下标**；`3` 表示该槽**不使用**。`mwv207_emit_alu` 用 `i = (ei.src >> (j*2)) & 3; if (i < 3) inst.src[j] = src[i]` 落地（`mwv207_emit.c:417-422`）。

| 宏 | 位模式（src0·src1·src2） | 含义 |
|---|---|---|
| `SRC_0_1_2` | 0,1,2 | 三源直通 |
| `SRC_0_1_X` | 0,1,空 | 二元（src2 不用） |
| `SRC_0_X_X` | 0,空,空 | 一元（只用 src0） |
| `SRC_0_X_1` | 0,空,1 | 二元但占用硬件槽 2（如 `fadd`：硬件 `+` 在第 3 槽） |
| `SRC_0_1_0` | 0,1,0 | 硬件槽 2 复用 NIR src0（`fmin/fmax` 类：槽 2 是 Ctrl 选择子） |
| `SRC_X_X_0` | 空,空,0 | 只用硬件槽 2（如 `frcp`/`fsqrt` 等把操作数放在第 3 槽） |
| `SRC_0_X_0` | 0,空,0 | 二者都取 NIR src0（`fddx/fddy`：槽 0 与槽 2 都是源） |
| `SRC_X_0_1` | 空,0,1 | 硬件槽 1 取 NIR src0、槽 2 取 NIR src1（`fdiv`） |

### 6.3 条目数

**共 101 条**（`mwv207_opcode.c:64-153` 中显式初始化的 `[nir_op_*]` 条目，逐行统计：`:64` 4 条、`:65-75` 12 条、`:77-86` 10 条、`:88-108` 21 条、`:110-118` 9 条、`:120-128` 9 条、`:130-131` 2 条、`:133-145` 13 条、`:147-153` 7 条 → 4+12+10+21+9+9+2+13+7+... 累计 101）。

> **死代码提示**：`:42-63` 定义了 `OPCT/OPC/IOPC/UOPC/U16_OPC/I16_OPC/EIOPC/EUOPC/OP/IOP/UOP/EIOP/EUOP/U16_OP/I16_OP` 共 15 个构造宏，但**表中一条也没有使用**（全部写成显式结构体字面量）。属于遗留/未使用宏。

### 6.4 完整对照表（NIR opcode → 硬件编码）

「type」列即 `struct mwv207_op_info.type`（`0x0=F32, 0x1=S32, 0x2=S8, 0x3=U16, 0x4=F16, 0x5=S16, 0x6=U32, 0x7=U8`，与 `get_alu_inst_type()` 的编码一致，见 §5.8）。cond 的语义汇总见 §6.5。

| NIR opcode | opcode | src | cond | type | round | ext | 行 |
|---|---|---|---|---|---|---|---|
| `mov` | 0x9 | `SRC_X_X_0` | 0 | 0x0 | 0 | 0 | `:64` |
| `fneg` | 0x9 | `SRC_X_X_0` | 0 | 0x0 | 0 | 0 | `:64` |
| `fabs` | 0x9 | `SRC_X_X_0` | 0 | 0x0 | 0 | 0 | `:64` |
| `fsat` | 0x9 | `SRC_X_X_0` | 0 | 0x0 | 0 | 0 | `:64` |
| `fmul` | 0x3 | `SRC_0_1_X` | 0 | 0x0 | 0 | 0 | `:65` |
| `fadd` | 0x1 | `SRC_0_X_1` | 0 | 0x0 | 0 | 0 | `:65` |
| `ffma` | 0x2 | `SRC_0_1_2` | 0 | 0x0 | 0 | 0 | `:65` |
| `fdot2` | 0x73 | `SRC_0_1_X` | 0 | 0x0 | 0 | 0 | `:66` |
| `fdot3` | 0x5 | `SRC_0_1_X` | 0 | 0x0 | 0 | 0 | `:66` |
| `fdot4` | 0x6 | `SRC_0_1_X` | 0 | 0x0 | 0 | 0 | `:66` |
| `fmin` | 0xF | `SRC_0_1_0` | 0x1 | 0x0 | 0 | 0 | `:67` |
| `fmax` | 0xF | `SRC_0_1_0` | 0x2 | 0x0 | 0 | 0 | `:67` |
| `ffract` | 0x13 | `SRC_X_X_0` | 0 | 0x0 | 0 | 0 | `:68` |
| `frcp` | 0xC | `SRC_X_X_0` | 0 | 0x0 | 0 | 0 | `:68` |
| `frsq` | 0xD | `SRC_X_X_0` | 0 | 0x0 | 0 | 0 | `:68` |
| `fsqrt` | 0x21 | `SRC_X_X_0` | 0 | 0x0 | 0 | 0 | `:69` |
| `fsin` | 0x22 | `SRC_X_X_0` | 0 | 0x0 | 0 | 0 | `:69` |
| `fcos` | 0x23 | `SRC_X_X_0` | 0 | 0x0 | 0 | 0 | `:69` |
| `fsign` | 0x27 | `SRC_X_X_0` | 0 | 0x0 | 0 | 0 | `:70` |
| `ffloor` | 0x25 | `SRC_X_X_0` | 0 | 0x0 | 0 | 0 | `:70` |
| `fceil` | 0x26 | `SRC_X_X_0` | 0 | 0x0 | 0 | 0 | `:70` |
| `flog2` | 0x12 | `SRC_X_X_0` | 0 | 0x0 | 0 | 0 | `:71` |
| `fexp2` | 0x11 | `SRC_X_X_0` | 0 | 0x0 | 0 | 0 | `:71` |
| `seq` | 0x10 | `SRC_0_1_X` | 0x5 | 0x0 | 0 | 0 | `:72` |
| `sne` | 0x10 | `SRC_0_1_X` | 0x6 | 0x0 | 0 | 0 | `:72` |
| `sge` | 0x10 | `SRC_0_1_X` | 0x3 | 0x0 | 0 | 0 | `:72` |
| `slt` | 0x10 | `SRC_0_1_X` | 0x2 | 0x0 | 0 | 0 | `:72` |
| `fcsel` | 0xF | `SRC_0_1_2` | 0xB | 0x0 | 0 | 0 | `:73` |
| `fdiv` | 0x64 | `SRC_X_0_1` | 0 | 0x0 | 0 | 0 | `:74` |
| `fddx` | 0x7 | `SRC_0_X_0` | 0 | 0x0 | 0 | 0 | `:75` |
| `fddy` | 0x8 | `SRC_0_X_0` | 0 | 0x0 | 0 | 0 | `:75` |
| `fround_even` | 0x1 | `SRC_X_X_0` | 0 | 0x0 | **0x1** | 0 | `:77` |
| `extract_u16` | 0x60 | `SRC_0_X_X` | 0 | 0x1 | 0 | 0 | `:79` |
| `ibitfield_extract` | 0x60 | `SRC_0_1_2` | 0 | 0x1 | 0 | 0 | `:80` |
| `ubitfield_extract` | 0x60 | `SRC_0_1_2` | 0 | 0x6 | 0 | 0 | `:81` |
| `bitfield_reverse` | 0x6D | `SRC_0_X_X` | 0 | 0x6 | 0 | 0 | `:82` |
| `bitfield_insert` | 0x54 | `SRC_0_1_2` | 0 | 0x1 | 0 | 0 | `:83` |
| `find_lsb` | 0x7F | `SRC_0_X_X` | 0 | 0x1 | 0 | **0xB** | `:84` |
| `ifind_msb` | 0x7F | `SRC_0_X_X` | 0 | 0x1 | 0 | **0xC** | `:85` |
| `ufind_msb` | 0x7F | `SRC_0_X_X` | 0 | 0x6 | 0 | **0xC** | `:86` |
| `i2f32` | 0x2D | `SRC_0_X_X` | 0 | 0x1 | 0 | 0 | `:88` |
| `i2i32` | 0x2C | `SRC_0_X_X` | 0 | 0x1 | 0 | 0 | `:89` |
| `i2f16` | 0x2C | `SRC_0_X_X` | 0 | 0x5 | 0 | 0 | `:90` |
| `i2i16` | 0x2C | `SRC_0_X_X` | 0 | 0x5 | 0 | 0 | `:91` |
| `i2i8` | 0x2C | `SRC_0_X_X` | 0 | 0x2 | 0 | 0 | `:92` |
| `u2f32` | 0x2D | `SRC_0_X_X` | 0 | 0x6 | 0 | 0 | `:93` |
| `u2u32` | 0x2C | `SRC_0_X_X` | 0 | 0x6 | 0 | 0 | `:94` |
| `u2u16` | 0x2C | `SRC_0_X_X` | 0 | 0x3 | 0 | 0 | `:95` |
| `u2u8` | 0x2C | `SRC_0_X_X` | 0 | 0x7 | 0 | 0 | `:96` |
| `u2f16` | 0x2C | `SRC_0_X_X` | 0 | 0x5 | 0 | 0 | `:97` |
| `f2i32` | 0x2E | `SRC_0_X_X` | 0 | 0x1 | 0 | 0 | `:98` |
| `f2i16` | 0x2E | `SRC_0_X_X` | 0 | 0x5 | 0 | 0 | `:99` |
| `f2i8` | 0x2E | `SRC_0_X_X` | 0 | 0x2 | 0 | 0 | `:100` |
| `f2u32` | 0x2E | `SRC_0_X_X` | 0 | 0x6 | 0 | 0 | `:101` |
| `f2u16` | 0x2E | `SRC_0_X_X` | 0 | 0x3 | 0 | 0 | `:102` |
| `f2u8` | 0x2E | `SRC_0_X_X` | 0 | 0x7 | 0 | 0 | `:103` |
| `b2f32` | 0x5D | `SRC_0_X_X` | 0 | 0x6 | 0 | 0 | `:104` |
| `b2i32` | 0x5D | `SRC_0_X_X` | 0 | 0x6 | 0 | 0 | `:105` |
| `b2i16` | 0x5D | `SRC_0_X_X` | 0 | 0x5 | 0 | 0 | `:106` |
| `b2i8` | 0x5D | `SRC_0_X_X` | 0 | 0x2 | 0 | 0 | `:107` |
| `uclz` | 0x58 | `SRC_X_X_0` | 0 | 0x6 | 0 | 0 | `:108` |
| `iadd` | 0x1 | `SRC_0_X_1` | 0 | 0x1 | 0 | 0 | `:110` |
| `iadd_sat` | 0x3B | `SRC_0_X_1` | 0 | 0x1 | 0 | 0 | `:111` |
| `uadd_sat` | 0x3B | `SRC_0_X_1` | 0 | 0x6 | 0 | 0 | `:112` |
| `isub_sat` | 0x3B | `SRC_0_X_1` | 0 | 0x1 | 0 | 0 | `:113` |
| `usub_sat` | 0x3B | `SRC_0_X_1` | 0 | 0x6 | 0 | 0 | `:114` |
| `imod` | 0x48 | `SRC_0_1_X` | 0 | 0x5 | 0 | 0 | `:115` |
| `imul` | 0x3C | `SRC_0_1_X` | 0 | 0x1 | 0 | 0 | `:116` |
| `imul_high` | 0x40 | `SRC_0_1_X` | 0 | 0x1 | 0 | 0 | `:117` |
| `umul_high` | 0x40 | `SRC_0_1_X` | 0 | 0x6 | 0 | 0 | `:118` |
| `ineg` | 0x1 | `SRC_X_X_0` | 0 | 0x1 | 0 | 0 | `:120` |
| `iabs` | 0x57 | `SRC_X_X_0` | 0 | 0x1 | 0 | 0 | `:121` |
| `isign` | 0x27 | `SRC_X_X_0` | 0 | 0x1 | 0 | 0 | `:122` |
| `imin` | 0xF | `SRC_0_1_0` | 0x1 | 0x1 | 0 | 0 | `:123` |
| `imax` | 0xF | `SRC_0_1_0` | 0x2 | 0x1 | 0 | 0 | `:124` |
| `umin` | 0xF | `SRC_0_1_0` | 0x1 | 0x6 | 0 | 0 | `:125` |
| `umax` | 0xF | `SRC_0_1_0` | 0x2 | 0x6 | 0 | 0 | `:126` |
| `idiv` | 0x44 | `SRC_0_1_X` | 0 | 0x5 | 0 | 0 | `:127` |
| `udiv` | 0x44 | `SRC_0_1_X` | 0 | 0x3 | 0 | 0 | `:128` |
| `b32csel` | 0xF | `SRC_0_1_2` | 0xB | 0x6 | 0 | 0 | `:130` |
| `bcsel` | 0xF | `SRC_0_1_2` | 0xB | 0x6 | 0 | 0 | `:131` |
| `feq32` | 0x31 | `SRC_0_1_X` | 0x5 | 0x0 | 0 | 0 | `:133` |
| `fneu32` | 0x31 | `SRC_0_1_X` | 0x6 | 0x0 | 0 | 0 | `:134` |
| `fge32` | 0x31 | `SRC_0_1_X` | 0x3 | 0x0 | 0 | 0 | `:135` |
| `flt32` | 0x31 | `SRC_0_1_X` | 0x2 | 0x0 | 0 | 0 | `:136` |
| `flt` | 0x31 | `SRC_0_1_X` | 0x2 | 0x0 | 0 | 0 | `:137` |
| `ieq32` | 0x31 | `SRC_0_1_X` | 0x5 | 0x1 | 0 | 0 | `:138` |
| `ine32` | 0x31 | `SRC_0_1_X` | 0x6 | 0x1 | 0 | 0 | `:139` |
| `ige32` | 0x31 | `SRC_0_1_X` | 0x3 | 0x1 | 0 | 0 | `:140` |
| `ilt32` | 0x31 | `SRC_0_1_X` | 0x2 | 0x1 | 0 | 0 | `:141` |
| `ilt8` | 0x31 | `SRC_0_1_X` | 0x2 | 0x2 | 0 | 0 | `:142` |
| `uge32` | 0x31 | `SRC_0_1_X` | 0x3 | 0x6 | 0 | 0 | `:143` |
| `ult32` | 0x31 | `SRC_0_1_X` | 0x2 | 0x6 | 0 | 0 | `:144` |
| `ult` | 0x31 | `SRC_0_1_X` | 0x2 | 0x6 | 0 | 0 | `:145` |
| `ior` | 0x5C | `SRC_0_X_1` | 0 | 0x6 | 0 | 0 | `:147` |
| `iand` | 0x5D | `SRC_0_X_1` | 0 | 0x6 | 0 | 0 | `:148` |
| `ixor` | 0x5E | `SRC_0_X_1` | 0 | 0x1 | 0 | 0 | `:149` |
| `inot` | 0x5F | `SRC_X_X_0` | 0 | 0x1 | 0 | 0 | `:150` |
| `ishl` | 0x59 | `SRC_0_X_1` | 0 | 0x1 | 0 | 0 | `:151` |
| `ishr` | 0x5A | `SRC_0_X_1` | 0 | 0x1 | 0 | 0 | `:152` |
| `ushr` | 0x5A | `SRC_0_X_1` | 0 | 0x6 | 0 | 0 | `:153` |

**注意**：表里的 `type` 只是默认值，`mwv207_emit_alu` 会按实际位宽**覆盖**一部分（`iabs/imin/imax/iadd_sat/ieq32/ine32/ilt32/ige32/…` → `int`；`iand/uadd_sat/ult32/uge32/…` → `uint`；`i2i32/i2f32/u2u32/u2f32/usub_sat/isub_sat` → 由 `src_bitsize` 细化），见 §5.8。

### 6.5 cond / round / ext 取值汇总

| 字段 | 值 | 出现处（NIR op 或 emit 层） |
|---|---|---|
| cond | `0x1` | `fmin/fmax/imin/imax/umin/umax`（min） |
| | `0x2` | 同上（max）；`flt32/flt/ilt32/ilt8/ult32/ult`（`0x31` 的 lt） |
| | `0x3` | `fge32/ige32/uge32`（ge） |
| | `0x5` | `seq/feq32/ieq32`（eq） |
| | `0x6` | `sne/fneu32/ine32`（ne） |
| | `0xB` | `fcsel/b32csel/bcsel`（条件选择）；`emit` 层的 `jump_with_nz`（`mwv207_emit.c:487`）与 `discard`（`:529`） |
| | `0xA` | `emit` 层的无条件/条件 `jump`（`mwv207_emit.c:508`） |
| round | `0x1` | `fround_even`（round-to-even）；`mwv207_assemble` 用 `out[1] |= round`（`mwv207_common.c:263-326`） |
| ext（`opcode==0x7F`） | `0x1` | `emit_vertex`（`mwv207_emit.c:1600-1617`） |
| | `0x2` | `end_primitive`（`:1619-1635`） |
| | `0x4` | `txl`（lod 查询，`:457-482`） |
| | `0xB` | `find_lsb` |
| | `0xC` | `ifind_msb/ufind_msb` |
| | `0xE` | `txf_ms`（`:457-482`） |

### 6.6 **未确认 / 缺失依赖**（重要）

`INST_OPCODE_*`、`INST_TYPE_*`、`INST_CONDITION_*`、`INST_ROUND_MODE_*`、`INST_EXT_OPCODE_*` 这些宏在**本开源树中没有任何定义**（全树 `grep` 无 `#define INST_OPCODE_`）：

- `include/hw/mwv207reg_shader.h` 是**空壳**（仅 30 行：版权头 + `#ifndef MWV207REG_SHADER_H` 守卫，内部无内容）；`include/hw/` 下另外三个头（`mwv207reg_3d.h`/`mwv207reg_common.h`/`mwv207reg_texture.h`）同样只有 30/33/30 行的空壳。
- `nir/meson.build`（实际是 `mwv207/meson.build:86-108` 的 `mwv207_compiler` 可执行目标）与 `files_libmwv207`（`:30-84`）都**没有**生成 ISA 头的规则；`src/mesa/src/gallium/drivers/etnaviv/hw/isa.xml.h` 中存在同名体系宏（`#define INST_TYPE_F32 0x00000000`，该文件 `:200`；`INST_OPCODE_ADD` 亦见其中）。
- 结论：mwv207 的指令编码常量头（与 etnaviv 的 `isa.xml` → `isa.xml.h` 同机制生成）**未随本开源包发布**；因此 §6.4 表中 `opcode` 字段的**厂商语义**没有权威来源。

**但是**——**这不影响本树编译**，因为 `INST_OPCODE_*` 等宏的全部出现位置都在**从未被调用的宏体内部**：

| 位置 | 宏 | 是否被使用 |
|---|---|---|
| `mwv207_opcode.c:42-63` | 15 个 `OPCT/OPC/IOPC/UOPC/U16_OPC/I16_OPC/EIOPC/EUOPC/OP/IOP/UOP/EIOP/EUOP/U16_OP/I16_OP` | **否**（表体 `:64-153` 全部写成显式结构体字面量） |
| `mwv207_disasm.c:1022-1052` | 31 个 `OPC_NO_OPND/…/OPC_EXT_LOAD` | **否**（表体 `:1054-1189` 全部写成 `[0x..] = {…}` 字面量） |

宏未展开 → 无需 `INST_OPCODE_*` 的定义 → 本驱动 NIR 后端**不依赖**该缺失头（`nir/` 目录实际使用的 `INST_*` 记号只有 `INST_SWIZ_*`，它们定义在 `mwv207_common.h:40-67`；`MWV207_*` 记号全部来自 `mwv207_common.h` 的枚举与 winsys 头）。**表格中 `opcode` 数值的厂商含义仍属未确认**，但下列交叉印证可显著收窄不确定性。

### 6.7 与反汇编器名称表的交叉印证（`mwv207_disasm.c`）

`mwv207_disasm.c:1015-1190` 的 `opcs[(1<<7) + 0x15]`（索引即硬件 opcode，0x7F 起再加 ext）是一份**独立的硬件 opcode 名称表**，其表体是显式字面量（不依赖缺失头），因此可作为 §6.4 数值的第二来源。逐条比对（左侧为 §6.4 中的 opcode）：

| opcode | disasm 名称（`mwv207_disasm.c`） | 与 §6.4 的对应 | 是否印证 |
|---|---|---|---|
| 0x1 | `ADD` | `fadd`/`iadd`/`ineg` | ✔ |
| 0x2 | `MAD` | `ffma` | ✔ |
| 0x3 | `MUL` | `fmul` | ✔ |
| 0x5/0x6 | `DP3`/`DP4` | `fdot3`/`fdot4` | ✔ |
| 0x7/0x8 | `DSX`/`DSY` | `fddx`/`fddy` | ✔ |
| 0x9 | `MOV` | `mov`/`fneg`/`fabs`/`fsat`/`fround_even` | ✔ |
| 0xA/0xB/0x56 | `MOVAR`/`MOVAF`/`MOVAI` | （emit 中 amode 相关） | — |
| 0xC/0xD | `RCP`/`RSQ` | `frcp`/`frsq` | ✔ |
| 0xF | `SELECT` | `fmin`/`fmax`/`imin`/`imax`/`umin`/`umax`/`fcsel`/`b32csel`/`bcsel`（cond 区分 min/max/sel） | ✔ |
| 0x10 | `SET` | `seq`/`sne`/`sge`/`slt` | ✔ |
| 0x11/0x12 | `EXP`/`LOG` | `fexp2`/`flog2` | ✔ |
| 0x13 | `FRC` | `ffract` | ✔ |
| 0x17 | `TEXKILL` | `discard`（`mwv207_emit.c:529`） | ✔ |
| 0x18 | `TEXLD` | tex/txb（`mwv207_emit.c:427-443`） | ✔ |
| 0x16 | `BRANCH` | `jump_with_nz`（`mwv207_emit.c:487`） | ✔ |
| 0x21-0x27 | `SQRT`/`SIN`/`COS`/`FLOOR`/`CEIL`/`SIGN` | `fsqrt`/`fsin`/`fcos`/`ffloor`/`fceil`/`fsign`/`isign` | ✔ |
| 0x2A | `BARRIER` | `barrier`（`mwv207_emit.c` intrinsic 表） | ✔ |
| 0x2C/0x2D/0x2E | `I2I`/`I2F`/`F2I` | 各 `i2*/u2*/f2*` | ✔ |
| 0x2F | `F2IRND` | （表外，未使用） | — |
| 0x31 | `CMP` | 全部 `feq32/…/ult` 比较 | ✔ |
| 0x32 | `LOAD` | `load_buffer`/`load_ubo`/`load_shared`（`mwv207_emit.c:931-1130`） | ✔ |
| 0x33 | `STORE` | `store_buffer`/`store_shared` | ✔ |
| 0x34/0x35 | `IMG_LOAD_3D`/`IMG_STORE_3D` | 多维图像访问（`mwv207_emit.c:1187/1232`） | ✔ |
| 0x36 | `CLAMP0_MAX` | （表外，`convert_clamp0_max` 专用） | — |
| 0x37/0x38 | `IMG_ADDR`/`IMG_ADDR_3D` | 图像原子/加载的地址计算 | ✔ |
| 0x39/0x3A | `LOADP`/`STOREP` | pack 型 ld/st（`is_pack_load_store`） | ✔ |
| 0x3B | `IADDSAT` | `iadd_sat`/`uadd_sat`/`isub_sat`/`usub_sat` | ✔ |
| 0x3C | `IMULLO0` | `imul` | ✔ |
| 0x40 | `IMULHI0` | `imul_high`/`umul_high` | ✔ |
| 0x42 | `STORE_ATTR` | `store_gs`/`store_tess`/TCS fix_start/fix_end（`mwv207_emit.c:810/831/1960/1992`） | ✔ |
| 0x43 | `SELECT_MAP` | select-map 路径（`mwv207_emit.c:633`） | ✔ |
| 0x44/0x48 | `IDIV0`/`IMOD0` | `idiv`/`udiv`/`imod` | ✔ |
| 0x46 | `IMG_ATOM` | 图像原子 | ✔ |
| 0x49 | `TEXLD_U_S_L` | `txf`（`mwv207_emit.c:427-443`） | ✔ |
| 0x50/0x4E/0x52/0x4C | `IMADHI0`/`IMADLOSAT0`/`IMADHISAT0`/`IMADLO0` | 图像层地址计算用的 `0x4C` | ✔ |
| 0x54/0x55 | `BIT_INSERT1`/`BIT_INSERT2` | `bitfield_insert`(0x54) | ✔ |
| 0x57-0x5F | `IABS`/`LEADZERO`/`LSHIFT`/`RSHIFT`/`ROTATE`/`OR`/`AND`/`XOR`/`NOT` | `iabs`/`uclz`/`ishl`/`ishr`·`ushr`/`ior`/`iand`/`ixor`/`inot`（与 §6.4 的 0x5C/0x5D/0x5E/0x59/0x5A 一致） | ✔ |
| 0x60 | `BIT_EXTRACT` | `extract_u16`/`ibitfield_extract`/`ubitfield_extract` | ✔ |
| 0x61 | `POPCOUNT` | （表外） | — |
| 0x63 | `ARCTRIG` | （表外） | — |
| 0x64 | `DIV` | `fdiv` | ✔ |
| 0x65-0x6C | `ATOM_ADD/XCHG/CMP_XCHG/MIN/MAX/OR/AND/XOR` | `mwv207_emit.c:953-983` 的原子映射 | ✔ |
| 0x6D-0x6E | `BIT_REV`/`BYTE_REV` | `bitfield_reverse` | ✔ |
| 0x6F | `TEXLDLPCF` | `txl`（`mwv207_emit.c:457-482`） | ✔ |
| 0x70 | `TEXLDGPCF` | （shadow 采样 PCF） | — |
| 0x71 | `PACK` | `PACK_INST` 类 | ✔ |
| 0x72 | `CONV` | `CONV_INST` 类 | ✔ |
| 0x73 | `DP2` | `fdot2` | ✔ |
| 0x78 | `LOAD_ATTR` | `load_patch`(`mwv207_emit.c:549`)/`load_perVertex` | ✔ |
| 0x79/0x7A | `IMG_LOAD`/`IMG_STORE` | `mwv207_emit.c:1187/1232` | ✔ |
| 0x7B/0x7C/0x7D | `TEXLD_U`/`LODQ_G`/`TEXLD_GATHER` | `tg4`(0x7D，`mwv207_emit.c:427-443`) | ✔ |
| 0x7F+0x1 | `EMIT` | `emit_vertex` | ✔ |
| 0x7F+0x2 | `RESTART` | `end_primitive` | ✔ |
| 0x7F+0x3 | `FLUSH` | （表外） | — |
| 0x7F+0x4 | `LODQ` | `txl`（emit 用 ext 0x4） | ✔ |
| 0x7F+0xB/0x7F+0xC | `BIT_FINDLSB`/`BIT_FINDMSB` | `find_lsb`/`ifind_msb`/`ufind_msb` | ✔ |
| 0x7F+0xD/0x7F+0xE | `TEXLD_FETCH_MS_S`/`TEXLD_FETCH_MS_U` | `txf_ms`（emit 用 ext 0xE） | ✔ |

**结论**：§6.4 的全部 opcode 数值都能在 `mwv207_disasm.c` 的名称表中找到语义自洽的解释（仅 `0x2F F2IRND`、`0x36 CLAMP0_MAX`、`0x61 POPCOUNT`、`0x63 ARCTRIG`、`0x7F+0x3 FLUSH` 5 个硬件 opcode 在 NIR 表中没有对应条目，属于“硬件有、NIR 侧未使用”）。因此“**数值语义**”已由驱动内部两份表互证，未确认的仅是**厂商手册/寄存器文档层面的命名与位域定义**。

**另注（可能是缺陷）**：`mwv207_disasm()`（`:1244-1254`）断言 `sizedwords % 2 == 0`，但循环步长是 **4**（`i += 4`）——该断言应为 `% 4`，否则 `sizedwords ≡ 2 (mod 4)` 时会越界读取。

---

## 7. 寄存器分配（mwv207_ra.c，856 行）

### 7.1 算法与框架

| 项 | 内容 | 行号 |
|---|---|---|
| 框架 | Mesa `util/register_allocate.h`（`mwv207_ra.c:27` include）→ **图着色**（不是线性扫描） | — |
| 图构建 | `ra_alloc_interference_graph(regs, num_nodes)` → `ra_add_node_interference` → `ra_set_node_class` / `ra_set_node_reg` → `ra_allocate` | `:600`、`:782`、`:653`、`:772`、`:786` |
| 溢出选择 | `ra_set_node_spill_cost(g, i, 1.0)` + `ra_get_best_spill_node(g)` | `:505-510` |
| 寄存器空间 | `ra_alloc_reg_set(NULL, 0x40 * NUM_REG_TYPES, false)` = **1408** 个节点（`NUM_REG_TYPES == 22`） | `:802-803`、`:82` |
| 类 | `NUM_REG_CLASSES == 7`，每个类 `ra_alloc_reg_class`，再按 `reg_get_class(r)` 把 1408 个物理 reg 分派到类 | `:56`、`:805-810` |
| 类内冲突 | 同一 base（`r`）、不同 type 之间若 `reg_writemask[i] & reg_writemask[j]` 则加冲突边（即**分量重叠的虚拟寄存器不能共用一个物理 VR**） | `:812-817`、`:85-109` |
| 优先级队列 | `ra_set_finalize(regs, q_values)`，`q_values[7][7]` 常量表 | `:819`、`:36-44` |

`enum` 类与 type（`:46-83`）：

| 寄存器类 | 对应 type |
|---|---|
| `REG_CLASS_VIRT_SCALAR` | `REG_TYPE_VIRT_SCALAR_{X,Y,Z,W}` |
| `REG_CLASS_VIRT_VEC2` | `REG_TYPE_VIRT_VEC2_{XY,XZ,XW,YZ,YW,ZW}` |
| `REG_CLASS_VIRT_VEC3` | `REG_TYPE_VIRT_VEC3_{XYZ,XYW,XZW,YZW}` |
| `REG_CLASS_VEC4` | `REG_TYPE_VEC4` |
| `REG_CLASS_VIRT_VEC2T` | `REG_TYPE_VIRT_VEC2T_{XY,ZW}`（**T** = 走 tex/amode 通道的 2 分量类） |
| `REG_CLASS_VIRT_VEC2C` | `REG_TYPE_VIRT_VEC2C_{XY,YZ,ZW}`（**C** = compact/紧凑） |
| `REG_CLASS_VIRT_VEC3C` | `REG_TYPE_VIRT_VEC3C_{XYZ,YZW}` |

**虚拟寄存器编号约定**：`virt_reg = base * NUM_REG_TYPES + type`；`reg_get_type(virt) = virt % NUM_REG_TYPES`（`:171-175`），`reg_get_base()` 把 base 映射到 stage 各自的重叠窗口（见 §7.2）。三张 type→属性表：`reg_writemask[]`（`:85-109`）、`reg_swiz[]`（源用，`:111-135`）、`reg_dst_swiz[]`（目的用，`:137-161`）；`input_swiz[]` 专供 `load_per_vertex_input` 的 component（`:163-169`）。

### 7.2 物理基址映射 `reg_get_base()`（`:177-205`）

```c
static int reg_get_base(struct mwv207_compile *c, int virt_reg)
```

| stage | 返回 | 行号 |
|---|---|---|
| VS | `virt_reg / NUM_REG_TYPES` | `:183-184` |
| FS | `(virt_reg / NUM_REG_TYPES + 1) % 0x40` | `:185-186` |
| GS | `(virt_reg / NUM_REG_TYPES + c->nir->num_outputs + offset) % 0x40`，`offset=1` 若 `gs_use_primitive_id || gs_use_invocation_id || gs_use_layer` | `:187-194` |
| TCS | `(virt_reg / NUM_REG_TYPES + reserve + 1) % 0x40`，`reserve = c->nir->info.patch_outputs_read ? 9 : 5` | `:181`、`:195-197` |
| TES | `(virt_reg / NUM_REG_TYPES + 5 + 1) % 0x40` | `:198-200` |
| CS / KERNEL | `(virt_reg / NUM_REG_TYPES + 3) % 0x40` | `:201-202` |
| 其它 | `virt_reg / NUM_REG_TYPES` | `:203-204` |

> 这些 +1/+3/+5/+6/+9 的偏移量把「输入寄存器窗口」与「临时寄存器窗口」错开；**未确认**是否有硬件文档依据（注释中没有说明）。

### 7.3 节点类别选择（`:602-654`）

| 条件 | 类别 | 行号 |
|---|---|---|
| 默认 | `dest->num_components - 1`（0→SCALAR、1→VEC2、2→VEC3、3→VEC4 的自然类） | `:605`、`:653` |
| `alu` 且 op ∈ `{fdiv, flog2, fsin, fcos}` | `REG_CLASS_VIRT_VEC2T` | `:607-618` |
| `load_ubo` 或 `load_global_amd` 且 2 分量 | `REG_CLASS_VIRT_VEC2C` | `:623-628` |
| `load_ubo` 或 `load_global_amd` 且 3 分量 | `REG_CLASS_VIRT_VEC3C` | `:629-631` |
| `decl_reg` | `nir_intrinsic_num_components(intr) - 1` | `:633-634` |
| `load_per_vertex_input` | `REG_CLASS_VEC4` | `:636-637` |
| `load_ssbo` / `load_shared` | `REG_CLASS_VEC4` | `:638-640` |
| `image_deref_load` | `REG_CLASS_VEC4` | `:642-643` |
| （被 `#if 0` 屏蔽）`ssbo_atomic_comp_swap` / `shared_atomic_comp_swap` | `REG_CLASS_VIRT_VEC2` | `:645-650` |

`VIRT_VEC2T` 的存在与 `mwv207_emit_alu` 把 `fdiv/flog2/fsin/fcos/frsq/frcp/fexp2/fsqrt/imul` 的源与 `tex.amode` 复合（§5.8）相关；`VEC2C`/`VEC3C` 与 §5.7 `load_uniform` 的 amode 寻址相关。

### 7.4 预着色（`:656-774`）

对每条 intrinsic 用 `ra_set_node_reg` 把目的节点钉到确定的物理寄存器：

| intrinsic | 物理 reg 计算 | 行号 |
|---|---|---|
| `store_deref` | FS 且 `var->data.location == FRAG_RESULT_DEPTH` → `((0x40-1)*NUM_REG_TYPES + REG_TYPE_VIRT_SCALAR_Z)`；否则仅把节点类别设为 `REG_CLASS_VEC4` | `:666-675` |
| `load_input` | 先按 stage 修正 `intrinsic_base`：FS 且 `key.render_arrary` 时 LAYER→0、其余 +1；FS 且 `useClipDistance` 时 CLIP_DIST0/1 加 `src0*4 + component`；TES 加 `src0`；FS/VS 且 `src0` 常量且非 CLIP_DIST → 加 `src0`。然后：TES 且 `base < inner_outer_count` → `base*22 + SCALAR_X + component`；否则 `base*22 + {SCALAR_X, VEC2_XY, VEC3_XYZ, VEC4}[ncomp-1]` | `:676-720` |
| `load_vertex_id` | `infile.num_reg*22 + REG_TYPE_VIRT_SCALAR_X` | `:721-723` |
| `load_instance_id` | `infile.num_reg*22 + REG_TYPE_VIRT_SCALAR_Y` | `:724-726` |
| `ssbo_atomic_swap` / `shared_atomic_swap` / `image_deref_atomic_swap` | `infile.num_reg*22 + REG_TYPE_VIRT_VEC2_XY`（交换类原子需要同时返回旧值+写入新值） | `:728-732` |
| `load_deref` | 解 deref 链：array 且常量下标 → `idx/offset += index`（若 `!var->data.compact` 则 idx 重置为 SCALAR_X）；回溯到 `nir_deref_type_var`；1 分量时 `idx += location_frac`；`offset += driver_location`；`reg = (infile.num_reg + offset)*22 + {idx, VEC2_XY, VEC3_XYZ, VEC4}[ncomp-1]` | `:733-767` |

### 7.5 干涉边与溢出（图着色循环）

1. **干涉边**（`:776-784`）：对每对节点，若活区间 **相交**（`!(i.end <= j.start \|\| j.end <= i.start)`）则 `ra_add_node_interference(g, i, j)`；`assert(defs[i].live_start <= defs[i].live_end)`。
2. `ra_allocate(g)`（`:786`）。
3. 失败 → `mwv207_spill_reg()`（`:497-571`）：
   - 全部节点 spill cost 置 `1.0`，取 `ra_get_best_spill_node()`（`-1` 则 `assert(false)`）；
   - 若该 def 是「寄存器变量」的 `store_reg` 结果，则改从 `nir_store_reg_for_def(def)->src[1].ssa` 取真正定义；
   - 在定义前插入：**`decl_reg` 情形**把每个 `load_reg`/`store_reg` 用法就地换成 `nir_load_ssbo`/`nir_store_ssbo`（`buffer_index = 0x4`，`buffer_offset = c->shader->spill_offset`，`align_mul = 4`）并删除原指令；**普通情形**把每个 use（含 `if` 条件 use）重写为 `nir_load_ssbo`，并在定义之后插 `nir_store_ssbo`；
   - 每次溢出 `c->shader->spill_offset += 16`；
   - `debug & 0x100` 时打印 `choose spill instr: ` + `nir_print_instr`（`:520-524`）。
4. **重跑整个分配**：`while (!mwv207_ra(c, nir, regs, ctx->ws->debug)) { ralloc_free(c->g); ralloc_free(c->live_map); }`（`:821-824`）——即溢出是「改写 NIR → 重新编号 → 重新建图」的迭代过程，而非一次着色内换寄存器。
5. 若最终 `spill_offset != 0`，创建 `PIPE_BUFFER` 的 `spill_buffer` 资源（`bind |= PIPE_BIND_LINEAR`，`width0 = spill_offset`）（`:826-835`）。

### 7.6 常量与立即数（`mwv207_const()`，`:252-299`）

```c
struct mwv207_inst_src mwv207_const(struct mwv207_compile *c, nir_const_value *value, uint32_t num_components);
```

- 单分量且标识位为 `MWV207_IMMEDIATE_CONSTANT` 时优先编码为**内嵌立即数**：低 12 位为 0 → `imm_type 0`（float，存 `bits >> 12`）；`bits < (1<<20)` → `imm_type 2`（uint）；`bits >= 0xfff80000` → `imm_type 1`（int）（`:256-267`）。
- 否则在 `c->consts[]` 中按 4 项窗口查找/追加（`const_add`，`mwv207_common.c:250` 起，窗口内去重），成功则返回 `SRC_CONST(i-1, swiz)`，`swiz` 每 2 bit 记录分量在窗口内的槽位（`:269-298`）。
- 上限：`assert(i <= 0x800 / 4)`（512 项）；FS 额外 `assert(i <= 0x400 / 4)`（256 项）（`:291-294`）；`c->const_count = MAX2(c->const_count, i)`。

### 7.7 源操作数的特殊处理（`mwv207_src()`，`:308-456`）

**`nir_intrinsic_get_ssbo_size`（任务点名的特化）**：

```c
case nir_intrinsic_get_ssbo_size:
   return mwv207_const(c, &CONST_VAL(MWV207_IMMEDIATE_SSBO_SIZE, nir_src_as_int(intr->src[0])), 1);
```
（`mwv207_ra.c:405-406`）——SSBO 尺寸是**硬件常量描述符**，因此该 intrinsic 在 **emit 层是空实现**（`mwv207_emit.c:1640-1647`），真正的值只在被当作**源**读取时由 RA 层产出。

| NIR 源形式 | 返回 | 行号 |
|---|---|---|
| `BYPASS_SRC` 的 `mov` | 递归 `src_swizzle(mwv207_src(src0), ALU_SWIZ(&alu->src[0]))`（断言 dtype 为 `nir_op_mov`） | `:316-321` |
| `load_const` | `mwv207_const(...)` | `:324-325` |
| `load_tess_coord` | TES → `SRC_REG(0, SWIZZLE(X,Y,Z,X))` | `:330-333` |
| `load_input` / `load_deref` / `load_vertex_id` / `load_instance_id` / `load_uniform` / `load_reg` | `mwv207_reg_src()`（查 RA 结果 + `reg_swiz`） | `:334-342` |
| `load_ubo` | 若 `src[1]` 常量则据 `index / bytesize - 1` 置位 `c->shader->ubo_mask[index/64] \|= 1ull << (index%64)`，再 `mwv207_reg_src()` | `:343-353` |
| `load_per_vertex_input` | `SRC_REG(base, input_swiz[component])`（按 component 选 swizzle） | `:354-358` |
| `load_patch_vertices_in` | 常量 `MWV207_UNIFORM_PATCH_VERTEX_IN` | `:359-360` |
| `load_primitive_id` | GS：`!restart && !invocation_id → REG(0,Z)`；`restart && !invocation_id → REG(0,W)`；否则 `REG(1,X)`。TCS → `REG(0,Z)`；TES → `REG(1,Z)`；其它 → `compile_error` + `assert(0)` | `:361-379` |
| `load_invocation_id` | GS：`!restart → REG(0,Z)` 否则 `REG(0,W)`；TCS → `REG(0,Y)` | `:380-390` |
| `load_front_face` | `(struct mwv207_inst_src){ .use = 1, .rgroup = 0x1 }`（**无寄存器，走特殊 rgroup**） | `:391-392` |
| `load_frag_coord` | `SRC_REG(0, INST_SWIZ_IDENTITY)` | `:393-394` |
| `load_shared/load_ssbo/ssbo_atomic(_swap)/shared_atomic(_swap)/image_deref_atomic(_swap)` | `mwv207_reg_src()` | `:396-404` |
| `get_ssbo_size` | 见上 | `:405-406` |
| `load_workgroup_id` | `SRC_REG(0, IDENTITY)` | `:407-408` |
| `load_local_invocation_id` | `SRC_REG(1, IDENTITY)` | `:409-410` |
| `load_global_invocation_id(_zero_base)` | `SRC_REG(2, IDENTITY)` | `:411-413` |
| `load_num_workgroups` | 3 分量常量 `MWV207_IMMEDIATE_WORK_GROUP_COUNT`(0/1/2) | `:414-421` |
| `load_workgroup_size` | 3 分量常量 `MWV207_IMMEDIATE_WORK_GROUP_SIZE`(0/1/2) | `:422-429` |
| `atomic_counter_*` / `image_deref_load` / `image_deref_size` | `mwv207_reg_src()` | `:430-435` |
| 其它 intrinsic | `compile_error("Unhandled NIR intrinsic type: %s")` | `:436-439` |
| `alu` / `tex` | `mwv207_reg_src()` | `:442-444` |
| `undef` | `mwv207_const(CONST(0), 1)` 再广播 X | `:445-449` |
| 其它 instr 类型 | `compile_error("Unhandled NIR instruction type: %d")` | `:450-453` |

`mwv207_reg_src()`（`:301-306`）：`SRC_REG(reg_get_base(c, reg), reg_swiz[reg_get_type(reg)])`，其中 `reg = ra_get_node_reg(c->g, c->live_map[src_index(c->impl, src)])`。

### 7.8 目的与入口/出口

```c
struct mwv207_inst_dst mwv207_dest(struct mwv207_compile *c, nir_def *dest, uint32_t *p_swiz);  /* :459-478 */
struct mwv207_inst_src mwv207_src_from_dest(struct mwv207_compile *c, nir_def *dest);           /* :480-495 */
void mwv207_init_reg_state(struct mwv207_context *ctx, struct mwv207_compile *c, nir_shader *nir); /* :798-837 */
void mwv207_free_reg_state(struct mwv207_compile *c);                                            /* :839-856 */
```

- `mwv207_dest`：先 `real_dest(dest, &swiz, &mask)` 穿透 `BYPASS_DST`/`BYPASS_SRC` 与 pass_flags（`mwv207_common.c:135` 起）；`*p_swiz = inst_swiz_compose(swiz, reg_dst_swiz[t])`；`write_mask = inst_write_mask_compose(mask, reg_writemask[t])`。
- `mwv207_src_from_dest`：`reg_swiz[reg_get_type(t)]`，其中 `t` 已经是 `reg_get_type(r)` —— **多了一次取模运算（冗余但结果等价）**（`:487`、`:493`）。
- `mwv207_init_reg_state`：建 reg set/class/finalize（`:802-819`），然后 `while(!mwv207_ra(c, nir, regs, ctx->ws->debug))` 迭代（`:821-824`），最后按需建 spill buffer（`:826-835`）。`debug` 来自 winsys（`ctx->ws->debug`，环境变量 `mwv207_debug`，见 §11）。
- `mwv207_free_reg_state`：`num_temps = max(所有已分配 reg 的 reg_get_base+1 的最大值, infile.reg[i].reg+1)`（`:845-855`），随后释放 graph/regs/live_map。

**ASI 说明**：`mwv207_ra()` 之前会把所有指令的 `pass_flags` 低 2 位（`BITFIELD_RANGE(0,2)` = `BYPASS_DST|BYPASS_SRC`）清零（`:584-590`）——即每次重新分配前重置穿透标记，避免上一轮标记污染。

---

## 8. 活跃性分析（mwv207_liveness.c，257 行）

### 8.1 数据结构

```c
struct live_def { nir_instr *instr; nir_def *dest; uint32_t live_start, live_end; };  /* mwv207_common.h:249-253 */
struct live_defs_state { uint32_t num_defs, bitset_words; nir_function_impl *impl; nir_block *block;
                         uint32_t index; struct live_def *defs; uint32_t *live_map;
                         nir_block_worklist worklist; };                             /* mwv207_common.h:255-267 */
```

入口签名：

```c
uint32_t live_defs(nir_function_impl *impl, struct live_def *defs, uint32_t *live_map, bool has_store);  /* :133-256 */
bool is_decl_reg(nir_instr *instr);                                                                     /* :123-131 */
```

**设计要点**：RA 用的不是「每个指令点的 bitset 干涉」，而是**线性活区间** `[live_start, live_end]`（以“定义序号”为坐标），bitset 只用于 CFG 数据流求解：

- live 区间坐标 = **def 编号**（`live_map[dest_index]` 的值），而不是指令序号；
- 干涉判定退化为区间相交（`mwv207_ra.c:776-784`）；
- `has_store` 参数：GS/TCS 传 `true`（`mwv207_ra.c:596-597`），表示 `store_reg` 之后仍需跟踪（寄存器变量语义）。

### 8.2 三个阶段

| 阶段 | 行号 | 说明 |
|---|---|---|
| ① 编号 | `:143-176` | `nir_foreach_block` × `nir_foreach_instr`：`dest_for_instr(instr)` 取目的，跳过无目的与 `is_decl_reg`；`live_map[idx] != ~0u` 已编号则跳过；`defs[num_defs] = {instr, dest, num_defs, num_defs}`；对 `load_input/load_vertex_id/load_instance_id/load_primitive_id/load_invocation_id/load_tess_coord/load_frag_coord` 强制 `live_start = 0`（`:159-170`，因为这些值从 shader 入口即活）；`block_live_index[block->index] = num_defs`；末尾 `block_live_index[num_blocks] = num_defs`（`:176`） |
| ② 初始化 | `:178-183` | `nir_block_worklist_init`，`bitset_words = BITSET_WORDS(num_defs)`，逐块 `init_liveness_block()`：`reralloc` `live_in`/`live_out` 并 `memset` 清零、把自己压入 worklist（`:40-55`） |
| ③ worklist 求解 | `:185-241` | 见下 |

### 8.3 worklist 求解细节（`:185-241`）

1. `pop_head` → `state.block`；`live_in = live_out`（`memcpy`）。
2. `state.index = block_live_index[block->index + 1]`（**从块末尾的 def 序号开始**）。
3. `nir_block_get_following_if(block)` 有则先 `set_src_live(&following_if->condition)`（条件在块尾活）。
4. **逆序**遍历指令：
   - 若 `state.index && instr == defs[state.index - 1].instr` → `state.index--` 并 `BITSET_CLEAR(live_in, index)`（该定义此刻起死）；
   - `is_dead_instruction(instr)` → 跳过；
   - `intrinsic == store_deref && !has_store` → `state.index = ~0u`（**标记此点之后无定义存活**，用于 `nir_var_shader_out` 存储之后不再跟踪）；
   - `process` 判定：`decl_reg` 或 `store_reg` → 为 true（这两类**不**执行 `set_src_live`，因为寄存器变量的读写由 §7.5 溢出逻辑专门处理）；
   - `nir_foreach_def(instr, set_ssa_def_dead, &state)`：`dest_index` → `~0u` 则忽略，否则 `BITSET_CLEAR(live_in, i)`；
   - `!processed` 时 `nir_foreach_src(instr, set_src_live, &state)`：`BITSET_SET(live_in, i)` + `range_include(&defs[i], state.index)`（`range_include` 见 `:30-38`，扩张 `live_start/live_end`）；
   - 恢复 `state.index = index`（临时改成 `~0u` 只影响本条指令的 src 处理）。
   - 结束 `assert(state.index == block_live_index[block->index])`（`:233`）。
5. 对每个前驱 `propagate_across_edge(pred, block)`：`pred->live_out |= succ->live_in`，若有新位置加入则把前驱压回 worklist（`:109-121`、`:235-240`）。
6. 收尾：对每块 `BITSET_FOREACH_SET(live_in)` → `range_include(defs[i], block_live_index[block->index])`；对 `live_out` → `range_include(defs[i], block_live_index[block->index + 1])`（`:245-253`）。返回 `num_defs`。

### 8.4 `set_src_live` 的特殊分支（`:57-92`）

| 情况 | 行为 | 行号 |
|---|---|---|
| `is_sysval(instr)`（`load_front_face` / `load_frag_coord`，`mwv207_common.c:64-73`） | 直接返回（不占用 def 编号） | `:64-65` |
| `deref` / `load_const` / `undef` 定义 | 直接返回 | `:68-71` |
| `alu` 且 `instr->pass_flags & BYPASS_SRC` | 递归把该 `mov` 的**所有输入**当活（即穿透合并节点，与 `real_dest`/`mwv207_src` 的穿透一致），且不把自己计入 | `:72-80` |
| 其它 | `i = live_map[src_index(impl, src)]`；`assert(i != ~0u)`；`BITSET_SET(block->live_in, i)`；`range_include(&defs[i], state.index)` | `:85-91` |

> `assert(i != ~0u)` 意味着**任何被当作源使用、但没有被阶段①编号的定义都会触发断言**。阶段①对 `decl_reg`/无目的指令跳过编号，因此这类 SSA 值不能直接作为普通源出现——这正是 `BYPROCESS`（`decl_reg`/`store_reg` 屏蔽 `set_src_live`）存在的原因。

### 8.5 未确认

- `has_store` 之外是否还有其它调用点：全树只有 `mwv207_ra.c:596-599` 一处调用 `live_defs`（未确认）。
- `store_deref && !has_store` 把 `state.index` 置 `~0u` 后，**该块内更早的 def 仍会被编号**；这一点与 `range_include` 的交互（是否会导致某些 def 的 `live_end` 偏小从而少加干涉边、进而影响着色质量）**未确认**，代码无注释。

## 9. 反汇编器（mwv207_disasm.c 1317 行 / mwv207_disasm.h 328 行）

### 9.1 用途与导出 API

| 函数 | 签名 | 行号 |
|---|---|---|
| `mwv207_disasm` | `void mwv207_disasm(uint32_t *dwords, int sizedwords)` | `mwv207_disasm.c:1245-1254` |
| `mwv207_dump_shader_nir` | `void mwv207_dump_shader_nir(const struct mwv207_shader *shader)`（声明 `mwv207_compiler.h:192`） | `:1256-1317` |
| `mwv207_disasm.h` | 仅提供 `struct instr`（编码器视图 + 反汇编视图）、`enum kinds_of_inst`、`enum dst_addr_reg_type`、`struct mwv207_inst_ctrl`、`PRINT` 宏 | `mwv207_disasm.h:31-326` |

调用点：`mwv207_shader.c:1011`（`ctx->ws->debug & 0x1` 时）、`nir/mwv207_cmdline.c:140`。**因此反汇编器的作用是：① 驱动内 NIR 调试 dump 的最后一环；② 离线工具检查 `shader->code`；③ 与汇编器 `mwv207_assemble()` 互为逆运算（`struct instr` 的“编码器视图”就是汇编写入的 4 个 dword 的位域分解，见 §5.1）。**

### 9.2 输出格式

每行一条指令（4 个 dword）：

```
%04d: 0x%08x 0x%08x 0x%08x 0x%08x  <NAME><后缀...> <目的>, <src0>, <src1>, <src2>
```

（`convert_and_print_inst` `:1213-1241`）。列对齐：名称+后缀后，目的列补齐到 `0x1C` 宽、每个源补齐到 `0xC` 宽（`print_align` `:77-90`，长度由 `print_with_update_length` `:65-74` 累加）。

**后缀顺序**（`print_inst_crontrol` `:93-245`）：

| 顺序 | 后缀 | 触发条件 |
|---|---|---|
| 1 | `.gt/.lt/.ge/.le/.eq/.ne/.and/.or/.xor/.not/.nz/.gez/.gz/.lez/.lz/.fin/.inf/.nan/.nml/.anymsb/.allmsb/.selmsb/.uc/.hlp/.nothlp` | `cond_opcode`（`_str_condop[]` `:99-126`） |
| 2 | `.rtz`/`.rtne`（2 值）或 `.rtn/.rtz/.rtne/.rtp`（4 值） | `rounding_mode`，`is_has4_rounding_mode` 区分（`:128-139`、`:174-177`） |
| 3 | `.pack` | 基 opcode 为 `0x39`/`0x3A`（`:179-180`） |
| 4 | `.sat` | `is_result_sat`（`:182-183`） |
| 5 | `.skpHp` | `is_skip_for_helper_kickoff`（`:185-186`） |
| 6 | `.denorm` | `!is_denorm` 且 flags 命中（`:188-189`） |
| 7 | `.ebb` | `is_end_of_BB`（`:191-192`） |
| 8 | `.UA` | `is_unallocate`（`:194-195`） |
| 9 | `.local`、`.ls<n>`、`.x3` | 内存访问类（`:197-210`） |
| 10 | `.rg<n>`、`.compSel` | opcode `0x43`（`:211-215`） |
| 11 | `.VS/.TCS/.TES/.GS`（+`.ild/.lnr`） | opcode `0x78`/`0x42`（`:219-225`） |
| 12 | `.ss<n>` | opcode `0x71`（`:226-228`） |
| 13 | `.restart`/`.JmpToEnd` | `0x7F` + ext `0x1`（`:229-234`） |
| 14 | `.infX0To0` | flags bit1（`:235-238`） |
| 15 | 类型名（`.f32/.f16/.s32/...`） | `inst_type`；当 `inst_type == 0` 时只有特定 flags 才打印（`:240-244`） |

**目的操作数**（`print_inst_dst` `:248-304`）：寄存器字母 `r`/`a`/`b`（由 flags bit3/bit4 决定 A0/B0）、寄存器号 = `reg | reg_bit7<<7 | reg_bit8<<8`（9 位）、动态索引 `[a0.x]…[b0]`、写掩码 `.x/.y/.xy/...`（16 项表 `:255-260`）；`dst.use == 0` 时打印 `void,`，且若 `write_mask` 非 0 会先打印掩码（`:273-281`）。

**源操作数**（`print_each_src` `:364-491`）：`use == 0` → `void`；`rgroup == 0x7` → 立即数按 `imm_type` 打印（0: `%f`（`imm_val<<12` 还原）、1: 符号扩展 `%d`、2: `%u`、3: 打包 `0x%X(0x%x)`）；`src->neg`/`abs` 用 `-`/`|…|`；寄存器形式 `rN[idx].swiz`（`rgroup` 名称表 `{r, vFace, c, c, r, Ext, L, I}` `:370-379`），`rgroup == 0x5` 且 `reg == 1/2` 或 `0x10..0x13` 时打印 `_str_inst_index[]`（`instanceId`/`in_primitiveId`/`out_primitiveId`/`remap0..5` 等 `:381-402`）；SAMPLE 类指令的第 0 个源打印为采样器 `s%u`（`:475-476`），并额外插入 `sampler_aux_src`（`print_inst_srcs` `:494-507`）。

### 9.3 解码流程与内部结构

| 阶段 | 函数 | 行号 | 说明 |
|---|---|---|---|
| 取 opcode | `convert_and_print_inst` | `:1193-1241` | `opcode = base_opcode_bit0_5 \| (base_opcode_bit6 << 6)`；若为 `0x7F` 则 `opc = opcode + ext_opcode`（`ext_opcode` 默认 `0xFFFFFFFF` 表示无 ext）；`opc == 0x45` 时报 `"Not support EVIS OPCODE for now"`（`:1208-1209`） |
| 名称 | `opcs[opc].name` | `:1211`/`:1218-1239` | 查不到打印 `unknown (%u)`（用的是 `inst->opc`） |
| 转换 | `opcs[opc].convert` = `convert_inst` / `convert_branch` / `convert_call` / `convert_clamp0_max` | `:895-1013` | `convert_branch`：`src2` 有效且 `src2_type == 0x7` → `DIRECT_BRANCH1_INST`，否则 `INDIRECT_BRANCH_INST`（`:980-993`）；`convert_call`：`src2` 有效 → `INDIRECT_CALL_INST`（`:996-1002`） |
| 源解码 | `decode_srcs` / `decode_check_each_src` / `decode_each_src` | `:596-635`、`:552-593` | 按表项 `src_mask` 的 bit0/1/2 过滤（对应 src0/1/2）；填 `use/rgroup/reg/amode/swiz/abs/neg`；`rgroup == 0x2` → `is_const_reg = true`；`rgroup == 0x7` → `decode_imm` |
| 立即数还原 | `decode_imm` | `:525-549` | `imm_val = reg_no \| swizzle<<9 \| mod_neg<<17 \| mod_abs<<18 \| (rel_addr_bit0)<<19`，`imm_type = rel_addr >> 1` —— **与 `mwv207_immediate_src()`（`imm_val:20` + `imm_type:2`）布局一致**（§5.2） |
| 类型 | `decode_inst_type` | `:637-650` | `bit0 \| (bit1_2 << 1)`，`LOAD/STORE` 再加 `bit3` |
| 控制字段 | `decode_inst_ctrl` → `decode_inst_ctrl_alu_inst` / `decode_inst_ctrl_load_store_inst` | `:722-811`、`:653-679`、`:682-720` | 按 `kind_of_inst` 分派；`CMPLX/SCATTER` 直接 `assert(false)`（`:803-806`），`0x45` 分支也 `assert(false)`（`:216-218`） |
| 分支目标 | `decode_direct_branch_target_src` | `:867-892` | `DIRECT_BRANCH0/LOOP` 用 20 位 `branch_target_or_call_target`；`DIRECT_BRANCH1` 用 `src2_branch_target_bit0_8 \| bit9_18<<9 \| bit19<<19` |
| 条件源计数 | `_cond_op_2_src_count[]` | `:35-62` | 26 项，决定条件分支“几个源是操作数、下一个才是跳转目标”（`convert_inst` `:906-911`、`:940-950`） |
| 伪 opcode | `map_sample_hw_opcode_to_aux_opcode` / `map_ld_st_hw_opcode_to_aux_opcode` | `:813-846`、`:848-865` | 把带辅助源/目的的同名硬件 opcode 映射到 `0xFFFF00nn` 伪 opcode，用于打印时区分形态（例：`0x18 TEXLD` → `+2/+3/+4/+5`；`0x6F` → `+0/+1`；`0x7A` → `0xFFFF0021`） |

**名称总表** `opcs[(1 << 0x7) + 0x15]`（149 项，索引 = 硬件 opcode、`0x7F` 后加 ext）位于 `:1021-1190`；表体为显式 `[0x..] = {…}` 字面量，且**每项携带 `{name, convert, kind_of_inst, flags, src_mask}` 五元组**——`flags` 决定哪些控制字段参与打印（如 `1U<<0` 内存访问、`1U<<2` 图像、`1U<<9` 饱和可用、`1U<<3/4` A0/B0 目的寻址）。名称到 NIR 的对应关系见 §6.7。

### 9.4 已知缺陷 / 不一致（逐条有行号）

| # | 问题 | 证据 | 影响 |
|---|---|---|---|
| 1 | **类型名表与编译器类型编码不一致** | `_str_inst_type[]`（`:141-158`）下标语义：`0=.f32, 1=.f16, 2=.s32, 3=.s16, 4=.s8, 5=.u32, 6=.u16, 7=.u8`；而 `get_alu_inst_type()`（`mwv207_emit.c:205-232`）与 §6.4 表用的是 `0=f32, 1=i32, 2=i8, 3=u16, 4=f16, 5=i16, 6=u32, 7=u8` | 除 `type == 0` 外，反汇编打印的类型名**全部错误**（例：`iadd`（表内 type `0x1`）会被打印为 `.f16`）；哪一侧代表真实硬件**未确认** |
| 2 | 断言与步长不匹配 | `mwv207_disasm()` 断言 `sizedwords % 2 == 0`（`:1249`）但循环 `i += 4`（`:1251`） | `sizedwords ≡ 2 (mod 4)` 时越界读；断言应为 `% 4` |
| 3 | FS 颜色输出打印在遇到第一个 0 时截断 | `for (idx = 0; idx < 8 && shader->ps_color_out_reg[idx] != 0; idx++)`（`:1309-1310`） | RT1 未写（reg=0）而 RT2 有值时，RT2 不会被打印 |
| 4 | 部分指令族无解码路径 | `CMPLX_INST`/`SCATTER_INST` → `assert(false)`（`:806`、`:923-925`）；`0x45`（EVIS）→ stderr 提示（`:1208-1209`） | 遇到这些指令会崩溃或只打印 `unknown`；`CONV_INST` 有控制字段解码（`:792-801`）但 `convert_inst` 不产生操作数 |
| 5 | `PRINT` 缓冲区固定 128 字节 | `print_with_update_length`（`:67`） | 超长输出被截断（对齐计算随之失真） |

### 9.5 `mwv207_dump_shader_nir()` 的输出内容

| 段 | 内容 | 行号 |
|---|---|---|
| 头 | `<stage> code:`（`gl_shader_stage_name(shader->stage)`） | `:1266` |
| 反汇编 | `mwv207_disasm(shader->code, shader->code_size)` | `:1268` |
| `num temps` | `shader->num_temps` | `:1270` |
| `immediates` | 每项 `[i/4].x|y|z|w = <float 值> (0x%08x) (<imm_contents 枚举>)` | `:1271-1280` |
| `inputs` | `[reg] name=<slot 名> comps=<n>`；VS 用 `gl_vert_attrib_name()`，其它 stage 用 `gl_varying_slot_name_for_stage()` | `:1282-1290` |
| `outputs` | 同上；FS 用 `gl_frag_result_name()` | `:1292-1300` |
| `special` | 非 FS：`vs_pos_out_reg`/`vs_pointsize_out_reg`/`vs_load_balancing`（0x%08x）；FS：`ps_color_out_reg`（≤8，见 §9.4 #3）与 `ps_depth_out_reg` | `:1302-1313` |
| key | `key : 0x%x`（`shader->key.global`） | `:1315` |

---

## 10. Shader state 层（`mwv207_shader.c` 1198 行 + `include/mwv207_shader.h` 105 行）

### 10.1 `struct mwv207_shader_state`（`include/mwv207_shader.h:42-57`）

| 字段 | 类型 | 作用 |
|---|---|---|
| `nir` | `struct nir_shader *` | 源 NIR（由 `mwv207_tgsi_to_nir` 或调用方直接提供），`delete` 时 `ralloc_free` |
| `shader_list` | `struct mwv207_shader *` | **已编译变体链表**（按 `mwv207_shader_key` 区分；头插） |
| `current` | `struct mwv207_shader *` | 最近选中的变体（`mwv207_set_current_shader` 的返回值即“是否发生变化”） |
| `bo` | `struct mwv207_bo *` | **着色器指令二进制所在 BO**（多条变体共享一段、按 `bo_offset` 顺序排布） |
| `bo_offset` | `uint32_t` | 下一条变体的写入偏移 |
| `base_stream_output` | `struct pipe_stream_output_info` | 由 `pipe_shader_state.stream_output` 保存 |
| `stream_output` | `struct pipe_stream_output_info` | 编译后由 `mwv207_link_shader` 写入的结果 |

配套 inline 工具：`mwv207_shader_state(void *)`（`:59-63`）、`mwv207_config_arrays(uint32_t *arrays, uint32_t bit_size, uint32_t index, uint32_t value)`（`:65-74`，把 `value` 按 `bit_size` 位宽拼进寄存器配置数组，`assert(value < 1 << bit_size)`）。

### 10.2 接口清单（`mwv207_context_shader_init` 注册的全部钩子）

| 钩子 | 签名 | 行号 | 行为 |
|---|---|---|---|
| create（vs/fs/gs/tcs/tes 共用） | `static void *mwv207_create_shader_state(struct pipe_context *pctx, const struct pipe_shader_state *pshader)` | `:1018-1038` | `type != PIPE_SHADER_IR_NIR` → `mwv207_tgsi_to_nir(screen, pshader->tokens)`，否则直接用 `pshader->ir.nir`；`CALLOC_STRUCT`；`bo=NULL`、`shader_list=NULL`、`base_stream_output = pshader->stream_output`；**立即 `mwv207_set_current_shader(ctx, state)` 触发首次编译**（`:1035`） |
| create（compute） | `static void *mwv207_create_cs_state(struct pipe_context *pctx, const struct pipe_compute_state *pshader)` | `:1112-1138` | TGSI → `mwv207_tgsi_to_nir`；`PIPE_SHADER_IR_NIR_SERIALIZED` → `blob_reader_init` + `nir_deserialize(NULL, &mwv207_nir_options_VSGSFS, &reader)`（`:1121-1127`）；否则按 NIR 直接用；同样立即 `mwv207_set_current_shader`（`:1135`） |
| bind | `static void mwv207_bind_{vs,fs,gs,tcs,tes,cs}_state(struct pipe_context *pctx, void *state)` | `:1040-1048`、`:1050-1058`、`:1060-1068`、`:1070-1078`、`:1080-1088`、`:1159-1167` | 写 `ctx->shader_states[PIPE_SHADER_*] = state` 并 `ctx->dirty \|= MWV207_DIRTY_SHADER` |
| delete | `static void mwv207_delete_shader_state(struct pipe_context *pctx, void *pshader)` | `:1090-1110` | `ralloc_free(state->nir)`；遍历 `shader_list` 逐个 `mwv207_destroy_shader`；`ctx->ws->bo_reference(ctx->ws, &state->bo, NULL)`；`FREE(state)` |
| compute info | `static void mwv207_get_compute_state_info(struct pipe_context *ctx, void *state, struct pipe_compute_state_object_info *info)` | `:1140-1149` | `max_threads = 1024`、`preferred_simd_size = 32`、`simd_sizes = 8\|16\|32`、`private_memory = 0` |
| subgroup size | `static uint32_t mwv207_get_compute_state_subgroup_size(struct pipe_context *ctx, void *state, const uint32_t block[3])` | `:1151-1157` | **恒 `return 0`**（未实现 subgroup 语义） |
| 注册 | `void mwv207_context_shader_init(struct pipe_context *pctx)` | `:1169-1198` | 把上面 6 组 create/bind/delete + 2 个 compute 钩子挂到 `pipe_context` |

**注意**：vs/fs/gs/tcs/tes/compute 的 `delete_*_state` **全部指向同一个** `mwv207_delete_shader_state`（`:1174`、`:1178`、`:1182`、`:1186`、`:1190`、`:1194`）。

### 10.3 变体缓存与 key（`mwv207_set_current_shader`，`:933-1016`）

```c
bool mwv207_set_current_shader(struct mwv207_context *ctx, struct mwv207_shader_state *state); /* include/mwv207_shader.h:76-77 */
```

流程（按函数内部顺序）：

1. 依据 `ctx->info`（`dsbi.is_dual`、`tex_128bit`、`pixel_128bit`、`z32_shadow`、`is_tg4_lower` 等）、`rast_state`（`front_ccw`、`sprite_coord_enable`、`sprite_coord_mode`）与当前 stage 信息组装 `struct mwv207_shader_key`（位域见 §2.3）。
2. 在 `state->shader_list` 链表中用 `mwv207_shader_key_equal` 查找同 key 变体。
3. **命中**：若 `tex_128bit` / `pixel_128bit` / `z32_shadow` / `is_dual` 对应的 dirty 位置位 → 重新 `mwv207_compile_shader(ctx, state, false)`（`offline = false`，会写 `state->bo`）；返回是否变化。
4. **未命中**：`mwv207_create_shader(state, key)`（`:882-917`）新建变体并**头插**链表，随后编译。
5. `ctx->ws->debug & 0x1` 时 `nir_print_shader` + `mwv207_dump_shader_nir`（`:1011` 附近）。

**着色器二进制缓存要点**：

- 缓存键 = `struct mwv207_shader_key`（单一 `uint32_t` 位域，§2.3）；缓存粒度 = **每 `mwv207_shader_state` 一份变体链表**（即每个 GL program 对象各自一份），不是全局缓存。
- 产物存放：`mwv207_compile_shader(..., offline = false)` 会把指令写入 `state->bo`（`bo_size = code_size*4*16`，`bo_offset += code_size*4*2`，见 §5.5）；`mwv207_shader.code` 由 `mwv207_record_code` 指向该 BO 的映射地址。
- 变体只在 `delete_shader_state` 时随 state 一起释放；链表**没有淘汰/复用上限**（`shader_list` 只增不减）。
- `mwv207_destroy_shader`（`:919-931`）释放 `spill_buffer`、`code`、`imm_data`、`imm_contents` —— 注意 `code` 是指向 `state->bo` 的指针时**也**会被释放，二者的释放次序由 `delete_shader_state` 保证（先 `destroy_shader` 再 `bo_reference(NULL)`）。

### 10.4 与 NIR 的接合点（本文件的“接缝”意义）

| 接缝 | 位置 | 说明 |
|---|---|---|
| 前端 → NIR | `mwv207_tgsi_to_nir`（`:868-880`） | `debug & 0x2` 时 `tgsi_dump_to_file`；使用 `mwv207_get_compiler_options` 提供的选项 |
| NIR → 硬件码 | `mwv207_set_current_shader` → `mwv207_compile_shader(ctx, state, offline)` | 唯一的编译触发点（create 与 key 变化时） |
| NIR 选项 | `mwv207_nir_options_VSGSFS`（`:759-807`）/ `mwv207_nir_options_TCSTES`（`:809-853`） | 含 `lower_all_io_to_temps`、`fuse_ffma16/32/64`、`lower_fdiv`、`lower_uniforms_to_ubo`、`lower_int64`、`lower_doubles`、`force_indirect_unrolling = nir_var_all`、`compact_arrays`、`has_uclz`、`vectorize_io` 等；TCSTES 版多 `lower_mul_high`、少 `lower_all_io_to_temps` |
| 选项分发 | `mwv207_get_compiler_options(struct pipe_screen *screen, enum pipe_shader_ir ir, enum pipe_shader_type shader)`（`:855-866`） | 按 `shader` 返回上述两份之一（**具体判断分支未逐行确认**） |
| 命令流配置 | `mwv207_config_vs/tcs/tes/gs/fs/link`（`:225-368`/`:370-431`/`:433-557`/`:559-633`/`:635-683`/`:685-757`） | 把编译产物（`num_temps`、`code_size`、输出寄存器号、`vs_load_balancing`、语义表等）写成现成的硬件命令（命令号见 §10.5） |

### 10.5 命令流寄存器号（供交叉检索）

| stage | 命令号（16 位寄存器号 = 厂商 `_Address >> 2`） | 行号 |
|---|---|---|
| VS | `0x202`、`0x1F1`、`0x230`（infile 数组）、`0x21C`、`0x228`、`0x5582`、`0x203`（num_temps）、`0x22A`、`0x219`、`0x21A`、`0x21D`、`0x22F`（code_size）、`0x21B`（reloc: bo/bo_offset）、`0x5580`、`0x5581`、`0x20C`（vs_load_balancing）、`0x238`（output 数组）、`0x201`、`0xE22` | `:225-368` |
| TCS | `0x5285`、`0x5288`、`0x5287`、`0x5290`、`0x5286`、`0x5293`、`0x21A`、`0x5280`、`0x5281`、`0x5282`（reloc）、`0x5284` | `:370-431` |
| TES | `0x52C0`、`0x52C6`、`0x52D0`、`0x52C8`、`0x52C7`、`0x52CD`、`0x52CB`、`0x21A`、`0x52C1`、`0x52C2`、`0x52C3`（reloc）、`0x52C5` | `:433-557` |
| GS | `0x440`、`0x441`、`0x442`、`0x448`、`0x451`、`0x452`、`0x450`、`0x457`、`0x455`、`0x21A`、`0x443`、`0x444`、`0x445`（reloc）、`0x447` | `:559-633` |
| FS | `0x401`、`0x40B`（output_reg）、`0x28D`、`0x5580`、`0x403`（num_temps + msaa）、`0x416`、`0x409`（常量池上限 `0x400/4`，与 `mwv207_ra.c:293-294` 呼应）、`0x21A`、`0x21F`、`0x424`（code_size）、`0x40A`（reloc）、`0x425` | `:635-683` |
| LINK | `0x2AA`、`0x28C`、`0x402`、`0xE07`、`0x2A4`、`0x420`、`0xE30`（semantics，16×4bit）、`0x380`（half_attribute）、`0x410`、`0x223`、`0x5283`、`0x52C4`、`0x446`、`0x412` | `:685-757` |

---

## 11. 调试开关（环境变量 + 独立命令行工具）

### 11.1 环境变量总表

| 变量 | 读取点 | 默认 | 作用 |
|---|---|---|---|
| `mwv207_debug` | `src/gallium/winsys/mwv207/drm/mwv207_drm_winsys.c:73`（`DEBUG_GET_ONCE_FLAGS_OPTION`）、`:125`（`ws->debug = debug_get_option_mwv207_debug()`） | `0` | 位掩码（见 §11.2），存于 `ws->debug`，驱动各处按位判断 |
| `mwv207_count` | `winsys/mwv207/drm/mwv207_drm_cs.c:312` | `""` | 限制 `cmd` dump 条数：`top[5\|10\|20\|50]` |
| `OPT_MOV` | `nir/mwv207_emit.c:289` | **`true`** | `false` 时不消除自赋值 `mov` |
| `OPT_IF_BAIL` | `nir/mwv207_emit.c:1919` | **`true`** | `false` 时不使用“then 只有 break/return”的短路发码形态 |
| `OPT_DIV` | `nir/mwv207_lower.c:1592-1593` | **`false`** | 启用 `fdiv` 窥孔（§4.7） |
| `OPT_FFMA` | `nir/mwv207_lower.c:1647-1648` | **`false`** | 启用 `ffma` 窥孔（§4.7） |
| `OPT_SET` | `nir/mwv207_lower.c:1650-1651` | **`false`** | 启用 `set` 窥孔（§4.7） |
| `MWV207_LOG_LEVEL` | `video-codec/common/mwv207_log.c:51`（`getenv`） | — | 视频编解码日志级别，**与 NIR 后端无关** |

开关读取宏：`debug_get_bool_option(name, default)`（Mesa util）；`do_by_default(OPT)` = `debug_get_bool_option("OPT_"#OPT, false)`（`mwv207_lower.c:1532` → `:1687` `#undef`）。**三个 `OPT_*` 窥孔默认全部关闭**——即默认配置下驱动不会做 `fdiv`/`ffma`/`set` 三种窥孔优化。

### 11.2 `mwv207_debug` 位表

| 名称 | 位 | winsys 宏（`mwv207_drm_winsys.h:33-40`） | 作用 | 消费点 |
|---|---|---|---|---|
| `nir` | `0x1` | `MWV207_DEBUG_NIR` | Dump NIR during program compile | `mwv207_shader.c:1011`（`nir_print_shader` + `mwv207_dump_shader_nir`） |
| `tgsi` | `0x2` | `MWV207_DEBUG_TGSI` | Dump TGSI during program compile | `mwv207_shader.c:868-880`（`tgsi_dump_to_file`） |
| `bo` | `0x4` | `MWV207_DEBUG_BO` | Dump bo info | `mwv207_drm_bo.c:60`、`:150`、`:215`、`:331`、`:405` |
| `cmd` | `0x8` | `MWV207_DEBUG_CMD` | Dump cmd（配 `mwv207_count`） | `mwv207_drm_cs.c:381` |
| `size` | `0x10` | `MWV207_DEBUG_SIZE` | Dump cmd size | `mwv207_drm_cs.c:335`（提前返回）、`:381` |
| `tex` | `0x20` | `MWV207_DEBUG_TEX` | Dump tex config | —（未逐一定位） |
| `flush` | `0x40` | `MWV207_DEBUG_FLUSH` | Flush all state after draw | —（未逐一定位） |
| `video` | `0x80` | `MWV207_DEBUG_VIDEO` | Dump video command stream 到 `mwv207_video_reg.dump` | video-codec |
| `nir_all` | `0x201` | —（`0x1 \| 0x200`） | Dump NIR after translate | `mwv207_compiler.c:452`（`debug == 0x201` 时 `nir_print_shader`） |
| `spill` | `0x100` | **无对应宏** | Dump spill info | `mwv207_ra.c:520-524`（直接 `debug & 0x100`） |

### 11.3 `mwv207_cmdline.c` —— 它是**独立命令行工具**，不是驱动开关

`nir/mwv207_cmdline.c`（170 行）是 `mwv207_compiler` 可执行文件的 `main()`（构建目标见 `mwv207/meson.build:86-108`，仅当 `with_tools.contains('mwv207')` 时构建；该文件**不在** `files_libmwv207` 内，因此不参与驱动库）。**任务背景中“mwv207_cmdline.c 提供的调试开关”应更正为：它提供的是一个离线 GLSL→NIR→硬件码的编译/反汇编工具入口。**

| 项 | 内容 | 行号 |
|---|---|---|
| `main` | `int main(int argc, char **argv)`；命令只有 `compile`；`disasm` 分支为 `unreachable("Unknown command. Valid: compile")` | `:154-170`、`:164-165` |
| 编译设置 | `standalone_options opts = { .glsl_version = 400, .do_link = true }` | `:97-100` |
| 上下文 | `CALLOC` 出 `ctx`/`ws`/`shader`/`shader->info` | `:102-107` |
| 前端 | `standalone_compile_shader(...)` | `:110` |
| 逐 stage NIR | 对 `MESA_SHADER_VERTEX … MESA_SHADER_STAGES` 的每个 `_LinkedShaders[i]`：TCS/TES 用 `mwv207_nir_options_TCSTES`，否则 `mwv207_nir_options_VSGSFS`，调 `glsl_to_nir`；`shader->stage = pipe_shader_type_from_mesa(i)` | `:114-123` |
| 固定 NIR 预处理 | `nir_lower_system_values` → `nir_lower_global_vars_to_local` → `nir_split_var_copies` → `nir_lower_var_copies` → `nir_lower_alu_to_scalar(NULL, NULL)` → `gl_nir_lower_bindless_images` → `gl_nir_lower_buffers` → `nir_opt_constant_folding` | `:125-135` |
| 编译 | `mwv207_compile_shader(ctx, shader->info, true)`（**`offline = true`**，只产出 `shader->code` 不写 BO） | `:137` |
| 输出 | `nir_print_shader(stderr, shader->info, ...)` + `mwv207_dump_shader_nir(shader)` | `:139-140` |
| 释放 | `code`/`imm_data`/`imm_contents` | `:142-144` |
| 自带 NIR 选项 | `mwv207_nir_options_VSGSFS`（`:46-68`，**无** `lower_fdiv`/`fuse_ffma*`/`lower_uniforms_to_ubo`）、`mwv207_nir_options_TCSTES`（`:70-91`）| 与 `mwv207_shader.c:759-807`/`:809-853` 的运行时版本**不是同一份** |

> **重要差异**：工具版选项缺 `fuse_ffma16|32|64` 与 `lower_fdiv`、`lower_uniforms_to_ubo`，因此**离线工具产出的指令序列与驱动运行时不一致**（尤其 32/16 位 FMA 融合与整除处理）。用它调试指令编码时应以运行时配置为准。

### 11.4 其它可用调试手段

| 手段 | 位置 | 说明 |
|---|---|---|
| `nir_print_shader` | `mwv207_compiler.c:452`（`debug == 0x201`）、`mwv207_shader.c:1011`（`debug & 0x1`） | 打印 NIR 文本 |
| `mwv207_dump_shader_nir` | `mwv207_disasm.c:1256-1317` | 打印反汇编 + 立即数 + 输入/输出 + key（见 §9.5） |
| spill 打印 | `mwv207_ra.c:520-524` | `mwv207_debug` 含 `0x100` 时对每个被溢出的 def 打印指令 |
| `compile_error` 宏 | `mwv207_common.h:73` | 编译期错误统一出口（未实现的 NIR 指令/内建会走这里） |

---

## 12. 未能确认之处汇总

按主题列出全部“未确认”项（均已在前文就地标注，行号为准）：

### 12.1 硬件 ISA 定义缺失

1. **`INST_OPCODE_*`/`INST_TYPE_*`/`INST_CONDITION_*`/`INST_ROUND_MODE_*`/`INST_EXT_OPCODE_*` 在本开源树无定义**（`include/hw/mwv207reg_shader.h` 为空壳；`include/hw/*.h` 全部为空壳或仅 `DISABLE/ENABLE`）。所有使用位置都在**从未调用的宏体**内（`mwv207_opcode.c:42-63` 15 个、`mwv207_disasm.c:1022-1052` 31 个），所以**不影响编译**，但表内 opcode 数值的“厂商手册语义”无权威来源（`§6.6`）。
2. 该缺失头推测与 etnaviv 同源（`src/mesa/src/gallium/drivers/etnaviv/hw/isa.xml.h` 有 `INST_OPCODE_ADD` 等），但**未找到生成规则**（`mwv207/meson.build` 中无相应 `custom_target`）——是遗漏还是刻意剥离，**未确认**（`§6.6`）。
3. `mwv207_disasm.c` 的 `_str_inst_type[]` 下标语义与 `get_alu_inst_type()`/`mwv207_ops[].type` **互相矛盾**；哪一侧对应真实硬件**未确认**（`§9.4 #1`）。

### 12.2 疑似缺陷（有行号，但无法从源码判断意图）

4. `mwv207_disasm()` 断言 `sizedwords % 2 == 0` 而步长为 4（`mwv207_disasm.c:1249`、`:1251`）。
5. `mwv207_dump_shader_nir()` 的 FS 颜色输出打印遇 0 截断（`mwv207_disasm.c:1309`）。
6. `mwv207_link_shader()` **恒 `return false`**（`mwv207_compiler.c:665`）；找不到语义输出时只 `printf` 不报错（`:641`）——是否为临时实现/故意，**未确认**（`§3.4`）。
7. `mwv207_compile_shader` 的 `bo_size = code_size * 4 * 16` 与 `bo_offset += code_size * 4 * 2` 的 16×/2× 倍数关系没有任何注释（`§5.5`）——**未确认**其硬件含义。
8. `mwv207_lower_shared_atomic` 的 `base_offset` 依赖**严格的指令形状匹配**，失败即 `assert(0)`（`mwv207_lower_shared_atomic.c:109-111`）——上游 pass 行为变化会导致崩溃，**未确认**该形状是否在全部 workgroup 形态下成立（`§4.9`）。
9. `winsys` 的 `0x100`（spill）在 `debug_options[]` 中登记，但 `mwv207_drm_winsys.h:33-40` **没有对应宏**（`§11.2`）。
10. `mwv207_cmdline.c` 自带的编译器选项集与运行时（`mwv207_shader.c:759-853`）**不一致**（缺 `fuse_ffma*`、`lower_fdiv`、`lower_uniforms_to_ubo`），离线结果不可直接对比运行时（`§11.3`）。

### 12.3 语义/实现细节存疑

11. `mwv207_ra.c` 的 `reg_get_base()` 中各 stage 的固定偏移（VS `+0`、FS `+1`、GS `+num_outputs(+1)`、TCS `+reserve(9/5)+1`、TES `+6`、CS/KERNEL `+3`）**没有任何注释或文档依据**（`§7.2`）。
12. 溢出循环 `while (!mwv207_ra(...)) { … }`（`mwv207_ra.c:821-824`）没有迭代上限——理论上可能不收敛（**未确认**是否有界）。
13. `mwv207_liveness.c` 中 `store_deref && !has_store` 时把 `state.index = ~0u`（`:214-216`）对后续 `range_include()` 扩张 `live_end` 的影响、以及是否会漏加干涉边，**未确认**（`§8.5`）。
14. `mwv207_set_current_shader` 的变体命中条件只检查 4 个 dirty 位（`tex_128bit`/`pixel_128bit`/`z32_shadow`/`is_dual`），其余 key 位（`frag_rgba_swap`/`front_ccw`/`render_arrary`/`has_tess`/`sprite_coord_*` 等）不同也可能复用同一变体——**未确认**这是否为有意设计（`§10.3`）。
15. `mwv207_lower_dual_blend` 把 RT 采样槽硬编码为 **4**（`ctx->sampler_views[4][...]`，`mwv207_lower_dual.c:109-151`）——与其它用途是否冲突**未确认**（`§4.5`）。
16. `nir_intrinsic_get_ssbo_size` 在 emit 层是空实现、真实值来自 RA 的常量源（`mwv207_ra.c:405-406`）——**未确认**若某 SSBO 尺寸既未被当作源、也未触发 `ubo_mask` 更新，硬件常量描述符是否会漏更新（`§7.7`）。
17. `mwv207_src_from_dest()` 中 `reg_swiz[reg_get_type(t)]` 对**已经取过类型**的 `t` 再做一次取模（`mwv207_ra.c:487`、`:493`，冗余但等价）——**未确认**是否为笔误。
18. `mwv207_emit.c:295-298` 遇到 `opcode == 0xff` 时先 `printf` 再 `assert`；`mwv207_opcode.c:41` 的默认 `{0xff}` 会让**任何未列出的 NIR ALU opcode 走到这里**——是否有意用断言兜底，**未确认**（`§5.8`）。
19. `mwv207_disasm.c` 中 `CONV_INST`（opcode `0x72`）有控制字段解码但无操作数解码路径（`:792-801` vs `:895-977`）——**未确认**该指令族是否已实现（`§9.4 #4`）。
20. 三个窥孔优化默认关闭（`OPT_*` 默认 `false`，`mwv207_lower.c:1532`）——是性能未达标而默认禁用，还是留给特定场景，**未确认**（`§4.7`）。
21. `mwv207_compiler.c` 的 `nis_all_shaders` 相关字段与 `mwv207_shader_key` 位域宽度（`tcs_input_vertries_regs:2` 仅 2 位，`(tcs_vertices_out-1)/8` 最大 3）能否覆盖全部细分参数，**未确认**（`§2.3`）。