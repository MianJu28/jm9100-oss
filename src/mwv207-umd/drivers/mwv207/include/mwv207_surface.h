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
#ifndef MWV207_SURFACE_H
#define MWV207_SURFACE_H

#include "pipe/p_state.h"
#include "mwv207_format.h"

struct pipe_context;

struct mwv207_tex_info {
   uint32_t first_level;
   uint32_t last_level;
   uint32_t level_count;
   uint32_t first_layer;
   uint32_t last_layer;
   uint32_t layer_count;
};

struct mwv207_pixel_info {
   struct mwv207_pixel_desc peDesc;

   uint32_t pixel_config;
   uint32_t pe_config;
   uint32_t pixel_config_ex;

   uint32_t has_stencil;
   uint32_t depth_config;
   uint32_t depth_normalize;

   uint32_t memory_config;
   uint32_t memory_config_ex;
};

struct mwv207_surface {
   struct pipe_surface base;
   uint32_t offset;
   uint32_t size;
   uint32_t stride;
   struct mwv207_tex_info info;
   struct mwv207_pixel_info peInfo;

   bool pixel_128bit;
};

struct mwv207_framebuffer_state {
   struct pipe_framebuffer_state base;
   uint32_t pixel_config;
   uint32_t depth_config;
};

struct mwv207_sampler_desc {
   uint32_t sampler_ctrl0;
   uint32_t sampler_ctrl1;
   uint32_t sampler_lod_config;
   uint32_t sampler_lod_bias;
   uint32_t sampler_anisval;
   uint32_t tex_desc_address;
   uint32_t tex_control;
   uint32_t tex_command;
};

struct mwv207_sampler {
   struct pipe_sampler_state state;
   struct mwv207_sampler_desc sampler_desc;
};

struct mwv207_sampler_view {
   struct pipe_sampler_view base;
   struct mwv207_tex_info info;
   struct mwv207_texture_desc tex_desc;
};

static inline struct mwv207_sampler_view *
mwv207_sampler_view(struct pipe_sampler_view *pview)
{
   return (struct mwv207_sampler_view *)pview;
}

static inline struct mwv207_surface *
mwv207_surface(struct pipe_surface *pipe)
{
   return (struct mwv207_surface *)pipe;
}

void
mwv207_config_sampler(struct mwv207_context *ctx, enum pipe_shader_type stage);

void
mwv207_context_surface_init(struct pipe_context *context);

void
mwv207_context_sampler_init(struct pipe_context *pctx);

void
mwv207_update_renderInfo(struct mwv207_surface *surface);

void *
mwv207_get_sampler_state(struct mwv207_context *ctx);

#endif
