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
#include "util/u_blitter.h"
#include "util/u_surface.h"
#include "util/u_pack_color.h"
#include "util/u_transfer.h"
#include "util/format/u_format.h"

#include "mwv207_context.h"
#include "mwv207_blit.h"
#include "mwv207_query.h"   /* mwv207_check_render_cond() — FIX(jm9100-oss) */

static bool
mwv207_can_blit(const struct pipe_blit_info *info)
{
   struct mwv207_resource *src = mwv207_resource(info->src.resource);
   struct mwv207_resource *dst = mwv207_resource(info->dst.resource);
   struct mwv207_blit_desc src_blit_desc, dst_blit_desc;

   if (info->mask != util_format_get_mask(dst->base.format))
      return false;

   if (info->scissor_enable || info->alpha_blend)
      return false;

   if (info->src.box.x < 0 || info->src.box.width < 0)
      return false;

   if (info->dst.box.x < 0 || info->dst.box.width < 0)
      return false;

   if (MAX2(info->src.box.width, -info->src.box.width) != MAX2(info->dst.box.width, -info->dst.box.width))
      return false;

   if (MAX2(info->src.box.height, -info->src.box.height) != MAX2(info->dst.box.height, -info->dst.box.height))
      return false;

   if (info->src.box.depth != info->dst.box.depth)
      return false;

   mwv207_get_blit_desc(src->base.format, &src_blit_desc);
   mwv207_get_blit_desc(dst->base.format, &dst_blit_desc);
   if (src_blit_desc.fake_format || dst_blit_desc.fake_format) {
      if (info->src.format != info->dst.format && src->base.format != dst->base.format)
         return false;

      if (src->sample_info.is_msaa && !dst->sample_info.is_msaa && !util_format_is_pure_integer(src->base.format)) {
         printf("color resolve unimplemented, may be need to use cpuBlit\n");
         return false;
      }
   }

   if (util_format_get_blocksize(src->base.format) > 8)
      return false;

   if (util_format_get_blocksize(dst->base.format) > 8)
      return false;

   if (util_format_get_blocksize(src->base.format) != util_format_get_blocksize(info->src.format) ||
       util_format_get_blocksize(dst->base.format) != util_format_get_blocksize(info->dst.format))
      return false;

   return true;
}

static void
mwv207_set_image_task(struct mwv207_context *ctx,
                      uint32_t **commandBuffer,
                      struct mwv207_task_info info)
{
   uint32_t cluster_mask = ctx->ws->cluster_mask;
   uint32_t cluster_count = ctx->ws->cluster_count;
   uint32_t used_cluster_mask = 0;
   uint32_t set_count = 0;

   assert(info.src_sampleInfo.xscale != 0 && info.dst_sampleInfo.xscale != 0 &&
         info.src_sampleInfo.yscale !=0 && info.dst_sampleInfo.yscale != 0);

   uint32_t leftWidth = info.width * info.src_sampleInfo.xscale;
   uint32_t average_width = leftWidth;
   uint32_t task_width = leftWidth;
   uint32_t task_height = info.height * info.src_sampleInfo.yscale;
   uint32_t src_startx = info.src_x * info.src_sampleInfo.xscale;
   uint32_t src_starty = info.src_y * info.src_sampleInfo.yscale;
   uint32_t dst_startx = info.dst_x * info.dst_sampleInfo.xscale;
   uint32_t dst_starty = info.dst_y * info.dst_sampleInfo.yscale;

   if (info.width > 64) {
      average_width = align(leftWidth / cluster_count, info.src_sampleInfo.xscale);
      task_width = leftWidth - (cluster_count - 1) * average_width;
   }

   while (cluster_mask) {
      if (cluster_mask & (1 << set_count)) {

         uint32_t srcEndX = align(src_startx + task_width, 64);
         task_width = MIN2(srcEndX - src_startx, leftWidth);
         task_width = align(task_width, info.src_sampleInfo.xscale);

         if (leftWidth > 0) {
            MWV207_EMIT(*commandBuffer, 0x50CE,
               (ALIGN_BIT((uint32_t) ((1 << set_count)), 0x0) & (0xFF)));

            MWV207_EMIT(*commandBuffer, 0x5005,
               (ALIGN_BIT((uint32_t) (src_startx), 0x0) & (0xFFFF)) |
               (ALIGN_BIT((uint32_t) (src_starty), 0x10) & (0xFFFF0000)));

            MWV207_EMIT(*commandBuffer, 0x500B,
               (ALIGN_BIT((uint32_t) (dst_startx), 0x0) & (0xFFFF)) |
               (ALIGN_BIT((uint32_t) (dst_starty), 0x10) & (0xFFFF0000)));

            MWV207_EMIT(*commandBuffer, 0x500C,
               (ALIGN_BIT((uint32_t) (task_width), 0x0) & (0xFFFF)) |
               (ALIGN_BIT((uint32_t) (task_height), 0x10) & (0xFFFF0000)));

            leftWidth -= task_width;
            src_startx += task_width;
            dst_startx += (task_width / info.src_sampleInfo.xscale) * info.dst_sampleInfo.xscale;
            task_width = average_width;
            used_cluster_mask |= (1 << set_count);
         }
         cluster_mask &= ~(1 << set_count);
      }
      set_count++;
   }

   if (set_count > 0)
      MWV207_EMIT(*commandBuffer, 0x50CE,
         (ALIGN_BIT((uint32_t) (used_cluster_mask), 0x0) & (0xFF)));

}

static void
mwv207_set_buffer_task(struct mwv207_context *ctx,
                       uint32_t **commandBuffer,
                       struct mwv207_region_info info)
{
   uint32_t srcAddr = info.src_box.x;
   uint32_t dstAddr = info.dst_box.x;
   uint32_t cluster_mask = ctx->ws->cluster_mask;
   uint32_t cluster_count = ctx->ws->cluster_count;
   uint64_t average_size = info.src_box.width / cluster_count;
   uint64_t copy_size = info.src_box.width - (cluster_count - 1) * average_size;
   uint32_t set_count = 0;

   MWV207_EMIT(*commandBuffer, 0x5004, 0);
   MWV207_EMIT(*commandBuffer, 0x5008, 0);

   while (cluster_mask) {
      if (cluster_mask & (1 << set_count)) {
         MWV207_EMIT(*commandBuffer, 0x50CE,
            (ALIGN_BIT((uint32_t) ((1 << set_count)), 0x0) & (0xFF)));
         mwv207_add_relocs(ctx, commandBuffer, 0x5000,
            info.src->bo, srcAddr, 0x0);
         mwv207_add_relocs(ctx, commandBuffer, 0x5006,
            info.dst->bo, dstAddr, 0x1);
         MWV207_EMIT(*commandBuffer, 0x5015, (uint32_t)copy_size);

         srcAddr += copy_size;
         dstAddr += copy_size;
         copy_size = average_size;
         cluster_mask &= ~(1 << set_count);
      }
      set_count++;
   }

   if (set_count > 0)
      MWV207_EMIT(*commandBuffer, 0x50CE,
         (ALIGN_BIT((uint32_t) (ctx->ws->cluster_mask), 0x0) & (0xFF)));
}

enum pipe_error
mwv207_fill_buffer(struct mwv207_context *ctx,
                   struct mwv207_bo *mem,
                   uint32_t offset,
                   uint32_t size,
                   uint32_t data)
{
   const uint32_t stride = 4096;
   uint32_t config, dst_configex, src_configex, blit_config;
   uint32_t width, height, extra_size;
   uint32_t *cmd_buf;
   uint32_t fill_value[2] = {0};
   struct mwv207_task_info task_info;

   fill_value[0] = data;

   if (size <= stride) {
      width  = (uint32_t)(size / sizeof(uint32_t));
      height = 1;
      extra_size = 0;
   } else {
      width  = stride / sizeof(uint32_t);
      height = (uint32_t)(size / stride);
      extra_size = (uint32_t)(size % stride);
   }

   config = (ALIGN_BIT((uint32_t) (0x0), 0x1B) & (0x18000000)) |
      (ALIGN_BIT((uint32_t) (stride), 0x0) & (0x1FFFFF)) |
      (ALIGN_BIT((uint32_t) (DISABLE), 0x1D) & (0x20000000)) |
      (ALIGN_BIT((uint32_t) (DISABLE), 0x1E) & (0x40000000)) |
      (ALIGN_BIT((uint32_t) (DISABLE), 0x1F) & (0x80000000)) |
      (ALIGN_BIT((uint32_t) (0x6), 0x15) & (0x7E00000)) ;

   dst_configex = (ALIGN_BIT(0x0, 0x14) & (0x100000)) |
      (ALIGN_BIT((uint32_t) (DISABLE), 0x0) & (0x1)) |
      (ALIGN_BIT((uint32_t) (DISABLE), 0x11) & (0x20000)) |
      (ALIGN_BIT((uint32_t) (DISABLE), 0x1) & (0x2));

   src_configex = (ALIGN_BIT(0x0, 0x13) & (0x80000)) |
      (ALIGN_BIT((uint32_t) (DISABLE), 0x0) & (0x1)) |
      (ALIGN_BIT((uint32_t) (DISABLE), 0x11) & (0x20000)) |
      (ALIGN_BIT((uint32_t) (DISABLE), 0x1) & (0x2));

   blit_config = (ALIGN_BIT((uint32_t) (0x3), 0x7) & (0x380));

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   mwv207_flush_cache(&cmd_buf, CACHE_WRITABLE);

   MWV207_BLIT_LOCK(ctx, cmd_buf);

   mwv207_add_relocs(ctx, &cmd_buf, 0x5000,
      mem, offset, 0x0);
   MWV207_EMIT(cmd_buf, 0x5002, config);
   MWV207_EMIT(cmd_buf, 0x5003, src_configex);

   mwv207_add_relocs(ctx, &cmd_buf, 0x5006,
      mem, offset, 0x1);
   MWV207_EMIT(cmd_buf, 0x5009, config);
   MWV207_EMIT(cmd_buf, 0x500A, dst_configex);

   MWV207_EMIT(cmd_buf, 0x5019, blit_config);

   MWV207_EMITS(cmd_buf, 0x5011, 2, fill_value);
   MWV207_EMIT(cmd_buf, 0x5013, 0xFFFFFFFF);

   task_info.src_x = 0;
   task_info.src_y = 0;
   task_info.dst_x = 0;
   task_info.dst_y = 0;
   task_info.width = width;
   task_info.height = height;
   task_info.src_sampleInfo.xscale = 1;
   task_info.src_sampleInfo.yscale = 1;
   task_info.dst_sampleInfo.xscale = 1;
   task_info.dst_sampleInfo.yscale = 1;

   mwv207_set_image_task(ctx, &cmd_buf, task_info);

   mwv207_excute_blit(&cmd_buf, 0x1);

   if (extra_size > 0) {
      offset += (height * stride);

      mwv207_add_relocs(ctx, &cmd_buf, 0x5006,
         mem, offset, 0x1);
      mwv207_add_relocs(ctx, &cmd_buf, 0x5000,
         mem, offset, 0x0);

      MWV207_EMIT(cmd_buf, 0x5005,
         (ALIGN_BIT((uint32_t) (0), 0x0) & (0xFFFF)) |
         (ALIGN_BIT((uint32_t) (0), 0x10) & (0xFFFF0000)));

      MWV207_EMIT(cmd_buf, 0x500B,
         (ALIGN_BIT((uint32_t) (0), 0x0) & (0xFFFF)) |
         (ALIGN_BIT((uint32_t) (0), 0x10) & (0xFFFF0000)));

      MWV207_EMIT(cmd_buf, 0x500C,
         (ALIGN_BIT((uint32_t) (extra_size / sizeof(uint32_t)), 0x0) & (0xFFFF)) |
         (ALIGN_BIT((uint32_t) (1), 0x10) & (0xFFFF0000)));

      mwv207_excute_blit(&cmd_buf, 0x1);
   }

   MWV207_BLIT_UNLOCK(ctx, cmd_buf);

   mwv207_end_record(ctx->cmd, &cmd_buf);

   return PIPE_OK;
}

enum pipe_error
mwv207_copy_buffer(struct mwv207_context *ctx, struct mwv207_region_info info)
{
   enum pipe_error result = PIPE_OK;
   uint32_t *cmd_buf;

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   mwv207_flush_cache(&cmd_buf, CACHE_WRITABLE);

   MWV207_BLIT_LOCK(ctx, cmd_buf);

   mwv207_set_buffer_task(ctx, &cmd_buf, info);

   mwv207_excute_blit(&cmd_buf, 0x3);

   MWV207_BLIT_UNLOCK(ctx, cmd_buf);

   mwv207_end_record(ctx->cmd, &cmd_buf);

   return result;
}

enum pipe_error
mwv207_update_buffer(struct mwv207_context *ctx, struct mwv207_region_info info)
{
   enum pipe_error result = PIPE_OK;
   uint32_t *cmd_buf;

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   mwv207_flush_cache(&cmd_buf, CACHE_WRITABLE);

   MWV207_BLIT_LOCK(ctx, cmd_buf);

   mwv207_set_buffer_task(ctx, &cmd_buf, info);

   mwv207_excute_blit(&cmd_buf, 0x3);

   MWV207_BLIT_UNLOCK(ctx, cmd_buf);

   mwv207_end_record(ctx->cmd, &cmd_buf);

   return result;
}

static enum pipe_error
mwv207_clear_image(struct mwv207_context *ctx, struct mwv207_clear_info info)
{
   enum pipe_error result = PIPE_OK;
   struct mwv207_resource *dst = info.res;
   struct mwv207_level_info *levelInfo = &dst->level_infos[info.tex_info.first_level];

   uint32_t offset;
   uint32_t tile_offset = 0;
   uint32_t src_configex, dst_configex;
   uint32_t clear_values[2] = {0}, clear_masks[2] = {0};
   uint32_t *cmd_buf;
   bool fast_clear = false;
   struct mwv207_task_info task_info;

   src_configex = levelInfo->src_configex;
   dst_configex = levelInfo->dst_configex;

   offset = dst->offset + levelInfo->offset + info.tex_info.first_layer * levelInfo->layer_size;

   mwv207_get_clearval(dst->base.format, info, clear_values, clear_masks);

   if (dst->ts.bo && dst->base.array_size == 1 && dst->base.depth0 == 1) {
      struct mwv207_tile_info *ts = &dst->ts;
      uint32_t index = info.tex_info.first_level * dst->base.array_size + info.tex_info.first_layer;
      tile_offset = ts->layer_offset * index;

      if (clear_masks[0] == 0xFFFFFFFF && clear_masks[1] == 0xFFFFFFFF) {
         ts->clear_values[0] = clear_values[0];
         ts->clear_values[1] = clear_values[1];
         fast_clear = true;
         dst->ts.enable = true;
         dst->linear_output = false;
         src_configex |= levelInfo->src_compress;
         dst_configex |= levelInfo->dst_compress;
      } else if (dst->ts.enable)
         mwv207_disable_tile(ctx, dst);
   }

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   mwv207_flush_cache(&cmd_buf, CACHE_WRITABLE);

   MWV207_BLIT_LOCK(ctx, cmd_buf);

   MWV207_EMIT(cmd_buf, 0x5019, levelInfo->blit_config);

   MWV207_EMIT(cmd_buf, 0x5009, levelInfo->config);
   MWV207_EMIT(cmd_buf, 0x500A, dst_configex |
      (ALIGN_BIT((uint32_t) (fast_clear), 0x0) & (0x1)) |
      (ALIGN_BIT((uint32_t) (dst->sample_info.is_msaa ? ENABLE : DISABLE), 0x1E) & (0x40000000)));

   mwv207_add_relocs(ctx, &cmd_buf, 0x5006,
      dst->bo, offset, 0x1);

   MWV207_EMIT(cmd_buf, 0x5002, levelInfo->config);
   MWV207_EMIT(cmd_buf, 0x5003, src_configex |
      (ALIGN_BIT((uint32_t) (fast_clear), 0x0) & (0x1)));

   mwv207_add_relocs(ctx, &cmd_buf, 0x5000,
      dst->bo, offset, 0x1);

   task_info.src_x = info.box.x;
   task_info.src_y = info.box.y;
   task_info.dst_x = info.box.x;
   task_info.dst_y = info.box.y;
   task_info.width = info.box.width;
   task_info.height = info.box.height;
   task_info.src_sampleInfo = dst->sample_info;
   task_info.dst_sampleInfo = dst->sample_info;

   mwv207_set_image_task(ctx, &cmd_buf, task_info);

   MWV207_EMITS(cmd_buf, 0x5011, 2, clear_values);
   MWV207_EMITS(cmd_buf, 0x5013, 2, clear_masks);

   if (fast_clear) {
      mwv207_add_relocs(ctx, &cmd_buf, 0x5004,
         dst->ts.bo, tile_offset, 0x1);
      mwv207_add_relocs(ctx, &cmd_buf, 0x5008,
         dst->ts.bo, tile_offset, 0x1);
      MWV207_EMITS(cmd_buf, 0x500F, 2, clear_values);
      MWV207_EMITS(cmd_buf, 0x500D, 2, clear_values);
   }

   mwv207_excute_blit(&cmd_buf, 0x1);

   MWV207_BLIT_UNLOCK(ctx, cmd_buf);

   mwv207_end_record(ctx->cmd, &cmd_buf);

   return result;
}

static enum pipe_error
mwv207_copy_layer(struct mwv207_context *ctx, struct mwv207_region_info info)
{
   enum pipe_error result = PIPE_OK;
   struct mwv207_resource *src = info.src;
   struct mwv207_resource *dst = info.dst;
   struct mwv207_level_info *src_level = &src->level_infos[info.src_info.first_level];
   struct mwv207_level_info *dst_level = &dst->level_infos[info.dst_info.first_level];

   bool fast_clear = false;
   uint32_t *cmd_buf = NULL;
   uint32_t src_offset, dst_offset;
   uint32_t src_tile_offset = 0;
   uint32_t src_configex = 0, dst_configex = 0, blit_config = 0;
   struct mwv207_task_info task_info;
   struct mwv207_blit_desc src_blit_desc, dst_blit_desc;
   uint32_t src_msaa_mode = mwv207_get_blit_msaa_mode(src->sample_info.xscale, src->sample_info.yscale);
   uint32_t dst_msaa_mode = mwv207_get_blit_msaa_mode(dst->sample_info.xscale, dst->sample_info.yscale);

   src_offset = (src->offset + src_level->offset + info.src_info.first_layer * src_level->layer_size);

   dst_offset = (dst->offset + dst_level->offset + info.dst_info.first_layer * dst_level->layer_size);

   src_configex = src_level->src_configex | src_level->src_compress;
   dst_configex = dst_level->dst_configex;

   if (dst->ts.enable && src != dst)
      mwv207_disable_tile(ctx, dst);

   if (src->ts.bo && !info.src_info.first_level && src->ts.enable) {
      src_tile_offset = src->ts.layer_offset * info.src_info.first_layer;
      fast_clear = (src_tile_offset == 0);
      src_configex |= (ALIGN_BIT((uint32_t) (fast_clear), 0x0) & (0x1));
   }

   mwv207_get_blit_desc(info.src_format, &src_blit_desc);
   mwv207_get_blit_desc(info.dst_format, &dst_blit_desc);
   if (util_format_is_compressed(info.src_format))
      mwv207_get_blit_desc(info.src_format, &dst_blit_desc);
   else if (util_format_is_compressed(info.dst_format))
      mwv207_get_blit_desc(info.dst_format, &src_blit_desc);

   blit_config = (ALIGN_BIT((uint32_t) (src_blit_desc.sample_mode), 0xC) & (0x1000));

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   mwv207_flush_cache(&cmd_buf, CACHE_WRITABLE);

   MWV207_BLIT_LOCK(ctx, cmd_buf);

   MWV207_EMIT(cmd_buf, 0x5019, blit_config);

   if (src->linear_output) {
      MWV207_EMIT(cmd_buf, 0x5002,
         (ALIGN_BIT((uint32_t) (src_level->stride), 0x0) & (0x1FFFFF)) |
         (ALIGN_BIT((uint32_t) (src_blit_desc.format), 0x15) & (0x7E00000)) |
         (ALIGN_BIT((uint32_t) (src_msaa_mode), 0x1B) & (0x18000000)));
      MWV207_EMIT(cmd_buf, 0x5003,
         (ALIGN_BIT((uint32_t) ((util_format_get_blocksize(src->base.format) == 8)), 0x13) & (0x80000)) |
         (ALIGN_BIT((uint32_t) (src_blit_desc.srgb), 0x8) & (0x100)) |
         (ALIGN_BIT((uint32_t) ((CACHE_MODE == 0x1)), 0x11) & (0x20000)));
   } else if (src_blit_desc.fake_format) {
      src_blit_desc.format = dst_blit_desc.format;
      src_blit_desc.swizzle = dst_blit_desc.swizzle;
      MWV207_EMIT(cmd_buf, 0x5002,
         (ALIGN_BIT((uint32_t) (src_level->stride), 0x0) & (0x1FFFFF)) |
         (ALIGN_BIT((uint32_t) (src_blit_desc.format), 0x15) & (0x7E00000)) |
         (ALIGN_BIT((uint32_t) ((src->mode == MWV207_SUPERTILED)), 0x1D) & (0x20000000)) |
         (ALIGN_BIT((uint32_t) ((src->mode == MWV207_SUPERTILED)), 0x1E) & (0x40000000)));
      MWV207_EMIT(cmd_buf, 0x5003, src_configex |
         (ALIGN_BIT((uint32_t) (src_blit_desc.srgb), 0x8) & (0x100)));

   } else {
      MWV207_EMIT(cmd_buf, 0x5002,
         (ALIGN_BIT((uint32_t) (src_level->stride), 0x0) & (0x1FFFFF)) |
         (ALIGN_BIT((uint32_t) (src_blit_desc.format), 0x15) & (0x7E00000)) |
         (ALIGN_BIT((uint32_t) (src_msaa_mode), 0x1B) & (0x18000000)) |
         (ALIGN_BIT((uint32_t) ((src->mode == MWV207_SUPERTILED)), 0x1D) & (0x20000000)) |
         (ALIGN_BIT((uint32_t) ((src->mode == MWV207_SUPERTILED)), 0x1E) & (0x40000000)));
     MWV207_EMIT(cmd_buf, 0x5003, src_configex |
         (ALIGN_BIT((uint32_t) (src_blit_desc.srgb), 0x8) & (0x100)));

   }

   MWV207_EMIT(cmd_buf, 0x502F, src_blit_desc.swizzle |
      (dst_blit_desc.swizzle << 0xC));

   MWV207_EMIT(cmd_buf, 0x5028,
      (ALIGN_BIT((uint32_t) (4), 0x0) & (0xFFFF)) |
      (ALIGN_BIT((uint32_t) (4), 0x10) & (0xFFFF0000)));

   MWV207_EMIT(cmd_buf, 0x5027,
      (ALIGN_BIT((uint32_t) (0x40), 0x0) & (0xFFFF)) |
      (ALIGN_BIT((uint32_t) (0x40), 0x10) & (0xFFFF0000)));

   if (fast_clear) {
      mwv207_add_relocs(ctx, &cmd_buf, 0x5004,
         src->ts.bo, src_tile_offset, 0x0);
      MWV207_EMIT(cmd_buf, 0x500D, src->ts.clear_values[0]);
   }

   mwv207_add_relocs(ctx, &cmd_buf, 0x5000,
      src->bo, src_offset, 0x0);

   MWV207_EMIT(cmd_buf, 0x5009,
      (ALIGN_BIT((uint32_t) (dst_level->stride), 0x0) & (0x1FFFFF)) |
      (ALIGN_BIT((uint32_t) (dst_blit_desc.format), 0x15) & (0x7E00000)) |
      (ALIGN_BIT((uint32_t) (dst_msaa_mode), 0x1B) & (0x18000000)) |
      (ALIGN_BIT((uint32_t) ((dst->mode == MWV207_SUPERTILED)), 0x1D) & (0x20000000)) |
      (ALIGN_BIT((uint32_t) ((dst->mode == MWV207_SUPERTILED)), 0x1E) & (0x40000000)));
   MWV207_EMIT(cmd_buf, 0x500A, dst_configex |
      (ALIGN_BIT((uint32_t) (dst_blit_desc.srgb), 0x8) & (0x100)) |
      (ALIGN_BIT(0x1, 0x16) & (0x400000)) |
      (ALIGN_BIT((uint32_t) (info.flip_y), 0x13) & (0x80000)));

   mwv207_add_relocs(ctx, &cmd_buf, 0x5006,
      dst->bo, dst_offset, 0x1);

   if (info.flip_y)
      MWV207_EMIT(cmd_buf, 0x50CF,
         (ALIGN_BIT((uint32_t) (info.dst_box.y), 0x0) & (0xFFFF)) |
         (ALIGN_BIT((uint32_t) ((info.dst_box.y + info.dst_box.height)), 0x10) & (0xFFFF0000)));
   if (util_format_is_compressed(info.src_format) || util_format_is_compressed(info.dst_format)) {
      task_info.src_x = util_format_get_stride(info.src_format, info.src_box.x);
      task_info.src_y = util_format_get_nblocksy(info.src_format, info.src_box.y);
      task_info.dst_x = util_format_get_stride(info.dst_format, info.dst_box.x);
      task_info.dst_y = util_format_get_nblocksy(info.dst_format, info.dst_box.y);
      task_info.width = util_format_get_stride(info.src_format, info.src_box.width);
      task_info.height = util_format_get_nblocksy(info.src_format, info.src_box.height);
   }

   task_info.src_x = info.src_box.x;
   task_info.src_y = info.src_box.y;
   task_info.dst_x = info.dst_box.x;
   task_info.dst_y = info.dst_box.y;
   task_info.width = info.src_box.width;
   task_info.height = info.src_box.height;
   task_info.src_sampleInfo = src->sample_info;
   task_info.dst_sampleInfo = dst->sample_info;

   mwv207_set_image_task(ctx, &cmd_buf, task_info);

   MWV207_EMIT(cmd_buf, 0x5016, ~0U);
   MWV207_EMIT(cmd_buf, 0x5017, ~0U);

   mwv207_excute_blit(&cmd_buf, 0x2);

   MWV207_BLIT_UNLOCK(ctx, cmd_buf);

   mwv207_end_record(ctx->cmd, &cmd_buf);

   dst->src_dirty = true;

   if (dst->linear)
      dst->need_decompress = true;

   return result;
}

static enum pipe_error
mwv207_mipmap(struct mwv207_context *ctx, struct mwv207_region_info info)
{
   enum pipe_error result = PIPE_OK;

   struct mwv207_resource *src = info.src;
   struct mwv207_level_info *src_level = &src->level_infos[info.src_info.first_level];

   uint32_t *cmd_buf;
   uint32_t offset, src_tile_offset = 0;
   uint32_t lod_stride, lod_offset;
   uint32_t blit_config, src_configex;

   struct mwv207_level_info lod_level;
   struct mwv207_blit_desc src_blit_desc, dst_blit_desc;

   offset  = (src->offset + src_level->offset + info.src_info.first_layer * src_level->layer_size);

   src_configex = src_level->src_configex | src_level->src_compress;
   if (src->ts.bo && info.src_info.first_level == 0)
      src_tile_offset = src->ts.layer_offset * info.src_info.first_layer;

   mwv207_get_blit_desc(src->base.format, &src_blit_desc);
   mwv207_get_blit_desc(src->base.format, &dst_blit_desc);

   blit_config = (ALIGN_BIT((uint32_t) (src_blit_desc.sample_mode), 0xC) & (0x1000));

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   mwv207_flush_cache(&cmd_buf, CACHE_WRITABLE);

   MWV207_BLIT_LOCK(ctx, cmd_buf);

   MWV207_EMIT(cmd_buf, 0x5019, blit_config);

   MWV207_EMIT(cmd_buf, 0x5016, ~0U);
   MWV207_EMIT(cmd_buf, 0x5017, ~0U);

   mwv207_add_relocs(ctx, &cmd_buf, 0x5000,
         src->bo, offset, 0x0);
   if (src->ts.bo && info.src_info.first_level == 0)
      mwv207_add_relocs(ctx, &cmd_buf, 0x5004,
         src->ts.bo, src_tile_offset, 0x0);
   else
      MWV207_EMIT(cmd_buf, 0x5004, 0);

   MWV207_EMIT(cmd_buf, 0x5002, src_level->config);
   MWV207_EMIT(cmd_buf, 0x5003, src_configex);

   MWV207_EMIT(cmd_buf, 0x5006, 0);
   MWV207_EMIT(cmd_buf, 0x5008, 0);
   MWV207_EMIT(cmd_buf, 0x5009, src_level->config);
   MWV207_EMIT(cmd_buf, 0x500A, src_level->dst_configex);

   MWV207_EMIT(cmd_buf, 0x502F, src_blit_desc.swizzle |
      (dst_blit_desc.swizzle << 0xC));

   MWV207_EMIT(cmd_buf, 0x500C,
      (ALIGN_BIT((uint32_t) (src_level->width), 0x0) & (0xFFFF)) |
      (ALIGN_BIT((uint32_t) (src_level->height), 0x10) & (0xFFFF0000)));

   MWV207_EMIT(cmd_buf, 0x5028,
      (ALIGN_BIT((uint32_t) (4), 0x0) & (0xFFFF)) |
      (ALIGN_BIT((uint32_t) (4), 0x10) & (0xFFFF0000)));

   MWV207_EMIT(cmd_buf, 0x5027,
      (ALIGN_BIT((uint32_t) (0x40), 0x0) & (0xFFFF)) |
      (ALIGN_BIT((uint32_t) (0x40), 0x10) & (0xFFFF0000)));

   for (int i = info.src_info.first_level + 1; i <= info.src_info.last_level; i++)
   {
      lod_level = src->level_infos[i];
      lod_stride = lod_level.stride;
      lod_offset = (src->offset + lod_level.offset + info.src_info.first_layer * lod_level.layer_size);

      mwv207_add_relocs(ctx, &cmd_buf, 0x5030 + i - 1,
         src->bo, lod_offset, 0x1);
      MWV207_EMIT(cmd_buf, 0x50C0 + i - 1, lod_stride);
   }

   MWV207_EMIT(cmd_buf, 0x502C,
      ((ALIGN_BIT((uint32_t) (info.src_info.level_count), 0x0) & (0xF)) | (~(0xF | 0x10))) &
      ((ALIGN_BIT((uint32_t) (1), 0x5) & (0x20)) | (~(0x20 | 0x40))));
   mwv207_excute_blit(&cmd_buf, 0x6);

   MWV207_BLIT_UNLOCK(ctx, cmd_buf);

   mwv207_end_record(ctx->cmd, &cmd_buf);

   return result;
}

enum pipe_error
mwv207_copy_image(struct mwv207_context *ctx, struct mwv207_region_info info)
{
   for (int i = 0; i < info.src_info.layer_count; i++) {
      mwv207_copy_layer(ctx, info);
      info.src_info.first_layer++;
      info.dst_info.first_layer++;
   }

   return PIPE_OK;
}

static enum pipe_error
mwv207_blit_self_image(struct mwv207_context *ctx, struct mwv207_resource *res)
{
   struct mwv207_region_info info;

   info.dst_info.first_level = 0;
   info.dst_info.last_level  = 0;
   info.dst_info.level_count  = 1;

   info.src = res;
   info.dst = res;

   info.src_format = res->base.format;
   info.dst_format = res->base.format;

   info.src_info.first_level = 0;
   info.src_info.first_layer = 0;
   info.src_info.layer_count = res->base.array_size;
   info.dst_info.first_level = 0;
   info.dst_info.first_layer = 0;
   info.dst_info.layer_count = res->base.array_size;

   info.src_box.x = 0;
   info.src_box.y = 0;
   info.src_box.z = 0;
   info.src_box.width  = res->base.width0;
   info.src_box.height = res->base.height0;
   info.src_box.depth  = 1;
   info.dst_box = info.src_box;
   info.flip_y = false;

   for (int i = 0; i < info.src_info.layer_count; i++) {

      mwv207_copy_layer(ctx, info);
      info.src_info.first_layer++;
      info.dst_info.first_layer++;
   }

   return PIPE_OK;
}

static uint32_t
mwv207_get_filler_bpp(enum pipe_format format)
{
   switch (util_format_get_blocksizebits(format)) {
   case 8:
      return 0x0;
   case 16:
      return 0x1;
   case 32:
      return 0x2;
   case 64:
      return 0x3;
   case 128:
      return 0x4;
   default:

      return 0x2;
   }
}

static void
mwv207_till_fill(struct mwv207_context *ctx, struct mwv207_resource *src)
{
   struct mwv207_level_info *src_level = &src->level_infos[0];
   struct mwv207_tile_info *ts = &src->ts;

   uint32_t *cmd_buf;
   uint32_t cluster_mask = ctx->ws->cluster_mask;
   uint32_t cluster_count = ctx->ws->cluster_count;
   uint32_t set_count = 0;
   uint32_t offset = 0, tile_offset = 0;
   uint32_t tile_count, average_tiles, cache_size;
   uint32_t general_config = 0;

   general_config =
      (ALIGN_BIT((uint32_t) ((CACHE_MODE == 0x1)), 0xA) & (0x400)) |
      (ALIGN_BIT((uint32_t) (0x1), 0xB) & (0x800)) |
      (ALIGN_BIT((uint32_t) (mwv207_get_filler_bpp(src->base.format)), 0xF) & (0x38000));

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   mwv207_flush_cache(&cmd_buf, CACHE_WRITABLE);

   MWV207_BLIT_LOCK(ctx, cmd_buf);

   MWV207_EMIT(cmd_buf, 0x5019, general_config);

   MWV207_EMITS(cmd_buf, 0x500F, 2, ts->clear_values);

   if (CACHE_MODE == 0x1)
      cache_size = 256;
   else
      cache_size = 128;

   tile_count = src_level->size / cache_size;

   average_tiles = align(tile_count / cluster_count, 4);
   tile_count = tile_count - average_tiles * (cluster_count - 1);

   while (cluster_mask) {
      if (cluster_mask & (1 << set_count)) {
         MWV207_EMIT(cmd_buf, 0x50CE,
            (ALIGN_BIT((uint32_t) ((1 << set_count)), 0x0) & (0xFF)));
         mwv207_add_relocs(ctx, &cmd_buf, 0x5006,
            src->bo, offset, 0x1);
         mwv207_add_relocs(ctx, &cmd_buf, 0x5008,
            ts->bo, tile_offset, 0x1);
         MWV207_EMIT(cmd_buf, 0x501A, tile_count);

         offset += tile_count * cache_size;
         tile_offset += tile_count / 2;
         tile_count = average_tiles;
         cluster_mask &= ~(1 << set_count);
      }
      set_count++;
   }

   if (set_count > 0)
      MWV207_EMIT(cmd_buf, 0x50CE,
         (ALIGN_BIT((uint32_t) (ctx->ws->cluster_mask), 0x0) & (0xFF)));

   mwv207_excute_blit(&cmd_buf, 0x4);

   MWV207_BLIT_UNLOCK(ctx, cmd_buf);

   mwv207_end_record(ctx->cmd, &cmd_buf);
}

void
mwv207_disable_tile(struct mwv207_context *ctx, struct mwv207_resource *res)
{
   if (res->mode == MWV207_LINEAR)
      return;

   if (res->ts.compressed)
      mwv207_blit_self_image(ctx, res);
   else
      mwv207_till_fill(ctx, res);

   res->ts.enable = false;
   res->src_dirty = true;
}

static void
mwv207_fix_box(struct pipe_box *box)
{
   if (box->height < 0) {
      box->y += box->height;
      box->height = -box->height;
   }
}

static bool
mwv207_blit_image(struct mwv207_context *ctx, const struct pipe_blit_info *info)
{
   struct mwv207_resource *src = mwv207_resource(info->src.resource);
   struct mwv207_resource *dst = mwv207_resource(info->dst.resource);

   struct mwv207_region_info blit_info;

   blit_info.src = src;
   blit_info.dst = dst;

   blit_info.src_format = info->src.format;
   blit_info.dst_format = info->dst.format;

   blit_info.src_box = info->src.box;
   blit_info.dst_box = info->dst.box;

   if (blit_info.src_box.height < 0 || blit_info.dst_box.height < 0) {
      blit_info.flip_y = true;
      mwv207_fix_box(&blit_info.src_box);
      mwv207_fix_box(&blit_info.dst_box);
   } else
      blit_info.flip_y = false;

   blit_info.src_info.first_level = info->src.level;
   blit_info.src_info.last_level  = info->src.level;
   blit_info.src_info.level_count  = 1;

   blit_info.src_info.first_layer = info->src.box.z;
   blit_info.src_info.last_layer  = info->src.box.z + info->src.box.depth - 1;
   blit_info.src_info.layer_count  = info->src.box.depth;

   blit_info.dst_info.first_level = info->dst.level;
   blit_info.dst_info.last_level  = info->dst.level;
   blit_info.dst_info.level_count  = 1;

   blit_info.dst_info.first_layer = info->dst.box.z;
   blit_info.dst_info.last_layer  = info->dst.box.z + info->dst.box.depth - 1;
   blit_info.dst_info.layer_count  = info->dst.box.depth;

   for (int i = 0; i < blit_info.src_info.layer_count; i++) {
      mwv207_copy_layer(ctx, blit_info);
      blit_info.src_info.first_layer++;
      blit_info.dst_info.first_layer++;
   }

   mwv207_submit(ctx, NULL, true);

   return true;
}

static bool
mwv207_try_blit128_or_blit96_copy_region(struct pipe_context *ctx, const struct pipe_blit_info *blit)
{
   struct mwv207_resource *src = mwv207_resource(blit->src.resource);
   struct mwv207_resource *dst = mwv207_resource(blit->dst.resource);
   if (util_format_get_blocksize(src->base.format) > 8 &&
       util_format_get_blocksize(dst->base.format) > 8 &&
       util_can_blit_via_copy_region(blit, true, false)) {
      ctx->resource_copy_region(ctx, blit->dst.resource, blit->dst.level,
                                blit->dst.box.x, blit->dst.box.y,
                                blit->dst.box.z,
                                blit->src.resource, blit->src.level,
                                &blit->src.box);
      return true;
   }
   return false;
}

static void
mwv207_blit(struct pipe_context *pctx, const struct pipe_blit_info *info)
{
   struct mwv207_context *ctx = mwv207_context(pctx);

   if (mwv207_can_blit(info)) {
      mwv207_blit_image(ctx, info);
      return;
   }

   if (util_try_blit_via_copy_region(pctx, info, false))
      return;
   if (mwv207_try_blit128_or_blit96_copy_region(pctx, info))
      return;

   if (!util_blitter_is_blit_supported(ctx->blitter, info)) {
      printf("blit unsupported %s -> %s\n",
         util_format_short_name(info->src.resource->format),
         util_format_short_name(info->dst.resource->format));
      return;
   }

   util_blitter_save_vertex_elements(ctx->blitter, ctx->element_state);
   util_blitter_save_vertex_buffers(ctx->blitter, ctx->vertex_buffer, ctx->num_vertex_buffers);
   util_blitter_save_viewport(ctx->blitter, &ctx->viewport);
   util_blitter_save_scissor(ctx->blitter, &ctx->scissor);
   util_blitter_save_rasterizer(ctx->blitter, ctx->rast_state);
   util_blitter_save_depth_stencil_alpha(ctx->blitter, ctx->zsa);
   util_blitter_save_blend(ctx->blitter, ctx->blend_state);
   util_blitter_save_stencil_ref(ctx->blitter, &ctx->stencil_ref.ref);
   util_blitter_save_vertex_shader(ctx->blitter, ctx->shader_states[PIPE_SHADER_VERTEX]);
   util_blitter_save_fragment_shader(ctx->blitter, ctx->shader_states[PIPE_SHADER_FRAGMENT]);
   util_blitter_save_geometry_shader(ctx->blitter, ctx->shader_states[PIPE_SHADER_GEOMETRY]);
   util_blitter_save_tessctrl_shader(ctx->blitter, ctx->shader_states[PIPE_SHADER_TESS_CTRL]);
   util_blitter_save_tesseval_shader(ctx->blitter, ctx->shader_states[PIPE_SHADER_TESS_EVAL]);
   util_blitter_save_fragment_constant_buffer_slot(ctx->blitter, ctx->cb[PIPE_SHADER_FRAGMENT]);
   util_blitter_save_sample_mask(ctx->blitter, ctx->sample_mask, 0);
   util_blitter_save_fragment_sampler_states(ctx->blitter, ctx->num_samplers[PIPE_SHADER_FRAGMENT],
      (void **)ctx->samplers[PIPE_SHADER_FRAGMENT]);
   util_blitter_save_fragment_sampler_views(ctx->blitter, ctx->num_sampler_views[PIPE_SHADER_FRAGMENT],
      ctx->sampler_views[PIPE_SHADER_FRAGMENT]);
   util_blitter_save_so_targets(ctx->blitter, ctx->stream_out.num_targets, ctx->stream_out.targets);
   util_blitter_save_framebuffer(ctx->blitter, &ctx->fb_state);
   util_blitter_blit(ctx->blitter, info);
}

static void
mwv207_clear_render_target(struct pipe_context *pctx,
                           struct pipe_surface *dst,
                           const union pipe_color_union *color,
                           unsigned dstx, unsigned dsty,
                           unsigned width, unsigned height,
                           bool render_condition_enabled)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   struct mwv207_surface *surface = mwv207_surface(dst);
   struct mwv207_resource *res = mwv207_resource(dst->texture);
   struct mwv207_clear_info clear_info;
   res->src_dirty = true;

   /* 条件渲染下，st 通过该参数告诉驱动这次 clear 是否受谓词约束。
    * FIX(jm9100-oss) */
   if (render_condition_enabled && !mwv207_check_render_cond(pctx))
      return;

   if (util_format_get_blocksize(res->base.format) >= 8) {
      util_clear_render_target(pctx, dst, color, dstx, dsty, width, height);

      if (res->res_128_bit)
         mwv207_split_128bit(pctx, dst->texture, PIPE_FORMAT_NONE);
      return;
   }

   clear_info.res = res;
   clear_info.tex_info = surface->info;
   clear_info.box.x = dstx;
   clear_info.box.y = dsty;
   clear_info.box.width = width;
   clear_info.box.height = height;
   clear_info.aspectMask = PIPE_CLEAR_COLOR;
   clear_info.color = *color;

   for (uint32_t num_layer = surface->info.first_layer; num_layer <= surface->info.last_layer; num_layer++) {
     clear_info.tex_info.first_layer = num_layer;
     mwv207_clear_image(ctx, clear_info);
   }
}

static void
mwv207_clear_depth_stencil(struct pipe_context *pctx,
                           struct pipe_surface *dst,
                           unsigned clear_flags,
                           double depth,
                           unsigned stencil,
                           unsigned dstx, unsigned dsty,
                           unsigned width, unsigned height,
                           bool render_condition_enabled)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   struct mwv207_surface *surface = mwv207_surface(dst);
   struct mwv207_resource *res = mwv207_resource(dst->texture);
   struct mwv207_clear_info clear_info;
   res->src_dirty = true;

   /* 条件渲染（见 clear_render_target 同处注释）。FIX(jm9100-oss) */
   if (render_condition_enabled && !mwv207_check_render_cond(pctx))
      return;

   clear_info.res = res;
   clear_info.tex_info = surface->info;
   clear_info.box.x = dstx;
   clear_info.box.y = dsty;
   clear_info.box.width = width;
   clear_info.box.height = height;
   clear_info.aspectMask = clear_flags;
   clear_info.depth = depth;
   clear_info.stencil = stencil;

   for (uint32_t num_layer = surface->info.first_layer; num_layer <= surface->info.last_layer; num_layer++) {
      clear_info.tex_info.first_layer = num_layer;
      mwv207_clear_image(ctx, clear_info);
   }
}

static void
mwv207_clear(struct pipe_context *pctx,
             unsigned buffers,
             const struct pipe_scissor_state *scissor_state,
             const union pipe_color_union *pcolor,
             double depth, unsigned stencil)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   struct pipe_framebuffer_state *fb = &ctx->fb_state;

   if (buffers & PIPE_CLEAR_COLOR) {
      for (unsigned i = 0; i < fb->nr_cbufs; i++) {
         if (!(buffers & (PIPE_CLEAR_COLOR0 << i)) || !fb->cbufs[i])
            continue;

         struct mwv207_surface *cbuf = mwv207_surface(fb->cbufs[i]);
         struct mwv207_resource *res = mwv207_resource(cbuf->base.texture);
         struct mwv207_level_info levelInfo = res->level_infos[cbuf->info.first_level];
         if (scissor_state) {
            pctx->clear_render_target(pctx, fb->cbufs[i], pcolor,
               scissor_state->minx, scissor_state->miny,
               scissor_state->maxx, scissor_state->maxy, false);
         } else {
            pctx->clear_render_target(pctx, fb->cbufs[i], pcolor,
               0, 0, levelInfo.width, levelInfo.height, false);
         }
      }
   }

   if (buffers & PIPE_CLEAR_DEPTHSTENCIL && fb->zsbuf) {
      struct mwv207_surface *zbuf = mwv207_surface(fb->zsbuf);
      struct mwv207_resource *res = mwv207_resource(zbuf->base.texture);
      struct mwv207_level_info levelInfo = res->level_infos[zbuf->info.first_level];
      if (scissor_state) {
         pctx->clear_depth_stencil(pctx, fb->zsbuf, (buffers & ~PIPE_CLEAR_COLOR),
            depth, stencil,
            scissor_state->minx, scissor_state->miny,
            scissor_state->maxx, scissor_state->maxy, false);
      } else {
         pctx->clear_depth_stencil(pctx, fb->zsbuf, (buffers & ~PIPE_CLEAR_COLOR),
            depth, stencil, 0, 0, levelInfo.width, levelInfo.height, false);
      }
   }

   mwv207_submit(ctx, NULL, false);

   ctx->dirty |= MWV207_DIRTY_FRAMEBUFFER;
}

static bool
mwv207_gen_mipmap(struct pipe_context *pctx,
                  struct pipe_resource *resource,
                  enum pipe_format format,
                  uint base_level, uint last_level,
                  uint first_layer, uint last_layer)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   struct mwv207_resource *res = mwv207_resource(resource);
   struct mwv207_region_info mipmap_info;
   struct mwv207_blit_desc blt_desc = {0};
   int j = first_layer;

   mwv207_get_blit_desc(res->base.format, &blt_desc);
   if (blt_desc.fake_format)
      return false;

   mipmap_info.src = res;
   mipmap_info.src_info.first_level = base_level;
   mipmap_info.src_info.last_level  = last_level;
   mipmap_info.src_info.level_count = last_level - base_level + 1;

   mipmap_info.src_info.layer_count  = 1;

   if (resource->target == PIPE_TEXTURE_3D) {
      for (int i = last_level; i > base_level; i--) {
         mipmap_info.src_info.last_level  = i;
         mipmap_info.src_info.level_count = i - base_level + 1;
         for (; j < util_num_layers(resource, i); j++) {
            mipmap_info.src_info.first_layer = j;
            mipmap_info.src_info.last_layer  = j + 1;
            mwv207_mipmap(ctx, mipmap_info);
         }
      }
   } else {
      for (int i = first_layer; i <= last_layer; i++) {
         mipmap_info.src_info.first_layer = i;
         mipmap_info.src_info.last_layer  = i + 1;
         mwv207_mipmap(ctx, mipmap_info);
      }
   }

   mwv207_submit(ctx, NULL, true);

   return true;
}

void
mwv207_blit_resource_box(struct pipe_context *pctx, struct pipe_resource *dst,
                         struct pipe_resource *src, int level,
                         const struct pipe_box *box)

{
   struct pipe_blit_info blit = {};
   blit.mask = util_format_get_mask(dst->format);
   blit.filter = PIPE_TEX_FILTER_NEAREST;
   blit.src.resource = src;
   blit.src.format = util_format_linear(src->format);
   blit.src.box = *box;
   blit.dst.resource = dst;
   blit.dst.format = util_format_linear(dst->format);
   blit.dst.box = *box;

   blit.src.level = blit.dst.level = level;

   mwv207_blit_image((struct mwv207_context *)pctx, &blit);
}

static void
mwv207_resource_copy_region(struct pipe_context *pctx,
                            struct pipe_resource *pdst,
                            unsigned dst_level, unsigned dstx, unsigned dsty, unsigned dstz,
                            struct pipe_resource *psrc,
                            unsigned src_level, const struct pipe_box *src_box)
{
   struct mwv207_resource *dst = mwv207_resource(pdst);
   struct mwv207_resource *src = mwv207_resource(psrc);
   struct mwv207_context *ctx = mwv207_context(pctx);
   struct mwv207_region_info copy_info;
   dst->src_dirty = true;

   copy_info.src = src;
   copy_info.dst = dst;

   copy_info.src_format = util_format_linear(psrc->format);
   copy_info.dst_format = util_format_linear(pdst->format);

   if (psrc->target == PIPE_BUFFER && pdst->target == PIPE_BUFFER) {
      copy_info.src_box.x = src_box->x;
      copy_info.src_box.width = src_box->width;
      copy_info.dst_box.x = dstx;
      mwv207_copy_buffer(ctx, copy_info);
   } else if (psrc->target == pdst->target &&
      util_format_get_blocksize(src->base.format) < 8) {
      copy_info.src_box = *src_box;
      copy_info.dst_box = *src_box;

      copy_info.dst_box.x = dstx;
      copy_info.dst_box.y = dsty;
      copy_info.dst_box.z = dstz;

      copy_info.src_info.first_level = src_level;
      copy_info.src_info.level_count = 1;
      copy_info.src_info.first_layer = src_box->z;
      copy_info.src_info.layer_count = src_box->depth;

      copy_info.dst_info.first_level = dst_level;
      copy_info.dst_info.level_count = 1;
      copy_info.dst_info.first_layer = dstz;
      copy_info.dst_info.layer_count = src_box->depth;
      copy_info.flip_y = false;

      if (src->ts.enable)
         mwv207_disable_tile(ctx, src);

      mwv207_copy_image(ctx, copy_info);
   } else {
      util_resource_copy_region(pctx, pdst, dst_level, dstx, dsty, dstz,
         psrc, src_level, src_box);

      if (dst->res_128_bit)
         mwv207_split_128bit(pctx, pdst, PIPE_FORMAT_NONE);
      return;
   }

   mwv207_submit(ctx, NULL, true);

}

void mwv207_blit_init(struct pipe_context *pctx)
{
   pctx->blit  = mwv207_blit;
   pctx->clear = mwv207_clear;

   pctx->clear_texture       = u_default_clear_texture;
   pctx->clear_buffer        = u_default_clear_buffer;
   pctx->clear_render_target = mwv207_clear_render_target;
   pctx->clear_depth_stencil = mwv207_clear_depth_stencil;
   pctx->generate_mipmap     = mwv207_gen_mipmap;

   pctx->transfer_flush_region = u_default_transfer_flush_region;
   pctx->buffer_subdata        = u_default_buffer_subdata;
   pctx->texture_subdata       = u_default_texture_subdata;
   pctx->resource_copy_region  = mwv207_resource_copy_region;
}
