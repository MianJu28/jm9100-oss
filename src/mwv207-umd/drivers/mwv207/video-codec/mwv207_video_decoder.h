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

#ifndef MWV207_VIDEO_DECODER_H
#define MWV207_VIDEO_DECODER_H

#include "pipe/p_context.h"
#include "pipe/p_state.h"
#include "pipe/p_video_codec.h"
#include "vl/vl_decoder.h"
#include "vl/vl_video_buffer.h"

#include "mwv207_video_dec.h"
#include "mwv207_codec_time.h"
#include "mwv207_log.h"
#include "mwv207_resource.h"
#include "mwv207_video_buffer.h"
#include "mwv207_video_cs.h"
#include "mwv207_video_state.h"

#define MWV207_NUM_BUFFERS 4

struct mwv207_video_decoder {
   struct pipe_video_codec base;

   struct pipe_screen *screen;
   struct mwv207_winsys *ws;

   uint32_t frame_number;

   uint32_t cur_buffer;

   struct mwv207_video_cs cs;

   union {
      struct mwv207_h264_picture_desc h264;
      struct mwv207_h265_picture_desc h265;
   } picture_desc;

   struct mwv207_video_buffer *bs_buffers[MWV207_NUM_BUFFERS];
   void *bs_ptr;
   uint32_t bs_size;

   void *streams[16];
   int streams_len[16];
   int streams_num;

   struct mwv207_video_dec *decoder;

   enum mwv207_video_format video_format;
   struct mwv207_codec_time codec_time;
};

static inline struct mwv207_video_decoder *
mwv207_video_dec(struct pipe_video_codec *decoder)
{
   return (struct mwv207_video_decoder *)decoder;
}

struct pipe_video_codec *
mwv207_create_decoder(struct pipe_context *context,
                     const struct pipe_video_codec *decoder);

void 
mwv207_9200_video_dec_picture(struct mwv207_video_decoder *decoder,
                                  struct pipe_video_buffer *target);

#endif

