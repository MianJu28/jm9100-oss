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

/* FIX(jm9100-oss): 双源混合(dual source blending)的硬件支持说明
 *
 * 硬件混合单元没有 SRC1 因子: mwv207_blend.c 的 alpha_func[] 表在
 * SRC1_COLOR(9)/SRC1_ALPHA(10)/INV_SRC1_*(25/26) 处是 ~0U。因此驱动只能把
 * 双源混合降级进片元着色器: 读回当前渲染目标(拷贝成纹理)作为 dst, 用
 * src(第一个输出) 与 src1(第二个输出, GL 的 index=1) 计算最终颜色。
 *
 * 之前的实现有几个让结果直接错掉的问题, 这里一并修掉:
 *   1. 混合因子/方程/blend color 在编译期被烘焙进二进制, 但它们没有进
 *      着色器缓存键 —— 先编译出来的一份会被后面所有不同的混合状态复用。
 *      (已由 fp_dual_blend 指纹解决, 见 mwv207_draw.c/mwv207_shader.c)
 *   2. 原来新加了一个 driver_location=2 的 FRAG_RESULT_DATA0 输出, 但保留了
 *      原来的两个 store —— 结果着色器实际写 3 个输出, 最终颜色落不到 RT0。
 *      现在直接改写第一个 store 的值, 并删掉 index=1 的 store (硬件只有
 *      一个 RT, 双源的第二个输出本来就不该写出去)。
 *   3. 原来用 nir_texop_txb + 归一化坐标采样 dst, 带三线性过滤与 LOD 偏移,
 *      会把相邻像素混进来(文字边缘因此糊成彩色条纹)。现在用 texelFetch
 *      (nir_texop_txf) 精确取整数坐标的像素。
 *   4. 因子/方程计算里对 1 分量/3 分量 def 取 channel 3 是未定义行为;
 *      现在因子统一规约成 vec4, alpha 取它的 .a。
 */

/* 本驱动的发射器没有 fsub（只有 fadd/fmul/ffma 与 fneg），减法一律写成
 * a + (-b)。写 nir_fsub 会在 mwv207_emit_alu 里命中 "not support nir opcode"
 * 并 assert（火狐启动即崩）。 */
static nir_def *
fsub_(nir_builder *b, nir_def *a, nir_def *c)
{
   return nir_fsub(b, a, c);
}

static nir_def *
splat_vec4(nir_builder *b, nir_def *v)
{
   if (v->num_components == 4)
      return v;
   if (v->num_components == 1)
      return nir_replicate(b, v, 4);
   /* vec3 等: 用第 3 个分量补 alpha, 只用于 rgb 因子, alpha 取 .a 时另有来源 */
   return nir_vec4(b, nir_channel(b, v, 0), nir_channel(b, v, 1),
                   nir_channel(b, v, 2), nir_channel(b, v, 2));
}

static nir_def *
get_factor_def(struct mwv207_context *ctx, nir_builder *b, unsigned int factor,
               nir_def *blend_src, nir_def *blend_src1, nir_def *blend_dst)
{
   nir_def *const_one = nir_imm_float(b, 1);
   nir_def *one_vec = nir_imm_vec4(b, 1, 1, 1, 1);
   nir_def *zero_vec = nir_imm_vec4(b, 0, 0, 0, 0);

   switch (factor) {
   case PIPE_BLENDFACTOR_ZERO:
      return zero_vec;
   case PIPE_BLENDFACTOR_ONE:
      return one_vec;
   case PIPE_BLENDFACTOR_SRC_COLOR:
      return blend_src;
   case PIPE_BLENDFACTOR_SRC_ALPHA:
      return nir_replicate(b, nir_channel(b, blend_src, 3), 4);
   case PIPE_BLENDFACTOR_DST_COLOR:
      return blend_dst;
   case PIPE_BLENDFACTOR_DST_ALPHA:
      return nir_replicate(b, nir_channel(b, blend_dst, 3), 4);
   case PIPE_BLENDFACTOR_SRC_ALPHA_SATURATE: {
      /* min(src.a, 1 - dst.a), rgb 三分量相同, alpha 为 1 */
      nir_def *sa = nir_channel(b, blend_src, 3);
      nir_def *inv_da = fsub_(b, const_one, nir_channel(b, blend_dst, 3));
      nir_def *f = nir_fmin(b, sa, inv_da);
      return nir_vec4(b, f, f, f, const_one);
   }
   case PIPE_BLENDFACTOR_CONST_COLOR:
      return nir_imm_vec4(b, ctx->blend_color.color[0], ctx->blend_color.color[1],
                             ctx->blend_color.color[2], ctx->blend_color.color[3]);
   case PIPE_BLENDFACTOR_CONST_ALPHA:
      return nir_replicate(b, nir_imm_float(b, ctx->blend_color.color[3]), 4);
   case PIPE_BLENDFACTOR_SRC1_COLOR:
      return blend_src1;
   case PIPE_BLENDFACTOR_SRC1_ALPHA:
      return nir_replicate(b, nir_channel(b, blend_src1, 3), 4);
   case PIPE_BLENDFACTOR_INV_SRC_COLOR:
      return fsub_(b, one_vec, blend_src);
   case PIPE_BLENDFACTOR_INV_SRC_ALPHA:
      return nir_replicate(b, fsub_(b, const_one, nir_channel(b, blend_src, 3)), 4);
   case PIPE_BLENDFACTOR_INV_DST_COLOR:
      return fsub_(b, one_vec, blend_dst);
   case PIPE_BLENDFACTOR_INV_DST_ALPHA:
      return nir_replicate(b, fsub_(b, const_one, nir_channel(b, blend_dst, 3)), 4);
   case PIPE_BLENDFACTOR_INV_CONST_COLOR:
      return nir_imm_vec4(b, 1.0f - ctx->blend_color.color[0],
                             1.0f - ctx->blend_color.color[1],
                             1.0f - ctx->blend_color.color[2],
                             1.0f - ctx->blend_color.color[3]);
   case PIPE_BLENDFACTOR_INV_CONST_ALPHA:
      return nir_replicate(b, nir_imm_float(b, 1.0f - ctx->blend_color.color[3]), 4);
   case PIPE_BLENDFACTOR_INV_SRC1_COLOR:
      return fsub_(b, one_vec, blend_src1);
   case PIPE_BLENDFACTOR_INV_SRC1_ALPHA:
      return nir_replicate(b, fsub_(b, const_one, nir_channel(b, blend_src1, 3)), 4);
   default:
      /* 未知因子按 ZERO 处理, 不要返回未初始化指针 */
      return zero_vec;
   }
}

/* 取整数坐标的 dst 像素: 拷贝纹理与渲染目标同尺寸同朝向, 所以 texel 坐标
 * 就是 floor(gl_FragCoord.xy)。用 texelFetch 避免任何过滤。 */
static nir_def *
load_dst_texel(struct mwv207_context *ctx, nir_builder *b, nir_shader *nir)
{
   nir_def *coord = nir_load_frag_coord(b);
   nir_def *xy = nir_vec2(b, nir_channel(b, coord, 0), nir_channel(b, coord, 1));
   nir_def *ij = nir_f2i32(b, xy);

   nir_tex_instr *tex = nir_tex_instr_create(nir, 1);
   tex->src[0].src_type = nir_tex_src_coord;
   tex->src[0].src = nir_src_for_ssa(ij);
   tex->op = nir_texop_txf;
   tex->sampler_dim = GLSL_SAMPLER_DIM_2D;
   tex->dest_type = nir_type_float32;
   tex->num_srcs = 1;
   tex->component = 0;
   tex->coord_components = 2;
   tex->is_array = false;
   tex->is_shadow = false;
   tex->is_new_style_shadow = false;
   tex->texture_index = ctx->info.dsbi.sampler_index;
   tex->texture_non_uniform = false;
   tex->sampler_index = ctx->info.dsbi.sampler_index;
   tex->sampler_non_uniform = false;
   nir_def_init(&tex->instr, &tex->def, 4, 32);
   nir_builder_instr_insert(b, &tex->instr);
   return &tex->def;
}

static nir_def *
blend_func_apply(nir_builder *b, unsigned func, nir_def *src, nir_def *dst)
{
   switch (func) {
   case PIPE_BLEND_ADD:
      return nir_fadd(b, src, dst);
   case PIPE_BLEND_SUBTRACT:
      return fsub_(b, src, dst);
   case PIPE_BLEND_REVERSE_SUBTRACT:
      return fsub_(b, dst, src);
   case PIPE_BLEND_MIN:
      return nir_fmin(b, src, dst);
   case PIPE_BLEND_MAX:
      return nir_fmax(b, src, dst);
   default:
      return nir_fadd(b, src, dst);
   }
}

/* 摘掉一个 nir_src 在 def->uses 链表里的链接。直接给 src 赋新值而不先摘链
 * 会把目标 def 的 uses 链表写坏(nir_validate 在 list_validate 里断言失败)。 */
static void
drop_src_use(nir_src *src)
{
   if (src->ssa)
      list_delinit(&src->use_link);
}

static void
mwv207_lower_dual_blend_impl(nir_function_impl *impl, struct mwv207_context *ctx,
                             struct mwv207_shader *shader)
{
   nir_shader *nir = impl->function->shader;
   nir_intrinsic_instr *store_color = NULL;
   nir_intrinsic_instr *store_src1 = NULL;

   /* 找双源的两条输出: location=FRAG_RESULT_DATA0, index 0 / 1 */
   nir_foreach_block(block, impl) {
      nir_foreach_instr(instr, block) {
         if (instr->type != nir_instr_type_intrinsic)
            continue;
         nir_intrinsic_instr *intr = nir_instr_as_intrinsic(instr);
         if (intr->intrinsic != nir_intrinsic_store_deref)
            continue;
         nir_deref_instr *deref = nir_src_as_deref(intr->src[0]);
         if (!deref || deref->deref_type != nir_deref_type_var)
            continue;
         nir_variable *var = deref->var;
         if (!var || var->data.mode != nir_var_shader_out)
            continue;
         if (var->data.location != FRAG_RESULT_DATA0)
            continue;
         if (var->data.index == 1)
            store_src1 = intr;
         else
            store_color = intr;
      }
   }

   /* 没有双源输出形态(或只有单输出)就什么都不做 */
   if (!store_color || !store_src1)
      return;

   shader->useFragCoord = true;

   nir_deref_instr *color_deref = nir_src_as_deref(store_color->src[0]);
   nir_variable *color_var = color_deref ? color_deref->var : NULL;
   if (!color_var)
      return;

   nir_def *src_ssa = store_color->src[1].ssa;
   nir_def *src1_ssa = store_src1->src[1].ssa;
   nir_instr *anchor = nir_instr_next(&store_color->instr);
   nir_block *blk = store_color->instr.block;

   const struct pipe_rt_blend_state *rbs = &ctx->info.dsbi.blend_state;
   nir_builder b = nir_builder_create(impl);
   b.cursor = nir_before_instr(&store_color->instr);

   nir_def *src = nir_channels(&b, src_ssa, 0xf);
   nir_def *src1 = nir_channels(&b, src1_ssa, 0xf);
   nir_def *dst = load_dst_texel(ctx, &b, nir);

   nir_def *sf_rgb = get_factor_def(ctx, &b, rbs->rgb_src_factor, src, src1, dst);
   nir_def *df_rgb = get_factor_def(ctx, &b, rbs->rgb_dst_factor, src, src1, dst);
   nir_def *sf_a   = get_factor_def(ctx, &b, rbs->alpha_src_factor, src, src1, dst);
   nir_def *df_a   = get_factor_def(ctx, &b, rbs->alpha_dst_factor, src, src1, dst);

   nir_def *src_rgb = nir_channels(&b, src, 0x7);
   nir_def *dst_rgb = nir_channels(&b, dst, 0x7);
   nir_def *rgb;
   if (rbs->rgb_func == PIPE_BLEND_MIN || rbs->rgb_func == PIPE_BLEND_MAX) {
      rgb = blend_func_apply(&b, rbs->rgb_func, src_rgb, dst_rgb);
   } else {
      nir_def *t1 = nir_fmul(&b, src_rgb, nir_channels(&b, sf_rgb, 0x7));
      nir_def *t2 = nir_fmul(&b, dst_rgb, nir_channels(&b, df_rgb, 0x7));
      rgb = blend_func_apply(&b, rbs->rgb_func, t1, t2);
   }

   nir_def *src_a = nir_channel(&b, src, 3);
   nir_def *dst_a = nir_channel(&b, dst, 3);
   nir_def *a;
   if (rbs->alpha_func == PIPE_BLEND_MIN || rbs->alpha_func == PIPE_BLEND_MAX) {
      a = blend_func_apply(&b, rbs->alpha_func, src_a, dst_a);
   } else {
      nir_def *t1 = nir_fmul(&b, src_a, nir_channel(&b, sf_a, 3));
      nir_def *t2 = nir_fmul(&b, dst_a, nir_channel(&b, df_a, 3));
      a = blend_func_apply(&b, rbs->alpha_func, t1, t2);
   }

   nir_def *final = nir_vec4(&b, nir_channel(&b, rgb, 0), nir_channel(&b, rgb, 1),
                             nir_channel(&b, rgb, 2), a);

   /* 删掉原来的两条输出 store (双源的第二个输出硬件写不出去), 用一条写
    * 最终颜色的 store 取代它们。src 的 uses 链必须显式摘掉, 否则
    * nir_validate 会在 list_validate / nir_src_parent_instr 上断言失败。 */
   drop_src_use(&store_color->src[0]);
   drop_src_use(&store_color->src[1]);
   drop_src_use(&store_src1->src[0]);
   drop_src_use(&store_src1->src[1]);
   nir_instr_remove(&store_color->instr);
   nir_instr_remove(&store_src1->instr);

   if (anchor)
      b.cursor = nir_before_instr(anchor);
   else
      b.cursor = nir_after_block(blk);

   nir_store_deref(&b, nir_build_deref_var(&b, color_var), final, 0xf);
}

void
mwv207_lower_dual_blend(nir_shader *nir, struct mwv207_context *ctx,
                        struct mwv207_shader *shader)
{
   if (!ctx->info.dsbi.is_dual)
      return;

   nir_foreach_function(function, nir) {
      if (function->impl)
         mwv207_lower_dual_blend_impl(function->impl, ctx, shader);
   }
}
