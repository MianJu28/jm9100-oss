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
#include "nir.h"

static nir_load_const_instr *
get_load_const_for_src(nir_alu_src *src)
{
   if (src->src.ssa->parent_instr->type != nir_instr_type_load_const)
      return NULL;

   return nir_instr_as_load_const(src->src.ssa->parent_instr);
}

static bool
mwv207_nir_opt_peephole_fdiv_block(nir_builder *b, nir_block *block)
{
   bool progress = false;

   nir_foreach_instr_safe(instr, block) {
      if (instr->type != nir_instr_type_alu)
         continue;

      nir_alu_instr *fdiv = nir_instr_as_alu(instr);

      if (fdiv->op != nir_op_fdiv)
         continue;

      if (fdiv->exact)
         continue;

      nir_alu_src *src_of_divisor = &fdiv->src[1];
      nir_load_const_instr *load_const = get_load_const_for_src(src_of_divisor);

      if (load_const == NULL)
         continue;

      if (load_const->def.num_components != 1)
         continue;

      b->cursor = nir_before_instr(&fdiv->instr);

      nir_alu_instr *frcp = nir_alu_instr_create(b->shader, nir_op_frcp);

      nir_alu_src_copy(&frcp->src[0], &fdiv->src[1]);

      unsigned bit_size = fdiv->def.bit_size;
      nir_def_init(&frcp->instr, &frcp->def, 1, bit_size);

      nir_alu_instr *fmul = nir_alu_instr_create(b->shader, nir_op_fmul);

      nir_alu_src_copy(&fmul->src[0], &fdiv->src[0]);
      fmul->src[1].src = nir_src_for_ssa(&frcp->def);

      bit_size = fdiv->def.bit_size;
      nir_def_init(&fmul->instr, &fmul->def,
         fdiv->def.num_components, bit_size);

      nir_def_rewrite_uses(&fdiv->def, &fmul->def);
      assert(list_is_empty(&fdiv->def.uses));

      nir_builder_instr_insert(b, &frcp->instr);
      nir_builder_instr_insert(b, &fmul->instr);
      nir_instr_remove(&fdiv->instr);

      progress = true;
   }

   return progress;
}

static bool
mwv207_nir_opt_peephole_fdiv_impl(nir_function_impl *impl)
{
   bool progress = false;

   nir_builder builder = nir_builder_create(impl);

   nir_foreach_block(block, impl) {
      progress |= mwv207_nir_opt_peephole_fdiv_block(&builder, block);
   }

   if (progress)
      nir_metadata_preserve(impl, nir_metadata_block_index |
                            nir_metadata_dominance);

   return progress;
}

bool
mwv207_nir_opt_peephole_fdiv(nir_shader *shader)
{
   bool progress = false;

   nir_foreach_function(function, shader) {
      if (function->impl)
         progress |= mwv207_nir_opt_peephole_fdiv_impl(function->impl);
   }

   return progress;
}
