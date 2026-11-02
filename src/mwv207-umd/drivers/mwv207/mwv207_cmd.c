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
#include "util/format/u_format.h"
#include "mwv207_cmd.h"
#include "mwv207_context.h"

static void
mwv207_start_cmd(struct mwv207_context *ctx)
{
   uint32_t *cmd_buf;
   mwv207_begin_record(ctx->cmd, &cmd_buf);

   MWV207_EMIT(cmd_buf, 0xE00, 0);

   MWV207_EMIT(cmd_buf, 0xE80, 0x10000000);

   MWV207_EMIT(cmd_buf, 0x6101, 0x1);

   MWV207_EMIT(cmd_buf, 0x6B, 0x0fffffff);

   MWV207_EMIT(cmd_buf, 0x1F6, (ALIGN_BIT((uint32_t) (ENABLE), 0x1) & (0x2)));

   MWV207_EMIT(cmd_buf, 0xE42, 0x2);

   MWV207_EMIT(cmd_buf, 0x221, 0x0808);

   MWV207_EMIT(cmd_buf, 0x1F2, true);

   MWV207_EMIT(cmd_buf, 0xE05, 0x1);

   mwv207_end_record(ctx->cmd, &cmd_buf);

   ctx->base.set_active_query_state(&ctx->base, true);
}

static void
mwv207_end_cmd(struct mwv207_context *ctx)
{
   uint32_t *cmd_buf;

   ctx->base.set_active_query_state(&ctx->base, false);

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   mwv207_flush_cache(&cmd_buf, CACHE_ALL);

   mwv207_end_record(ctx->cmd, &cmd_buf);
}

void
mwv207_pipeline_barrier(struct mwv207_context *ctx)
{
   uint32_t *cmd_buf = NULL;
   mwv207_begin_record(ctx->cmd, &cmd_buf);

   MWV207_EMIT(cmd_buf, 0xE03,
      (ALIGN_BIT((uint32_t) (ENABLE), 0x1) & (0x2)) |
      (ALIGN_BIT((uint32_t) (ENABLE), 0x0) & (0x1)) |
      (ALIGN_BIT((uint32_t) (ENABLE), 0x5) & (0x20)) |
      (ALIGN_BIT((uint32_t) (ENABLE), 0xB) & (0x800)) |
      (ALIGN_BIT((uint32_t) (ENABLE), 0xA) & (0x400)));

   MWV207_EMIT(cmd_buf, 0x594,
      (ALIGN_BIT((uint32_t) (ENABLE), 0x0) & (0x1)));

   mwv207_wait_blit(&cmd_buf, ctx->ws->cluster_mask);

   mwv207_end_record(ctx->cmd, &cmd_buf);

   return;
}

enum pipe_error
mwv207_flush_cache(uint32_t **pbuf, enum cache_mask mask)
{
   uint32_t *cmd_buf = *pbuf;
   bool stall = false;
   uint32_t flush_state = 0;

   assert(mask);

   if (mask & (CACHE_TEXTURE_ALL | CACHE_VST_ALL | CACHE_INSTRUCTION))
      stall = true;

   if (stall) {
      MWV207_EMIT(cmd_buf, 0xE02,
         (ALIGN_BIT((uint32_t) (0x1), 0x0) & (0x1F)) |
         (ALIGN_BIT((uint32_t) (0x7), 0x8) & (0x1F00)));

      MWV207_STALL(&cmd_buf,
         (ALIGN_BIT((uint32_t) (0x1), 0x0) & (0x1F)) |
         (ALIGN_BIT((uint32_t) (0x7), 0x8) & (0x1F00)));
   }

   if (mask & CACHE_DEPTH)
      flush_state |= (ALIGN_BIT((uint32_t) (ENABLE), 0x0) & (0x1));

   if (mask & CACHE_COLOR)
      flush_state |= (ALIGN_BIT((uint32_t) (ENABLE), 0x1) & (0x2));

   if (mask & CACHE_SH_L1)
      flush_state |= (ALIGN_BIT((uint32_t) (ENABLE), 0x5) & (0x20)) |
         (ALIGN_BIT((uint32_t) (ENABLE), 0xA) & (0x400)) |
         (ALIGN_BIT((uint32_t) (ENABLE), 0xB) & (0x800));

   if (mask & CACHE_L2)
      flush_state |= (ALIGN_BIT((uint32_t) (ENABLE), 0x6) & (0x40));

   if (mask & CACHE_VERTEX_DATA)
      flush_state |= (ALIGN_BIT((uint32_t) (ENABLE), 0xE) & (0x4000));

   if (mask & CACHE_TEXTURE_L1)
      flush_state |= ((ALIGN_BIT((uint32_t) (ENABLE), 0xC) & (0x1000))  |
         (ALIGN_BIT((uint32_t) (ENABLE), 0xD) & (0x2000)));

   if (flush_state)
      MWV207_EMIT(cmd_buf, 0xE03, flush_state);

   if (mask & CACHE_VST_DATA)
      MWV207_EMIT(cmd_buf, 0xE03, (ALIGN_BIT((uint32_t) (ENABLE), 0x4) & (0x10)));

   if (mask & CACHE_TS) {
      MWV207_EMIT(cmd_buf, 0x594,
         (ALIGN_BIT((uint32_t) (ENABLE), 0x0) & (0x1)));
      stall = true;
   }

   if (mask & CACHE_INSTRUCTION)
      MWV207_EMIT(cmd_buf, 0x22C, 0x1F);

   if (mask & CACHE_TX_DESC)
      MWV207_EMIT(cmd_buf, 0x5311,
         (ALIGN_BIT(0x0, 0x1C) & (0xF0000000)));

   if (mask & CACHE_TFB)
      MWV207_EMIT(cmd_buf, 0x7003, true);

   if (stall) {
      MWV207_EMIT(cmd_buf, 0xE02,
         (ALIGN_BIT((uint32_t) (0x1), 0x0) & (0x1F)) |
         (ALIGN_BIT((uint32_t) (0x7), 0x8) & (0x1F00)));

      MWV207_STALL(&cmd_buf,
         (ALIGN_BIT((uint32_t) (0x1), 0x0) & (0x1F)) |
         (ALIGN_BIT((uint32_t) (0x7), 0x8) & (0x1F00)));
   }

   *pbuf = cmd_buf;

   return PIPE_OK;
}

void
mwv207_wait_idle(uint32_t **pbuf, uint32_t target)
{
   uint32_t value =
      (ALIGN_BIT((uint32_t) (0x1), 0x0) & (0x1F)) |
      (ALIGN_BIT((uint32_t) (target), 0x8) & (0x1F00));
   MWV207_EMIT(*pbuf, 0xE02, value);
   MWV207_STALL(pbuf, value);
}

enum pipe_error
mwv207_dat_add_reloc(struct cmd_dat *dat,
                     uint32_t **ptr,
                     struct mwv207_bo *bo,
                     uint32_t offset,
                     uint32_t flags)
{
   struct drm_mwv207_submit_reloc *reloc;

   int idx = dat->nr_relocs;
   dat->bos[idx] = bo;

   reloc = &dat->relocs[dat->nr_relocs];
   reloc->idx = idx;
   reloc->cmd_offset = (*ptr - dat->data) * sizeof(uint32_t);
   reloc->bo_offset = offset;
   reloc->type   = 0x1;
   reloc->pad    = 0;

   *(*ptr)++ = offset;

   dat->nr_relocs++;

   return PIPE_OK;
}

enum pipe_error
mwv207_add_relocs(struct mwv207_context *ctx,
                  uint32_t **ptr,
                  uint32_t reg,
                  struct mwv207_bo *bo,
                  uint32_t offset,
                  uint32_t flags)
{
   uint32_t idx = ctx->ws->cmdbuf_add_bo(ctx->cmd, bo, flags);
   uint32_t cmd_offset = 0;
   uint32_t type = 0x1;
   uint32_t pad = 0;

   if (reg == 0x0) {
      cmd_offset = (*ptr - (uint32_t *)ctx->cmd->buf) * sizeof(uint32_t);
      *(*ptr)++ = offset;
   } else {
      cmd_offset = (*ptr - (uint32_t *)ctx->cmd->buf + 1) * sizeof(uint32_t);
      MWV207_EMIT(*ptr, reg, offset);
   }

   ctx->ws->cmdbuf_add_relloc(ctx->cmd, idx, cmd_offset, offset, type, pad);

   return 0;
}

enum pipe_error
mwv207_add_dat(struct mwv207_context *ctx,
               uint32_t **ptr,
               uint32_t reg,
               struct cmd_dat *dat)
{
   uint32_t bo_offset = ctx->ws->cmdbuf_add_dat(ctx->cmd, dat);
   uint32_t cmd_offset = (*ptr - (uint32_t *)ctx->cmd->buf + 1) * sizeof(uint32_t);
   uint32_t type = 0x1 | 0x80000000;
   uint32_t pad = 0;

   MWV207_EMIT(*ptr, reg, 0);

   ctx->ws->cmdbuf_add_relloc(ctx->cmd, 0, cmd_offset, bo_offset, type, pad);

   return 0;
}

enum pipe_error
mwv207_dma(struct mwv207_context *ctx,
           struct pipe_resource *psrc,
           struct pipe_resource *pdst,
           unsigned level,
           const struct pipe_box *box)
{
   struct mwv207_cmd *dma =  ctx->dma;
   struct mwv207_resource *src = mwv207_resource(psrc);
   struct mwv207_resource *dst = mwv207_resource(pdst);
   uint64_t *dma_buf = (uint64_t *)(dma->buf + dma->offset);

   uint32_t bo_idx = ctx->ws->cmdbuf_add_bo(ctx->dma, src->bo, 0x0);
   ctx->ws->cmdbuf_add_relloc(ctx->dma, bo_idx, 0, 0, 0x0, 0);
   *dma_buf++ = 0;
   *dma_buf++ = 0;

   if (psrc->target == PIPE_BUFFER) {
      *dma_buf++ = src->bo->offset + src->offset + util_format_get_stride(psrc->format, box->x);
      *dma_buf++ = psrc->width0;
   } else {
      *dma_buf++ = src->bo->offset + src->offset +
         src->level_infos[level].offset +
         util_format_get_stride(src->base.format, box->x) +
         util_format_get_nblocksy(src->base.format, box->y) * src->level_infos[level].stride +
         box->z * src->level_infos[level].layer_size;
      *dma_buf++ = src->level_infos[level].stride;
   }

   bo_idx = ctx->ws->cmdbuf_add_bo(ctx->dma, dst->bo, 0x1);
   ctx->ws->cmdbuf_add_relloc(ctx->dma, bo_idx,
      4 * sizeof(uint64_t), 0, 0x0, 0);
   *dma_buf++ = 0;
   *dma_buf++ = 0;

   if (pdst->target == PIPE_BUFFER) {
      *dma_buf++ = dst->bo->offset + dst->offset + util_format_get_stride(psrc->format, box->x);
      *dma_buf++ = pdst->width0;
   } else {
      *dma_buf++ = dst->bo->offset + dst->offset +
         dst->level_infos[level].offset +
         util_format_get_stride(dst->base.format, box->x) +
         util_format_get_nblocksy(dst->base.format, box->y) * dst->level_infos[level].stride +
         box->z * dst->level_infos[level].layer_size;
      *dma_buf++ = dst->level_infos[level].stride;
   }

   *dma_buf++ = util_format_get_stride(psrc->format, box->width);
   *dma_buf++ = util_format_get_nblocksy(psrc->format, box->height);

   dma->offset = 10 * sizeof(uint64_t);
   ctx->ws->cmdbuf_commit(ctx->ws, ctx->dma, -1, NULL);

   return 0;
}

void
mwv207_create_cmd(struct mwv207_context *ctx)
{
   ctx->cmd = ctx->ws->cmdbuf_create(ctx->ws);
   assert(ctx->cmd);
   ctx->cmd ->engine_type = 0x0;
   mwv207_start_cmd(ctx);
   ctx->dma = ctx->ws->cmdbuf_create(ctx->ws);
   assert(ctx->dma);
   ctx->dma ->engine_type = 0x4;
}

void
mwv207_begin_record(struct mwv207_cmd *cmd, uint32_t **start)
{
   assert(cmd->stamp == NULL);
   cmd->stamp = (uint32_t *)(cmd->buf + cmd->offset);
   *start = cmd->stamp;
}

void
mwv207_end_record(struct mwv207_cmd *cmd, uint32_t **end)
{
   cmd->offset += (*end - cmd->stamp)  * sizeof(uint32_t);
   cmd->stamp = NULL;
}

static bool
check_cmd(struct mwv207_cmd *cmd)
{
   if (cmd->size < (cmd->offset + 4096))
      return false;
   if (cmd->nr_bos > 64)
      return false;
   if (cmd->nr_relocs > 128)
      return false;
   if (cmd->dats.nr_relocs > 128)
      return false;
   return true;
}

void
mwv207_submit(struct mwv207_context *ctx, int *out_fence_fd, bool force)
{
   if (!force && check_cmd(ctx->cmd))
      return;

   mwv207_end_cmd(ctx);

   ctx->ws->cmdbuf_commit(ctx->ws, ctx->cmd, ctx->in_fence_fd, out_fence_fd);

   if (ctx->in_fence_fd >= 0) {
      close(ctx->in_fence_fd);
      ctx->in_fence_fd = -1;
   }

   mwv207_start_cmd(ctx);

   ctx->dirty = MWV207_DIRTY_ALL;
}
