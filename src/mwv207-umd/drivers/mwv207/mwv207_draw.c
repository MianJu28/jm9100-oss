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
#include "util/u_draw.h"
#include "util/u_prim.h"
#include "util/half_float.h"
#include "util/u_helpers.h"
#include "util/format/u_format.h"
#include "indices/u_primconvert.h"

#include "mwv207_cmd.h"
#include "mwv207_context.h"
#include "mwv207_query.h"
#include "mwv207_shader.h"
#include "mwv207_compiler.h"
#include "mwv207_blit.h"
#include "util/u_blitter.h"

static const uint32_t prim_mode[] = {

   0x1,

   0x2,

   0x7,

   0x3,

   0x4,

   0x5,

   0x6,

   0x4,

   0x4,

   0x4,

   0x9,

   0xA,

   0xB,

   0xC,

   0xD,
};

static void
mwv207_config_vertex(struct mwv207_context *ctx,
                     const struct pipe_draw_info *dinfo,
                     const struct pipe_draw_start_count_bias *draws)
{
   struct pipe_resource *index_buffer = NULL;
   unsigned index_offset = 0;
   uint32_t *cmd_buf;
   uint32_t offset = 0;
   uint32_t stride[PIPE_MAX_ATTRIBS] = {0};

   if (dinfo->index_size > 0) {
      index_buffer = dinfo->has_user_indices ? NULL : dinfo->index.resource;

      if (dinfo->has_user_indices &&
            !util_upload_index_buffer(&ctx->base, dinfo, &draws[0], &index_buffer, &index_offset, 4)) {
         debug_printf("util_upload_index_buffer() failed\n");
         return;
      }
   }

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   MWV207_EMIT(cmd_buf, 0x1F0, ctx->info.vertices_per_patch);

   if (ctx->element_state->num_elements == 0) {
      mwv207_wait_idle(&cmd_buf, 0x7);
      MWV207_EMIT(cmd_buf, 0x1F2, true);
   }

   for (int i = 0; i < ctx->element_state->num_elements; ++i) {
      struct mwv207_element element = ctx->element_state->elements[i];

      MWV207_EMIT(cmd_buf, 0x5E00 + i, element.attribue_config);

      MWV207_EMIT(cmd_buf, 0x5E80 + i, element.weight);

      MWV207_EMIT(cmd_buf, 0x5EA0 + i,
         (ALIGN_BIT((uint32_t) (element.fetch_break), 0xB) & (0x800)) |
         (ALIGN_BIT((uint32_t) (element.fetch_size), 0x0) & (0x1FF)));

      MWV207_EMIT(cmd_buf, 0x51A0 + i, element.base.instance_divisor);

      stride[element.base.vertex_buffer_index] = element.base.src_stride;
   }

   for (unsigned i = 0; i < ctx->num_vertex_buffers; i++) {
      struct pipe_vertex_buffer vb = ctx->vertex_buffer[i];
      struct mwv207_resource *res = mwv207_resource(vb.buffer.resource);

      if (res) {
         offset = vb.buffer_offset;

         if (dinfo->start_instance)
            offset += dinfo->start_instance * stride[i];

         mwv207_add_relocs(ctx, &cmd_buf, 0x5180 + i,
            res->bo, offset, 0x0);
      } else {
         MWV207_EMIT(cmd_buf, 0x5180 + i, 0);
      }
      MWV207_EMIT(cmd_buf, 0x5190 + i, stride[i]);
   }

   if (dinfo->index_size > 0) {
      uint32_t indexType, restartIndex;
      struct mwv207_resource *res = NULL;

      if (dinfo->index_size == 1) {
         indexType = 0x0;
         restartIndex = 0xFF;
      } else if (dinfo->index_size == 2) {
         indexType = 0x1;
         restartIndex = 0xFFFF;
      } else {
         indexType = 0x2;
         restartIndex = 0xFFFFFFFF;
      }

      res = mwv207_resource(index_buffer);
      offset = index_offset;
      offset += draws->start * dinfo->index_size;

      mwv207_add_relocs(ctx, &cmd_buf, 0x191,
         res->bo, offset, 0x0);

      MWV207_EMIT(cmd_buf, 0x192,
         (ALIGN_BIT((uint32_t) (indexType), 0x0) & (0x3)) |
         (ALIGN_BIT((uint32_t) (dinfo->primitive_restart), 0x8) & (0x100)));

      if (dinfo->primitive_restart) {
         MWV207_EMIT(cmd_buf, 0x19D, dinfo->restart_index);
      } else {
         MWV207_EMIT(cmd_buf, 0x19D, restartIndex);
      }
   } else {
      MWV207_EMIT(cmd_buf, 0x191, 0);
      MWV207_EMIT(cmd_buf, 0x192, 0);
      MWV207_EMIT(cmd_buf, 0x19D, 0);
   }

   mwv207_end_record(ctx->cmd, &cmd_buf);

   if (index_buffer != dinfo->index.resource)
      pipe_resource_reference(&index_buffer, NULL);

}

static void
mwv207_config_viewport(struct mwv207_context *ctx)
{
   uint32_t  *cmd_buf;
   mwv207_begin_record(ctx->cmd, &cmd_buf);

   float clip_w = MAX2(ctx->viewport.scale[0], ctx->viewport.scale[1]);
   clip_w = clip_w / (2.0f * 8388607.0f - 8192.0f);

   MWV207_EMIT(cmd_buf, 0x280,  fui(ctx->viewport.scale[0]));
   MWV207_EMIT(cmd_buf, 0x281,  fui(ctx->viewport.scale[1]));
   MWV207_EMIT(cmd_buf, 0x282,  fui(ctx->viewport.scale[2]));
   MWV207_EMIT(cmd_buf, 0x283, fui(ctx->viewport.translate[0]));
   MWV207_EMIT(cmd_buf, 0x284, fui(ctx->viewport.translate[1]));
   MWV207_EMIT(cmd_buf, 0x285, fui(ctx->viewport.translate[2]));
   MWV207_EMIT(cmd_buf, 0x501, fui(0.0f));
   MWV207_EMIT(cmd_buf, 0x502,  fui(1.0f));
   MWV207_EMIT(cmd_buf, 0x2A0,   fui(clip_w));
   MWV207_EMIT(cmd_buf, 0x28B, fui(1.0f / 8388607.0f));
   MWV207_EMIT(cmd_buf, 0x2A3, 0x00000000);
   MWV207_EMITF(cmd_buf, 0x2A1, 0x00800000);

   mwv207_end_record(ctx->cmd, &cmd_buf);
}

static void
mwv207_config_scissor(struct mwv207_context *ctx)
{
   struct pipe_scissor_state scissor;
   uint32_t  *cmd_buf;
   int32_t fix_right = 0;
   int32_t fix_bottom = 0x1111;
   uint32_t left, top, right, bottom;

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   if (ctx->rast_state && ctx->rast_state->base.scissor)
      scissor = ctx->scissor;
   else {
      scissor.minx = ctx->viewport.translate[0] - MAX2(ctx->viewport.scale[0], -ctx->viewport.scale[0]);
      scissor.miny = ctx->viewport.translate[1] - MAX2(ctx->viewport.scale[1], -ctx->viewport.scale[1]);
      scissor.maxx = ctx->viewport.translate[0] + MAX2(ctx->viewport.scale[0], -ctx->viewport.scale[0]);
      scissor.maxy = ctx->viewport.translate[1] + MAX2(ctx->viewport.scale[1], -ctx->viewport.scale[1]);
   }

   left   = MAX2(scissor.minx, 0);
   top    = MAX2(scissor.miny, 0);
   right  = MIN2(scissor.maxx, ctx->fb_state.width);
   bottom = MIN2(scissor.maxy, ctx->fb_state.height);

   if ((left >= right) || (top >= bottom))
      left = top = right = bottom = 1;

   if (right == 8192)
      fix_right = -0x119;

   if (bottom == 8192)
      fix_bottom = -0x111;

   MWV207_EMITF(cmd_buf, 0x301, top << 16);
   MWV207_EMITF(cmd_buf, 0x300, left << 16);
   MWV207_EMITF(cmd_buf, 0x302, ((right << 16) + fix_right));
   MWV207_EMITF(cmd_buf, 0x303, ((bottom << 16) + fix_bottom));
   MWV207_EMITF(cmd_buf, 0x308, ((right << 16) + 0xFFFF));
   MWV207_EMITF(cmd_buf, 0x309, ((bottom << 16) + 0xFFFF));

   mwv207_end_record(ctx->cmd, &cmd_buf);
}

static void
mwv207_config_framebuffer(struct mwv207_context *ctx)
{
   struct mwv207_resource *res;
   struct mwv207_tile_info *tsInfo;
   uint32_t tileStatusAddress = 0;
   uint32_t memory_config = 0;
   uint32_t tileCount = 0;
   uint32_t rtMaxLayers = 0;
   uint32_t output_config = 0;
   uint32_t output_precision[2] = {0};
   uint32_t *cmd_buf;
   uint32_t color_count = 0;

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   if (ctx->info.linear_outmode) {
      MWV207_EMIT(cmd_buf, 0x529,
         ((ALIGN_BIT(0x1, 0x8) & (0x300)) | (~(0x300 | 0x80))));
   } else {
      MWV207_EMIT(cmd_buf, 0x529,
         ((ALIGN_BIT(0x3, 0x8) & (0x300)) | (~(0x300 | 0x80))));
   }

   for (int i = 0; i < ctx->fb_state.nr_cbufs; i++) {
      uint32_t render_num;
      struct mwv207_surface *surface;

      if (!ctx->fb_state.cbufs[i])
         continue;

      render_num = ctx->info.cbufs_map[i];
      surface = mwv207_surface(ctx->fb_state.cbufs[i]);
      res = mwv207_resource(surface->base.texture);

      if (surface->pixel_128bit) {
         res->rgba_dirty = true;
         res = mwv207_resource(res->rg);
      }

      if (res->linear)
         res->need_decompress = true;

      res->linear_output = ctx->info.linear_outmode;

      tsInfo = &res->ts;

      if (tsInfo->bo)
         tileStatusAddress = tsInfo->layer_offset * surface->info.first_layer;
      else
         tileStatusAddress = 0;

      tileCount = surface->size / ((CACHE_MODE == 0x1) ? 256 : 128);
      MWV207_EMIT(cmd_buf, 0xE03, (ALIGN_BIT((uint32_t) (ENABLE), 0x1) & (0x2)));

      if (render_num == 0) {
         MWV207_EMIT(cmd_buf, 0xffff, 0x00000000);

         if (tsInfo->bo)
            mwv207_add_relocs(ctx, &cmd_buf, 0x596,
               tsInfo->bo, tileStatusAddress, 0x1);
         else
            MWV207_EMIT(cmd_buf, 0x596, 0);

         mwv207_add_relocs(ctx, &cmd_buf, 0x597,
            res->bo, surface->offset, 0x1);

         MWV207_EMIT(cmd_buf, 0x598, tsInfo->clear_values[0]);
         MWV207_EMIT(cmd_buf, 0x5AF, tsInfo->clear_values[1]);

         if (tsInfo->enable)
            memory_config |= surface->peInfo.memory_config;

         MWV207_EMIT(cmd_buf, 0x595, memory_config);

         mwv207_add_relocs(ctx, &cmd_buf, 0x518,
            res->bo, surface->offset , 0x1);
         mwv207_add_relocs(ctx, &cmd_buf, 0x518 + 1,
            res->bo, surface->offset , 0x1);

         MWV207_EMIT(cmd_buf, 0x50D, surface->stride);
         MWV207_EMIT(cmd_buf, 0x50B, surface->peInfo.pixel_config);
         MWV207_EMIT(cmd_buf, 0xE28, surface->size);

         if (ctx->info.frag_rgba_swap)
            mwv207_update_renderInfo(surface);

         MWV207_EMIT(cmd_buf, 0x529, surface->peInfo.pe_config);
         rtMaxLayers = surface->info.layer_count - 1;
      } else {
         if (tsInfo->compressed == false)
            MWV207_EMIT(cmd_buf, 0x690 + render_num, tileCount);

         if (tsInfo->bo)
            mwv207_add_relocs(ctx, &cmd_buf, 0x5F0 + render_num,
               tsInfo->bo, tileStatusAddress, 0x1);
         else
            MWV207_EMIT(cmd_buf, 0x5F0 + render_num, 0);

         mwv207_add_relocs(ctx, &cmd_buf, 0x5F8 + render_num,
            res->bo, surface->offset, 0x1);

         MWV207_EMIT(cmd_buf, 0x680 + render_num, tsInfo->clear_values[0]);
         MWV207_EMIT(cmd_buf, 0x688 + render_num, tsInfo->clear_values[1]);

         if (tsInfo->enable) {
            MWV207_EMIT(cmd_buf, 0x5E8 + render_num, surface->peInfo.memory_config_ex);
         } else {
            MWV207_EMIT(cmd_buf, 0x5E8 + render_num, 0);
         }

         mwv207_add_relocs(ctx, &cmd_buf, (0x5200 + (render_num - 1) * 8),
            res->bo, surface->offset, 0x1);
         mwv207_add_relocs(ctx, &cmd_buf, (0x5200 + (render_num - 1) * 8) + 1,
            res->bo, surface->offset, 0x1);

         MWV207_EMIT(cmd_buf, 0xE28 + render_num, surface->size);
      }

      mwv207_config_arrays(&output_config, 4, render_num, surface->peInfo.peDesc.output_mode);
      mwv207_config_arrays(output_precision, 8, render_num, 0x3f);
      color_count++;

      if (ctx->info.pixel_record[i]) {
         uint32_t ba_index = ctx->info.pixel_record[i];
         res = mwv207_resource(surface->base.texture);
         res = mwv207_resource(res->ba);

         tsInfo = &res->ts;

         if (tsInfo->bo)
            tileStatusAddress = tsInfo->layer_offset * surface->info.first_layer;
         else
            tileStatusAddress = 0;

         tileCount = surface->size / ((CACHE_MODE == 0x1) ? 256 : 128);
         MWV207_EMIT(cmd_buf, 0xE03, (ALIGN_BIT((uint32_t) (ENABLE), 0x1) & (0x2)));

         if (tsInfo->bo)
            mwv207_add_relocs(ctx, &cmd_buf, 0x5F0 + ba_index,
               tsInfo->bo, tileStatusAddress, 0x1);
         else
            MWV207_EMIT(cmd_buf, 0x5F0 + ba_index, 0);

         mwv207_add_relocs(ctx, &cmd_buf, 0x5F8 + ba_index,
            res->bo, surface->offset, 0x1);

         MWV207_EMIT(cmd_buf, 0x680 + ba_index, tsInfo->clear_values[0]);
         MWV207_EMIT(cmd_buf, 0x688 + ba_index, tsInfo->clear_values[1]);

         if (tsInfo->enable) {
            MWV207_EMIT(cmd_buf, 0x5E8 + ba_index, surface->peInfo.memory_config_ex);
         } else {
            MWV207_EMIT(cmd_buf, 0x5E8 + ba_index, 0);
         }
         mwv207_add_relocs(ctx, &cmd_buf, (0x5200 + (ba_index - 1) * 8),
            res->bo, surface->offset, 0x1);
         mwv207_add_relocs(ctx, &cmd_buf, (0x5200 + (ba_index - 1) * 8) + 1,
            res->bo, surface->offset, 0x1);

         MWV207_EMIT(cmd_buf, 0xE28 + ba_index, surface->size);

         mwv207_config_arrays(&output_config, 4, ctx->info.pixel_record[i], surface->peInfo.peDesc.output_mode);
         mwv207_config_arrays(output_precision, 8, ctx->info.pixel_record[i], 0x3f);
         color_count++;
      }
   }

   if (color_count == 0 && !ctx->query_info->enabled) {
      MWV207_EMIT(cmd_buf, 0x404,
         (ALIGN_BIT((uint32_t) (ENABLE), 0x0) & (0x1)));
   } else {
      MWV207_EMIT(cmd_buf, 0x404,
         (ALIGN_BIT((uint32_t) (color_count - 1), 0x8) & (0xF00)));
   }

   MWV207_EMIT(cmd_buf, 0x40C, output_config);
   MWV207_EMITS(cmd_buf, 0x40D, 2, output_precision);

   if (ctx->fb_state.nr_cbufs == 0)
      MWV207_EMIT(cmd_buf, 0x50B, ((ALIGN_BIT((uint32_t) (0x0), 0x8) & (0xF00)) | (~(0xF00 | 0x1000))) &
         ((ALIGN_BIT((uint32_t) (0x1), 0x6) & (0x40)) | (~(0x40 | 0x80))) &
         ((ALIGN_BIT((uint32_t) (0x1), 0x10) & (0x10000)) | (~(0x10000 | 0x20000))));

   if (ctx->fb_state.zsbuf) {
      struct mwv207_surface *surface = mwv207_surface(ctx->fb_state.zsbuf);
      res = mwv207_resource(surface->base.texture);
      tsInfo = &res->ts;

      if (tsInfo->bo)
         tileStatusAddress = tsInfo->layer_offset * surface->info.first_layer;
      else
         tileStatusAddress = 0;

      tileCount = surface->size / ((CACHE_MODE == 0x1) ? 256 : 128);

      MWV207_EMIT(cmd_buf, 0xE03, (ALIGN_BIT((uint32_t) (ENABLE), 0x0) & (0x1)));
      MWV207_EMIT(cmd_buf, 0xffff, 0x00000000);

      if (tsInfo->bo)
         mwv207_add_relocs(ctx, &cmd_buf, 0x599,
            tsInfo->bo, tileStatusAddress , 0x1);
      else
         MWV207_EMIT(cmd_buf, 0x599, 0);

      mwv207_add_relocs(ctx, &cmd_buf, 0x59A,
         res->bo, surface->offset, 0x1);

      MWV207_EMIT(cmd_buf, 0x59B, tsInfo->clear_values[0]);
      MWV207_EMIT(cmd_buf, 0x6D0, tsInfo->clear_values[1]);

      if (tsInfo->enable)
         memory_config |= surface->peInfo.memory_config;

      MWV207_EMIT(cmd_buf, 0x595, memory_config);

      mwv207_add_relocs(ctx, &cmd_buf, 0x520,
         res->bo, surface->offset, 0x1);
      mwv207_add_relocs(ctx, &cmd_buf, 0x520 + 1,
         res->bo, surface->offset, 0x1);

      MWV207_EMIT(cmd_buf, 0x505, surface->stride);
      MWV207_EMIT(cmd_buf, 0xE23, surface->size);

      MWV207_EMIT(cmd_buf, 0x503, fui(surface->peInfo.depth_normalize));
      MWV207_EMIT(cmd_buf, 0x529, surface->peInfo.pe_config);
      rtMaxLayers = MAX2(rtMaxLayers, (surface->info.layer_count - 1));
   } else {
      MWV207_EMIT(cmd_buf, 0x500,
         ((ALIGN_BIT(0x0, 0x0) & (0x3)) | (~(0x3 | 0x8))) &
         ((ALIGN_BIT((uint32_t) (DISABLE), 0x14) & (0x100000)) | (~(0x100000 | 0x200000))) &
         ((ALIGN_BIT((uint32_t) (DISABLE), 0x10) & (0x10000)) | (~(0x10000 | 0x20000))) &
         ((ALIGN_BIT(0x1, 0x18) & (0x1000000)) | (~(0x1000000 | 0x2000000))) &
         ((ALIGN_BIT(0x1, 0x12) & (0x40000)) | (~(0x40000 | 0x80000))) &
         ((ALIGN_BIT((uint32_t) (0x7), 0x8) & (0x700)) | (~(0x700 | 0x800))));
      MWV207_EMIT(cmd_buf, 0x503, 0);
      MWV207_EMIT(cmd_buf, 0xE23, 0);
   }

   MWV207_EMIT(cmd_buf, 0x426, rtMaxLayers);

   MWV207_EMIT(cmd_buf, 0x38D,
      (ALIGN_BIT((uint32_t) ((rtMaxLayers != 0)), 0x4) & (0x10)));

   mwv207_end_record(ctx->cmd, &cmd_buf);
}

static void
mwv207_config_rasterizer(struct mwv207_context *ctx)
{
   uint32_t *cmd_buf;
   struct mwv207_rasterizer_state *rast_state = ctx->rast_state;

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   mwv207_wait_idle(&cmd_buf, 0x7);

   MWV207_EMIT(cmd_buf, 0x2A2, 0x40000000);

   MWV207_EMIT(cmd_buf, 0x28A, 0x1 | 0x10);

   MWV207_EMIT(cmd_buf, 0xE13, 0x0);

   MWV207_EMIT(cmd_buf, 0x28D, rast_state->pa_config);

   MWV207_EMIT(cmd_buf, 0x7000,
      (ALIGN_BIT((uint32_t) (rast_state->tfb_control), 0x0) & (0x1)) |
      (ALIGN_BIT((uint32_t) (0), 0x4) & (0x30)));

   MWV207_EMIT(cmd_buf, 0x286, rast_state->line_scale);
   MWV207_EMIT(cmd_buf, 0x28E, rast_state->line_scale);
   MWV207_EMIT(cmd_buf, 0x28F, rast_state->line_scale);

   if (!rast_state->base.point_size_per_vertex)
      MWV207_EMIT(cmd_buf, 0x287, fui(rast_state->base.point_size / 2.0f));

   MWV207_EMIT(cmd_buf, 0x380, (ALIGN_BIT((uint32_t) (ENABLE), 0x0) & (0x1)));

   MWV207_EMIT(cmd_buf, 0x306, 0x00000000);

   if (ctx->fb_state.zsbuf) {
      struct mwv207_surface *surface = mwv207_surface(ctx->fb_state.zsbuf);
      MWV207_EMIT(cmd_buf, 0x305,
         fui((ctx->rast_state->base.offset_units * 2.0f / surface->peInfo.depth_normalize)));
   }

   MWV207_EMIT(cmd_buf, 0x304, fui(rast_state->base.offset_scale));

   mwv207_end_record(ctx->cmd, &cmd_buf);
}

static void
mwv207_config_multisamples(struct mwv207_context *ctx)
{
   uint32_t *cmd_buf;
   uint32_t pe_config = 0, pe_config_ex = 0;
   uint32_t sample_enable_bits = 0;
   uint32_t sample_mode = 0x0;

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   if (ctx->info.rast_samples > 1) {
      pe_config =
         ((ALIGN_BIT(0x1, 0x18) & (0x1000000)) | (~(0x1000000 | 0x2000000))) &
         ((ALIGN_BIT(0x1, 0x1A) & (0x4000000)) | (~(0x4000000 | 0x8000000)));

      if (ctx->fb_state.nr_cbufs)
         pe_config_ex |= (ALIGN_BIT((uint32_t) (ENABLE), 0xF) & (0x8000));
      pe_config_ex |= (ALIGN_BIT((uint32_t) (DISABLE), 0x10) & (0x10000));

      sample_mode = 0x2;
      sample_enable_bits = 0xff;
   } else {
      pe_config =
         ((ALIGN_BIT(0x0, 0x5) & (0x60)) | (~(0x60 | 0x400))) &
         ((ALIGN_BIT(0x0, 0x18) & (0x1000000)) | (~(0x1000000 | 0x2000000))) &
         ((ALIGN_BIT(0x0, 0x1A) & (0x4000000)) | (~(0x4000000 | 0x8000000)));
   }

   if (CACHE_MODE == 0x1) {
      pe_config_ex |=
         (ALIGN_BIT(0x1, 0x18) & (0x1000000)) |
         (ALIGN_BIT(0x1, 0x1A) & (0x4000000));
   } else {
      pe_config_ex |=
         (ALIGN_BIT(0x0, 0x18) & (0x1000000)) |
         (ALIGN_BIT(0x0, 0x1A) & (0x4000000));
   }

   MWV207_EMIT(cmd_buf, 0x418 + 0, 0x6e80e680);
   MWV207_EMIT(cmd_buf, 0x418 + 1, 0x2ac42a4c);
   MWV207_EMIT(cmd_buf, 0x418 + 2, 0x15fb5d3b);
   MWV207_EMIT(cmd_buf, 0x418 + 3, 0x9d7391f7);
   MWV207_EMIT(cmd_buf, 0x418 + 4, 0x08e691f7);
   MWV207_EMIT(cmd_buf, 0x418 + 5, 0x4ca25d3b);
   MWV207_EMIT(cmd_buf, 0x418 + 6, 0xbf512a4c);
   MWV207_EMIT(cmd_buf, 0x418 + 7, 0x37d9e680);

   MWV207_EMIT(cmd_buf, 0xE06,
      (ALIGN_BIT((uint32_t) (sample_mode), 0x0) & (0x3)) |
      (ALIGN_BIT((uint32_t) (sample_enable_bits), 0x4) & (0xF0)) |
      (ALIGN_BIT(0x0, 0xC) & (0x7000)) |
      (ALIGN_BIT(0x0, 0x10) & (0x30000)) |
      (ALIGN_BIT((uint32_t) (3), 0x14) & (0x300000)) |
      (ALIGN_BIT((uint32_t) (DISABLE), 0x18) & (0x1000000)) |
      (ALIGN_BIT((uint32_t) (ENABLE), 0x1C) & (0x10000000)));
   if (ctx->info.rast_samples > 1) {
      MWV207_EMIT(cmd_buf, 0x381, 0x0);
      MWV207_EMIT(cmd_buf, 0x390,     0x4a6e2688);
      MWV207_EMIT(cmd_buf, 0x390 + 1, 0x888864a2);
      MWV207_EMIT(cmd_buf, 0x390 + 2, 0x88ac88ea);
      MWV207_EMIT(cmd_buf, 0x390 + 3, 0x888888c6);
   }

   MWV207_EMIT(cmd_buf, 0x383, 0x00000000);

   MWV207_EMIT(cmd_buf, 0x529, pe_config);
   MWV207_EMIT(cmd_buf, 0x52F, pe_config_ex);

   mwv207_end_record(ctx->cmd, &cmd_buf);
}

static void
mwv207_config_sample_mask(struct mwv207_context *ctx)
{
   uint32_t *cmd_buf;
   bool coverage_enable = ctx->sample_mask != 0xffff ? 1 : 0;
   bool coverage_invert = 0xfffffff0 & ctx->sample_mask ? 1 : 0;
   uint32_t sample_mask = 0xf & ctx->sample_mask;

   if (coverage_invert)
      sample_mask = (~sample_mask & 0xf);

   if (sample_mask != 0x0)
      sample_mask = 0x10 | sample_mask;

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   MWV207_EMIT(cmd_buf, 0x415,
      ((ALIGN_BIT((uint32_t) (coverage_invert), 0x10) & (0x10000)) | (~(0x10000 | 0x80000))) &
      ((ALIGN_BIT((uint32_t) (sample_mask), 0x14) & (0x1F00000)) | (~(0x1F00000 | 0x8000000))) &
      ((ALIGN_BIT((uint32_t) (coverage_enable), 0x1C) & (0x10000000)) | (~(0x10000000 | 0x80000000))));

   mwv207_end_record(ctx->cmd, &cmd_buf);
}

static void
mwv207_config_zsa(struct mwv207_context *ctx)
{
   uint32_t *cmd_buf;

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   for (int i = 0; i < ctx->fb_state.nr_cbufs; i++) {
      if (i == 0) {
         MWV207_EMIT(cmd_buf, 0x508, ctx->zsa->alpha_config);
      } else {
         MWV207_EMIT(cmd_buf, 0x5250 + (i - 1), ctx->zsa->alpha_config);
      }
      if (ctx->info.pixel_record[i])
         MWV207_EMIT(cmd_buf, 0x5250 + (ctx->info.pixel_record[i] - 1), ctx->zsa->alpha_config);
   }

   MWV207_EMIT(cmd_buf, 0x528, ctx->zsa->alpha_reference);

   MWV207_EMIT(cmd_buf, 0x388,
      (ALIGN_BIT((uint32_t) (ctx->zsa->func), 0xC) & (0x7000)));

   MWV207_EMIT(cmd_buf, 0x515, 0);

   mwv207_end_record(ctx->cmd, &cmd_buf);
}

static void
mwv207_config_blend(struct mwv207_context *ctx)
{
   uint32_t *cmd_buf;

   if (ctx->blend_state == NULL)  return;

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   MWV207_EMIT(cmd_buf, 0x529, ctx->blend_state->logic_config);

   for (int i = 0; i < ctx->fb_state.nr_cbufs; i++) {
      if (!ctx->fb_state.cbufs[i])
         continue;

      uint32_t render_num = ctx->info.cbufs_map[i];
      uint32_t colormask = mwv207_update_colormask(ctx, i);
      uint32_t dst_read = 0x1;

      if (ctx->blend_state->dest_read[i]) {
         MWV207_EMIT(cmd_buf, 0xE03, (ALIGN_BIT((uint32_t) (ENABLE), 0x1) & (0x2)));
         dst_read = 0x0;
      }

      if (render_num == 0) {
         MWV207_EMIT(cmd_buf, 0x530, ctx->blend_state->alpha_config);

         MWV207_EMIT(cmd_buf, 0x50A, ctx->blend_state->alpha_blend);

         MWV207_EMIT(cmd_buf, 0x50B,
            ((ALIGN_BIT((uint32_t) (colormask), 0x8) & (0xF00)) | (~(0xF00 | 0x1000))) &
            ((ALIGN_BIT((uint32_t) (dst_read), 0x10) & (0x10000)) | (~(0x10000 | 0x20000))));
      } else {
         MWV207_EMIT(cmd_buf, 0x5258 + (render_num - 1),
            ctx->blend_state->blend_config[i]);

         MWV207_EMIT(cmd_buf, 0x5248 + (render_num - 1),
            ((ALIGN_BIT((uint32_t) (colormask), 0x4) & (0xF0)) | (~(0xF0 | 0x800)))&
            ((ALIGN_BIT((uint32_t) (dst_read), 0x8) & (0x100)) | (~(0x100 | 0x200))));
      }

      if (ctx->info.pixel_record[i]) {
         uint32_t newrtcm = ctx->blend_state->color_mask[i] & (PIPE_MASK_B | PIPE_MASK_A);

         if (ctx->blend_state->color_mask[i] & PIPE_MASK_B)
            newrtcm |= PIPE_MASK_R;

         if (ctx->blend_state->color_mask[i] & PIPE_MASK_A)
            newrtcm |= PIPE_MASK_G;

         MWV207_EMIT(cmd_buf, 0x5258 + (ctx->info.pixel_record[i] - 1),
            ctx->blend_state->blend_config[i]);
         MWV207_EMIT(cmd_buf, 0x5248 + (ctx->info.pixel_record[i] - 1),
            ((ALIGN_BIT((uint32_t) (newrtcm), 0x4) & (0xF0)) | (~(0xF0 | 0x800))));
      }
   }

   if (ctx->blend_state->blend_state.dither) {
      MWV207_EMIT(cmd_buf, 0x52A, 0x6e4ca280);
      MWV207_EMIT(cmd_buf, 0x52B, 0x5d7f91b3);
   } else {
      MWV207_EMIT(cmd_buf, 0x52A, 0xffffffff);
      MWV207_EMIT(cmd_buf, 0x52B, 0xffffffff);
   }

   mwv207_end_record(ctx->cmd, &cmd_buf);
}

static void
mwv207_config_blendColor(struct mwv207_context *ctx)
{
   uint32_t  *cmd_buf;
   uint32_t color_8, colorLow, colorHigh;
   struct pipe_blend_color *color = &ctx->blend_color;
   struct mwv207_surface *surface = NULL;

   for (int i = 0; i < ctx->fb_state.nr_cbufs; i++) {
      if (ctx->fb_state.cbufs[i]) {
         surface = mwv207_surface(ctx->fb_state.cbufs[i]);
         break;
      }
   }

   if (!surface)
      return;

   color_8 = (float_to_ubyte(color->color[surface->peInfo.peDesc.swizzles[0]]) << 8) |
             (float_to_ubyte(color->color[surface->peInfo.peDesc.swizzles[1]]) << 8) |
             (float_to_ubyte(color->color[surface->peInfo.peDesc.swizzles[2]]) << 8) |
             (float_to_ubyte(color->color[surface->peInfo.peDesc.swizzles[3]]) << 8);

   colorLow  = _mesa_float_to_half(color->color[surface->peInfo.peDesc.swizzles[0]]) |
               (_mesa_float_to_half(color->color[surface->peInfo.peDesc.swizzles[1]]) << 16);
   colorHigh = _mesa_float_to_half(color->color[surface->peInfo.peDesc.swizzles[2]]) |
               (_mesa_float_to_half(color->color[surface->peInfo.peDesc.swizzles[3]]) << 16);

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   MWV207_EMIT(cmd_buf, 0x509, color_8);

   MWV207_EMIT(cmd_buf, 0x52C, colorLow);
   MWV207_EMIT(cmd_buf, 0x52D, colorHigh);

   for (int i = 1; i < ctx->fb_state.nr_cbufs; i++) {
      if (!ctx->fb_state.cbufs[i])
         continue;

      MWV207_EMIT(cmd_buf, 0x5260 + (i - 1), colorLow);
      MWV207_EMIT(cmd_buf, 0x5268 + (i - 1), colorHigh);

      if (ctx->info.pixel_record[i]) {
         MWV207_EMIT(cmd_buf, 0x5260 + (ctx->info.pixel_record[i] - 1), colorLow);
         MWV207_EMIT(cmd_buf, 0x5268 + (ctx->info.pixel_record[i] - 1), colorHigh);
      }
   }

   mwv207_end_record(ctx->cmd, &cmd_buf);
}

static void
mwv207_config_stream_out(struct mwv207_context *ctx, bool suspend)
{
   uint32_t stride;
   uint32_t dst_offset[4] = {0};
   uint32_t tfb_des_count = 0;
   uint32_t *cmd_buf;
   struct mwv207_resource *buffer;
   struct pipe_stream_output output;
   struct pipe_stream_output_info stream_output;
   struct pipe_stream_output_target *target;
   struct mwv207_stream_output_target *mwv207_target;

   if (ctx->shader_states[PIPE_SHADER_GEOMETRY])
      stream_output = ctx->shader_states[PIPE_SHADER_GEOMETRY]->stream_output;
   else if (ctx->shader_states[PIPE_SHADER_TESS_EVAL])
      stream_output = ctx->shader_states[PIPE_SHADER_TESS_EVAL]->stream_output;
   else
      stream_output = ctx->shader_states[PIPE_SHADER_VERTEX]->stream_output;

   if (ctx->stream_out.num_targets == 0)
      return;

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   mwv207_wait_idle(&cmd_buf, 0x7);

   MWV207_EMIT(cmd_buf, 0xE03, (ALIGN_BIT((uint32_t) (ENABLE), 0xE) & (0x4000)));

   if (suspend) {
      MWV207_EMIT(cmd_buf, 0x7001, (ALIGN_BIT(0x2, 0x0) & (0x7)));

      MWV207_EMIT(cmd_buf, 0xE02,
         (ALIGN_BIT(0x3, 0x1C) & (0x30000000)) |
         (ALIGN_BIT((uint32_t) (0x1), 0x0) & (0x1F)) |
         (ALIGN_BIT((uint32_t) (0x7), 0x8) & (0x1F00)));
      MWV207_STALL(&cmd_buf,
         (ALIGN_BIT(0x3, 0x1C) & (0x30000000)) |
         (ALIGN_BIT((uint32_t) (0x1), 0x0) & (0x1F)) |
         (ALIGN_BIT((uint32_t) (0x7), 0x8) & (0x1F00)));
   } else {

      mwv207_target = mwv207_stream_output_target(ctx->stream_out.targets[0]);
      mwv207_add_relocs(ctx, &cmd_buf, 0x7002,
         mwv207_target->bo, 0, 0x1);

      for (unsigned i = 0; i < stream_output.num_outputs; i++) {
         output = stream_output.output[i];

         while (dst_offset[output.output_buffer] != output.dst_offset) {
            uint32_t offset_components = output.dst_offset - dst_offset[output.output_buffer];

            if (offset_components > 4)
               offset_components = 4;

            MWV207_EMIT(cmd_buf, 0x7200 + tfb_des_count,
               (ALIGN_BIT((uint32_t) (output.output_buffer), 0x0) & (0x3)) |
               (ALIGN_BIT((uint32_t) (true), 0x4) & (0x10)) |
               (ALIGN_BIT((uint32_t) (offset_components), 0x14) & (0x300000)));
            tfb_des_count += 1;
            dst_offset[output.output_buffer] += offset_components;
         }

         MWV207_EMIT(cmd_buf, 0x7200 + tfb_des_count,
            (ALIGN_BIT((uint32_t) (output.output_buffer), 0x0) & (0x3)) |
            (ALIGN_BIT((uint32_t) (output.register_index), 0x8) & (0x1F00)) |
            (ALIGN_BIT((uint32_t) (output.start_component), 0x10) & (0x30000)) |
            (ALIGN_BIT((uint32_t) (output.num_components), 0x14) & (0x300000)));
         tfb_des_count += 1;
         dst_offset[output.output_buffer] += output.num_components;
      }

      MWV207_EMIT(cmd_buf, 0x7040, tfb_des_count);

      for (unsigned i = 0; i < ctx->stream_out.num_targets; i++) {
         stride = stream_output.stride[i] * 4;
         target = ctx->stream_out.targets[i];
         buffer = mwv207_resource(target->buffer);
         mwv207_add_relocs(ctx, &cmd_buf, 0x7010 + i,
            buffer->bo, buffer->offset + target->buffer_offset , 0x1);
         MWV207_EMIT(cmd_buf, 0x7030 + i, stride);
         MWV207_EMIT(cmd_buf, 0x7020 + i, target->buffer_size);
      }

      if (mwv207_target->is_actived) {
         MWV207_EMIT(cmd_buf, 0x7001, (ALIGN_BIT(0x4, 0x0) & (0x7)));
      } else {
         mwv207_target->is_actived = true;
         MWV207_EMIT(cmd_buf, 0x7001, (ALIGN_BIT(0x1, 0x0) & (0x7)));
      }
   }

   mwv207_end_record(ctx->cmd, &cmd_buf);

}

static void
mwv207_config_draw(struct mwv207_context *ctx,
                   const struct pipe_draw_info *dinfo,
                   unsigned drawid_offset,
                   const struct pipe_draw_indirect_info *dindirect,
                   const struct pipe_draw_start_count_bias *draws,
                   unsigned num_draws)
{
   static uint32_t draw_count = 0;
   uint32_t count = draws->count;
   uint32_t offset = draws->start;
   uint32_t draw_mode = 0x0;
   uint32_t *cmd_buf;

   if (dinfo->index_size > 0) {
      draw_mode = 0x1;
      offset = draws->index_bias;
   }

   if (dindirect && dindirect->count_from_stream_output)
      count = dindirect->count_from_stream_output->buffer_size / ctx->element_state->elements[0].base.src_stride;

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   if (ctx->info.texture_dirty)
      MWV207_EMIT(cmd_buf, 0xE03, (ALIGN_BIT((uint32_t) (ENABLE), 0x4) & (0x10)));

   MWV207_EMIT(cmd_buf, 0xE27, draw_count++);

   MWV207_EMIT(cmd_buf, 0xE02, (ALIGN_BIT((uint32_t) (0x5), 0x0) & (0x1F)) |
      (ALIGN_BIT((uint32_t) (0x7), 0x8) & (0x1F00)));

   MWV207_EMIT(cmd_buf, 0xF00, (ALIGN_BIT((uint32_t) (0x5), 0x0) & (0x1F)) |
      (ALIGN_BIT((uint32_t) (0x7), 0x8) & (0x1F00)));

   if (dindirect) {
      struct mwv207_resource *buffer = mwv207_resource(dindirect->buffer);
      *cmd_buf++ = (ALIGN_BIT(0x10, 0x1B) & (0xF8000000)) |
         (ALIGN_BIT((uint32_t) (draw_mode), 0x8) & (0x100)) |
         (ALIGN_BIT((uint32_t) ((dinfo->instance_count != 1)), 0xE) & (0x4000)) |
         (ALIGN_BIT((uint32_t) (prim_mode[dinfo->mode]), 0x0) & (0xF));
      mwv207_add_relocs(ctx, &cmd_buf, 0x0,
         buffer->bo, buffer->offset + dindirect->offset, 0x0);
   } else {
      *cmd_buf++ = (ALIGN_BIT(0xC, 0x1B) & (0xF8000000)) |
         (ALIGN_BIT((uint32_t) (draw_mode), 0x14) & (0x700000)) |
         (ALIGN_BIT((uint32_t) ((dinfo->instance_count & 0xffff)), 0x0) & (0xFFFF)) |
         (ALIGN_BIT((uint32_t) (prim_mode[dinfo->mode]), 0x10) & (0xF0000));
      *cmd_buf++ = (ALIGN_BIT((uint32_t) (count), 0x0) & (0xFFFFFF)) |
         (ALIGN_BIT((uint32_t) ((dinfo->instance_count >> 16)), 0x18) & (0xFF000000));
      *cmd_buf++ = offset;
      *cmd_buf++ = 0;
   }

   MWV207_EMIT(cmd_buf, 0xE03, (ALIGN_BIT((uint32_t) (ENABLE), 0x0) & (0x1)));
   MWV207_SNAP_TO_PAGE(cmd_buf);

   mwv207_end_record(ctx->cmd, &cmd_buf);
}

static void
mwv207_config_other(struct mwv207_context *ctx)
{
   uint32_t *cmd_buf;
   uint32_t ra_control = 0;

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   if (ctx->dirty & (MWV207_DIRTY_FRAMEBUFFER | MWV207_DIRTY_BLEND)) {
      for (int i = 0; i < ctx->fb_state.nr_cbufs; i++) {
         struct mwv207_surface *surface;
         uint32_t render_num = ctx->info.cbufs_map[i];

         if (!ctx->fb_state.cbufs[i])
            continue;

         surface = mwv207_surface(ctx->fb_state.cbufs[i]);

         if (render_num != 0)
            MWV207_EMIT(cmd_buf, (0x5240 + (render_num - 1)),
               surface->peInfo.pixel_config_ex | ctx->blend_state->blend_configex[i]);

         if (ctx->info.pixel_record[i])
            MWV207_EMIT(cmd_buf, (0x5240 + (ctx->info.pixel_record[i] - 1)),
               surface->peInfo.pixel_config_ex | ctx->blend_state->blend_configex[i]);
      }

      if (ctx->info.rast_samples > 1)
         MWV207_EMIT(cmd_buf, 0x415,
            ((ALIGN_BIT((uint32_t) (ctx->blend_state->blend_state.alpha_to_coverage), 0xC) & (0x1000)) | (~(0x1000 | 0x8000))));
   }

   if (ctx->dirty & (MWV207_DIRTY_FRAMEBUFFER | MWV207_DIRTY_ZSA | MWV207_DIRTY_SHADER)) {
      if (ctx->fb_state.zsbuf) {
         bool ps_modify_z = ctx->shader_states[PIPE_SHADER_FRAGMENT] &&
            ctx->shader_states[PIPE_SHADER_FRAGMENT]->current->ps_depth_out_reg >= 0;

         struct mwv207_surface *surface = mwv207_surface(ctx->fb_state.zsbuf);

         if (ctx->info.subsamplez_dirty)
            ra_control |= (ALIGN_BIT(0x0, 0x1E) & (0xC0000000));
         else
            ra_control |= (ALIGN_BIT(0x1, 0x1E) & (0xC0000000));

         if (surface->base.format == PIPE_FORMAT_S8_UINT || ps_modify_z)
            ra_control |= (ALIGN_BIT((uint32_t) (0), 0x0) & (0x1));
         else {
            ra_control |= (ALIGN_BIT((uint32_t) (true), 0x0) & (0x1)) |
                          (ALIGN_BIT(0x3, 0x4) & (0xF0));
         }

         if (ps_modify_z)
            ra_control |= (ALIGN_BIT(0x1, 0x1C) & (0x30000000));
         else
            ra_control |= (ALIGN_BIT(0x0, 0x1C) & (0x30000000));

         if (ps_modify_z) {
            MWV207_EMIT(cmd_buf, 0x500, surface->peInfo.depth_config & ctx->zsa->depth_config &
               ((ALIGN_BIT(0x0, 0x18) & (0x1000000)) | (~(0x1000000 | 0x2000000))) &
               ((ALIGN_BIT((uint32_t) ((ctx->fb_state.nr_cbufs == 0)), 0x14) & (0x100000)) | (~(0x100000 | 0x200000))) &
               ((ALIGN_BIT((uint32_t) (DISABLE), 0x10) & (0x10000)) | (~(0x10000 | 0x20000))));
         } else {
            MWV207_EMIT(cmd_buf, 0x500, surface->peInfo.depth_config & ctx->zsa->depth_config &
               ((ALIGN_BIT((uint32_t) ((ctx->fb_state.nr_cbufs == 0)), 0x14) & (0x100000)) | (~(0x100000 | 0x200000))) &
               ((ALIGN_BIT(0x1, 0x18) & (0x1000000)) | (~(0x1000000 | 0x2000000))));
         }

         MWV207_EMIT(cmd_buf, 0x382, ra_control |
            (ALIGN_BIT((uint32_t) (DISABLE), 0x2) & (0x4)) |
            (ALIGN_BIT((uint32_t) (true), 0x1A) & (0x4000000)) |
            (ALIGN_BIT((uint32_t) (true), 0x18) & (0x1000000)));
      } else {
         struct mwv207_shader *fs = ctx->shader_states[PIPE_SHADER_FRAGMENT]->current;
         bool use_w = fs->useFragCoord;

         if (ctx->shader_states[PIPE_SHADER_GEOMETRY])
            use_w |= ctx->shader_states[PIPE_SHADER_GEOMETRY]->current->gs_use_layer;

         MWV207_EMIT(cmd_buf, 0x382, ra_control |
            (ALIGN_BIT(0x1, 0x1E) & (0xC0000000)) |
            (ALIGN_BIT((uint32_t) (DISABLE), 0x1C) & (0x30000000)) |
            (ALIGN_BIT((uint32_t) (use_w), 0x1A) & (0x4000000)) |
            (ALIGN_BIT((uint32_t) (fs->useFragCoord), 0x18) & (0x1000000)));
      }
   }

   if (ctx->dirty & (MWV207_DIRTY_ZSA | MWV207_DIRTY_STENCIL_REF | MWV207_DIRTY_RASTERIZER)) {
      MWV207_EMIT(cmd_buf, 0x506, ctx->zsa->stencil_op[ctx->rast_state->base.front_ccw]);

      MWV207_EMIT(cmd_buf, 0x507, ctx->zsa->stencil_config[ctx->rast_state->base.front_ccw] |
         ctx->stencil_ref.stencil_reference[ctx->rast_state->base.front_ccw]);

      MWV207_EMIT(cmd_buf, 0x528,
         ctx->stencil_ref.stencil_back_reference[ctx->rast_state->base.front_ccw]);

      MWV207_EMIT(cmd_buf, 0x52E,
         ctx->zsa->stencil_back_config[ctx->rast_state->base.front_ccw]);
   }

   mwv207_end_record(ctx->cmd, &cmd_buf);
}

static bool
mwv207_update_shader(struct mwv207_context *ctx)
{
   struct pipe_sampler_view *view;
   struct mwv207_sampler_view *sampler_view;
   struct mwv207_sampler *sampler;
   struct mwv207_resource *res;
   const struct util_format_description *desc;
   bool result = false;

   ctx->info.texture_dirty = false;
   ctx->info.z32_shadow = false;
   ctx->info.tg4_lower = false;

   for (int stage = 0; stage < PIPE_SHADER_TYPES; stage++)
      for (int i = 0; i < ctx->num_samplers[stage]; i++) {
         view = ctx->sampler_views[stage][i];
         sampler = ctx->samplers[stage][i];

         if (!sampler || !view)
            continue;

         sampler_view = mwv207_sampler_view(view);

         if (ctx->info.tex_128bit && sampler_view->tex_desc.tex_128bit)
            mwv207_split_128bit((struct pipe_context *)ctx, sampler_view->base.texture, PIPE_FORMAT_NONE);

         res = mwv207_resource(sampler_view->base.texture);

         if (res->ts.enable)
            mwv207_disable_tile(ctx , res);

         if (res->src_dirty)
            ctx->info.texture_dirty = true;

         res->src_dirty = false;

         if (sampler->state.compare_mode != PIPE_TEX_COMPARE_NONE &&
             view->texture->format == PIPE_FORMAT_Z32_FLOAT)
            ctx->info.z32_shadow = true;

         desc = util_format_description(view->texture->format);
         if (desc->channel[0].size == 32)
            ctx->info.tg4_lower = true;
      }

   if (ctx->info.linear_outmode) {
      for (int i = 0; i < ctx->fb_state.nr_cbufs; i++) {
         if (!ctx->fb_state.cbufs[i])
            continue;

         res = mwv207_resource(ctx->fb_state.cbufs[i]->texture);

         if (res->mode == MWV207_SUPERTILED)
            mwv207_disable_tile(ctx, res);
      }
   }

   /* FIX(jm9100-oss): 顶点着色器原来只在 create/bind shader_state 时选一次，之后
    * 采样器视图变化（例如绑定了 RGBA32F 这类 128 位纹理、需要把 4x32 位纹素取样
    * 拆成两次 64 位取样）不会让它重编译 → VS 里 texelFetch 只拿到 x/y。
    * 这里让 VS 每次绘制前与 FS 一样重新核对 shader key。 */
   if (ctx->shader_states[PIPE_SHADER_VERTEX])
      result |= mwv207_set_current_shader(ctx, ctx->shader_states[PIPE_SHADER_VERTEX]);

   result |= mwv207_set_current_shader(ctx, ctx->shader_states[PIPE_SHADER_FRAGMENT]);

   if (ctx->shader_states[PIPE_SHADER_GEOMETRY])
      result |= mwv207_set_current_shader(ctx, ctx->shader_states[PIPE_SHADER_GEOMETRY]);

   if (ctx->shader_states[PIPE_SHADER_TESS_CTRL])
      result |= mwv207_set_current_shader(ctx, ctx->shader_states[PIPE_SHADER_TESS_CTRL]);

   if (ctx->info.subsamplez_dirty)
      result |= true;

   return result;
}

static void
mwv207_config_dual_blend(struct pipe_context *pctx,
                         const struct pipe_draw_info *dinfo)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   if (ctx->info.dsbi.is_dual) {
      struct pipe_sampler_view sampler_view;
      struct pipe_surface *src_surface = ctx->fb_state.cbufs[0];
      struct pipe_resource res_templ;
      struct pipe_resource *texture_res;
      struct pipe_blit_info blt_info = {};

      memset(&res_templ, 0, sizeof(struct pipe_resource));
      res_templ.target = PIPE_TEXTURE_2D;
      res_templ.width0 = src_surface->width;
      res_templ.height0 = src_surface->height;
      res_templ.depth0 = 1;
      res_templ.format = src_surface->format;
      res_templ.array_size = 1;
      res_templ.usage = PIPE_USAGE_DEFAULT;
      texture_res = pctx->screen->resource_create(pctx->screen, &res_templ);
      blt_info.mask = PIPE_MASK_RGBA;
      blt_info.dst.resource = texture_res;
      blt_info.dst.format = src_surface->format;
      blt_info.dst.level = 0;
      blt_info.dst.box.x = 0;
      blt_info.dst.box.y = 0;
      blt_info.dst.box.depth = 1;
      blt_info.dst.box.width = src_surface->width;
      blt_info.dst.box.height = src_surface->height;
      blt_info.src.resource = src_surface->texture;
      blt_info.src.format = src_surface->format;
      blt_info.src.level = 0;
      blt_info.src.box.x = 0;
      blt_info.src.box.y = 0;
      blt_info.src.box.depth = 1;
      blt_info.src.box.width = src_surface->width;
      blt_info.src.box.height = src_surface->height;
      pctx->blit(pctx, &blt_info);

      memset(&sampler_view, 0, sizeof(struct pipe_sampler_view));
      util_blitter_default_src_texture(ctx->blitter, &sampler_view, texture_res, 0);
      sampler_view.format = src_surface->format;

      ctx->info.dsbi.y_invert = 0;
      ctx->info.dsbi.sampler_index = ctx->num_samplers[PIPE_SHADER_FRAGMENT];
      struct pipe_sampler_view *src_view;
      src_view = pctx->create_sampler_view(pctx, texture_res, &sampler_view);
      pctx->set_sampler_views(pctx, PIPE_SHADER_FRAGMENT,
                              ctx->num_sampler_views[PIPE_SHADER_FRAGMENT], 1,
                              0, true, &src_view);
      void *sampler_state = mwv207_get_sampler_state(ctx);
      pctx->bind_sampler_states(pctx, PIPE_SHADER_FRAGMENT,
                                ctx->num_samplers[PIPE_SHADER_FRAGMENT],
                                1, &sampler_state);
      pipe_resource_reference(&texture_res, NULL);
   }
}

static void
mwv207_config_render_piepline(struct mwv207_context *ctx,
                              const struct pipe_draw_info *dinfo,
                              unsigned drawid_offset,
                              const struct pipe_draw_indirect_info *dindirect,
                              const struct pipe_draw_start_count_bias *draws,
                              unsigned num_draws)
{
   mwv207_config_dual_blend(&ctx->base, dinfo);

   if (mwv207_update_shader(ctx)) {
      ctx->dirty |= MWV207_DIRTY_CONSTBUF;
      ctx->dirty |= MWV207_DIRTY_SHADER;
   }

   if (ctx->dirty & (MWV207_DIRTY_SHADER | MWV207_DIRTY_CONSTBUF)) {
      mwv207_config_uniform(ctx, PIPE_SHADER_VERTEX);
      mwv207_config_uniform(ctx, PIPE_SHADER_TESS_CTRL);
      mwv207_config_uniform(ctx, PIPE_SHADER_TESS_EVAL);
      mwv207_config_uniform(ctx, PIPE_SHADER_GEOMETRY);
      mwv207_config_uniform(ctx, PIPE_SHADER_FRAGMENT);
   }

   mwv207_config_vertex(ctx, dinfo, draws);

   if (ctx->dirty & MWV207_DIRTY_VIEWPORT)
      mwv207_config_viewport(ctx);

   if ((ctx->dirty & MWV207_DIRTY_VIEWPORT) ||
       (ctx->dirty & MWV207_DIRTY_FRAMEBUFFER) ||
       (ctx->dirty & MWV207_DIRTY_SCISSOR) ||
       (ctx->dirty & MWV207_DIRTY_RASTERIZER))
      mwv207_config_scissor(ctx);

   if (ctx->dirty & (MWV207_DIRTY_SAMPLER_VIEWS | MWV207_DIRTY_SAMPLERS))
      for (int i = 0; i < PIPE_SHADER_COMPUTE; i++)
         if (ctx->num_sampler_views[i])
            mwv207_config_sampler(ctx, i);

   if (ctx->dirty & MWV207_DIRTY_FRAMEBUFFER)
      mwv207_config_framebuffer(ctx);

   if (ctx->dirty & MWV207_DIRTY_RASTERIZER)
      mwv207_config_rasterizer(ctx);

   if (ctx->dirty & MWV207_DIRTY_FRAMEBUFFER)
      mwv207_config_multisamples(ctx);

   if (ctx->dirty & MWV207_DIRTY_SAMPLE_MASK)
      mwv207_config_sample_mask(ctx);

   if (ctx->dirty & MWV207_DIRTY_ZSA)
      mwv207_config_zsa(ctx);

   mwv207_config_other(ctx);

   if (ctx->dirty & MWV207_DIRTY_BLEND)
      mwv207_config_blend(ctx);

   if (ctx->dirty & MWV207_DIRTY_BLEND_COLOR)
      mwv207_config_blendColor(ctx);

   if (ctx->dirty & MWV207_DIRTY_SHADER) {
      mwv207_config_vs(ctx);
      mwv207_config_tcs(ctx);
      mwv207_config_tes(ctx);
      mwv207_config_gs(ctx);
   }

   if (ctx->dirty & (MWV207_DIRTY_SHADER | MWV207_DIRTY_FRAMEBUFFER)) {
      mwv207_config_fs(ctx);
      mwv207_config_link(ctx);
   }

   if ((ctx->dirty & MWV207_DIRTY_STREAM_OUT) ||
       ctx->stream_out.num_targets != 0)
      mwv207_config_stream_out(ctx, false);

   mwv207_config_draw(ctx, dinfo, drawid_offset, dindirect, draws, num_draws);

   if (ctx->stream_out.num_targets != 0)
      mwv207_config_stream_out(ctx, true);

   ctx->dirty = 0;

}

void
mwv207_draw_vbo(struct pipe_context *pctx,
                const struct pipe_draw_info *dinfo,
                unsigned drawid_offset,
                const struct pipe_draw_indirect_info *indirect,
                const struct pipe_draw_start_count_bias *draws,
                unsigned num_draws)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   struct mwv207_rasterizer_state *rast_state = ctx->rast_state;

   /* 条件渲染（GL_NV_conditional_render）：谓词不成立就整条绘制都不发。
    * FIX(jm9100-oss) */
   if (!mwv207_check_render_cond(pctx))
      return;

   if (num_draws > 1) {
      util_draw_multi(pctx, dinfo, drawid_offset, indirect, draws, num_draws);
      return;
   }

   if (!indirect && (!draws[0].count || !dinfo->instance_count))
      return;

   if (dinfo->mode == MESA_PRIM_QUADS ||
       dinfo->mode == MESA_PRIM_QUAD_STRIP ||
       dinfo->mode == MESA_PRIM_POLYGON) {
      if (!u_trim_pipe_prim(dinfo->mode, (unsigned *)&(draws[0].count)))
         return;

      util_primconvert_save_rasterizer_state(ctx->primconvert, &rast_state->base);
      util_primconvert_draw_vbo(ctx->primconvert,
         dinfo, drawid_offset, indirect, draws, num_draws);
      return;
   }

   mwv207_config_render_piepline(ctx, dinfo, drawid_offset, indirect, draws, num_draws);

   mwv207_submit(ctx, NULL, false);

   if (unlikely(ctx->ws->debug & 0x40))
      pctx->flush(pctx, NULL, 0);
}
