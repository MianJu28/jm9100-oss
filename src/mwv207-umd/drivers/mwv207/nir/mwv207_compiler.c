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
#include <stdlib.h>
#include "mwv207_common.h"
#include "mwv207_compiler.h"
#include "mwv207_cmd.h"
#include "mwv207_context.h"
#include "mwv207_emit.h"

static uint32_t
mwv207_get_interpolation(uint32_t interpolation)
{
   switch (interpolation) {
   case INTERP_MODE_FLAT:
      return 0x2;
   case INTERP_MODE_NOPERSPECTIVE:
      return 0x1;
   default:
      return 0x0;
   }
}

static void
mwv207_record_input(struct mwv207_compile *c)
{
   nir_shader *nir = c->nir;
   struct mwv207_shader *shader = c->shader;

   struct mwv207_shader_io_file *sf = &shader->infile;

   if (shader->stage == PIPE_SHADER_FRAGMENT) {
      nir_foreach_shader_in_variable(var, nir) {
         uint32_t idx = var->data.driver_location;

         switch (var->data.location) {
         case VARYING_SLOT_CLIP_DIST0:
         case VARYING_SLOT_CLIP_DIST1: {
            shader->useClipDistance = true;
            for (int i = 0; i < glsl_get_length(var->type); i++) {
               sf->reg[idx].reg = idx + 1;
               sf->reg[idx].slot = VARYING_SLOT_CLIP_DIST0;
               sf->reg[idx].is_array = true;
               sf->reg[idx].arry_index = i;
               sf->reg[idx].num_components = glsl_get_components(glsl_without_array(var->type));
               sf->num_reg = MAX2(sf->num_reg, idx + 1);
               idx++;
            }
         } break;
         case VARYING_SLOT_PNTC:
            shader->usePointCoord = true;
            FALLTHROUGH;
         default: {
            for (uint32_t i = 0; i < glsl_count_attribute_slots(var->type, false); ++i) {
               sf->reg[idx].reg = idx + 1;
               sf->reg[idx].slot = var->data.location + i;
               sf->reg[idx].is_array = false;
               sf->reg[idx].interpolation = mwv207_get_interpolation(var->data.interpolation);
               sf->reg[idx].num_components = glsl_get_components(glsl_without_array(var->type));
               sf->num_reg = MAX2(sf->num_reg, idx + 1);
               idx++;
            }
         } break;
         }
      }
   } else {
      shader->inner_outer_count = 0;
      int offset = -999;
      nir_foreach_shader_in_variable(var, nir) {
         if (!var->data.patch) {
            if (shader->stage == PIPE_SHADER_VERTEX) {
               for (uint32_t i = 0; i < glsl_count_attribute_slots(var->type, false); ++i) {
                  uint32_t idx = var->data.driver_location;
                  idx = idx + i;
                  sf->reg[idx].reg = idx;
                  sf->reg[idx].slot = var->data.location + i;
                  sf->reg[idx].num_components = glsl_get_components(glsl_without_array(var->type));
                  if (glsl_base_type_bit_size(glsl_without_array(var->type)->base_type) == 64) {
                     uint32_t num_components = sf->reg[idx].num_components * 2;
                     if (num_components > 4)
                        num_components = i ? num_components - 4 : 4;
                     sf->reg[idx].num_components = num_components;
                  }
                  sf->reg[idx].interpolation = mwv207_get_interpolation(var->data.interpolation);
                  sf->num_reg = MAX2(sf->num_reg, idx + 1);
               }
            } else {
               uint32_t idx = var->data.driver_location;
               sf->reg[idx].reg = idx;
               sf->reg[idx].slot = var->data.location;
               sf->reg[idx].num_components = glsl_get_components(glsl_without_array(var->type));
               sf->reg[idx].interpolation = mwv207_get_interpolation(var->data.interpolation);
               sf->num_reg = MAX2(sf->num_reg, idx + 1);
            }
         } else {
            if (var->data.location == VARYING_SLOT_TESS_LEVEL_INNER) {
               var->data.driver_location = 1;
               c->shader->inner_outer_count++;
               continue;
            } else if (var->data.location == VARYING_SLOT_TESS_LEVEL_OUTER) {
               var->data.driver_location = 0;
               c->shader->inner_outer_count++;
               continue;
            }

            if (offset == -999)
               offset = var->data.driver_location - 2;

            var->data.driver_location -= offset;
         }
      }
   }
}

static void
mwv207_reset_output_index(struct mwv207_compile *c)
{
   nir_shader *nir = c->nir;
   struct mwv207_shader *shader = c->shader;
   int driver_location = 0;
   int need_fix_loc = 1;

   if (shader->stage == PIPE_SHADER_GEOMETRY) {
      nir_foreach_shader_out_variable(var, nir) {
         switch (var->data.location) {
         case VARYING_SLOT_PRIMITIVE_ID:
            shader->gs_use_primitive_id = true;
            break;
         case VARYING_SLOT_LAYER:
            shader->gs_use_layer = true;
            var->data.driver_location = driver_location++;
            need_fix_loc = var->data.driver_location;
            break;
         case VARYING_SLOT_PNTC:
         case VARYING_SLOT_PSIZ:
            break;
         default:
            var->data.driver_location = driver_location++;
            break;
         }
      }

      if (need_fix_loc != 1) {
         nir_foreach_shader_out_variable(var, nir) {
            if (var->data.driver_location == need_fix_loc)
               var->data.driver_location = 1;
            else if (var->data.driver_location > need_fix_loc)
               var->data.driver_location -= 1;
            else if (var->data.driver_location < need_fix_loc &&
                     var->data.driver_location >= 1)
               var->data.driver_location += 1;
         }
      }

      nir_foreach_shader_out_variable(var, nir) {
         if (var->data.location == VARYING_SLOT_PNTC)
            var->data.driver_location = driver_location++;
      }
      nir_foreach_shader_out_variable(var, nir) {
         if (var->data.location == VARYING_SLOT_PRIMITIVE_ID)
            var->data.driver_location = driver_location++;
      }
      nir_foreach_shader_out_variable(var, nir) {
         if (var->data.location == VARYING_SLOT_PSIZ)
            var->data.driver_location = driver_location++;
      }
   } else if (shader->stage == PIPE_SHADER_TESS_CTRL) {

      uint32_t output_index = 0;
      uint32_t outiut_size[32] = {0};
      uint32_t last_driver_location = 0;
      nir_foreach_shader_out_variable(var, nir) {
         if (var->data.driver_location != 0)
            outiut_size[output_index++] = var->data.driver_location - last_driver_location;

         last_driver_location = var->data.driver_location;
      }

      uint32_t vertex_location = 0;
      uint32_t patch_location = 2;
      shader->output_num = 0;
      shader->patch_num = 0;
      output_index = 0;
      nir_foreach_shader_out_variable(var, nir) {
         if (var->data.patch) {

            if (var->data.location == VARYING_SLOT_TESS_LEVEL_OUTER)
               var->data.driver_location = 0;
            else if (var->data.location == VARYING_SLOT_TESS_LEVEL_INNER)
               var->data.driver_location = 1;
            else {
               var->data.driver_location = patch_location;
               patch_location += outiut_size[output_index];
            }

            shader->patch_num++;
         } else {
            var->data.driver_location = vertex_location;
            vertex_location += outiut_size[output_index];
            shader->output_num++;
         }

         output_index++;
      }

      if (shader->patch_num < 2)
         shader->patch_num = 2;
   }
}

static void
copy_uniform_state_to_shader(struct mwv207_shader *shader, uint64_t *consts, uint32_t count)
{
   struct mwv207_shader_uniform_info *uinfo = &shader->uniforms;

   uinfo->imm_count = count * 4;
   uinfo->imm_data = MALLOC(uinfo->imm_count * sizeof(*uinfo->imm_data));
   uinfo->imm_contents = MALLOC(uinfo->imm_count * sizeof(*uinfo->imm_contents));
   uinfo->no_unifrom = true;

   for (uint32_t i = 0; i < uinfo->imm_count; i++) {
      uinfo->imm_data[i] = consts[i];
      uinfo->imm_contents[i] = consts[i] >> 32;

      if (uinfo->imm_contents[i] == MWV207_IMMEDIATE_UNIFORM)
         uinfo->no_unifrom = false;
   }
}

static void
mwv207_record_uniform(struct mwv207_compile *c)
{
   bool have_indirect_uniform = false;
   uint32_t indirect_max = 0;
   nir_shader *shader = c->nir;
   nir_function_impl *impl = nir_shader_get_entrypoint(shader);

   nir_builder b = nir_builder_create(impl);

   nir_foreach_block(block, impl) {
      nir_foreach_instr_safe(instr, block) {
         switch (instr->type) {
         case nir_instr_type_load_const: {
            nir_load_const_instr *load_const = nir_instr_as_load_const(instr);

            for (uint32_t  i = 0; i < load_const->def.num_components; i++)
               load_const->value[i] = CONST(load_const->value[i].u32);
         } break;
         case nir_instr_type_intrinsic: {
            nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);

            if (intr->intrinsic != nir_intrinsic_load_uniform)
               break;

            nir_const_value *off = nir_src_as_const_value(intr->src[0]);

            if (!off || off[0].u64 >> 32 != MWV207_IMMEDIATE_CONSTANT) {
               have_indirect_uniform = true;
               indirect_max = nir_intrinsic_base(intr) + nir_intrinsic_range(intr);
               break;
            }

            uint32_t base = nir_intrinsic_base(intr);

            base += off[0].u32;
            nir_const_value value[4];

            for (uint32_t i = 0; i < intr->def.num_components; i++) {
               if (nir_intrinsic_base(intr) < 0) {
                  if (nir_intrinsic_range(intr) == 0)
                     value[i] = TEXSCALE(~nir_intrinsic_base(intr), i);
                  else {

                     value[i] = TEXSIZE(~nir_intrinsic_base(intr), i);
                  }
               } else
                  value[i] = UNIFORM(base * 4 + i);
            }

            b.cursor = nir_after_instr(instr);
            nir_def *def = nir_build_imm(&b, intr->def.num_components, 32, value);

            nir_def_rewrite_uses(&intr->def, def);
            nir_instr_remove(instr);
         } break;
         default:
            break;
         }
      }
   }

   if (have_indirect_uniform) {
      for (uint32_t i = 0; i < indirect_max * 4; i++)
         c->consts[i] = UNIFORM(i).u64;

      c->const_count = indirect_max;
   }

   nir_foreach_block(block, impl) {
      nir_foreach_instr_safe(instr, block) {
         if (instr->type != nir_instr_type_intrinsic)
            continue;

         nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);

         switch (intr->intrinsic) {
         case nir_intrinsic_store_deref: {
            nir_src *src = &intr->src[1];

            if (nir_src_is_const(*src) || is_sysval(src->ssa->parent_instr)) {
               b.cursor = nir_before_instr(instr);
               nir_src_rewrite(src, nir_mov(&b, src->ssa));
            }
         } break;
         default:
            break;
         }
      }
   }

   nir_opt_dce(shader);

   nir_convert_from_ssa(shader, true);
   nir_trivialize_registers(shader);
}

static void
mwv207_record_code(struct mwv207_context *ctx,
                   struct mwv207_compile *c,
                   struct mwv207_shader_state *state,
                   bool offline)
{
   struct mwv207_shader *shader = c->shader;

   if (!c->inst_ptr)
      emit_inst(c, &(struct mwv207_inst) { .opcode = 0x0 });

   uint32_t *code = MALLOC(c->inst_ptr * 16);
   for (uint32_t i = 0; i < c->inst_ptr; i++) {
      struct mwv207_inst *inst = &c->code[i];
      if (inst->opcode == 0x16)
         inst->imm = c->block_ptr[inst->imm];
      mwv207_assemble(&code[i * 4], inst);
   }

   shader->code_size = c->inst_ptr * 4;

   if (!offline) {
      if (state->bo == NULL) {

         uint32_t bo_size = shader->code_size * sizeof(uint32_t) * 16;

         if (bo_size < 0x1000)
            bo_size = 0x1000;

         state->bo = ctx->ws->bo_create(ctx->ws, bo_size,
            MWV207_DOMAIN_VRAM, MWV207_FLAG_CPU_ACCESS);
         state->bo_offset = 0;
      }

      if (shader->code) {
         ctx->base.flush(&ctx->base, NULL, 0);
         ctx->ws->bo_wait(ctx->ws, state->bo, OS_TIMEOUT_INFINITE);

         FREE(shader->code);
         char *dst = (char *)ctx->ws->bo_map(ctx->ws, state->bo);
         dst += shader->bo_offset;
         memcpy(dst, code, state->current->code_size * sizeof(uint32_t));
         ctx->ws->bo_unmap(ctx->ws, state->bo);
      } else {
         shader->bo_offset = state->bo_offset;
         char *dst = (char *)ctx->ws->bo_map(ctx->ws, state->bo);
         dst += state->bo_offset;
         memcpy(dst, code, state->current->code_size * sizeof(uint32_t));
         ctx->ws->bo_unmap(ctx->ws, state->bo);

         state->bo_offset += (state->current->code_size * sizeof(uint32_t) * 2);
      }

      assert(state->bo_offset < state->bo->base.base.size);
   }

   shader->code = code;

}

bool
mwv207_compile_shader(struct mwv207_context *ctx, struct mwv207_shader_state *state, bool offline)
{
   struct mwv207_shader *shader = state->current;

   if (unlikely(!shader))
      return false;

   struct mwv207_compile *c = CALLOC_STRUCT(mwv207_compile);

   if (!c)
      return false;

   c->shader = shader;
   c->nir = nir_shader_clone(NULL, shader->info->nir);

   /* FIX(jm9100-oss): 同一个 struct mwv207_shader 会被重编译多次 —— 采样器视图 /
    * 帧缓冲变化命中新的 shader key 时，mwv207_set_current_shader() 会再调一次
    * mwv207_compile_shader()。而 infile/outfile/link 这三份"本次编译产物"挂在
    * shader 上（不在本次编译上下文 c 里），填充时用的是
    *   sf->num_reg = MAX2(sf->num_reg, idx + 1)   /   sf->reg[sf->num_reg++]
    * 只增不减，于是 num_reg 逐次越过 reg[0x10] 的边界，把紧随其后的 link 与
    * code 指针一起覆盖。实测火狐崩溃现场（core dump）：
    *   outfile.num_reg = 25（数组只有 16 项）、shader->code = 0x2300000003，
    *   SIGSEGV 落在 mwv207_record_code() 的 FREE(shader->code)。
    * 每次编译前把这三份记录清零。 */
   memset(&shader->infile, 0, sizeof(shader->infile));
   memset(&shader->outfile, 0, sizeof(shader->outfile));
   memset(&shader->link, 0, sizeof(shader->link));

   /* 同理，立即数表也属于"本次编译产物"，重编译时先释放上一次的再重填，
    * 否则每重编译一次泄漏一份（mwv207_record_uniform 会重新 MALLOC）。 */
   FREE(shader->uniforms.imm_data);
   FREE(shader->uniforms.imm_contents);
   shader->uniforms.imm_data = NULL;
   shader->uniforms.imm_contents = NULL;
   shader->uniforms.imm_count = 0;

   /* 记录编译这份二进制时各状态的指纹，供 mwv207_set_current_shader() 把它
    * 当成缓存键的一部分（布局不同的二进制各自独立成条目）。FIX(jm9100-oss) */
   {
      const unsigned st = (unsigned) shader->stage;
      shader->built_fp_tex_128bit   = (st < PIPE_SHADER_TYPES) ? ctx->fp_tex_128bit[st] : 0;
      shader->built_fp_z32_shadow   = (st < PIPE_SHADER_TYPES) ? ctx->fp_z32_shadow[st] : 0;
   }
   shader->built_fp_pixel_128bit = ctx->fp_pixel_128bit;
   shader->built_fp_is_dual      = ctx->fp_is_dual;
   /* PIPE_CAP_ROBUST_BUFFER_ACCESS_BEHAVIOR is advertised (see
    * mwv207_screen.c), so the guarantee has to actually hold: clamp every
    * SSBO access offset to an in-bounds value before it reaches the hardware.
    * Mesa then exposes GL_KHR_robust_buffer_access_behavior / GL_KHR_robustness,
    * and KHR_robustness is one of the gates for GLES 3.2.
    *
    * Only SSBOs are lowered on purpose:
    *  - the driver implements get_ssbo_size() but NOT get_ubo_size(), so
    *    .lower_ubo would emit an unimplemented intrinsic;
    *  - this pass in 24.1.2 only matches non-deref image intrinsics, while the
    *    driver sees image_deref_*, so .lower_image would be a no-op here;
    *  - .lower_shared is a Vulkan/debug concern.
    */
   const nir_lower_robust_access_options robust_opts = {
      .lower_ssbo = true,
   };
   NIR_PASS_V(c->nir, nir_lower_robust_access, &robust_opts);

   mwv207_lower(ctx, c);

   mwv207_record_input(c);

   mwv207_reset_output_index(c);

   mwv207_record_uniform(c);

   if (unlikely(ctx->ws->debug == 0x201))
      nir_print_shader(c->nir, stdout);

   mwv207_emit_shader(ctx, c);

   copy_uniform_state_to_shader(c->shader, c->consts, c->const_count);

   mwv207_record_code(ctx, c, state, offline);

   if (shader->stage == PIPE_SHADER_VERTEX) {

      int half_out = shader->outfile.num_reg / 2 + 1;

      uint32_t b = MAX2(VS_BALANCE(half_out), 1);

      uint32_t a = (b + 512 / (0x40 * half_out)) / 2;

      shader->vs_load_balancing = (ALIGN_BIT((uint32_t) (MIN2(a, 255)), 0x0) & (0xFF)) |
         (ALIGN_BIT((uint32_t) (MIN2(b, 255)), 0x8) & (0xFF00)) |
         (ALIGN_BIT((uint32_t) (0x3f), 0x10) & (0x3F0000)) |
         (ALIGN_BIT((uint32_t) (0x0f), 0x18) & (0xF000000));
   }

   ralloc_free(c->nir);
   FREE(c);
   return true;
}

static const struct mwv207_shader_inout *
mwv207_stream_out_lookup(const struct mwv207_shader *shader,
                         int output_index)
{

   for (int i = 0; i < shader->outfile.num_reg; i++)
      if (shader->outfile.reg[i].driver_location == output_index)
         return &shader->outfile.reg[i];

   unreachable("stream out find error!");
   return NULL;
}

static int
mwv207_link_lookup(const struct mwv207_shader_link_info *info,
                   const struct mwv207_shader_inout *output)
{
   for (int i = 0; i < info->num_varyings_fixed; i++)
      if (info->varyings[i].reg == output->reg)
         return i;

   return -1;
}

static void
mwv207_fix_stream_out(struct mwv207_shader_link_info *info,
                      const struct mwv207_shader *shader)
{
   int index_offset = 0;

   if (shader->vs_pos_out_reg != -1)
      index_offset += 1;

   shader->info->stream_output = shader->info->base_stream_output;
   info->num_varyings_fixed = info->num_varyings;

   for (int idx = 0; idx < shader->info->stream_output.num_outputs; ++idx) {
      int output_index =  shader->info->base_stream_output.output[idx].register_index;
      if (output_index == 0 && shader->vs_pos_out_reg != -1) {
         shader->info->stream_output.output[idx].register_index = 0;
      } else if (output_index == 1 && shader->vs_pointsize_out_reg != -1) {
         shader->info->stream_output.output[idx].register_index = 63;
      } else {
         const struct mwv207_shader_inout *output = mwv207_stream_out_lookup(shader, output_index);
         int link_index  = mwv207_link_lookup(info, output);

         if (link_index != -1)
            shader->info->stream_output.output[idx].register_index = link_index + index_offset;
         else {
            struct mwv207_varying *varying = &info->varyings[info->num_varyings_fixed];
            varying->reg = output->reg;
            varying->num_components = output->num_components;
            varying->interpolation = output->interpolation;
            shader->info->stream_output.output[idx].register_index = info->num_varyings_fixed + index_offset;
            info->num_varyings_fixed += 1;
         }
      }
   }

   for (int idx = 0; idx < shader->info->stream_output.num_outputs; ++idx)
      if (shader->info->stream_output.output[idx].register_index == 63)
         shader->info->stream_output.output[idx].register_index = info->num_varyings_fixed + index_offset;
}

static struct mwv207_shader_inout *
mwv207_output_lookup(struct mwv207_shader *shader,
                     struct mwv207_shader_inout *input)
{
   for (int i = 0; i < shader->outfile.num_reg; i++) {
      if (input->is_array) {
         if (shader->outfile.reg[i].is_array &&
             shader->outfile.reg[i].slot == input->slot &&
             shader->outfile.reg[i].arry_index == input->arry_index)
            return &shader->outfile.reg[i];
      } else {
         if (shader->outfile.reg[i].slot == input->slot)
            return &shader->outfile.reg[i];
      }
   }

   return NULL;
}

static struct mwv207_shader *
mwv207_get_next_shader(struct mwv207_context *ctx, enum pipe_shader_type stage)
{
   enum pipe_shader_type next_stage = PIPE_SHADER_FRAGMENT;

   if (stage == PIPE_SHADER_TESS_EVAL) {
      if (ctx->shader_states[PIPE_SHADER_GEOMETRY])
         next_stage = PIPE_SHADER_GEOMETRY;
   } else if (stage == PIPE_SHADER_TESS_CTRL) {
      if (ctx->shader_states[PIPE_SHADER_TESS_EVAL])
         next_stage = PIPE_SHADER_TESS_EVAL;
      else if (ctx->shader_states[PIPE_SHADER_GEOMETRY])
         next_stage = PIPE_SHADER_GEOMETRY;
   } else if (stage == PIPE_SHADER_VERTEX) {
      if (ctx->shader_states[PIPE_SHADER_TESS_CTRL])
         next_stage = PIPE_SHADER_TESS_CTRL;
      else if (ctx->shader_states[PIPE_SHADER_TESS_EVAL])
         next_stage = PIPE_SHADER_TESS_EVAL;
      else if (ctx->shader_states[PIPE_SHADER_GEOMETRY])
         next_stage = PIPE_SHADER_GEOMETRY;
   }

   return ctx->shader_states[next_stage]->current;
}

bool
mwv207_link_shader(struct mwv207_context *ctx,
                   struct mwv207_shader *shader,
                   struct mwv207_shader_link_info *info)
{
   int link_offset = -1;
   int comp_ofs = 0;
   int varyings_offset = 0;
   struct mwv207_shader *next_shader = mwv207_get_next_shader(ctx, shader->stage);
   assert(next_shader->infile.num_reg < 0x10);
   info->num_varyings = 0;
   info->pcoord_varying_comp_ofs = -1;

   if (shader->stage == PIPE_SHADER_GEOMETRY && shader->gs_use_layer)
      link_offset = 0;

   for (int idx = 0; idx < next_shader->infile.num_reg; ++idx) {
      struct mwv207_shader_inout *input = &next_shader->infile.reg[idx];
      struct mwv207_shader_inout *output = mwv207_output_lookup(shader, input);
      struct mwv207_varying *varying;

      if (input->reg == 0 && input->slot == VARYING_SLOT_POS)
         continue;

      if (input->reg == 0 && output != NULL) {
         if (next_shader->stage != PIPE_SHADER_FRAGMENT)
            shader->vs_pos_out_reg = -1;
         link_offset = 0;
         info->num_varyings = 0;
         varyings_offset = 1;
      }

      if (input->slot == VARYING_SLOT_LAYER)
         continue;

      if (input->reg > info->num_varyings)
         info->num_varyings = input->reg;

      if (input->num_components == 0)
         input->num_components = output->num_components;

      varying = &info->varyings[input->reg + link_offset];

      varying->interpolation = input->interpolation;
      varying->num_components = input->num_components;

      if (input->slot == VARYING_SLOT_PNTC)
         info->pcoord_varying_comp_ofs = comp_ofs;
      else {
         if (output == NULL) {
            if (next_shader->stage == PIPE_SHADER_FRAGMENT && input->slot == VARYING_SLOT_PRIMITIVE_ID)
               continue;

            printf("Semantic value not found in vertex shader outputs, please check if this is a fake input\n");
         } else
            varying->reg = output->reg;
      }

      comp_ofs += varying->num_components;
   }

   if (next_shader->infile.num_reg == 0 && next_shader->stage != PIPE_SHADER_FRAGMENT)
      shader->vs_pos_out_reg = -1;

   info->num_varyings += varyings_offset;

   if (shader->stage == PIPE_SHADER_GEOMETRY && shader->gs_use_layer) {
      info->varyings[0].interpolation = 0x2;
      info->varyings[0].num_components = 1;
      info->num_varyings += 1;
   }

   mwv207_fix_stream_out(info, shader);

   if (next_shader->stage == PIPE_SHADER_FRAGMENT)
      next_shader->link = *info;

   return false;
}
