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
#include "mwv207_lower.h"

static nir_def *
get_factor_def(struct mwv207_context *ctx, nir_builder *b, unsigned int factor,
               nir_def *blend_src, nir_def *blend_src1, nir_def *blend_dst)
{
   nir_def *factorDef = NULL;
   nir_def *const_zero = nir_imm_float(b, 0);
   nir_def *const_one = nir_imm_float(b, 1);
   nir_def *const_minus_one = nir_imm_float(b, -1);
   nir_def *const_minus = NULL;
   switch (factor) {
   case PIPE_BLENDFACTOR_ZERO:
      factorDef = const_zero;
      break;
   case PIPE_BLENDFACTOR_SRC_COLOR:
      factorDef = blend_src;
      break;
   case PIPE_BLENDFACTOR_SRC_ALPHA:
      factorDef = nir_channel(b, blend_src, 3);
      break;
   case PIPE_BLENDFACTOR_DST_COLOR:
      factorDef = blend_dst;
      break;
   case PIPE_BLENDFACTOR_DST_ALPHA:
      factorDef = nir_channel(b, blend_dst, 3);
      break;
   case PIPE_BLENDFACTOR_SRC_ALPHA_SATURATE:

      const_minus = nir_fadd(b, nir_fmul(b, nir_channels(b, blend_dst, 0x8), const_minus_one), const_one);
      factorDef = nir_fmin(b, nir_channels(b, blend_src, 0x8), const_minus);
      factorDef = nir_vec4(b, nir_channels(b, factorDef, 1), nir_channels(b, factorDef, 1),
                           nir_channels(b, factorDef, 1), const_one);
      break;
   case PIPE_BLENDFACTOR_CONST_COLOR:
      factorDef = nir_vec4(b, nir_imm_float(b, ctx->blend_color.color[0]),
                           nir_imm_float(b, ctx->blend_color.color[1]),
                           nir_imm_float(b, ctx->blend_color.color[2]),
                           nir_imm_float(b, ctx->blend_color.color[3]));
      break;
   case PIPE_BLENDFACTOR_CONST_ALPHA:
      factorDef = nir_imm_float(b, ctx->blend_color.color[3]);
      break;
   case PIPE_BLENDFACTOR_SRC1_COLOR:
      factorDef = nir_channels(b, blend_src1, 0xf );
      break;
   case PIPE_BLENDFACTOR_SRC1_ALPHA:
      factorDef = nir_channels(b, blend_src1, 0x8);
      break;
   case PIPE_BLENDFACTOR_ONE:
      factorDef = const_one;
      break;
   case PIPE_BLENDFACTOR_INV_SRC_COLOR:
      factorDef = nir_fadd(b, nir_fmul(b, nir_channels(b, blend_src, 0xf), const_minus_one), const_one);
      break;
   case PIPE_BLENDFACTOR_INV_SRC_ALPHA:
      factorDef = nir_fadd(b, nir_fmul(b, nir_channels(b, blend_src, 0x8), const_minus_one), const_one);
      break;
   case PIPE_BLENDFACTOR_INV_DST_ALPHA:
      factorDef = nir_fadd(b, nir_fmul(b, nir_channels(b, blend_dst, 0x8), const_minus_one), const_one);
      break;
   case PIPE_BLENDFACTOR_INV_DST_COLOR:
      factorDef = nir_fadd(b, nir_fmul(b, nir_channels(b, blend_dst, 0x7), const_minus_one), const_one);
      break;
   case PIPE_BLENDFACTOR_INV_CONST_COLOR:
      factorDef = nir_vec3(b, nir_imm_float(b, 1 - ctx->blend_color.color[0]),
                           nir_imm_float(b, 1 - ctx->blend_color.color[1]),
                           nir_imm_float(b, 1 - ctx->blend_color.color[2]));
      break;
   case PIPE_BLENDFACTOR_INV_CONST_ALPHA:
      factorDef = nir_imm_float(b, 1 - ctx->blend_color.color[3]);
      break;
   case PIPE_BLENDFACTOR_INV_SRC1_COLOR:
      factorDef = nir_fadd(b, nir_fmul(b, nir_channels(b, blend_src1, 0x7), const_minus_one), const_one);
      break;
   case PIPE_BLENDFACTOR_INV_SRC1_ALPHA:
      factorDef = nir_fadd(b, nir_fmul(b, nir_channels(b, blend_src1, 0x8), const_minus_one), const_one);
      break;
   }
   return factorDef;
}

static nir_def *
get_rt_texture_coord(struct mwv207_context *ctx, nir_builder *b)
{
   nir_def *coord = nir_load_frag_coord(b);
   nir_def *fragcoord_x = nir_channel(b, coord, 0);
   nir_def *fragcoord_y = nir_channel(b, coord, 1);
   nir_def *width = nir_imm_float(b, ctx->sampler_views[4][ctx->info.dsbi.sampler_index]->texture->width0);
   nir_def *height = nir_imm_float(b, ctx->sampler_views[4][ctx->info.dsbi.sampler_index]->texture->height0);
   nir_def *y_invert = nir_imm_float(b, ctx->info.dsbi.y_invert);
   nir_def *const_one = nir_imm_float(b, 1);
   nir_def *const_two = nir_imm_float(b, 2);
   nir_def *const_minus_one = nir_imm_float(b, -1);
   nir_def *tmp1 = nir_fmul(b, height, y_invert);
   nir_def *tmp2 = nir_fadd(b, const_one, nir_fmul(b, const_minus_one, nir_fmul(b, y_invert, const_two)));
   nir_def *tmp3 = nir_fmul(b, tmp2, fragcoord_y);
   nir_def *tmp4 = nir_fadd(b, tmp1, tmp3);
   nir_def *src0 = nir_vec2(b, fragcoord_x, tmp4);
   nir_def *src1 = nir_vec2(b, width, height);
   nir_def *tex_coord = nir_fdiv(b, src0, src1);
   return tex_coord;
}

static nir_tex_instr *
create_rt_texture(struct mwv207_context *ctx, nir_shader *nir, nir_def *tex_coord)
{
   nir_tex_instr *rt_tex = nir_tex_instr_create(nir, 1);
   rt_tex->src[0].src_type = nir_tex_src_coord;
   rt_tex->src[0].src = nir_src_for_ssa(tex_coord);
   rt_tex->op = nir_texop_txb;
   rt_tex->sampler_dim = GLSL_SAMPLER_DIM_2D;
   rt_tex->dest_type = nir_type_float;
   rt_tex->num_srcs = 1;
   rt_tex->component = 0;
   rt_tex->coord_components = 2;
   rt_tex->is_array = false;
   rt_tex->is_shadow = false;
   rt_tex->is_new_style_shadow = false;
   rt_tex->texture_index = ctx->info.dsbi.sampler_index;
   rt_tex->texture_non_uniform = false;
   rt_tex->sampler_index = ctx->info.dsbi.sampler_index;
   rt_tex->sampler_non_uniform = false;
   return rt_tex;
}

static nir_def *
handle_blend_func(unsigned int func, nir_builder *b, nir_def *src, nir_def *dst)
{
   nir_def *blend_dst = NULL;
   nir_def *const_minus_one = nir_imm_float(b, -1);
   switch (func) {
   case PIPE_BLEND_ADD:
      blend_dst = nir_fadd(b, src, dst);
      break;
   case PIPE_BLEND_SUBTRACT:
      blend_dst = nir_fadd(b, src, nir_fmul(b, dst, const_minus_one));
      break;
   case PIPE_BLEND_REVERSE_SUBTRACT:
      blend_dst = nir_fadd(b, dst, nir_fmul(b, src, const_minus_one));
      break;
   case PIPE_BLEND_MIN:
      blend_dst = nir_fmin(b, dst, src);
      break;
   case PIPE_BLEND_MAX:
      blend_dst = nir_fmax(b, dst, src);
      break;
   default:
      printf("rgb_func error!!\n");
      break;
   }
   return blend_dst;
}
static void
mwv207_lower_dual_blend_impl(nir_function_impl *impl, struct mwv207_context *ctx, struct mwv207_shader *shader)
{
   nir_shader *nir = impl->function->shader;
   nir_builder b = nir_builder_create(impl);
   uint32_t count = 0;
   nir_def *blend_src;
   nir_def *blend_src1;
   nir_def *blend_dst;
   nir_def *sfactor_rgb;
   nir_def *dfactor_rgb;
   nir_def *sfactor_alpha;
   nir_def *dfactor_alpha;
   nir_def *blend_src_rgb;
   nir_def *blend_dst_rgb;
   nir_def *blend_src_alpha;
   nir_def *blend_dst_alpha;
   struct pipe_rt_blend_state blend_state;
   nir_foreach_block(block, impl) {
      nir_foreach_instr_safe(instr, block) {
         if (instr->type != nir_instr_type_intrinsic)
            continue;
         nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
         if (intr->intrinsic != nir_intrinsic_store_deref)
            continue;
         count++;
         if (count == 1)
            blend_src = intr->src[1].ssa;
         if (count != 2)
            continue;

         if (count == 2)
            blend_src1 = intr->src[1].ssa;
         shader->useFragCoord = true;

         b.cursor = nir_after_instr(instr);
         nir_def *tex_coord = get_rt_texture_coord(ctx, &b);

         nir_tex_instr *rt_tex = create_rt_texture(ctx, nir, tex_coord);
         nir_def_init(&rt_tex->instr, &rt_tex->def, 4, 32);
         nir_builder_instr_insert(&b, &rt_tex->instr);
         blend_dst = &rt_tex->def;
         blend_state = ctx->info.dsbi.blend_state;
         sfactor_rgb = get_factor_def(ctx, &b, blend_state.rgb_src_factor, blend_src, blend_src1, blend_dst);
         dfactor_rgb = get_factor_def(ctx, &b, blend_state.rgb_dst_factor, blend_src, blend_src1, blend_dst);
         sfactor_alpha = get_factor_def(ctx, &b, blend_state.alpha_src_factor, blend_src, blend_src1, blend_dst);
         dfactor_alpha = get_factor_def(ctx, &b, blend_state.alpha_dst_factor, blend_src, blend_src1, blend_dst);

         nir_variable *var = nir_variable_create(nir, nir_var_shader_out,
                                                 glsl_vec4_type(), "final_out");
         var->data.location = FRAG_RESULT_DATA0;
         var->data.driver_location = 2;
         nir->num_outputs++;
         blend_src_rgb = nir_fmul(&b, blend_src, sfactor_rgb);
         blend_dst_rgb = nir_fmul(&b, blend_dst, dfactor_rgb);
         blend_src_alpha = nir_fmul(&b, blend_src, sfactor_alpha);
         blend_dst_alpha = nir_fmul(&b, blend_dst, dfactor_alpha);

         blend_dst_rgb = handle_blend_func(blend_state.rgb_func, &b, blend_src_rgb, blend_dst_rgb);
         blend_dst_alpha = handle_blend_func(blend_state.alpha_func, &b, blend_src_alpha, blend_dst_alpha);
         nir_alu_instr *mov = nir_alu_instr_create(nir, nir_op_vec4);

         mov->src[0].src = nir_src_for_ssa(nir_channel(&b, blend_dst_rgb, 0));
         mov->src[1].src = nir_src_for_ssa(nir_channel(&b, blend_dst_rgb, 1));
         mov->src[2].src = nir_src_for_ssa(nir_channel(&b, blend_dst_rgb, 2));
         mov->src[3].src = nir_src_for_ssa(nir_channel(&b, blend_dst_alpha, 3));
         nir_def_init(&mov->instr, &mov->def, 4, 32);
         nir_builder_instr_insert(&b, &mov->instr);
         nir_intrinsic_instr *store =
            nir_intrinsic_instr_create(nir, nir_intrinsic_store_deref);
         store->num_components = 4;
         store->src[0] = nir_src_for_ssa(&nir_build_deref_var(&b, var)->def);
         store->src[1] = nir_src_for_ssa(&mov->def);
         nir_intrinsic_set_write_mask(store, 0xf);
         nir_builder_instr_insert(&b, &store->instr);
         break;
      }
   }
}

void
mwv207_lower_dual_blend(nir_shader *nir, struct mwv207_context *ctx, struct mwv207_shader *shader)
{
   if (ctx->info.dsbi.is_dual) {
      nir_foreach_function(function, nir) {
         if (function->impl)
            mwv207_lower_dual_blend_impl(function->impl, ctx, shader);
      }
   }
}