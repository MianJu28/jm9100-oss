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
#include "util/u_transfer.h"
#include "util/format/u_format.h"
#include "drm-uapi/drm_fourcc.h"

#include "mwv207_cmd.h"
#include "mwv207_context.h"
#include "mwv207_screen.h"
#include "mwv207_blit.h"

static bool mwv207_tile_supported_format(enum pipe_format format)
{
   if (util_format_get_blocksize(format) > 8)
      return false;

   if (util_format_is_compressed(format))
      return false;

   /* FIX(jm9100-oss): the 16-bit normalized formats must stay LINEAR.
    * mwv207_transfer_map()/unmap() move texel data through a linear staging
    * resource and copy it into the tiled resource with a 2D blit
    * (mwv207_blit_resource_box()).  For these formats the blit does not
    * preserve the texel layout (the sampled result loses the red channel and
    * the read-back comes back byte-swapped), while the untranslated CPU path
    * used for LINEAR resources is exact.  16-bit normalized formats are rare
    * enough that giving up super-tiling for them is the safe trade. */
   switch (format) {
   case PIPE_FORMAT_R16_UNORM:
   case PIPE_FORMAT_R16_SNORM:
   case PIPE_FORMAT_R16G16_UNORM:
   case PIPE_FORMAT_R16G16_SNORM:
   case PIPE_FORMAT_R16G16B16A16_UNORM:
   case PIPE_FORMAT_R16G16B16A16_SNORM:
      return false;
   default:
      break;
   }

   return true;
}

static enum pipe_error
mwv207_can_tile(struct mwv207_resource *res)
{

   const bool enable_scanout_tile = false;

   if (!mwv207_tile_supported_format(res->base.format))
      return PIPE_ERROR_BAD_INPUT;

   if (res->base.bind & PIPE_BIND_LINEAR)
      return PIPE_ERROR_BAD_INPUT;

   if (res->base.bind & PIPE_BIND_SCANOUT) {
      if (enable_scanout_tile &&
          res->base.array_size == 1 &&
          res->base.depth0 == 1 &&
          res->base.last_level == 0)
         return PIPE_OK;
      else
         return PIPE_ERROR_BAD_INPUT;
   }

   return PIPE_OK;
}

static enum pipe_error
mwv207_get_tile_info(struct mwv207_screen *screen,
                     struct mwv207_resource *res,
                     uint32_t size)
{
   enum pipe_error result = PIPE_OK;
   uint32_t tile_size = 0;
   struct mwv207_tile_info *tile = &res->ts;

   tile->clear_values[0] = 0x0;
   tile->clear_values[1] = 0x0;

   if (CACHE_MODE == 0x1)
      tile->layer_offset = align((res->level_infos[0].size >> 9) / res->base.array_size, 64);
   else
      tile->layer_offset = align((res->level_infos[0].size >> 8) / res->base.array_size, 64);

   tile_size = tile->layer_offset * res->base.array_size * res->base.depth0;

   tile->bo = screen->ws->bo_create(screen->ws, tile_size,
      MWV207_DOMAIN_VRAM, MWV207_FLAG_CPU_ACCESS);

   void *dst = screen->ws->bo_map(screen->ws, tile->bo);
   memset((void *)dst, (uint8_t)0, tile_size);
   screen->ws->bo_unmap(screen->ws, tile->bo);

   tile->compressed_format = mwv207_get_compressed_format(res->base.format);
   tile->compressed = (tile->compressed_format != -1);
   tile->enable = false;
   res->linear_output = false;

   if (res->base.depth0 > 1 || res->base.array_size > 1 || res->base.last_level != 0)
      tile->compressed = false;

   return result;
}

static bool
mwv207_screen_can_create_resource(struct pipe_screen *pscreen,
                                  const struct pipe_resource *templ)
{
   if (templ->nr_samples > 4)
      return false;

   return true;
}

static void
mwv207_set_miptree(struct mwv207_screen *screen,
                   struct mwv207_resource *res,
                   struct winsys_handle *whandle)
{
   uint32_t totalBytes = 0;
   uint32_t width, height, depth;
   uint32_t align_w = 64, align_h = 64, align_size = 256;

   if (whandle == NULL) {
      if (res->base.usage == PIPE_USAGE_STAGING)
         res->mode = MWV207_LINEAR;
      else if (mwv207_can_tile(res) == PIPE_OK)
         res->mode = MWV207_SUPERTILED;
      else
         res->mode = MWV207_LINEAR;
   } else {
      if (whandle->modifier == DRM_FORMAT_MOD_VIVANTE_SUPER_TILED)
         res->mode = MWV207_SUPERTILED;
      else
         res->mode = MWV207_LINEAR;
   }

   if (util_format_is_compressed(res->base.format)) {
      align_w = (util_format_is_etc(res->base.format) || util_format_is_s3tc(res->base.format)) ? 16 : 1;
      align_h = align_size = 1;
   }

   mwv207_get_sample_info(res->base.nr_samples, &res->sample_info.xscale,
      &res->sample_info.yscale, &res->sample_info.nr_samples);

   if (res->sample_info.nr_samples > 1)
      res->sample_info.is_msaa = true;

   for (uint32_t i = 0; i <= res->base.last_level; i++) {
      struct mwv207_level_info *level = &res->level_infos[i];

      width  = u_minify(res->base.width0, i);
      height = u_minify(res->base.height0, i);
      depth  = util_num_layers(&res->base, i);

      level->width = width;
      level->height = height;
      level->depth = depth;

      if (whandle) {
         level->alloced_width = ALIGN_NP2(width * res->sample_info.xscale, 1);
         level->alloced_height = ALIGN_NP2(height * res->sample_info.yscale, 1);
         level->stride = whandle->stride;
         level->offset = whandle->offset + totalBytes;
      } else {
         level->alloced_width = ALIGN_NP2(width * res->sample_info.xscale, align_w);
         level->alloced_height = ALIGN_NP2(height * res->sample_info.yscale, align_h);
         level->stride = util_format_get_stride(res->base.format, level->alloced_width);
         level->offset = totalBytes;
      }

      level->layer_size = level->stride * util_format_get_nblocksy(res->base.format, level->alloced_height);
      level->layer_size = align(level->layer_size, align_size);

      level->size = level->layer_size * depth;
      level->size = align(level->size, 0x1000);
      totalBytes += level->size;

   }

   res->offset = 0;
   res->size = totalBytes;
   if (whandle)
      res->size += whandle->offset;

   if (res->mode != MWV207_LINEAR)
      mwv207_get_tile_info(screen, res, 0);

   mwv207_get_image_info(res);

   res->rg = NULL;
   res->ba = NULL;

   if ((util_format_get_blocksize(res->base.format) == 16) &&
       !util_format_is_compressed(res->base.format))
      res->res_128_bit = true;
   else
      res->res_128_bit = false;

   res->linear = NULL;

   if ((res->base.bind & PIPE_BIND_SCANOUT) && (res->mode == MWV207_SUPERTILED)) {
      struct pipe_resource templ = res->base;
      templ.usage |= PIPE_USAGE_STAGING;
      templ.bind |= PIPE_BIND_LINEAR;
      res->linear = screen->base.resource_create(&screen->base, &templ);
   }
}

static void
mwv207_resource_destroy(struct pipe_screen *pscreen, struct pipe_resource *pres)
{
   struct mwv207_screen *screen = mwv207_screen(pscreen);
   struct mwv207_resource *res = mwv207_resource(pres);

   util_range_destroy(&res->valid_buffer_range);

   screen->ws->bo_reference(screen->ws, &res->bo, NULL);

   if (pres->target != PIPE_BUFFER) {
      if (res->ts.bo)
         screen->ws->bo_reference(screen->ws, &(res->ts.bo), NULL);

      if (res->rg)
         pscreen->resource_destroy(pscreen, res->rg);

      if (res->ba)
         pscreen->resource_destroy(pscreen, res->ba);

      if (res->linear)
         pscreen->resource_destroy(pscreen, res->linear);
   }

   FREE(res);
}

static struct pipe_resource *
resource_create(struct pipe_screen *pscreen,
                const struct pipe_resource *templ,
                struct winsys_handle *whandle,
                unsigned external_usage)
{
   struct mwv207_screen *screen = mwv207_screen(pscreen);
   struct mwv207_resource *res = CALLOC_STRUCT(mwv207_resource);
   enum mwv207_bo_flag flag = MWV207_FLAG_CPU_ACCESS;
   enum mwv207_bo_domain domain = MWV207_DOMAIN_VRAM;

   res->base = *templ;
   res->mode = MWV207_LINEAR;

   pipe_reference_init(&res->base.reference, 1);
   res->base.screen = pscreen;

   if (templ->target == PIPE_BUFFER)
      res->size = templ->width0;
   else
      mwv207_set_miptree(screen, res, whandle);

   if ((res->base.flags & (PIPE_RESOURCE_FLAG_DRV_PRIV<<0)) || res->mode != MWV207_LINEAR)
      flag = MWV207_FLAG_NO_CPU_ACCESS;

   if (res->base.flags & (PIPE_RESOURCE_FLAG_DRV_PRIV<<1))
      flag |= MWV207_FLAG_SUBALLOC;

   if (res->base.flags & (PIPE_RESOURCE_FLAG_DRV_PRIV<<2))
      flag |= MWV207_FLAG_REUSE;

   if (res->base.bind & PIPE_BIND_SHARED)
      flag = MWV207_FLAG_2D_ACCESS;

   if (res->base.usage & PIPE_USAGE_STAGING)
      domain = MWV207_DOMAIN_GTT;

   if (whandle)
      res->bo = screen->ws->bo_from_handle(screen->ws, whandle, res->size);
   else
      res->bo = screen->ws->bo_create(screen->ws, res->size, domain, flag);

   return &res->base;
}

static struct pipe_resource *
mwv207_resource_create(struct pipe_screen *pscreen, const struct pipe_resource *templ)
{
   return resource_create(pscreen, templ, NULL, 0);
}

static struct pipe_resource *
mwv207_resource_from_handle(struct pipe_screen *pscreen,
                            const struct pipe_resource *templ,
                            struct winsys_handle *whandle,
                            unsigned usage)
{
   return resource_create(pscreen, templ, whandle, usage);
}

static bool
mwv207_resource_get_handle(struct pipe_screen *pscreen,
                           struct pipe_context *context,
                           struct pipe_resource *tex,
                           struct winsys_handle *whandle,
                           unsigned usage)
{
   struct mwv207_screen *screen = mwv207_screen(pscreen);
   struct mwv207_resource *res = mwv207_resource(tex);
   bool ret;

   if (whandle->type == WINSYS_HANDLE_TYPE_SHMID)
      return false;

   if (res->base.target != PIPE_BUFFER)
      whandle->stride = res->level_infos[0].stride;

   if (res->linear)
      return pscreen->resource_get_handle(pscreen, context, res->linear, whandle, usage);
   else
      ret = (screen->ws->bo_get_handle(screen->ws, res->bo, whandle) == 0);

   if (ret) {
      if (res->mode == MWV207_SUPERTILED && !res->linear)
         whandle->modifier = DRM_FORMAT_MOD_VIVANTE_SUPER_TILED;
      else
         whandle->modifier = DRM_FORMAT_MOD_LINEAR;
   }

   return ret;
}

static void
mwv207_flush_resource(struct pipe_context *pctx,
                      struct pipe_resource *resource)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   struct mwv207_resource *res = (struct mwv207_resource *)resource;

   if (res->ts.enable && !res->linear)
      mwv207_disable_tile(ctx , res);

   if (res->linear && res->need_decompress) {
      struct mwv207_level_info pLevel = res->level_infos[0];
      struct mwv207_resource *linear = mwv207_resource(res->linear);
      struct mwv207_region_info info;
      info.src = res;
      info.dst = linear;

      info.src_info.first_level = 0;
      info.src_info.first_layer = 0;
      info.src_info.layer_count = 1;

      info.dst_info.first_level = 0;
      info.dst_info.first_layer = 0;
      info.dst_info.layer_count = 1;

      info.src_box.x = 0;
      info.src_box.y = 0;
      info.src_box.z = 0;
      info.src_box.width  = pLevel.width;
      info.src_box.height = pLevel.height;
      info.src_box.depth  = 1;
      info.dst_box = info.src_box;
      info.flip_y = false;

      info.src_format = res->base.format;
      info.dst_format = linear->base.format;

      mwv207_copy_image(ctx, info);

      res->need_decompress = false;
   }
}

void
mwv207_wait_resource(struct pipe_context *pctx,
                     struct pipe_resource *resource)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   struct mwv207_resource *res = (struct mwv207_resource *)resource;
   mwv207_submit(ctx, NULL, true);
   ctx->ws->bo_wait(ctx->ws, res->bo, OS_TIMEOUT_INFINITE);
}

static void *
mwv207_transfer_map(struct pipe_context *pctx,
                    struct pipe_resource *pres,
                    unsigned level,
                    unsigned usage,
                    const struct pipe_box *box,
                    struct pipe_transfer **transfer)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   struct mwv207_resource *res = mwv207_resource(pres);

   void *ptr;
   struct mwv207_transfer *trans = slab_alloc(&ctx->transfer_pool);

   if (!trans)
      return NULL;

   memset(trans, 0, sizeof(*trans));
   pipe_resource_reference(&trans->base.resource, pres);

   trans->base.resource = pres;
   trans->base.level = level;
   trans->base.usage = usage;
   trans->base.box = *box;

   if (pres->target == PIPE_BUFFER) {
      if ((usage & PIPE_MAP_READ) ||
           util_ranges_intersect(&res->valid_buffer_range,
                                 box->x,box->x + box->width))
         mwv207_wait_resource(pctx, pres);

      ptr = ctx->ws->bo_map(ctx->ws, res->bo);

      trans->base.stride = 0;
      trans->base.layer_stride = 0;
      ptr = ((uint8_t *)ptr) + res->offset + util_format_get_stride(pres->format, box->x);
   } else {
      if (!(usage & PIPE_MAP_UNSYNCHRONIZED))
         mwv207_wait_resource(pctx, pres);

      if (res->mode != MWV207_LINEAR) {
         struct pipe_resource templ = *pres;
         templ.usage |= PIPE_USAGE_STAGING;
         templ.bind |= PIPE_BIND_LINEAR;
         trans->staging_res = pctx->screen->resource_create(pctx->screen, &templ);

         if (!trans->staging_res)
            return NULL;

         struct mwv207_resource *staging_res = mwv207_resource(trans->staging_res);

         if (usage & PIPE_MAP_READ) {
            mwv207_blit_resource_box(pctx, &staging_res->base, pres, level, box);
            mwv207_wait_resource(pctx, &staging_res->base);
         }

         trans->base.stride = staging_res->level_infos[level].stride;
         trans->base.layer_stride = staging_res->level_infos[level].layer_size;

         ptr = ctx->ws->bo_map(ctx->ws, staging_res->bo);
         ptr = ((uint8_t *)ptr) + staging_res->offset +
               staging_res->level_infos[level].offset +
               util_format_get_stride(staging_res->base.format, box->x) +
               util_format_get_nblocksy(pres->format, box->y) * trans->base.stride +
               box->z * trans->base.layer_stride;

      } else {

         if (res->res_128_bit)
            mwv207_merge_128bit(pctx, pres);

         trans->base.stride = res->level_infos[level].stride;
         trans->base.layer_stride = res->level_infos[level].layer_size;

         ptr = ctx->ws->bo_map(ctx->ws, res->bo);
         ptr = ((uint8_t *)ptr) + res->offset +
               res->level_infos[level].offset +
               util_format_get_stride(pres->format, box->x) +
               util_format_get_nblocksy(pres->format, box->y) * trans->base.stride +
               box->z * trans->base.layer_stride;
      }

      if ((usage & PIPE_MAP_READ) && !(usage & PIPE_MAP_WRITE) &&
          (box->width * box->height >= 1024)) {
         struct pipe_resource templ = *pres;
         templ.usage |= PIPE_USAGE_STAGING;
         templ.bind |= PIPE_BIND_LINEAR;
         templ.flags |= (PIPE_RESOURCE_FLAG_DRV_PRIV<<2);
         struct pipe_resource *pdma = pctx->screen->resource_create(pctx->screen, &templ);
         struct mwv207_resource *dma = mwv207_resource(pdma);

         if (trans->staging_res) {
            mwv207_dma(ctx, trans->staging_res, pdma, level, box);
            pipe_resource_reference(&trans->staging_res, NULL);
            trans->staging_res = pdma;
         } else {
            mwv207_dma(ctx, pres, pdma, level, box);
            trans->staging_res = pdma;
         }

         mwv207_wait_resource(pctx, pdma);

         ptr = ctx->ws->bo_map(ctx->ws, dma->bo);
         ptr = ((uint8_t *)ptr) + dma->offset +
               dma->level_infos[level].offset +
               util_format_get_stride(dma->base.format, box->x) +
               util_format_get_nblocksy(pres->format, box->y) * trans->base.stride +
               box->z * trans->base.layer_stride;;
      }
   }

   *transfer = &trans->base;
   return ptr;
}

/* 上传暂存的延迟释放: 把 staging 资源挂到上下文的小环里, 环满时才等待最老的
 * 一个。这样 GPU 侧 blit 读它的数据不会被下一次上传覆盖 (见 context.h 注释)。 */
void
mwv207_retire_staging(struct pipe_context *pctx, struct pipe_resource *resource)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   struct pipe_resource **slot = &ctx->retired_staging[ctx->retired_staging_next];

   if (*slot) {
      mwv207_wait_resource(pctx, *slot);
      pipe_resource_reference(slot, NULL);
   }
   *slot = resource;
   ctx->retired_staging_next =
      (ctx->retired_staging_next + 1) % ARRAY_SIZE(ctx->retired_staging);
}

static void
mwv207_transfer_unmap(struct pipe_context *pctx, struct pipe_transfer *ptrans)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   struct mwv207_resource *res = mwv207_resource(ptrans->resource);
   struct mwv207_transfer *trans = (struct mwv207_transfer *)ptrans;

   if (trans->staging_res) {
      struct mwv207_resource *staging_res = mwv207_resource(trans->staging_res);

      ctx->ws->bo_unmap(ctx->ws, staging_res->bo);

      if (trans->base.usage & PIPE_MAP_WRITE) {
         struct pipe_resource *keep = NULL;

         mwv207_blit_resource_box(pctx, &res->base, &staging_res->base,
            ptrans->level, &ptrans->box);

         /* 把 staging 的引用转交到"延迟释放"环: 立刻 ignore 会让 pb_cache 把
          * 同一块内存交给下一次上传, 而 GPU 还在读它做本次 blit —— 这正是
          * Firefox chrome 字形/图标被撕裂的原因。 */
         pipe_resource_reference(&keep, &staging_res->base);
         mwv207_retire_staging(pctx, keep);
      }

      pipe_resource_reference(&trans->staging_res, NULL);
   } else {
      ctx->ws->bo_unmap(ctx->ws, res->bo);
      res->src_dirty = true;
   }

   if (res->base.target == PIPE_BUFFER)
      util_range_add(&res->base, &res->valid_buffer_range,
                     ptrans->box.x, ptrans->box.x + ptrans->box.width);

   if (res->linear)
      pctx->flush_resource(pctx, trans->base.resource);

   pipe_resource_reference(&trans->base.resource, NULL);

   slab_free(&ctx->transfer_pool, ptrans);
}

static void
mwv207_flush_region(struct pipe_context *pctx,
                    struct pipe_transfer *ptrans,
                    const struct pipe_box *box)
{
   struct mwv207_resource *res = mwv207_resource(ptrans->resource);

   if (ptrans->resource->target == PIPE_BUFFER)
      util_range_add(&res->base,
                     &res->valid_buffer_range,
                     ptrans->box.x + box->x,
                     ptrans->box.x + box->x + box->width);
}

void
mwv207_context_resource_init(struct pipe_context *pctx)
{
   pctx->buffer_map   = mwv207_transfer_map;
   pctx->buffer_unmap = mwv207_transfer_unmap;
   pctx->texture_map   = mwv207_transfer_map;
   pctx->texture_unmap = mwv207_transfer_unmap;
   pctx->flush_resource = mwv207_flush_resource;
   pctx->transfer_flush_region = mwv207_flush_region;
}

static const uint64_t supported_modifiers[] = {
   DRM_FORMAT_MOD_LINEAR,
   DRM_FORMAT_MOD_VIVANTE_SUPER_TILED,
};

static void
mwv207_screen_query_dmabuf_modifiers(struct pipe_screen *pscreen,
                                     enum pipe_format format, int max,
                                     uint64_t *modifiers,
                                     unsigned int *external_only, int *count)
{
   int i, num_modifiers = 0;

   if (max > ARRAY_SIZE(supported_modifiers))
      max = ARRAY_SIZE(supported_modifiers);

   if (!max) {
      modifiers = NULL;
      max = ARRAY_SIZE(supported_modifiers);
   }

   if (util_format_is_yuv(format) || !mwv207_tile_supported_format(format))
      max = 1;

   for (i = 0; num_modifiers < max; i++) {
      if (modifiers)
         modifiers[num_modifiers] = supported_modifiers[i];

      if (external_only)
         external_only[num_modifiers] = util_format_is_yuv(format) ? 1 : 0;

      num_modifiers++;
   }

   *count = num_modifiers;
}

static struct pipe_resource *
mwv207_resource_create_modifiers(struct pipe_screen *pscreen,
                                 const struct pipe_resource *templat,
                                 const uint64_t *modifiers, int count)
{
   uint64_t modifier = DRM_FORMAT_MOD_INVALID;
   struct pipe_resource templ = *templat;
   int i;

   for (i = 0; i < count; ++i) {
      if (modifiers[i] != DRM_FORMAT_MOD_INVALID)
         modifier = modifiers[i];

      if (modifier == DRM_FORMAT_MOD_VIVANTE_SUPER_TILED)
         break;
   }

   if (modifier == DRM_FORMAT_MOD_INVALID)
      return NULL;

   assert(modifier == DRM_FORMAT_MOD_LINEAR ||
          modifier == DRM_FORMAT_MOD_VIVANTE_SUPER_TILED);

   if (modifier == DRM_FORMAT_MOD_LINEAR)
      templ.bind |= PIPE_BIND_LINEAR;

   return resource_create(pscreen, &templ, NULL, 0);
}

void
mwv207_screen_resource_init(struct pipe_screen *pscreen)
{
   pscreen->can_create_resource = mwv207_screen_can_create_resource;
   pscreen->resource_create  = mwv207_resource_create;
   pscreen->resource_destroy = mwv207_resource_destroy;

   pscreen->resource_get_handle  = mwv207_resource_get_handle;
   pscreen->resource_from_handle = mwv207_resource_from_handle;

   pscreen->query_dmabuf_modifiers = mwv207_screen_query_dmabuf_modifiers;
   pscreen->resource_create_with_modifiers = mwv207_resource_create_modifiers;
}

