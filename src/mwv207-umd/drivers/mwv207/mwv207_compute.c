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
#include <stdint.h>
#include "util/u_helpers.h"

#include "mwv207_cmd.h"
#include "mwv207_context.h"
#include "mwv207_shader.h"
#include "mwv207_compiler.h"
#include "mwv207_blit.h"

static void
mwv207_set_hw_atomic_buffers(struct pipe_context *pctx,
                             unsigned start_slot, unsigned count,
                             const struct pipe_shader_buffer *buffers)
{
   struct mwv207_context *ctx = mwv207_context(pctx);

   util_set_shader_buffers_mask(ctx->atomic_bo, &ctx->atomic_mask,
                                buffers, start_slot, count);
}

static void
mwv207_set_shader_buffers(struct pipe_context *pctx,
                          enum pipe_shader_type shader,
                          unsigned start_slot, unsigned count,
                          const struct pipe_shader_buffer *buffers,
                          unsigned writable_bitmask)
{
   struct mwv207_context *ctx = mwv207_context(pctx);

   util_set_shader_buffers_mask(ctx->ssbo[shader], &ctx->ssbo_mask[shader],
                                buffers, start_slot, count);
}

static void
mwv207_set_shader_images(struct pipe_context *pctx,
                         enum pipe_shader_type shader,
                         unsigned start_slot, unsigned count,
                         unsigned unbind_num_trailing_slots,
                         const struct pipe_image_view *images)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   struct pipe_image_view *dst = ctx->ssimage[shader];

   for ( int i = 0; i < count; i++)
      if (images && images[start_slot + i].resource)
         util_copy_image_view(&dst[start_slot + i], &images[start_slot + i]);

   ctx->num_images[shader] = start_slot + count;
}

static void
mwv207_set_compute_resources(struct pipe_context *pctx,
                             unsigned start, unsigned count,
                             struct pipe_surface **resources)
{

}

static void
mwv207_set_global_binding(struct pipe_context *pctx,
                          unsigned first, unsigned count,
                          struct pipe_resource **resources,
                          uint32_t **handles)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   struct mwv207_global_bindings_state *so = &ctx->global_bindings;
   uint32_t mask = 0;

   if (resources) {
      for (uint32_t i = 0; i < count; i++) {
         uint32_t n = i + first;

         mask |= BITFIELD_BIT(n);

         pipe_resource_reference(&so->buf[n], resources[i]);

         if (so->buf[n]) {
            struct mwv207_resource *rsc = mwv207_resource(so->buf[n]);
            uint32_t offset = *handles[i];
            uint32_t va = (uint32_t)(uintptr_t)rsc->bo->logical + offset;
            memcpy(handles[i], &va, sizeof(va));

            so->enable_mask |= BITFIELD_BIT(n);
         } else {
            so->enable_mask &= ~BITFIELD_BIT(n);
         }
      }
   } else {
      mask = (BITFIELD_BIT(count) - 1) << first;
      for (uint32_t i = 0; i < count; i++) {
         uint32_t n = i + first;
         pipe_resource_reference(&so->buf[n], NULL);
      }
      so->enable_mask &= ~mask;
   }
}

static void
mwv207_launch_grid(struct pipe_context *pctx,
                   const struct pipe_grid_info *info)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   struct mwv207_shader_state *shader = ctx->shader_states[PIPE_SHADER_COMPUTE];
   struct mwv207_shader *cs = shader->current;
   uint32_t *cmd_buf;
   uint32_t local_memory_size = shader->nir->info.shared_size;
   uint32_t local_memory_pack_size = align(shader->nir->info.shared_size, 16) / 16;
   uint64_t max_local_memory_size = 0;
   ctx->base.screen->get_compute_param(ctx->base.screen, PIPE_SHADER_COMPUTE,
                                       PIPE_COMPUTE_CAP_MAX_LOCAL_SIZE, &max_local_memory_size);
   uint64_t max_thread_count = 0;
   ctx->base.screen->get_compute_param(ctx->base.screen, PIPE_SHADER_COMPUTE,
                                       PIPE_COMPUTE_CAP_MAX_THREADS_PER_BLOCK, &max_thread_count);
   uint32_t work_group_size = info->block[0] * info->block[1] * info->block[2];
   uint32_t work_group_count_per_shader_group = (uint32_t)(floor((double)max_thread_count / (double)work_group_size));
   uint32_t local_memory_count = (uint32_t)(floor((double) max_local_memory_size / (double)local_memory_size));
   work_group_count_per_shader_group = MIN2(work_group_count_per_shader_group, local_memory_count);
   uint32_t max_running_work_groups = MAX2(work_group_size, max_thread_count);

   if (info->indirect) {
      struct pipe_transfer *transfer;
      uint32_t *map = pipe_buffer_map_range(pctx, info->indirect,
                                            info->indirect_offset,
                                            3 * sizeof(uint32_t),
                                            PIPE_MAP_READ,
                                            &transfer);
      memcpy(ctx->info.grid, map, 3 * sizeof(uint32_t));
      pipe_buffer_unmap(pctx, transfer);

      if (ctx->info.grid[0] == 0 ||
          ctx->info.grid[1] == 0 ||
          ctx->info.grid[2] == 0) {
         return;
      }
   } else {
      ctx->info.grid[0] = info->grid[0];
      ctx->info.grid[1] = info->grid[1];
      ctx->info.grid[2] = info->grid[2];
   }

   if (cs->stage == MESA_SHADER_KERNEL)
      mwv207_emit_kernel_params(ctx, info);
   else
      mwv207_config_uniform(ctx, PIPE_SHADER_COMPUTE);
   mwv207_config_sampler(ctx, PIPE_SHADER_COMPUTE);

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   MWV207_EMIT(cmd_buf, 0x1F0, 0x00000003);

   MWV207_EMIT(cmd_buf, 0xE02,
      (ALIGN_BIT((uint32_t) (0x1), 0x0) & (0x1F)) |
      (ALIGN_BIT((uint32_t) (0x7), 0x8) & (0x1F00)));
   MWV207_STALL(&cmd_buf,
      (ALIGN_BIT((uint32_t) (0x1), 0x0) & (0x1F)) |
      (ALIGN_BIT((uint32_t) (0x7), 0x8) & (0x1F00)));

   MWV207_EMIT(cmd_buf, 0x5580, 0x00000002);
   MWV207_EMIT(cmd_buf, 0x21A, (ALIGN_BIT(0x1, 0x0) & (0x1)));
   MWV207_EMIT(cmd_buf, 0x403, cs->num_temps);

   MWV207_EMIT(cmd_buf, 0x2A4,   0x2);
   MWV207_EMIT(cmd_buf, 0x420, 0x2);

   if (cs->cs_shared_size) {
      MWV207_EMIT(cmd_buf, 0x427,
         (ALIGN_BIT((uint32_t) (max_local_memory_size >> 10), 0x0) & (0x7F)) |
         (ALIGN_BIT((uint32_t) (0), 0x8) & (0xFF00)) |
         (ALIGN_BIT((uint32_t) (0), 0x10) & (0xFFF0000)));
   } else {
      MWV207_EMIT(cmd_buf, 0x427,
         (ALIGN_BIT((uint32_t) (max_local_memory_size >> 10), 0x0) & (0x7F)) |
         (ALIGN_BIT((uint32_t) (work_group_count_per_shader_group), 0x8) & (0xFF00)) |
         (ALIGN_BIT((uint32_t) (align(max_running_work_groups, 2) * 2 - 1), 0x10) & (0xFFF0000)));
   }

   MWV207_EMIT(cmd_buf, 0x416, 0);
   MWV207_EMIT(cmd_buf, 0x409, 0);

   MWV207_EMIT(cmd_buf, 0x21F, 0);
   MWV207_EMIT(cmd_buf, 0x424, (cs->code_size / 4));
   mwv207_add_relocs(ctx, &cmd_buf, 0x40A,
      shader->bo, shader->current->bo_offset, 0x1);
   MWV207_EMIT(cmd_buf, 0x425, (cs->code_size / 4 - 1));

   MWV207_EMIT(cmd_buf, 0x402,
       (ALIGN_BIT((uint32_t) (3), 0x0) & (0x3F)) |
       (ALIGN_BIT((uint32_t) (31), 0x8) & (0x1F00)) |
       (ALIGN_BIT((uint32_t) (false), 0x18) & (0x1000000)));

   MWV207_EMIT(cmd_buf, 0x404, 0x00000f00);

   MWV207_EMIT(cmd_buf, 0x228,    0);
   MWV207_EMIT(cmd_buf, 0x5582,  0);
   MWV207_EMIT(cmd_buf, 0x5285,    0);
   MWV207_EMIT(cmd_buf, 0x5286,   0);
   MWV207_EMIT(cmd_buf, 0x52C6,    0);
   MWV207_EMIT(cmd_buf, 0x52C7,   0);
   MWV207_EMIT(cmd_buf, 0x52CD, 0);
   MWV207_EMIT(cmd_buf, 0x440,     0);
   MWV207_EMIT(cmd_buf, 0x450,    0);
   MWV207_EMIT(cmd_buf, 0x457,  0xff);

   MWV207_EMIT(cmd_buf, 0x28C,    0x00000300);
   MWV207_EMIT(cmd_buf, 0xE07, 0x22);

   MWV207_EMIT(cmd_buf, 0x40C, 0);
   MWV207_EMIT(cmd_buf, 0x40D, 0);

   MWV207_EMIT(cmd_buf, 0x20C, 0x0f3f0000);
   MWV207_EMIT(cmd_buf, 0x201, 0x00000001);
   MWV207_EMIT(cmd_buf, 0x2AA, 0);
   MWV207_EMIT(cmd_buf, 0xE22, 0x007f0000);

   MWV207_EMIT(cmd_buf, 0x412, 0);

   MWV207_EMIT(cmd_buf, 0x240,
      (ALIGN_BIT((uint32_t) (3), 0x0) & (0x3)) |
      (ALIGN_BIT(0x1, 0x18) & (0x7000000)));

   if (cs->cs_shared_size) {
      MWV207_EMIT(cmd_buf, 0x249,
         (ALIGN_BIT((uint32_t) (0), 0x0) & (0xFFFF)) |
         (ALIGN_BIT((uint32_t) (63), 0x14) & (0x3F00000)) |
         (ALIGN_BIT(0x0, 0x10) & (0x10000)) |
         (ALIGN_BIT(0x0, 0x1C) & (0x30000000)));
   } else {
      MWV207_EMIT(cmd_buf, 0x249,
         (ALIGN_BIT((uint32_t) (local_memory_pack_size), 0x0) & (0xFFFF)) |
         (ALIGN_BIT((uint32_t) (63), 0x14) & (0x3F00000)) |
         (ALIGN_BIT(0x0, 0x10) & (0x10000)) |
         (ALIGN_BIT(0x1, 0x1C) & (0x30000000)));
   }

   uint32_t thread = align(info->block[0] * info->block[1] * info->block[2], 64) / 64;

   MWV207_EMIT(cmd_buf, 0x247,  thread);

   MWV207_EMIT(cmd_buf, 0x24B, 0);
   MWV207_EMIT(cmd_buf, 0x24D, 0);
   MWV207_EMIT(cmd_buf, 0x24F, 0);

   MWV207_EMIT(cmd_buf, 0x250, ctx->info.grid[0] - 1);
   MWV207_EMIT(cmd_buf, 0x251, ctx->info.grid[1] - 1);
   MWV207_EMIT(cmd_buf, 0x252, ctx->info.grid[2] - 1);

   MWV207_EMIT(cmd_buf, 0x253, info->block[0] - 1);
   MWV207_EMIT(cmd_buf, 0x254, info->block[1] - 1);
   MWV207_EMIT(cmd_buf, 0x255, info->block[2] - 1);

   MWV207_EMIT(cmd_buf, 0x248, 0xbadabeeb);
   MWV207_EMIT(cmd_buf, 0xE02,
      (ALIGN_BIT((uint32_t) (0x1), 0x0) & (0x1F)) |
      (ALIGN_BIT((uint32_t) (0x7), 0x8) & (0x1F00)));
   MWV207_STALL(&cmd_buf,
      (ALIGN_BIT((uint32_t) (0x1), 0x0) & (0x1F)) |
      (ALIGN_BIT((uint32_t) (0x7), 0x8) & (0x1F00)));

   MWV207_EMIT(cmd_buf, 0xE03, 0xc23);

   mwv207_end_record(ctx->cmd, &cmd_buf);

   mwv207_submit(ctx, NULL, false);
}

static void
mwv207_create_local_memory_buffer(struct mwv207_context *ctx)
{
   struct pipe_resource templ;
   uint64_t max_local_memory_size;

   ctx->base.screen->get_compute_param(ctx->base.screen, PIPE_SHADER_COMPUTE,
                                       PIPE_COMPUTE_CAP_MAX_LOCAL_SIZE, &max_local_memory_size);
   ctx->local_memory.buffer_offset = 0;
   ctx->local_memory.buffer_size = max_local_memory_size;
   memset(&templ, 0, sizeof(templ));
   templ.target = PIPE_BUFFER;
   templ.bind |= PIPE_BIND_LINEAR;
   templ.width0 = max_local_memory_size;
   ctx->local_memory.buffer = ctx->base.screen->resource_create(ctx->base.screen, &templ);
}

void
mwv207_context_compute_init(struct pipe_context *pctx)
{
   mwv207_create_local_memory_buffer(mwv207_context(pctx));
   pctx->set_hw_atomic_buffers = mwv207_set_hw_atomic_buffers;
   pctx->set_shader_buffers    = mwv207_set_shader_buffers;
   pctx->set_shader_images     = mwv207_set_shader_images;
   pctx->set_compute_resources = mwv207_set_compute_resources;
   pctx->set_global_binding    = mwv207_set_global_binding;
   pctx->launch_grid           = mwv207_launch_grid;
}
