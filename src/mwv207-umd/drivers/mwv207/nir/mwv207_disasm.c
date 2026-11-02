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
#include "util/u_math.h"
#include "util/half_float.h"
#include "tgsi/tgsi_info.h"

#include "mwv207_common.h"
#include "mwv207_compiler.h"
#include "mwv207_disasm.h"

static uint32_t _cond_op_2_src_count[] = {
   0,
   2,
   2,
   2,
   2,
   2,
   2,
   2,
   2,
   2,
   1,
   1,
   1,
   1,
   1,
   1,
   1,
   1,
   1,
   1,
   1,
   1,
   1,
   2,
   0,
   0,
};

static void
print_with_update_length(uint32_t *print_length, char *format, ...)
{
   char buff[128] = {0};
   va_list ap;
   va_start(ap, format);
   vsnprintf(buff, sizeof(buff), format, ap);
   va_end(ap);
   printf("%s", buff);
   (*print_length) += strlen(buff);
}

static void
print_align(uint32_t *print_length, uint32_t max_length)
{
   int pad_num_of_space = max_length - *print_length;
   char buff[128] = {0};

   if (pad_num_of_space <= 0) {
      printf(" ");
      return;
   }

   memset(buff, ' ', pad_num_of_space);
   printf("%s", buff);
   *print_length = 0;
}

static void
print_inst_crontrol(struct mwv207_inst_ctrl *inst_ctrl, struct mwv207_inst *operands, uint32_t *print_length)
{
   uint32_t base_opcode = operands->opcode;
   uint32_t ext_opcode = operands->ext_opcode;
   uint32_t flags = operands->flags;

   static const char *_str_condop[] = {
      "",
      ".gt",
      ".lt",
      ".ge",
      ".le",
      ".eq",
      ".ne",
      ".and",
      ".or",
      ".xor",
      ".not",
      ".nz",
      ".gez",
      ".gz",
      ".lez",
      ".lz",
      ".fin",
      ".inf",
      ".nan",
      ".nml",
      ".anymsb",
      ".allmsb",
      ".selmsb",
      ".uc",
      ".hlp",
      ".nothlp",
   };

   static const char *_str_rounding_mode[] = {
      "",
      ".rtz",
      ".rtne"
   };

   static const char *_str4_rounding_mode[] = {
      ".rtn",
      ".rtz",
      ".rtne",
      ".rtp"
   };

   static const char *_str_inst_type[] = {
      ".f32",
      ".f16",
      ".s32",
      ".s16",
      ".s8",
      ".u32",
      ".u16",
      ".u8",
      "",
      "",
      ".s64",
      ".snorm16",
      ".snorm8",
      ".u64",
      ".unorm16",
      ".unorm8",
   };

   static const char *_str_load_store_attr_client[] = {
      ".VS",
      ".TCS",
      ".TES",
      ".GS"
   };

   static const char *_str_load_store_attr_layout[] = {
      ".ild",
      ".lnr"
   };

   PRINT("%s", _str_condop[inst_ctrl->cond_opcode]);

   if (inst_ctrl->is_has4_rounding_mode)
      PRINT("%s", _str4_rounding_mode[inst_ctrl->rounding_mode]);
   else
      PRINT("%s", _str_rounding_mode[inst_ctrl->rounding_mode]);

   if (base_opcode == 0x39 || base_opcode == 0x3A)
      PRINT(".pack");

   if (inst_ctrl->is_result_sat)
      PRINT(".sat");

   if (inst_ctrl->is_skip_for_helper_kickoff)
      PRINT(".skpHp");

   if (!inst_ctrl->is_denorm && (flags & (((1U<<5)|(1U<<6))|((1U<<7)|(1U<<8))|(1U<<9))))
      PRINT(".denorm");

   if (inst_ctrl->is_end_of_BB)
      PRINT(".ebb");

   if ((flags & ((1U<<0)|((1U<<5)|(1U<<6))|((1U<<7)|(1U<<8))|(1U<<10))) && inst_ctrl->u.memory_access_ctrl.is_unallocate)
      PRINT(".UA");

   if (flags & (((1U<<0)|((1U<<5)|(1U<<6))|((1U<<7)|(1U<<8))|(1U<<10))|(1U<<11)|(1U<<4))) {
      if (inst_ctrl->u.memory_access_ctrl.is_access_local_storage)
         PRINT(".local");

      if (inst_ctrl->u.memory_access_ctrl.is_under_evis_mode)
         assert(false);

      if (flags & (((1U<<5)|(1U<<6))|(1U<<4))) {
         if (inst_ctrl->u.memory_access_ctrl.u.load_store_ctrl.offset_left_shift)
            PRINT(".ls%d", inst_ctrl->u.memory_access_ctrl.u.load_store_ctrl.offset_left_shift);

         if (inst_ctrl->u.memory_access_ctrl.u.load_store_ctrl.is_offset_x3)
            PRINT(".x3");
      }
   } else if (base_opcode == 0x43) {
      PRINT(".rg%d", inst_ctrl->u.select_map_ctrl.range_to_match);

      if (inst_ctrl->u.select_map_ctrl.is_component_select)
         PRINT(".compSel");
   } else if (base_opcode == 0x45) {

      assert(false);
   } else if (base_opcode == 0x78 ||
              base_opcode == 0x42) {
      if (inst_ctrl->u.load_store_attr_ctrl.sh_stage_client)
         PRINT("%s", _str_load_store_attr_client[inst_ctrl->u.load_store_attr_ctrl.sh_stage_client]);

      if (inst_ctrl->u.load_store_attr_ctrl.attr_layout)
         PRINT("%s", _str_load_store_attr_layout[inst_ctrl->u.load_store_attr_ctrl.attr_layout]);
   } else if (base_opcode == 0x71) {
      if (inst_ctrl->u.src_select)
         PRINT(".ss%d", inst_ctrl->u.src_select);
   } else if (base_opcode == 0x7F && ext_opcode == 0x1) {
      if (inst_ctrl->u.emit_ctrl.is_need_restart_prim)
         PRINT(".restart");

      if (!inst_ctrl->u.emit_ctrl.is_no_jmp_to_end_on_max_vtx_cnt)
         PRINT(".JmpToEnd");
   } else if (flags & (1U<<1)) {
      if (inst_ctrl->u.is_inf_x0_to_zero)
         PRINT(".infX0To0");
   }

   if (inst_ctrl->inst_type == 0x0) {
      if (flags & ((1U<<7)|(1U<<3)|(1U<<12)))
         PRINT("%s", _str_inst_type[inst_ctrl->inst_type]);
   } else
      PRINT("%s", _str_inst_type[inst_ctrl->inst_type]);
}

static void
print_inst_dst(struct mwv207_inst_dst *dst,
               enum dst_addr_reg_type addr_reg_type,
               bool is_need_comma,
               uint32_t *print_length)
{
   char dst_reg_letter;

   static const char *_str_write_mask[] = {
      ".",   ".x",   ".y",   ".xy",
      ".z",  ".xz",  ".yz",  ".xyz",
      ".w",  ".xw",  ".yw",  ".xyw",
      ".zw", ".xzw", ".yzw", ""
   };

   static const char *_str_dynamic_indexing[] = {
      "",
      "[a0.x]",
      "[a0.y]",
      "[a0.z]",
      "[a0.w]",
      "[aL]",
      "[InstanceId]",
      "[b0]"
   };

   if (!dst->use) {

      if (dst->write_mask)
         PRINT("%s ", _str_write_mask[dst->write_mask]);

      print_align(print_length, 0x1C);
      PRINT("void,");
      return;
   }

   print_align(print_length, 0x1C);

   if (addr_reg_type == DST_ADDR_REG_TYPE_A0)
      dst_reg_letter = 'a';
   else if (addr_reg_type == DST_ADDR_REG_TYPE_B0)
      dst_reg_letter = 'b';
   else
      dst_reg_letter = 'r';

   uint32_t reg = dst->reg |
                  (dst->reg_bit7 << 7) |
                  (dst->reg_bit8 << 8);

   PRINT("%c%u", dst_reg_letter, reg);

   PRINT("%s", _str_dynamic_indexing[dst->amode]);

   PRINT("%s", _str_write_mask[dst->write_mask]);

   if (is_need_comma)
      PRINT(", ");
}

static uint64_t
get_bits(uint64_t data, uint32_t start, uint32_t end)
{
   if (end >= start) {
      uint64_t mask = (uint64_t)(~0ULL >> (63 - end + start));
      return (data >> start) & mask;
   } else {
      uint64_t mask = (uint64_t)(~0ULL >> (63 - start + end));
      return (data >> end) & mask;
   }
}

static inline void
print_swiz_comp(uint8_t swiz_comp, uint32_t *print_length)
{
   switch (swiz_comp) {
   case INST_SWIZ_COMP_X:
      PRINT("x");
      break;

   case INST_SWIZ_COMP_Y:
      PRINT("y");
      break;

   case INST_SWIZ_COMP_Z:
      PRINT("z");
      break;

   case INST_SWIZ_COMP_W:
      PRINT("w");
      break;

   default:
      abort();
      break;
   }
}

static void
print_swiz(uint8_t swiz, uint32_t *print_length)
{

   if (swiz == 0xe4)
      return;

   const uint32_t x = swiz & 0x3;
   const uint32_t y = (swiz & 0x0C) >> 2;
   const uint32_t z = (swiz & 0x30) >> 4;
   const uint32_t w = (swiz & 0xc0) >> 6;

   PRINT(".");
   print_swiz_comp(x, print_length);
   print_swiz_comp(y, print_length);
   print_swiz_comp(z, print_length);
   print_swiz_comp(w, print_length);
}

static void
print_each_src(struct mwv207_inst_src *src,
               bool is_need_comma,
               uint32_t index,
               enum kinds_of_inst kind_of_inst,
               uint32_t *print_length)
{
   static const char *_str_src_type[] = {
      "r",
      "vFace",
      "c",
      "c",
      "r",
      "Ext",
      "L",
      "I"
   };

   static const char *_str_inst_index[] = {
      "instanceId",
      "in_primitiveId",
      "out_primitiveId",
      "iu",
      "is",
      "cash_id",
      "remap",
      "%#@!$",
      "remap0",
      "remap1",
      "remap2",
      "remap3",
      "remap4",
      "remap5",
      "next_pc",
      "vertexid",
      "sampleid",
      "samplepos",
      "in_samplemask",
      "cluster_id",
   };

   static const char *_str_dynamic_indexing[] = {
      "",
      "[a0.x]",
      "[a0.y]",
      "[a0.z]",
      "[a0.w]",
      "[aL]",
      "[InstanceId]",
      "[b0]"
   };

   if (!src->use) {
      PRINT("void");

      if (is_need_comma)
         PRINT(", ");

      return;
   }

   if (src->rgroup == 0x7) {
      uint32_t hw_imm_raw;

      switch (src->imm_type) {
      case 0x0:
         PRINT("%f", uif(src->imm_val << 12));
         break;

      case 0x1:
         PRINT("%d", ((int) src->imm_val << 12) >> 12);
         break;

      case 0x2:
         PRINT("%u", src->imm_val);
         break;

      case 0x3:

         hw_imm_raw = (uint32_t)get_bits((uint64_t)src->imm_val, 15, 0) |
                      (uint32_t)(get_bits((uint64_t)src->imm_val, 15, 0) << 16);

         PRINT("0x%X(0x%x)", src->imm_val, hw_imm_raw);
         break;

      default:
         assert(false);
      }

      if (is_need_comma)
         PRINT(", ");

      return;
   }

   if (src->neg)
      PRINT("-");

   if (src->abs)
      PRINT("|");

   if ((src->reg == 0x1 ||
        src->reg == 0x2) &&
        src->rgroup == 0x5)
      PRINT("%s", _str_inst_index[src->reg]);
   else if ((src->reg >= 0x10 &&
             src->reg <= 0x13 ) &&
             src->rgroup == 0x5) {
      PRINT("%s", _str_inst_index[src->reg]);
      print_swiz(src->swiz, print_length);
   } else {

      if (index == -1 && (kind_of_inst == SAMPLE_INST || kind_of_inst == SAMPLE_EXT_INST))
         PRINT("s%u", src->reg);
      else
         PRINT("%s%u", _str_src_type[src->rgroup], src->reg);

      PRINT("%s", _str_dynamic_indexing[src->amode]);

      print_swiz(src->swiz, print_length);
   }

   if (src->abs)
      PRINT("|");

   if (is_need_comma)
      PRINT(", ");

}

static void
print_inst_srcs(struct mwv207_inst *operands, uint32_t *print_length)
{
   for (int i = 0; i < 0x3; i++) {
      print_align(print_length, 0xC);

      if (i == 0 && (operands->kind_of_inst == SAMPLE_INST || operands->kind_of_inst == SAMPLE_EXT_INST)) {

         print_each_src(&operands->sampler_aux_src, (i < 0x3 - 1), -1, operands->kind_of_inst, print_length);
         print_align(print_length, 0xC);
      }

      print_each_src(&operands->src[i], (i < 0x3 - 1), i, operands->kind_of_inst, print_length);
   }
}

static void
print_inst(struct mwv207_inst *operands, struct instr *inst, uint32_t *print_length)
{
   enum dst_addr_reg_type addr_reg_type = DST_ADDR_REG_TYPE_NONE;

   if (operands->flags & (1U<<3))
      addr_reg_type = DST_ADDR_REG_TYPE_A0;
   else if (operands->flags & (1U<<4))
      addr_reg_type = DST_ADDR_REG_TYPE_B0;

   print_inst_crontrol(&operands->inst_ctrl, operands, print_length);
   print_inst_dst(&operands->dst, addr_reg_type, operands->src_count != 0, print_length);
   print_inst_srcs(operands, print_length);
}

static void
decode_imm(uint32_t index, struct instr *inst, struct mwv207_inst_src *src)
{
   if (index == 0) {
      src->imm_val = inst->src0_reg_no;
      src->imm_val |= (inst->src0_swizzle << 9);
      src->imm_val |= (inst->is_src0_mod_neg << 17);
      src->imm_val |= (inst->is_src0_mod_abs << 18);
      src->imm_val |= ((inst->src0_rel_addr & 0x01) << 19);
      src->imm_type = (inst->src0_rel_addr >> 1);
   } else if (index == 1) {
      src->imm_val = inst->src1_reg_no;
      src->imm_val |= (inst->src1_swizzle << 9);
      src->imm_val |= (inst->is_src1_mod_neg << 17);
      src->imm_val |= (inst->is_src1_mod_abs << 18);
      src->imm_val |= ((inst->src1_rel_addr & 0x01) << 19);
      src->imm_type = (inst->src1_rel_addr >> 1);
   } else {
      src->imm_val = inst->src2_reg_no;
      src->imm_val |= (inst->src2_swizzle << 9);
      src->imm_val |= (inst->is_src2_mod_neg << 17);
      src->imm_val |= (inst->is_src2_mod_abs << 18);
      src->imm_val |= ((inst->src2_rel_addr & 0x01) << 19);
      src->imm_type = (inst->src2_rel_addr >> 1);
   }
}

static void
decode_each_src(uint32_t index, struct mwv207_inst_src *src, struct instr *inst)
{
   if (index == 0) {
      src->use = inst->is_src0_valid;
      src->rgroup = inst->src0_type;

      if (src->rgroup != 0x7) {
         src->reg = inst->src0_reg_no;
         src->amode = inst->src0_rel_addr;
         src->swiz = inst->src0_swizzle;
         src->abs = inst->is_src0_mod_abs;
         src->neg = inst->is_src0_mod_neg;
      }
   } else if (index == 1) {
      src->use = inst->is_src1_valid;
      src->rgroup = inst->src1_type;

      if (src->rgroup != 0x7) {
         src->reg = inst->src1_reg_no;
         src->amode = inst->src1_rel_addr;
         src->swiz = inst->src1_swizzle;
         src->abs = inst->is_src1_mod_abs;
         src->neg = inst->is_src1_mod_neg;
      }
   } else {
      src->use = inst->is_src2_valid;
      src->rgroup = inst->src2_type;

      if (src->rgroup != 0x7) {
         src->reg = inst->src2_reg_no;
         src->amode = inst->src2_rel_addr;
         src->swiz = inst->src2_swizzle;
         src->abs = inst->is_src2_mod_abs;
         src->neg = inst->is_src2_mod_neg;
      }
   }

   if (src->rgroup == 0x7)
      decode_imm(index, inst, src);
   else if (src->rgroup == 0x2)
      src->is_const_reg = true;
}

static void
decode_check_each_src(uint32_t index,
                      struct mwv207_inst *operands,
                      struct instr *inst,
                      uint32_t expected_src_idx_mask)
{
   switch (index) {
   case 0:
      if (!(inst->is_src0_valid && (expected_src_idx_mask & 0x1)))
         return;

      break;

   case 1:
      if (!(inst->is_src1_valid && (expected_src_idx_mask & 0x2)))
         return;

      break;

   case 2:
      if (!(inst->is_src2_valid && (expected_src_idx_mask & 0x4)))
         return;

      break;

   default:
      assert(false);
   }

   decode_each_src(index, &operands->src[index], inst);
   operands->src_count++;
}

static void
decode_srcs(struct mwv207_inst *operands, struct instr *inst, uint32_t expected_src_idx_mask)
{
   assert(operands->src_count <= 0x3);

   for (int i = 0; i < 0x3; i++)
      decode_check_each_src(i, operands, inst, expected_src_idx_mask);
}

static uint32_t
decode_inst_type(enum kinds_of_inst kind_of_inst, struct instr *inst)
{
   uint32_t inst_type;

   /* FIX(jm9100-oss): 原公式 inst_type = bit0 | (bit1_2 << 1) 与汇编器
    * mwv207_assemble() (nir/mwv207_common.c:264-326) 的编码相反:
    * assemble 把 type_bit2=(type>>2)&1 写进 dword1[21] (即 inst_type_bit0
    * 所在位), 把 type_bit01=type&3 写进 dword2[31:30] (即 inst_type_bit1_2)。
    * 位序交换导致 type!=0 时 _str_inst_type[] 打印全错
    * (例如 .f16 被打成 .s32)。按汇编器语义重组: type = bit0<<2 | bit1_2。 */
   inst_type = (inst->inst_type_bit0 << 2) | inst->inst_type_bit1_2;

   if (kind_of_inst == LOAD_INST ||
       kind_of_inst == STORE_INST)
      inst_type |= (inst->inst_type_bit3_or_img_vec_size_bit0 << 3);

   return inst_type;
}

static void
decode_inst_ctrl_alu_inst(struct mwv207_inst *operands, struct instr *inst)
{
   operands->inst_ctrl.is_result_sat = inst->is_result_sat;

   operands->inst_ctrl.rounding_mode = inst->round_mode;

   if (operands->opcode != 0x72) {
      operands->inst_ctrl.pack_mode_or_vec_size_bit0 = inst->pack_mode_or_vec_size_bit0;

      if (operands->kind_of_inst != CMPLX_INST && !(operands->flags & (1U<<0)))
         operands->inst_ctrl.vec_size_bit1 = inst->vec_size_bit1;
   }

   if (operands->flags & (1U<<0)) {
      operands->inst_ctrl.is_skip_for_helper_kickoff = inst->is_skip_for_helper_kickoff;
      operands->inst_ctrl.u.memory_access_ctrl.is_access_local_storage = inst->is_access_local_storage;
   }

   if (operands->opcode == 0x78) {
      operands->inst_ctrl.u.load_store_attr_ctrl.sh_stage_client = inst->sh_stage_client;
      operands->inst_ctrl.u.load_store_attr_ctrl.attr_layout = inst->is_end_of_BB_cc_alu_or_attr_layout;
   }

   if (operands->flags & (1U<<1))
      operands->inst_ctrl.u.is_inf_x0_to_zero = inst->is_inf_x0_to_zero_or_mult_inf;

}

static void
decode_inst_ctrl_load_store_inst(struct mwv207_inst *operands, struct instr *inst, bool is_for_img, bool is_img_atom)
{
   bool is_pack_load_store;
   uint32_t base_opcode = operands->opcode;
   bool is_full_pack_mode_support = 0;

   operands->inst_ctrl.is_skip_for_helper_kickoff = inst->is_skip_for_helper_kickoff;
   operands->inst_ctrl.u.memory_access_ctrl.is_access_local_storage = inst->is_access_local_storage;
   operands->inst_ctrl.rounding_mode = inst->round_mode;

   is_pack_load_store = (base_opcode == 0x39 || base_opcode == 0x3A);

   if (!is_for_img && !is_pack_load_store)
      operands->inst_ctrl.is_denorm = inst->is_denorm_or_vec_size_bit1_for_ls_inst;

   if (is_pack_load_store) {
      operands->inst_ctrl.pack_mode_or_vec_size_bit0 = inst->pack_mode_or_vec_size_bit0;
      operands->inst_ctrl.vec_size_bit1 = inst->is_denorm_or_vec_size_bit1_for_ls_inst;
   } else if (is_for_img) {
      if (is_full_pack_mode_support && !is_img_atom)
         operands->inst_ctrl.pack_mode_or_vec_size_bit0 = inst->inst_type_bit3_or_img_vec_size_bit0;
   } else
      operands->inst_ctrl.pack_mode_or_vec_size_bit0 = inst->pack_mode_or_vec_size_bit0;

   if (!is_for_img) {
      operands->inst_ctrl.u.memory_access_ctrl.u.load_store_ctrl.is_offset_x3 = inst->is_offset_x3;
      operands->inst_ctrl.u.memory_access_ctrl.u.load_store_ctrl.offset_left_shift = inst->offset_left_shift;
   } else
      operands->inst_ctrl.u.memory_access_ctrl.is_under_evis_mode = 0;

   if (is_img_atom) {
      operands->inst_ctrl.u.memory_access_ctrl.u.img_atom_ctrl.atomic_mode = inst->atomic_mode;
      operands->inst_ctrl.u.memory_access_ctrl.u.img_atom_ctrl.is_3d_img_mode = inst->is_3d_img_mode;
   }

   if (operands->dst.use || is_for_img)
      operands->inst_ctrl.is_result_sat = inst->is_result_sat;

}

static void
decode_inst_ctrl(struct mwv207_inst *operands, struct instr *inst)
{
   switch (operands->kind_of_inst) {
   case TRI_SRCS_CC_ALU_INST:
   case BIN_SRCS_SRC0_SRC1_ALU_CC_INST:
      operands->inst_ctrl.cond_opcode = inst->cond_opcode;
      FALLTHROUGH;
   case TRI_SRCS_ALU_INST:
   case BIN_SRCS_SRC0_SRC1_ALU_INST:
   case BIN_SRCS_SRC0_SRC2_ALU_INST:
   case BIN_SRCS_SRC1_SRC2_ALU_INST:
   case UNA_SRC_SRC0_ALU_INST:
   case UNA_SRC_SRC1_ALU_INST:
   case UNA_SRC_SRC2_ALU_INST:
      decode_inst_ctrl_alu_inst(operands, inst);
      break;

   case PACK_INST:
      operands->inst_ctrl.u.src_select = inst->src_select_or_sampler_swizzle;
      operands->inst_ctrl.is_result_sat = inst->is_result_sat;
      break;

   case SAMPLE_INST:
   case SAMPLE_EXT_INST:
      operands->inst_ctrl.is_result_sat = inst->is_result_sat;
      break;

   case LOAD_INST:
   case IMG_LOAD_INST:
   case STORE_INST:
   case IMG_STORE_INST:
   case IMG_ATOM_INST:
      decode_inst_ctrl_load_store_inst(operands, inst, operands->flags & (1U<<2),
         operands->kind_of_inst == IMG_ATOM_INST);
      break;

   case STORE_ATTR_INST:

      if (!operands->dst.use)

         operands->dst.write_mask = inst->write_mask;

      operands->inst_ctrl.u.load_store_attr_ctrl.sh_stage_client = inst->sh_stage_client;
      operands->inst_ctrl.u.load_store_attr_ctrl.attr_layout = inst->is_end_of_BB_cc_alu_or_attr_layout;
      break;

   case SELECT_MAP_INST:
      operands->inst_ctrl.u.select_map_ctrl.range_to_match = inst->range_to_match;
      operands->inst_ctrl.u.select_map_ctrl.is_component_select = inst->is_component_select;
      operands->inst_ctrl.is_result_sat = inst->is_result_sat;
      break;

   case DIRECT_BRANCH0_INST:
      operands->inst_ctrl.u.loop_optype = inst->loop_optype;
      FALLTHROUGH;
   case DIRECT_BRANCH1_INST:
   case INDIRECT_BRANCH_INST:
   case LOOP_INST:
      operands->inst_ctrl.cond_opcode = inst->cond_opcode;
      operands->inst_ctrl.pack_mode_or_vec_size_bit0 = inst->pack_mode_or_vec_size_bit0;
      operands->inst_ctrl.vec_size_bit1 = inst->vec_size_bit1;
      break;

   case EMIT_INST:
      operands->inst_ctrl.u.emit_ctrl.is_need_restart_prim = inst->is_need_restart_prim;
      operands->inst_ctrl.u.emit_ctrl.is_no_jmp_to_end_on_max_vtx_cnt = inst->is_no_jmp_to_end_on_max_vtx_cnt;
      operands->inst_ctrl.is_result_sat = inst->is_result_sat;
      break;

   case CONV_INST:
      operands->inst_ctrl.is_result_sat = inst->is_result_sat;
      operands->inst_ctrl.rounding_mode = inst->round_mode;

      operands->inst_ctrl.u.conv_ctrl.is_dst_pack_or_vec_size_bit0 = inst->conv_inst_is_dst_pack_or_vec_size_bit0;
      operands->inst_ctrl.u.conv_ctrl.is_dst_vec_size_bit1 = inst->conv_inst_is_dst_vec_size_bit1;
      operands->inst_ctrl.u.conv_ctrl.is_src_pack_or_vec_size_bit0 = inst->conv_inst_is_src_pack_or_vec_size_bit0;
      operands->inst_ctrl.is_denorm = inst->is_denorm_or_vec_size_bit1_for_ls_inst;

      break;

   case CMPLX_INST:
   case SCATTER_INST:

      assert(false);

   default:
      break;
   }
}

static uint32_t
map_sample_hw_opcode_to_aux_opcode(uint32_t base_opcode, uint32_t src_idx_1_and_2_mask)
{

   if (base_opcode == 0x6F) {
      if (src_idx_1_and_2_mask & (1 << 2))
         return 0xFFFF0001;
      else if (src_idx_1_and_2_mask & (1 << 1))
         return 0xFFFF0000;
   } else if (base_opcode == 0x18) {
      if (src_idx_1_and_2_mask & (1 << 1) && src_idx_1_and_2_mask & (1 << 2))
         return 0xFFFF0003;
      else if (src_idx_1_and_2_mask & (1 << 1))
         return 0xFFFF0002;
      else if (src_idx_1_and_2_mask & (1 << 2))
         return 0xFFFF0005;
      else if (src_idx_1_and_2_mask == 0)
         return 0xFFFF0004;
   } else if (base_opcode == 0x7B) {

      if (src_idx_1_and_2_mask & (1 << 1))
         return 0xFFFF000C;
      else if (src_idx_1_and_2_mask & (1 << 2))
         return 0xFFFF000B;

   } else if (base_opcode == 0x7D) {
      if (src_idx_1_and_2_mask & (1 << 2))
         return 0xFFFF000A;
      else if (src_idx_1_and_2_mask & (1 << 1))
         return 0xFFFF0009;
   }

   return base_opcode;
}

static uint32_t
map_ld_st_hw_opcode_to_aux_opcode(uint32_t base_opcode, bool is_dst_enabled)
{
   if (is_dst_enabled) {
      if (base_opcode == 0x33)
         return 0xFFFF0020;
      else if (base_opcode == 0x3A)
         return 0xFFFF0024;
      else if (base_opcode == 0x7A)
         return 0xFFFF0021;
      else if (base_opcode == 0x35)
         return 0xFFFF0022;
      else if (base_opcode == 0x42)
         return 0xFFFF0023;
   }

   return base_opcode;
}

static void
decode_direct_branch_target_src(struct mwv207_inst_src *src,
                                struct instr *inst,
                                enum kinds_of_inst kind_of_inst)
{
   src->use = true;
   src->rgroup = 0x7;

   switch (kind_of_inst) {
   case DIRECT_BRANCH0_INST:
   case LOOP_INST:
      src->imm_type = 0x2;
      src->imm_val = inst->branch_target_or_call_target;
      return;

   case DIRECT_BRANCH1_INST:
      src->imm_type = inst->imm_type;
      src->imm_val = inst->src2_branch_target_bit0_8;
      src->imm_val |= (inst->src2_branch_target_bit9_18 << 9);
      src->imm_val |= (inst->src2_branch_target_bit19 << 19);
      return;

   default:
      assert(false);
   }
}

static void
convert_inst(struct mwv207_inst *operands, struct instr *inst)
{
   uint32_t expected_src_idx_mask = operands->src_mask;
   uint32_t src_idx_1_and_2_mask = 0;
   uint32_t branch_target_src_idx;

   switch (operands->kind_of_inst) {
   case NO_OPND_INST:
      return;

   case DIRECT_BRANCH1_INST:
      branch_target_src_idx = _cond_op_2_src_count[inst->cond_opcode];

      for (int i = 0; i < branch_target_src_idx; i++) {
         decode_check_each_src(i, operands, inst, expected_src_idx_mask);
         operands->src_count++;
      }

      break;

   case DIRECT_CALL_INST:
      operands->src[0].use = true;
      operands->src[0].rgroup = 0x7;
      operands->src[0].imm_type = 0x2;
      operands->src[0].imm_val = inst->branch_target_or_call_target;
      operands->src_count = 1;
      break;

   case SCATTER_INST:
   case CMPLX_INST:
      assert(false);

   case SAMPLE_EXT_INST:
   case SAMPLE_INST:
      operands->sampler_aux_src.use = true;
      operands->sampler_aux_src.rgroup = inst->src0_type;
      operands->sampler_aux_src.reg = inst->sampler_slot_for_sample_inst;
      operands->sampler_aux_src.swiz = inst->src_select_or_sampler_swizzle;
      operands->sampler_aux_src.amode = inst->sampler_rel_addr;
      FALLTHROUGH;
   default:
      decode_srcs(operands, inst, expected_src_idx_mask);
      break;
   }

   branch_target_src_idx = 1;

   switch (operands->kind_of_inst) {
   case DIRECT_BRANCH0_INST:
   case DIRECT_BRANCH1_INST:
      branch_target_src_idx = _cond_op_2_src_count[inst->cond_opcode];
      operands->src_count++;
      FALLTHROUGH;
   case LOOP_INST:
      decode_direct_branch_target_src(&operands->src[branch_target_src_idx], inst, operands->kind_of_inst);
      break;

   default:
      break;
   }

   decode_inst_ctrl(operands, inst);

   switch (operands->kind_of_inst) {
   case SAMPLE_INST:
   case SAMPLE_EXT_INST:
      if (operands->src[2].use)
         src_idx_1_and_2_mask |= 0x4;

      operands->opcode = map_sample_hw_opcode_to_aux_opcode(operands->opcode, src_idx_1_and_2_mask);
      FALLTHROUGH;
   case LOAD_INST:
   case IMG_LOAD_INST:
   case STORE_INST:
   case IMG_STORE_INST:
   case IMG_ATOM_INST:
   case STORE_ATTR_INST:
      operands->opcode = map_ld_st_hw_opcode_to_aux_opcode(operands->opcode, operands->dst.use);
      FALLTHROUGH;
   default:
      break;
   }
}

static void
convert_branch(struct mwv207_inst *operands, struct instr *inst)
{
   if (inst->is_src2_valid) {
      if (inst->src2_type == 0x7) {
         operands->kind_of_inst = DIRECT_BRANCH1_INST;
         operands->src_mask = 0x1 | 0x2 | 0x4;
      } else {
         operands->kind_of_inst = INDIRECT_BRANCH_INST;
         operands->src_mask = 0x4;
      }
   }

   convert_inst(operands, inst);
}

static void
convert_call(struct mwv207_inst *operands, struct instr *inst)
{
   if (inst->is_src2_valid)
      operands->kind_of_inst = INDIRECT_CALL_INST;

   convert_inst(operands, inst);
}

static void
convert_clamp0_max(struct mwv207_inst *operands, struct instr *inst)
{
   if (inst->is_src2_valid) {
      operands->src_mask = 0x1 | 0x2 | 0x4;
      operands->kind_of_inst = TRI_SRCS_ALU_INST;
   }

   convert_inst(operands, inst);
}

static const struct opc_info {
   const char *name;
   void (*convert)(struct mwv207_inst *operands, struct instr *inst);
   enum kinds_of_inst kind_of_inst;
   uint32_t flags;
   uint32_t src_mask;
} opcs[(1 << 0x7) + 0x15] = {
#define OPC_NO_OPND(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, NO_OPND_INST, (flags), 0}
#define OPC_3_SRCS_CC_ALU(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, TRI_SRCS_CC_ALU_INST, (flags), (0x1 | 0x2 | 0x4)}
#define OPC_3_SRCS_ALU(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, TRI_SRCS_ALU_INST, (flags), (0x1 | 0x2 | 0x4)}
#define OPC_2_SRCS_SRC0_SRC1_ALU(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, BIN_SRCS_SRC0_SRC1_ALU_INST, (flags), (0x1 | 0x2)}
#define OPC_2_SRCS_SRC0_SRC1_CC_ALU(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, BIN_SRCS_SRC0_SRC1_ALU_CC_INST, (flags), (0x1 | 0x2)}
#define OPC_2_SRCS_SRC0_SRC2_ALU(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, BIN_SRCS_SRC0_SRC2_ALU_INST, (flags), (0x1 | 0x4)}
#define OPC_2_SRCS_SRC1_SRC2_ALU(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, BIN_SRCS_SRC1_SRC2_ALU_INST, (flags), (0x2 | 0x4)}
#define OPC_1_SRC_SRC0_ALU(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, UNA_SRC_SRC0_ALU_INST, (flags), (0x1)}
#define OPC_1_SRC_SRC1_ALU(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, UNA_SRC_SRC1_ALU_INST, (flags), (0x2)}
#define OPC_1_SRC_SRC2_ALU(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, UNA_SRC_SRC2_ALU_INST, (flags), (0x4)}
#define OPC_PACK(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, PACK_INST, (flags), (0x1 | 0x2 | 0x4)}
#define OPC_SAMPLE(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, SAMPLE_INST, (flags), (0x1 | 0x2 | 0x4)}
#define OPC_LOAD(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, LOAD_INST, (flags), (0x1 | 0x2)}
#define OPC_IMG_LOAD(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, IMG_LOAD_INST, (1U<<2) | (flags), (0x1 | 0x2 | 0x4)}
#define OPC_STORE(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, STORE_INST, (flags), (0x1 | 0x2 | 0x4)}
#define OPC_IMG_STORE(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, IMG_STORE_INST, (1U<<2) | (flags), (0x1 | 0x2 | 0x4)}
#define OPC_IMG_ATOM(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, IMG_ATOM_INST, (1U<<2) | (flags), (0x1 | 0x2 | 0x4)}
#define OPC_STORE_ATTR(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, STORE_ATTR_INST, (flags), (0x1 | 0x2 | 0x4)}
#define OPC_SELECT_MAP(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, SELECT_MAP_INST, (flags), (0x1 | 0x2 | 0x4)}
#define OPC_BRANCH(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_branch, DIRECT_BRANCH0_INST, (flags), (0x1 | 0x2)}
#define OPC_CALL(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_call, DIRECT_CALL_INST, (flags), (0x4)}
#define OPC_LOOP(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, LOOP_INST, (flags), (0x2)}
#define OPC_CONV(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, CONV_INST, (flags), (0x2 | 0x4)}
#define OPC_SCATTER(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, SCATTER_INST, (flags), (0x2 | 0x4)}
#define OPC_CMPLX(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_inst, CMPLX_INST, (flags), (0x1 | 0x2 | 0x4)}
#define OPC_CLAMP0_MAX(opc, flags) [INST_OPCODE_##opc] = {#opc, convert_clamp0_max, BIN_SRCS_SRC0_SRC1_ALU_CC_INST, (flags), (0x1 | 0x2)}
#define OPC_EXT_EMIT(opc, flags) [0x7f + INST_EXT_OPCODE_##opc] = {#opc, convert_inst, EMIT_INST, (flags), (0x2 | 0x4)}
#define OPC_EXT_2_SRCS_SRC0_SRC1_ALU(opc, flags) [0x7f + INST_EXT_OPCODE_##opc] = {#opc, convert_inst, BIN_SRCS_SRC0_SRC1_ALU_INST, (flags), (0x1 | 0x2)}
#define OPC_EXT_1_SRC_SRC0_ALU(opc, flags) [0x7f + INST_EXT_OPCODE_##opc] = {#opc, convert_inst, UNA_SRC_SRC0_ALU_INST, (flags), (0x1)}
#define OPC_SAMPLE_EXT(opc, flags) [0x7f + INST_EXT_OPCODE_##opc] = {#opc, convert_inst, SAMPLE_EXT_INST, (flags), (0x1 | 0x2)}
#define OPC_EXT_LOAD(opc, flags) [0x7f + INST_EXT_OPCODE_##opc] = {#opc, convert_inst, LOAD_INST, (flags), (0x1 | 0x2)}

    [0x0] = {"NOP", convert_inst, NO_OPND_INST, (0), 0},
    [0x2A] = {"BARRIER", convert_inst, NO_OPND_INST, (0), 0},
    [0x15] = {"RET", convert_inst, NO_OPND_INST, (0), 0},
    [0x9] = {"MOV", convert_inst, TRI_SRCS_CC_ALU_INST, (0), (1 | 2 | 4)},
    [0x56] = {"MOVAI", convert_inst, TRI_SRCS_CC_ALU_INST, ((1U<<3)), (1 | 2 | 4)},
    [0xA] = {"MOVAR", convert_inst, TRI_SRCS_CC_ALU_INST, ((1U<<3)), (1 | 2 | 4)},
    [0xB] = {"MOVAF", convert_inst, TRI_SRCS_CC_ALU_INST, ((1U<<3)), (1 | 2 | 4)},
    [0xF] = {"SELECT", convert_inst, TRI_SRCS_CC_ALU_INST, ((1U<<9)), (1 | 2 | 4)},
    [0x31] = {"CMP", convert_inst, TRI_SRCS_CC_ALU_INST, ((1U<<9)), (1 | 2 | 4)},
    [0x2] = {"MAD", convert_inst, TRI_SRCS_ALU_INST, ((1U<<1)), (1 | 2 | 4)},
    [0xE] = {"LITP", convert_inst, TRI_SRCS_ALU_INST, (0), (1 | 2 | 4)},
    [0x30] = {"FMA", convert_inst, TRI_SRCS_ALU_INST, ((1U<<1)), (1 | 2 | 4)},
    [0x2B] = {"SWIZZLE", convert_inst, TRI_SRCS_ALU_INST, (0), (1 | 2 | 4)},
    [0x4C] = {"IMADLO0", convert_inst, TRI_SRCS_ALU_INST, (0), (1 | 2 | 4)},

    [0x50] = {"IMADHI0", convert_inst, TRI_SRCS_ALU_INST, (0), (1 | 2 | 4)},

    [0x4E] = {"IMADLOSAT0", convert_inst, TRI_SRCS_ALU_INST, (0), (1 | 2 | 4)},

    [0x52] = {"IMADHISAT0", convert_inst, TRI_SRCS_ALU_INST, (0), (1 | 2 | 4)},

    [0x65] = {"ATOM_ADD", convert_inst, TRI_SRCS_ALU_INST, ((1U<<0)), (1 | 2 | 4)},
    [0x66] = {"ATOM_XCHG", convert_inst, TRI_SRCS_ALU_INST, ((1U<<0)), (1 | 2 | 4)},
    [0x67] = {"ATOM_CMP_XCHG", convert_inst, TRI_SRCS_ALU_INST, ((1U<<0)), (1 | 2 | 4)},
    [0x68] = {"ATOM_MIN", convert_inst, TRI_SRCS_ALU_INST, ((1U<<0)), (1 | 2 | 4)},
    [0x69] = {"ATOM_MAX", convert_inst, TRI_SRCS_ALU_INST, ((1U<<0)), (1 | 2 | 4)},
    [0x6A] = {"ATOM_OR", convert_inst, TRI_SRCS_ALU_INST, ((1U<<0)), (1 | 2 | 4)},
    [0x6B] = {"ATOM_AND", convert_inst, TRI_SRCS_ALU_INST, ((1U<<0)), (1 | 2 | 4)},
    [0x6C] = {"ATOM_XOR", convert_inst, TRI_SRCS_ALU_INST, ((1U<<0)), (1 | 2 | 4)},
    [0x60] = {"BIT_EXTRACT", convert_inst, TRI_SRCS_ALU_INST, (0), (1 | 2 | 4)},
    [0x54] = {"BIT_INSERT1", convert_inst, TRI_SRCS_ALU_INST, (0), (1 | 2 | 4)},
    [0x55] = {"BIT_INSERT2", convert_inst, TRI_SRCS_ALU_INST, (0), (1 | 2 | 4)},
    [0x78] = {"LOAD_ATTR", convert_inst, TRI_SRCS_ALU_INST, (0), (1 | 2 | 4)},
    [0x63] = {"ARCTRIG", convert_inst, TRI_SRCS_ALU_INST, (0), (1 | 2 | 4)},
    [0x36] = {"CLAMP0_MAX", convert_clamp0_max, BIN_SRCS_SRC0_SRC1_ALU_CC_INST, ((1U<<9)), (1 | 2)},
    [0x10] = {"SET", convert_inst, BIN_SRCS_SRC0_SRC1_ALU_CC_INST, (0), (1 | 2)},
    [0x17] = {"TEXKILL", convert_inst, BIN_SRCS_SRC0_SRC1_ALU_CC_INST, (0), (1 | 2)},
    [0x2C] = {"I2I", convert_inst, BIN_SRCS_SRC0_SRC1_ALU_INST, ((1U<<12)), (1 | 2)},
    [0x77] = {"NORM_MUL", convert_inst, BIN_SRCS_SRC0_SRC1_ALU_INST, ((1U<<1)), (1 | 2)},
    [0x3] = {"MUL", convert_inst, BIN_SRCS_SRC0_SRC1_ALU_INST, ((1U<<1)), (1 | 2)},
    [0x29] = {"MULLO", convert_inst, BIN_SRCS_SRC0_SRC1_ALU_INST, ((1U<<1)), (1 | 2)},
    [0x73] = {"DP2", convert_inst, BIN_SRCS_SRC0_SRC1_ALU_INST, ((1U<<1)), (1 | 2)},
    [0x5] = {"DP3", convert_inst, BIN_SRCS_SRC0_SRC1_ALU_INST, ((1U<<1)), (1 | 2)},
    [0x6] = {"DP4", convert_inst, BIN_SRCS_SRC0_SRC1_ALU_INST, ((1U<<1)), (1 | 2)},
    [0x4] = {"DST", convert_inst, BIN_SRCS_SRC0_SRC1_ALU_INST, ((1U<<1)), (1 | 2)},
    [0x3C] = {"IMULLO0", convert_inst, BIN_SRCS_SRC0_SRC1_ALU_INST, (0), (1 | 2)},

    [0x3E] = {"IMULLOSAT0", convert_inst, BIN_SRCS_SRC0_SRC1_ALU_INST, (0), (1 | 2)},

    [0x40] = {"IMULHI0", convert_inst, BIN_SRCS_SRC0_SRC1_ALU_INST, (0), (1 | 2)},

    [0x44] = {"IDIV0", convert_inst, BIN_SRCS_SRC0_SRC1_ALU_INST, (0), (1 | 2)},
    [0x48] = {"IMOD0", convert_inst, BIN_SRCS_SRC0_SRC1_ALU_INST, (0), (1 | 2)},
    [0x7] = {"DSX", convert_inst, BIN_SRCS_SRC0_SRC2_ALU_INST, (0), (1 | 4)},
    [0x8] = {"DSY", convert_inst, BIN_SRCS_SRC0_SRC2_ALU_INST, (0), (1 | 4)},
    [0x5C] = {"OR", convert_inst, BIN_SRCS_SRC0_SRC2_ALU_INST, (0), (1 | 4)},
    [0x5D] = {"AND", convert_inst, BIN_SRCS_SRC0_SRC2_ALU_INST, (0), (1 | 4)},
    [0x5E] = {"XOR", convert_inst, BIN_SRCS_SRC0_SRC2_ALU_INST, (0), (1 | 4)},
    [0x59] = {"LSHIFT", convert_inst, BIN_SRCS_SRC0_SRC2_ALU_INST, (0), (1 | 4)},
    [0x5A] = {"RSHIFT", convert_inst, BIN_SRCS_SRC0_SRC2_ALU_INST, (0), (1 | 4)},
    [0x5B] = {"ROTATE", convert_inst, BIN_SRCS_SRC0_SRC2_ALU_INST, (0), (1 | 4)},
    [0x3B] = {"IADDSAT", convert_inst, BIN_SRCS_SRC0_SRC2_ALU_INST, (0), (1 | 4)},
    [0x1] = {"ADD", convert_inst, BIN_SRCS_SRC0_SRC2_ALU_INST, (0), (1 | 4)},
    [0x28] = {"ADDLO", convert_inst, BIN_SRCS_SRC0_SRC2_ALU_INST, (0), (1 | 4)},
    [0x64] = {"DIV", convert_inst, BIN_SRCS_SRC1_SRC2_ALU_INST, (0), (2 | 4)},
    [0x2D] = {"I2F", convert_inst, UNA_SRC_SRC0_ALU_INST, (0), (1)},
    [0x2E] = {"F2I", convert_inst, UNA_SRC_SRC0_ALU_INST, (0), (1)},
    [0x2F] = {"F2IRND", convert_inst, UNA_SRC_SRC0_ALU_INST, (0), (1)},
    [0x6D] = {"BIT_REV", convert_inst, UNA_SRC_SRC0_ALU_INST, (0), (1)},
    [0x6E] = {"BYTE_REV", convert_inst, UNA_SRC_SRC0_ALU_INST, (0), (1)},
    [0x74] = {"NORM_DP2", convert_inst, UNA_SRC_SRC0_ALU_INST, ((1U<<1)), (1)},
    [0x75] = {"NORM_DP3", convert_inst, UNA_SRC_SRC0_ALU_INST, ((1U<<1)), (1)},
    [0x76] = {"NORM_DP4", convert_inst, UNA_SRC_SRC0_ALU_INST, ((1U<<1)), (1)},
    [0xC] = {"RCP", convert_inst, UNA_SRC_SRC2_ALU_INST, (0), (4)},
    [0xD] = {"RSQ", convert_inst, UNA_SRC_SRC2_ALU_INST, (0), (4)},
    [0x21] = {"SQRT", convert_inst, UNA_SRC_SRC2_ALU_INST, (0), (4)},
    [0x11] = {"EXP", convert_inst, UNA_SRC_SRC2_ALU_INST, (0), (4)},
    [0x12] = {"LOG", convert_inst, UNA_SRC_SRC2_ALU_INST, (0), (4)},
    [0x13] = {"FRC", convert_inst, UNA_SRC_SRC2_ALU_INST, (0), (4)},
    [0x22] = {"SIN", convert_inst, UNA_SRC_SRC2_ALU_INST, (0), (4)},
    [0x23] = {"COS", convert_inst, UNA_SRC_SRC2_ALU_INST, (0), (4)},
    [0x25] = {"FLOOR", convert_inst, UNA_SRC_SRC2_ALU_INST, (0), (4)},
    [0x26] = {"CEIL", convert_inst, UNA_SRC_SRC2_ALU_INST, (0), (4)},
    [0x27] = {"SIGN", convert_inst, UNA_SRC_SRC2_ALU_INST, (0), (4)},
    [0x57] = {"IABS", convert_inst, UNA_SRC_SRC2_ALU_INST, (0), (4)},
    [0x58] = {"LEADZERO", convert_inst, UNA_SRC_SRC2_ALU_INST, (0), (4)},
    [0x5F] = {"NOT", convert_inst, UNA_SRC_SRC2_ALU_INST, (0), (4)},
    [0x61] = {"POPCOUNT", convert_inst, UNA_SRC_SRC2_ALU_INST, (0), (4)},
    [0x71] = {"PACK", convert_inst, PACK_INST, (0), (1 | 2 | 4)},
    [0x19] = {"TEXLDB", convert_inst, SAMPLE_INST, (0), (1 | 2 | 4)},
    [0x1C] = {"TEXLDPCF", convert_inst, SAMPLE_INST, (0), (1 | 2 | 4)},
    [0x1B] = {"TEXLDL", convert_inst, SAMPLE_INST, (0), (1 | 2 | 4)},
    [0x1A] = {"TEXLDD", convert_inst, SAMPLE_INST, (0), (1 | 2 | 4)},
    [0x70] = {"TEXLDGPCF", convert_inst, SAMPLE_INST, (0), (1 | 2 | 4)},
    [0x7C] = {"LODQ_G", convert_inst, SAMPLE_INST, (0), (1 | 2 | 4)},
    [0x6F] = {"TEXLDLPCF", convert_inst, SAMPLE_INST, (0), (1 | 2 | 4)},
    [0x18] = {"TEXLD", convert_inst, SAMPLE_INST, (0), (1 | 2 | 4)},
    [0x7B] = {"TEXLD_U", convert_inst, SAMPLE_INST, (0), (1 | 2 | 4)},
    [0x7D] = {"TEXLD_GATHER", convert_inst, SAMPLE_INST, (0), (1 | 2 | 4)},
    [0x4B] = {"TEXLD_U_F_L", convert_inst, SAMPLE_INST, (0), (1 | 2 | 4)},
    [0x49] = {"TEXLD_U_S_L", convert_inst, SAMPLE_INST, (0), (1 | 2 | 4)},
    [0x4A] = {"TEXLD_U_U_L", convert_inst, SAMPLE_INST, (0), (1 | 2 | 4)},
    [0x32] = {"LOAD", convert_inst, LOAD_INST, ((1U<<5)), (1 | 2)},
    [0x39] = {"LOADP", convert_inst, LOAD_INST, ((1U<<5)), (1 | 2)},
    [0x79] = {"IMG_LOAD", convert_inst, IMG_LOAD_INST, (1U<<2) | ((1U<<7)), (1 | 2 | 4)},
    [0x34] = {"IMG_LOAD_3D", convert_inst, IMG_LOAD_INST, (1U<<2) | ((1U<<7)), (1 | 2 | 4)},
    [0x37] = {"IMG_ADDR", convert_inst, IMG_LOAD_INST, (1U<<2) | ((1U<<11)), (1 | 2 | 4)},
    [0x38] = {"IMG_ADDR_3D", convert_inst, IMG_LOAD_INST, (1U<<2) | ((1U<<11)), (1 | 2 | 4)},
    [0x33] = {"STORE", convert_inst, STORE_INST, ((1U<<6) | (1U<<12)), (1 | 2 | 4)},
    [0x3A] = {"STOREP", convert_inst, STORE_INST, ((1U<<6)), (1 | 2 | 4)},
    [0x7A] = {"IMG_STORE", convert_inst, IMG_STORE_INST, (1U<<2) | ((1U<<8)), (1 | 2 | 4)},
    [0x35] = {"IMG_STORE_3D", convert_inst, IMG_STORE_INST, (1U<<2) | ((1U<<8)), (1 | 2 | 4)},
    [0x46] = {"IMG_ATOM", convert_inst, IMG_ATOM_INST, (1U<<2) | ((1U<<10)), (1 | 2 | 4)},
    [0x42] = {"STORE_ATTR", convert_inst, STORE_ATTR_INST, (0), (1 | 2 | 4)},
    [0x43] = {"SELECT_MAP", convert_inst, SELECT_MAP_INST, (0), (1 | 2 | 4)},
    [0x16] = {"BRANCH", convert_branch, DIRECT_BRANCH0_INST, (0), (1 | 2)},
    [0x24] = {"BRANCH2", convert_branch, DIRECT_BRANCH0_INST, (0), (1 | 2)},
    [0x14] = {"CALL", convert_call, DIRECT_CALL_INST, (0), (4)},
    [0x1F] = {"LOOP", convert_inst, LOOP_INST, (0), (2)},
    [0x1D] = {"REP", convert_inst, LOOP_INST, (0), (2)},
    [0x20] = {"ENDLOOP", convert_inst, LOOP_INST, (0), (2)},
    [0x1E] = {"ENDREP", convert_inst, LOOP_INST, (0), (2)},
    [0x7f + 0x1] = {"EMIT", convert_inst, EMIT_INST, (0), (2 | 4)},
    [0x7f + 0x2] = {"RESTART", convert_inst, BIN_SRCS_SRC0_SRC1_ALU_INST, (0), (1 | 2)},
    [0x7f + 0xF] = {"HALFMIX", convert_inst, BIN_SRCS_SRC0_SRC1_ALU_INST, (0), (1 | 2)},
    [0x7f + 0x10] = {"LDEXP", convert_inst, BIN_SRCS_SRC0_SRC1_ALU_INST, (0), (1 | 2)},
    [0x7f + 0x3] = {"FLUSH", convert_inst, UNA_SRC_SRC0_ALU_INST, (0), (1)},
    [0x7f + 0xB] = {"BIT_FINDLSB", convert_inst, UNA_SRC_SRC0_ALU_INST, (0), (1)},
    [0x7f + 0xC] = {"BIT_FINDMSB", convert_inst, UNA_SRC_SRC0_ALU_INST, (0), (1)},

    [0x7f + 0x4] = {"LODQ", convert_inst, SAMPLE_EXT_INST, (0), (1 | 2)},
    [0x7f + 0xD] = {"TEXLD_FETCH_MS_S", convert_inst, SAMPLE_EXT_INST, (0), (1 | 2)},
    [0x7f + 0xE] = {"TEXLD_FETCH_MS_U", convert_inst, SAMPLE_EXT_INST, (0), (1 | 2)},
    [0x7f + 0x13] = {"MOVBIX", convert_inst, LOAD_INST, ((1U<<4)), (1 | 2)},
    [0x72] = {"CONV", convert_inst, CONV_INST, ((1U<<12)), (2 | 4)},

};

static void
convert_and_print_inst(uint32_t *dwords, int n)
{
   struct instr *inst = (struct instr *)dwords;
   struct mwv207_inst operands = {0};
   uint32_t opc;

   operands.opcode = inst->base_opcode_bit0_5 | (inst->base_opcode_bit6 << 6);
   opc = operands.opcode;
   operands.ext_opcode = 0xFFFFFFFF;

   if (operands.opcode == 0x7F) {
      operands.ext_opcode = inst->ext_opcode;
      opc += operands.ext_opcode;
   }

   if (opc == 0x45)
      fprintf(stderr, "Not support EVIS OPCODE for now\n");

   const char *name = opcs[opc].name;

   printf("%04d: ", n);

   printf("0x%08x 0x%08x 0x%08x 0x%08x  ", dwords[0], dwords[1],
          dwords[2], dwords[3]);

   if (name) {
      operands.dst.use = inst->is_dst_valid;
      operands.dst.amode = inst->dst_rel_addr;
      operands.dst.reg = inst->dst_reg_no_bit0_6;
      operands.dst.write_mask = inst->write_mask;

      operands.dst.reg_bit7 = inst->dst_reg_no_bit7_or_thread_type_bit0;
      operands.dst.reg_bit8 = inst->dst_reg_no_bit8_or_thread_type_bit1;
      uint32_t print_length = 0;

      operands.inst_ctrl.pack_mode_or_vec_size_bit0 = 0x1;
      operands.kind_of_inst = opcs[opc].kind_of_inst;
      operands.inst_ctrl.inst_type = decode_inst_type(operands.kind_of_inst, inst);
      operands.src_mask = opcs[opc].src_mask;
      operands.flags = opcs[opc].flags;

      print_length += strlen(name);
      opcs[opc].convert(&operands, inst);
      printf("%s", name);
      print_inst(&operands, inst, &print_length);
   } else
      printf("unknown (%u)", inst->opc);

   printf("\n");
}

void
mwv207_disasm(uint32_t *dwords, int sizedwords)
{
   uint32_t i;

   /* FIX(jm9100-oss): F.3.2 —— 断言写的是 %2，但下面循环步长是 4，
    * 非 4 对齐的 sizedwords（例如截断的 shader、或只 dump 前若干条时）
    * 会在最后一次迭代越界读 3 个 dword。改成 %4 与循环一致。 */
   assert((sizedwords % 4) == 0);

   for (i = 0; i < sizedwords; i += 4)
      convert_and_print_inst(&dwords[i], i / 4);

}

void
mwv207_dump_shader_nir(const struct mwv207_shader *shader)
{
   static char *swizzle_names[4] = {
      "x",
      "y",
      "z",
      "w"
   };

   printf("%s code:\n", gl_shader_stage_name(shader->stage));

   mwv207_disasm(shader->code, shader->code_size);

   printf("num temps: %u\n", shader->num_temps);
   printf("immediates:\n");

   for (int idx = 0; idx < shader->uniforms.imm_count; ++idx) {
      printf(" [%i].%s = %f (0x%08x) (%d)\n",
             idx / 4,
             swizzle_names[idx % 4],
             (uif(shader->uniforms.imm_data[idx])),
             shader->uniforms.imm_data[idx],
             shader->uniforms.imm_contents[idx]);
   }

   printf("inputs:\n");

   for (int idx = 0; idx < shader->infile.num_reg; ++idx) {
      printf(" [%i] name=%s comps=%i\n", shader->infile.reg[idx].reg,
             (shader->stage == PIPE_SHADER_VERTEX) ?
             gl_vert_attrib_name(shader->infile.reg[idx].slot) :
             gl_varying_slot_name_for_stage(shader->infile.reg[idx].slot, shader->stage),
             shader->infile.reg[idx].num_components);
   }

   printf("outputs:\n");

   for (int idx = 0; idx < shader->outfile.num_reg; ++idx) {
      printf(" [%i] name=%s comps=%i\n", shader->outfile.reg[idx].reg,
             (shader->stage == PIPE_SHADER_FRAGMENT) ?
             gl_frag_result_name(shader->outfile.reg[idx].slot) :
             gl_varying_slot_name_for_stage(shader->outfile.reg[idx].slot, shader->stage),
             shader->outfile.reg[idx].num_components);
   }

   printf("special:\n");

   if (shader->stage != PIPE_SHADER_FRAGMENT) {
      printf("  vs_pos_out_reg=%i\n", shader->vs_pos_out_reg);
      printf("  vs_pointsize_out_reg=%i\n", shader->vs_pointsize_out_reg);
      printf("  vs_load_balancing=0x%08x\n", shader->vs_load_balancing);
   } else {
      /* FIX(jm9100-oss): F.3.3 —— 原来用 "!= 0" 当终止条件，但 0 是合法
       * 寄存器号（r0）。只要 RT0 的颜色输出落在 r0，后面的 RT1..RT7 就都不会
       * 打印。改用编译期记录的个数。 */
      for (int idx = 0; idx < shader->ps_color_out_num; idx++)
         printf("  ps_color_out_reg=%i\n", shader->ps_color_out_reg[idx]);

      printf("  ps_depth_out_reg=%i\n", shader->ps_depth_out_reg);
   }

   printf("key : 0x%x \n", shader->key.global);

}
