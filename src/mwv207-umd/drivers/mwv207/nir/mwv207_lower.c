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
#include "tgsi/tgsi_strings.h"
#include "util/half_float.h"

#include "mwv207_cmd.h"
#include "mwv207_context.h"
#include "mwv207_common.h"
#include "mwv207_compiler.h"
#include "mwv207_emit.h"
#include "mwv207_nir_opt.h"
#include "mwv207_lower.h"
#include "util/u_debug.h"

static nir_def *
get_texture_size(nir_builder *b, nir_tex_instr *tex)
{
   b->cursor = nir_before_instr(&tex->instr);

   nir_tex_instr *txs;

   uint32_t num_srcs = 1;

   for (uint32_t i = 0; i < tex->num_srcs; i++) {
      if (tex->src[i].src_type == nir_tex_src_texture_deref ||
          tex->src[i].src_type == nir_tex_src_sampler_deref ||
          tex->src[i].src_type == nir_tex_src_texture_offset ||
          tex->src[i].src_type == nir_tex_src_sampler_offset ||
          tex->src[i].src_type == nir_tex_src_texture_handle ||
          tex->src[i].src_type == nir_tex_src_sampler_handle)
         num_srcs++;
   }

   txs = nir_tex_instr_create(b->shader, num_srcs);
   txs->op = nir_texop_txs;
   txs->sampler_dim = tex->sampler_dim;
   txs->is_array = tex->is_array;
   txs->is_shadow = tex->is_shadow;
   txs->is_new_style_shadow = tex->is_new_style_shadow;
   txs->texture_index = tex->texture_index;
   txs->sampler_index = tex->sampler_index;
   txs->dest_type = nir_type_int;

   uint32_t idx = 0;

   for (uint32_t i = 0; i < tex->num_srcs; i++) {
      if (tex->src[i].src_type == nir_tex_src_texture_deref ||
          tex->src[i].src_type == nir_tex_src_sampler_deref ||
          tex->src[i].src_type == nir_tex_src_texture_offset ||
          tex->src[i].src_type == nir_tex_src_sampler_offset ||
          tex->src[i].src_type == nir_tex_src_texture_handle ||
          tex->src[i].src_type == nir_tex_src_sampler_handle) {
         txs->src[idx].src = tex->src[i].src;
         txs->src[idx].src_type = tex->src[i].src_type;
         idx++;
      }
   }

   txs->src[idx].src = nir_src_for_ssa(nir_imm_int(b, 0));
   txs->src[idx].src_type = nir_tex_src_lod;

   nir_def_init(&txs->instr, &txs->def,
                nir_tex_instr_dest_size(txs), 32);
   nir_builder_instr_insert(b, &txs->instr);

   return nir_i2f32(b, &txs->def);
}

static nir_def *
compute_Lod(nir_builder *b, nir_tex_instr *tex)
{
   int coord_index = nir_tex_instr_src_index(tex, nir_tex_src_coord);

   b->cursor = nir_before_instr(&tex->instr);
   nir_def *Dx = nir_fddx(b, tex->src[coord_index].src.ssa);
   nir_def *Dy = nir_fddy(b, tex->src[coord_index].src.ssa);
   nir_def *txs = get_texture_size(b, tex);
   Dy = nir_fneg(b, Dy);
   nir_def *xMulSize = nir_fmul(b, Dx, txs);
   nir_def *yMulSize = nir_fmul(b, Dy, txs);
   nir_def *absValDx = nir_fdot2(b, xMulSize, xMulSize);
   nir_def *absValDy = nir_fdot2(b, yMulSize, yMulSize);
   nir_def *maxVal = nir_fmax(b, absValDx, absValDy);
   nir_def *maxZero = nir_fmax(b, maxVal, nir_imm_float(b, 0.0));
   nir_def *log2MaxVal = nir_flog2(b, maxZero);
   nir_def *mulTempMaxVal = nir_fmul(b, log2MaxVal, nir_imm_float(b, 0.5));
   nir_def *addedExp = nir_fadd(b, mulTempMaxVal, nir_imm_float(b, 393216.0));

   nir_def *BitWise = nir_iand(b, addedExp, nir_imm_int(b, 0xffff));
   nir_def *BitWise2 = nir_iand(b, BitWise, nir_imm_int(b, 0x8000));
   nir_def *BitWise3 = nir_ior(b, BitWise, nir_imm_int(b, 0xffff0000));
   nir_def *sel = nir_b32csel(b, nir_i2i32(b, BitWise2), BitWise3, BitWise);

   nir_def *sel2 = nir_fmax(b, nir_i2f32(b, sel), nir_imm_float(b, 0.0));
   nir_def *flod = nir_fdiv(b, sel2, nir_imm_float(b, 32.0));
   nir_def *floorlodtmp = nir_fadd(b, flod, nir_imm_float(b, 0.5));
   nir_def *floorlod = nir_ffloor(b, floorlodtmp);

   return floorlod;
}

static void
mwv207_tex_offset(nir_builder *b, nir_tex_instr *tex, struct mwv207_shader *shader)
{
   int offset_index = nir_tex_instr_src_index(tex, nir_tex_src_offset);

   if (offset_index < 0)
      assert(0);

   int coord_index = nir_tex_instr_src_index(tex, nir_tex_src_coord);
   assert(coord_index >= 0);

   int lod_index = nir_tex_instr_src_index(tex, nir_tex_src_lod);

   nir_def *offset = tex->src[offset_index].src.ssa;
   nir_def *coord = tex->src[coord_index].src.ssa;
   b->cursor = nir_before_instr(&tex->instr);
   nir_def *lod = NULL;

   if ((lod_index != -1) && tex->src[lod_index].src.ssa) {
      if (nir_tex_instr_src_type(tex, lod_index) == nir_type_float) {
         lod = nir_fmax(b, tex->src[lod_index].src.ssa, nir_imm_float(b, 0.0));
         lod = nir_fmin(b, lod, nir_imm_float(b, 100.0));
         lod = nir_fadd(b, lod, nir_imm_float(b, 0.5));
      } else
         lod = nir_fadd(b, nir_i2f32(b, tex->src[lod_index].src.ssa), nir_imm_float(b, 0.5));

      lod = nir_ffloor(b, lod);
      nir_src_rewrite(&tex->src[lod_index].src, lod);
   } else {
      lod = nir_imm_float(b, 0);

      if (shader->stage == PIPE_SHADER_FRAGMENT && tex->op != nir_texop_txb) {
         b->cursor = nir_before_instr(&tex->instr);
         lod = compute_Lod(b, tex);
      }
   }

   b->cursor = nir_before_instr(&tex->instr);

   nir_def *offset_coord;

   if (nir_tex_instr_src_type(tex, coord_index) == nir_type_float) {
      if (tex->sampler_dim == GLSL_SAMPLER_DIM_RECT)
         offset_coord = nir_fadd(b, coord, nir_i2f32(b, offset));
      else {
         nir_def *txs = get_texture_size(b, tex);
         nir_def *txsRsh = nir_ishr(b, nir_f2i32(b, txs), nir_f2i32(b, lod));
         nir_def *scale = nir_frcp(b,  nir_i2f32(b, txsRsh));

         offset_coord = nir_fadd(b, coord,
                                 nir_fmul(b,
                                          nir_i2f32(b, offset),
                                          scale));
      }
   } else
      offset_coord = nir_iadd(b, coord, offset);

   if (tex->is_array) {

      if (tex->coord_components == 2) {
         offset_coord = nir_vec2(b, nir_channel(b, offset_coord, 0),
                                 nir_channel(b, coord, 1));
      } else if (tex->coord_components == 3) {
         offset_coord = nir_vec3(b, nir_channel(b, offset_coord, 0),
                                 nir_channel(b, offset_coord, 1),
                                 nir_ffloor(b, nir_fadd(b, nir_channel(b, coord, 2), nir_imm_float(b, 0.5))));
      } else
         unreachable("Invalid number of components");
   }

   nir_src_rewrite(&tex->src[coord_index].src, offset_coord);

   nir_tex_instr_remove_src(tex, offset_index);
}

static bool
is_const_ubo(const nir_instr *instr, const void *_data)
{
   int *uniform_count = (int*)_data;
   if (*uniform_count > 0xF0)
      return false;

   if (instr->type != nir_instr_type_intrinsic)
      return false;

   nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
   if (intr->intrinsic != nir_intrinsic_load_ubo)
      return false;

   if (!nir_src_is_const(intr->src[0]) || !nir_src_is_const(intr->src[1]))
      return false;

   const uint32_t block = nir_src_as_uint(intr->src[0]);
   if (block > 0)
      return false;

   return true;
}

static nir_def *
lower_ubo_to_uniform(nir_builder *b, nir_instr *instr, void *_data)
{
   nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);

   int *uniform_count = (int*)_data;

   if (intr->def.num_components > 1)
      *uniform_count += 4;
   else
      *uniform_count += 1;

   b->cursor = nir_before_instr(instr);

   nir_def *ubo_offset = intr->src[1].ssa;
   nir_def *range_base = nir_imm_int(b, nir_intrinsic_range_base(intr));

   nir_def *uniform_offset =
      nir_ushr_imm(b, nir_isub(b, ubo_offset, range_base), 4);

   nir_def *uniform =
      nir_load_uniform(b, intr->num_components, intr->def.bit_size, uniform_offset,
                       .base = nir_intrinsic_range_base(intr) / 16,
                       .range = nir_intrinsic_range(intr) / 16,
                       .dest_type = nir_type_float32);

   nir_def_rewrite_uses(&intr->def, uniform);

   return uniform;
}

static bool
mwv207_nir_lower_ubo_to_uniform(nir_shader *shader)
{
   int uniform_count = 0;
   return nir_shader_lower_instructions(shader,
                                        is_const_ubo,
                                        lower_ubo_to_uniform,
                                        (void*)&uniform_count);

}

static void
mwv207_lower_sprite_coord(nir_shader *nir, struct mwv207_shader *shader)
{
   int8_t idx = -1;
   nir_foreach_shader_in_variable(var, nir) {
      uint32_t location = var->data.location;
      if (location != VARYING_SLOT_TEX0 &&
         location != VARYING_SLOT_PNTC)
         continue;

      if (location == VARYING_SLOT_TEX0)
         idx = var->data.driver_location;
      if ((location == VARYING_SLOT_PNTC) && (idx != -1))
         var->data.driver_location = (unsigned)idx;
   }
}

static bool
can_remove_dead_varying(nir_variable *var, void *data)
{
   bool *is_sprite_coord = (bool *)data;
   return (*is_sprite_coord && (var->data.location == VARYING_SLOT_TEX0));
}

static void
mwv207_lower_sprite_coord_late(nir_shader *nir, struct mwv207_compile *c)
{
   struct nir_remove_dead_variables_options opts;
   bool is_sprite_coord = c->shader->key.sprite_coord_enable;

   opts.can_remove_var_data = &is_sprite_coord;
   opts.can_remove_var = &can_remove_dead_varying;
   bool progress = nir_remove_dead_variables(nir, nir_var_shader_in, &opts);
   if (progress) {
      nir->num_inputs -= 1;
   }
}

static void
mwv207_invert_point_coord(nir_shader* nir) {
   nir_variable* pntc_var = nir_find_variable_with_location(nir, nir_var_shader_in, VARYING_SLOT_PNTC);
   if (!pntc_var)
      return;
   nir_foreach_function_safe(function, nir) {
      if (!function->impl)
         continue;
      nir_builder builder = nir_builder_create(function->impl);
      nir_foreach_block_safe(block, function->impl) {
         nir_foreach_instr_safe(instr, block) {
            if (instr->type != nir_instr_type_intrinsic)
               continue;
            nir_intrinsic_instr* intr = nir_instr_as_intrinsic(instr);
            if (intr->intrinsic != nir_intrinsic_load_deref)
               continue;
            nir_variable* var = nir_intrinsic_get_var(intr, 0);
            if (!var || var->data.location != VARYING_SLOT_PNTC)
               continue;
            builder.cursor = nir_after_instr(&intr->instr);
            nir_def* def = nir_vec2(&builder, nir_channel(&builder, &intr->def, 0),
                              nir_fsub_imm(&builder, 1.0, nir_channel(&builder, &intr->def, 1)));
            nir_def_rewrite_uses_after(&intr->def, def, def->parent_instr);
         }
      }
   }
}

static void
mwv207_early_lower(nir_shader* nir, struct mwv207_compile* c) {
   struct mwv207_shader* shader = c->shader;
   struct mwv207_shader_key key = shader->key;
   if (shader->stage == MESA_SHADER_FRAGMENT) {
      if (key.sprite_coord_enable) {
         NIR_PASS_V(nir, nir_lower_texcoord_replace, key.sprite_coord_enable,
            false, key.sprite_coord_yinvert);
         mwv207_lower_sprite_coord(nir, shader);
      } else if (key.sprite_coord_yinvert)
         NIR_PASS_V(nir, mwv207_invert_point_coord);
   }
}

static void
mwv207_lower_io(nir_shader *nir, struct mwv207_shader *shader)
{
   nir_foreach_function(function, nir) {
      nir_builder b = nir_builder_create(function->impl);

      nir_foreach_block(block, function->impl) {
         nir_foreach_instr_safe(instr, block) {
            if (instr->type == nir_instr_type_intrinsic) {
               nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);

               switch (intr->intrinsic) {
               case nir_intrinsic_load_front_face: {
                  intr->def.bit_size = 32;

                  b.cursor = nir_after_instr(instr);

                  nir_def *ssa = nir_ine(&b, &intr->def, nir_imm_int(&b, 0));

                  if (shader->key.front_ccw)
                     nir_instr_as_alu(ssa->parent_instr)->op = nir_op_ieq;

                  nir_def_rewrite_uses_after(&intr->def, ssa, ssa->parent_instr);
               } break;

               case nir_intrinsic_load_frag_coord:
                  shader->useFragCoord = true;
                  break;
               case nir_intrinsic_load_vertex_id:
               case nir_intrinsic_load_instance_id:
                  shader->vs_id_in_reg = shader->infile.num_reg;
                  break;

               case nir_intrinsic_load_primitive_id:
                  shader->gs_use_primitive_id = true;
                  break;

               case nir_intrinsic_load_invocation_id:
                  shader->gs_use_invocation_id = true;
                  break;

               case nir_intrinsic_end_primitive:
                  shader->gs_use_restart = true;
                  break;

               default:
                  break;
               }
            }

            if (instr->type != nir_instr_type_tex)
               continue;

            nir_tex_instr *tex = nir_instr_as_tex(instr);
            nir_src *coord = NULL;
            nir_src *lod_bias = NULL;
            uint32_t lod_bias_idx;

            assert(tex->sampler_index == tex->texture_index);

            for (uint32_t i = 0; i < tex->num_srcs; i++) {
               switch (tex->src[i].src_type) {
               case nir_tex_src_coord:
                  coord = &tex->src[i].src;
                  break;

               case nir_tex_src_bias:
                  break;

               case nir_tex_src_lod: {
                  assert(!lod_bias);
                  lod_bias = &tex->src[i].src;
                  lod_bias_idx = i;
                  nir_def *lod = tex->src[i].src.ssa;
                  b.cursor = nir_before_instr(&tex->instr);

                  if (nir_tex_instr_src_type(tex, lod_bias_idx) == nir_type_float) {
                     lod = nir_fmax(&b, lod, nir_imm_float(&b, 0.0));
                     lod = nir_fmin(&b, lod, nir_imm_float(&b, 100.0));
                     nir_src_rewrite(&tex->src[i].src, lod);
                  } else if (nir_tex_instr_src_type(tex, lod_bias_idx) == nir_type_int) {
                     lod = nir_imax(&b, lod, nir_imm_int(&b, 0));
                     lod = nir_imin(&b, lod, nir_imm_int(&b, 100));
                     lod = nir_i2f32(&b, lod);
                     nir_src_rewrite(&tex->src[i].src, lod);
                  }
               } break;

               case nir_tex_src_offset:
                  mwv207_tex_offset(&b, tex, shader);
                  break;

               case nir_tex_src_ms_index:
               case nir_tex_src_comparator:
               case nir_tex_src_sampler_deref:
               case nir_tex_src_texture_deref:
               case nir_tex_src_texture_offset:
               case nir_tex_src_sampler_offset:
                  break;

               default:
                  printf("unsupport io type %d\n", tex->src[i].src_type);
                  assert(0);
                  break;
               }
            }

            if (tex->sampler_dim == GLSL_SAMPLER_DIM_RECT) {

               b.cursor = nir_before_instr(instr);
               nir_intrinsic_instr *load =
                  nir_intrinsic_instr_create(b.shader, nir_intrinsic_load_uniform);
               nir_intrinsic_set_base(load, ~tex->sampler_index);
               load->num_components = 2;
               load->src[0] = nir_src_for_ssa(nir_imm_float(&b, 0.0f));
               nir_def_init(&load->instr, &load->def, 2, 32);
               nir_intrinsic_set_dest_type(load, nir_type_float);

               nir_builder_instr_insert(&b, &load->instr);

               nir_def *new_coord = nir_fmul(&b, coord->ssa, &load->def);
               nir_src_rewrite(coord, new_coord);
            }
         }
      }
   }
}

static bool
mwv207_alu_to_scalar_filter_cb(const nir_instr *instr, const void *data)
{
   if (instr->type != nir_instr_type_alu)
      return false;

   nir_alu_instr *alu = nir_instr_as_alu(instr);

   switch (alu->op) {
   case nir_op_frsq:
   case nir_op_frcp:
   case nir_op_flog2:
   case nir_op_fexp2:
   case nir_op_fsqrt:
   case nir_op_fcos:
   case nir_op_fsin:
   case nir_op_fdiv:
   case nir_op_imul:
   case nir_op_imul_high:
   case nir_op_umul_high:
   case nir_op_fdph:
   case nir_op_fround_even:
      return true;

   case nir_op_b32all_fequal2:
   case nir_op_b32all_fequal3:
   case nir_op_b32all_fequal4:
   case nir_op_b32any_fnequal2:
   case nir_op_b32any_fnequal3:
   case nir_op_b32any_fnequal4:
   case nir_op_b32all_iequal2:
   case nir_op_b32all_iequal3:
   case nir_op_b32all_iequal4:
   case nir_op_b32any_inequal2:
   case nir_op_b32any_inequal3:
   case nir_op_b32any_inequal4:
      return true;

   case nir_op_fdot2:
      break;

   default:
      break;
   }

   return false;
}

static nir_const_value *get_alu_cv(nir_alu_src *src)
 {
   nir_const_value *cv = nir_src_as_const_value(src->src);

   if (!cv &&
       (src->src.ssa->parent_instr->type == nir_instr_type_alu)) {
      nir_alu_instr *parent = nir_instr_as_alu(src->src.ssa->parent_instr);

      if ((parent->op == nir_op_fabs) ||
          (parent->op == nir_op_fneg)) {
         cv = nir_src_as_const_value(parent->src[0].src);

         if (cv) {

            for (unsigned i = 0; i < parent->def.num_components; i++) {
               if (cv[i].u64 >> 32 != MWV207_IMMEDIATE_CONSTANT) {
                  cv = NULL;
                  break;
               }
            }
         }
      }
   }

   return cv;
 }

static void
mwv207_lower_alu_impl(nir_function_impl *impl, struct mwv207_compile *c)
{
   nir_shader *nir = impl->function->shader;

   nir_builder b = nir_builder_create(impl);

   nir_foreach_block(block, impl) {
      nir_foreach_instr_safe(instr, block) {
         if (instr->type != nir_instr_type_alu)
            continue;

         nir_alu_instr *alu = nir_instr_as_alu(instr);

         if (alu->op == nir_op_fsin || alu->op == nir_op_fcos) {
            b.cursor = nir_before_instr(instr);

            nir_def *imm = nir_imm_float(&b, 1.0 / M_PI);

            nir_src_rewrite(&alu->src[0].src, nir_fmul(&b, alu->src[0].src.ssa, imm));
         }

         if (alu->op == nir_op_fdiv || alu->op == nir_op_flog2 ||
             alu->op == nir_op_fsin || alu->op == nir_op_fcos) {
            nir_def *ssa = &alu->def;

            assert(ssa->num_components == 1);

            nir_alu_instr *mul = nir_alu_instr_create(nir, nir_op_fmul);
            mul->src[0].src = mul->src[1].src = nir_src_for_ssa(ssa);
            mul->src[1].swizzle[0] = 1;

            nir_def_init(&mul->instr, &mul->def, 1, 32);

            alu->src[0].swizzle[1] = 0;
            ssa->num_components = 2;

            nir_instr_insert_after(instr, &mul->instr);

            nir_def_rewrite_uses_after(ssa, &mul->def, &mul->instr);
         }

         if (alu->op == nir_op_bitfield_insert) {
            b.cursor = nir_before_instr(instr);

            nir_def *imm = nir_imm_int(&b, 8);

            nir_src_rewrite(&alu->src[2].src,
               nir_ior(&b, alu->src[2].src.ssa, nir_ishl(&b, alu->src[3].src.ssa, imm)));
         }

      }
   }
}

static void
mwv207_lower_alu(nir_shader *nir, struct mwv207_compile *c)
{
   nir_foreach_function(function, nir) {
      if (function->impl)
         mwv207_lower_alu_impl(function->impl, c);
   }
}

static void
mwv207_lower_global_impl(nir_function_impl *impl)
{
   nir_shader *nir = impl->function->shader;
   nir_builder b = nir_builder_create(impl);

   nir_foreach_block(block, impl) {
      nir_foreach_instr_safe(instr, block) {
         if (instr->type != nir_instr_type_intrinsic)
            continue;

         nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
         switch (intr->intrinsic) {
         case nir_intrinsic_load_global:
         case  nir_intrinsic_load_global_constant:
         case nir_intrinsic_store_global:
            break;
         default:
            continue;
         }

         b.cursor = nir_before_instr(instr);
         nir_src *addr_src = nir_get_io_offset_src(intr);
         nir_scalar addr = {addr_src->ssa, 0};
         addr = nir_scalar_chase_movs(addr);
         nir_scalar addr_base, addr_off;
         if (nir_scalar_is_alu(addr) &&
             nir_scalar_alu_op(addr) == nir_op_iadd) {
            addr_base = nir_scalar_chase_alu_src(addr, 0);
            addr_off = nir_scalar_chase_alu_src(addr, 1);
         } else {
            addr_base = addr;
            addr_off = nir_get_scalar((nir_imm_int(&b, 0)), 0);
         }

         nir_def *v, *intr_off;
         nir_alu_instr *alu_intr = nir_alu_instr_create(nir, nir_op_iadd);
         nir_def *base = nir_channel(&b, addr_base.def, addr_base.comp);
         nir_def *off = nir_channel(&b, addr_off.def, addr_off.comp);
         nir_instr_type instr_type = base->parent_instr->type;
         if (instr_type  == nir_instr_type_load_const) {
            base = nir_channel(&b, addr_off.def, addr_off.comp);
            off = nir_channel(&b, addr_base.def, addr_base.comp);
         }
         instr_type = base->parent_instr->type;
         if (instr_type == nir_instr_type_alu) {
            nir_scalar alu_addr = {base, 0};
            nir_scalar alu_base = nir_scalar_chase_alu_src(alu_addr, 0);
            nir_scalar alu_off = nir_scalar_chase_alu_src(alu_addr, 1);
            base = nir_channel(&b, alu_base.def, alu_base.comp);
            intr_off = nir_channel(&b, alu_off.def, alu_off.comp);

            alu_intr->src[0].src = nir_src_for_ssa(intr_off);
            alu_intr->src[0].swizzle[0] = 0;
            alu_intr->src[1].src = nir_src_for_ssa(off);
            alu_intr->src[1].swizzle[0] = 0;
            nir_def_init(&alu_intr->instr, &alu_intr->def, 1, base->bit_size);
            nir_builder_instr_insert(&b, &alu_intr->instr);

            nir_alu_instr *add_intr = nir_alu_instr_create(nir, nir_op_iadd);
            add_intr->src[0].src = nir_src_for_ssa(base);
            add_intr->src[0].swizzle[0] = 0;
            add_intr->src[1].src = nir_src_for_ssa(&alu_intr->def);
            add_intr->src[1].swizzle[0] = 0;
            nir_def_init(&add_intr->instr, &add_intr->def, 1, base->bit_size);
            nir_builder_instr_insert(&b, &add_intr->instr);
            nir_def_rewrite_uses(addr_src->ssa, &add_intr->def);
         }

         if (intr->intrinsic == nir_intrinsic_store_global) {
            assert(nir_intrinsic_src_components(intr, 1) == 1);
            uint32_t num_comp = nir_intrinsic_src_components(intr, 0);
            v = nir_channels(&b, intr->src[0].ssa, BITFIELD_MASK(num_comp));

            nir_intrinsic_instr *new_intr =
               nir_intrinsic_instr_create(nir, nir_intrinsic_store_global_amd);
            new_intr->num_components = intr->num_components;
            new_intr->src[1] = nir_src_for_ssa(base);
            new_intr->src[0] = nir_src_for_ssa(v);
            new_intr->src[2] = nir_src_for_ssa(off);
            if (instr_type == nir_instr_type_alu)
               new_intr->src[2] = nir_src_for_ssa(&alu_intr->def);

            if (nir_intrinsic_infos[nir_intrinsic_store_global].has_dest)
               nir_def_init(&new_intr->instr, &new_intr->def, intr->num_components, v->bit_size);

            nir_builder_instr_insert(&b, &new_intr->instr);
            if (nir_intrinsic_infos[nir_intrinsic_store_global].has_dest)
               nir_def_rewrite_uses(&intr->def, &new_intr->def);

         } else {
            assert(nir_intrinsic_src_components(intr, 0) == 1);
            nir_intrinsic_instr *new_intr =
               nir_intrinsic_instr_create(nir, nir_intrinsic_load_global_amd);
            new_intr->num_components = intr->num_components;
            new_intr->src[0] = nir_src_for_ssa(base);
            new_intr->src[1] = nir_src_for_ssa(off);
            if (instr_type == nir_instr_type_alu)
               new_intr->src[1] = nir_src_for_ssa(&alu_intr->def);
            nir_def_init(&new_intr->instr, &new_intr->def, intr->def.num_components, intr->def.bit_size);
            nir_builder_instr_insert(&b, &new_intr->instr);
            nir_def_rewrite_uses(&intr->def, &new_intr->def);
         }
         nir_instr_remove(instr);
      }
   }
}

static bool
mwv207_lower_global(nir_shader *nir)
{
   nir_foreach_function(function, nir)
      if (function->impl)
         mwv207_lower_global_impl(function->impl);
   return true;
}

static void
split_double_store_global(nir_function_impl *impl)
{
   nir_builder b = nir_builder_create(impl);

   nir_foreach_block(block, impl) {
      nir_foreach_instr_safe(instr, block) {
         if (instr->type != nir_instr_type_intrinsic)
            continue;

         nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
         switch (intr->intrinsic) {
         case nir_intrinsic_store_global:
         if (nir_src_bit_size(intr->src[0]) != 64 ||
            nir_src_num_components(intr->src[0]) < 3)
               continue;
            break;
         default:
            continue;
         }
         b.cursor = nir_before_instr(instr);
         nir_src src = intr->src[0];
         uint32_t old_components = nir_src_num_components(src);
         nir_intrinsic_instr *store1 = nir_intrinsic_instr_create(b.shader, nir_intrinsic_store_global);
         nir_intrinsic_instr *store2 = nir_intrinsic_instr_create(b.shader, nir_intrinsic_store_global);
         nir_def *src1 = nir_trim_vector(&b, src.ssa, 2);
         nir_def *src2 = nir_channels(&b, src.ssa, old_components == 3 ? 4 : 0xc);
         store1->src[0] = nir_src_for_ssa(src1);
         store1->src[1] = intr->src[1];
         nir_intrinsic_set_write_mask(store1, 3);
         nir_intrinsic_set_align_mul(store1, nir_intrinsic_align_mul(intr));
         store1->num_components = 2;
         nir_builder_instr_insert(&b, &store1->instr);
         store2->src[0] = nir_src_for_ssa(src2);
         store2->src[1] = nir_src_for_ssa(nir_iadd_imm(&b, intr->src[1].ssa, 16));
         nir_intrinsic_set_write_mask(store2, old_components == 3 ? 1 : 3);
         nir_intrinsic_set_align_mul(store2, nir_intrinsic_align_mul(intr));
         store2->num_components = old_components - 2;
         nir_builder_instr_insert(&b, &store2->instr);
         nir_instr_remove(instr);
      }
   }
}

static bool
mwv207_lower_double_store_global(nir_shader *nir)
{
   nir_foreach_function(function, nir)
      if (function->impl)
         split_double_store_global(function->impl);
   return true;
}

static bool
vec_dest_has_swizzle(nir_alu_instr *vec, nir_def *ssa)
{
   for (uint32_t i = 0; i < vec->def.num_components; i++) {
      if (vec->src[i].src.ssa != ssa)
         continue;

      if (vec->src[i].swizzle[0] != i)
         return true;
   }

   nir_foreach_use(use_src, ssa) {
      nir_instr *instr = nir_src_parent_instr(use_src);

      if (instr->type != nir_instr_type_alu)
         continue;

      nir_alu_instr *alu = nir_instr_as_alu(instr);

      switch (alu->op) {
      case nir_op_mov:
      case nir_op_vec2:
      case nir_op_vec3:
      case nir_op_vec4:
         return true;

      default:
         break;
      }
   }
   return false;
}

static uint32_t
insert_vec_mov(nir_alu_instr *vec, uint32_t start_idx, nir_shader *nir)
{
   assert(start_idx < nir_op_infos[vec->op].num_inputs);
   uint32_t write_mask = (1u << start_idx);
   nir_alu_instr *mov = nir_alu_instr_create(nir, nir_op_mov);
   nir_alu_src_copy(&mov->src[0], &vec->src[start_idx]);

   mov->src[0].swizzle[0] = vec->src[start_idx].swizzle[0];

   if (is_src_mod_neg(&vec->instr, start_idx))
      set_src_mod_neg(&mov->instr, 0);

   if (is_src_mod_abs(&vec->instr, start_idx))
      set_src_mod_abs(&mov->instr, 0);

   uint32_t num_components = 1;

   for (uint32_t i = start_idx + 1; i < vec->def.num_components; i++) {

      if (nir_srcs_equal(vec->src[i].src, vec->src[start_idx].src) &&
          is_src_mod_neg(&vec->instr, i) == is_src_mod_neg(&vec->instr, start_idx) &&
          is_src_mod_abs(&vec->instr, i) == is_src_mod_neg(&vec->instr, start_idx)) {
         write_mask |= (1 << i);
         mov->src[0].swizzle[num_components] = vec->src[i].swizzle[0];
         num_components++;
      }
   }

   nir_def_init(&mov->instr, &mov->def, num_components, vec->def.bit_size);

   for (uint32_t i = 0, j = 0; i < 4; i++) {
      if (!(write_mask & (1 << i)))
         continue;

      nir_src_rewrite(&vec->src[i].src, &mov->def);
      vec->src[i].swizzle[0] = j++;
   }

   nir_instr_insert_before(&vec->instr, &mov->instr);

   return write_mask;
}

static void
mwv207_lower_vecn_impl(nir_function_impl *impl, struct mwv207_compile *c)
{
   nir_shader *nir = impl->function->shader;

   nir_builder b = nir_builder_create(impl);

   nir_foreach_block(block, impl) {
      nir_foreach_instr_safe(instr, block) {
         if (instr->type != nir_instr_type_alu)
            continue;

         nir_alu_instr *alu = nir_instr_as_alu(instr);

         if (alu->op != nir_op_vec2 && alu->op != nir_op_vec3 &&
             alu->op != nir_op_vec4)
            continue;

         nir_op_info info = nir_op_infos[alu->op];

         b.cursor = nir_before_instr(&alu->instr);

         nir_const_value value[4];
         uint32_t num_components = 0;

         for (uint32_t i = 0; i < info.num_inputs; i++) {
            nir_const_value *cv = get_alu_cv(&alu->src[i]);

            if (cv)
               value[num_components++] = cv[alu->src[i].swizzle[0]];
         }

         if (num_components > 1) {
            nir_def *def = nir_build_imm(&b, num_components, 32, value);

            if (num_components == info.num_inputs) {
               nir_def_rewrite_uses(&alu->def, def);
               nir_instr_remove(&alu->instr);
               continue;
            }

            for (uint32_t i = 0, j = 0; i < info.num_inputs; i++) {
               nir_const_value *cv = get_alu_cv(&alu->src[i]);

               if (!cv)
                  continue;

               nir_src_rewrite(&alu->src[i].src, def);
               alu->src[i].swizzle[0] = j++;
            }
         }

         uint32_t finished_write_mask = 0;

         for (uint32_t i = 0; i < alu->def.num_components; i++) {

            nir_def *ssa = alu->src[i].src.ssa;

            bool need_mov = list_length(&ssa->uses) != 0;
            nir_foreach_use(use_src, ssa) {
               if (nir_src_is_if(use_src) || nir_src_parent_instr(use_src) != &alu->instr)
                  need_mov = true;
            }

            nir_instr *instr = ssa->parent_instr;

            switch (instr->type) {
            case nir_instr_type_alu:
            case nir_instr_type_tex:
               break;

            case nir_instr_type_intrinsic:
               if (nir_instr_as_intrinsic(instr)->intrinsic == nir_intrinsic_load_input) {
                  need_mov = vec_dest_has_swizzle(alu, &nir_instr_as_intrinsic(instr)->def);
                  break;
               }
               FALLTHROUGH;
            default:
               need_mov = true;
            }

            if (need_mov && !(finished_write_mask & (1 << i)))
               finished_write_mask |= insert_vec_mov(alu, i, nir);
         }
      }
   }
}

static void
mwv207_lower_vecn(nir_shader *nir, struct mwv207_compile *c)
{
   nir_foreach_function(function, nir) {
      if (function->impl)
         mwv207_lower_vecn_impl(function->impl, c);
   }
}

static void
mwv207_exchange_texture_swizzle(struct mwv207_context *ctx, nir_tex_instr *tex,
                              nir_alu_instr *mov, nir_tex_instr *tex_ba, nir_builder b)
{
   struct mwv207_sampler_view *sampler_view =
      mwv207_sampler_view(ctx->sampler_views[PIPE_SHADER_FRAGMENT][tex->sampler_index]);

   for (int i = 0; i < 4; i++) {
      unsigned swizzle;
      switch (i) {
      case 0:
         swizzle = sampler_view->base.swizzle_r;
         break;
      case 1:
         swizzle = sampler_view->base.swizzle_g;
         break;
      case 2:
         swizzle = sampler_view->base.swizzle_b;
         break;
      case 3:
         swizzle = sampler_view->base.swizzle_a;
         break;
      }
      switch (swizzle) {
      case 0:
         mov->src[i].src = nir_src_for_ssa(&tex->def);
         mov->src[i].swizzle[0] = 0;
         break;
      case 1:
         mov->src[i].src = nir_src_for_ssa(&tex->def);
         mov->src[i].swizzle[0] = 1;
         break;
      case 2:
         mov->src[i].src = nir_src_for_ssa(&tex_ba->def);
         mov->src[i].swizzle[0] = 0;
         break;
      case 3:
         mov->src[i].src = nir_src_for_ssa(&tex_ba->def);
         mov->src[i].swizzle[0] = 1;
         break;
      case 4:
         mov->src[i].src = nir_src_for_ssa(nir_imm_float(&b, 0.0));
         mov->src[i].swizzle[0] = 0;
         break;
      case 5:
         mov->src[i].src = nir_src_for_ssa(nir_imm_float(&b, 1.0));
         mov->src[i].swizzle[0] = 0;
         break;
      default:
         assert(!"WRONG SWIZZLE");
         return;
      }
   }
}

static void
mwv207_lower_tex_128_impl(nir_function_impl *impl, struct mwv207_context *ctx)
{
   nir_shader *nir = impl->function->shader;

   nir_builder b = nir_builder_create(impl);

   nir_foreach_block(block, impl) {
      nir_foreach_instr_safe(instr, block) {
         if (instr->type != nir_instr_type_tex)
            continue;

         nir_tex_instr *tex = nir_instr_as_tex(instr);

         if (tex->op == nir_texop_tg4)
            continue;

         if (ctx->info.tex_record[PIPE_SHADER_FRAGMENT][tex->sampler_index] == 0)
            continue;

         if (tex->def.num_components == 1)
            continue;

         b.cursor = nir_before_instr(instr);

         nir_tex_instr *tex_ba = nir_tex_instr_create(nir, tex->num_srcs);

         for (int i = 0; i < tex->num_srcs; i++) {
            tex_ba->src[i].src_type = tex->src[i].src_type;
            tex_ba->src[i].src = nir_src_for_ssa(tex->src[i].src.ssa);
         }

         tex_ba->op = tex->op;
         tex_ba->sampler_dim = tex->sampler_dim;
         tex_ba->dest_type = tex->dest_type;
         tex_ba->num_srcs = tex->num_srcs;
         tex_ba->component = tex->component;
         tex_ba->coord_components = tex->coord_components;
         tex_ba->is_array = tex->is_array;
         tex_ba->is_shadow = tex->is_shadow;
         tex_ba->is_new_style_shadow = tex->is_new_style_shadow;
         tex_ba->texture_non_uniform = tex->texture_non_uniform;
         tex_ba->sampler_index = ctx->info.tex_record[PIPE_SHADER_FRAGMENT][tex->sampler_index];
         tex_ba->texture_index = tex_ba->sampler_index;
         tex_ba->sampler_non_uniform = tex->sampler_non_uniform;
         tex_ba->tg4_offsets[0][0] = tex->tg4_offsets[0][0];
         tex_ba->tg4_offsets[0][1] = tex->tg4_offsets[0][1];
         tex_ba->tg4_offsets[1][0] = tex->tg4_offsets[1][0];
         tex_ba->tg4_offsets[1][1] = tex->tg4_offsets[1][1];
         tex_ba->tg4_offsets[2][0] = tex->tg4_offsets[2][0];
         tex_ba->tg4_offsets[2][1] = tex->tg4_offsets[2][1];
         tex_ba->tg4_offsets[3][0] = tex->tg4_offsets[3][0];
         tex_ba->tg4_offsets[3][1] = tex->tg4_offsets[3][1];

         nir_def_init(&tex_ba->instr, &tex_ba->def, 4, 32);

         nir_builder_instr_insert(&b, &tex_ba->instr);

         nir_alu_instr *mov = nir_alu_instr_create(nir, nir_op_vec4);
         mwv207_exchange_texture_swizzle(ctx, tex, mov, tex_ba, b);

         nir_def_init(&mov->instr, &mov->def, 4, 32);

         nir_instr_insert_after(instr, &mov->instr);

         nir_def_rewrite_uses_after(&tex->def, &mov->def, &mov->instr);
      }
   }
}

static void
mwv207_lower_fb_128_impl(nir_function_impl *impl, struct mwv207_context *ctx)
{
   nir_shader *nir = impl->function->shader;

   uint32_t location = 0;
   nir_op op;
   nir_builder b = nir_builder_create(impl);

   nir_foreach_block(block, impl) {
      nir_foreach_instr_safe(instr, block) {
         if (instr->type != nir_instr_type_intrinsic)
            continue;

         nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);

         if (intr->intrinsic != nir_intrinsic_store_deref)
            continue;

         nir_variable *base = nir_src_as_deref(intr->src[0])->var;

         if (base->data.location == FRAG_RESULT_COLOR)
            location = FRAG_RESULT_DATA0;
         else
            location = base->data.location;

         if (!ctx->info.pixel_record[location - FRAG_RESULT_DATA0])
            continue;

         if (intr->src[1].ssa->num_components == 4)
            op = nir_op_vec4;
         else if (intr->src[1].ssa->num_components == 3)
            op = nir_op_vec3;
         else
            continue;

         b.cursor = nir_before_instr(instr);

         nir_alu_instr *mov_ba = nir_alu_instr_create(nir, op);
         mov_ba->src[0].src = nir_src_for_ssa(intr->src[1].ssa);
         mov_ba->src[0].swizzle[0] = 2;
         mov_ba->src[2].src = nir_src_for_ssa(nir_imm_float(&b, 0.0));
         mov_ba->src[2].swizzle[0] = 0;

         if (op == nir_op_vec4) {
            mov_ba->src[1].src = nir_src_for_ssa(intr->src[1].ssa);
            mov_ba->src[1].swizzle[0] = 3;
            mov_ba->src[3].src = nir_src_for_ssa(nir_imm_float(&b, 1.0));
            mov_ba->src[3].swizzle[0] = 0;
            nir_def_init(&mov_ba->instr, &mov_ba->def, 4, 32);
         } else {
            mov_ba->src[1].src = nir_src_for_ssa(nir_imm_float(&b, 0.0));
            mov_ba->src[1].swizzle[0] = 0;
            nir_def_init(&mov_ba->instr, &mov_ba->def, 3, 32);
         }

         nir_builder_instr_insert(&b, &mov_ba->instr);

         nir_variable *var = nir_variable_create(nir, nir_var_shader_out,
                                                 base->type, "fix_var");
         var->data.location = FRAG_RESULT_DATA0 +
                              ctx->info.pixel_record[location - FRAG_RESULT_DATA0];
         nir->num_outputs++;

         nir_load_var(&b, var);

         nir_intrinsic_instr *store =
            nir_intrinsic_instr_create(nir, nir_intrinsic_store_deref);

         store->num_components = intr->num_components;
         store->src[0] = nir_src_for_ssa(&nir_build_deref_var(&b, var)->def);
         store->src[1] = nir_src_for_ssa(&mov_ba->def);
         nir_intrinsic_set_write_mask(store, ((1 << intr->num_components) - 1));

         nir_builder_instr_insert(&b, &store->instr);

         nir_alu_instr *mov_rg = nir_alu_instr_create(nir, op);
         mov_rg->src[0].src = nir_src_for_ssa(intr->src[1].ssa);
         mov_rg->src[0].swizzle[0] = 0;
         mov_rg->src[1].src = nir_src_for_ssa(intr->src[1].ssa);
         mov_rg->src[1].swizzle[0] = 1;
         mov_rg->src[2].src = nir_src_for_ssa(nir_imm_float(&b, 0.0));
         mov_rg->src[2].swizzle[0] = 0;

         if (op == nir_op_vec4) {
            mov_rg->src[3].src = nir_src_for_ssa(nir_imm_float(&b, 1.0));
            mov_rg->src[3].swizzle[0] = 0;
            nir_def_init(&mov_rg->instr, &mov_rg->def, 4, 32);
         } else {
            nir_def_init(&mov_rg->instr, &mov_rg->def, 3, 32);
         }

         nir_builder_instr_insert(&b, &mov_rg->instr);

         nir_def_rewrite_uses_after(intr->src[1].ssa,
            &mov_rg->def, &mov_rg->instr);
     }
   }
}

static void
mwv207_lower_128bit(nir_shader *nir, struct mwv207_context *ctx)
{
   if (nir->info.stage != MESA_SHADER_FRAGMENT)
      return;

   if (ctx->info.tex_128bit) {
      nir_foreach_function(function, nir) {
         if (function->impl)
            mwv207_lower_tex_128_impl(function->impl, ctx);
      }
   }

   if (ctx->info.pixel_128bit) {
      nir_foreach_function(function, nir) {
         if (function->impl)
            mwv207_lower_fb_128_impl(function->impl, ctx);
      }
   }
}

static void
mwv207_lower_rgba_swap_impl(nir_function_impl *impl, struct mwv207_context *ctx)
{
   nir_shader *shader = impl->function->shader;

   nir_op op;
   nir_builder b = nir_builder_create(impl);

   nir_foreach_block(block, impl) {
      nir_foreach_instr_safe(instr, block) {
         if (instr->type != nir_instr_type_intrinsic)
            continue;

         nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);

         if (intr->intrinsic != nir_intrinsic_store_deref)
            continue;

         nir_deref_instr *deref = nir_src_as_deref(intr->src[0]);
         assert(deref->deref_type == nir_deref_type_var);
         nir_variable *base = deref->var;
         int record_local = base->data.location - FRAG_RESULT_DATA0;

         if (intr->src[1].ssa->num_components == 4)
            op = nir_op_vec4;
         else if (intr->src[1].ssa->num_components == 3)
            op = nir_op_vec3;
         else
            continue;

         if ((SWIZZLE_STATE(ctx->info.swizzle_record[record_local])))
            continue;

         b.cursor = nir_before_instr(instr);
         nir_alu_instr *mov = nir_alu_instr_create(shader, nir_op_vec4);
         mov->src[0].src = nir_src_for_ssa(intr->src[1].ssa);
         mov->src[0].swizzle[0] = ctx->info.swizzle_record[record_local][0];
         mov->src[1].src = nir_src_for_ssa(intr->src[1].ssa);
         mov->src[1].swizzle[0] = ctx->info.swizzle_record[record_local][1];
         mov->src[2].src = nir_src_for_ssa(intr->src[1].ssa);
         mov->src[2].swizzle[0] = ctx->info.swizzle_record[record_local][2];

         if (op == nir_op_vec4) {
            mov->src[3].src = nir_src_for_ssa(intr->src[1].ssa);
            mov->src[3].swizzle[0] = ctx->info.swizzle_record[record_local][3];
            nir_def_init(&mov->instr, &mov->def, 4, 32);
         } else {
            mov->src[3].src = nir_src_for_ssa(nir_imm_float(&b, 0.0));
            mov->src[3].swizzle[0] = 0;
            nir_def_init(&mov->instr, &mov->def, 3, 32);
         }

         nir_builder_instr_insert(&b, &mov->instr);
         nir_def_rewrite_uses_after(intr->src[1].ssa, &mov->def, &mov->instr);
      }
   }
}

static void
mwv207_lower_rgba_swap(nir_shader *shader, struct mwv207_context *ctx)
{
   if (shader->info.stage != MESA_SHADER_FRAGMENT)
      return;

   if (ctx->info.frag_rgba_swap) {
      nir_foreach_function(function, shader) {
         if (function->impl)
            mwv207_lower_rgba_swap_impl(function->impl, ctx);
      }
   }
}

static void
mwv207_lower_txs(nir_shader *nir, struct mwv207_context *ctx, struct mwv207_shader *shader)
{
   nir_foreach_function(function, nir) {
      nir_builder b = nir_builder_create(function->impl);
      nir_foreach_block(block, function->impl) {
         nir_foreach_instr_safe(instr, block) {
            if (instr->type != nir_instr_type_tex)
               continue;

            nir_tex_instr *tex = nir_instr_as_tex(instr);

            if (tex->op == nir_texop_txs) {

               uint8_t tex_dim = tex->sampler_dim == GLSL_SAMPLER_DIM_2D ? 2 : 3;

               if (tex->is_array)
                  tex_dim = 3;

               b.cursor = nir_after_instr(instr);
               nir_intrinsic_instr *load =
                  nir_intrinsic_instr_create(b.shader, nir_intrinsic_load_uniform);
               nir_intrinsic_set_base(load, ~tex->sampler_index);
               nir_intrinsic_set_range(load, 1);
               load->num_components = tex_dim;
               load->src[0] = nir_src_for_ssa(nir_imm_int(&b, 0));
               nir_def_init(&load->instr, &load->def, tex_dim, 32);
               nir_intrinsic_set_dest_type(load, nir_type_int);
               nir_builder_instr_insert(&b, &load->instr);

               nir_def_rewrite_uses_after(&tex->def, &load->def, &load->instr);
               nir_instr_remove(&tex->instr);
            }
         }
      }
   }
}

static void
mwv207_lower_texelFetch(nir_shader *nir, struct mwv207_context *ctx, struct mwv207_shader *shader)
{
   nir_foreach_function(function, nir) {
      nir_builder b = nir_builder_create(function->impl);
      nir_foreach_block(block, function->impl) {
         nir_foreach_instr_safe(instr, block) {
            if (instr->type != nir_instr_type_tex)
               continue;
            nir_tex_instr *tex = nir_instr_as_tex(instr);
            if (tex->op != nir_texop_txf)
               continue;
            if (tex->coord_components >= 2)
               continue;
            b.cursor = nir_before_instr(instr);
            int coord_index = nir_tex_instr_src_index(tex, nir_tex_src_coord);
            nir_def *coord_2d = nir_vec2(&b, tex->src[coord_index].src.ssa, nir_imm_int(&b, 0));
            tex->coord_components = 2;
            nir_src_rewrite(&tex->src[coord_index].src, coord_2d);
         }
      }
   }
}

static void
mwv207_lower_roundEven(nir_shader *nir, struct mwv207_context *ctx)
{
   nir_foreach_function(function, nir) {
       nir_builder b = nir_builder_create(function->impl);
      nir_foreach_block(block, function->impl) {
         nir_foreach_instr_safe(instr, block) {
            if (instr->type != nir_instr_type_alu)
               continue;

            nir_alu_instr *alu = nir_instr_as_alu(instr);

            if (alu->op == nir_op_fround_even) {
               alu->op = nir_op_mov;
               nir_def *src = &alu->def;

               b.cursor = nir_after_instr(instr);
               nir_def *sign = nir_fsign(&b, src);
               nir_def *absSRC = nir_fabs(&b, src);
               nir_def *add05 = nir_fround_even(&b, absSRC);
               nir_def *floorRst = nir_ffloor(&b, add05);

               nir_def *frac = nir_ffract(&b, absSRC);
               nir_def *intrPart = nir_ffloor(&b, absSRC);

               nir_def *isEven = nir_iand(&b, nir_f2i32(&b, intrPart), nir_imm_int(&b, 0x1));
               nir_def *tmpRst = nir_fcsel(&b, nir_i2f32(&b, isEven), nir_fadd(&b, intrPart, nir_imm_float(&b, 1.0f)) , intrPart);

               nir_def *finalRst = nir_fcsel(&b, nir_b2f32(&b, nir_feq32(&b, frac, nir_imm_float(&b, 0.5))), tmpRst, floorRst);

               nir_def *rst = nir_fmul(&b, sign, finalRst);
               nir_def_rewrite_uses_after(&alu->def, rst, rst->parent_instr);
            }
         }
      }
   }
}

static void
mwv207_nir_remove_shadow_tex(nir_tex_instr *tex,
                             nir_def * (*set)(nir_builder *b, nir_def *src0, nir_def *src1),
                             nir_builder *b, bool swap)
{
   nir_def *compare = NULL, *result;
   uint32_t i;

   for (i = 0; i < tex->num_srcs; i++) {
      if (tex->src[i].src_type == nir_tex_src_comparator) {
         compare = tex->src[i].src.ssa;
         break;
      }
   }

   if (!compare)
      return;

   nir_tex_instr_remove_src(tex, i);

   b->cursor = nir_after_instr(&tex->instr);

   if (swap)
      result = set(b, compare, &tex->def);
   else
      result = set(b, &tex->def, compare);

   nir_def_rewrite_uses_after(&tex->def, result, result->parent_instr);
}

static void
mwv207_lower_shadow_tex_impl(nir_function_impl *impl, struct mwv207_context *ctx,
                             struct mwv207_shader *shader)
{
   struct pipe_sampler_view *view;
   struct mwv207_sampler *sampler;
   nir_builder b = nir_builder_create(impl);

   nir_foreach_block(block, impl) {
      nir_foreach_instr_safe(instr, block) {
         if (instr->type != nir_instr_type_tex)
            continue;

         nir_tex_instr *tex = nir_instr_as_tex(instr);

         if (!tex->is_shadow)
            continue;

         sampler = ctx->samplers[shader->stage][tex->sampler_index];
         view = ctx->sampler_views[shader->stage][tex->texture_index];

         if (!sampler || !view)
            continue;

         if (view->texture->format != PIPE_FORMAT_Z32_FLOAT)
            continue;

         shader->z32_shadow[shader->stage][tex->texture_index] = true;

         switch (sampler->state.compare_func) {
         case PIPE_FUNC_NEVER:
            b.cursor = nir_before_instr(&tex->instr);
            nir_def_rewrite_uses(&tex->def, nir_imm_float(&b, 0.0f));
            nir_instr_remove(&tex->instr);
            break;

         case PIPE_FUNC_ALWAYS:
            b.cursor = nir_before_instr(&tex->instr);
            nir_def_rewrite_uses(&tex->def, nir_imm_float(&b, 1.0f));
            nir_instr_remove(&tex->instr);
            break;

         case PIPE_FUNC_EQUAL:
            mwv207_nir_remove_shadow_tex(tex, nir_seq, &b, false);
            break;

         case PIPE_FUNC_NOTEQUAL:
            mwv207_nir_remove_shadow_tex(tex, nir_sne, &b, false);
            break;

         case PIPE_FUNC_GREATER:
            mwv207_nir_remove_shadow_tex(tex, nir_slt, &b, false);
            break;

         case PIPE_FUNC_GEQUAL:
            mwv207_nir_remove_shadow_tex(tex, nir_sge, &b, true);
            break;

         case PIPE_FUNC_LESS:
            mwv207_nir_remove_shadow_tex(tex, nir_slt, &b, true);
            break;

         case PIPE_FUNC_LEQUAL:
            mwv207_nir_remove_shadow_tex(tex, nir_sge, &b, false);
            break;
         }
      }
   }
}

static void
mwv207_lower_shadow_tex(nir_shader *nir, struct mwv207_context *ctx,
                        struct mwv207_shader *shader)
{
   if (ctx->info.z32_shadow) {
      nir_foreach_function(function, nir) {
         if (function->impl)
            mwv207_lower_shadow_tex_impl(function->impl, ctx, shader);
      }
   }
}

static int
mwv207_glsl_type_size(const struct glsl_type *type, bool bindless)
{
   return glsl_count_attribute_slots(type, false);
}

static void
mwv207_optimize_loop(nir_shader *nir)
{
   bool progress;

   do {
      progress = false;

      NIR_PASS_V(nir, nir_lower_vars_to_ssa);
      progress |= OPT(nir, nir_opt_copy_prop_vars);
      progress |= OPT(nir, nir_opt_shrink_stores, true);
      progress |= OPT(nir, nir_opt_shrink_vectors, false);
      progress |= OPT(nir, nir_copy_prop);
      progress |= OPT(nir, nir_opt_dce);
      progress |= OPT(nir, nir_opt_cse);
      progress |= OPT(nir, nir_opt_peephole_select, 16, true, true);
      progress |= OPT(nir, nir_opt_intrinsics);
      progress |= OPT(nir, nir_opt_algebraic);
      progress |= OPT(nir, nir_opt_constant_folding);
      progress |= OPT(nir, nir_opt_dead_cf);

      if (OPT(nir, nir_opt_loop)) {
         progress = true;
         OPT(nir, nir_copy_prop);
         OPT(nir, nir_opt_dce);
      }

      progress |= OPT(nir, nir_opt_loop_unroll);
      progress |= OPT(nir, nir_opt_if, nir_opt_if_optimize_phi_true_false);
      progress |= OPT(nir, nir_lower_64bit_phis);
      progress |= OPT(nir, nir_opt_remove_phis);
      progress |= OPT(nir, nir_opt_undef);
      NIR_PASS(progress, nir, nir_opt_conditional_discard);
      if (nir->info.stage == MESA_SHADER_FRAGMENT)
         NIR_PASS_V(nir, nir_opt_move_discards_to_top);
   } while (progress);
}

#define do_by_default(OPT) (debug_get_bool_option("OPT_"#OPT, false))

static nir_mem_access_size_align
mem_access_size_align_cb(nir_intrinsic_op intrin, uint8_t bytes,
                         uint8_t bit_size, uint32_t align,
                         uint32_t align_offset, bool offset_is_const,
                         const void *cb_data)
{
   align = nir_combined_align(align, align_offset);

   assert(util_is_power_of_two_nonzero(align));

   return (nir_mem_access_size_align){
      .num_components = MIN2(bytes / (bit_size / 8), 4),
      .bit_size = bit_size,
      .align = bit_size / 8,
   };
}

static uint8_t
lower_vec816_alu(const nir_instr *instr, const void *cb_data)
{
   return 4;
}

void
mwv207_lower(struct mwv207_context *ctx, struct mwv207_compile *c)
{
   nir_shader *nir = c->nir;
   struct mwv207_shader *shader = c->shader;

   NIR_PASS_V(nir, mwv207_early_lower, c);

   NIR_PASS_V(nir, nir_remove_dead_variables, nir_var_shader_in, NULL);

   if (shader->stage != PIPE_SHADER_TESS_CTRL &&
       shader->stage != PIPE_SHADER_TESS_EVAL) {
      NIR_PASS_V(nir, nir_lower_io, nir_var_shader_in | nir_var_uniform, mwv207_glsl_type_size,
                 nir_lower_io_lower_64bit_to_32);
   } else {
      NIR_PASS_V(nir, nir_lower_io, nir_var_uniform, mwv207_glsl_type_size,
                 nir_lower_io_lower_64bit_to_32);
   }

   NIR_PASS_V(nir, nir_lower_vars_to_ssa);

   if (shader->stage != PIPE_SHADER_TESS_CTRL &&
       shader->stage != PIPE_SHADER_TESS_EVAL)
      NIR_PASS_V(nir, nir_lower_indirect_derefs, nir_var_all, UINT32_MAX);

   NIR_PASS_V(nir, nir_lower_tex,
   &(struct nir_lower_tex_options) {
      .lower_txp = ~0u,
      .lower_txf_offset = true,
      .lower_txd_3d = true,
      .lower_txd = true,
      .lower_txs_lod = true,});

   NIR_PASS_V(nir, nir_lower_alu_to_scalar, mwv207_alu_to_scalar_filter_cb, NULL);

   if (do_by_default(DIV))
      NIR_PASS_V(nir, mwv207_nir_opt_peephole_fdiv);

   NIR_PASS_V(nir, nir_lower_flrp, 16 | 32 | 64, false);
   NIR_PASS_V(nir, nir_lower_frexp);

   mwv207_optimize_loop(nir);

   if (shader->stage == MESA_SHADER_KERNEL) {
      nir_lower_mem_access_bit_sizes_options lower_mem_access_options = {
         .modes = nir_var_all,
         .may_lower_unaligned_stores_to_atomics = true,
         .callback = mem_access_size_align_cb,
         .cb_data = NULL,
      };
      NIR_PASS_V(nir, nir_lower_mem_access_bit_sizes, &lower_mem_access_options);
      NIR_PASS_V(nir, nir_lower_alu_width, lower_vec816_alu, NULL);
      NIR_PASS_V(nir, nir_lower_alu_vec8_16_srcs);
      NIR_PASS_V(nir, nir_lower_phis_to_scalar, false);
   }
   NIR_PASS_V(nir, nir_opt_dce);
   NIR_PASS_V(nir, nir_lower_alu_to_scalar, mwv207_fp64_to_scalar_filter_cb, NULL);
   NIR_PASS_V(nir, mwv207_nir_split_64bit_var);
   NIR_PASS_V(nir, mwv207_lower_double_store_global);
   NIR_PASS_V(nir, nir_split_64bit_vec3_and_vec4);
   NIR_PASS_V(nir, nir_lower_int64);
   NIR_PASS_V(nir, mwv207_nir_lower_64bit_uniforms_and_ubo);
   NIR_PASS_V(nir, nir_copy_prop);
   NIR_PASS_V(nir, nir_opt_algebraic);
   NIR_PASS_V(nir, nir_opt_dce);
   NIR_PASS_V(nir, mwv207_nir_64_to_vec2);
   NIR_PASS_V(nir, nir_opt_combine_stores, nir_var_shader_out);
   NIR_PASS_V(nir, nir_lower_discard_or_demote, true);

   mwv207_optimize_loop(nir);
   if (shader->stage != MESA_SHADER_KERNEL)
      if (OPT(nir, mwv207_nir_lower_ubo_to_uniform))
         mwv207_optimize_loop(nir);

   NIR_PASS_V(nir, mwv207_lower_io, shader);

   if (shader->stage == MESA_SHADER_FRAGMENT && shader->key.sprite_coord_enable)
      mwv207_lower_sprite_coord_late(nir ,c);

   NIR_PASS_V(nir, nir_lower_pack);
   mwv207_optimize_loop(nir);

   nir_lower_idiv_options idiv_options = {
      .allow_fp16 = true,
   };
   NIR_PASS_V(nir, nir_lower_idiv, &idiv_options);
   NIR_PASS_V(nir, nir_lower_alu);

   mwv207_optimize_loop(nir);

   if (do_by_default(FFMA))
      NIR_PASS_V(nir, mwv207_nir_opt_peephole_ffma);

   if (do_by_default(SET))
      NIR_PASS_V(nir, mwv207_nir_opt_peephole_set);

   if (shader->stage == MESA_SHADER_KERNEL) {
      NIR_PASS_V(nir, mwv207_lower_global);
   }

   while( OPT(nir, nir_opt_vectorize, NULL, NULL) );

   if (nir->info.stage == MESA_SHADER_FRAGMENT)
      NIR_PASS_V(nir, mwv207_lower_dual_blend, ctx, shader);

   NIR_PASS_V(nir, nir_lower_alu_to_scalar, mwv207_alu_to_scalar_filter_cb, NULL);

   NIR_PASS_V(nir, nir_remove_dead_variables, nir_var_function_temp, NULL);
   NIR_PASS_V(nir, nir_opt_algebraic_late);
   NIR_PASS_V(nir, nir_opt_constant_folding);

   NIR_PASS_V(nir, nir_move_vec_src_uses_to_dest, false);
   NIR_PASS_V(nir, nir_copy_prop);

   NIR_PASS_V(nir, nir_opt_dce);
   NIR_PASS_V(nir, nir_opt_cse);
   NIR_PASS_V(nir, nir_lower_bool_to_int32);
   NIR_PASS_V(nir, nir_lower_bool_to_bitsize);
   NIR_PASS_V(nir, mwv207_lower_alu, c);
   NIR_PASS_V(nir, mwv207_lower_roundEven, ctx);
   NIR_PASS_V(nir, mwv207_lower_txs, ctx, shader);
   NIR_PASS_V(nir, mwv207_lower_texelFetch, ctx, shader);
   NIR_PASS_V(nir, mwv207_lower_128bit, ctx);
   NIR_PASS_V(nir, mwv207_lower_shadow_tex, ctx, shader);
   NIR_PASS_V(nir, mwv207_nir_lower_tg4, shader, ctx);
   NIR_PASS_V(nir, mwv207_lower_rgba_swap, ctx);
   NIR_PASS_V(nir, mwv207_lower_vecn, c);
   NIR_PASS_V(nir, mwv207_lower_shared_atomic, c, ctx);
}

#undef do_by_default
