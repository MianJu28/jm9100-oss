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
#ifndef H_MWV207_COMMON
#define H_MWV207_COMMON

#include "util/u_math.h"
#include "util/u_memory.h"

#include "compiler/nir/nir.h"
#include "compiler/nir/nir_deref.h"
#include "compiler/nir/nir_builder.h"
#include "compiler/nir/nir_worklist.h"

#include "mwv207_disasm.h"

#define INST_SWIZ_COMP_X               0x00000000
#define INST_SWIZ_COMP_Y               0x00000001
#define INST_SWIZ_COMP_Z               0x00000002
#define INST_SWIZ_COMP_W               0x00000003

#define INST_SWIZ_X__MASK              0x00000003
#define INST_SWIZ_X__SHIFT             0
#define INST_SWIZ_X(x)                 (((x) << INST_SWIZ_X__SHIFT) & INST_SWIZ_X__MASK)
#define INST_SWIZ_Y__MASK              0x0000000c
#define INST_SWIZ_Y__SHIFT             2
#define INST_SWIZ_Y(x)                 (((x) << INST_SWIZ_Y__SHIFT) & INST_SWIZ_Y__MASK)
#define INST_SWIZ_Z__MASK              0x00000030
#define INST_SWIZ_Z__SHIFT             4
#define INST_SWIZ_Z(x)                 (((x) << INST_SWIZ_Z__SHIFT) & INST_SWIZ_Z__MASK)
#define INST_SWIZ_W__MASK              0x000000c0
#define INST_SWIZ_W__SHIFT             6
#define INST_SWIZ_W(x)                 (((x) << INST_SWIZ_W__SHIFT) & INST_SWIZ_W__MASK)

#define INST_SWIZ_BROADCAST(x)         (INST_SWIZ_X(x) | INST_SWIZ_Y(x) | INST_SWIZ_Z(x) | INST_SWIZ_W(x))

#define INST_SWIZ_IDENTITY         (INST_SWIZ_X(0) | INST_SWIZ_Y(1) | INST_SWIZ_Z(2) | INST_SWIZ_W(3))

#define INST_SWIZ(x,y,z,w)         (INST_SWIZ_X(x) | INST_SWIZ_Y(y) | INST_SWIZ_Z(z) | INST_SWIZ_W(w))
#define SWIZZLE(c0,c1,c2,c3) \
   INST_SWIZ(INST_SWIZ_COMP_##c0, \
             INST_SWIZ_COMP_##c1, \
             INST_SWIZ_COMP_##c2, \
             INST_SWIZ_COMP_##c3)

#define COND(bool, val) ((bool) ? (val) : 0)

#define VS_BALANCE(A) (((20480 / (0x400 - 2 * A * 0x10)) + 9) / 10)

#define compile_error(ctx, args...) ({ \
      printf(args); \
      assert(0); \
   })

enum mwv207_pass_flags {
   BYPASS_DST = BITFIELD_BIT(0),
   BYPASS_SRC = BITFIELD_BIT(1),

   SRC0_MOD_NEG = BITFIELD_BIT(2),
   SRC1_MOD_NEG = BITFIELD_BIT(3),
   SRC2_MOD_NEG = BITFIELD_BIT(4),
   SRC0_MOD_ABS = BITFIELD_BIT(5),
   SRC1_MOD_ABS = BITFIELD_BIT(6),
   SRC2_MOD_ABS = BITFIELD_BIT(7),
};

static_assert(BITFIELD_RANGE(0,2) == (BYPASS_DST | BYPASS_SRC), "is_dead_mask is wrong");
static_assert(BITFIELD_RANGE(2,3) == (SRC0_MOD_NEG | SRC1_MOD_NEG | SRC2_MOD_NEG), "src_mod_neg_mask is wrong");
static_assert(BITFIELD_RANGE(5,3) == (SRC0_MOD_ABS | SRC1_MOD_ABS | SRC2_MOD_ABS), "src_mod_abs_mask is wrong");

static inline bool is_dead_instruction(nir_instr *instr)
{
   return instr->pass_flags & BITFIELD_RANGE(0,2);
}

static inline void set_src_mod_abs(nir_instr *instr, unsigned idx)
{
   assert(idx < 3);
   instr->pass_flags |= (SRC0_MOD_ABS << idx);
}

static inline void set_src_mod_neg(nir_instr *instr, unsigned idx)
{
   assert(idx < 3);
   instr->pass_flags |= (SRC0_MOD_NEG << idx);
}

static inline void toggle_src_mod_neg(nir_instr *instr, unsigned idx)
{
   assert(idx < 3);
   instr->pass_flags ^= (SRC0_MOD_NEG << idx);
}

static inline bool is_src_mod_abs(nir_instr *instr, unsigned idx)
{
   if (idx < 3)
      return instr->pass_flags & (SRC0_MOD_ABS << idx);

   return false;
}

static inline bool is_src_mod_neg(nir_instr *instr, unsigned idx)
{
   if (idx < 3)
      return instr->pass_flags & (SRC0_MOD_NEG << idx);

   return false;
}

enum mwv207_immediate_contents {
   MWV207_IMMEDIATE_UNUSED = 0,
   MWV207_IMMEDIATE_CONSTANT,
   MWV207_IMMEDIATE_UNIFORM,
   MWV207_UNIFORM_TEXTURE_WIDTH,
   MWV207_UNIFORM_TEXTURE_HEIGHT,
   MWV207_UNIFORM_TEXTURE_DEPTH,
   MWV207_UNIFORM_PATCH_VERTEX_IN,
   MWV207_IMMEDIATE_TEXRECT_SCALE_X,
   MWV207_IMMEDIATE_TEXRECT_SCALE_Y,
   MWV207_IMMEDIATE_WORK_GROUP_COUNT,
   MWV207_IMMEDIATE_WORK_GROUP_SIZE,
   MWV207_IMMEDIATE_UBO,
   MWV207_IMMEDIATE_SSBO,
   MWV207_IMMEDIATE_SSIMAGE,
   MWV207_IMMEDIATE_SSIMAGE_STRIDE,
   MWV207_IMMEDIATE_SSIMAGE_SIZE,
   MWV207_IMMEDIATE_SSIMAGE_INFO,
   MWV207_IMMEDIATE_SSIMAGE_LAYER_SIZE,
   MWV207_IMMEDIATE_SSBO_SIZE,
   MWV207_IMMEDIATE_ATOMIC_BO,
   MWV207_IMMEDIATE_LOCAL_MEMORY_OFFSET,
   MWV207_IMMEDIATE_GLOBAL_MEMORY_OFFSET,
};

struct mwv207_inst_dst {
   uint32_t use: 1;
   uint32_t amode: 3;
   uint32_t reg: 7;
   uint32_t reg_bit7: 1;
   uint32_t reg_bit8: 1;
   uint32_t write_mask: 4;
};

struct mwv207_inst_tex {
   uint32_t id: 5;
   union {
      struct {
         uint32_t amode: 3;
         uint32_t swiz: 8;
      };
      struct {
         uint32_t s1_reserve0  : 2;
         uint32_t pack_mode    : 1;
         uint32_t attri_client : 2;
         uint32_t s1_reserve1  : 1;
         uint32_t attri_layout : 1;
         uint32_t s1_reserve2  : 1;
         uint32_t is_local     : 1;
         uint32_t s1_reserve3  : 2;
      };
      struct {
         uint32_t s2_reserve0 : 3;
         uint32_t restart     : 1;
         uint32_t no_jump_end : 1;
         uint32_t s2_reserve1 : 6;
      };
   };
};

struct mwv207_inst_src {
   uint32_t use: 1;
   uint32_t rgroup: 3;
   union {
      struct {
         uint32_t reg: 9;
         uint32_t swiz: 8;
         uint32_t neg: 1;
         uint32_t abs: 1;
         uint32_t amode: 3;
         uint32_t is_const_reg: 1;
      };
      struct {
         uint32_t imm_val : 20;
         uint32_t imm_type : 2;
         uint32_t reverse : 1;
      };
   };
};

struct mwv207_inst {
   uint32_t opcode;
   uint32_t ext_opcode;
   uint32_t type;
   uint32_t cond: 5;
   uint32_t sat: 1;
   uint32_t round: 2;
   uint32_t sel_bit0: 1;
   uint32_t sel_bit1: 1;
   uint32_t dst_full: 1;
   struct mwv207_inst_dst dst;
   struct mwv207_inst_tex tex;
   struct mwv207_inst_src src[0x3];
   uint32_t imm;

   enum kinds_of_inst kind_of_inst;
   uint32_t flags;
   uint32_t src_mask;
   uint32_t src_count;
   struct mwv207_inst_src sampler_aux_src;
   struct mwv207_inst_ctrl inst_ctrl;
};

typedef struct mwv207_inst_dst hw_dst;

struct mwv207_op_info {
   uint8_t opcode;
   uint8_t src;
   uint8_t cond;
   uint8_t type;
   uint8_t round;
   uint8_t ext_opcode;
};

extern struct mwv207_op_info mwv207_ops[nir_num_opcodes];

struct live_def {
   nir_instr *instr;
   nir_def *dest;
   uint32_t live_start, live_end;
};

struct live_defs_state {
   uint32_t num_defs;
   uint32_t bitset_words;

   nir_function_impl *impl;
   nir_block *block;
   uint32_t index;

   struct live_def *defs;
   uint32_t *live_map;

   nir_block_worklist worklist;
};

int
mwv207_assemble(uint32_t *out, const struct mwv207_inst *inst);

uint32_t
inst_swiz_compose(uint32_t swz1, uint32_t swz2);

uint32_t
inst_write_mask_compose(uint32_t wm1, uint32_t wm2);

struct mwv207_inst_src
src_swizzle(struct mwv207_inst_src src, uint32_t swizzle);

bool
is_sysval(nir_instr *instr);

uint32_t
src_index(nir_function_impl *impl, nir_src *src);

uint32_t
dest_index(nir_function_impl *impl, nir_def *dest);

nir_def *
real_dest(nir_def *dest, uint32_t *swiz, uint32_t *mask);

nir_def *
dest_for_instr(nir_instr *instr);

int
const_add(uint64_t *c, uint64_t value);
#endif
