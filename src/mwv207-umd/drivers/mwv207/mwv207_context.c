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
#include "util/u_blitter.h"
#include "util/u_helpers.h"
#include "util/u_upload_mgr.h"
#include "util/hash_table.h"
#include "util/u_transfer.h"
#include "util/format/u_format.h"
#include "indices/u_primconvert.h"

#include "mwv207_context.h"
#include "video-codec/mwv207_video_codec.h"
#include "mwv207_shader.h"
#include "mwv207_blit.h"
#include "mwv207_screen.h"
#include "mwv207_query.h"

static bool
mwv207_context_helper_init(struct mwv207_context *ctx)
{
   int prim_hwsupport =
      1 << MESA_PRIM_POINTS |
      1 << MESA_PRIM_LINES |
      1 << MESA_PRIM_LINE_STRIP |
      1 << MESA_PRIM_TRIANGLES |
      1 << MESA_PRIM_TRIANGLE_STRIP |
      1 << MESA_PRIM_TRIANGLE_FAN;

   ctx->primconvert = util_primconvert_create(&ctx->base, prim_hwsupport);

   ctx->blitter = util_blitter_create(&ctx->base);

   ctx->base.stream_uploader = u_upload_create_default(&ctx->base);
   ctx->base.const_uploader = ctx->base.stream_uploader;

   if (!ctx->primconvert || !ctx->blitter)
      return true;

   return false;
}

static void
mwv207_flush(struct pipe_context *pctx,
             struct pipe_fence_handle **pfence,
             enum pipe_flush_flags flags)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   int out_fence_fd = -1;

   for (int i = 0; i < ctx->fb_state.nr_cbufs; i++)
      if (ctx->fb_state.cbufs[i])
         pctx->flush_resource(pctx, ctx->fb_state.cbufs[i]->texture);

   mwv207_submit(ctx, pfence ? &out_fence_fd : NULL, true);

   for (int i = 0; i < ctx->fb_state.nr_cbufs; i++) {
      struct mwv207_surface *surface;

      if (!ctx->fb_state.cbufs[i])
         continue;

      surface = mwv207_surface(ctx->fb_state.cbufs[i]);

      if (surface->pixel_128bit)
         mwv207_merge_128bit(pctx, surface->base.texture);
   }

   /* FIX(jm9100-oss): O5 —— submit 失败时 mwv207_drm_cmdbuf_commit() 会让
    * out_fence_fd 保持 -1（只在 !err 时写该值）。此前这里照样调用
    * fence_create(-1)：该 fence 的 fence_wait() 恒返回 true，调用者会把
    * 一次提交失败当成"已完成"，等于把失败伪装成成功。改为不返回 fence；
    * Mesa 前端对 *fence == NULL 都有判空（st_finish 与 st_manager 的
    * ST_FLUSH_WAIT 分支）。失败本身已在 cmdbuf_commit 里打印。 */
   if (pfence) {
      if (out_fence_fd < 0)
         *pfence = NULL;
      else
         *pfence = ctx->ws->fence_create(ctx->ws, out_fence_fd);
   }
}

static void
mwv207_memory_barrier(struct pipe_context *pctx, uint32_t flags)
{
   if (flags & PIPE_BARRIER_UPDATE)
      return;
   mwv207_flush(pctx, NULL, 0);
}

static void
mwv207_context_destroy(struct pipe_context *pctx)
{
   struct mwv207_context *ctx = mwv207_context(pctx);

   for (unsigned i = 0; i < ARRAY_SIZE(ctx->retired_staging); i++) {
      if (ctx->retired_staging[i]) {
         mwv207_wait_resource(pctx, ctx->retired_staging[i]);
         pipe_resource_reference(&ctx->retired_staging[i], NULL);
      }
   }

   if (ctx->local_memory.buffer) {
      pipe_resource_reference(&ctx->local_memory.buffer, NULL);
      util_copy_shader_buffer(&ctx->local_memory, NULL);
   }

   for (int i = 0; i < PIPE_SHADER_TYPES; i++)
      for (int j = 0; j <  PIPE_MAX_CONSTANT_BUFFERS; j++) {
         if (ctx->cb[i][j].buffer)
            pipe_resource_reference(&ctx->cb[i][j].buffer, NULL);
      }

   for (int i = 0; i < PIPE_SHADER_TYPES; i++)
      for (int j = 0; j <  PIPE_MAX_SHADER_SAMPLER_VIEWS; j++)
         if (ctx->sampler_views[i][j])
            pipe_sampler_view_reference(&ctx->sampler_views[i][j], NULL);

   util_set_vertex_buffers_count(ctx->vertex_buffer,
      &ctx->num_vertex_buffers, NULL, 0, false);

   mwv207_context_query_destroy(pctx);

   util_unreference_framebuffer_state(&ctx->fb_state);

   util_primconvert_destroy(ctx->primconvert);

   u_upload_destroy(pctx->stream_uploader);

   slab_destroy_child(&ctx->transfer_pool);

   util_blitter_destroy(ctx->blitter);

   ctx->ws->cmdbuf_destroy(ctx->ws, ctx->cmd);

   ctx->ws->cmdbuf_destroy(ctx->ws, ctx->dma);

   if (ctx->in_fence_fd >= 0) {
      close(ctx->in_fence_fd);
      ctx->in_fence_fd = -1;
   }

   FREE(ctx);
}

static void
mwv207_create_fence_fd(struct pipe_context *pctx,
                       struct pipe_fence_handle **pfence,
                       int fd,
                       enum pipe_fd_type type)
{
   struct mwv207_context *ctx = mwv207_context(pctx);

   assert(type == PIPE_FD_TYPE_NATIVE_SYNC);

   *pfence = ctx->ws->fence_create(ctx->ws, dup(fd));
}

static void
mwv207_fence_server_sync(struct pipe_context *pctx,
                         struct pipe_fence_handle *fence)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   ctx->ws->fence_accumulate(ctx->ws, &ctx->in_fence_fd, fence);
}

struct pipe_context *
mwv207_context_create(struct pipe_screen *pscreen, void *priv, unsigned flags)
{
   struct mwv207_screen *screen = mwv207_screen(pscreen);
   struct mwv207_context *ctx = CALLOC_STRUCT(mwv207_context);

   if (!ctx)
      goto fail;

   ctx->base.screen = pscreen;
   ctx->base.priv = priv;

   ctx->ws = screen->ws;
   ctx->dirty = 0;
   ctx->in_fence_fd = -1;
   ctx->sample_mask = 0xffff;
   memset(&ctx->local_memory, 0, sizeof(struct pipe_shader_buffer));

   ctx->base.destroy = mwv207_context_destroy;

   mwv207_context_query_init(&ctx->base);

   mwv207_context_resource_init(&ctx->base);

   mwv207_context_surface_init(&ctx->base);

   mwv207_context_state_init(&ctx->base);

   mwv207_context_shader_init(&ctx->base);

   mwv207_context_sampler_init(&ctx->base);

   mwv207_blit_init(&ctx->base);

   ctx->base.draw_vbo = mwv207_draw_vbo;
   ctx->base.flush = mwv207_flush;
   ctx->base.create_fence_fd = mwv207_create_fence_fd;
   ctx->base.fence_server_sync = mwv207_fence_server_sync;
   ctx->base.memory_barrier = mwv207_memory_barrier;

   mwv207_context_compute_init(&ctx->base);
   mwv207_context_video_codec_init(&ctx->base);

   mwv207_create_cmd(ctx);

   if (mwv207_context_helper_init(ctx))
      goto fail;

   slab_create_child(&ctx->transfer_pool, &screen->transfer_pool);

   return &ctx->base;

fail:

   if (ctx)
      FREE(ctx);

   return NULL;
}
