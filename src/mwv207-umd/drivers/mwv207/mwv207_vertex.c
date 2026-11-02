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
#include "util/u_helpers.h"
#include "util/u_inlines.h"
#include "util/u_upload_mgr.h"

#include "mwv207_cmd.h"
#include "mwv207_format.h"
#include "mwv207_resource.h"
#include "mwv207_state.h"
#include "mwv207_context.h"
#include "mwv207_compiler.h"

static void *
mwv207_create_elements(struct pipe_context *pctx,
                       unsigned num_elements,
                       const struct pipe_vertex_element *elements)
{
   struct mwv207_elements_state *state = CALLOC_STRUCT(mwv207_elements_state);

   if (!state)
      return NULL;

   for (int i = 0; i < num_elements; ++i) {
      const struct pipe_vertex_element *elem = elements + i;
      state->elements[i].base = *elem;
      state->elements[i].fetch_break = true;
      mwv207_get_element_desc(&state->elements[i]);
   }

   for (int i = 1; i < num_elements; ++i) {
      if (state->elements[i - 1].base.vertex_buffer_index ==
          state->elements[i].base.vertex_buffer_index)
         state->elements[i - 1].fetch_break = false;
   }

   state->num_elements = num_elements;

   return state;
}

static void
mwv207_bind_elements(struct pipe_context *pctx, void *state)
{
   struct mwv207_context *ctx = mwv207_context(pctx);

   ctx->element_state = (struct mwv207_elements_state *)state;

   ctx->dirty |= MWV207_DIRTY_VERTEX_ELEMENTS;
}

static void
mwv207_delete_elements(struct pipe_context *pctx, void *state)
{
   FREE(state);
}

static void
mwv207_set_vertex_buffers(struct pipe_context *pctx,
                          unsigned num_buffers,
                          const struct pipe_vertex_buffer *buffers)
{
   struct mwv207_context *ctx = mwv207_context(pctx);

   util_set_vertex_buffers_count(ctx->vertex_buffer,
                                 &ctx->num_vertex_buffers,
                                 buffers, num_buffers, true);

   ctx->dirty |= MWV207_DIRTY_VERTEX_BUFFERS;
}

static uint32_t
get_texture_size(struct mwv207_context *ctx, struct mwv207_shader *vs,
                 enum mwv207_immediate_contents contents, uint32_t index)
{
   struct mwv207_sampler_view *sampler_view;

   sampler_view = mwv207_sampler_view(ctx->sampler_views[vs->stage][index]);

   uint8_t contentsIdx = contents - MWV207_UNIFORM_TEXTURE_WIDTH;

   assert(contentsIdx < 3);

   return sampler_view->tex_desc.tex_fetch[contentsIdx];
}

static uint32_t
get_texrect_scale(struct mwv207_context *ctx,
                  enum pipe_shader_type stage,
                  enum mwv207_immediate_contents contents,
                  uint32_t index)
{
   struct pipe_sampler_view *texture = ctx->sampler_views[stage][index];
   uint32_t dim;

   if (contents == MWV207_IMMEDIATE_TEXRECT_SCALE_X)
      dim = texture->texture->width0;
   else
      dim = texture->texture->height0;

   return fui(1.0f / dim);
}

static uint32_t
get_ubo_offset(struct mwv207_context *ctx,
               enum pipe_shader_type stage,
               uint32_t idx,
               uint32_t *cmd_buf,
               uint32_t base)
{
   uint32_t ubo_offset;
   struct mwv207_resource *res;
   struct pipe_constant_buffer cb = ctx->cb[stage][idx];

   if (!cb.buffer)
      u_upload_data(ctx->base.const_uploader, 0, cb.buffer_size,
         256, cb.user_buffer, &ubo_offset, &cb.buffer);
   else
      ubo_offset = cb.buffer_offset;

   ubo_offset += base;

   res = mwv207_resource(cb.buffer);
   uint32_t bo_idx = ctx->ws->cmdbuf_add_bo(ctx->cmd, res->bo, 0x1);
   uint32_t cmd_offset = ((uint8_t *)cmd_buf - ctx->cmd->buf);
   uint32_t type   = 0x1;
   uint32_t pad    = 0;
   ctx->ws->cmdbuf_add_relloc(ctx->cmd, bo_idx, cmd_offset, ubo_offset, type, pad);

   return ubo_offset;
}

static uint32_t
get_ssbo_offset(struct mwv207_context *ctx,
                enum pipe_shader_type stage,
                uint32_t idx,
                uint32_t *cmd_buf,
                uint32_t base)
{

   struct pipe_shader_buffer ssbo;
   assert(idx <= 0x4);
   if (idx == 0x4)
      ssbo = ctx->shader_states[stage]->current->spill_buffer;
   else
      ssbo = ctx->ssbo[stage][idx];
   struct mwv207_resource *res = mwv207_resource(ssbo.buffer);
   uint32_t offset = ssbo.buffer_offset + res->offset + base;
   uint32_t bo_idx = ctx->ws->cmdbuf_add_bo(ctx->cmd, res->bo, 0x2);
   uint32_t cmd_offset = ((uint8_t *)cmd_buf - ctx->cmd->buf);
   uint32_t type   = 0x1;
   uint32_t pad    = 0;
   ctx->ws->cmdbuf_add_relloc(ctx->cmd, bo_idx, cmd_offset, offset, type, pad);

   return offset;
}

static uint32_t
get_global_offset(struct mwv207_context *ctx,
                  uint32_t idx,
                  uint32_t *cmd_buf,
                  uint32_t base)
{
   struct mwv207_global_bindings_state *so = &ctx->global_bindings;
   struct mwv207_resource *res = mwv207_resource(so->buf[idx]);
   uint32_t offset = res->offset + base;
   uint32_t bo_idx = ctx->ws->cmdbuf_add_bo(ctx->cmd, res->bo, 0x2);
   uint32_t cmd_offset = ((uint8_t *)cmd_buf - ctx->cmd->buf);
   ctx->ws->cmdbuf_add_relloc(ctx->cmd, bo_idx, cmd_offset, offset, 1, 0);
   return offset;
}

static uint32_t
get_local_offset(struct mwv207_context *ctx,
                 uint32_t *cmd_buf,
                 uint32_t base)
{
   struct pipe_shader_buffer local_memory = ctx->local_memory;
   struct mwv207_resource *res = mwv207_resource(local_memory.buffer);
   uint32_t offset = local_memory.buffer_offset + res->offset + base;
   uint32_t bo_idx = ctx->ws->cmdbuf_add_bo(ctx->cmd, res->bo, 0x2);
   uint32_t cmd_offset = ((uint8_t *)cmd_buf - ctx->cmd->buf);
   uint32_t type   = 0x1;
   uint32_t pad    = 0;
   ctx->ws->cmdbuf_add_relloc(ctx->cmd, bo_idx, cmd_offset, offset, type, pad);

   return offset;
}

static uint32_t
get_ssimage_info(struct mwv207_context *ctx,
                 enum pipe_shader_type stage,
                 uint32_t idx,
                 uint32_t *cmd_buf,
                 uint32_t type)
{
   struct pipe_image_view image = ctx->ssimage[stage][idx];
   struct mwv207_resource *res = mwv207_resource(image.resource);

   if (type == MWV207_IMMEDIATE_SSIMAGE) {
      uint32_t offset = res->offset;
      uint32_t bo_idx = ctx->ws->cmdbuf_add_bo(ctx->cmd, res->bo, 0x2);
      uint32_t cmd_offset = ((uint8_t *)cmd_buf - ctx->cmd->buf);
      uint32_t pad    = 0;

      if (res->base.target == PIPE_BUFFER)
         offset += image.u.buf.offset;
      else {
         struct mwv207_level_info *level = &res->level_infos[image.u.tex.level];
         offset += level->offset;
         offset += level->layer_size * image.u.tex.first_layer;
      }

      ctx->ws->cmdbuf_add_relloc(ctx->cmd, bo_idx, cmd_offset, offset, 0x1, pad);

      return offset;
   } else if (type == MWV207_IMMEDIATE_SSIMAGE_STRIDE) {
      if (res->base.target == PIPE_BUFFER)
         return image.u.buf.size;
      else {
         struct mwv207_level_info *level = &res->level_infos[image.u.tex.level];
         return level->stride;
      }
   } else if (type == MWV207_IMMEDIATE_SSIMAGE_SIZE) {
      if (res->base.target == PIPE_BUFFER)
         return (image.u.buf.size | (1 << 16));
      else {
         struct mwv207_level_info *level = &res->level_infos[image.u.tex.level];
         return (level->width | (level->height << 16));
      }
   } else if (type == MWV207_IMMEDIATE_SSIMAGE_LAYER_SIZE) {
      struct mwv207_level_info *level = &res->level_infos[image.u.tex.level];
      return level->layer_size;
   } else {

      switch (res->base.format) {
      case PIPE_FORMAT_RGBA8888_UNORM:
         return 0x32101bd2;
      case PIPE_FORMAT_RGBA8888_SINT:
         return 0x32101912;
      case PIPE_FORMAT_RGBA8888_UINT:
         return 0x321019d2;
      case PIPE_FORMAT_RGBA8888_SNORM:
         return 0x32101b12;
      case PIPE_FORMAT_R16G16B16A16_FLOAT:
         return 0x32101853;
      case PIPE_FORMAT_R16G16B16A16_SINT:
         return 0x321018d3;
      case PIPE_FORMAT_R16G16B16A16_UINT:
         return 0x32101993;
      case PIPE_FORMAT_R32_UINT:
         return 0x54405962;
      case PIPE_FORMAT_R32_SINT:
         return 0x544058A2;
      default:
         return 0x30121bd2;
      }
   }
}

static uint32_t
get_atomic_offset(struct mwv207_context *ctx,
                  enum pipe_shader_type stage,
                  uint32_t idx,
                  uint32_t *cmd_buf,
                  uint32_t base)
{
   struct pipe_shader_buffer atomic_bo = ctx->atomic_bo[idx];
   struct mwv207_resource *res = mwv207_resource(atomic_bo.buffer);
   uint32_t offset = atomic_bo.buffer_offset + res->offset + base;
   uint32_t bo_idx = ctx->ws->cmdbuf_add_bo(ctx->cmd, res->bo, 0x2);
   uint32_t cmd_offset = ((uint8_t *)cmd_buf - ctx->cmd->buf);
   uint32_t type   = 0x1;
   uint32_t pad    = 0;
   ctx->ws->cmdbuf_add_relloc(ctx->cmd, bo_idx, cmd_offset, offset, type, pad);

   return offset;
}

void
mwv207_config_uniform(struct mwv207_context *ctx, enum pipe_shader_type stage)
{
   struct mwv207_shader *shader;
   const struct mwv207_shader_uniform_info *uinfo;

   uint32_t base;
   uint32_t buffer_offset;
   uint32_t *data;
   uint32_t unifrom[4] = {0};
   uint32_t *cmd_buf;

   if (!ctx->shader_states[stage])
      return;

   shader = ctx->shader_states[stage]->current;
   uinfo = &shader->uniforms;

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   buffer_offset = ctx->cb[stage][0].buffer_offset;
   data = (uint32_t *)ctx->cb[stage][0].user_buffer;

   switch (stage) {
   case PIPE_SHADER_VERTEX:
   case PIPE_SHADER_TESS_CTRL:
   case PIPE_SHADER_TESS_EVAL:
   case PIPE_SHADER_GEOMETRY:
      base = 0xD000 + 0x0;
      break;
   case PIPE_SHADER_FRAGMENT:
      base = 0xD800 + 0x400;
      break;
   case PIPE_SHADER_COMPUTE:
      base = 0xD800 + 0x0;
      break;
   default:
      unreachable("unsupport shader");
      break;
   }

   if (stage == PIPE_SHADER_TESS_CTRL || stage == PIPE_SHADER_TESS_EVAL ||
       stage == PIPE_SHADER_GEOMETRY) {
      uint32_t offset = ctx->shader_states[PIPE_SHADER_VERTEX]->current->uniforms.imm_count + 4;

      if (stage == PIPE_SHADER_TESS_CTRL) {
         MWV207_EMIT(cmd_buf, 0x5291, (offset / 4));
      } else {
         if (ctx->shader_states[PIPE_SHADER_TESS_CTRL])
            offset += ctx->shader_states[PIPE_SHADER_TESS_CTRL]->current->uniforms.imm_count + 4;

         if (stage == PIPE_SHADER_TESS_EVAL) {
            MWV207_EMIT(cmd_buf, 0x52C9, (offset / 4));
         } else {
            if (ctx->shader_states[PIPE_SHADER_TESS_EVAL])
               offset += ctx->shader_states[PIPE_SHADER_TESS_EVAL]->current->uniforms.imm_count + 4;

            MWV207_EMIT(cmd_buf, 0x453, (offset / 4));
         }
      }

      base += offset;
   }

   if (!uinfo->imm_count) {
      mwv207_end_record(ctx->cmd, &cmd_buf);
      return;
   }

   for (uint32_t i = 0; i < uinfo->imm_count; i++) {
      uint32_t val = uinfo->imm_data[i];

      switch (uinfo->imm_contents[i]) {
      case MWV207_IMMEDIATE_CONSTANT:
         unifrom[i % 4] = val;
         break;

      case MWV207_IMMEDIATE_UNIFORM:
         unifrom[i % 4] = data[val + buffer_offset];
         break;

      case MWV207_UNIFORM_TEXTURE_WIDTH:
      case MWV207_UNIFORM_TEXTURE_HEIGHT:
      case MWV207_UNIFORM_TEXTURE_DEPTH:
         unifrom[i % 4] = get_texture_size(ctx, shader, uinfo->imm_contents[i], val);
         break;

      case MWV207_IMMEDIATE_TEXRECT_SCALE_X:
      case MWV207_IMMEDIATE_TEXRECT_SCALE_Y:
         unifrom[i % 4] = get_texrect_scale(ctx, stage, uinfo->imm_contents[i], val);
         break;

      case MWV207_UNIFORM_PATCH_VERTEX_IN:
         unifrom[i % 4] = ctx->info.vertices_per_patch;
         break;

      case MWV207_IMMEDIATE_WORK_GROUP_COUNT:
         unifrom[i % 4] = ctx->info.grid[val];
         break;

      case MWV207_IMMEDIATE_UBO:
         unifrom[i % 4] = get_ubo_offset(ctx, stage, val, (cmd_buf + (i % 4) + 1), 0);
         break;

      case MWV207_IMMEDIATE_SSBO:
         unifrom[i % 4] = get_ssbo_offset(ctx, stage, val, (cmd_buf + (i % 4) + 1), 0);
         break;
      case MWV207_IMMEDIATE_LOCAL_MEMORY_OFFSET:
         unifrom[i % 4] = get_local_offset(ctx, (cmd_buf + (i % 4) + 1), 0);
         break;

      case MWV207_IMMEDIATE_SSIMAGE:
      case MWV207_IMMEDIATE_SSIMAGE_STRIDE:
      case MWV207_IMMEDIATE_SSIMAGE_SIZE:
      case MWV207_IMMEDIATE_SSIMAGE_INFO:
      case MWV207_IMMEDIATE_SSIMAGE_LAYER_SIZE:
         unifrom[i % 4] = get_ssimage_info(ctx, stage, val, (cmd_buf + (i % 4) + 1), uinfo->imm_contents[i]);
         break;

      case MWV207_IMMEDIATE_SSBO_SIZE:
         unifrom[i % 4] = ctx->ssbo[stage][val].buffer_size;
         break;

      case MWV207_IMMEDIATE_ATOMIC_BO:
         unifrom[i % 4] = get_atomic_offset(ctx, stage, val, (cmd_buf + (i % 4) + 1), 0);
         break;
      case MWV207_IMMEDIATE_GLOBAL_MEMORY_OFFSET:
         unifrom[i % 4] = get_global_offset(ctx, val, (cmd_buf + (i % 4) + 1), 0);
         break;

      case MWV207_IMMEDIATE_WORK_GROUP_SIZE:
      case MWV207_IMMEDIATE_UNUSED:
         unifrom[i % 4] = 0;
         break;
      }

      if ((i % 4) == 3) {
         MWV207_EMITS(cmd_buf, base, 4, unifrom);
         base += 4;
      }
   }

   mwv207_end_record(ctx->cmd, &cmd_buf);
}

static void
mwv207_set_constant_buffer(struct pipe_context *pctx,
                           enum pipe_shader_type shader,
                           uint index, bool take_ownership,
                           const struct pipe_constant_buffer *cb)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   util_copy_constant_buffer(&ctx->cb[shader][index], cb, take_ownership);

   if (cb && ctx->cb[shader][index].user_buffer == NULL && ctx->cb[shader][index].buffer) {
      struct mwv207_resource *res = mwv207_resource(ctx->cb[shader][index].buffer);
      ctx->cb[shader][index].user_buffer = ctx->ws->bo_map(ctx->ws, res->bo);
   }

   ctx->dirty |= MWV207_DIRTY_CONSTBUF;
}

void
mwv207_context_vertex_init(struct pipe_context *pctx)
{
   struct mwv207_context *ctx = mwv207_context(pctx);

   ctx->num_vertex_buffers = 0;

   pctx->create_vertex_elements_state = mwv207_create_elements;
   pctx->bind_vertex_elements_state   = mwv207_bind_elements;
   pctx->delete_vertex_elements_state = mwv207_delete_elements;

   pctx->set_vertex_buffers  = mwv207_set_vertex_buffers;
   pctx->set_constant_buffer = mwv207_set_constant_buffer;
}

static uint32_t
bit_countn(uint64_t *bitfield, uint32_t n)
{
    uint32_t count = 0;

    uint32_t full_blocks = n / 64;
    uint32_t remaining_bits = n % 64;

    for (uint32_t i = 0; i < full_blocks; i++)
        count += util_bitcount64(bitfield[i]);

    if (remaining_bits > 0) {
        uint64_t mask = (1ull << remaining_bits) - 1;
        count += util_bitcount64(bitfield[full_blocks] & mask);
    }

    return count;
}

static uint32_t
bit_scann(uint32_t val, uint64_t *bitfield)
{
   uint32_t first_bit = 0;
   for (int i = 0; i < 0x8; i++) {
      uint64_t mask = bitfield[i];
      if (mask != 0)
        first_bit = i * 64 + ffsll(bitfield[i]) - 1;
   }

   val = (first_bit >= val) ? val : (val - bit_countn(bitfield, val));
   return val;
}

void
mwv207_emit_kernel_params(struct mwv207_context *ctx, const struct pipe_grid_info *info)
{
   struct mwv207_shader *shader;
   const struct mwv207_shader_uniform_info *uinfo;

   uint32_t base;
   uint32_t unifrom[4] = {0};
   uint32_t *cmd_buf;
   shader = ctx->shader_states[PIPE_SHADER_COMPUTE]->current;
   uinfo = &shader->uniforms;
   base = 0xD800 + 0x0;

   uint32_t const_count = 0;
   for (int i = 0; i < 0x8; i++)
      const_count += util_bitcount64(shader->ubo_mask[i]);

   mwv207_begin_record(ctx->cmd, &cmd_buf);
   for (uint32_t i = 0; i < uinfo->imm_count; i++) {
      uint32_t val = uinfo->imm_data[i];
      switch (uinfo->imm_contents[i]) {
      case MWV207_IMMEDIATE_CONSTANT:
         unifrom[i % 4] = val;
         break;
      case MWV207_IMMEDIATE_UBO:
         unifrom[i % 4] = get_ubo_offset(ctx, PIPE_SHADER_COMPUTE, val, (cmd_buf + (i % 4) + 1), 0);
         break;
      case MWV207_IMMEDIATE_WORK_GROUP_COUNT:
         unifrom[i % 4] = ctx->info.grid[val];
         break;
      case MWV207_IMMEDIATE_WORK_GROUP_SIZE:
         unifrom[i % 4] = info->block[val];
         break;
      case MWV207_IMMEDIATE_GLOBAL_MEMORY_OFFSET:
         if (const_count != 0)
            val = bit_scann(val, shader->ubo_mask);
         unifrom[i % 4] = get_global_offset(ctx, val, (cmd_buf + (i % 4) + 1), 0);
         break;
      case MWV207_IMMEDIATE_UNUSED:
         unifrom[i % 4] = 0;
         break;
      default:
         break;
      }

      if ((i % 4) == 3) {
         MWV207_EMITS(cmd_buf, base, 4, unifrom);
         base += 4;
      }

   }
   mwv207_end_record(ctx->cmd, &cmd_buf);
}
