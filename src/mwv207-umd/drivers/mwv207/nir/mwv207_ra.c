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
#include <stdio.h>
#include "util/register_allocate.h"

#include "mwv207_cmd.h"
#include "mwv207_context.h"
#include "mwv207_common.h"
#include "mwv207_compiler.h"
#include "mwv207_emit.h"
#include "compiler/nir/nir_worklist.h"

static uint32_t *q_values[] = {
   (uint32_t[]) {1, 2, 3, 4, 2, 2, 3, },
   (uint32_t[]) {3, 5, 6, 6, 5, 5, 6, },
   (uint32_t[]) {3, 4, 4, 4, 4, 4, 4, },
   (uint32_t[]) {1, 1, 1, 1, 1, 1, 1, },
   (uint32_t[]) {1, 2, 2, 2, 1, 2, 2, },
   (uint32_t[]) {2, 3, 3, 3, 2, 3, 3, },
   (uint32_t[]) {2, 2, 2, 2, 2, 2, 2, },
};

enum {
   REG_CLASS_VIRT_SCALAR,
   REG_CLASS_VIRT_VEC2,
   REG_CLASS_VIRT_VEC3,
   REG_CLASS_VEC4,

   REG_CLASS_VIRT_VEC2T,

   REG_CLASS_VIRT_VEC2C,
   REG_CLASS_VIRT_VEC3C,
   NUM_REG_CLASSES,
};

enum {
   REG_TYPE_VEC4,
   REG_TYPE_VIRT_VEC3_XYZ,
   REG_TYPE_VIRT_VEC3_XYW,
   REG_TYPE_VIRT_VEC3_XZW,
   REG_TYPE_VIRT_VEC3_YZW,
   REG_TYPE_VIRT_VEC2_XY,
   REG_TYPE_VIRT_VEC2_XZ,
   REG_TYPE_VIRT_VEC2_XW,
   REG_TYPE_VIRT_VEC2_YZ,
   REG_TYPE_VIRT_VEC2_YW,
   REG_TYPE_VIRT_VEC2_ZW,
   REG_TYPE_VIRT_SCALAR_X,
   REG_TYPE_VIRT_SCALAR_Y,
   REG_TYPE_VIRT_SCALAR_Z,
   REG_TYPE_VIRT_SCALAR_W,
   REG_TYPE_VIRT_VEC2T_XY,
   REG_TYPE_VIRT_VEC2T_ZW,
   REG_TYPE_VIRT_VEC2C_XY,
   REG_TYPE_VIRT_VEC2C_YZ,
   REG_TYPE_VIRT_VEC2C_ZW,
   REG_TYPE_VIRT_VEC3C_XYZ,
   REG_TYPE_VIRT_VEC3C_YZW,
   NUM_REG_TYPES,
};

static const uint8_t
reg_writemask[NUM_REG_TYPES] = {
   [REG_TYPE_VEC4] = 0xf,
   [REG_TYPE_VIRT_SCALAR_X]  = 0x1,
   [REG_TYPE_VIRT_SCALAR_Y]  = 0x2,
   [REG_TYPE_VIRT_VEC2_XY]   = 0x3,
   [REG_TYPE_VIRT_VEC2T_XY]  = 0x3,
   [REG_TYPE_VIRT_VEC2C_XY]  = 0x3,
   [REG_TYPE_VIRT_SCALAR_Z]  = 0x4,
   [REG_TYPE_VIRT_VEC2_XZ]   = 0x5,
   [REG_TYPE_VIRT_VEC2_YZ]   = 0x6,
   [REG_TYPE_VIRT_VEC2C_YZ]  = 0x6,
   [REG_TYPE_VIRT_VEC3_XYZ]  = 0x7,
   [REG_TYPE_VIRT_VEC3C_XYZ] = 0x7,
   [REG_TYPE_VIRT_SCALAR_W]  = 0x8,
   [REG_TYPE_VIRT_VEC2_XW]   = 0x9,
   [REG_TYPE_VIRT_VEC2_YW]   = 0xa,
   [REG_TYPE_VIRT_VEC3_XYW]  = 0xb,
   [REG_TYPE_VIRT_VEC2_ZW]   = 0xc,
   [REG_TYPE_VIRT_VEC2T_ZW]  = 0xc,
   [REG_TYPE_VIRT_VEC2C_ZW]  = 0xc,
   [REG_TYPE_VIRT_VEC3_XZW]  = 0xd,
   [REG_TYPE_VIRT_VEC3_YZW]  = 0xe,
   [REG_TYPE_VIRT_VEC3C_YZW] = 0xe,
};

static const uint8_t
reg_swiz[NUM_REG_TYPES] = {
   [REG_TYPE_VEC4]           = INST_SWIZ_IDENTITY,
   [REG_TYPE_VIRT_SCALAR_X]  = SWIZZLE(X, X, X, X),
   [REG_TYPE_VIRT_SCALAR_Y]  = SWIZZLE(Y, Y, Y, Y),
   [REG_TYPE_VIRT_VEC2_XY]   = INST_SWIZ_IDENTITY,
   [REG_TYPE_VIRT_VEC2T_XY]  = INST_SWIZ_IDENTITY,
   [REG_TYPE_VIRT_VEC2C_XY]  = INST_SWIZ_IDENTITY,
   [REG_TYPE_VIRT_SCALAR_Z]  = SWIZZLE(Z, Z, Z, Z),
   [REG_TYPE_VIRT_VEC2_XZ]   = SWIZZLE(X, Z, X, Z),
   [REG_TYPE_VIRT_VEC2_YZ]   = SWIZZLE(Y, Z, Y, Z),
   [REG_TYPE_VIRT_VEC2C_YZ]  = SWIZZLE(Y, Z, Y, Z),
   [REG_TYPE_VIRT_VEC3_XYZ]  = INST_SWIZ_IDENTITY,
   [REG_TYPE_VIRT_VEC3C_XYZ] = INST_SWIZ_IDENTITY,
   [REG_TYPE_VIRT_SCALAR_W]  = SWIZZLE(W, W, W, W),
   [REG_TYPE_VIRT_VEC2_XW]   = SWIZZLE(X, W, X, W),
   [REG_TYPE_VIRT_VEC2_YW]   = SWIZZLE(Y, W, Y, W),
   [REG_TYPE_VIRT_VEC3_XYW]  = SWIZZLE(X, Y, W, X),
   [REG_TYPE_VIRT_VEC2_ZW]   = SWIZZLE(Z, W, Z, W),
   [REG_TYPE_VIRT_VEC2T_ZW]  = SWIZZLE(Z, W, Z, W),
   [REG_TYPE_VIRT_VEC2C_ZW]  = SWIZZLE(Z, W, Z, W),
   [REG_TYPE_VIRT_VEC3_XZW]  = SWIZZLE(X, Z, W, X),
   [REG_TYPE_VIRT_VEC3_YZW]  = SWIZZLE(Y, Z, W, X),
   [REG_TYPE_VIRT_VEC3C_YZW] = SWIZZLE(Y, Z, W, X),
};

static const uint8_t
reg_dst_swiz[NUM_REG_TYPES] = {
   [REG_TYPE_VEC4]           = INST_SWIZ_IDENTITY,
   [REG_TYPE_VIRT_SCALAR_X]  = INST_SWIZ_IDENTITY,
   [REG_TYPE_VIRT_SCALAR_Y]  = SWIZZLE(X, X, X, X),
   [REG_TYPE_VIRT_VEC2_XY]   = INST_SWIZ_IDENTITY,
   [REG_TYPE_VIRT_VEC2T_XY]  = INST_SWIZ_IDENTITY,
   [REG_TYPE_VIRT_VEC2C_XY]  = INST_SWIZ_IDENTITY,
   [REG_TYPE_VIRT_SCALAR_Z]  = SWIZZLE(X, X, X, X),
   [REG_TYPE_VIRT_VEC2_XZ]   = SWIZZLE(X, X, Y, Y),
   [REG_TYPE_VIRT_VEC2_YZ]   = SWIZZLE(X, X, Y, Y),
   [REG_TYPE_VIRT_VEC2C_YZ]  = SWIZZLE(X, X, Y, Y),
   [REG_TYPE_VIRT_VEC3_XYZ]  = INST_SWIZ_IDENTITY,
   [REG_TYPE_VIRT_VEC3C_XYZ] = INST_SWIZ_IDENTITY,
   [REG_TYPE_VIRT_SCALAR_W]  = SWIZZLE(X, X, X, X),
   [REG_TYPE_VIRT_VEC2_XW]   = SWIZZLE(X, X, Y, Y),
   [REG_TYPE_VIRT_VEC2_YW]   = SWIZZLE(X, X, Y, Y),
   [REG_TYPE_VIRT_VEC3_XYW]  = SWIZZLE(X, Y, Z, Z),
   [REG_TYPE_VIRT_VEC2_ZW]   = SWIZZLE(X, X, X, Y),
   [REG_TYPE_VIRT_VEC2T_ZW]  = SWIZZLE(X, X, X, Y),
   [REG_TYPE_VIRT_VEC2C_ZW]  = SWIZZLE(X, X, X, Y),
   [REG_TYPE_VIRT_VEC3_XZW]  = SWIZZLE(X, Y, Y, Z),
   [REG_TYPE_VIRT_VEC3_YZW]  = SWIZZLE(X, X, Y, Z),
   [REG_TYPE_VIRT_VEC3C_YZW] = SWIZZLE(X, X, Y, Z),
};

static const uint8_t
input_swiz[NUM_REG_TYPES] = {
   [0]  = SWIZZLE(X, Y, Z, W),
   [1]  = SWIZZLE(Y, Z, W, W),
   [2]  = SWIZZLE(Z, W, W, W),
   [3]  = SWIZZLE(W, W, W, W),
};

static int
reg_get_type(int virt_reg)
{
   return virt_reg % NUM_REG_TYPES;
}

static int
reg_get_base(struct mwv207_compile *c, int virt_reg)
{

   uint reserve_register_tcs = c->nir->info.patch_outputs_read ? 9 : 5;

   if (c->shader->stage == PIPE_SHADER_VERTEX)
      return virt_reg / NUM_REG_TYPES;
   else if (c->shader->stage == PIPE_SHADER_FRAGMENT)
      return (virt_reg / NUM_REG_TYPES + 1) % 0x40;
   else if (c->shader->stage == PIPE_SHADER_GEOMETRY) {
      uint offset = 0;

      if (c->shader->gs_use_primitive_id || c->shader->gs_use_invocation_id ||
          c->shader->gs_use_layer)
         offset += 1;

      return (virt_reg / NUM_REG_TYPES + c->nir->num_outputs + offset) % 0x40;
   } else if (c->shader->stage == PIPE_SHADER_TESS_CTRL)

      return (virt_reg / NUM_REG_TYPES + reserve_register_tcs + 1) % 0x40;
   else if (c->shader->stage == PIPE_SHADER_TESS_EVAL)

      return (virt_reg / NUM_REG_TYPES + 5 + 1) % 0x40;
   else if (c->shader->stage == PIPE_SHADER_COMPUTE || c->shader->stage == MESA_SHADER_KERNEL)
      return (virt_reg / NUM_REG_TYPES + 3) % 0x40;
   else
      return virt_reg / NUM_REG_TYPES;
}

static int
reg_get_class(int virt_reg)
{
   switch (reg_get_type(virt_reg)) {
   case REG_TYPE_VEC4:
      return REG_CLASS_VEC4;

   case REG_TYPE_VIRT_VEC3_XYZ:
   case REG_TYPE_VIRT_VEC3_XYW:
   case REG_TYPE_VIRT_VEC3_XZW:
   case REG_TYPE_VIRT_VEC3_YZW:
      return REG_CLASS_VIRT_VEC3;

   case REG_TYPE_VIRT_VEC2_XY:
   case REG_TYPE_VIRT_VEC2_XZ:
   case REG_TYPE_VIRT_VEC2_XW:
   case REG_TYPE_VIRT_VEC2_YZ:
   case REG_TYPE_VIRT_VEC2_YW:
   case REG_TYPE_VIRT_VEC2_ZW:
      return REG_CLASS_VIRT_VEC2;

   case REG_TYPE_VIRT_SCALAR_X:
   case REG_TYPE_VIRT_SCALAR_Y:
   case REG_TYPE_VIRT_SCALAR_Z:
   case REG_TYPE_VIRT_SCALAR_W:
      return REG_CLASS_VIRT_SCALAR;

   case REG_TYPE_VIRT_VEC2T_XY:
   case REG_TYPE_VIRT_VEC2T_ZW:
      return REG_CLASS_VIRT_VEC2T;

   case REG_TYPE_VIRT_VEC2C_XY:
   case REG_TYPE_VIRT_VEC2C_YZ:
   case REG_TYPE_VIRT_VEC2C_ZW:
      return REG_CLASS_VIRT_VEC2C;

   case REG_TYPE_VIRT_VEC3C_XYZ:
   case REG_TYPE_VIRT_VEC3C_YZW:
      return REG_CLASS_VIRT_VEC3C;
   }

   assert(false);
   return 0;
}

struct mwv207_inst_src
mwv207_const(struct mwv207_compile *c, nir_const_value *value, uint32_t num_components)
{

   if (num_components == 1 && value[0].u64 >> 32 == MWV207_IMMEDIATE_CONSTANT) {
      uint32_t bits = value[0].u32;

      if ((bits & 0xfff) == 0)
         return mwv207_immediate_src(0, bits >> 12);

      if (bits < (1 << 20))
         return mwv207_immediate_src(2, bits);

      if (bits >= 0xfff80000)
         return mwv207_immediate_src(1, bits);
   }

   uint32_t i;
   int swiz = -1;

   for (i = 0; swiz < 0; i++) {
      uint64_t *a = &c->consts[i * 4];
      uint64_t save[4];
      memcpy(save, a, sizeof(save));
      swiz = 0;

      for (uint32_t j = 0; j < num_components; j++) {
         int c = const_add(a, value[j].u64);

         if (c < 0) {
            memcpy(a, save, sizeof(save));
            swiz = -1;
            break;
         }

         swiz |= c << j * 2;
      }
   }

   assert(i <= 0x800 / 4);

   if (c->shader->stage == PIPE_SHADER_FRAGMENT)
      assert(i <= 0x400 / 4);

   c->const_count = MAX2(c->const_count, i);

   return SRC_CONST(i - 1, swiz);
}

static struct mwv207_inst_src
mwv207_reg_src(struct mwv207_compile *c, nir_src *src)
{
   uint32_t reg = ra_get_node_reg(c->g, c->live_map[src_index(c->impl, src)]);
   return SRC_REG(reg_get_base(c, reg), reg_swiz[reg_get_type(reg)]);
}

struct mwv207_inst_src
mwv207_src(struct mwv207_compile *c, nir_src *src)
{
   if (!src)
      return SRC_DISABLE;

   nir_instr *instr = src->ssa->parent_instr;

   if (instr->pass_flags & BYPASS_SRC) {
      assert(instr->type == nir_instr_type_alu);
      nir_alu_instr *alu = nir_instr_as_alu(instr);
      assert(alu->op == nir_op_mov);
      return src_swizzle(mwv207_src(c, &alu->src[0].src), ALU_SWIZ(&alu->src[0]));
   }

   switch (instr->type) {
   case nir_instr_type_load_const:
      return mwv207_const(c, nir_instr_as_load_const(instr)->value, src->ssa->num_components);
   case nir_instr_type_intrinsic: {
      nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);

      switch (intr->intrinsic) {
      case nir_intrinsic_load_tess_coord:
         if (c->shader->stage == PIPE_SHADER_TESS_EVAL)
            return SRC_REG(0, SWIZZLE(X, Y, Z, X));
         FALLTHROUGH;
      case nir_intrinsic_load_input:
      case nir_intrinsic_load_global_amd:
      case nir_intrinsic_load_global_constant:
      case nir_intrinsic_load_deref:
      case nir_intrinsic_load_vertex_id:
      case nir_intrinsic_load_instance_id:
      case nir_intrinsic_load_uniform:
      case nir_intrinsic_load_reg:
         return mwv207_reg_src(c, src);
      case nir_intrinsic_load_ubo: {
         nir_intrinsic_instr *load_intr = nir_instr_as_intrinsic(instr);
         if (nir_src_is_const(load_intr->src[1])) {
            uint32_t index = nir_src_as_int(load_intr->src[1]);
            uint32_t bytesize = intr->def.bit_size * intr->def.num_components / 8;
            index = (index == 0) ? 0 : (index / bytesize - 1);

            c->shader->ubo_mask[index / 64] |= (1ull << (index % 64));
         }
         return mwv207_reg_src(c, src);
      }
      case nir_intrinsic_load_per_vertex_input: {
         uint32_t reg = ra_get_node_reg(c->g, c->live_map[src_index(c->impl, src)]);
         nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
         return SRC_REG(reg_get_base(c, reg), input_swiz[nir_intrinsic_component(intr)]);
      } FALLTHROUGH;
      case nir_intrinsic_load_patch_vertices_in:
         return mwv207_const(c, &CONST_VAL(MWV207_UNIFORM_PATCH_VERTEX_IN, 0), 1);
      case nir_intrinsic_load_primitive_id: {
         if (c->shader->stage == PIPE_SHADER_GEOMETRY) {
            if (!c->shader->gs_use_restart &&
                !c->shader->gs_use_invocation_id)
               return SRC_REG(0, INST_SWIZ_COMP_Z);
            else if (c->shader->gs_use_restart &&
                     !c->shader->gs_use_invocation_id)
               return SRC_REG(0, INST_SWIZ_COMP_W);
            else
               return SRC_REG(1, INST_SWIZ_COMP_X);
         } else if (c->shader->stage == PIPE_SHADER_TESS_CTRL)
            return SRC_REG(0, INST_SWIZ_COMP_Z);
         else if (c->shader->stage == PIPE_SHADER_TESS_EVAL) {
            return SRC_REG(1, INST_SWIZ_COMP_Z);
         } else {
            compile_error(ctx, "the primitive id in PS should be assigned to the last register\n");
            assert(0);
         }
      }
      case nir_intrinsic_load_invocation_id: {
         if (c->shader->stage == PIPE_SHADER_GEOMETRY) {
            if (!c->shader->gs_use_restart)

               return SRC_REG(0, INST_SWIZ_COMP_Z);
            else

               return SRC_REG(0, INST_SWIZ_COMP_W);
         } else if (c->shader->stage == PIPE_SHADER_TESS_CTRL)
            return SRC_REG(0, INST_SWIZ_COMP_Y);
      } FALLTHROUGH;
      case nir_intrinsic_load_front_face:
         return (struct mwv207_inst_src) { .use = 1, .rgroup = 0x1 };
      case nir_intrinsic_load_frag_coord:
         return SRC_REG(0, INST_SWIZ_IDENTITY);

      case nir_intrinsic_load_shared:
      case nir_intrinsic_load_ssbo:
      case nir_intrinsic_ssbo_atomic:
      case nir_intrinsic_ssbo_atomic_swap:
      case nir_intrinsic_shared_atomic:
      case nir_intrinsic_shared_atomic_swap:
      case nir_intrinsic_image_deref_atomic:
      case nir_intrinsic_image_deref_atomic_swap:
         return mwv207_reg_src(c, src);
      case nir_intrinsic_get_ssbo_size:
         return mwv207_const(c, &CONST_VAL(MWV207_IMMEDIATE_SSBO_SIZE, nir_src_as_int(intr->src[0])), 1);
      case nir_intrinsic_load_workgroup_id:
         return SRC_REG(0, INST_SWIZ_IDENTITY);
      case nir_intrinsic_load_local_invocation_id:
         return SRC_REG(1, INST_SWIZ_IDENTITY);
      case nir_intrinsic_load_global_invocation_id:
      case nir_intrinsic_load_global_invocation_id_zero_base:
         return SRC_REG(2, INST_SWIZ_IDENTITY);
      case nir_intrinsic_load_num_workgroups: {
         static nir_const_value group_num[3] = {
            CONST_VAL(MWV207_IMMEDIATE_WORK_GROUP_COUNT, 0),
            CONST_VAL(MWV207_IMMEDIATE_WORK_GROUP_COUNT, 1),
            CONST_VAL(MWV207_IMMEDIATE_WORK_GROUP_COUNT, 2),
         };
         return mwv207_const(c, group_num, 3);
      }
      case nir_intrinsic_load_workgroup_size: {
         static nir_const_value group_size[3] = {
            CONST_VAL(MWV207_IMMEDIATE_WORK_GROUP_SIZE, 0),
            CONST_VAL(MWV207_IMMEDIATE_WORK_GROUP_SIZE, 1),
            CONST_VAL(MWV207_IMMEDIATE_WORK_GROUP_SIZE, 2),
         };
         return mwv207_const(c, group_size, 3);
      }
      case nir_intrinsic_atomic_counter_inc:
      case nir_intrinsic_atomic_counter_pre_dec:
      case nir_intrinsic_atomic_counter_read:
      case nir_intrinsic_image_deref_load:
      case nir_intrinsic_image_deref_size:
         return mwv207_reg_src(c, src);
      default:
         compile_error(c, "Unhandled NIR intrinsic type: %s\n",
                       nir_intrinsic_infos[intr->intrinsic].name);
         break;
      }
   } break;
   case nir_instr_type_alu:
   case nir_instr_type_tex:
      return mwv207_reg_src(c, src);
   case nir_instr_type_undef: {

      nir_const_value value = CONST(0);
      return src_swizzle(mwv207_const(c, &value, 1), SWIZZLE(X, X, X, X));
   }
   default:
      compile_error(c, "Unhandled NIR instruction type: %d\n", instr->type);
      break;
   }

   return SRC_DISABLE;
}

struct mwv207_inst_dst
mwv207_dest(struct mwv207_compile *c, nir_def *dest, uint32_t *p_swiz)
{
   uint32_t swiz = INST_SWIZ_IDENTITY, mask = 0xf;
   dest = real_dest(dest, &swiz, &mask);

   if (dest == NULL)
      return (struct mwv207_inst_dst) {0};

   uint32_t r = ra_get_node_reg(c->g, c->live_map[dest_index(c->impl, dest)]);

   uint32_t t = reg_get_type(r);

   *p_swiz = inst_swiz_compose(swiz, reg_dst_swiz[t]);

   return (struct mwv207_inst_dst) {
      .use = 1,
      .reg = reg_get_base(c, r),
      .write_mask = inst_write_mask_compose(mask, reg_writemask[t]),
   };
}

struct mwv207_inst_src
mwv207_src_from_dest(struct mwv207_compile *c, nir_def *dest)
{
   uint32_t swiz = INST_SWIZ_IDENTITY, mask = 0xf;
   dest = real_dest(dest, &swiz, &mask);

   uint32_t r = ra_get_node_reg(c->g, c->live_map[dest_index(c->impl, dest)]);
   uint32_t t = reg_get_type(r);

   return (struct mwv207_inst_src) {
      .use = 1,
      .reg = reg_get_base(c, r),
      .rgroup = 0x0,
      .swiz = reg_swiz[reg_get_type(t)],
   };
}

static void
mwv207_spill_reg(struct mwv207_compile *c,
                 struct ra_graph *g,
                 uint32_t num_nodes,
                 struct live_def *defs,
                 nir_function_impl *impl,
                 uint32_t debug)
{
   for(uint32_t i = 0; i < num_nodes; i++) {
      ra_set_node_spill_cost(g, i, 1.0);
   }
   int node = ra_get_best_spill_node(g);
   if (node == -1)
      assert(false);
   nir_def *def = defs[node].dest;
   if (defs[node].dest->index != dest_index(impl, defs[node].dest)) {
      nir_intrinsic_instr *store = nir_store_reg_for_def(def);
      def = store->src[1].ssa;
   }
   nir_instr *instr = def->parent_instr;
   nir_builder b = nir_builder_create(impl);
   b.cursor = nir_before_instr(instr);

   if (unlikely(debug & 0x100)) {
      printf("choose spill instr: ");
      nir_print_instr(instr, stdout);
      printf("\n");
   }

   nir_def *buffer_index = nir_imm_int(&b, 0x4);
   nir_def *buffer_offset = nir_imm_int(&b, c->shader->spill_offset);
   if (is_decl_reg(instr)) {
      nir_foreach_use_safe (src, def) {
         nir_instr *use_instr = nir_src_parent_instr(src);
         assert(use_instr->type == nir_instr_type_intrinsic);
         nir_intrinsic_instr *intr = nir_instr_as_intrinsic(use_instr);
         if (intr->intrinsic == nir_intrinsic_store_reg) {
            nir_def *data = intr->src[0].ssa;
            b.cursor = nir_before_instr(use_instr);
            nir_store_ssbo(&b, data, buffer_index, buffer_offset, .align_mul = 4);
            nir_instr_remove(use_instr);
         } else {
            assert(intr->intrinsic == nir_intrinsic_load_reg);
            b.cursor = nir_before_instr(use_instr);
            nir_def *new_def = nir_load_ssbo(&b, def->num_components,
                                             def->bit_size, buffer_index,
                                             buffer_offset, .align_mul = 4);
            nir_def_rewrite_uses(&intr->def, new_def);
            nir_instr_remove(use_instr);
         }
      }
      nir_instr_remove(instr);
   } else {
      nir_foreach_use_safe (src, def) {
         nir_instr *use_instr = nir_src_parent_instr(src);
         b.cursor = nir_before_instr(use_instr);
         nir_def *new_def = nir_load_ssbo(&b, def->num_components,
                                          def->bit_size, buffer_index,
                                          buffer_offset, .align_mul = 4);
         nir_src_rewrite(src, new_def);
      }
      nir_foreach_if_use_safe (src, def) {
         nir_if *nif = nir_src_parent_if(src);
         b.cursor = nir_before_cf_node(&nif->cf_node);
         nir_def *new_def = nir_load_ssbo(&b, def->num_components,
                                          def->bit_size, buffer_index,
                                          buffer_offset, .align_mul = 4);
         nir_src_rewrite(src, new_def);
      }
      b.cursor = nir_after_instr(instr);
      nir_store_ssbo(&b, def, buffer_index, buffer_offset, .align_mul = 4);
   }

   c->shader->spill_offset += 16;
}

static bool
mwv207_ra(struct mwv207_compile *c,
          nir_shader *nir,
          struct ra_regs *regs,
          uint32_t debug)
{
   nir_function_impl *impl = nir_shader_get_entrypoint(nir);

   nir_index_blocks(impl);
   nir_index_ssa_defs(impl);

   nir_foreach_function_impl(impl, nir) {
      nir_foreach_block(block, impl) {
         nir_foreach_instr(instr, block) {
            instr->pass_flags &= ~BITFIELD_RANGE(0,2);
         }
      }
   }

   uint32_t max_nodes = impl->ssa_alloc;
   uint32_t *live_map = ralloc_array(NULL, uint32_t, max_nodes);
   memset(live_map, 0xff, sizeof(uint32_t) * max_nodes);
   struct live_def *defs = rzalloc_array(NULL, struct live_def, max_nodes);
   bool hase_store = (c->shader->stage == PIPE_SHADER_GEOMETRY ||
                      c->shader->stage == PIPE_SHADER_TESS_CTRL);

   uint32_t num_nodes = live_defs(impl, defs, live_map, hase_store);
   struct ra_graph *g = ra_alloc_interference_graph(regs, num_nodes);

   for (uint32_t i = 0; i < num_nodes; i++) {
      nir_instr *instr = defs[i].instr;
      nir_def *dest = defs[i].dest;
      uint32_t c = dest->num_components - 1;

      if (instr->type == nir_instr_type_alu) {
         switch (nir_instr_as_alu(instr)->op) {
         case nir_op_fdiv:
         case nir_op_flog2:
         case nir_op_fsin:
         case nir_op_fcos:
            c = REG_CLASS_VIRT_VEC2T;
            break;
         default:
            break;
         }
      }

      if (instr->type == nir_instr_type_intrinsic) {
         nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);

         if (intr->intrinsic == nir_intrinsic_load_ubo ||
             intr->intrinsic == nir_intrinsic_load_global_amd) {

            if (dest->num_components == 2)
               c = REG_CLASS_VIRT_VEC2C;

            if (dest->num_components == 3)
               c = REG_CLASS_VIRT_VEC3C;
         }

         if (intr->intrinsic == nir_intrinsic_decl_reg)
            c = nir_intrinsic_num_components(intr) - 1;

         if (intr->intrinsic == nir_intrinsic_load_per_vertex_input)
            c = REG_CLASS_VEC4;
         if (intr->intrinsic == nir_intrinsic_load_ssbo ||
             intr->intrinsic == nir_intrinsic_load_shared)
            c = REG_CLASS_VEC4;

         if (intr->intrinsic == nir_intrinsic_image_deref_load)
            c = REG_CLASS_VEC4;

#if 0

         if (intr->intrinsic == nir_intrinsic_ssbo_atomic_comp_swap ||
             intr->intrinsic == nir_intrinsic_shared_atomic_comp_swap)
            c = REG_CLASS_VIRT_VEC2;
#endif
      }

      ra_set_node_class(g, i, ra_get_class_from_index(regs, c));
   }

   nir_foreach_block(block, impl) {
      nir_foreach_instr(instr, block) {
         if (instr->type != nir_instr_type_intrinsic)
            continue;

         nir_def *dest = dest_for_instr(instr);
         nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
         uint32_t reg;

         switch (intr->intrinsic) {
         case nir_intrinsic_store_deref: {
            nir_deref_instr *deref = nir_src_as_deref(intr->src[0]);
            uint32_t index = live_map[src_index(impl, &intr->src[1])];

            if (nir->info.stage == MESA_SHADER_FRAGMENT &&
                deref->var->data.location == FRAG_RESULT_DEPTH)
               ra_set_node_reg(g, index, ((0x40-1)*NUM_REG_TYPES+REG_TYPE_VIRT_SCALAR_Z));
            else
               ra_set_node_class(g, index, ra_get_class_from_index(regs, REG_CLASS_VEC4));
         } continue;
         case nir_intrinsic_load_input: {
            int intrinsic_base = nir_intrinsic_base(intr);

            if (nir->info.stage == MESA_SHADER_FRAGMENT &&
                c->shader->key.render_arrary) {
               nir_foreach_shader_in_variable(var, nir) {
                  if (var->data.location == VARYING_SLOT_LAYER)
                     intrinsic_base = 0;
                  else
                     intrinsic_base += 1;
               }
            }
            if (nir->info.stage == MESA_SHADER_FRAGMENT &&
                c->shader->useClipDistance) {
               nir_foreach_shader_in_variable(var, nir) {
                  if (var->data.location == VARYING_SLOT_CLIP_DIST0 ||
                      var->data.location == VARYING_SLOT_CLIP_DIST1)
                     intrinsic_base += nir_src_as_int(intr->src[0]) * 4 + nir_intrinsic_component(intr);
               }
            }

            if (nir->info.stage == MESA_SHADER_TESS_EVAL)
               if (intr->num_components != 0 && nir_src_is_const(intr->src[0]))
                  intrinsic_base += nir_src_as_int(intr->src[0]);

            if (nir->info.stage == MESA_SHADER_FRAGMENT ||
                nir->info.stage == MESA_SHADER_VERTEX)
               if (nir_src_is_const(intr->src[0]) &&
                   nir_intrinsic_io_semantics(intr).location != VARYING_SLOT_CLIP_DIST0 &&
                   nir_intrinsic_io_semantics(intr).location != VARYING_SLOT_CLIP_DIST1)
                  intrinsic_base += nir_src_as_int(intr->src[0]);

            if (nir->info.stage == MESA_SHADER_TESS_EVAL &&
                intrinsic_base < c->shader->inner_outer_count) {

               reg = intrinsic_base * NUM_REG_TYPES + REG_TYPE_VIRT_SCALAR_X + nir_intrinsic_component(intr);
            } else {
               reg = intrinsic_base * NUM_REG_TYPES + (uint32_t[]) {
                  REG_TYPE_VIRT_SCALAR_X,
                  REG_TYPE_VIRT_VEC2_XY,
                  REG_TYPE_VIRT_VEC3_XYZ,
                  REG_TYPE_VEC4,
               }[dest->num_components - 1];
            }
         } break;
         case nir_intrinsic_load_vertex_id:
            reg = c->shader->infile.num_reg * NUM_REG_TYPES + REG_TYPE_VIRT_SCALAR_X;
            break;
         case nir_intrinsic_load_instance_id:
            reg = c->shader->infile.num_reg * NUM_REG_TYPES + REG_TYPE_VIRT_SCALAR_Y;
            break;

         case nir_intrinsic_ssbo_atomic_swap:
         case nir_intrinsic_shared_atomic_swap:
         case nir_intrinsic_image_deref_atomic_swap:
            reg = c->shader->infile.num_reg * NUM_REG_TYPES + REG_TYPE_VIRT_VEC2_XY;
            break;
         case nir_intrinsic_load_deref: {
            nir_deref_instr *deref = nir_src_as_deref(intr->src[0]);
            int idx = REG_TYPE_VIRT_SCALAR_X;
            int offset = 0;

            if (deref->deref_type == nir_deref_type_array) {
               if (nir_src_is_const(deref->arr.index)) {
                  idx += nir_src_as_uint(deref->arr.index);
                  offset += nir_src_as_uint(deref->arr.index);

                  while (deref->deref_type != nir_deref_type_var)
                     deref = nir_deref_instr_parent(deref);

                  if (!deref->var->data.compact)
                     idx = REG_TYPE_VIRT_SCALAR_X;
               } else {

               }
            }

            while (deref->deref_type != nir_deref_type_var)
               deref = nir_deref_instr_parent(deref);

            if (dest->num_components == 1)
               idx += deref->var->data.location_frac;

            offset += deref->var->data.driver_location;
            reg = (c->shader->infile.num_reg + offset) * NUM_REG_TYPES +
            (uint32_t[]) {
               idx,
               REG_TYPE_VIRT_VEC2_XY,
               REG_TYPE_VIRT_VEC3_XYZ,
               REG_TYPE_VEC4,
            }[dest->num_components - 1];
         } break;
         default:
            continue;
         }

         ra_set_node_reg(g, live_map[dest_index(impl, dest)], reg);
      }
   }

   for (uint32_t i = 0; i < num_nodes; i++) {
      assert(defs[i].live_start <= defs[i].live_end);
      for (uint32_t j = 0; j < i; j++) {
         if (defs[i].live_start >= defs[j].live_end || defs[j].live_start >= defs[i].live_end)
            continue;

         ra_add_node_interference(g, i, j);
      }
   }

   bool ok = ra_allocate(g);

   if (!ok)
      mwv207_spill_reg(c, g, num_nodes, defs, impl, debug);
   ralloc_free(defs);
   c->g = g;
   c->regs = regs;
   c->live_map = live_map;
   c->num_nodes = num_nodes;
   return ok;
}

void
mwv207_init_reg_state(struct mwv207_context *ctx, struct mwv207_compile *c,
                      nir_shader *nir)
{
   struct ra_regs *regs = ra_alloc_reg_set(NULL, 0x40 *
                                           NUM_REG_TYPES, false);

   struct ra_class *classes[NUM_REG_CLASSES];
   for (int c = 0; c < NUM_REG_CLASSES; c++)
      classes[c] = ra_alloc_reg_class(regs);

   for (int r = 0; r < NUM_REG_TYPES * 0x40; r++)
      ra_class_add_reg(classes[reg_get_class(r)], r);

   for (int r = 0; r < 0x40; r++)
      for (int i = 0; i < NUM_REG_TYPES; i++)
         for (int j = 0; j < i; j++)
            if (reg_writemask[i] & reg_writemask[j])
               ra_add_reg_conflict(regs, NUM_REG_TYPES * r + i,
                                         NUM_REG_TYPES * r + j);

   ra_set_finalize(regs, q_values);

   /* FIX(jm9100-oss): 原循环无上限且循环体只释放 c->g/c->live_map ——
    * 不重建就再次进入 mwv207_ra() 会 use-after-free; 若 spilling 永不收敛
    * (mwv207_spill_reg() 找不到候选时 assert(false), release 下是
    * __builtin_unreachable()) 则死循环冻结应用。加上限, 失败时明确报错退出。 */
   for (int retry = 0; ; retry++) {
      if (mwv207_ra(c, nir, regs, ctx->ws->debug))
         break;
      if (retry >= 32) {
         fprintf(stderr, "mwv207: register allocation failed to converge after %d spill rounds\n", retry);
         unreachable("mwv207 RA: spill did not converge");
      }
      ralloc_free(c->g);
      ralloc_free(c->live_map);
   }

   if (c->shader->spill_offset) {
      struct pipe_resource templ;
      c->shader->spill_buffer.buffer_offset = 0;
      c->shader->spill_buffer.buffer_size = c->shader->spill_offset;
      memset(&templ, 0, sizeof(templ));
      templ.target = PIPE_BUFFER;
      templ.bind |= PIPE_BIND_LINEAR;
      templ.width0 = c->shader->spill_offset;
      c->shader->spill_buffer.buffer = ctx->base.screen->resource_create(ctx->base.screen, &templ);
   }

}

void
mwv207_free_reg_state(struct mwv207_compile *c)
{

   uint32_t j = 0;

   for (uint32_t i = 0; i < c->num_nodes; i++)
      j = MAX2(j, reg_get_base(c, ra_get_node_reg(c->g, i)) + 1);

   ralloc_free(c->g);
   ralloc_free(c->regs);
   ralloc_free(c->live_map);

   for (int idx = 0; idx < c->shader->infile.num_reg; ++idx)
      j = MAX2(j, c->shader->infile.reg[idx].reg + 1);

   c->shader->num_temps = j;
}
