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
#include "util/u_inlines.h"
#include "util/u_memory.h"
#include "util/u_framebuffer.h"
#include "util/format/u_format.h"

#include "mwv207_cmd.h"
#include "mwv207_format.h"
#include "mwv207_context.h"

void
mwv207_update_renderInfo(struct mwv207_surface *surface)
{
   uint32_t swizzle_set = 0;

   swizzle_set =
      (ALIGN_BIT((uint32_t) (SWIZZLE_RED  ), 0xC) & (0x3000)) |
      (ALIGN_BIT((uint32_t) (SWIZZLE_GREEN), 0xE) & (0xC000)) |
      (ALIGN_BIT((uint32_t) (SWIZZLE_BLUE ), 0x10) & (0x30000)) |
      (ALIGN_BIT((uint32_t) (SWIZZLE_ALPHA), 0x12) & (0xC0000));

   surface->peInfo.pe_config &= 0xFFF007FF;
   surface->peInfo.pe_config |= swizzle_set;
}

static void
mwv207_get_renderInfo(struct mwv207_surface *surface)
{
   struct mwv207_resource *res = mwv207_resource(surface->base.texture);
   bool super_tile = false;
   bool msaa_compressed = DISABLE;
   uint32_t tile_config = 0x0;
   uint32_t swizzle_set = 0;

   if (surface->pixel_128bit) {
      res = mwv207_resource(res->rg);
      mwv207_get_pixel_desc(res->base.format, &surface->peInfo.peDesc);
   } else
      mwv207_get_pixel_desc(surface->base.format, &surface->peInfo.peDesc);

   if (res->mode == MWV207_SUPERTILED) {
      super_tile = true;
      tile_config = 0x1;
   }

   swizzle_set = 0xFFF007FF |
      (ALIGN_BIT((uint32_t) (surface->peInfo.peDesc.swizzles[0]), 0xC) & (0x3000)) |
      (ALIGN_BIT((uint32_t) (surface->peInfo.peDesc.swizzles[1]), 0xE) & (0xC000)) |
      (ALIGN_BIT((uint32_t) (surface->peInfo.peDesc.swizzles[2]), 0x10) & (0x30000)) |
      (ALIGN_BIT((uint32_t) (surface->peInfo.peDesc.swizzles[3]), 0x12) & (0xC0000));

   surface->peInfo.pixel_config =
      ((ALIGN_BIT((uint32_t) (surface->peInfo.peDesc.format), 0x18) & (0x3F000000)) | (~(0x3F000000 | 0x80000000))) &
      ((ALIGN_BIT((uint32_t) (false), 0x6) & (0x40)) | (~(0x40 | 0x80))) &
      ((ALIGN_BIT((uint32_t) (super_tile), 0x14) & (0x100000)) | (~(0x100000 | 0x200000))) &
      ((ALIGN_BIT((uint32_t) (tile_config), 0xD) & (0x6000)) | (~(0x6000 | 0x8000)));

   surface->peInfo.pe_config = swizzle_set &
      ((ALIGN_BIT((uint32_t) (surface->peInfo.peDesc.srgb), 0x1F) & (0x80000000)) | (~(0x80000000 | 0x40000000))) &
      ((ALIGN_BIT(0x3, 0x5) & (0x60)) | (~(0x60 | 0x400)));

   surface->peInfo.pixel_config_ex =
      (ALIGN_BIT((uint32_t) (surface->peInfo.peDesc.srgb), 0x1E) & (0x40000000)) |
      (ALIGN_BIT((uint32_t) (surface->peInfo.peDesc.format), 0x14) & (0x3F00000)) |
      (ALIGN_BIT((uint32_t) (super_tile), 0x1C) & (0x10000000)) |
      (ALIGN_BIT((uint32_t) (tile_config), 0x1A) & (0xC000000)) |
      (ALIGN_BIT((uint32_t) (surface->stride), 0x0) & (0x3FFFF));

   if (res->ts.bo) {
      if (res->sample_info.is_msaa && res->ts.compressed)
         msaa_compressed = ENABLE;

      surface->peInfo.memory_config =
         (ALIGN_BIT((uint32_t) (msaa_compressed), 0x1A) & (0x4000000)) |
         (ALIGN_BIT((uint32_t) (DISABLE), 0x1B) & (0x8000000)) |
         (ALIGN_BIT((uint32_t) (ENABLE), 0x1) & (0x2)) |
         (ALIGN_BIT((uint32_t) (res->ts.compressed), 0x7) & (0x80)) |
         (ALIGN_BIT((uint32_t) (res->ts.compressed_format), 0x8) & (0xF00)) |
         (ALIGN_BIT((uint32_t) ((util_format_get_blocksize(res->base.format) == 8)), 0x1E) & (0x40000000));

      surface->peInfo.memory_config_ex =
         (ALIGN_BIT((uint32_t) (msaa_compressed), 0x9) & (0x200)) |
         (ALIGN_BIT((uint32_t) (DISABLE), 0xA) & (0x400)) |
         (ALIGN_BIT((uint32_t) (ENABLE), 0x0) & (0x1)) |
         (ALIGN_BIT((uint32_t) (res->ts.compressed), 0x2) & (0x4)) |
         (ALIGN_BIT((uint32_t) (res->ts.compressed_format), 0x3) & (0x78)) |
         (ALIGN_BIT((uint32_t) ((util_format_get_blocksize(res->base.format) == 8)), 0x7) & (0x80));
   } else {
      surface->peInfo.memory_config =
         (ALIGN_BIT((uint32_t) (DISABLE), 0x1) & (0x2)) |
         (ALIGN_BIT((uint32_t) (DISABLE), 0x5) & (0x20)) |
         (ALIGN_BIT((uint32_t) (DISABLE), 0x7) & (0x80));

      surface->peInfo.memory_config_ex =
         (ALIGN_BIT((uint32_t) (DISABLE), 0x0) & (0x1)) |
         (ALIGN_BIT((uint32_t) (DISABLE), 0x1) & (0x2)) |
         (ALIGN_BIT((uint32_t) (DISABLE), 0x2) & (0x4));
   }
}

static void
mwv207_get_depthInfo(struct mwv207_surface *surface)
{
   struct mwv207_resource *res = mwv207_resource(surface->base.texture);

   uint32_t depth_normalize = 0xFFFFFF;
   uint32_t depth_config = 0xFFFFFFFF;
   bool msaa_compressed = DISABLE;
   bool isZ16 = false;

   surface->peInfo.has_stencil = false;
   surface->peInfo.pe_config = ((ALIGN_BIT((uint32_t) (0x0), 0x14) & (0x300000)) | (~(0x300000 | 0x800000)));

   depth_config &= ((ALIGN_BIT((uint32_t) (ENABLE), 0xC) & (0x1000)) | (~(0x1000 | 0x2000)));
   depth_config &= ((ALIGN_BIT((uint32_t) (DISABLE), 0x16) & (0x400000)) | (~(0x400000 | 0x800000)));
   depth_config &= ((ALIGN_BIT((uint32_t) (DISABLE), 0xE) & (0x4000)) | (~(0x4000 | 0x8000)));
   depth_config &= ((ALIGN_BIT((uint32_t) (0x1), 0x4) & (0x10)) | (~(0x10 | 0x20)));
   depth_config &= ((ALIGN_BIT((uint32_t) (true), 0x1A) & (0x4000000)) | (~(0x4000000 | 0x8000000)));

   switch (res->base.format) {
   case PIPE_FORMAT_S8_UINT:
      surface->peInfo.has_stencil = true;
      depth_config &= ((ALIGN_BIT((uint32_t) (DISABLE), 0xC) & (0x1000)) | (~(0x1000 | 0x2000)));
      surface->peInfo.pe_config = ((ALIGN_BIT(0x1, 0x14) & (0x300000)) | (~(0x300000 | 0x800000)));
      break;

   case PIPE_FORMAT_Z16_UNORM:
      isZ16 = true;
      depth_normalize = 0xFFFF;
      depth_config &= ((ALIGN_BIT(0x0, 0x4) & (0x10)) | (~(0x10 | 0x20)));
      break;

   case PIPE_FORMAT_S8_UINT_Z24_UNORM:
      surface->peInfo.has_stencil = true;
      break;

   default:
      break;
   }

   surface->peInfo.depth_config = depth_config;
   surface->peInfo.depth_normalize = depth_normalize;

   if (res->ts.bo) {
      if (res->sample_info.is_msaa && res->ts.compressed)
         msaa_compressed = ENABLE;

      surface->peInfo.memory_config =
         (ALIGN_BIT((uint32_t) (msaa_compressed), 0x1A) & (0x4000000)) |
         (ALIGN_BIT((uint32_t) (ENABLE), 0x0) & (0x1)) |
         (ALIGN_BIT((uint32_t) (res->ts.compressed), 0x6) & (0x40)) |
         (ALIGN_BIT((uint32_t) (surface->peInfo.has_stencil), 0xE) & (0x4000)) |
         (ALIGN_BIT((uint32_t) (isZ16), 0x3) & (0x8));
   } else {
      surface->peInfo.memory_config =
         (ALIGN_BIT((uint32_t) (DISABLE), 0x0) & (0x1)) |
         (ALIGN_BIT((uint32_t) (DISABLE), 0x4) & (0x10)) |
         (ALIGN_BIT((uint32_t) (DISABLE), 0x6) & (0x40));
   }
}

static struct pipe_surface *
mwv207_create_surface(struct pipe_context *pctx,
                      struct pipe_resource *pres,
                      const struct pipe_surface *templ)
{
   unsigned int level = templ->u.tex.level;

   struct mwv207_surface *surface = CALLOC_STRUCT(mwv207_surface);
   struct mwv207_resource *res = mwv207_resource(pres);

   if (!surface)
      return NULL;

   surface->base.context = pctx;
   surface->base.texture = NULL;

   pipe_resource_reference(&surface->base.texture, pres);
   pipe_reference_init(&surface->base.reference, 1);

   surface->base.format = templ->format;
   surface->base.width = u_minify(pres->width0, level);
   surface->base.height = u_minify(pres->height0, level);
   surface->base.nr_samples = templ->nr_samples;
   surface->base.u.tex = templ->u.tex;

   surface->info.first_layer = templ->u.tex.first_layer;
   surface->info.last_layer  = templ->u.tex.last_layer;
   surface->info.layer_count  = templ->u.tex.last_layer - templ->u.tex.first_layer + 1;

   surface->info.first_level = templ->u.tex.level;
   surface->info.last_level  = templ->u.tex.level;
   surface->info.level_count  = 1;

   if (pres->target == PIPE_TEXTURE_CUBE || pres->target == PIPE_TEXTURE_CUBE_ARRAY)
      surface->info.layer_count *= 6;

   if (util_format_get_blocksize(res->base.format) == 16) {
      if (!res->rg) {
         struct pipe_resource templ = res->base;
         templ.bind |= PIPE_BIND_LINEAR;
         templ.format = mwv207_transfer_128bit(res->base.format);
         res->rg = pctx->screen->resource_create(pctx->screen, &templ);
         res->ba = pctx->screen->resource_create(pctx->screen, &templ);
      }

      res = mwv207_resource(res->rg);
      surface->pixel_128bit = true;
   } else
      surface->pixel_128bit = false;

   struct mwv207_level_info *pLevel = &res->level_infos[surface->base.u.tex.level];

   surface->offset = res->offset + pLevel->offset +
      surface->base.u.tex.first_layer * pLevel->layer_size;

   surface->size = pLevel->layer_size;
   surface->stride = pLevel->stride;

   if (res->base.bind & (PIPE_BIND_RENDER_TARGET | PIPE_BIND_SAMPLER_VIEW))
      mwv207_get_renderInfo(surface);

   if (res->base.bind & PIPE_BIND_DEPTH_STENCIL)
      mwv207_get_depthInfo(surface);

   return &surface->base;
}

static void
mwv207_surface_destroy(struct pipe_context *pctx,
                       struct pipe_surface *pSurface)
{
   struct mwv207_surface *surface = mwv207_surface(pSurface);

   if (pSurface->texture)
      pipe_resource_reference(&pSurface->texture, NULL);

   FREE(surface);
}

static int
mwv207_swzzile_state_equal(uint32_t *cur_swzzi, uint32_t *pre_swzzi)
{
   for (int i = 0; i < 4; i++) {
      if (cur_swzzi[i] != pre_swzzi[i])
         return false;
   }

   return true;
}

void
mwv207_set_framebuffer_state(struct pipe_context *pctx,
                             const struct pipe_framebuffer_state *state)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   struct mwv207_surface *surface;
   struct mwv207_resource *res;
   uint32_t count = 0;
   uint8_t rast_samples = 1;
   uint32_t pixle_128_offset = 0;
   bool subsamplez_dirty = false;

   util_copy_framebuffer_state(&ctx->fb_state, state);

   ctx->info.linear_outmode = false;

   for (int i = 0; i < ctx->fb_state.nr_cbufs; i++) {
      ctx->info.cbufs_map[i] = 0;

      if (!ctx->fb_state.cbufs[i])
         continue;

      ctx->info.cbufs_map[i] = count++;

      res = mwv207_resource(ctx->fb_state.cbufs[i]->texture);

      if (res->mode != MWV207_SUPERTILED)
         ctx->info.linear_outmode = true;

      rast_samples = MAX2(rast_samples, res->sample_info.nr_samples);
   }

   ctx->info.pixel_128bit = false;
   ctx->info.frag_rgba_swap = false;
   ctx->info.has_render_arrary = false;

   for (int i = 0; i < ctx->fb_state.nr_cbufs; i++) {
      ctx->info.pixel_record[i] = 0;

      if (!state->cbufs[i])
         continue;

      surface = mwv207_surface(state->cbufs[i]);

      if (surface->pixel_128bit) {
         ctx->info.pixel_128bit = true;
         ctx->info.pixel_record[i] = count + pixle_128_offset;
         ctx->info.cbufs_map[ctx->fb_state.nr_cbufs + pixle_128_offset] = count + pixle_128_offset;
         pixle_128_offset += 1;
      }

      if (surface->info.layer_count != 1)
         ctx->info.has_render_arrary = true;

      ctx->info.swizzle_record[i][0] = surface->peInfo.peDesc.swizzles[0];
      ctx->info.swizzle_record[i][1] = surface->peInfo.peDesc.swizzles[1];
      ctx->info.swizzle_record[i][2] = surface->peInfo.peDesc.swizzles[2];
      ctx->info.swizzle_record[i][3] = surface->peInfo.peDesc.swizzles[3];
   }

   for (int i = 0; i < ctx->fb_state.nr_cbufs; i++) {
      if (i > 0 && !mwv207_swzzile_state_equal(ctx->info.swizzle_record[i], ctx->info.swizzle_record[i - 1])) {
         ctx->info.frag_rgba_swap = true;
         break;
      }
   }

   if (state->zsbuf && state->zsbuf->texture->nr_samples) {
      res = mwv207_resource(state->zsbuf->texture);
      rast_samples = MAX2(rast_samples, res->sample_info.nr_samples);
      if (rast_samples > 1)
         subsamplez_dirty = true;
   }

   ctx->info.subsamplez_dirty = subsamplez_dirty;
   ctx->info.rast_samples = rast_samples;
   ctx->info.pixel_num = pixle_128_offset;
   ctx->dirty |= MWV207_DIRTY_FRAMEBUFFER;
}

void
mwv207_context_surface_init(struct pipe_context *context)
{
   context->create_surface        = mwv207_create_surface;
   context->surface_destroy       = mwv207_surface_destroy;
   context->set_framebuffer_state = mwv207_set_framebuffer_state;
}
