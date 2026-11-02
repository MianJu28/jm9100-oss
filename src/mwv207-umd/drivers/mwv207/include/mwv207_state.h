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
#ifndef MWV207_STATE_H
#define MWV207_STATE_H

#include "pipe/p_state.h"

struct mwv207_context;
struct mwv207_sample_info;

struct mwv207_element {
   struct pipe_vertex_element base;
   uint32_t attribue_config;
   uint32_t fetch_size;
   bool fetch_break;
   uint32_t weight;
};

struct mwv207_elements_state {
   struct mwv207_element elements[PIPE_MAX_ATTRIBS];
   uint32_t num_elements;
};

struct mwv207_rasterizer_state {
   struct pipe_rasterizer_state base;
   uint32_t line_scale;
   uint32_t cull_mode;
   uint32_t flat_mode;
   uint32_t fill_mode;
   uint32_t tfb_control;
   bool depth_clamp;
   uint32_t pa_config;
   uint16_t sprite_coord_mode;
   uint16_t sprite_coord_enable;
};

struct mwv207_blend_state {
   struct pipe_blend_state blend_state;
   uint32_t alpha_config;
   uint32_t alpha_blend;
   uint32_t logic_config;
   uint32_t color_mask[PIPE_MAX_COLOR_BUFS];
   uint32_t blend_config[PIPE_MAX_COLOR_BUFS];
   uint32_t blend_configex[PIPE_MAX_COLOR_BUFS];
   uint32_t dest_read[PIPE_MAX_COLOR_BUFS];
   /* HW extended blend-equation code for GL_KHR_blend_equation_advanced
    * (0 when the state does not use an advanced equation). */
   uint32_t advanced_ext;
};

struct mwv207_depth_stencil_alpha_state {
   struct pipe_depth_stencil_alpha_state base;
   struct pipe_stencil_ref stencil_ref;
   bool stencil_test;
   bool front_not_keep;
   bool back_not_keep;
   bool depth_bounds_test;
   bool early_z;
   float min_depth_bounds;
   float max_depth_bounds;
   uint32_t func;
   uint32_t mode;
   uint32_t depth_config;
   uint32_t stencil_op[2];
   uint32_t stencil_config[2];
   uint32_t stencil_back_config[2];
   uint32_t alpha_config;
   uint32_t alpha_reference;
};

struct mwv207_stencil_ref {
   struct pipe_stencil_ref ref;
   uint32_t stencil_reference[2];
   uint32_t stencil_back_reference[2];
};

struct mwv207_stream_output_target {
   struct pipe_stream_output_target base;
   uint32_t is_actived;
   struct mwv207_bo *bo;
};

struct mwv207_global_bindings_state {
   struct pipe_resource *buf[0x20];
   uint32_t enable_mask;
};

struct mwv207_stream_output {
   uint32_t num_targets;
   struct pipe_stream_output_target *targets[4];
   uint32_t offsets[4];
};

static inline struct mwv207_stream_output_target *
mwv207_stream_output_target(struct pipe_stream_output_target *r)
{
   return (struct mwv207_stream_output_target *)r;
}

void
mwv207_context_state_init(struct pipe_context *pctx);

void
mwv207_context_vertex_init(struct pipe_context *pctx);

void
mwv207_context_blend_init(struct pipe_context *pctx);

void
mwv207_context_zsa_init(struct pipe_context *pctx);

void
mwv207_set_framebuffer_state(struct pipe_context *pctx,
                             const struct pipe_framebuffer_state *state);

void
mwv207_config_uniform(struct mwv207_context *ctx, enum pipe_shader_type stage);

void
mwv207_emit_kernel_params(struct mwv207_context *ctx, const struct pipe_grid_info *info);
#endif
