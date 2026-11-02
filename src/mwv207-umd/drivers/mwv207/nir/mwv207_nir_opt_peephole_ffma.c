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

static inline bool
are_all_uses_fadd(nir_def *def)
{
   if (!list_is_empty(&def->uses))
      return false;

   nir_foreach_use(use_src, def) {
      nir_instr *use_instr = nir_src_parent_instr(use_src);

      if (use_instr->type != nir_instr_type_alu)
         return false;

      nir_alu_instr *use_alu = nir_instr_as_alu(use_instr);

      switch (use_alu->op) {
      case nir_op_fadd:
         break;

      case nir_op_mov:
      case nir_op_fneg:
      case nir_op_fabs:
         if (!are_all_uses_fadd(&use_alu->def))
            return false;

         break;

      default:
         return false;
      }
   }

   return true;
}

static nir_alu_instr *
get_mul_for_src(nir_alu_src *src, unsigned num_components,
                uint8_t swizzle[4], bool *negate, bool *abs)
{
   uint8_t swizzle_tmp[4];

   nir_instr *instr = src->src.ssa->parent_instr;

   if (instr->type != nir_instr_type_alu)
      return NULL;

   nir_alu_instr *alu = nir_instr_as_alu(instr);

   if (alu->exact)
      return NULL;

   switch (alu->op) {
   case nir_op_mov:
      alu = get_mul_for_src(&alu->src[0], alu->def.num_components,
                            swizzle, negate, abs);
      break;

   case nir_op_fneg:
      alu = get_mul_for_src(&alu->src[0], alu->def.num_components,
                            swizzle, negate, abs);
      *negate = !*negate;
      break;

   case nir_op_fabs:
      alu = get_mul_for_src(&alu->src[0], alu->def.num_components,
                            swizzle, negate, abs);
      *negate = false;
      *abs = true;
      break;

   case nir_op_fmul:

      if (!are_all_uses_fadd(&alu->def))
         return NULL;

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
mwv207_nir_opt_peephole_ffma_block(nir_builder *b, nir_block *block)
{
   bool progress = false;

   nir_foreach_instr_safe(instr, block) {
      if (instr->type != nir_instr_type_alu)
         continue;

      nir_alu_instr *add = nir_instr_as_alu(instr);

      if (add->op != nir_op_fadd)
         continue;

      if (add->exact)
         continue;

      if (add->src[0].src.ssa == add->src[1].src.ssa)
         continue;

      nir_alu_instr *mul;
      uint8_t add_mul_src, swizzle[4];
      bool negate, abs;

      for (add_mul_src = 0; add_mul_src < 2; add_mul_src++) {
         for (unsigned i = 0; i < 4; i++)
            swizzle[i] = i;

         negate = false;
         abs = false;

         mul = get_mul_for_src(&add->src[add_mul_src],
                               add->def.num_components,
                               swizzle, &negate, &abs);

         if (mul != NULL)
            break;
      }

      if (mul == NULL)
         continue;

      unsigned bit_size = add->def.bit_size;

      nir_def *mul_src[2];
      mul_src[0] = mul->src[0].src.ssa;
      mul_src[1] = mul->src[1].src.ssa;

      b->cursor = nir_before_instr(&add->instr);

      if (abs) {
         for (unsigned i = 0; i < 2; i++)
            mul_src[i] = nir_fabs(b, mul_src[i]);
      }

      if (negate)
         mul_src[0] = nir_fneg(b, mul_src[0]);

      nir_alu_instr *ffma = nir_alu_instr_create(b->shader, nir_op_ffma);

      for (unsigned i = 0; i < 2; i++) {
         ffma->src[i].src = nir_src_for_ssa(mul_src[i]);

         for (unsigned j = 0; j < add->def.num_components; j++)
            ffma->src[i].swizzle[j] = mul->src[i].swizzle[swizzle[j]];
      }

      nir_alu_src_copy(&ffma->src[2], &add->src[1 - add_mul_src]);

      nir_def_init(&ffma->instr, &ffma->def,
                   add->def.num_components, bit_size);
      nir_def_rewrite_uses(&add->def, &ffma->def);
      assert(list_is_empty(&add->def.uses));

      nir_builder_instr_insert(b, &ffma->instr);
      nir_instr_remove(&add->instr);

      progress = true;
   }

   return progress;
}

static bool
mwv207_nir_opt_peephole_ffma_impl(nir_function_impl *impl)
{
   bool progress = false;

   nir_builder builder = nir_builder_create(impl);

   nir_foreach_block(block, impl) {
      progress |= mwv207_nir_opt_peephole_ffma_block(&builder, block);
   }

   if (progress)
      nir_metadata_preserve(impl, nir_metadata_block_index |
                            nir_metadata_dominance);

   return progress;
}

bool
mwv207_nir_opt_peephole_ffma(nir_shader *shader)
{
   bool progress = false;

   nir_foreach_function(function, shader) {
      if (function->impl)
         progress |= mwv207_nir_opt_peephole_ffma_impl(function->impl);
   }

   return progress;
}
