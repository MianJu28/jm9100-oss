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
#ifndef MWV207_CMD_H
#define MWV207_CMD_H

#include "hw/mwv207reg_common.h"
#include "hw/mwv207reg_3d.h"
#include "hw/mwv207reg_texture.h"
#include "hw/mwv207reg_shader.h"

#include "mwv207_drm_winsys.h"

struct mwv207_screen;
struct mwv207_context;

enum cache_mask {
   CACHE_TEXTURE_L2       = 1 << 0,
   CACHE_TEXUTRE_L1       = 1 << 1,
   CACHE_TEXTURE_DESC     = 1 << 2,
   CACHE_VST_L2           = 1 << 3,
   CACHE_VST_L1           = 1 << 4,
   CACHE_VST_DESC         = 1 << 5,
   CACHE_SH_L1            = 1 << 6,
   CACHE_DEPTH            = 1 << 7,
   CACHE_COLOR            = 1 << 8,
   CACHE_TS               = 1 << 9,
   CACHE_INSTRUCTION      = 1 << 10,
   CACHE_TFB              = 1 << 11,
   CACHE_L2               = 1 << 12,
   CACHE_VERTEX_DATA      = 1 << 13,

   CACHE_TEXTURE_DATA     = CACHE_TEXTURE_L2 | CACHE_TEXUTRE_L1,
   CACHE_TEXTURE_ALL      = CACHE_TEXTURE_DATA | CACHE_TEXTURE_DESC,
   CACHE_VST_DATA         = CACHE_VST_L2 | CACHE_VST_L1,
   CACHE_VST_ALL          = CACHE_VST_DATA | CACHE_VST_DESC,
   CACHE_TX_DESC          = CACHE_TEXTURE_DESC | CACHE_VST_DESC,
   CACHE_WRITABLE         = CACHE_SH_L1 | CACHE_DEPTH | CACHE_COLOR | CACHE_TS | CACHE_TFB,
   CACHE_ALL              = ~0,
   CACHE_NONE             = 0,
} ;

#define ALIGN_BIT(data, offset) (((uint32_t) (data)) << (offset))

#define VALUE(reg, field, value)  (ALIGN_BIT((uint32_t) (value), reg##_##field##_Offset) & (reg##_##field##_Mask))

#define NAME(reg, field, value)  (ALIGN_BIT(reg##_##field##_##value, reg##_##field##_Offset) & (reg##_##field##_Mask))

#define MASK(reg, field)  (~(reg##_##field##_Mask | reg##_MASK_##field##_Mask))

#define MASK_VALUE(reg, field, value) \
   (\
    VALUE(reg, field, value) | \
    MASK(reg, field) \
   )

#define MASK_NAME(reg, field, value) \
   (\
    NAME(reg, field, value) | \
    MASK(reg, field) \
   )

#define MWV207_STALL(cmd, value) \
   {\
      *(*cmd)++ = (ALIGN_BIT(0x9, 0x1B) & (0xF8000000));\
      *(*cmd)++ = value; \
   }

#define MWV207_EMIT(cmd, reg, value) \
   {\
      *(cmd)++ = (0x08010000 + reg);\
      *(cmd)++ = value; \
   }

#define MWV207_EMITF(cmd, reg, value) \
   {\
      *(cmd)++ = (0x0C010000 + reg);\
      *(cmd)++ = value; \
   }

#define MWV207_EMITS(cmd, reg, count, value) \
   {\
      *(cmd)++ = (0x08000000 + reg + (count << 16));\
      memcpy(cmd, value, count * sizeof(uint32_t));\
      cmd += count;\
      if (count % 2 == 0) *(cmd)++ = 0xdeadbeef;\
   }

#define MWV207_EMITFS(cmd, reg, count, value) \
   {\
      *(cmd)++ = (0x0C000000 + reg + (count << 16));\
      memcpy(cmd, value, count * sizeof(uint32_t));\
      cmd += count;\
      if (count % 2 == 0) *(cmd)++ = 0xdeadbeef;\
   }

#define MWV207_SNAP_TO_PAGE(cmd)\
   {\
      *(cmd)++ = 0x9800001f;\
      *(cmd)++ = 0x00000000;\
   }

#define MWV207_BLIT_LOCK(ctx, cmd_buf) \
   MWV207_EMIT(cmd_buf, 0x502E, \
      (ALIGN_BIT(0x1, 0x0) & (0x1))); \
   MWV207_EMIT(cmd_buf, 0x50CE, \
      (ALIGN_BIT((uint32_t) (ctx->ws->cluster_mask), 0x0) & (0xFF)));

#define MWV207_BLIT_UNLOCK(ctx, cmd_buf) \
   MWV207_EMIT(cmd_buf, 0x502E, \
      (ALIGN_BIT(0x0, 0x0) & (0x1)));\
   mwv207_wait_blit(&cmd_buf, ctx->ws->cluster_mask);

static inline void
mwv207_wait_blit(uint32_t **cmd_buf, uint32_t cluster_mask)
{

   MWV207_EMIT(*cmd_buf, 0x502E,
      (ALIGN_BIT(0x1, 0x0) & (0x1)));

   if (cluster_mask) {
      MWV207_EMIT(*cmd_buf, 0x50CE,
         (ALIGN_BIT((uint32_t) (cluster_mask), 0x0) & (0xFF)));
   }

   MWV207_EMIT(*cmd_buf, 0xE02,
      (ALIGN_BIT((uint32_t) (0x1), 0x0) & (0x1F)) |
      (ALIGN_BIT((uint32_t) (0x10), 0x8) & (0x1F00)));
   MWV207_STALL(cmd_buf,
      (ALIGN_BIT((uint32_t) (0x1), 0x0) & (0x1F)) |
      (ALIGN_BIT((uint32_t) (0x10), 0x8) & (0x1F00)));

   MWV207_EMIT(*cmd_buf, 0x502E,
      (ALIGN_BIT(0x0, 0x0) & (0x1)));
}

static inline void
mwv207_excute_blit(uint32_t **cmd_buf, uint32_t op)
{
   MWV207_EMIT(*cmd_buf, 0x502B,
      (ALIGN_BIT((uint32_t) (ENABLE), 0x1) & (0x2)) |
      (ALIGN_BIT((uint32_t) (ENABLE), 0x0) & (0x1)));
   MWV207_EMIT(*cmd_buf, 0x5018,
      (ALIGN_BIT((uint32_t) (op), 0x0) & (0x7)) |
      (ALIGN_BIT((uint32_t) (DISABLE), 0x4) & (0x10)));
   MWV207_EMIT(*cmd_buf, 0x502B,
      (ALIGN_BIT((uint32_t) (ENABLE), 0x1) & (0x2)) |
      (ALIGN_BIT((uint32_t) (ENABLE), 0x0) & (0x1)));
}

void
mwv207_submit(struct mwv207_context *ctx, int *out_fence_fd, bool force);

void
mwv207_create_cmd(struct mwv207_context *ctx);

void
mwv207_begin_record(struct mwv207_cmd *cmd, uint32_t **start);

void
mwv207_end_record(struct mwv207_cmd *cmd, uint32_t **end);

enum pipe_error
mwv207_dat_add_reloc(struct cmd_dat *dat,
                     uint32_t **ptr,
                     struct mwv207_bo *bo,
                     uint32_t offset,
                     uint32_t flags);

enum pipe_error
mwv207_add_dat(struct mwv207_context *ctx,
               uint32_t **ptr,
               uint32_t reg,
               struct cmd_dat *dat);

enum pipe_error
mwv207_add_relocs(struct mwv207_context *ctx,
                  uint32_t **ptr,
                  uint32_t reg,
                  struct mwv207_bo *bo,
                  uint32_t offset,
                  uint32_t flags);

enum pipe_error
mwv207_dma(struct mwv207_context *ctx,
           struct pipe_resource *psrc,
           struct pipe_resource *pdst,
           unsigned level,
           const struct pipe_box *box);

void
mwv207_pipeline_barrier(struct mwv207_context *ctx);

void
mwv207_wait_idle(uint32_t **pbuf, uint32_t target);

enum pipe_error
mwv207_flush_cache(uint32_t **pbuf, enum cache_mask mask);

#endif
