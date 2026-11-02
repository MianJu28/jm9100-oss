/*
* Copyright (c) 2020 ChangSha JingJiaMicro Electronics Co., Ltd.
* All rights reserved.
*
* Author:
*      shanjinkui <shanjinkui@jingjiamicro.com>
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice (including the next
* paragraph) shall be included in all copies or substantial portions of the
* Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* ChangSha JingJiaMicro Electronics Co., Ltd. AND/OR ITS SUPPLIERS BE LIABLE
* FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
* OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/
#ifndef H_MWV207_EMIT
#define H_MWV207_EMIT

#include "mwv207_compiler.h"

#define emit(type, args...) mwv207_emit_##type(c, args)

#define DST_REG(idx, m)   ((struct mwv207_inst_dst){.use = 1, .reg = idx, .write_mask = m})
#define ALU_SWIZ(s)       INST_SWIZ((s)->swizzle[0], (s)->swizzle[1], (s)->swizzle[2], (s)->swizzle[3])
#define SRC_DISABLE       ((struct mwv207_inst_src){})
#define SRC_CONST(idx, s) ((struct mwv207_inst_src){.use=1, .rgroup = 0x2, .reg=idx, .swiz=s})
#define SRC_REG(idx, s)   ((struct mwv207_inst_src){.use=1, .rgroup = 0x0, .reg=idx, .swiz=s})

#define CONST_VAL(a, b) (nir_const_value) {.u64 = (uint64_t)(a) << 32 | (uint64_t)(b)}
#define CONST(x)        CONST_VAL(MWV207_IMMEDIATE_CONSTANT, x)
#define UNIFORM(x)      CONST_VAL(MWV207_IMMEDIATE_UNIFORM, x)
#define TEXSCALE(x, i)  CONST_VAL(MWV207_IMMEDIATE_TEXRECT_SCALE_X + (i), x)
#define TEXSIZE(x, i)   CONST_VAL(MWV207_UNIFORM_TEXTURE_WIDTH + (i), x)

static inline bool
mwv207_rgroup_is_uniform(uint32_t rgroup)
{
   return rgroup == 0x2 ||
          rgroup == 0x3;
}

static inline struct mwv207_inst_src
mwv207_immediate_src(uint32_t type, uint32_t bits)
{
   return (struct mwv207_inst_src) {
      .use = 1,
      .rgroup = 0x7,
      .imm_val = bits,
      .imm_type = type
   };
}

static inline struct mwv207_inst_src
mwv207_immediate_float(float x)
{
   uint32_t bits = fui(x);
   assert((bits & 0xfff) == 0);
   return mwv207_immediate_src(0x0, bits >> 12);
}

static inline struct mwv207_inst_src
mwv207_immediate_int(int x)
{
   assert(x >= -0x80000 && x < 0x80000);
   return mwv207_immediate_src(0x1, x);
}

static inline struct mwv207_inst_src
mwv207_immediate_uint(int x)
{
   assert(x >= -0x80000 && x < 0x80000);
   return mwv207_immediate_src(0x2, x);
}

bool is_decl_reg(nir_instr *instr);

uint32_t
live_defs(nir_function_impl *impl, struct live_def *defs, uint32_t *live_map, bool has_store);

void
mwv207_init_reg_state(struct mwv207_context *ctx, struct mwv207_compile *c, nir_shader *shader);

void
mwv207_free_reg_state(struct mwv207_compile *c);

struct mwv207_inst_dst
mwv207_dest(struct mwv207_compile *c, nir_def *dest, uint32_t *p_swiz);

struct mwv207_inst_src
mwv207_src_from_dest(struct mwv207_compile *c, nir_def *dest);

struct mwv207_inst_src
mwv207_src(struct mwv207_compile *c, nir_src *src);

struct mwv207_inst_src
mwv207_const(struct mwv207_compile *c, nir_const_value *value, uint32_t num_components);

void
emit_inst(struct mwv207_compile *c, struct mwv207_inst *inst);

void
mwv207_emit_shader(struct mwv207_context *ctx, struct mwv207_compile *c);

#endif
