#include "pipe/p_state.h"
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
#ifndef MWV207_RESOURCE_H
#define MWV207_RESOURCE_H

#include "util/u_range.h"

#define R 0
#define G 1
#define B 2
#define A 3

#define CACHE_MODE 0x1

/* video-codec compatibility names (matches the jemoic 22.3-era headers) */
#define MWV207_RESOURCE_FLAG_VIDEO_BUFFER  (PIPE_RESOURCE_FLAG_DRV_PRIV << 0)
#define MWV207_RESOURCE_FLAG_SUBALLOC      (PIPE_RESOURCE_FLAG_DRV_PRIV << 1)

#define ALIGN_NP2(n, align) (((n) + (align) - 1) - (((n) + (align) - 1) % (align)))

struct pipe_screen;
struct mwv207_context;

enum mwv207_tile_mode {
   MWV207_LINEAR,
   MWV207_TILED,
   MWV207_SUPERTILED,
};

struct mwv207_transfer {
   struct pipe_transfer base;
   struct pipe_resource *staging_res;
};

struct mwv207_tile_info {
   struct mwv207_bo *bo;
   uint32_t layer_offset;
   bool compressed;
   int32_t compressed_format;
   uint32_t clear_values[2];
   bool enable;
};

struct mwv207_level_info {

   uint32_t width;
   uint32_t height;
   uint32_t depth;

   uint32_t alloced_width;
   uint32_t alloced_height;

   uint32_t stride;
   uint32_t offset;
   uint32_t layer_size;
   uint32_t size;

   uint32_t config;
   uint32_t src_configex;
   uint32_t dst_configex;
   uint32_t src_compress;
   uint32_t dst_compress;
   uint32_t blit_config;
};

struct mwv207_sample_info {
   bool is_msaa;

   uint8_t xscale;
   uint8_t yscale;
   uint8_t nr_samples;
};

struct mwv207_resource {
   struct pipe_resource base;
   struct mwv207_context *ctx;

   uint32_t offset;
   uint32_t size;

   enum mwv207_tile_mode mode;
   struct mwv207_tile_info ts;
   struct mwv207_level_info level_infos[PIPE_MAX_TEXTURE_LEVELS];
   struct util_range valid_buffer_range;

   struct mwv207_bo *bo;
   bool need_decompress;
   bool linear_output;

   struct pipe_resource *linear;

   bool res_128_bit;
   bool src_dirty;
   bool rgba_dirty;
   struct pipe_resource *rg;
   struct pipe_resource *ba;

   struct mwv207_sample_info sample_info;
};

static inline struct mwv207_resource *
mwv207_resource(struct pipe_resource *r)
{
   return (struct mwv207_resource *)r;
}

void
mwv207_screen_resource_init(struct pipe_screen *pscreen);

void
mwv207_context_resource_init(struct pipe_context *pctx);

void
mwv207_wait_resource(struct pipe_context *pctx, struct pipe_resource *resource);

enum pipe_error
mwv207_get_image_info(struct mwv207_resource *res);

void
mwv207_split_128bit(struct pipe_context *pctx, struct pipe_resource *pres, enum pipe_format format);

void
mwv207_merge_128bit(struct pipe_context *pctx, struct pipe_resource *pres);

enum pipe_format
mwv207_transfer_128bit(enum pipe_format input);

#endif
