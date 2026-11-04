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

#ifndef MWV207_VDIEO_CS_H
#define MWV207_VDIEO_CS_H
#include "mwv207_codec_time.h"
#include "mwv207_video_reg_buf.h"
#include "util/u_inlines.h"

struct mwv207_video_cs {
   struct mwv207_winsys *ws;
   struct mwv207_cmd *cmd;
   struct mwv207_codec_time *codec_time;

   uint32_t *cmdbuf_start;
   uint32_t *cmdbuf_cur;
   
   uint32_t *regbuf_cur;
};

void mwv207_video_cs_init(struct mwv207_video_cs *cs, struct mwv207_winsys *ws);
void mwv207_video_cs_release(struct mwv207_video_cs *cs);
void mwv207_video_cs_submit(struct mwv207_video_cs *cs,
                           struct mwv207_video_reg_buf *cmdbuf);
void mwv207_video_cs_submit_wait(struct mwv207_video_cs *cs,
                                struct mwv207_video_reg_buf *cmdbuf);

#endif

