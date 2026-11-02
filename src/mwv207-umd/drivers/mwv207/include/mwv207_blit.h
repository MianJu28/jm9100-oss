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
#ifndef MWV207_BLIT_H
#define MWV207_BLIT_H

struct mwv207_task_info {
   uint32_t src_x;
   uint32_t src_y;
   uint32_t dst_x;
   uint32_t dst_y;
   uint32_t width;
   uint32_t height;
   struct mwv207_sample_info src_sampleInfo;
   struct mwv207_sample_info dst_sampleInfo;
};

struct mwv207_clear_info {
   struct mwv207_resource *res;
   struct pipe_box box;
   struct mwv207_tex_info tex_info;

   uint32_t aspectMask;
   union pipe_color_union color;
   double depth;
   unsigned stencil;
};

struct mwv207_region_info {
   struct mwv207_resource *src;
   struct mwv207_resource *dst;
   struct mwv207_tex_info src_info;
   struct mwv207_tex_info dst_info;
   struct pipe_box src_box;
   struct pipe_box dst_box;
   enum pipe_format src_format;
   enum pipe_format dst_format;
   bool flip_y;
};

enum pipe_error
mwv207_copy_buffer(struct mwv207_context *ctx,
                   struct mwv207_region_info info);

enum pipe_error
mwv207_copy_image(struct mwv207_context *ctx,
                  struct mwv207_region_info info);

enum pipe_error
mwv207_fill_buffer(struct mwv207_context *ctx,
                   struct mwv207_bo *mem,
                   uint32_t offset,
                   uint32_t size,
                   uint32_t data);

enum pipe_error
mwv207_update_buffer(struct mwv207_context *ctx,
                     struct mwv207_region_info info);

void
mwv207_blit_init(struct pipe_context *pctx);

void
mwv207_disable_tile(struct mwv207_context *ctx,
                    struct mwv207_resource *res);

void
mwv207_decompress_fb(struct pipe_context *pctx);

void
mwv207_blit_resource_box(struct pipe_context *pctx, struct pipe_resource *dst,
                         struct pipe_resource *src, int level,
                         const struct pipe_box *box);

#endif
