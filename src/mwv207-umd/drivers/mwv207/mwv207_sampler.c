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
#include "util/u_helpers.h"
#include "util/u_memory.h"
#include "util/u_inlines.h"
#include "util/format/u_format.h"
#include "util/format/format_utils.h"

#include "mwv207_cmd.h"
#include "mwv207_context.h"
#include "mwv207_shader.h"

static const int32_t mwv207_address_info[] = {
   0x0,
   0x2,
   0x2,
   0x3,
   0x1,

   0x1,
   0x1,
   0x1,
};

static const uint32_t mwv207_fliter_info[] = {
   0x1,
   0x2,
   0x0,
};

static const uint32_t mwv207_compare_func[] = {
   0x7,
   0x2,
   0x4,
   0x0,
   0x3,
   0x5,
   0x1,
   0x6,
};

static const uint32_t mwv207_texture_type[] = {
   0x2,
   0x2,
   0x2,
   0x3,
   0x5,
   0x2,
   0x3,
   0x3,
   0x5,
};

static const struct mwv207_sampler_desc sampler_regs[] = {
   {
      0x5800, 0x5880,
      0x5900, 0x5980,
      0x5A00, 0x5600,
      0x5680, 0x5311
   }, {
      0x5B00, 0x5B80,
      0x5C00, 0x5C80,
      0x5D00, 0x5700,
      0x5780, 0x5312
   },
};

static struct mwv207_sampler default_sampler;

static uint32_t
mwv207_log2(uint32_t val)
{
   uint32_t ret = 0;
   uint32_t scale = (1 << 8);

   if ( val <= 1 )
      return 0;

   if (!(val & 0xFF)) {
      val >>= 8;
      ret += 8 * scale;
   }

   if (!(val & 0xF)) {
      val >>= 4;
      ret += 4 * scale;
   }

   if (!(val & 0x3)) {
      val >>= 2;
      ret += 2 * scale;
   }

   if (!(val & 0x1)) {
      val >>= 1;
      ret += scale;
   }

   if (val > 1)
      ret += (uint32_t)(log((float)val) / log(2.0f) * scale);

   return ret;
}

static int32_t
mwv207_float_to_fixed(float x, int8_t trunc_bits, int8_t frac_bits)
{
   int32_t result;
   uint32_t in;

   in = fui(x);

   const uint16_t fixed_mask = (1 << (trunc_bits + frac_bits)) - 1;
   const uint16_t fixed_max  = (1 << (trunc_bits + frac_bits - 1)) - 1;
   const uint16_t fixed_min  = 1 << (trunc_bits + frac_bits - 1);

   bool sign_in = (in & 0x80000000) ? true : false;

   int16_t  exp_in  = ((in >> 23) & 0xFF) - 0x7F;

   uint32_t man_in  = (in & 0x7FFFFF) | 0x800000;

   if (exp_in < -frac_bits)
      result = 0;
   else if (exp_in >= trunc_bits - 1)
      result = sign_in ? fixed_min : fixed_max;
   else {
      int32_t man_shift = 23 - (exp_in + frac_bits);
      result = man_in >> man_shift;

      if (sign_in)
         result = (~result + 1) & fixed_mask;
   }
   return result;
}

static void
mwv207_print_samper(struct mwv207_sampler_view *sampler_view, uint32_t start_slot)
{
   struct mwv207_texture_config *tex_config = &sampler_view->tex_desc.tex_config;

   printf("================================================\n");
   printf("adress[0-3]: 0x%x, 0x%x, 0x%x ,0x%x\n", tex_config->address[0],
          tex_config->address[1], tex_config->address[2], tex_config->address[3]);
   printf("adress[4-7]: 0x%x, 0x%x, 0x%x ,0x%x\n", tex_config->address[4],
          tex_config->address[5], tex_config->address[6], tex_config->address[7]);
   printf("Sample:%u  TXSize : 0x%x\n", start_slot, tex_config->tex_size);
   printf("Sample:%u  TXLinearStride : 0x%x\n", start_slot, tex_config->tex_stride);
   printf("Sample:%u  TXSizeExt : 0x%x\n", start_slot, tex_config->tex_size_ext);
   printf("Sample:%u  TXVolumeExt : 0x%x\n", start_slot, tex_config->tex_volume_ext);
   printf("base level : %u level count %d tx3d: %d\n", sampler_view->info.first_level, sampler_view->info.level_count,
          tex_config->tex_3d);
   printf("stride : 0x%x slice : 0x%x\n", tex_config->tex_stride, tex_config->tex_slice);
   printf("\n");

   printf("Sample:%u  TXConfig : 0x08014000,0x%x\n", start_slot, tex_config->tex_config);
   printf("Sample:%u  TXExtConfig : 0x080140e0,0x%x\n", start_slot, tex_config->tex_config_ext);
   printf("Sample:%u  TXConfig2 : 0x080141e0,0x%x\n", start_slot, tex_config->tex_config2);
   printf("Sample:%u  TXConfig3 : 0x080144A0,0x%x\n", start_slot, tex_config->tex_config3);
   printf("TXASTC0Ex : 0x08014140,0x%x\n", tex_config->tex_astc0);
   printf("TXASTC1Ex : 0x08014160,0x%x\n", tex_config->tex_astc1);
   printf("TXASTC2Ex : 0x08014180,0x%x\n", tex_config->tex_astc2);
   printf("TXASTC3Ex : 0x080141A0,0x%x\n", tex_config->tex_astc3);
   printf("================================================\n");

}

static bool
mwv207_is_128bit_sampler(struct mwv207_resource *res, struct mwv207_sampler_view *sampler_view)
{
   if (res->res_128_bit)
      return true;
   if (sampler_view->base.target == PIPE_BUFFER &&
       util_format_get_blocksize(sampler_view->base.format) == 16)
      return true;

   return false;
}

enum pipe_format
mwv207_transfer_128bit(enum pipe_format input)
{
   if (input == PIPE_FORMAT_R32G32B32A32_FLOAT)
      return PIPE_FORMAT_R32G32_FLOAT;
   else if (input == PIPE_FORMAT_R32G32B32A32_UINT)
      return PIPE_FORMAT_R32G32_UINT;
   else if (input == PIPE_FORMAT_R32G32B32A32_SINT)
      return PIPE_FORMAT_R32G32_SINT;
   else
      return PIPE_FORMAT_R32G32_FLOAT;
}

void
mwv207_split_128bit(struct pipe_context *pctx,
                    struct pipe_resource *pres,
                    enum pipe_format format)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   struct mwv207_resource *res = mwv207_resource(pres);
   struct mwv207_resource *fake_rg, *fake_ba;

   if (res->rgba_dirty)
      mwv207_merge_128bit(pctx, pres);

   if (!res->rg) {
      struct pipe_resource templ = res->base;
      if (templ.target == PIPE_BUFFER) {
         templ.target = PIPE_TEXTURE_1D;
         templ.width0 = templ.width0 / 16;
      }
      templ.bind |= PIPE_BIND_LINEAR;
      enum pipe_format split_format =
         ((format != PIPE_FORMAT_NONE) ? format : res->base.format);
      templ.format = mwv207_transfer_128bit(split_format);
      res->rg = pctx->screen->resource_create(pctx->screen, &templ);
      res->ba = pctx->screen->resource_create(pctx->screen, &templ);
   }

   if (!res->src_dirty)
      return;

   fake_rg = mwv207_resource(res->rg);
   fake_ba = mwv207_resource(res->ba);

   mwv207_wait_resource(pctx, pres);
   mwv207_wait_resource(pctx, res->rg);
   mwv207_wait_resource(pctx, res->ba);

   uint32_t *src = ctx->ws->bo_map(ctx->ws, res->bo);
   uint32_t *dst_rg = ctx->ws->bo_map(ctx->ws, fake_rg->bo);
   uint32_t *dst_ba = ctx->ws->bo_map(ctx->ws, fake_ba->bo);

   uint32_t offset_dst = 0, offset_src = 0;
   uint32_t array_offset_dst = 0, array_offset_src = 0;
   uint32_t level_offset_dst = 0, level_offset_src = 0;

   for (int level = 0; level <= pres->last_level; level++) {
      level_offset_src = res->level_infos[level].offset;
      level_offset_dst = fake_rg->level_infos[level].offset;
      for (int array = 0; array < util_num_layers(&res->base, level); array++) {
         array_offset_src = level_offset_src + array * res->level_infos[level].layer_size;
         array_offset_dst = level_offset_dst + array * fake_rg->level_infos[level].layer_size;
         for (int j = 0; j < fake_rg->level_infos[level].height; j++) {
            offset_src = (array_offset_src + j * res->level_infos[level].stride) / sizeof(uint32_t);
            offset_dst = (array_offset_dst + j * fake_rg->level_infos[level].stride) / sizeof(uint32_t);
            for (int i = 0; i < fake_rg->level_infos[level].width; i++) {
               dst_rg[offset_dst + i * 2 + 0] = src[offset_src + i * 4 + 0];
               dst_rg[offset_dst + i * 2 + 1] = src[offset_src + i * 4 + 1];

               dst_ba[offset_dst + i * 2 + 0] = src[offset_src + i * 4 + 2];
               dst_ba[offset_dst + i * 2 + 1] = src[offset_src + i * 4 + 3];
            }
         }
      }
   }

   res->src_dirty = false;
}

void
mwv207_merge_128bit(struct pipe_context *pctx,
                    struct pipe_resource *pres)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   struct mwv207_resource *res = mwv207_resource(pres);
   struct mwv207_resource *fake_rg, *fake_ba;

   if (!res->rgba_dirty)
      return;

   if (!res->rg) {
      struct pipe_resource templ = res->base;
      templ.bind |= PIPE_BIND_LINEAR;
      templ.format = PIPE_FORMAT_R32G32_FLOAT;
      res->rg = pctx->screen->resource_create(pctx->screen, &templ);
      res->ba = pctx->screen->resource_create(pctx->screen, &templ);
   }

   fake_rg = mwv207_resource(res->rg);
   fake_ba = mwv207_resource(res->ba);

   mwv207_wait_resource(pctx, pres);
   mwv207_wait_resource(pctx, res->rg);
   mwv207_wait_resource(pctx, res->ba);

   uint32_t *src = ctx->ws->bo_map(ctx->ws, res->bo);
   uint32_t *dst_rg = ctx->ws->bo_map(ctx->ws, fake_rg->bo);
   uint32_t *dst_ba = ctx->ws->bo_map(ctx->ws, fake_ba->bo);

   const int32_t tile_map[8] = {0, 0, 2, 2, 2, 2, -4, -4};
   uint32_t tile_offset;

   uint32_t offset_dst = 0, offset_src = 0;
   uint32_t array_offset_dst = 0, array_offset_src = 0;
   uint32_t level_offset_dst = 0, level_offset_src = 0;

   for (int level = 0; level <= pres->last_level; level++) {
      level_offset_src = res->level_infos[level].offset;
      level_offset_dst = fake_rg->level_infos[level].offset;
      for (int array = 0; array < util_num_layers(&res->base, level); array++) {
         array_offset_src = level_offset_src + array * res->level_infos[level].layer_size;
         array_offset_dst = level_offset_dst + array * fake_rg->level_infos[level].layer_size;
         for (int j = 0; j < fake_rg->level_infos[level].height; j++) {
            offset_src = (array_offset_src + j * res->level_infos[level].stride) / sizeof(uint32_t);
            offset_dst = (array_offset_dst + j * fake_rg->level_infos[level].stride) / sizeof(uint32_t);
            for (int i = 0; i < fake_rg->level_infos[level].width; i++) {
               if (pres->last_level == 0)
                  tile_offset = i + tile_map[i % 8];
               else
                  tile_offset = i;
               src[offset_src + i * 4 + 0] = dst_rg[offset_dst + tile_offset * 2 + 0];
               src[offset_src + i * 4 + 1] = dst_rg[offset_dst + tile_offset * 2 + 1];

               src[offset_src + i * 4 + 2] = dst_ba[offset_dst + tile_offset * 2 + 0];
               src[offset_src + i * 4 + 3] = dst_ba[offset_dst + tile_offset * 2 + 1];
            }
         }
      }
   }

   res->rgba_dirty = false;
   res->src_dirty = true;
}

static uint32_t
mwv207_transfer_border(uint32_t swizzle, uint32_t current)
{
   switch (swizzle) {
   case 0x0:
      return 0;
   case 0x1:
      return 1;
   case 0x2:
      return 2;
   case 0x3:
      return 3;
   default:
      return current;
   }
}

static union pipe_color_union
mwv207_clamp_color(uint32_t format, struct mwv207_sampler *sampler)
{
   union pipe_color_union color
      = util_clamp_color(format, &sampler->state.border_color);

   if (util_format_is_compressed(format)) {
      if (util_format_is_unorm(format)) {
         color.f[0] = CLAMP(color.f[0], 0.0f, 1.0f);
         color.f[1] = CLAMP(color.f[1], 0.0f, 1.0f);
         color.f[2] = CLAMP(color.f[2], 0.0f, 1.0f);
         color.f[3] = CLAMP(color.f[3], 0.0f, 1.0f);
      } else {
         color.f[0] = CLAMP(color.f[0], -1.0f, 1.0f);
         color.f[1] = CLAMP(color.f[1], -1.0f, 1.0f);
         color.f[2] = CLAMP(color.f[2], -1.0f, 1.0f);
         color.f[3] = CLAMP(color.f[3], -1.0f, 1.0f);
      }
   } else if (util_format_is_depth_or_stencil(format)) {
      switch (format) {
      case PIPE_FORMAT_X8Z24_UNORM:
      case PIPE_FORMAT_S8_UINT_Z24_UNORM:
         color.ui[0] = _mesa_float_to_unorm(color.f[0], 24);
         color.ui[1] = 0;
         color.ui[2] = 0;
         color.ui[3] = _mesa_float_to_unorm(color.f[3], 24);
         break;
      case PIPE_FORMAT_S8X24_UINT:
      case PIPE_FORMAT_S8_UINT:
         color.ui[0] =_mesa_unsigned_to_unsigned(color.ui[0], 8);
         color.ui[1] = 0;
         color.ui[2] = 0;
         color.ui[3] = _mesa_unsigned_to_unsigned(color.ui[3], 8);
         break;
      case PIPE_FORMAT_Z16_UNORM:
         color.ui[0] = _mesa_float_to_unorm(color.f[0], 16);
         color.ui[1] = 0;
         color.ui[2] = 0;
         color.ui[3] = _mesa_float_to_unorm(color.f[3], 16);
         break;
      case PIPE_FORMAT_X32_S8X24_UINT:
      case PIPE_FORMAT_Z32_FLOAT_S8X24_UINT:
      case PIPE_FORMAT_Z32_FLOAT:

         break;
      default:
         printf("unsupport format %s\n", util_format_short_name(format));
         break;
      }
   }
   return color;
}

static void
mwv207_reconfig_texture(struct mwv207_sampler_view *sampler_view,
                        struct mwv207_sampler *sampler,
                        bool use_ba)
{
   uint32_t  *cmd_buf;
   struct mwv207_texture_desc *tex_desc = &sampler_view->tex_desc;
   struct mwv207_resource *res = mwv207_resource(sampler_view->base.texture);
   uint32_t swizzle_r, swizzle_g, swizzle_b, swizzle_a;
   union pipe_color_union color =
      mwv207_clamp_color(sampler_view->base.format, sampler);

   swizzle_r = mwv207_transfer_border(tex_desc->swizzles[0], 0);
   swizzle_g = mwv207_transfer_border(tex_desc->swizzles[1], 1);
   swizzle_b = mwv207_transfer_border(tex_desc->swizzles[2], 2);
   swizzle_a = mwv207_transfer_border(tex_desc->swizzles[3], 3);

   if (tex_desc->tex_128bit) {
      if (use_ba) {
         res = mwv207_resource(res->ba);
         swizzle_r = mwv207_transfer_border(tex_desc->swizzles[2], 2);
         swizzle_g = mwv207_transfer_border(tex_desc->swizzles[3], 3);
      } else
         res = mwv207_resource(res->rg);
   }

   tex_desc->tex_config.tex_border_red32   = color.ui[swizzle_r];
   tex_desc->tex_config.tex_border_green32 = color.ui[swizzle_g];
   tex_desc->tex_config.tex_border_blue32  = color.ui[swizzle_b];
   tex_desc->tex_config.tex_border_alpha32 = color.ui[swizzle_a];

   char resultColor[4] = {0, 0, 0, 0};
   resultColor[0] = color.f[swizzle_r] * 0xff;
   resultColor[1] = color.f[swizzle_g] * 0xff;
   resultColor[2] = color.f[swizzle_b] * 0xff;
   resultColor[3] = color.f[swizzle_a] * 0xff;

   tex_desc->tex_config.tex_border_color =
      (resultColor[3] << 24) | (resultColor[0] << 16) |
      (resultColor[1] << 8)  | (resultColor[2]);

   uint32_t levelCount = sampler_view->info.level_count;
   tex_desc->dat.nr_relocs = 0;
   cmd_buf = tex_desc->dat.data;

   for (uint32_t levelIdx = 0; levelIdx < levelCount; ++levelIdx)
      mwv207_dat_add_reloc(&tex_desc->dat, &cmd_buf, res->bo, tex_desc->tex_config.address[levelIdx], 0);

   memcpy(cmd_buf, &tex_desc->tex_config.address[levelCount],
          sizeof(struct mwv207_texture_config) - sizeof(uint32_t) * levelCount);
   tex_desc->dat.size = sizeof(struct mwv207_texture_config);
}

static uint32_t
mwv207_get_stage_offset(enum pipe_shader_type stage)
{
   switch (stage) {
   case PIPE_SHADER_VERTEX:
      return 0x10;
   case PIPE_SHADER_FRAGMENT:
      return 0x0;
   case PIPE_SHADER_GEOMETRY:
      return 0x30;
   case PIPE_SHADER_TESS_CTRL:
      return 0x20;
   case PIPE_SHADER_TESS_EVAL:
      return 0x28;
   case PIPE_SHADER_COMPUTE:
      return 0x0;
   default:
      return 0x50;
   }
}

void
mwv207_config_sampler(struct mwv207_context *ctx, enum pipe_shader_type stage)
{
   struct mwv207_sampler *sampler;
   struct mwv207_sampler_view *sampler_view;
   struct mwv207_texture_desc *tex_desc;
   struct mwv207_sampler_desc *sampler_desc;

   uint32_t *cmd_buf;
   uint32_t config_id = 0x0;
   uint32_t sampler_ctrl0, sampler_ctrl1, index;
   uint32_t stage_offset = mwv207_get_stage_offset(stage);
   uint32_t min_filter, mip_filter, mag_filter;

   if (stage == PIPE_SHADER_FRAGMENT || stage == PIPE_SHADER_COMPUTE)
      config_id = 0x1;

   mwv207_begin_record(ctx->cmd, &cmd_buf);

   MWV207_EMIT(cmd_buf, 0xE03,
      (ALIGN_BIT((uint32_t) (ENABLE), 0x5) & (0x20)) |
      (ALIGN_BIT((uint32_t) (ENABLE), 0xB) & (0x800)) |
      (ALIGN_BIT((uint32_t) (ENABLE), 0xA) & (0x400)));

   MWV207_EMIT(cmd_buf, 0xE03,
      (ALIGN_BIT((uint32_t) (ENABLE), 0xC) & (0x1000)) |
      (ALIGN_BIT((uint32_t) (ENABLE), 0xD) & (0x2000)));

   for (int i = 0; i < ctx->num_sampler_views[stage]; i++) {
      if (!ctx->sampler_views[stage][i])
         continue;

      sampler_view = mwv207_sampler_view(ctx->sampler_views[stage][i]);
      if (ctx->samplers[stage][i])
         sampler = ctx->samplers[stage][i];
      else
         sampler = &default_sampler;
      tex_desc = &sampler_view->tex_desc;
      sampler_desc = &sampler->sampler_desc;

      if (unlikely(ctx->ws->debug & 0x20))
         mwv207_print_samper(sampler_view, i);

      sampler_ctrl0 = sampler_desc->sampler_ctrl0 |
                      (ALIGN_BIT((uint32_t) (tex_desc->stencil), 0x10) & (0x10000));

      min_filter = sampler->state.min_img_filter;
      mip_filter = sampler->state.min_mip_filter;
      mag_filter = sampler->state.mag_img_filter;

      if (util_format_is_depth_or_stencil(sampler_view->base.texture->format)) {
         if (min_filter == PIPE_TEX_FILTER_LINEAR)
            min_filter = PIPE_TEX_FILTER_NEAREST;
         if (mip_filter == PIPE_TEX_FILTER_LINEAR)
            mip_filter = PIPE_TEX_FILTER_NEAREST;
         if (mag_filter == PIPE_TEX_FILTER_LINEAR)
            mag_filter = PIPE_TEX_FILTER_NEAREST;
      }

      sampler_ctrl0 = sampler_ctrl0 |
         (ALIGN_BIT((uint32_t) (mwv207_fliter_info[min_filter]), 0x9) & (0x600)) |
         (ALIGN_BIT((uint32_t) (mwv207_fliter_info[mip_filter]), 0xB) & (0x1800)) |
         (ALIGN_BIT((uint32_t) (mwv207_fliter_info[mag_filter]), 0xD) & (0x6000));

      if (tex_desc->fast_filter && sampler_desc->sampler_anisval == 0)
         sampler_ctrl0 |= (ALIGN_BIT(0x1, 0x17) & (0x800000));

      sampler_ctrl1 = sampler_desc->sampler_ctrl1 |
         (ALIGN_BIT((uint32_t) (tex_desc->srgb), 0x2) & (0x4));

      index = i + stage_offset;

      mwv207_reconfig_texture(sampler_view, sampler, false);

      MWV207_EMIT(cmd_buf, sampler_regs[config_id].sampler_ctrl0 + index , sampler_ctrl0);

      MWV207_EMIT(cmd_buf, sampler_regs[config_id].sampler_ctrl1 + index, sampler_ctrl1);

      MWV207_EMIT(cmd_buf, sampler_regs[config_id].sampler_lod_config + index, sampler_desc->sampler_lod_config);

      MWV207_EMIT(cmd_buf, sampler_regs[config_id].sampler_lod_bias + index, sampler_desc->sampler_lod_bias);

      MWV207_EMIT(cmd_buf, sampler_regs[config_id].sampler_anisval + index,
         tex_desc->cube ? 0 : sampler_desc->sampler_anisval);

      mwv207_add_dat(ctx, &cmd_buf, sampler_regs[config_id].tex_desc_address + index, &tex_desc->dat);

      MWV207_EMIT(cmd_buf, sampler_regs[config_id].tex_control + index, 0);

      if (tex_desc->tex_128bit) {
         mwv207_reconfig_texture(sampler_view, sampler, true);

         index = ctx->info.tex_record[stage][i] + stage_offset;

         MWV207_EMIT(cmd_buf, sampler_regs[config_id].sampler_ctrl0 + index , sampler_ctrl0);

         MWV207_EMIT(cmd_buf, sampler_regs[config_id].sampler_ctrl1 + index, sampler_ctrl1);

         MWV207_EMIT(cmd_buf, sampler_regs[config_id].sampler_lod_config + index, sampler_desc->sampler_lod_config);

         MWV207_EMIT(cmd_buf, sampler_regs[config_id].sampler_lod_bias + index, sampler_desc->sampler_lod_bias);

         MWV207_EMIT(cmd_buf, sampler_regs[config_id].sampler_anisval + index,
            tex_desc->cube ? 0 : sampler_desc->sampler_anisval);

         mwv207_add_dat(ctx, &cmd_buf, sampler_regs[config_id].tex_desc_address + index, &tex_desc->dat);

         MWV207_EMIT(cmd_buf, sampler_regs[config_id].tex_control + index, 0);
      }
   }

   MWV207_EMIT(cmd_buf, 0x5C8 + 7, 0xcccccccd);

   mwv207_wait_idle(&cmd_buf, 0x7);

   MWV207_EMIT(cmd_buf, 0x5310,
      (ALIGN_BIT((uint32_t) (ENABLE), 0x0) & (0x1)));

   if (stage == PIPE_SHADER_FRAGMENT || stage == PIPE_SHADER_COMPUTE) {
      for (int i = 0; i < ctx->num_sampler_views[stage]; i++) {
         if (!ctx->sampler_views[stage][i])
            continue;

         MWV207_EMIT(cmd_buf, sampler_regs[config_id].tex_command,
            (ALIGN_BIT(0x2, 0x1C) & (0xF0000000)) |
            (ALIGN_BIT((uint32_t) (i + stage_offset), 0x0) & (0x7F)));

         if ( ctx->info.tex_record[stage][i] != 0) {
            MWV207_EMIT(cmd_buf, sampler_regs[config_id].tex_command,
               (ALIGN_BIT(0x2, 0x1C) & (0xF0000000)) |
               (ALIGN_BIT((uint32_t) (ctx->info.tex_record[stage][i] + stage_offset), 0x0) & (0x7F)));
         }
      }
   }

   MWV207_EMIT(cmd_buf, 0xE03, (ALIGN_BIT((uint32_t) (ENABLE), 0x4) & (0x10)));

   mwv207_end_record(ctx->cmd, &cmd_buf);

   ctx->info.texture_dirty = false;

   if (stage == PIPE_SHADER_FRAGMENT && ctx->info.dsbi.is_dual) {
      ctx->base.set_sampler_views(&ctx->base, PIPE_SHADER_FRAGMENT,
                                  ctx->num_sampler_views[PIPE_SHADER_FRAGMENT] - 1,
                                  0, 1, false, NULL);
      void *first_samplers = NULL;
      void **samplers = &first_samplers;
      ctx->base.bind_sampler_states(&ctx->base, PIPE_SHADER_FRAGMENT,
                                    ctx->num_samplers[PIPE_SHADER_FRAGMENT] - 1,
                                    1, samplers);
   }
}

static uint32_t
mwv207_get_texture_swizzle(uint32_t swizzle,
                           uint32_t current_set,
                           const uint32_t *texture_swizzles)
{
   switch (swizzle) {
   case PIPE_SWIZZLE_NONE:
      return current_set;
   case PIPE_SWIZZLE_0:
      return 0x4;
   case PIPE_SWIZZLE_1:
      return 0x5;
   case PIPE_SWIZZLE_X:
      return texture_swizzles[0];
   case PIPE_SWIZZLE_Y:
      return texture_swizzles[1];
   case PIPE_SWIZZLE_Z:
      return texture_swizzles[2];
   case PIPE_SWIZZLE_W:
      return texture_swizzles[3];
   default:
      assert(!"WRONG SWIZZLE");
      return 0x4;
   }
}

static enum pipe_error
mwv207_get_texture_info(struct mwv207_context *ctx, struct mwv207_sampler_view *sampler_view)
{
   struct mwv207_level_info *level_info;
   struct mwv207_texture_desc *tex_desc = &sampler_view->tex_desc;
   struct mwv207_texture_config *tex_config = &tex_desc->tex_config;
   struct mwv207_resource *res = mwv207_resource(sampler_view->base.texture);

   enum pipe_format format = sampler_view->base.format;
   enum pipe_texture_target viewType = sampler_view->base.target;

   bool is_array = (viewType >= PIPE_TEXTURE_1D_ARRAY);
   uint32_t depth;
   uint32_t addressing_mode;
   uint32_t log_width, log_height, log_depth;
   uint32_t swizzle_r, swizzle_g, swizzle_b, swizzle_a;
   uint32_t txConfig2 = 0x30000;
   uint32_t horizonal_align = 0x1;
   uint32_t yuv_standard = 0x0;
   uint32_t levelCount = sampler_view->info.level_count;

   if (mwv207_is_128bit_sampler(res, sampler_view)) {
      mwv207_split_128bit((struct pipe_context *)ctx, sampler_view->base.texture, format);
      res = mwv207_resource(res->rg);
      format = mwv207_transfer_128bit(format);
      tex_desc->tex_128bit = true;
   }

   if (mwv207_get_texture_desc(format, tex_desc) != PIPE_OK) {
      const struct util_format_description *desc = util_format_description(format);
      printf("unsupport texture format %s\n", desc->name);
      assert(0);
   }

   level_info = &res->level_infos[sampler_view->info.first_level];

   if (util_format_is_compressed(format))
      addressing_mode = 0x0;
   else if (res->mode == MWV207_SUPERTILED)
      addressing_mode = 0x0;
   else if (res->base.last_level != 0)
      addressing_mode = 0x1;
   else
      addressing_mode = 0x3;

   if (util_format_is_compressed(format))
      horizonal_align = 0x1;
   else if (util_format_get_blocksize(format) >= 8)
      horizonal_align = 0x2;
   else if (res->mode == MWV207_TILED)
      horizonal_align = 0x0;
   else if (res->mode == MWV207_SUPERTILED)
      horizonal_align = 0x2;
   else
      horizonal_align = 0x1;

   if (is_array)
      depth = sampler_view->info.layer_count;
   else
      depth = level_info->depth;

   if (res->res_128_bit) {
      swizzle_r = 0x0;
      swizzle_g = 0x1;
      swizzle_b = 0x2;
      swizzle_a = 0x3;
    } else {
      swizzle_r = mwv207_get_texture_swizzle(sampler_view->base.swizzle_r, tex_desc->swizzles[0], tex_desc->swizzles);
      swizzle_g = mwv207_get_texture_swizzle(sampler_view->base.swizzle_g, tex_desc->swizzles[1], tex_desc->swizzles);
      swizzle_b = mwv207_get_texture_swizzle(sampler_view->base.swizzle_b, tex_desc->swizzles[2], tex_desc->swizzles);
      swizzle_a = mwv207_get_texture_swizzle(sampler_view->base.swizzle_a, tex_desc->swizzles[3], tex_desc->swizzles);
    }

   tex_desc->srgb = util_format_is_srgb(format);
   tex_desc->stencil = (format == PIPE_FORMAT_S8_UINT ||
                        format == PIPE_FORMAT_S8X24_UINT);
   tex_desc->msaa = (res->sample_info.nr_samples > 1);
   tex_desc->cube = (viewType == PIPE_TEXTURE_CUBE) || (viewType == PIPE_TEXTURE_CUBE_ARRAY);

   tex_desc->fast_filter = util_format_is_unorm(format) &&
      !util_format_is_compressed(format) &&
      !util_format_is_srgb(format) &&
      !util_format_is_depth_or_stencil(format) &&
      (format != PIPE_FORMAT_R10G10B10A2_UNORM) &&
      (format != PIPE_FORMAT_B10G10R10A2_UNORM) &&
      (mwv207_texture_type[viewType] == 0x2);

   tex_desc->tex_fetch[0] = level_info->width;
   tex_desc->tex_fetch[1] = level_info->height;
   tex_desc->tex_fetch[2] = depth;
   tex_desc->tex_fetch[3] = level_info->stride;

   log_width  = mwv207_log2(level_info->width);
   log_height = mwv207_log2(level_info->height);
   log_depth  = mwv207_log2(depth);

   memset(tex_config, 0, sizeof(struct mwv207_texture_config));

   for (uint32_t levelIdx = 0; levelIdx < levelCount; ++levelIdx) {
      struct mwv207_level_info *level = &res->level_infos[sampler_view->info.first_level + levelIdx];
      uint32_t physical = res->offset + level->offset
         + sampler_view->info.first_layer * level->layer_size;
      tex_config->address[levelIdx] = physical;
   }

   tex_config->tex_config =
      (ALIGN_BIT((uint32_t) (mwv207_texture_type[viewType]), 0x0) & (0x7)) |
      (ALIGN_BIT((uint32_t) ((tex_desc->format >> 0x0)), 0xD) & (0x3E000)) |
      (ALIGN_BIT((uint32_t) (addressing_mode), 0x14) & (0x300000));

   tex_config->tex_size =
      (ALIGN_BIT((uint32_t) (level_info->width), 0x0) & (0x7FFF)) |
      (ALIGN_BIT((uint32_t) (level_info->height), 0x10) & (0x7FFF0000));

   tex_config->tex_3d = (ALIGN_BIT((uint32_t) (depth), 0x0) & (0x3FFF));

   tex_config->tex_stride = level_info->stride;

   tex_config->tex_config_ext =
      (ALIGN_BIT((uint32_t) ((tex_desc->format >> 0x8)), 0x0) & (0x3F)) |
      (ALIGN_BIT((uint32_t) (swizzle_r), 0x8) & (0x700)) |
      (ALIGN_BIT((uint32_t) (swizzle_g), 0xC) & (0x7000)) |
      (ALIGN_BIT((uint32_t) (swizzle_b), 0x10) & (0x70000)) |
      (ALIGN_BIT((uint32_t) (swizzle_a), 0x14) & (0x700000)) |
      (ALIGN_BIT((uint32_t) (false), 0xF) & (0x8000)) |
      (ALIGN_BIT((uint32_t) (is_array), 0x18) & (0x1000000)) |
      (ALIGN_BIT((uint32_t) (horizonal_align), 0x1A) & (0x1C000000)) |
      (ALIGN_BIT((uint32_t) (ENABLE), 0x19) & (0x2000000)) |
      (ALIGN_BIT((uint32_t) (yuv_standard), 0x1D) & (0x20000000)) |
      (ALIGN_BIT(0x0, 0x1F) & (0x80000000)) |
      (ALIGN_BIT((uint32_t) (tex_desc->msaa), 0x17) & (0x800000)) |
      (ALIGN_BIT((uint32_t) (false), 0x7) & (0x80));

   tex_config->tex_base_lod =
      (ALIGN_BIT((uint32_t) (0), 0x0) & (0xF)) |
      (ALIGN_BIT((uint32_t) (levelCount - 1), 0x8) & (0xF00));

   if (tex_desc->int_format == 0)
      txConfig2 |= (ALIGN_BIT((uint32_t) (DISABLE), 0x10) & (0x10000));

   tex_config->tex_config2 = txConfig2 |
      (ALIGN_BIT((uint32_t) (tex_desc->int_format), 0x12) & (0xC0000));

   tex_config->tex_config3 = 0x0;

   tex_config->tex_config3 |= (ALIGN_BIT((uint32_t) (tex_desc->msaa), 0x3) & (0x8));

   tex_config->tex_size_ext =
      (ALIGN_BIT((uint32_t) (log_width), 0x0) & (0xFFFF)) |
      (ALIGN_BIT((uint32_t) (log_height), 0x10) & (0xFFFF0000));

   tex_config->tex_volume_ext =
      (ALIGN_BIT((uint32_t) (log_depth), 0x0) & (0xFFFF));

   tex_config->tex_slice = level_info->layer_size;

   tex_config->tex_log_size |= (ALIGN_BIT((uint32_t) (0), 0x1B) & (0x18000000));

   if ((tex_desc->format >> 0x8) == 0x14) {
      enum pipe_format liner_format = util_format_linear(format);
      uint32_t astc_size = (liner_format - PIPE_FORMAT_ASTC_4x4);
      uint32_t astc_srgb = tex_desc->srgb;

      assert(astc_size <= 0xD);

      tex_config->tex_log_size |= (ALIGN_BIT((uint32_t) (2), 0x1B) & (0x18000000));

      tex_config->tex_astc0 =
      tex_config->tex_astc1 =
      tex_config->tex_astc2 =
      tex_config->tex_astc3 =
         (ALIGN_BIT((uint32_t) (astc_size), 0x0) & (0xF)) |
         (ALIGN_BIT((uint32_t) (astc_srgb), 0x4) & (0x10)) |
         (ALIGN_BIT((uint32_t) (astc_size), 0x8) & (0xF00)) |
         (ALIGN_BIT((uint32_t) (astc_srgb), 0xC) & (0x1000)) |
         (ALIGN_BIT((uint32_t) (astc_size), 0x10) & (0xF0000)) |
         (ALIGN_BIT((uint32_t) (astc_srgb), 0x14) & (0x100000)) |
         (ALIGN_BIT((uint32_t) (astc_size), 0x18) & (0xF000000)) |
         (ALIGN_BIT((uint32_t) (astc_srgb), 0x1C) & (0x10000000));
   }

   return PIPE_OK;
}

static void *
mwv207_create_sampler_state(struct pipe_context *pctx,
                            const struct pipe_sampler_state *state)
{
   struct mwv207_sampler *sampler = CALLOC(1, sizeof(struct mwv207_sampler));
   struct mwv207_sampler_desc *sampler_desc = &sampler->sampler_desc;

   bool unnormalizedCoordinates;
   uint32_t compare_func = 0;
   int32_t  lodbias = 0, minlod = 0, maxlod = 0;

   if (!sampler)
      return NULL;

   sampler->state = *state;

   lodbias = mwv207_float_to_fixed(sampler->state.lod_bias, 8, 8);
   maxlod  = mwv207_float_to_fixed(sampler->state.max_lod,  5, 8);
   minlod  = mwv207_float_to_fixed(MAX2(0.0f, sampler->state.min_lod),  5, 8);
   unnormalizedCoordinates = 0;

   if (sampler->state.compare_mode)
      compare_func = mwv207_compare_func[sampler->state.compare_func];
   else
      compare_func = 0x0;

   sampler_desc->sampler_ctrl0 =
      (ALIGN_BIT((uint32_t) (mwv207_address_info[sampler->state.wrap_s]), 0x0) & (0x7)) |
      (ALIGN_BIT((uint32_t) (mwv207_address_info[sampler->state.wrap_t]), 0x3) & (0x38)) |
      (ALIGN_BIT((uint32_t) (mwv207_address_info[sampler->state.wrap_r]), 0x6) & (0x1C0)) |
      (ALIGN_BIT((uint32_t) (!sampler->state.unnormalized_coords), 0xF) & (0x8000)) |
      (ALIGN_BIT((uint32_t) (sampler->state.compare_mode), 0x11) & (0x20000)) |
      (ALIGN_BIT((uint32_t) (compare_func), 0x12) & (0x1C0000)) |
      (ALIGN_BIT(0x1, 0x15) & (0x600000));

   sampler_desc->sampler_ctrl1 =
      (ALIGN_BIT(0x2, 0x0) & (0x3));

   sampler_desc->sampler_ctrl1 |=
      (ALIGN_BIT((uint32_t) (unnormalizedCoordinates), 0x6) & (0x40));

   sampler_desc->sampler_lod_config =
      (ALIGN_BIT((uint32_t) (maxlod), 0x0) & (0x1FFF)) |
      (ALIGN_BIT((uint32_t) (minlod), 0x10) & (0x1FFF0000));

   sampler_desc->sampler_lod_bias =
      (ALIGN_BIT((uint32_t) (lodbias), 0x0) & (0xFFFF)) |
      (ALIGN_BIT((uint32_t) (lodbias != 0), 0x10) & (0x10000));

   if (sampler->state.max_anisotropy > 1) {
      assert(sampler->state.max_anisotropy <= 16);
      sampler_desc->sampler_anisval = mwv207_log2(sampler->state.max_anisotropy);
   }

   return sampler;
}

static void
mwv207_bind_sampler_states(struct pipe_context *pctx, enum pipe_shader_type stage,
                           unsigned start_slot, unsigned num_samplers,
                           void **samplers)
{
   struct mwv207_context *ctx = mwv207_context(pctx);
   uint32_t max_samplers = start_slot + num_samplers;

   for (int i = 0; i < num_samplers; ++i)
      ctx->samplers[stage][start_slot + i] = (struct mwv207_sampler *)samplers[i];

   max_samplers = MAX2(ctx->num_samplers[stage], max_samplers);

   for (int i = max_samplers - 1; i >= 0; i--) {
      if (ctx->samplers[stage][i] == NULL)
         max_samplers --;
      else
         break;
   }

   ctx->num_samplers[stage] = max_samplers;
   ctx->dirty |= MWV207_DIRTY_SAMPLERS;
}

static void
mwv207_delete_sampler_state(struct pipe_context *pctx,
                            void *sampler_state)
{
   FREE(sampler_state);
}

static struct pipe_sampler_view *
mwv207_create_sampler_view(struct pipe_context *pctx, struct pipe_resource *pres,
                           const struct pipe_sampler_view *state)
{
   struct mwv207_sampler_view *sampler_view = CALLOC_STRUCT(mwv207_sampler_view);

   sampler_view->base = *state;
   sampler_view->base.texture = NULL;
   pipe_resource_reference(&sampler_view->base.texture, pres);
   sampler_view->base.reference.count = 1;
   sampler_view->base.context = pctx;

   if (state->target == PIPE_BUFFER) {
      struct mwv207_resource *res = mwv207_resource(sampler_view->base.texture);
      int size = state->u.buf.size;
      int width = size / util_format_get_blocksize(state->format);

      sampler_view->info.first_layer = 0;
      sampler_view->info.last_layer  = 0;
      sampler_view->info.layer_count  = 1;

      sampler_view->info.first_level = 0;
      sampler_view->info.last_level  = 0;
      sampler_view->info.level_count  = 1;

      res->level_infos[0].width = width;
      res->level_infos[0].height = 1;
      res->level_infos[0].depth = width;
      res->level_infos[0].offset = state->u.buf.offset;
      res->level_infos[0].layer_size = size;
      res->level_infos[0].stride = size;
   } else {
      sampler_view->info.first_layer = state->u.tex.first_layer;
      sampler_view->info.last_layer  = state->u.tex.last_layer;
      sampler_view->info.layer_count  = state->u.tex.last_layer - state->u.tex.first_layer + 1;

      sampler_view->info.first_level = state->u.tex.first_level;
      sampler_view->info.last_level  = state->u.tex.last_level;
      sampler_view->info.level_count  = state->u.tex.last_level - state->u.tex.first_level + 1;
   }

   mwv207_get_texture_info(mwv207_context(pctx), sampler_view);

   return &sampler_view->base;
}

static void
mwv207_sampler_view_destroy(struct pipe_context *pctx,
                            struct pipe_sampler_view *pview)
{
   struct mwv207_sampler_view *view = mwv207_sampler_view(pview);

   pipe_resource_reference(&view->base.texture, NULL);

   FREE(view);
}

static void
mwv207_set_sampler_views(struct pipe_context *pctx,
                         enum pipe_shader_type stage,
                         unsigned start_slot, unsigned num_views,
                         unsigned unbind_num, bool take_ownership,
                         struct pipe_sampler_view **views)
{
   struct mwv207_sampler_view *sampler_view;
   struct mwv207_context *ctx = mwv207_context(pctx);
   uint32_t tex_128_offset = 0;
   uint32_t max_sampler_views = start_slot + num_views;

   ctx->info.tex_128bit = false;

   for (uint32_t i = 0; i < num_views; ++i) {
      sampler_view = mwv207_sampler_view(views[i]);

      if (sampler_view && sampler_view->tex_desc.tex_128bit) {
         ctx->info.tex_128bit = true;
         ctx->info.tex_record[stage][start_slot + i] = num_views + tex_128_offset;
         tex_128_offset += 1;
      } else
         ctx->info.tex_record[stage][start_slot + i] = 0;

      if (take_ownership) {
         pipe_sampler_view_reference(&ctx->sampler_views[stage][start_slot + i], NULL);
         ctx->sampler_views[stage][start_slot + i] = views[i];
      } else {
         pipe_sampler_view_reference(&ctx->sampler_views[stage][start_slot + i], views[i]);
      }
   }

   for (uint32_t i = 0; i < unbind_num; i++)
      pipe_sampler_view_reference(&ctx->sampler_views[stage][max_sampler_views + i], NULL);

   max_sampler_views = MAX2(ctx->num_sampler_views[stage], max_sampler_views);

   for (int i = max_sampler_views - 1; i >= 0; i--) {
      if (ctx->sampler_views[stage][i] == NULL)
         max_sampler_views --;
      else
         break;
   }

   ctx->num_sampler_views[stage] = max_sampler_views;
   ctx->dirty |= MWV207_DIRTY_SAMPLER_VIEWS;
}

void *
mwv207_get_sampler_state(struct mwv207_context *ctx)
{
   struct pipe_sampler_state state;
   memset(&state, 0, sizeof(struct pipe_sampler_state));
   state.wrap_s = PIPE_TEX_WRAP_CLAMP_TO_EDGE;
   state.wrap_t = PIPE_TEX_WRAP_CLAMP_TO_EDGE;
   state.wrap_r = PIPE_TEX_WRAP_CLAMP_TO_EDGE;

   return ctx->base.create_sampler_state(&ctx->base, &state);
}

void
mwv207_context_sampler_init(struct pipe_context *pctx)
{
   pctx->create_sampler_state = mwv207_create_sampler_state;
   pctx->bind_sampler_states  = mwv207_bind_sampler_states;
   pctx->delete_sampler_state = mwv207_delete_sampler_state;

   pctx->create_sampler_view  = mwv207_create_sampler_view;
   pctx->set_sampler_views    = mwv207_set_sampler_views;
   pctx->sampler_view_destroy = mwv207_sampler_view_destroy;

   default_sampler.state.wrap_s = PIPE_TEX_WRAP_CLAMP;
   default_sampler.state.wrap_t = PIPE_TEX_WRAP_CLAMP;
   default_sampler.state.wrap_r = PIPE_TEX_WRAP_CLAMP;
   default_sampler.state.min_img_filter = PIPE_TEX_FILTER_NEAREST;
   default_sampler.state.min_mip_filter = PIPE_TEX_MIPFILTER_NONE;
   default_sampler.state.mag_img_filter = PIPE_TEX_FILTER_NEAREST;
   default_sampler.sampler_desc.sampler_anisval = 0;
   default_sampler.sampler_desc.sampler_ctrl0 = 0x00382292;
   default_sampler.sampler_desc.sampler_ctrl1 = 0x00000002;
   default_sampler.sampler_desc.sampler_lod_bias = 0;
   default_sampler.sampler_desc.sampler_lod_config = 0x00000fff;
}
