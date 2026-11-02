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
#include "pipe/p_state.h"
#include "util/u_inlines.h"

#include "mwv207_cmd.h"
#include "mwv207_format.h"
#include "mwv207_resource.h"
#include "mwv207_surface.h"
#include "mwv207_state.h"
#include "mwv207_context.h"

static const uint8_t compare_func[] = {
   0x0,
   0x1,
   0x2,
   0x3,
   0x4,
   0x5,
   0x6,
   0x7
};

static const uint8_t stencil_operation[] = {
   0x0,
   0x1,
   0x2,
   0x3,
   0x4,
   0x6,
   0x7,
   0x5,
};

static uint32_t
line_scale(float width, float granularity, const float min, const float max)
{
   assert(granularity >= 0);
   assert(min <= max);

   if (granularity > 0)
      width = roundf(width / granularity) * granularity;

   width = CLAMP(width, min, max);

   return fui(width / 2.0f);
}

static uint32_t
fill_mode(uint32_t fill_mode)
{
   switch (fill_mode) {
   case PIPE_POLYGON_MODE_FILL:
      return 0x2;
   case PIPE_POLYGON_MODE_LINE:
      return 0x1;
   case PIPE_POLYGON_MODE_POINT:
      return 0x0;
   case PIPE_POLYGON_MODE_FILL_RECTANGLE:
      return 0x2;
   default:
      unreachable("error fill mode");
      return 0x2;
   }
}

static void *
mwv207_create_rasterizer_state(struct pipe_context *pctx,
                               const struct pipe_rasterizer_state *state)
{
   struct mwv207_rasterizer_state *raster = CALLOC_STRUCT(mwv207_rasterizer_state);

   if (!raster)
      return NULL;

   raster->base = *state;

   raster->line_scale = line_scale(state->line_width, 0.125, 1.0f, 8.0f);

   assert(state->depth_clip_far == state->depth_clip_near);
   raster->depth_clamp = (state->depth_clip_near == 0);

   assert(state->fill_front <= PIPE_POLYGON_MODE_POINT);
   raster->fill_mode = fill_mode(raster->base.fill_front);

   switch (raster->base.cull_face) {
   case PIPE_FACE_NONE:
      raster->cull_mode = 0x0;
      break;
   case PIPE_FACE_FRONT:
      if (raster->base.front_ccw)
         raster->cull_mode = 0x2;
      else
         raster->cull_mode = 0x1;

      raster->fill_mode = fill_mode(raster->base.fill_back);
      break;
   case PIPE_FACE_BACK:
      if (raster->base.front_ccw)
         raster->cull_mode = 0x1;
      else
         raster->cull_mode = 0x2;
      break;
   case PIPE_FACE_FRONT_AND_BACK:
      break;
   default:
      assert(0);
   }

   if (raster->base.rasterizer_discard || raster->base.cull_face == PIPE_FACE_FRONT_AND_BACK)
      raster->tfb_control = 0x1;
   else
      raster->tfb_control = 0x0;

   raster->flat_mode = 0x0;

   raster->sprite_coord_enable = state->sprite_coord_enable;
   raster->sprite_coord_mode = state->sprite_coord_mode;

   raster->pa_config =
      ((ALIGN_BIT((uint32_t) (raster->base.point_size_per_vertex), 0x2) & (0x4)) | (~(0x4 | 0x8))) &
      ((ALIGN_BIT((uint32_t) (false), 0x6) & (0x40)) | (~(0x40 | 0x80))) &
      ((ALIGN_BIT((uint32_t) (raster->cull_mode), 0x8) & (0x300)) | (~(0x300 | 0x400))) &
      ((ALIGN_BIT((uint32_t) (raster->fill_mode), 0xC) & (0x3000)) | (~(0x3000 | 0x4000))) &
      ((ALIGN_BIT((uint32_t) (raster->flat_mode), 0x10) & (0x30000)) | (~(0x30000 | 0x40000))) &
      ((ALIGN_BIT((uint32_t) (true), 0x16) & (0x400000)) | (~(0x400000 | 0x800000))) &
      ((ALIGN_BIT((uint32_t) (false), 0x1D) & (0x20000000)) | (~(0x20000000 | 0x40000000))) &
      ((ALIGN_BIT((uint32_t) (0), 0x18) & (0xF000000)) | (~(0xF000000 | 0x10000000)));

   return raster;
}

static void
mwv207_bind_rasterizer_state(struct pipe_context *pctx, void *state)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   ctx->rast_state = state;
   ctx->dirty |= MWV207_DIRTY_RASTERIZER;
}

static void
mwv207_delete_rasterizer_state(struct pipe_context *pctx, void *state)
{
   FREE(state);
}

static void *
mwv207_create_depth_stencil_alpha_state(struct pipe_context *pctx,
                                        const struct pipe_depth_stencil_alpha_state *state)
{
   struct mwv207_depth_stencil_alpha_state *zsa = CALLOC_STRUCT(mwv207_depth_stencil_alpha_state);

   if (!zsa)
      return NULL;

   zsa->base = *state;

   zsa->stencil_test = state->stencil[0].enabled | state->stencil[1].enabled;

   zsa->front_not_keep =
      state->stencil[0].zpass_op != PIPE_STENCIL_OP_KEEP ||
      state->stencil[0].zfail_op != PIPE_STENCIL_OP_KEEP;

   zsa->back_not_keep  =
      state->stencil[1].zpass_op != PIPE_STENCIL_OP_KEEP ||
      state->stencil[1].zfail_op != PIPE_STENCIL_OP_KEEP;

   if (state->depth_bounds_test) {
      zsa->depth_bounds_test = true;
      zsa->min_depth_bounds = state->depth_bounds_min;
      zsa->max_depth_bounds = state->depth_bounds_max;
   }

   if ((zsa->base.depth_func == PIPE_FUNC_NOTEQUAL) ||
       (zsa->stencil_test && (zsa->front_not_keep || zsa->back_not_keep)))
      zsa->early_z = true;

   if (zsa->stencil_test || zsa->base.depth_enabled)
      zsa->mode = 0x1;
   else
      zsa->mode = 0x0;

   if (zsa->base.depth_enabled)
      zsa->func = compare_func[zsa->base.depth_func];
   else
      zsa->func = 0x7;

   zsa->depth_config =
      ((ALIGN_BIT(0x1, 0x18) & (0x1000000)) | (~(0x1000000 | 0x2000000))) &
      ((ALIGN_BIT(0x1, 0x12) & (0x40000)) | (~(0x40000 | 0x80000))) &
      ((ALIGN_BIT((uint32_t) (zsa->mode), 0x0) & (0x3)) | (~(0x3 | 0x8))) &
      ((ALIGN_BIT((uint32_t) (zsa->early_z), 0x10) & (0x10000)) | (~(0x10000 | 0x20000))) &
      ((ALIGN_BIT((uint32_t) (zsa->func), 0x8) & (0x700)) | (~(0x700 | 0x800))) &
      ((ALIGN_BIT((uint32_t) (zsa->base.depth_writemask), 0xC) & (0x1000)) | (~(0x1000 | 0x2000)));

   uint32_t stencil_mode = 0x0;
   struct pipe_stencil_state front = {0}, back = {0};

   if (zsa->stencil_test) {
      front = zsa->base.stencil[0];
      back  = zsa->base.stencil[1];

      if (front.enabled && back.enabled)
         stencil_mode = 0x2;
      else
         stencil_mode = 0x1;
   } else {
      front.func     = PIPE_FUNC_ALWAYS;
      front.fail_op  = PIPE_STENCIL_OP_KEEP;
      front.zpass_op = PIPE_STENCIL_OP_KEEP;
      front.zfail_op = PIPE_STENCIL_OP_KEEP;

      back.func     = PIPE_FUNC_ALWAYS;
      back.fail_op  = PIPE_STENCIL_OP_KEEP;
      back.zpass_op = PIPE_STENCIL_OP_KEEP;
      back.zfail_op = PIPE_STENCIL_OP_KEEP;
   }

   zsa->stencil_op[0] =
      (ALIGN_BIT((uint32_t) (compare_func[front.func]), 0x0) & (0x7)) |
      (ALIGN_BIT((uint32_t) (stencil_operation[front.zpass_op]), 0x4) & (0x70)) |
      (ALIGN_BIT((uint32_t) (stencil_operation[front.fail_op]), 0x8) & (0x700)) |
      (ALIGN_BIT((uint32_t) (stencil_operation[front.zfail_op]), 0xC) & (0x7000)) |
      (ALIGN_BIT((uint32_t) (compare_func[back.func]), 0x10) & (0x70000)) |
      (ALIGN_BIT((uint32_t) (stencil_operation[back.zpass_op]), 0x14) & (0x700000)) |
      (ALIGN_BIT((uint32_t) (stencil_operation[back.fail_op]), 0x18) & (0x7000000)) |
      (ALIGN_BIT((uint32_t) (stencil_operation[back.zfail_op]), 0x1C) & (0x70000000));

   zsa->stencil_config[0] =
      (ALIGN_BIT((uint32_t) (stencil_mode), 0x0) & (0x3)) |
      (ALIGN_BIT((uint32_t) (front.valuemask), 0x10) & (0xFF0000)) |
      (ALIGN_BIT((uint32_t) (front.writemask), 0x18) & (0xFF000000));

   zsa->stencil_back_config[0] =
      (ALIGN_BIT((uint32_t) (back.writemask), 0x8) & (0xFF00)) |
      (ALIGN_BIT((uint32_t) (back.valuemask), 0x0) & (0xFF));

   zsa->stencil_op[1] =
      (ALIGN_BIT((uint32_t) (compare_func[back.func]), 0x0) & (0x7)) |
      (ALIGN_BIT((uint32_t) (stencil_operation[back.zpass_op]), 0x4) & (0x70)) |
      (ALIGN_BIT((uint32_t) (stencil_operation[back.fail_op]), 0x8) & (0x700)) |
      (ALIGN_BIT((uint32_t) (stencil_operation[back.zfail_op]), 0xC) & (0x7000)) |
      (ALIGN_BIT((uint32_t) (compare_func[front.func]), 0x10) & (0x70000)) |
      (ALIGN_BIT((uint32_t) (stencil_operation[front.zpass_op]), 0x14) & (0x700000)) |
      (ALIGN_BIT((uint32_t) (stencil_operation[front.fail_op]), 0x18) & (0x7000000)) |
      (ALIGN_BIT((uint32_t) (stencil_operation[front.zfail_op]), 0x1C) & (0x70000000));

   zsa->stencil_config[1] =
      (ALIGN_BIT((uint32_t) (stencil_mode), 0x0) & (0x3)) |
      (ALIGN_BIT((uint32_t) (back.valuemask), 0x10) & (0xFF0000)) |
      (ALIGN_BIT((uint32_t) (back.writemask), 0x18) & (0xFF000000));

   zsa->stencil_back_config[1] =
      (ALIGN_BIT((uint32_t) (front.writemask), 0x8) & (0xFF00)) |
      (ALIGN_BIT((uint32_t) (front.valuemask), 0x0) & (0xFF));

   if (zsa->base.alpha_enabled) {
      zsa->alpha_config =
         (ALIGN_BIT((uint32_t) (zsa->base.alpha_enabled), 0x0) & (0x1)) |
         (ALIGN_BIT((uint32_t) (compare_func[zsa->base.alpha_func]), 0x4) & (0x70)) |
         (ALIGN_BIT((uint32_t) (zsa->base.alpha_ref_value * 0xFF), 0x8) & (0xFF00));
      zsa->alpha_reference =
         ((ALIGN_BIT((uint32_t) (zsa->base.alpha_ref_value * 0xFFFF), 0x10) & (0xFFFF0000)) | (~(0xFFFF0000 | 0x200)));
   } else {
      zsa->alpha_config =
         (ALIGN_BIT((uint32_t) (DISABLE), 0x0) & (0x1)) |
         (ALIGN_BIT((uint32_t) (0x7), 0x4) & (0x70)) |
         (ALIGN_BIT((uint32_t) (0), 0x8) & (0xFF00));
      zsa->alpha_reference =
         ((ALIGN_BIT((uint32_t) (0), 0x10) & (0xFFFF0000)) | (~(0xFFFF0000 | 0x200)));
   }

   return zsa;
}

static void
mwv207_delete_depth_stencil_alpha_state(struct pipe_context *pctx, void *state)
{
   FREE(state);
}

static void
mwv207_bind_depth_stencil_alpha_state(struct pipe_context *pctx, void *state)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   ctx->zsa = (struct mwv207_depth_stencil_alpha_state *)state;
   ctx->dirty |= MWV207_DIRTY_ZSA;
}

static void
mwv207_set_stencil_ref(struct pipe_context *pctx,
                       const struct pipe_stencil_ref ref)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   ctx->stencil_ref.ref = ref;

   ctx->stencil_ref.stencil_reference[0] =
      (ALIGN_BIT((uint32_t) (ref.ref_value[0]), 0x8) & (0xFF00));
   ctx->stencil_ref.stencil_reference[1] =
      (ALIGN_BIT((uint32_t) (ref.ref_value[1]), 0x8) & (0xFF00));

   ctx->stencil_ref.stencil_back_reference[0] =
      ((ALIGN_BIT((uint32_t) (ref.ref_value[1]), 0x0) & (0xFF)) | (~(0xFF | 0x100)));
   ctx->stencil_ref.stencil_back_reference[1] =
      ((ALIGN_BIT((uint32_t) (ref.ref_value[0]), 0x0) & (0xFF)) | (~(0xFF | 0x100)));

   ctx->dirty |= MWV207_DIRTY_STENCIL_REF;
}

static void
mwv207_set_viewport_states(struct pipe_context *pctx,
                           unsigned start_slot,
                           unsigned num_viewports,
                           const struct pipe_viewport_state *state)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   ctx->viewport = *state;
   ctx->dirty |= MWV207_DIRTY_VIEWPORT;
}

static void
mwv207_set_scissor_states(struct pipe_context *pctx,
                          unsigned start_slot, unsigned num_scissors,
                          const struct pipe_scissor_state *states)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   ctx->scissor = *states;
   ctx->dirty |= MWV207_DIRTY_SCISSOR;
}

static void
mwv207_set_polygon_stipple(struct pipe_context *pctx,
                           const struct pipe_poly_stipple *ps)
{

}

static void
mwv207_set_clip_state(struct pipe_context *pctx,
                      const struct pipe_clip_state *pcs)
{

}

static void
mwv207_set_stream_output_targets(struct pipe_context *pctx,
                                 unsigned num_targets,
                                 struct pipe_stream_output_target **targets,
                                 const unsigned *offsets)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   struct mwv207_stream_output_target *mwv207_target;

   for (unsigned i = 0; i < num_targets; i++) {
      pipe_so_target_reference(&ctx->stream_out.targets[i], targets[i]);
      ctx->stream_out.offsets[i] = offsets[i];
   }

   if (ctx->stream_out.num_targets > num_targets)
      for (unsigned i = num_targets; i < ctx->stream_out.num_targets; i++)
         pipe_so_target_reference(&ctx->stream_out.targets[i], NULL);

   if (num_targets > 0) {
      mwv207_target = mwv207_stream_output_target(targets[0]);

      if (!mwv207_target->bo) {
         mwv207_target->bo = ctx->ws->bo_create(ctx->ws, 4096,
            MWV207_DOMAIN_VRAM, MWV207_FLAG_CPU_ACCESS);
         mwv207_target->is_actived = false;
      }
   }

   ctx->stream_out.num_targets = num_targets;
   ctx->dirty |= MWV207_DIRTY_STREAM_OUT;
}

static struct pipe_stream_output_target *
mwv207_create_stream_output_target(struct pipe_context *ctx,
                                   struct pipe_resource *res,
                                   unsigned buffer_offset,
                                   unsigned buffer_size)
{
   struct mwv207_stream_output_target *mwv207_target = CALLOC_STRUCT(mwv207_stream_output_target);
   struct pipe_stream_output_target *t = (struct pipe_stream_output_target *)mwv207_target;

   if (!t)
      return NULL;

   pipe_reference_init(&t->reference, 1);
   pipe_resource_reference(&t->buffer, res);
   t->context = ctx;
   t->buffer_offset = buffer_offset;
   t->buffer_size = buffer_size;
   mwv207_target->bo = NULL;
   return t;
}

static void
mwv207_stream_output_target_destroy(struct pipe_context *pctx,
                                    struct pipe_stream_output_target *t)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   struct mwv207_stream_output_target *mwv207_target = mwv207_stream_output_target(t);

   if (mwv207_target->bo)
      ctx->ws->bo_reference(ctx->ws, &mwv207_target->bo, NULL);

   pipe_resource_reference(&t->buffer, NULL);
   FREE(t);
}

static void
mwv207_set_tess_state(struct pipe_context *pctx,
                      const float default_outer_level[4],
                      const float default_inner_level[2])
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   memcpy(&ctx->tess_outer_level[0], &default_outer_level[0], 4 * sizeof(float));
   memcpy(&ctx->tess_inner_level[0], &default_inner_level[0], 2 * sizeof(float));
}

static void
MWV207_set_patch_vertices(struct pipe_context *pctx, uint8_t patch_vertices)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   ctx->info.vertices_per_patch = patch_vertices;
}

static void
mwv207_set_sample_mask(struct pipe_context *pctx, unsigned sample_mask)
{
   struct mwv207_context *ctx = mwv207_context(pctx);

   ctx->sample_mask = sample_mask;
   ctx->dirty |= MWV207_DIRTY_SAMPLE_MASK;
}

void
mwv207_context_state_init(struct pipe_context *pctx)
{
   struct mwv207_context *ctx = mwv207_context(pctx);

   pctx->create_rasterizer_state = mwv207_create_rasterizer_state;
   pctx->bind_rasterizer_state   = mwv207_bind_rasterizer_state;
   pctx->delete_rasterizer_state = mwv207_delete_rasterizer_state;

   pctx->create_depth_stencil_alpha_state = mwv207_create_depth_stencil_alpha_state;
   pctx->bind_depth_stencil_alpha_state   = mwv207_bind_depth_stencil_alpha_state;
   pctx->delete_depth_stencil_alpha_state = mwv207_delete_depth_stencil_alpha_state;

   pctx->set_clip_state      = mwv207_set_clip_state;
   pctx->set_polygon_stipple = mwv207_set_polygon_stipple;
   pctx->set_viewport_states = mwv207_set_viewport_states;
   pctx->set_scissor_states  = mwv207_set_scissor_states;
   pctx->set_sample_mask = mwv207_set_sample_mask;

   ctx->stream_out.num_targets = 0;
   pctx->create_stream_output_target  = mwv207_create_stream_output_target;
   pctx->stream_output_target_destroy = mwv207_stream_output_target_destroy;
   pctx->set_stream_output_targets    = mwv207_set_stream_output_targets;

   pctx->set_stencil_ref = mwv207_set_stencil_ref;

   ctx->stencil_ref.stencil_reference[0] = 0;
   ctx->stencil_ref.stencil_reference[1] = 0;
   ctx->stencil_ref.stencil_back_reference[0] = 0;
   ctx->stencil_ref.stencil_back_reference[1] = 0;

   mwv207_context_vertex_init(pctx);
   mwv207_context_blend_init(pctx);

   pctx->set_tess_state = mwv207_set_tess_state;
   pctx->set_patch_vertices = MWV207_set_patch_vertices;
}
