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

#ifndef HEVC_EXP_GOLOMB_H_
#define HEVC_EXP_GOLOMB_H_

/*------------------------------------------------------------------------------
         1. Include headers
------------------------------------------------------------------------------*/

#include <stdint.h>
#include "mwv207_decoder_sw_stream.h"

/*------------------------------------------------------------------------------
         2. Module defines
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
         3. Data types
------------------------------------------------------------------------------*/

/*------------------------------------------------------------------------------
         4. Function prototypes
------------------------------------------------------------------------------*/

uint32_t 
mwv207_hevc_decode_exp_golomb_unsigned(struct mwv207_strm_data *stream,
                                          uint32_t *value);
uint32_t 
mwv207_hevc_decode_exp_golomb_signed(struct mwv207_strm_data *stream,
                                        int32_t *value);

#endif

