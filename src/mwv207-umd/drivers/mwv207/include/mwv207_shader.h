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
#ifndef MWV207_SHADER_H
#define MWV207_SHADER_H

#include "pipe/p_defines.h"
#include "pipe/p_state.h"
#include "compiler/shader_info.h"

struct nir_shader_compiler_options;
struct nir_shader;
struct tgsi_token;
struct pipe_screen;
struct mwv207_screen;
struct mwv207_context;
struct mwv207_shader;

struct mwv207_shader_state {

   struct nir_shader *nir;

   struct mwv207_shader *shader_list;

   struct mwv207_shader *current;

   struct mwv207_bo *bo;

   uint32_t bo_offset;

   struct pipe_stream_output_info base_stream_output;

   struct pipe_stream_output_info stream_output;
};

static inline struct mwv207_shader_state *
mwv207_shader_state(void *state)
{
   return (struct mwv207_shader_state *)state;
}

static inline void
mwv207_config_arrays(uint32_t *arrays, uint32_t bit_size, uint32_t index, uint32_t value)
{
   uint32_t num = (index * bit_size) / 32;
   uint32_t shift = (index * bit_size) % 32;

   assert(value < 1 << bit_size);

   arrays[num] |= value << shift;
}

bool
mwv207_set_current_shader(struct mwv207_context *ctx, struct mwv207_shader_state *state);

void
mwv207_config_vs(struct mwv207_context *ctx);

void
mwv207_config_fs(struct mwv207_context *ctx);

void
mwv207_config_tcs(struct mwv207_context *ctx);

void
mwv207_config_tes(struct mwv207_context *ctx);

void
mwv207_config_gs(struct mwv207_context *ctx);

enum pipe_error
mwv207_config_link(struct mwv207_context *ctx);

const void *
mwv207_get_compiler_options(struct pipe_screen *screen,
                            enum pipe_shader_ir ir,
                            enum pipe_shader_type shader);

void
mwv207_context_shader_init(struct pipe_context *pctx);

#endif
