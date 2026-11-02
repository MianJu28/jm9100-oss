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

#ifndef __MWV207_DEC_TIME_H__
#define __MWV207_DEC_TIME_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "util/os_time.h"

struct mwv207_codec_time {
   int64_t codec_start_time;
   int64_t codec_finish_time;
   int64_t codec_wait_hw_start_time;
   int64_t codec_wait_hw_finish_time;
   float codec_wait_hw_use_time;
   float codec_use_time;

   int64_t codec_last_call_time;
   float codec_call_gap_time;
};

void 
mwv207_codec_time_init(struct mwv207_codec_time *codecTime);

void 
mwv207_codec_time_record_start(struct mwv207_codec_time *codecTime);

void
 mwv207_codec_time_compute(struct mwv207_codec_time *codecTime);

#ifdef __cplusplus
}
#endif

#endif

