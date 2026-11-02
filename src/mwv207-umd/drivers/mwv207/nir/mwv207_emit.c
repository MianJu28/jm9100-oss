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
#include "mwv207_context.h"
#include "mwv207_common.h"
#include "mwv207_compiler.h"
#include "mwv207_emit.h"

void
emit_inst(struct mwv207_compile *c, struct mwv207_inst *inst)
{
   assert(c->inst_ptr < 0x20000);
   c->code[c->inst_ptr++] = *inst;
}

static uint32_t
mwv207_get_store_attri_client(struct mwv207_compile *c)
{
   switch (c->shader->stage) {
   case PIPE_SHADER_VERTEX :
      return 0x0;

   case PIPE_SHADER_TESS_CTRL :
      return 0x1;

   case PIPE_SHADER_TESS_EVAL :
      return 0x2;

   case PIPE_SHADER_GEOMETRY :
      return 0x3;

   default:
      unreachable("error input shader stage");
      return 0;
   }
}

static uint32_t
mwv207_get_load_attri_client(struct mwv207_compile *c)
{
   switch (c->shader->stage) {
   case PIPE_SHADER_TESS_CTRL :
      return 0x0;

   case PIPE_SHADER_TESS_EVAL :
      return 0x1;

   case PIPE_SHADER_GEOMETRY :
      if (c->shader->key.has_tess)
         return 0x2;
      else
         return 0x0;

   default:
      unreachable("error input shader stage");
      return 0;
   }
}

static uint32_t
mwv207_get_LS_attr_client(struct mwv207_compile *c)
{
   uint32_t client = 0x0;

   switch (c->shader->stage) {
   case PIPE_SHADER_VERTEX:
      client = 0x0;
      break;

   case PIPE_SHADER_TESS_CTRL :
      client = 0x1;
      break;

   case PIPE_SHADER_TESS_EVAL :
      client = 0x2;
      break;

   case PIPE_SHADER_GEOMETRY :
      client = 0x3;
      break;

   default:
      unreachable("error input shader stage");
      return 0;
   }

   return client;
}

static uint32_t
gen_coorddata(uint32_t dataType, uint32_t mode)
{
   uint addressingType = 0x1;
   uint magFilter = 0x0;
   uint minFilter = 0x0;

   uint imm_val = (ALIGN_BIT((uint32_t) (dataType), 0x0) & (0x3))
                  | (ALIGN_BIT((uint32_t) (mode), 0xC) & (0x3000))
                  | (ALIGN_BIT((uint32_t) (addressingType), 0x8) & (0x300))
                  | (ALIGN_BIT((uint32_t) (magFilter), 0x4) & (0x10))
                  | (ALIGN_BIT((uint32_t) (minFilter), 0x5) & (0x20));
   return imm_val;
}

static nir_variable_mode
get_deref_var_type(nir_deref_instr *deref)
{
   while (deref->deref_type != nir_deref_type_var)
      deref = nir_src_as_deref(deref->parent);

   return deref->modes;
}

static uint32_t
get_deref_driver_location(nir_deref_instr *deref_instr)
{
   while (deref_instr->deref_type != nir_deref_type_var)
      deref_instr = nir_deref_instr_parent(deref_instr);

   return deref_instr->var->data.driver_location;
}

static bool
is_deref_var_compact(nir_deref_instr *deref)
{
   while (deref->deref_type != nir_deref_type_var)
      deref = nir_src_as_deref(deref->parent);

   return (deref->var->data.compact);
}

static uint32_t
get_deref_index(nir_deref_instr *deref_instr)
{
   uint32_t constant_index = 0;

   while (deref_instr->deref_type != nir_deref_type_var) {
      if (deref_instr->deref_type == nir_deref_type_array) {
         if (nir_src_is_const(deref_instr->arr.index)) {
            constant_index += glsl_count_attribute_slots(deref_instr->type, false)
                              * nir_src_as_uint(deref_instr->arr.index);
         }

         deref_instr = nir_src_as_deref(deref_instr->parent);
      } else if (deref_instr->deref_type == nir_deref_type_struct) {
         uint32_t sidx = deref_instr->strct.index;
         deref_instr = nir_src_as_deref(deref_instr->parent);
         constant_index += glsl_get_struct_location_offset(deref_instr->type, sidx);
      } else
         unreachable("Unsupported deref type");
   }

   return constant_index;
}

static uint32_t
get_deref_index_perVertex(nir_deref_instr *deref_instr)
{
   uint32_t constant_index = 0;

   while (nir_src_as_deref(deref_instr->parent)->deref_type != nir_deref_type_var) {
      if (deref_instr->deref_type == nir_deref_type_array) {
         if (nir_src_is_const(deref_instr->arr.index)) {
            constant_index += glsl_count_attribute_slots(deref_instr->type, false)
                              * nir_src_as_uint(deref_instr->arr.index);
         } else {

         }

         deref_instr = nir_src_as_deref(deref_instr->parent);
      } else if (deref_instr->deref_type == nir_deref_type_struct) {
         uint32_t sidx = deref_instr->strct.index;
         deref_instr = nir_src_as_deref(deref_instr->parent);
         constant_index += glsl_get_struct_location_offset(deref_instr->type, sidx);
      } else
         unreachable("Unsupported deref type");
   }

   return constant_index;
}

static uint32_t
get_alu_inst_type(nir_alu_type dest_type, uint32_t bitsize)
{
   switch (dest_type | bitsize) {
   case nir_type_float32:
      return 0x0;
   case nir_type_float16:
      return 0x4;
   case nir_type_int32:
      return 0x1;
   case nir_type_int16:
      return 0x5;
   case nir_type_int8:
      return 0x2;
   case nir_type_bool32:
   case nir_type_uint32:
      return 0x6;
   case nir_type_bool16:
   case nir_type_uint16:
       return 0x3;
   case nir_type_bool8:
   case nir_type_uint8:
       return 0x7;
   default:
       unreachable("Unsupported alu type");
      break;
   }
}

static bool
same_swiz(unsigned swiz_src, unsigned dst_write_mask)
{

   static const unsigned int swiz[] = {
      0,
      0b00,
      0b01,
      0b0100,
      0b10,
      0b1000,
      0b1001,
      0b100100,
      0b11,
      0b1100,
      0b1101,
      0b110100,
      0b1110,
      0b111000,
      0b111001,
   };

   if (dst_write_mask < 1 || dst_write_mask > 14)
      return false;

   assert(dst_write_mask >= 1 && dst_write_mask <= 14);
   const unsigned swiz_dst = swiz[dst_write_mask];
   const unsigned s4 = (swiz_src & 0xC0) >> 6;
   const unsigned s3 = (swiz_src & 0x30) >> 4;
   const unsigned s2 = (swiz_src & 0x0C) >> 2;
   const unsigned s1 = (swiz_src & 0x03);
   const unsigned d4 = (swiz_dst & 0xC0) >> 6;
   const unsigned d3 = (swiz_dst & 0x30) >> 4;
   const unsigned d2 = (swiz_dst & 0x0C) >> 2;
   const unsigned d1 = (swiz_dst & 0x03);

   if (d4 != 0)
      return s1 == d1 && s2 == d2 && s3 == d3 && s4 == d4;
   else if (d3 != 0)
      return s1 == d1 && s2 == d2 && s3 == d3;
   else if (d2 != 0)
      return s1 == d1 && s2 == d2;
   else if (d1 != 0)
      return s1 == d1;

   return false;
}

static void
mwv207_emit_alu(struct mwv207_compile *c, nir_op op, struct mwv207_inst_dst dst,
                struct mwv207_inst_src src[3], uint32_t src_bitsize, bool saturate)
{
   struct mwv207_op_info ei = mwv207_ops[op];
   uint32_t swiz_scalar = INST_SWIZ_BROADCAST(ffs(dst.write_mask) - 1);

   if (debug_get_bool_option("OPT_MOV", true) &&
       op == nir_op_mov && src[0].rgroup == 0x0 &&
       src[0].reg == dst.reg && src[0].abs == 0 && src[0].neg == 0 &&
       same_swiz(src[0].swiz, dst.write_mask))
      return;

   if (ei.opcode == 0xff)
      printf("\n not support nir opcode %s \n", nir_op_infos[op].name);

   assert(ei.opcode != 0xff);

   struct mwv207_inst inst = {
      .opcode = ei.opcode,
      .type = ei.type,
      .cond = ei.cond,
      .dst = dst,
      .sat = saturate,
      .round = ei.round,
   };

   if (inst.opcode == 0x7F)
      inst.ext_opcode = ei.ext_opcode;

   switch (op) {
   case nir_op_fdiv:
   case nir_op_flog2:
   case nir_op_fsin:
   case nir_op_fcos:
      inst.tex.amode = 1;

   case nir_op_frsq:
   case nir_op_frcp:
   case nir_op_fexp2:
   case nir_op_fsqrt:
   case nir_op_imul:

      if (src[0].rgroup != 0x7)
         src[0].swiz = inst_swiz_compose(src[0].swiz, swiz_scalar);

      if (src[1].rgroup != 0x7)
         src[1].swiz = inst_swiz_compose(src[1].swiz, swiz_scalar);

      break;

   case nir_op_b2f32:
      inst.src[2] = mwv207_immediate_float(1.0f);
      break;

   case nir_op_b2i32:
   case nir_op_b2i16:
   case nir_op_b2i8:
      inst.src[2] = mwv207_immediate_int(1);
      break;

   case nir_op_ineg:
      inst.src[0] = mwv207_immediate_int(0);
      src[0].neg = 1;
      break;

   case nir_op_fround_even:
      inst.src[0] = mwv207_immediate_float(0.5f);
      break;

   case nir_op_extract_u16:

      assert(false);
      break;

   case nir_op_isub_sat:

      assert(src[1].rgroup != 0x7);
      inst.type = get_alu_inst_type(nir_type_int, src_bitsize);
      src[1].neg = 1;
      break;

   case nir_op_usub_sat:

      assert(src[1].rgroup != 0x7);
      inst.type = get_alu_inst_type(nir_type_uint, src_bitsize);
      src[1].neg = 1;
      break;

   case nir_op_u2u32:
   case nir_op_u2u16:
   case nir_op_u2u8:
      inst.type = get_alu_inst_type(nir_type_uint, src_bitsize);
      inst.src[1] = mwv207_immediate_uint(32);
      break;

   case nir_op_i2i32:
      inst.type = get_alu_inst_type(nir_type_int, src_bitsize);
      inst.src[1] = mwv207_immediate_int(32);
      break;

   case nir_op_i2f32:
      inst.type = get_alu_inst_type(nir_type_int, src_bitsize);
      inst.src[1] = mwv207_immediate_float(32.0f);
      break;

   case nir_op_u2f32:
      inst.type = get_alu_inst_type(nir_type_uint, src_bitsize);
      inst.src[1] = mwv207_immediate_float(32.0f);
      break;

   case nir_op_iabs:
   case nir_op_imin:
   case nir_op_imax:
   case nir_op_iadd_sat:
   case nir_op_ieq32:
   case nir_op_ine32:
   case nir_op_ilt32:
   case nir_op_ige32:
      inst.type = get_alu_inst_type(nir_type_int, src_bitsize);
      break;

   case nir_op_iand:
   case nir_op_uadd_sat:
   case nir_op_ult32:
   case nir_op_uge32:
      inst.type = get_alu_inst_type(nir_type_uint, src_bitsize);
      break;

   default:
      break;
   }

   if (inst.opcode == 0x31 && inst.src[2].use == 0)
      inst.src[2] = mwv207_immediate_int(-1);
   for (uint32_t j = 0; j < 3; j++) {
      uint32_t i = ((ei.src >> j * 2) & 3);

      if (i < 3)
         inst.src[j] = src[i];
   }

   emit_inst(c, &inst);
}

static void
mwv207_emit_tex(struct mwv207_compile *c, nir_texop op, uint32_t texid,
                struct mwv207_inst_dst dst, uint32_t dst_swiz, struct mwv207_inst_src coord,
                struct mwv207_inst_src lod_bias, struct mwv207_inst_src compare,
                struct mwv207_inst_src ms_index, struct mwv207_inst_src tex_offset, uint32_t component)
{
   struct mwv207_inst inst = {
      .dst = dst,
      .tex.id = texid,
      .tex.swiz = dst_swiz,
      .src[0] = coord,
   };

   if (lod_bias.use)
      inst.src[1] = lod_bias;

   if (compare.use)
      inst.src[2] = compare;

   if (tex_offset.use) {
      struct mwv207_inst movai_1 = {
            .opcode = 0x56,
            .type = 0x6,
            .dst = DST_REG(0, 0x1),
            .src[2] = tex_offset,
      };
     emit_inst(c, &movai_1);
     inst.tex.amode = 0x1;
   }

   switch (op) {
   case nir_texop_tex: inst.opcode = 0x18; break;
   case nir_texop_txb: inst.opcode = 0x19; break;
   case nir_texop_txl: inst.opcode = 0x6F; break;
   case nir_texop_txf: {
      inst.opcode = 0x49;
      inst.src[2] = mwv207_immediate_int(gen_coorddata(0x0, 0x1));
   } break;
   case nir_texop_tg4: {
      inst.opcode = 0x7D;
      inst.src[1] = mwv207_immediate_uint(component);
   } break;

   case nir_texop_lod: {
      inst.opcode = 0x7F;
      inst.ext_opcode = 0x4;
   } break;
   case nir_texop_txf_ms: {
      inst.opcode = 0x7F;
      inst.ext_opcode = 0xE;
      inst.src[1] = ms_index;
   } break;

   default:
      assert(0);
   }

   emit_inst(c, &inst);
}

static void
mwv207_emit_jump_with_nz(struct mwv207_compile *c, unsigned block, struct mwv207_inst_src condition)
{
   if (!condition.use) {
      emit_inst(c, &(struct mwv207_inst) {.opcode = 0x16, .imm = block });
      return;
   }

   struct mwv207_inst inst = {
      .opcode = 0x16,
      .cond = 0xB,
      .type = 0x6,
      .src[0] = condition,
      .imm = block,
   };

   inst.src[0].swiz = INST_SWIZ_BROADCAST(inst.src[0].swiz & 3);

   emit_inst(c, &inst);
}

static void
mwv207_emit_jump(struct mwv207_compile *c, uint32_t block, struct mwv207_inst_src condition)
{
   if (!condition.use) {
      emit_inst(c, &(struct mwv207_inst) {.opcode = 0x16, .imm = block });
      return;
   }

   struct mwv207_inst inst = {
      .opcode = 0x16,
      .cond = 0xA,
      .type = 0x6,
      .src[0] = condition,
      .imm = block,
   };

   inst.src[0].swiz = INST_SWIZ_BROADCAST(inst.src[0].swiz & 3);

   emit_inst(c, &inst);
}

static void
mwv207_emit_discard(struct mwv207_compile *c, struct mwv207_inst_src condition)
{
   if (!condition.use) {
      emit_inst(c, &(struct mwv207_inst) { .opcode = 0x17 });
      return;
   }

   struct mwv207_inst inst = {
      .opcode = 0x17,
      .cond = 0xB,
      .type = 0x6,
      .src[0] = condition,
   };

   inst.src[0].swiz = INST_SWIZ_BROADCAST(inst.src[0].swiz & 3);

   emit_inst(c, &inst);
}

static void
mwv207_emit_load_patch(struct mwv207_compile *c,
                       nir_intrinsic_instr *intr)
{
   uint32_t dst_swiz;
   struct mwv207_inst_dst dst = mwv207_dest(c, &intr->def, &dst_swiz);
   nir_deref_instr *deref = nir_src_as_deref(intr->src[0]);
   uint32_t load_offset = get_deref_driver_location(deref);
   uint32_t remapIndex = c->shader->stage == PIPE_SHADER_TESS_CTRL ? 0 : 1;

   if (!is_deref_var_compact(deref))
      load_offset += get_deref_index(deref);

   emit_inst(c, &(struct mwv207_inst) {
      .opcode = 0x78,
      .dst = dst,
      .tex = {
         .pack_mode = 0x1,
         .attri_client = 0x1,
         .attri_layout = 0x1,
      },
      .src[0] = SRC_REG(0, SWIZZLE(W,W,W,W)),
      .src[1] = mwv207_immediate_int(remapIndex),
      .src[2] = mwv207_immediate_int(load_offset),
   });
}

static void
mwv207_emit_load_gs(struct mwv207_compile *c,
                    nir_intrinsic_instr *intr)
{
   uint32_t dst_swiz;
   uint32_t src_swiz = SWIZZLE(X, Y, Z, W);
   uint32_t load_reg = 1;
   uint32_t load_offset = nir_intrinsic_base(intr);
   uint32_t attri_layout = 0x0;
   struct mwv207_inst_src src = {0};
   struct mwv207_inst_dst dst = mwv207_dest(c, &intr->def, &dst_swiz);

   if (c->shader->stage == PIPE_SHADER_GEOMETRY &&
       !c->shader->gs_use_primitive_id &&
       !c->shader->gs_use_invocation_id &&
       !c->shader->gs_use_restart) {
      src_swiz = SWIZZLE(Z, W, X, Y);
      load_reg = 0;
   }

   if (nir_src_is_const(intr->src[0])) {
      int load_base = nir_src_as_int(intr->src[0]);

      if (load_base >= 8) {
         load_base -= 8;
         load_reg = 2;
      }

      src =  mwv207_immediate_int(load_base);
   } else {
      if (intr->src[0].ssa->parent_instr->type == nir_instr_type_intrinsic) {
         nir_intrinsic_instr *src_intr = nir_instr_as_intrinsic(intr->src[0].ssa->parent_instr);

         if (src_intr->intrinsic == nir_intrinsic_load_invocation_id)
            src = SRC_REG(0, SWIZZLE(Y, Y, Y, Y));
         else
            assert(0);
      }
   }

   if (nir_src_is_const(intr->src[1]))
      load_offset += nir_src_as_int(intr->src[1]);

   emit_inst(c, &(struct mwv207_inst) {
      .opcode = 0x78,
      .dst = dst,
      .tex = {
         .attri_client = mwv207_get_load_attri_client(c),
         .attri_layout = attri_layout,
         .pack_mode = 0x1,
      },
      .src[0] = SRC_REG(load_reg, src_swiz),
      .src[1] = src,
      .src[2] = mwv207_immediate_int(load_offset),
   });
}

static void
mwv207_emit_load_perVertex(struct mwv207_compile *c,
                           nir_intrinsic_instr *intr)
{
   uint32_t dst_swiz;
   uint32_t src_swiz = SWIZZLE(X, Y, Z, W);
   uint32_t load_reg = 1;
   uint32_t attri_layout = 0x0;
   uint32_t client = mwv207_get_load_attri_client(c);
   struct mwv207_inst_src src = {0};
   struct mwv207_inst_dst dst = mwv207_dest(c, &intr->def, &dst_swiz);

   if (c->shader->stage == PIPE_SHADER_TESS_EVAL)
      attri_layout = 0x1;

   nir_deref_instr *deref = nir_src_as_deref(intr->src[0]);
   uint32_t load_offset = get_deref_driver_location(deref);
   load_offset += get_deref_index_perVertex(deref);

   nir_deref_instr *deref_tmp = nir_src_as_deref(intr->src[0]);

   while (deref_tmp->deref_type != nir_deref_type_var) {
      deref = deref_tmp;
      deref_tmp =  nir_deref_instr_parent(deref);
   }

   if (deref->arr.index.ssa->parent_instr->type == nir_instr_type_intrinsic) {
      nir_intrinsic_instr *src_intr = nir_instr_as_intrinsic(deref->arr.index.ssa->parent_instr);

      if (src_intr->intrinsic == nir_intrinsic_load_invocation_id) {
         src = SRC_REG(0, SWIZZLE(Y, Y, Y, Y));
         uint32_t input_vertices_reg = c->shader->key.tcs_input_vertries_regs + 1;

         if (input_vertices_reg >= 2) {
            uint32_t map_start = 1;
            uint32_t select_map_reg = c->nir->info.patch_outputs_read ? 9 : 5;

            if (get_deref_var_type(deref) == nir_var_shader_out) {
               map_start = 5;
               client = mwv207_get_LS_attr_client(c);
            }

            emit_inst(c, &(struct mwv207_inst) {
               .opcode = 0x43,
               .dst = (struct mwv207_inst_dst) {
                   .use = 1,
                   .reg = select_map_reg,
                   .write_mask = 0x1,
               },
               .tex = (struct mwv207_inst_tex) {
                  .swiz = 0x80,
               },
               .type = 0x3,
               .src[0] = src,
               .src[1] = SRC_REG(map_start, SWIZZLE(X,Y,Z,W)),
               .src[2] = SRC_REG(map_start + 1, SWIZZLE(X,Y,Z,W)),
            });
            emit_inst(c, &(struct mwv207_inst) {
               .opcode = 0x43,
               .dst = (struct mwv207_inst_dst) {
                   .use = 1,
                   .reg = select_map_reg,
                   .write_mask = 0x1,
               },
               .tex = (struct mwv207_inst_tex) {
                  .swiz = 0x81,
               },
               .type = 0x3,
               .src[0] = src,
               .src[1] = SRC_REG(map_start + 2, SWIZZLE(X,Y,Z,W)),
               .src[2] = SRC_REG(map_start + 3, SWIZZLE(X,Y,Z,W)),
            });
            uint32_t dst_swiz;
            emit_inst(c, &(struct mwv207_inst) {
               .opcode = 0x78,
               .dst = mwv207_dest(c, &intr->def, &dst_swiz),
               .tex = {
                  .attri_client = client,
                  .attri_layout = attri_layout,
                  .pack_mode = 0x1,
               },
               .src[0] = SRC_REG(select_map_reg, SWIZZLE(X,X,X,X)),
               .src[1] = mwv207_immediate_int(0),
               .src[2] = mwv207_immediate_int(0),
            });
            return;
         }
      }
   } else if (deref->arr.index.ssa->parent_instr->type == nir_instr_type_alu) {
      uint32_t map_start = 1;
      uint32_t select_map_reg = c->nir->info.patch_outputs_read ? 9 : 5;

      if (get_deref_var_type(deref) == nir_var_shader_out) {
         map_start = 5;
         client = mwv207_get_LS_attr_client(c);
      }

      src = mwv207_src(c, &deref->arr.index);
      emit_inst(c, &(struct mwv207_inst) {
         .opcode = 0x43,
         .dst = (struct mwv207_inst_dst) {
            .use = 1,
            .reg = select_map_reg,
            .write_mask = 0x1,
         },
         .tex = (struct mwv207_inst_tex) {
            .swiz = 0x80,
         },
         .type = 0x3,
         .src[0] = src,
         .src[1] = SRC_REG(map_start, SWIZZLE(X,Y,Z,W)),
         .src[2] = SRC_REG(map_start + 1, SWIZZLE(X,Y,Z,W)),
      });
      emit_inst(c, &(struct mwv207_inst) {
         .opcode = 0x43,
         .dst = (struct mwv207_inst_dst) {
            .use = 1,
            .reg = select_map_reg,
            .write_mask = 0x1,
         },
         .tex = (struct mwv207_inst_tex) {
            .swiz = 0x81,
         },
         .type = 0x3,
         .src[0] = src,
         .src[1] = SRC_REG(map_start + 2, SWIZZLE(X,Y,Z,W)),
         .src[2] = SRC_REG(map_start + 3, SWIZZLE(X,Y,Z,W)),
      });
      uint32_t dst_swiz;
      emit_inst(c, &(struct mwv207_inst) {
         .opcode = 0x78,
         .dst = mwv207_dest(c, &intr->def, &dst_swiz),
         .tex = {
            .attri_client = client,
            .attri_layout = attri_layout,
            .pack_mode = 0x1,
         },
         .src[0] = SRC_REG(select_map_reg, SWIZZLE(X,X,X,X)),
         .src[1] = mwv207_immediate_int(0),
         .src[2] = mwv207_immediate_int(0),
      });
      return;
   } else if (nir_src_is_const(deref->arr.index)) {
      int remapIndex = nir_src_as_int(deref->arr.index);
      load_reg = remapIndex / 8 + 1;
      src = mwv207_immediate_int(remapIndex % 8);
   }

   emit_inst(c, &(struct mwv207_inst) {
      .opcode = 0x78,
      .dst = dst,
      .tex = {
         .attri_client = client,
         .attri_layout = attri_layout,
         .pack_mode = 0x1,
      },
      .src[0] = SRC_REG(load_reg, src_swiz),
      .src[1] = src,
      .src[2] = mwv207_immediate_int(load_offset),
   });
}

static void
mwv207_emit_load_deref(struct mwv207_compile *c,
                       nir_intrinsic_instr *intr)
{
   nir_deref_instr *deref = nir_instr_as_deref((intr->src[0]).ssa->parent_instr);

   while (deref->deref_type != nir_deref_type_var)
      deref = nir_deref_instr_parent(deref);

   if (!deref->var->data.patch)
      emit(load_perVertex, intr);
   else
      emit(load_patch, intr);
}

static void
mwv207_emit_store_gs(struct mwv207_compile *c,
                     nir_variable *var,
                     nir_intrinsic_instr *intr,
                     struct mwv207_inst_src src)
{
   emit_inst(c, &(struct mwv207_inst) {
      .opcode = 0x42,
      .dst = {
         .write_mask = nir_intrinsic_write_mask(intr),
      },
      .tex = {
         .attri_client = mwv207_get_store_attri_client(c),
         .attri_layout = 0x1,
       },
      .src[0] = SRC_REG(0, SWIZZLE(X,Y,Y,Y)),
      .src[1] = mwv207_immediate_int(var->data.driver_location),
      .src[2] = src,
   });
}

static void
mwv207_emit_store_tess(struct mwv207_compile *c,
                       nir_variable *var,
                       nir_intrinsic_instr *intr,
                       struct mwv207_inst_src src)
{
   int attr_index = 0;
   nir_deref_instr *deref = nir_src_as_deref(intr->src[0]);
   uint32_t patch_offset = var->data.driver_location * 4 + var->data.location_frac;

   if (var->data.location == VARYING_SLOT_TESS_LEVEL_OUTER ||
       var->data.location == VARYING_SLOT_TESS_LEVEL_INNER )
      patch_offset += nir_src_as_int(deref->arr.index);
   else
      patch_offset += get_deref_index(deref) * 4;

   if (var->data.patch) {
      static uint32_t swizzle[4] = {
         SWIZZLE(X, X, X, X),
         SWIZZLE(Y, Y, Y, Y),
         SWIZZLE(Z, Z, Z, Z),
         SWIZZLE(W, W, W, W),
      };

      int loop = 0;
      int start = src.swiz & 0x3;
      struct mwv207_inst_dst dst = {0};

      if (src.rgroup != 0x0) {
         dst.use = 1;
         dst.reg = 2;
      }

      while (loop < nir_src_num_components(intr->src[1])) {
         attr_index = patch_offset / 4;
         dst.write_mask = 1 << (patch_offset % 4);

         src.swiz = swizzle[start + loop];

         emit_inst(c, &(struct mwv207_inst) {
            .opcode = 0x42,
            .dst = dst,
            .tex = {
               .attri_client = mwv207_get_store_attri_client(c),
               .attri_layout = 0x1,
             },
            .src[0] = SRC_REG(0, SWIZZLE(W,W,W,W)),
            .src[1] = mwv207_immediate_int(attr_index),
            .src[2] = src,
         });
         patch_offset++;
         loop++;
      }

      c->shader->patch_num = MAX2(c->shader->patch_num, attr_index + 1);
   } else {
      attr_index = patch_offset / 4;
      src.swiz = SWIZZLE(X, Y, Z, W);
      emit_inst(c, &(struct mwv207_inst) {
         .opcode = 0x42,
         .dst = {
            .write_mask = nir_intrinsic_write_mask(intr),
         },
         .tex = {
            .attri_client = mwv207_get_store_attri_client(c),
            .attri_layout = 0x1,
          },
         .src[0] = SRC_REG(0, SWIZZLE(X,X,X,X)),
         .src[1] = mwv207_immediate_int(attr_index),
         .src[2] = src,
      });
      c->shader->output_num = MAX2(c->shader->output_num, attr_index + 1);
   }
}

static void
mwv207_emit_load_shared(struct mwv207_compile *c,
                        nir_intrinsic_instr *intr)
{
   struct mwv207_inst_src local_address_src = {
      .reg = 1,
      .rgroup = 0x0,
      .use = 1,
      .swiz = SWIZZLE(W, W, W, W),
   };
   uint32_t dst_swiz;
   struct mwv207_inst_dst dst = mwv207_dest(c, &intr->def, &dst_swiz);
   emit_inst(c, &(struct mwv207_inst) {
      .opcode = 0x32,
      .type = 0x6,
      .dst = dst,
      .tex = {
         .pack_mode = 0x1,
         .is_local = 1,
      },
      .src[0] = local_address_src,
      .src[1] = mwv207_src(c, &intr->src[0]),
   });
}

static void
mwv207_emit_load_buffer(struct mwv207_compile *c,
                        nir_intrinsic_instr *intr,
                        bool is_local)
{
   uint32_t dst_swiz;
   struct mwv207_inst_dst dst = mwv207_dest(c, &intr->def, &dst_swiz);
   int buffer_index = is_local ? 0 : nir_src_as_int(intr->src[0]);
   uint32_t uniform = is_local ? MWV207_IMMEDIATE_LOCAL_MEMORY_OFFSET
                                : MWV207_IMMEDIATE_SSBO;

   emit_inst(c, &(struct mwv207_inst) {
      .opcode = 0x32,
      .dst = dst,
      .tex = {
         .pack_mode = 0x1,
      },
      .src[0] = mwv207_const(c, &CONST_VAL(uniform, buffer_index), 1),
      .src[1] = is_local ? mwv207_src(c, &intr->src[0]) : mwv207_src(c, &intr->src[1]),
   });
}

static uint32_t
get_atomic_op(nir_intrinsic_instr *intr)
{
   switch (nir_intrinsic_atomic_op(intr)) {
   case nir_atomic_op_iadd:
   case nir_atomic_op_fadd:
      return 0x65;
   case nir_atomic_op_umin:
   case nir_atomic_op_imin:
   case nir_atomic_op_fmin:
      return 0x68;
   case nir_atomic_op_umax:
   case nir_atomic_op_imax:
   case nir_atomic_op_fmax:
      return 0x69;
   case nir_atomic_op_iand:
      return 0x6B;
   case nir_atomic_op_ior:
      return 0x6A;
   case nir_atomic_op_ixor:
      return 0x6C;
   case nir_atomic_op_xchg:
      return 0x66;
   case nir_atomic_op_cmpxchg:
   case nir_atomic_op_fcmpxchg:
      return 0x67;
   default:
      assert(0);
   }
   return 0;
}

static struct mwv207_inst_src
get_image_info(struct mwv207_compile *c,
                    uint32_t ssimage_index)
{
   nir_const_value image_info[4] = {
      CONST_VAL(MWV207_IMMEDIATE_SSIMAGE,        ssimage_index),
      CONST_VAL(MWV207_IMMEDIATE_SSIMAGE_STRIDE, ssimage_index),
      CONST_VAL(MWV207_IMMEDIATE_SSIMAGE_SIZE,   ssimage_index),
      CONST_VAL(MWV207_IMMEDIATE_SSIMAGE_INFO,   ssimage_index),
   };
   return  mwv207_const(c, image_info, 4);
}

static void
mwv207_emit_image_addr(struct mwv207_compile *c,
                             struct mwv207_inst_src src,
                             int ssimage_index,
                             int opcode)
{
   struct mwv207_inst_dst dst  = {
      .use = 1,
      .reg = src.reg,
      .write_mask = 0x1 | 0x2 | 0x4 | 0x8,
   };

   assert(src.swiz == SWIZZLE(X,Y,Z,W));

   emit_inst(c, &(struct mwv207_inst) {
      .opcode = opcode,
      .type = 0x6,
      .dst = dst,
      .src[0] = get_image_info(c, ssimage_index),
      .src[1] = src,
   });
}

static void
mwv207_emit_atom(struct mwv207_compile *c,
                 nir_intrinsic_instr *intr,
                 bool is_local,
                 bool is_image)
{
   uint32_t opcode = get_atomic_op(intr);
   assert(opcode != 0);
   assert(!(is_local && is_image));
   uint32_t dst_swiz;
   uint32_t type = 0x6;
   int32_t index[] = {0, 1, 2, 3};
   if (is_local)
      for (int i = 0; i < ARRAY_SIZE(index); i++)
         index[i] -= 1;

   if (is_image)
      for (int i = 0; i < ARRAY_SIZE(index); i++)
         index[i] += 1;

   struct mwv207_inst_dst dst = mwv207_dest(c, &intr->def, &dst_swiz);
   struct mwv207_inst_src src2 = mwv207_src(c, &intr->src[index[2]]);
   struct mwv207_inst_src src0;

   if (is_image) {
      uint32_t opcode = 0x37;
      nir_deref_instr *deref = nir_instr_as_deref((intr->src[0]).ssa->parent_instr);
      while (deref->deref_type != nir_deref_type_var)
         deref = nir_src_as_deref(deref->parent);
      nir_variable *var = nir_deref_instr_get_variable(deref);
      int ssimage_index = var->data.binding;

      const struct glsl_type *type = deref->type;
      enum glsl_sampler_dim dim = glsl_get_sampler_dim(type);
      bool is_array = glsl_sampler_type_is_array(type);
      if (dim > GLSL_SAMPLER_DIM_2D || is_array) {
         opcode = 0x38;

         struct mwv207_inst_src coord = mwv207_src(c, &intr->src[1]);
         coord.swiz = SWIZZLE(Z, Z, Z, Z);
         emit_inst(c, &(struct mwv207_inst) {
            .opcode = 0x4C,
            .type = 0x1,
            .dst.use = 1,
            .dst.reg = coord.reg,
            .dst.write_mask = 0x4,
            .src[0] = coord,
            .src[1] = mwv207_const(c, &CONST_VAL(MWV207_IMMEDIATE_SSIMAGE_LAYER_SIZE, ssimage_index), 1),
            .src[2] = mwv207_const(c, &CONST_VAL(MWV207_IMMEDIATE_SSIMAGE, ssimage_index), 1),
         });
      }
      src0 = mwv207_src(c, &intr->src[1]);
      emit(image_addr, src0, ssimage_index, opcode);
      src0.swiz = INST_SWIZ_BROADCAST(src0.swiz & 0x3);
   }else {
      uint32_t ssbo_index = is_local ? 0 : nir_src_as_int(intr->src[0]);
      uint32_t uniform = is_local ? MWV207_IMMEDIATE_LOCAL_MEMORY_OFFSET
                                  : MWV207_IMMEDIATE_SSBO;
      src0 = mwv207_const(c, &CONST_VAL(uniform, ssbo_index), 1);
   }

   switch (nir_intrinsic_atomic_op(intr)) {
   case nir_atomic_op_imin:
   case nir_atomic_op_imax:
      type = 0x1;
      break;

   case nir_atomic_op_cmpxchg: {
      struct mwv207_inst_src mov_src = mwv207_src(c, &intr->src[index[2]]);

      mov_src.swiz = SWIZZLE(X, X, X, X);
      emit_inst(c, &(struct mwv207_inst) {
         .opcode = 0x9,
         .type = 0x6,
         .dst.use = 1,
         .dst.reg = mwv207_src(c, &intr->src[index[3]]).reg,
         .dst.write_mask = 0x2,
         .src[2] = mov_src,
      });
      src2 = mwv207_src(c, &intr->src[index[3]]);
   } break;

   default:
      break;
   }

   if (dst.use == 0) {
      dst.use = 1;
      dst.write_mask = 0x1;
   }

   emit_inst(c, &(struct mwv207_inst) {
      .opcode = opcode,
      .type = type,
      .dst = dst,
      .tex = {
         .pack_mode = 0x1,
      },
      .src[0] = src0,
      .src[1] = mwv207_src(c, &intr->src[index[1]]),
      .src[2] = src2,
   });
}

static void
mwv207_emit_store_shared(struct mwv207_compile *c,
                         nir_intrinsic_instr *intr)
{
   struct mwv207_inst_src local_address_src = {
      .reg = 1,
      .rgroup = 0x0,
      .use = 1,
      .swiz = SWIZZLE(W, W, W, W),
   };
   struct mwv207_inst_src data = mwv207_src(c, &intr->src[0]);
   int dst_use = 0;

   if (data.rgroup != 0x0)
      dst_use = 1;

   emit_inst(c, &(struct mwv207_inst) {
      .opcode = 0x33,
      .type = 0x6,
      .dst = {
         .use = dst_use,
         .write_mask = nir_intrinsic_write_mask(intr),
      },
      .tex = {
         .pack_mode = 0x1,
         .is_local = 1,
      },
      .src[0] = local_address_src,
      .src[1] = mwv207_src(c, &intr->src[1]),
      .src[2] = data,
   });
}

static void
mwv207_emit_store_buffer(struct mwv207_compile *c,
                         nir_intrinsic_instr *intr,
                         bool is_local)
{
   int buffer_index = is_local ? 0 : nir_src_as_int(intr->src[1]);
   struct mwv207_inst_src data = mwv207_src(c, &intr->src[0]);
   uint32_t uniform = is_local ? MWV207_IMMEDIATE_LOCAL_MEMORY_OFFSET
                                : MWV207_IMMEDIATE_SSBO;
   int dst_use = 0;
   if (data.rgroup != 0x0)
      dst_use = 1;

   emit_inst(c, &(struct mwv207_inst) {
      .opcode = 0x33,
      .type = 0x6,
      .dst = {
         .use = dst_use,
         .write_mask = nir_intrinsic_write_mask(intr),
      },
      .tex = {
          .pack_mode = 0x1,
       },
      .src[0] = mwv207_const(c, &CONST_VAL(uniform, buffer_index), 1),
      .src[1] = is_local ? mwv207_src(c, &intr->src[1]) : mwv207_src(c, &intr->src[2]),
      .src[2] = data,
   });
}

static void
mwv207_emit_load_image(struct mwv207_compile *c,
                       nir_intrinsic_instr *intr)
{
   uint32_t dst_swiz;
   struct mwv207_inst_dst dst = mwv207_dest(c, &intr->def, &dst_swiz);
   nir_deref_instr *deref = nir_instr_as_deref((intr->src[0]).ssa->parent_instr);
   const struct glsl_type *type = deref->type;
   enum glsl_sampler_dim dim = glsl_get_sampler_dim(type);
   bool is_array = glsl_sampler_type_is_array(type);

   while (deref->deref_type != nir_deref_type_var)
      deref = nir_src_as_deref(deref->parent);

   nir_variable *var = nir_deref_instr_get_variable(deref);
   int ssimage_index = var->data.binding;

   int opcode = 0x79;

   if (dim > GLSL_SAMPLER_DIM_2D || is_array) {
      opcode = 0x34;

      struct mwv207_inst_src coord = mwv207_src(c, &intr->src[1]);
      coord.swiz = SWIZZLE(Z, Z, Z, Z);
      emit_inst(c, &(struct mwv207_inst) {
         .opcode = 0x4C,
         .type = 0x1,
         .dst.use = 1,
         .dst.reg = coord.reg,
         .dst.write_mask = 0x4,
         .src[0] = coord,
         .src[1] = mwv207_const(c, &CONST_VAL(MWV207_IMMEDIATE_SSIMAGE_LAYER_SIZE, ssimage_index), 1),
         .src[2] = mwv207_const(c, &CONST_VAL(MWV207_IMMEDIATE_SSIMAGE, ssimage_index), 1),
      });
   }

   emit_inst(c, &(struct mwv207_inst) {
      .opcode = opcode,
      .type = 0x0,
      .dst = dst,
      .src[0] = get_image_info(c, ssimage_index),
      .src[1] = mwv207_src(c, &intr->src[1]),
   });
}

static void
mwv207_emit_store_image(struct mwv207_compile *c,
                        nir_intrinsic_instr *intr)
{
   struct mwv207_inst_src data = mwv207_src(c, &intr->src[3]);
   int dst_use = 0;
   nir_deref_instr *deref = nir_instr_as_deref((intr->src[0]).ssa->parent_instr);

   while (deref->deref_type != nir_deref_type_var)
      deref = nir_src_as_deref(deref->parent);

   const struct glsl_type *type = deref->type;
   nir_variable *var = nir_deref_instr_get_variable(deref);
   int ssimage_index = var->data.binding;
   enum glsl_sampler_dim dim = glsl_get_sampler_dim(type);
   bool is_array = glsl_sampler_type_is_array(type);
   int opcode = 0x7A;

   if (dim > GLSL_SAMPLER_DIM_2D || is_array) {
      opcode = 0x35;

      struct mwv207_inst_src coord = mwv207_src(c, &intr->src[1]);
      coord.swiz = SWIZZLE(Z, Z, Z, Z);
      emit_inst(c, &(struct mwv207_inst) {
         .opcode = 0x4C,
         .type = 0x1,
         .dst.use = 1,
         .dst.reg = coord.reg,
         .dst.write_mask = 0x4,
         .src[0] = coord,
         .src[1] = mwv207_const(c, &CONST_VAL(MWV207_IMMEDIATE_SSIMAGE_LAYER_SIZE, ssimage_index), 1),
         .src[2] = mwv207_const(c, &CONST_VAL(MWV207_IMMEDIATE_SSIMAGE, ssimage_index), 1),
      });
   }

   if (data.rgroup != 0x0)
      dst_use = 1;

   emit_inst(c, &(struct mwv207_inst) {
      .opcode = opcode,
      .type = 0x0,
      .dst = {
         .use = dst_use,
         .write_mask = 0xf,
      },
      .src[0] = get_image_info(c, ssimage_index),
      .src[1] = mwv207_src(c, &intr->src[1]),
      .src[2] = data,
   });
}

static void
mwv207_emit_output(struct mwv207_compile *c,
                   nir_intrinsic_instr *intr,
                   struct mwv207_inst_src src)
{
   struct mwv207_shader_io_file *sf = &c->shader->outfile;
   nir_deref_instr *deref = nir_instr_as_deref((intr->src[0]).ssa->parent_instr);
   nir_variable *var;

   while (deref->deref_type != nir_deref_type_var &&
          deref->deref_type != nir_deref_type_array)
      deref = nir_src_as_deref(deref->parent);

   var = nir_deref_instr_get_variable(deref);

   if (c->shader->stage == PIPE_SHADER_GEOMETRY)
      mwv207_emit_store_gs(c, var, intr, src);
   else if (c->shader->stage == PIPE_SHADER_TESS_CTRL)
      mwv207_emit_store_tess(c, var, intr, src);

   if (c->shader->stage == PIPE_SHADER_FRAGMENT) {
      switch (var->data.location) {
      case FRAG_RESULT_COLOR:
      case FRAG_RESULT_DATA0:
         if (var->data.index == 0)
            c->shader->ps_color_out_reg[0] = src.reg;
         break;

      case FRAG_RESULT_DATA1:
      case FRAG_RESULT_DATA2:
      case FRAG_RESULT_DATA3:
      case FRAG_RESULT_DATA4:
      case FRAG_RESULT_DATA5:
      case FRAG_RESULT_DATA6:
      case FRAG_RESULT_DATA7:
         c->shader->ps_color_out_reg[var->data.location - FRAG_RESULT_DATA0] = src.reg;
         break;

      case FRAG_RESULT_DEPTH:
         c->shader->ps_depth_out_reg = src.reg;
         break;

      default:
         unreachable("Unsupported fs output");
      }
   } else if (c->shader->stage == PIPE_SHADER_VERTEX) {
      switch (var->data.location) {
      case VARYING_SLOT_POS:
         c->shader->vs_pos_out_reg = src.reg;
         break;

      case VARYING_SLOT_PSIZ:
         c->shader->vs_pointsize_out_reg = src.reg;
         break;

      case VARYING_SLOT_CLIP_DIST0:
      case VARYING_SLOT_CLIP_DIST1: {
         sf->reg[sf->num_reg].reg = src.reg;
         sf->reg[sf->num_reg].slot = var->data.location;
         sf->reg[sf->num_reg].driver_location = var->data.driver_location;
         sf->reg[sf->num_reg].is_array = true;
         sf->reg[sf->num_reg].arry_index = nir_src_as_int(deref->arr.index);
         sf->reg[sf->num_reg].num_components = glsl_get_components(glsl_without_array(var->type));
         sf->num_reg++;
      } break;

      default:
         sf->reg[sf->num_reg].reg = src.reg;
         sf->reg[sf->num_reg].slot = var->data.location;
         sf->reg[sf->num_reg].driver_location = var->data.driver_location;
         sf->reg[sf->num_reg].is_array = false;
         sf->reg[sf->num_reg].num_components = glsl_get_components(var->type);
         sf->num_reg++;
         break;
      }
   } else {
      if (var->data.patch)
         return;

      for (int i = 0; i < sf->num_reg; i++)
         if (var->data.location == sf->reg[i].slot)
            return;

      switch (var->data.location) {
      case VARYING_SLOT_POS:
         c->shader->vs_pos_out_reg = src.reg;
         break;

      case VARYING_SLOT_PSIZ:
         c->shader->vs_pointsize_out_reg = src.reg;
         break;

      default:
         sf->reg[sf->num_reg].reg = src.reg;
         sf->reg[sf->num_reg].slot = var->data.location;
         sf->reg[sf->num_reg].driver_location = var->data.driver_location;
         sf->reg[sf->num_reg].is_array = false;
         sf->reg[sf->num_reg].num_components = glsl_get_components(var->type);
         sf->num_reg++;
         break;
      }
   }
}

static void
mwv207_emit_block_start(struct mwv207_compile *c, uint32_t block)
{
   assert(block < 0x10000);
   c->block_ptr[block] = c->inst_ptr;
}

static void
emit_alu(struct mwv207_compile *c, nir_alu_instr *alu)
{
   const nir_op_info *info = &nir_op_infos[alu->op];

   if (is_dead_instruction(&alu->instr))
      return;

   assert(!(alu->op >= nir_op_vec2 && alu->op <= nir_op_vec4));

   uint32_t dst_swiz;
   struct mwv207_inst_dst dst = mwv207_dest(c, &alu->def, &dst_swiz);

   switch (alu->op) {
   case nir_op_fdot2:
   case nir_op_fdot3:
   case nir_op_fdot4:

      dst_swiz = INST_SWIZ_IDENTITY;
      break;
   default:
      break;
   }

   struct mwv207_inst_src srcs[3];

   for (int i = 0; i < info->num_inputs; i++) {
      if ((alu->op == nir_op_bitfield_insert) && (i == 3)) {
         break;
      }

      nir_alu_src *asrc = &alu->src[i];
      struct mwv207_inst_src src;

      src = src_swizzle(mwv207_src(c, &asrc->src), ALU_SWIZ(asrc));
      src = src_swizzle(src, dst_swiz);

      if (src.rgroup != 0x7) {
         src.neg = (alu->op == nir_op_fneg);
         src.abs = (alu->op == nir_op_fabs);
      }

      srcs[i] = src;
   }
   uint32_t src_bitsize = nir_src_bit_size(alu->src[0].src);
   emit(alu, alu->op, dst, srcs, src_bitsize, (alu->op == nir_op_fsat));
}

static void
emit_tex(struct mwv207_compile *c, nir_tex_instr *tex)
{
   uint32_t dst_swiz;
   struct mwv207_inst_dst dst = mwv207_dest(c, &tex->def, &dst_swiz);
   nir_src *coord = NULL, *lod_bias = NULL, *compare = NULL, *ms_index = NULL;
   nir_src *tex_offset = NULL, *sampler_offset = NULL;
   for (uint32_t i = 0; i < tex->num_srcs; i++) {
      switch (tex->src[i].src_type) {
      case nir_tex_src_coord:
         coord = &tex->src[i].src;
         break;

      case nir_tex_src_bias:
      case nir_tex_src_lod:
         assert(!lod_bias);
         lod_bias = &tex->src[i].src;
         break;

      case nir_tex_src_comparator:
         compare = &tex->src[i].src;
         break;

      case nir_tex_src_ms_index:
         ms_index = &tex->src[i].src;
         break;

      case nir_tex_src_sampler_deref:
      case nir_tex_src_texture_deref:

         break;

      case nir_tex_src_texture_offset:
         tex_offset = &tex->src[i].src;
         break;

      case nir_tex_src_sampler_offset:
         sampler_offset = &tex->src[i].src;
         break;

      default:
         compile_error(c, "Unhandled NIR tex src type: %d\n",
                       tex->src[i].src_type);
         break;
      }
   }

   if (tex_offset && sampler_offset)
      assert(tex_offset->ssa->index == sampler_offset->ssa->index);

   mwv207_emit_tex(c, tex->op, tex->texture_index,dst, dst_swiz, mwv207_src(c, coord),
                   lod_bias ? mwv207_src(c, lod_bias) : SRC_DISABLE,
                   compare ? mwv207_src(c, compare) : SRC_DISABLE,
                   ms_index ? mwv207_src(c, ms_index) : SRC_DISABLE,
                   tex_offset ? mwv207_src(c, tex_offset) : SRC_DISABLE,
                   tex->component);
}

static void
emit_intrinsic(struct mwv207_compile *c, nir_intrinsic_instr *intr)
{
   switch (intr->intrinsic) {
   case nir_intrinsic_store_deref:
      emit(output, intr, mwv207_src(c, &intr->src[1]));
      break;

   case nir_intrinsic_demote_if:
   case nir_intrinsic_discard_if:
      emit(discard, mwv207_src(c, &intr->src[0]));
      break;

   case nir_intrinsic_demote:
   case nir_intrinsic_discard:
      emit(discard, SRC_DISABLE);
      break;

   case nir_intrinsic_barrier:
      emit_inst(c, &(struct mwv207_inst) { .opcode = 0x2A });
      break;
   case nir_intrinsic_load_uniform: {
      uint32_t dst_swiz;
      struct mwv207_inst_dst dst = mwv207_dest(c, &intr->def, &dst_swiz);

      emit_inst(c, &(struct mwv207_inst) {
         .opcode = 0xA,
         .dst.write_mask = 0x1,
         .src[2] = mwv207_src(c, &intr->src[0]),
      });
      emit_inst(c, &(struct mwv207_inst) {
         .opcode = 0x9,
         .dst = dst,
         .src[2] = {
            .use = 1,
            .rgroup = 0x2,
            .reg = nir_intrinsic_base(intr),
            .swiz = dst_swiz,
            .amode = 0x1,
         },
      });
   } break;

   case nir_intrinsic_load_ubo: {

      nir_const_value *const_value = nir_src_as_const_value(intr->src[0]);
      uint32_t idx = 0;
      if (const_value)
         idx = const_value->u32;
      else
         printf("no const ubo idx, just use 0, fix me later\n");
      uint32_t dst_swiz;
      emit_inst(c, &(struct mwv207_inst) {
         .opcode = 0x32,
         .type = 0x6,
         .dst = mwv207_dest(c, &intr->def, &dst_swiz),
         .src[0] = mwv207_src(c, &intr->src[1]),
         .src[1] = mwv207_const(c, &CONST_VAL(MWV207_IMMEDIATE_UBO, idx), 1),
      });
   } break;

   case nir_intrinsic_load_front_face:
   case nir_intrinsic_load_frag_coord:
      break;

   case nir_intrinsic_load_primitive_id:
      if (c->shader->stage == PIPE_SHADER_TESS_EVAL) {
         emit_inst(c, &(struct mwv207_inst) {
            .opcode = 0x78,
            .dst = (struct mwv207_inst_dst) {
                  .use = 1,
                  .reg = 1,
                  .write_mask = 0x4,
            },
            .tex = {
               .attri_client = mwv207_get_load_attri_client(c),
               .attri_layout = 0x1,
               .pack_mode = 0x1,
            },
            .src[0] = SRC_REG(0, SWIZZLE(W,W,W,W)),
            .src[1] = mwv207_immediate_int(1),
            .src[2] = mwv207_immediate_int(1),
         });
      } break;

   case nir_intrinsic_load_vertex_id:
   case nir_intrinsic_load_instance_id:
   case nir_intrinsic_load_invocation_id:
   case nir_intrinsic_load_patch_vertices_in:
   case nir_intrinsic_load_input:
      break;

   case nir_intrinsic_load_per_vertex_input:
      emit(load_gs, intr);
      break;

   case nir_intrinsic_load_deref:
      emit(load_deref, intr);
      break;

   case nir_intrinsic_emit_vertex:
      emit_inst(c, &(struct mwv207_inst) {
         .opcode = 0x7F,
         .ext_opcode = 0x1,
         .type = 0x0,
         .dst = {
            .use = 1,
            .reg = 0,
            .write_mask = 0xf,
         },
         .tex = {
            .restart = c->shader->gs_use_restart,
            .no_jump_end = true,
         },
         .src[0] = SRC_REG(0, INST_SWIZ_IDENTITY),
         .src[2] = mwv207_immediate_uint(1),
      });
      break;

   case nir_intrinsic_end_primitive:
      emit_inst(c, &(struct mwv207_inst) {
         .opcode = 0x7F,
         .ext_opcode = 0x2,
         .type = 0x0,
         .dst = {
            .use = 1,
            .reg = 0,
            .write_mask = 0xf,
         },
         .tex= {
            .restart = true,
         },
         .src[0] = SRC_REG(0, INST_SWIZ_IDENTITY),
         .src[2] = mwv207_immediate_uint(0),
      });
      break;

   case nir_intrinsic_load_tess_coord:
      break;

   case nir_intrinsic_load_num_workgroups:
   case nir_intrinsic_get_ssbo_size:
   case nir_intrinsic_load_workgroup_id:
   case nir_intrinsic_load_workgroup_size:
   case nir_intrinsic_load_local_invocation_id:
   case nir_intrinsic_load_global_invocation_id:
   case nir_intrinsic_load_global_invocation_id_zero_base:
      break;

   case nir_intrinsic_load_ssbo:
      emit(load_buffer, intr, false);
      break;

   case nir_intrinsic_store_ssbo:
      emit(store_buffer, intr, false);
      break;

   case nir_intrinsic_load_global_constant:
   case nir_intrinsic_load_global_amd: {
      nir_intrinsic_instr *load_intr = nir_instr_as_intrinsic(intr->src[0].ssa->parent_instr);
      uint32_t dst_swiz;
      uint32_t bytesize = load_intr->def.bit_size * load_intr->def.num_components / 8;
      uint32_t index = nir_src_as_int(load_intr->src[1]) / bytesize;
      struct mwv207_inst inst = {
         .opcode = 0x32,
         .type = get_alu_inst_type(nir_type_uint, intr->def.bit_size),
         .dst = mwv207_dest(c, &intr->def, &dst_swiz),
         .src[0] = mwv207_const(c, &CONST_VAL(MWV207_IMMEDIATE_GLOBAL_MEMEORY_OFFSET, index), 1),
         .src[1] = mwv207_src(c, &intr->src[1]),
      };

      emit_inst(c, &inst);
   } break;

   case nir_intrinsic_store_global_amd: {
      nir_intrinsic_instr *load_intr = nir_instr_as_intrinsic(intr->src[1].ssa->parent_instr);
      uint32_t num_components = nir_src_num_components(intr->src[0]);
      uint32_t bytesize = load_intr->def.bit_size * load_intr->def.num_components / 8;
      uint32_t index = nir_src_as_int(load_intr->src[1]) / bytesize;
      struct mwv207_inst inst = {
         .opcode = 0x33,
         .type = get_alu_inst_type(nir_type_uint, nir_src_bit_size(intr->src[0])),
         .dst = {
            .write_mask = (1 << num_components) - 1,
         },
         .src[0] = mwv207_const(c, &CONST_VAL(MWV207_IMMEDIATE_GLOBAL_MEMEORY_OFFSET, index), 1),
         .src[1] = mwv207_src(c, &intr->src[2]),
         .src[2] = mwv207_src(c, &intr->src[0]),
      };

      inst.dst.use = inst.src[2].rgroup != 0x0;
      if (inst.src[2].rgroup != 0x7)
         inst.src[2].swiz &= ((1 << (num_components * 2)) - 1);
      emit_inst(c, &inst);
   } break;

   case nir_intrinsic_image_deref_load:
      emit(load_image, intr);
      break;

   case nir_intrinsic_image_deref_store:
      emit(store_image, intr);
      break;
   case nir_intrinsic_ssbo_atomic:
   case nir_intrinsic_ssbo_atomic_swap:
      emit(atom, intr, false, false);
      break;
   case nir_intrinsic_shared_atomic:
   case nir_intrinsic_shared_atomic_swap:
      emit(atom, intr, true, false);
      break;
   case nir_intrinsic_image_deref_atomic:
   case nir_intrinsic_image_deref_atomic_swap:
      emit(atom, intr, false, true);
      break;
   case nir_intrinsic_load_shared: {
      if (c->shader->cs_shared_size)
         emit(load_buffer, intr, true);
      else
         emit(load_shared, intr);
   } break;
   case nir_intrinsic_store_shared: {
      if (c->shader->cs_shared_size)
         emit(store_buffer, intr, true);
      else
         emit(store_shared, intr);
   } break;
   case nir_intrinsic_atomic_counter_read: {
      uint32_t dst_swiz;
      int index = nir_intrinsic_base(intr);
      emit_inst(c, &(struct mwv207_inst) {
         .opcode = 0x65,
         .type = 0x6,
         .dst = mwv207_dest(c, &intr->def, &dst_swiz),
         .src[0] = mwv207_const(c, &CONST_VAL(MWV207_IMMEDIATE_ATOMIC_BO, index), 1),
         .src[1] = mwv207_src(c, &intr->src[0]),
         .src[2] = mwv207_immediate_int(0),
      });
   } break;

   case nir_intrinsic_atomic_counter_inc: {
      uint32_t dst_swiz;
      int index = nir_intrinsic_base(intr);
      struct mwv207_inst_dst dest = mwv207_dest(c, &intr->def, &dst_swiz);

      if (dest.use == 0) {
         dest.use = 1;
         dest.write_mask = 0x1;
      }

      emit_inst(c, &(struct mwv207_inst) {
         .opcode = 0x65,
         .type = 0x6,
         .dst = dest,
         .src[0] = mwv207_const(c, &CONST_VAL(MWV207_IMMEDIATE_ATOMIC_BO, index), 1),
         .src[1] = mwv207_src(c, &intr->src[0]),
         .src[2] = mwv207_immediate_int(1),
      });
   } break;

   case nir_intrinsic_atomic_counter_pre_dec: {
      uint32_t dst_swiz;
      int index = nir_intrinsic_base(intr);
      struct mwv207_inst_dst dest = mwv207_dest(c, &intr->def, &dst_swiz);
      emit_inst(c, &(struct mwv207_inst) {
         .opcode = 0x65,
         .type = 0x6,
         .dst = dest,
         .src[0] = mwv207_const(c, &CONST_VAL(MWV207_IMMEDIATE_ATOMIC_BO, index), 1),
         .src[1] = mwv207_src(c, &intr->src[0]),
         .src[2] = mwv207_immediate_int(-1),
      });

      if (dest.use) {

         emit_inst(c, &(struct mwv207_inst) {
            .opcode = 0x1,
            .type = 0x6,
            .dst = dest,
            .src[0] = mwv207_src_from_dest(c, &intr->def),
            .src[2] = mwv207_immediate_int(-1),
         });
      }
   } break;

   case nir_intrinsic_image_deref_size: {
      uint32_t dst_swiz;
      struct mwv207_inst_dst dst = mwv207_dest(c, &intr->def, &dst_swiz);
      nir_deref_instr *deref = nir_instr_as_deref((intr->src[0]).ssa->parent_instr);

      while (deref->deref_type != nir_deref_type_var)
         deref = nir_src_as_deref(deref->parent);

      nir_variable *var = nir_deref_instr_get_variable(deref);
      int ssimage_index = var->data.binding;

      emit_inst(c, &(struct mwv207_inst) {
         .opcode = 0x9,
         .type = 0x6,
         .dst = dst,
         .src[2] = mwv207_const(c, &CONST_VAL(MWV207_IMMEDIATE_SSIMAGE_SIZE, ssimage_index), 1),
      });
   } break;
   case nir_intrinsic_decl_reg:
   case nir_intrinsic_load_reg:
   case nir_intrinsic_store_reg:
      break;
   default:
      compile_error(c, "Unhandled NIR intrinsic type: %s\n",
                    nir_intrinsic_infos[intr->intrinsic].name);
   }
}

static void
emit_instr(struct mwv207_compile *c, nir_instr *instr)
{
   switch (instr->type) {
   case nir_instr_type_alu:
      emit_alu(c, nir_instr_as_alu(instr));
      break;

   case nir_instr_type_tex:
      emit_tex(c, nir_instr_as_tex(instr));
      break;

   case nir_instr_type_intrinsic:
      emit_intrinsic(c, nir_instr_as_intrinsic(instr));
      break;

   case nir_instr_type_jump:
      assert(nir_instr_is_last(instr));
      break;

   case nir_instr_type_load_const:
   case nir_instr_type_undef:
   case nir_instr_type_deref:
      break;

   default:
      compile_error(c, "Unhandled NIR instruction type: %d\n", instr->type);
      break;
   }
}

static void
emit_block(struct mwv207_compile *c, nir_block *block)
{
   emit(block_start, block->index);

   nir_foreach_instr(instr, block)
      emit_instr(c, instr);

   nir_block *succs = block->successors[0];

   if (nir_block_ends_in_jump(block) || succs->index < block->index)
      emit(jump, succs->index, SRC_DISABLE);
}

static void
emit_cf_list(struct mwv207_compile *c, struct exec_list *list);

static bool
has_only_one_jump_instr(nir_block *block, nir_jump_instr **jump)
{
   if (!nir_cf_node_is_last(&block->cf_node))
      return false;

   if (exec_list_is_empty(&block->instr_list) || !exec_list_is_singular(&block->instr_list))
      return false;

   nir_instr *last_instr = nir_block_last_instr(block);
   assert(last_instr);

   if (last_instr->type != nir_instr_type_jump)
      return false;

   *jump = nir_instr_as_jump(last_instr);
   return true;
}

static bool
is_block_empty(nir_block *block)
{
   return nir_cf_node_is_last(&block->cf_node) &&
          exec_list_is_empty(&block->instr_list);
}

static bool
is_then_bail_else_empty_if(nir_if *nif)
{
   nir_block *then_block = nir_if_first_then_block(nif);
   nir_block *else_block = nir_if_first_else_block(nif);

   if (then_block == NULL || else_block == NULL)
      return false;

   if (nir_if_last_then_block(nif) != then_block ||
       nir_if_last_else_block(nif) != else_block)
      return false;

   if (!is_block_empty(else_block))
      return false;

   nir_jump_instr *jump = NULL;

   if (!has_only_one_jump_instr(then_block, &jump))
      return false;

   assert(jump);

   if (jump->type != nir_jump_break && jump->type != nir_jump_return)
      return false;

   return true;
}

static void
emit_if(struct mwv207_compile *c, nir_if *nif)
{
   if (debug_get_bool_option("OPT_IF_BAIL", true) && is_then_bail_else_empty_if(nif)) {

      emit(jump_with_nz, nir_if_first_then_block(nif)->successors[0]->index,
           mwv207_src(c, &nif->condition));
   } else {
      emit(jump, nir_if_first_else_block(nif)->index, mwv207_src(c, &nif->condition));
      emit_cf_list(c, &nif->then_list);

      if (!nir_block_ends_in_jump(nir_if_last_then_block(nif)) &&
          !nir_cf_list_is_empty_block(&nif->else_list))
         emit(jump, nir_if_last_else_block(nif)->successors[0]->index, SRC_DISABLE);

      emit_cf_list(c, &nif->else_list);
   }
}

static void
emit_cf_list(struct mwv207_compile *c, struct exec_list *list)
{

   foreach_list_typed(nir_cf_node, node, node, list) {
      switch (node->type) {
      case nir_cf_node_block:
         emit_block(c, nir_cf_node_as_block(node));
         break;

      case nir_cf_node_if:
         emit_if(c, nir_cf_node_as_if(node));
         break;

      case nir_cf_node_loop:
         emit_cf_list(c, &nir_cf_node_as_loop(node)->body);
         break;

      default:
         compile_error(c, "Unknown NIR node type\n");
         break;
      }
   }
}

static void
mwv207_fix_start(struct mwv207_compile *c)
{

   if (c->shader->stage == PIPE_SHADER_TESS_CTRL && c->shader->patch_num != 2) {
      emit_inst(c, &(struct mwv207_inst) {
         .opcode = 0x42,
         .dst = {
            .write_mask = 0x8,
         },
         .tex = {
            .attri_client = mwv207_get_store_attri_client(c),
            .attri_layout = 0x1,
          },
         .src[0] = SRC_REG(0, SWIZZLE(W,W,W,W)),
         .src[1] = mwv207_immediate_int(1),
         .src[2] = SRC_REG(0, SWIZZLE(W,W,W,W)),
      });
   } else if (c->shader->stage == PIPE_SHADER_FRAGMENT && c->shader->key.render_arrary) {
      emit_inst(c, &(struct mwv207_inst) {
         .opcode = 0x9,
         .type = 0x6,
         .dst = {
            .use = 1,
            .reg = 0,
            .write_mask = 0x8,
         },
         .src[2] = SRC_REG(1, SWIZZLE(X,X,X,X)),
      });
   }
}

static void
mwv207_fix_end(struct mwv207_compile *c)
{
   if (c->shader->stage == PIPE_SHADER_TESS_CTRL) {
      emit_inst(c, &(struct mwv207_inst) {
         .opcode = 0x42,
         .dst = {

            .write_mask = 0x4,
         },
         .tex = {
            .attri_client = mwv207_get_store_attri_client(c),
            .attri_layout = 0x1,
          },
         .src[0] = SRC_REG(0, SWIZZLE(W,W,W,W)),
         .src[1] = mwv207_immediate_int(1),
         .src[2] = SRC_REG(0, SWIZZLE(Z,Z,Z,Z)),
      });
   }
}

void
mwv207_emit_shader(struct mwv207_context *ctx, struct mwv207_compile *c)
{
   nir_shader *shader =  c->nir;

   c->impl = nir_shader_get_entrypoint(c->nir);

   mwv207_init_reg_state(ctx, c, shader);

   mwv207_fix_start(c);

   emit_cf_list(c, &nir_shader_get_entrypoint(shader)->body);

   mwv207_fix_end(c);

   mwv207_free_reg_state(c);
}
