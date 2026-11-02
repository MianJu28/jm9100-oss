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
#include "util/u_memory.h"
#include "util/u_dual_blend.h"
#include "mwv207_context.h"

static const uint32_t alpha_func[] = {
   0x0,
   0x1,
   0x2,
   0x4,
   0x6,
   0x8,
   0xA,
   0xD,
   0xB,
   ~0U,
   ~0U,
   ~0U,
   ~0U,
   ~0U,
   ~0U,
   ~0U,
   ~0U,
   0x0,
   0x3,
   0x5,
   0x7,
   0x9,
   ~0U,
   0xE,
   0xC,
   ~0U,
   ~0U,
};

static const uint32_t alpha_mode[] = {
   0x0,
   0x1,
   0x2,
   0x3,
   0x4,
};

static const uint32_t alpha_mode_ext[] = {
   0x0,
   0x0,
   0x0,
   0x0,
   0x0,
   0x1,
   0x2,
   0x3,
   0x4,
   0x5,
   ~0U,
   ~0U,
   0x6,
   ~0U,
   0x7,
   0x8,
   ~0U,
   ~0U,
   ~0U,
   ~0U,
};

static const bool alpha_opt[] = {
   0x0,
   true,
   true,
   true,
   false,
   false,
   true,
   true,
   true,
   0x0,
   0x0,
   true,
   true,
   true,
   false,
   false,
   0x0,
   true,
   true,
   0x0,
   0x0,
};

static uint8_t logic_opt[] = {
   0x0,
   0x1,
   0x2,
   0x3,
   0x4,
   0x5,
   0x6,
   0x7,
   0x8,
   0x9,
   0xA,
   0xB,
   0xC,
   0xD,
   0xE,
   0xF,
};

static void *
mwv207_create_blend_state(struct pipe_context *pctx,
                          const struct pipe_blend_state *state)
{
   struct mwv207_blend_state *blend_state = CALLOC_STRUCT(mwv207_blend_state);
   struct pipe_rt_blend_state *rt;
   bool dst_read = false;

   if (!blend_state)
      return NULL;

   blend_state->blend_state = *state;

   /* GL_KHR_blend_equation_advanced.
    *
    * Since Mesa 23 the advanced equation is delivered through
    * pipe_blend_state::advanced_blend_func (enum gl_advanced_blend_mode) and
    * the state tracker leaves rt[i].blend_enable / rgb_func at their
    * defaults for that path, while enum pipe_blend_func only carries
    * ADD/SUB/REVSUB/MIN/MAX.  The historical alpha_mode_ext[rt->rgb_func]
    * lookup below can therefore never reach its advanced entries any more,
    * so translate advanced_blend_func explicitly and turn blending on for
    * RT0 (the spec only allows advanced equations with a single draw buffer).
    * Only the equations the hardware encodes in alpha_mode_ext[] are
    * accepted; anything else keeps the previous no-blend behaviour. */
   {
      static const uint32_t adv_ext[BLEND_HSL_LUMINOSITY + 1] = {
         [BLEND_MULTIPLY]   = 0x1,
         [BLEND_SCREEN]     = 0x2,
         [BLEND_OVERLAY]    = 0x3,
         [BLEND_DARKEN]     = 0x4,
         [BLEND_LIGHTEN]    = 0x5,
         [BLEND_HARDLIGHT]  = 0x6,
         [BLEND_DIFFERENCE] = 0x7,
         [BLEND_EXCLUSION]  = 0x8,
         /* The hardware blend unit implements exactly these eight extended
          * modes.  Probed empirically: feeding codes 0x9..0xF for
          * COLORDODGE/COLORBURN/SOFTLIGHT/HSL_* zeroes the framebuffer
          * instead of blending, so those seven equations are a genuine
          * hardware gap and deliberately stay unmapped (blending is left
          * disabled for them, matching the vendor table). */
      };
      unsigned adv = blend_state->blend_state.advanced_blend_func;

      if (adv <= BLEND_HSL_LUMINOSITY && adv_ext[adv]) {
         struct pipe_rt_blend_state *r0 = &blend_state->blend_state.rt[0];

         blend_state->advanced_ext = adv_ext[adv];
         r0->blend_enable = 1;
         r0->rgb_src_factor = r0->alpha_src_factor = PIPE_BLENDFACTOR_ONE;
         r0->rgb_dst_factor = r0->alpha_dst_factor = PIPE_BLENDFACTOR_ONE;
         r0->colormask = 0xf;
         blend_state->blend_state.independent_blend_enable = 1;
      }
   }

   if (blend_state->blend_state.logicop_enable)
      blend_state->logic_config = ((ALIGN_BIT((uint32_t) (logic_opt[blend_state->blend_state.logicop_func]), 0x0) & (0xF)) | (~(0xF | 0x10)));
   else
      blend_state->logic_config = ((ALIGN_BIT((uint32_t) (logic_opt[PIPE_LOGICOP_COPY]), 0x0) & (0xF)) | (~(0xF | 0x10)));

   if (blend_state->blend_state.logicop_enable &&
       blend_state->blend_state.logicop_func != PIPE_LOGICOP_COPY)
      dst_read = true;

   for (int i = 0; i < PIPE_MAX_COLOR_BUFS; ++i) {
      uint32_t alphaConfig = 0, alphaControl = 0;

      if (blend_state->blend_state.independent_blend_enable)
         rt = &blend_state->blend_state.rt[i];
      else
         rt = &blend_state->blend_state.rt[0];

      if (rt->blend_enable)
         dst_read = true;

      bool blend_enable = rt->blend_enable &&
         !(rt->rgb_src_factor == PIPE_BLENDFACTOR_ONE &&
         rt->rgb_dst_factor == PIPE_BLENDFACTOR_ZERO &&
         rt->rgb_func == PIPE_BLEND_ADD &&
         rt->alpha_src_factor == PIPE_BLENDFACTOR_ONE &&
         rt->alpha_dst_factor == PIPE_BLENDFACTOR_ZERO &&
         rt->alpha_func == PIPE_BLEND_ADD);
      bool separate_alpha = blend_enable &&
         !(rt->rgb_src_factor == rt->alpha_src_factor &&
         rt->rgb_dst_factor == rt->alpha_dst_factor &&
         rt->rgb_func == rt->alpha_func);

      if (blend_enable) {
         bool modeAdd, modeSub;

         modeAdd = (rt->rgb_func == PIPE_BLEND_ADD) &&
                   (rt->alpha_func == PIPE_BLEND_ADD);
         modeSub = (rt->rgb_func == PIPE_BLEND_SUBTRACT) &&
                   (rt->alpha_func == PIPE_BLEND_SUBTRACT);

         if ((modeAdd || modeSub) && (rt->colormask == 0xf)) {
            enum pipe_blendfactor  src_color_func = rt->rgb_src_factor;
            enum pipe_blendfactor  src_alpha_func = rt->alpha_src_factor;
            enum pipe_blendfactor  dst_color_func = rt->rgb_dst_factor;
            enum pipe_blendfactor  dst_alpha_func = rt->alpha_dst_factor;

            bool no_read_dst = alpha_opt[src_alpha_func] && alpha_opt[src_color_func] &&
               alpha_opt[dst_alpha_func] && alpha_opt[dst_color_func];

            bool src_alpha_one_noread = no_read_dst &&
               (dst_alpha_func == PIPE_BLENDFACTOR_INV_SRC_ALPHA) &&
               (dst_color_func == PIPE_BLENDFACTOR_INV_SRC_ALPHA);

            bool src_alpha_zero_noread = no_read_dst &&
               (dst_alpha_func == PIPE_BLENDFACTOR_SRC_ALPHA ) &&
               (dst_color_func == PIPE_BLENDFACTOR_SRC_ALPHA );

            bool src_all_one_noread = no_read_dst &&
               (dst_alpha_func == PIPE_BLENDFACTOR_INV_SRC_ALPHA ||
               dst_alpha_func == PIPE_BLENDFACTOR_INV_SRC_COLOR) &&
               (dst_color_func == PIPE_BLENDFACTOR_INV_SRC_ALPHA ||
               dst_color_func == PIPE_BLENDFACTOR_INV_SRC_COLOR);

            bool src_all_zero_noread = no_read_dst &&
               (dst_alpha_func == PIPE_BLENDFACTOR_SRC_ALPHA ||
               dst_alpha_func == PIPE_BLENDFACTOR_SRC_COLOR) &&
               (dst_color_func == PIPE_BLENDFACTOR_SRC_ALPHA ||
               dst_color_func == PIPE_BLENDFACTOR_SRC_COLOR);

            if (src_alpha_one_noread) {
               alphaConfig |= (ALIGN_BIT((uint32_t) (ENABLE), 0x5) & (0x20));
               alphaControl |= (ALIGN_BIT((uint32_t) (ENABLE), 0x11) & (0x20000));
            }

            if (src_alpha_zero_noread) {
               alphaConfig |= (ALIGN_BIT((uint32_t) (ENABLE), 0xB) & (0x800));
               alphaControl |= (ALIGN_BIT((uint32_t) (ENABLE), 0x2) & (0x4));
            }

            if (src_all_one_noread) {
               alphaConfig |= (ALIGN_BIT((uint32_t) (ENABLE), 0x6) & (0x40));
               alphaControl |= (ALIGN_BIT((uint32_t) (ENABLE), 0x12) & (0x40000));
            }

            if (src_all_zero_noread) {
               alphaConfig |= (ALIGN_BIT((uint32_t) (ENABLE), 0xC) & (0x1000));
               alphaControl |= (ALIGN_BIT((uint32_t) (ENABLE), 0x3) & (0x8));
            }
         }
      }

      if (i == 0) {
         blend_state->alpha_config = alphaConfig |
            (ALIGN_BIT((uint32_t) (blend_state->advanced_ext ?
                                   blend_state->advanced_ext :
                                   alpha_mode_ext[rt->rgb_func]), 0x0) & (0xF)) |
            (ALIGN_BIT(0x0, 0x4) & (0x10));

         blend_state->alpha_blend = (ALIGN_BIT((uint32_t) (blend_enable), 0x0) & (0x1)) |
            (ALIGN_BIT((uint32_t) (separate_alpha), 0x10) & (0x10000)) |
            (ALIGN_BIT((uint32_t) (alpha_func[rt->rgb_src_factor]), 0x4) & (0xF0)) |
            (ALIGN_BIT((uint32_t) (alpha_func[rt->alpha_src_factor]), 0x14) & (0xF00000)) |
            (ALIGN_BIT((uint32_t) (alpha_func[rt->rgb_dst_factor]), 0x8) & (0xF00)) |
            (ALIGN_BIT((uint32_t) (alpha_func[rt->alpha_dst_factor]), 0x18) & (0xF000000)) |
            (ALIGN_BIT((uint32_t) (alpha_mode[rt->rgb_func]), 0xC) & (0x7000)) |
            (ALIGN_BIT((uint32_t) (alpha_mode[rt->alpha_func]), 0x1C) & (0x70000000));
      }

      blend_state->blend_config[i] = alphaControl |
         (ALIGN_BIT((uint32_t) (blend_enable), 0x0) & (0x1)) |
         (ALIGN_BIT((uint32_t) (alpha_func[rt->rgb_src_factor]), 0x4) & (0xF0)) |
         (ALIGN_BIT((uint32_t) (alpha_func[rt->alpha_src_factor]), 0x14) & (0xF00000)) |
         (ALIGN_BIT((uint32_t) (alpha_func[rt->rgb_dst_factor]), 0x8) & (0xF00)) |
         (ALIGN_BIT((uint32_t) (alpha_func[rt->alpha_dst_factor]), 0x18) & (0xF000000)) |
         (ALIGN_BIT((uint32_t) (alpha_mode[rt->rgb_func]), 0xC) & (0x7000)) |
         (ALIGN_BIT((uint32_t) (alpha_mode[rt->alpha_func]), 0x1C) & (0x70000000));

      blend_state->blend_configex[i] = (ALIGN_BIT((uint32_t) (separate_alpha), 0x1D) & (0x20000000));

      blend_state->color_mask[i] = rt->colormask;

      if (blend_state->color_mask[i] != 0xf)
         dst_read = true;

      blend_state->dest_read[i] = dst_read;
   }

   return blend_state;
}

static void
mwv207_delete_blend_state(struct pipe_context *pctx, void *blend_state)
{
   FREE(blend_state);
}

static void
mwv207_bind_blend_state(struct pipe_context *pctx, void *state)
{
   struct mwv207_context *ctx = mwv207_context(pctx);

   ctx->blend_state = (struct mwv207_blend_state *)state;
   ctx->info.dsbi.is_dual = false;
   if (ctx->blend_state) {
      ctx->info.dsbi.is_dual = util_blend_state_is_dual(&ctx->blend_state->blend_state, 0);
      ctx->info.dsbi.blend_state = ctx->blend_state->blend_state.rt[0];
   }

   if (ctx->info.dsbi.is_dual) {
      for (int i = 0; i < PIPE_MAX_COLOR_BUFS; ++i) {
         if (i == 0)
            ctx->blend_state->alpha_blend = (ALIGN_BIT((uint32_t) (!ctx->info.dsbi.is_dual), 0x0) & (0x1));

         ctx->blend_state->blend_config[i] = 0 |
            (ALIGN_BIT((uint32_t) (!ctx->info.dsbi.is_dual), 0x0) & (0x1));
      }
   }
   ctx->dirty |= MWV207_DIRTY_BLEND;
}

static void
mwv207_set_blend_color(struct pipe_context *pctx,
                       const struct pipe_blend_color *color)
{
   struct mwv207_context *ctx = mwv207_context(pctx);

   ctx->blend_color = *color;

   ctx->dirty |= MWV207_DIRTY_BLEND_COLOR;
}

void
mwv207_context_blend_init(struct pipe_context *pctx)
{
   pctx->create_blend_state = mwv207_create_blend_state;
   pctx->bind_blend_state   = mwv207_bind_blend_state;
   pctx->delete_blend_state = mwv207_delete_blend_state;
   pctx->set_blend_color    = mwv207_set_blend_color;
}
