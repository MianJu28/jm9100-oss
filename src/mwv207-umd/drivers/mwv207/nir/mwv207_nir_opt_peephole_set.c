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

#include "mwv207_nir_opt.h"
#include "compiler/nir/nir_builder.h"

static nir_alu_instr *
get_cmp_for_src(nir_alu_src *src, unsigned num_components,
                uint8_t swizzle[4])
{
   uint8_t swizzle_tmp[4];

   nir_instr *instr = src->src.ssa->parent_instr;

   if (instr->type != nir_instr_type_alu)
      return NULL;

   nir_alu_instr *alu = nir_instr_as_alu(instr);

   if (alu->exact)
      return NULL;

   switch (alu->op) {
   case nir_op_feq32:
   case nir_op_ine32:
   case nir_op_fge32:
   case nir_op_flt32:
   case nir_op_flt:
      break;

   case nir_op_mov:
      alu = get_cmp_for_src(&alu->src[0], alu->def.num_components,
                            swizzle);
      break;

   default:
      return NULL;
   }

   if (!alu)
      return NULL;

   memcpy(swizzle_tmp, swizzle, 4 * sizeof(uint8_t));

   for (int i = 0; i < num_components; i++)
      swizzle[i] = swizzle_tmp[src->swizzle[i]];

   return alu;
}

static bool
mwv207_nir_opt_peephole_set_block(nir_builder *b, nir_block *block)
{
   bool progress = false;

   nir_foreach_instr_safe(instr, block) {
      if (instr->type != nir_instr_type_alu)
         continue;

      nir_alu_instr *b2f32 = nir_instr_as_alu(instr);

      if (b2f32->op != nir_op_b2f32)
         continue;

      if (b2f32->exact)
         continue;

      nir_alu_instr *cmp;
      uint8_t swizzle[4];

      for (unsigned i = 0; i < 4; i++)
         swizzle[i] = i;

      cmp = get_cmp_for_src(&b2f32->src[0],
                            b2f32->def.num_components,
                            swizzle);

      if (cmp == NULL)
         continue;

      nir_op set_op;

      switch (cmp->op) {
      case nir_op_fge32:
         set_op = nir_op_sge;
         break;

      case nir_op_flt32:
      case nir_op_flt:
         set_op = nir_op_slt;
         break;

      case nir_op_feq32:
         set_op = nir_op_seq;
         break;

      case nir_op_ine32:
         set_op = nir_op_sne;
         break;

      default:
         unreachable("Not one of the comparison ops expected");
      }

      unsigned bit_size = b2f32->def.bit_size;

      nir_def *cmp_src[2];
      cmp_src[0] = cmp->src[0].src.ssa;
      cmp_src[1] = cmp->src[1].src.ssa;

      b->cursor = nir_before_instr(&b2f32->instr);

      nir_alu_instr *set = nir_alu_instr_create(b->shader, set_op);

      for (unsigned i = 0; i < 2; i++) {
         set->src[i].src = nir_src_for_ssa(cmp_src[i]);

         for (unsigned j = 0; j < b2f32->def.num_components; j++)
            set->src[i].swizzle[j] = cmp->src[i].swizzle[swizzle[j]];
      }

      nir_def_init(&set->instr, &set->def,
                   b2f32->def.num_components, bit_size);
      nir_def_rewrite_uses(&b2f32->def, &set->def);

      nir_builder_instr_insert(b, &set->instr);
      nir_instr_remove(&b2f32->instr);

      progress = true;
   }

   return progress;
}

static bool
mwv207_nir_opt_peephole_set_impl(nir_function_impl *impl)
{
   bool progress = false;

   nir_builder builder = nir_builder_create(impl);

   nir_foreach_block(block, impl) {
      progress |= mwv207_nir_opt_peephole_set_block(&builder, block);
   }

   if (progress)
      nir_metadata_preserve(impl, nir_metadata_block_index |
                            nir_metadata_dominance);

   return progress;
}

bool
mwv207_nir_opt_peephole_set(nir_shader *shader)
{
   bool progress = false;

   nir_foreach_function(function, shader) {
      if (function->impl)
         progress |= mwv207_nir_opt_peephole_set_impl(function->impl);
   }

   return progress;
}
