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

#ifndef mwv207_video_buffer_h
#define jmpgu_video_buffer_h

#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include "pipe/p_video_codec.h"

#include <stdint.h>
#include "mwv207_drm_winsys.h"
#include "mwv207_resource.h"
#include "vl/vl_defines.h"

#define MWV207_MACROBLOCK_WIDTH 64
#define MWV207_MACROBLOCK_HEIGHT 64

struct mwv207_offset {
   uint32_t surface_origin_width;
   uint32_t surface_origin_height;
   uint32_t frame_width;
   uint32_t frame_height;
   uint32_t tiled_data_wstride;
   uint32_t tiled_data_hstride;
   uint32_t ref_cstride;
   uint32_t ref_ystride;
   uint32_t luma_size;
   uint32_t pic_size_in_mbs;
   uint32_t dmv_mem_size;
   uint32_t pic_buff_size;
   uint32_t pp_buff_size;
   uint32_t sync_mc_offset;
   uint32_t dir_mv_offset;
   uint32_t cbs_tbl_offsety;
   uint32_t cbs_tbl_offsetc;
   uint32_t fake_tbly_size;
   uint32_t fake_tblc_size;

   uint32_t chroma_base_offset;
};

/**
 * implementation of a planar ycbcr buffer
 */

struct mwv207_video_buffer {
   struct pipe_video_buffer base;
   unsigned num_planes;
   struct pipe_resource *resources[VL_NUM_COMPONENTS];
   struct pipe_sampler_view *sampler_view_planes[VL_NUM_COMPONENTS];
   struct pipe_sampler_view *sampler_view_components[VL_NUM_COMPONENTS];
   struct pipe_surface *surfaces[VL_MAX_SURFACES];

   unsigned usage;
   struct mwv207_resource *res;
   void *virtual_addr;

   struct mwv207_offset mwv207_offset_info;
   uint32_t tiled_pic_size;
};

static inline void
mwv207_video_buffer_adjust_size(unsigned *width,
                               unsigned *height,
                               unsigned plane,
                               enum pipe_video_chroma_format chroma_format,
                               bool interlaced)
{
   if (interlaced) {
      *height /= 2;
   }
   if (plane > 0) {
      if (chroma_format == PIPE_VIDEO_CHROMA_FORMAT_420) {
         *width /= 2;
         *height /= 2;
      } else if (chroma_format == PIPE_VIDEO_CHROMA_FORMAT_422) {
         *width /= 2;
      }
   }
}

/**
 * get subformats for each plane
 */
void mwv207_get_video_buffer_formats(
   struct pipe_screen *screen,
   enum pipe_format format,
   enum pipe_format out_format[VL_NUM_COMPONENTS]);

/**
 * get YUV plane order
 */
const unsigned *mwv207_video_buffer_plane_order(enum pipe_format format);

/**
 * get maximum size of video buffers
 */
unsigned mwv207_video_buffer_max_size(struct pipe_screen *screen);

/**
 * check if video buffer format is supported for a codec/profile
 * can be used as default implementation of screen->is_video_format_supported
 */
bool
mwv207_video_buffer_is_format_supported(struct pipe_screen *screen,
                                       enum pipe_format format,
                                       enum pipe_video_profile profile,
                                       enum pipe_video_entrypoint entrypoint);

/*
 * set the associated data for the given video buffer
 */
void 
mwv207_video_buffer_set_associated_data(
   struct pipe_video_buffer *vbuf,
   struct pipe_video_codec *vcodec,
   void *associated_data,
   void (*destroy_associated_data)(void *));

/*
 * get the associated data for the given video buffer
 */
void *
mwv207_video_buffer_get_associated_data(struct pipe_video_buffer *vbuf,
                                             struct pipe_video_codec *vcodec);

/**
 * fill a resource template for the given plane
 */
void 
mwv207_video_buffer_template(struct pipe_resource *templ,
                                 const struct pipe_video_buffer *templat,
                                 enum pipe_format resource_format,
                                 unsigned depth,
                                 unsigned array_size,
                                 unsigned usage,
                                 unsigned plane);

/**
 * creates a video buffer, can be used as a standard implementation for
 * pipe->create_video_buffer
 */
struct pipe_video_buffer *
mwv207_video_buffer_create(struct pipe_context *pipe,
                          const struct pipe_video_buffer *templat);

void 
mwv207_video_buffer_destroy(struct pipe_video_buffer *buffer);

/**
 * extended create function, gets depth, array_size, usage and formats for
 * each plane seperately
 */
struct pipe_video_buffer *mwv207_video_buffer_create_ex(
   struct pipe_context *pipe,
   const struct pipe_video_buffer *templat,
   const enum pipe_format resource_formats[VL_NUM_COMPONENTS],
   unsigned depth,
   unsigned array_size,
   unsigned usage);

/**
 * even more extended create function, provide the pipe_resource for each
 * plane
 */
struct pipe_video_buffer *
mwv207_video_buffer_create_ex2(
   struct pipe_context *pipe,
   const struct pipe_video_buffer *templat,
   struct pipe_resource *resources[VL_NUM_COMPONENTS]);

struct pipe_video_buffer *
mwv207_video_buffer_create_as_resource(struct pipe_context *pipe,
                                      const struct pipe_video_buffer *tmpl);

struct pipe_video_buffer *
mwv207_pipe_video_buffer_create(struct pipe_context *pipe,
                               const struct pipe_video_buffer *tmpl);

struct mwv207_video_buffer *
mwv207_video_buffer_create_2(struct pipe_context *pipe,
                            unsigned size,
                            unsigned usage);
void mwv207_video_buffer_alloc_buffer(struct pipe_context *pipe,
                                     struct mwv207_video_buffer *buffer,
                                     unsigned size,
                                     unsigned usage);

void 
mwv207_video_buffer_clear(struct pipe_context *context,
                              struct mwv207_video_buffer *buffer);

#endif

