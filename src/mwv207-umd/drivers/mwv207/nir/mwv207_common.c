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
#include "mwv207_common.h"

uint32_t
inst_swiz_compose(uint32_t swz1, uint32_t swz2)
{
   return INST_SWIZ_X((swz1 >> (((swz2 >> 0) & 3) * 2)) & 3) |
          INST_SWIZ_Y((swz1 >> (((swz2 >> 2) & 3) * 2)) & 3) |
          INST_SWIZ_Z((swz1 >> (((swz2 >> 4) & 3) * 2)) & 3) |
          INST_SWIZ_W((swz1 >> (((swz2 >> 6) & 3) * 2)) & 3);
}

uint32_t
inst_write_mask_compose(uint32_t wm1, uint32_t wm2)
{
   uint32_t wm = 0;

   for (uint32_t i = 0, j = 0; i < 4; i++) {
      if (wm2 & (1 << i)) {
         if (wm1 & (1 << j))
            wm |= (1 << i);

         j++;
      }
   }

   return wm;
}

struct mwv207_inst_src
src_swizzle(struct mwv207_inst_src src, uint32_t swizzle)
{
   if (src.rgroup != 0x7)
      src.swiz = inst_swiz_compose(src.swiz, swizzle);

   return src;
}

bool
is_sysval(nir_instr *instr)
{
   if (instr->type != nir_instr_type_intrinsic)
      return false;

   nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
   return intr->intrinsic == nir_intrinsic_load_front_face ||
          intr->intrinsic == nir_intrinsic_load_frag_coord;
}

uint32_t
src_index(nir_function_impl *impl, nir_src *src)
{
   nir_intrinsic_instr *load = nir_load_reg_for_def(src->ssa);

   if (load) {
      nir_def *reg = load->src[0].ssa;
      ASSERTED nir_intrinsic_instr *decl = nir_reg_get_decl(reg);
      assert(nir_intrinsic_base(load) == 0);
      assert(nir_intrinsic_num_array_elems(decl) == 0);

      return reg->index;
   }

   return src->ssa->index;
}

uint32_t
dest_index(nir_function_impl *impl, nir_def *def)
{
   nir_intrinsic_instr *store = nir_store_reg_for_def(def);

   if (store) {
      nir_def *reg = store->src[1].ssa;
      ASSERTED nir_intrinsic_instr *decl = nir_reg_get_decl(reg);
      assert(nir_intrinsic_base(store) == 0);
      assert(nir_intrinsic_num_array_elems(decl) == 0);

      return reg->index;
   }

   return def->index;

}

static void
update_swiz_mask(nir_alu_instr *alu, nir_def *dest, uint32_t *swiz, uint32_t *mask)
{
   if (!swiz)
      return;

   bool is_vec = dest != NULL;
   uint32_t swizzle = 0, write_mask = 0;

   for (uint32_t i = 0; i < alu->def.num_components; i++) {

      if (is_vec && alu->src[i].src.ssa != dest)
         continue;

      uint32_t src_swiz = is_vec ? alu->src[i].swizzle[0] : alu->src[0].swizzle[i];
      swizzle |= (*swiz >> src_swiz * 2 & 3) << i * 2;

      if (*mask & (1 << src_swiz))
         write_mask |= 1 << i;
   }

   *swiz = swizzle;
   *mask = write_mask;
}

nir_def *
real_dest(nir_def *dest, uint32_t *swiz, uint32_t *mask)
{
   if (!dest)
      return dest;

   bool can_bypass_src = !nir_def_used_by_if(dest);
   nir_instr *p_instr = dest->parent_instr;

   nir_foreach_use(use_src, dest) {
      nir_instr *instr = nir_src_parent_instr(use_src);

      switch (instr->type) {
      case nir_instr_type_tex:
         if (p_instr->type == nir_instr_type_alu &&
             nir_instr_as_alu(p_instr)->op == nir_op_mov)
            break;
         FALLTHROUGH;
      default:
         can_bypass_src = false;
         break;
      }

      if (instr->type != nir_instr_type_alu)
         continue;

      nir_alu_instr *alu = nir_instr_as_alu(instr);

      switch (alu->op) {
      case nir_op_vec2:
      case nir_op_vec3:
      case nir_op_vec4:
         assert(!nir_def_used_by_if(dest));
         nir_foreach_use(use_src, dest)
            assert(nir_src_parent_instr(use_src) == instr);

         update_swiz_mask(alu, dest, swiz, mask);
         break;
      case nir_op_mov: {
         switch (dest->parent_instr->type) {
         case nir_instr_type_alu:
         case nir_instr_type_tex:
            break;
         default:
            continue;
         }

         if (nir_def_used_by_if(dest) || list_length(&dest->uses) > 1)
            continue;

         update_swiz_mask(alu, NULL, swiz, mask);
         break;
      };
      default:
         continue;
      }

      assert(!(instr->pass_flags & BYPASS_SRC));
      instr->pass_flags |= BYPASS_DST;
      return real_dest(&alu->def, swiz, mask);
   }

   if (can_bypass_src && !(p_instr->pass_flags & BYPASS_DST)) {
      if (p_instr->type == nir_instr_type_intrinsic) {
         nir_intrinsic_instr *tmp_intr = nir_instr_as_intrinsic(p_instr);
         if (tmp_intr->intrinsic == nir_intrinsic_image_deref_atomic)
            return dest;
      }
      p_instr->pass_flags |= BYPASS_SRC;
      return NULL;
   }

   return dest;
}

nir_def *
dest_for_instr(nir_instr *instr)
{
   nir_def *dest = NULL;

   switch (instr->type) {
   case nir_instr_type_alu:
      dest = &nir_instr_as_alu(instr)->def;
      break;
   case nir_instr_type_tex:
      dest = &nir_instr_as_tex(instr)->def;
      break;
   case nir_instr_type_intrinsic: {
      nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
      if (intr->intrinsic == nir_intrinsic_load_front_face ||
          intr->intrinsic == nir_intrinsic_load_frag_coord)
         break;
      const nir_intrinsic_info *info = &nir_intrinsic_infos[intr->intrinsic];
      if (info->has_dest) {
         dest = &intr->def;
         if (intr->intrinsic == nir_intrinsic_ssbo_atomic ||
             intr->intrinsic == nir_intrinsic_image_deref_atomic) {
            assert(dest);
            nir_def *dest_tmp = real_dest(dest, NULL, NULL);
            if (dest_tmp)
               return dest_tmp;
            else
               return dest;
         }
      }
   } break;
   case nir_instr_type_deref:
      return NULL;
   default:
      break;
   }

   return real_dest(dest, NULL, NULL);
}

int
const_add(uint64_t *c, uint64_t value)
{
   for (uint32_t i = 0; i < 4; i++) {
      if (c[i] == value || !c[i]) {
         c[i] = value;
         return i;
      }
   }

   return -1;
}

int
mwv207_assemble(uint32_t *out, const struct mwv207_inst *inst)
{

   if (inst->imm && inst->src[2].use)
      return 1;

   assert(!(inst->opcode & ~0x7f));

   uint32_t opcode = inst->opcode;
   uint32_t extcode = 0;
   if (opcode == 0x7F)
      extcode = inst->ext_opcode;

   struct instr *code = (struct instr *)out;

   code->opc       = (opcode & 0x3f);
   code->cond      = inst->cond;
   code->sat       = inst->sat;
   code->dst_use   = inst->dst.use;
   code->dst_amode = inst->dst.amode;
   code->dst_reg   = inst->dst.reg;
   code->dst_comps = inst->dst.write_mask;
   code->tex_id    = inst->tex.id;

   code->tex_amode = inst->tex.amode;
   code->tex_swiz  = inst->tex.swiz;
   code->src0_use  = inst->src[0].use;
   code->src0_reg  = inst->src[0].reg;
   code->src0_swiz = inst->src[0].swiz;
   code->src0_neg  = inst->src[0].neg;
   code->src0_abs  = inst->src[0].abs;
   code->type_bit2 = (inst->type & 0x4) >> 2;

   out[1] |= inst->round;

   code->src0_amode  = inst->src[0].amode;
   code->src0_rgroup = inst->src[0].rgroup;
   code->src1_use    = inst->src[1].use;
   code->src1_reg    = inst->src[1].reg;
   code->src1_swiz   = inst->src[1].swiz;
   code->src1_neg    = inst->src[1].neg;
   code->src1_abs    = inst->src[1].abs;
   code->src1_amode  = inst->src[1].amode;
   code->opcode_bit6 = (opcode & 0x40) >> 6;
   code->type_bit01  = (inst->type & 0x3);

   code->src1_rgroup = inst->src[1].rgroup;
   code->src2_use    = inst->src[2].use;
   code->src2_reg    = inst->src[2].reg;
   code->src2_swiz   = inst->src[2].swiz;
   code->src2_neg    = inst->src[2].neg;
   code->src2_abs    = inst->src[2].abs;
   code->src2_amode  = inst->src[2].amode;
   code->src2_rgroup = inst->src[2].rgroup;
   code->sel_0       = inst->sel_bit0;
   code->sel_1       = inst->sel_bit1;
   code->dst_full    = inst->dst_full;

   out[3] |= (extcode << 0x4);
   out[3] |= (inst->imm << 0x7);

   return 0;
}
