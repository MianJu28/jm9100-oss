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

#ifndef SW_STREAM_H_
#define SW_STREAM_H_

#include <stdint.h>

struct mwv207_strm_data {
   const uint8_t *strm_buff_start;
   const uint8_t *strm_curr_pos;
   uint32_t bit_pos_in_word;
   uint32_t strm_buff_size;
   uint32_t strm_data_size;
   uint32_t strm_buff_read_bits;
   uint32_t remove_emul3_byte; /* signal the pre-removal of emulation prevention 3
                                    bytes */
   uint32_t emul_byte_count;
   uint32_t is_rb;
};

uint32_t mwv207_sw_get_bits(struct mwv207_strm_data *stream, uint32_t num_bits);
uint32_t
mwv207_sw_get_bits_unsigned_max(struct mwv207_strm_data *stream, uint32_t max_value);
uint32_t mwv207_sw_show_bits(struct mwv207_strm_data *stream, uint32_t num_bits);
uint32_t mwv207_sw_flush_bits(struct mwv207_strm_data *stream, uint32_t num_bits);
uint32_t mwv207_sw_is_byte_aligned(const struct mwv207_strm_data *);

#endif

