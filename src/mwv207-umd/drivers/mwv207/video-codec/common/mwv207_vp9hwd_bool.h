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

#ifndef __VP9_BOOL_H__
#define __VP9_BOOL_H__

#include <stdint.h>
#define END_OF_STREAM 0xFFFFFFFFU

#include <time.h>
#define STREAM_TRACE(x, y)
#define STREAM_TRACE_CFF(x, y, a)
#define VP9DEC_DEBUG(x)

#define CHECK_END_OF_STREAM(s)                                               \
   if ((s) == END_OF_STREAM)                                                 \
   return (s)

struct mwv207_vp_bool_coder {
   uint32_t lowvalue;
   uint32_t range;
   uint32_t value;
   int32_t count;
   uint32_t read_len;
   const uint8_t *buffer;
   const uint8_t *buffer_start;
   uint32_t BitCounter;
   uint32_t buffer_len;
   uint32_t stream_len;
   uint32_t strm_error;
};
typedef uint8_t vp9_prob;

extern void mwv207_vp9_bool_start(struct mwv207_vp_bool_coder *bc,
                                 const uint8_t *stream,
                                 uint32_t strm_len,
                                 const uint8_t *strm_buf,
                                 uint32_t buf_len);
extern uint32_t
mwv207_vp9_decode_bool(struct mwv207_vp_bool_coder *bc, int32_t probability);
extern uint32_t mwv207_vp9_decode_bool_128(struct mwv207_vp_bool_coder *bc);
extern void mwv207_vp9_bool_stop(struct mwv207_vp_bool_coder *bc);

uint32_t
mwv207_vp9_decode_sub_exp(struct mwv207_vp_bool_coder *bc, uint32_t k, uint32_t num_syms);
uint32_t mwv207_vp9_read_bits(struct mwv207_vp_bool_coder *br, int32_t bits);
vp9_prob
mwv207_vp9_read_prob_diff_update(struct mwv207_vp_bool_coder *bc, int oldp);

#endif

