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

#ifndef ASSERT
#include <assert.h>
#define ASSERT(expr) assert(expr)
#endif
#include "mwv207_decoder_sw_util.h"

uint32_t
mwv207_sw_count_leading_zeros(uint32_t value, uint32_t length)
{
   uint32_t zeros = 0;
   uint32_t mask = 1 << (length - 1);

   ASSERT(length <= 32);

   while (mask && !(value & mask)) {
      zeros++;
      mask >>= 1;
   }
   return (zeros);
}

uint32_t
mwv207_sw_num_bits(uint32_t value)
{
   return 32 - mwv207_sw_count_leading_zeros(value, 32);
}

uint8_t *
mwv207_sw_turn_around(
   const uint8_t *strm, const uint8_t *buf, uint8_t *tmp_buf, uint32_t buf_size, uint32_t num_bits)
{
   uint32_t bytes = (num_bits + 7) / 8;
   uint32_t is_turn_around = 0;
   if ((strm + bytes) > (buf + buf_size))
      is_turn_around = 1;

   if ((uint64_t)strm - (uint64_t)buf < 2) {
      ASSERT(is_turn_around == 0);
      is_turn_around = 2;
   }

   if (is_turn_around == 0) {
      return NULL;
   } else if (is_turn_around == 1) {
      int32_t i;
      uint32_t bytes_left = (uint32_t)((uint64_t)(buf + buf_size) - (uint64_t)strm);

      for (i = -3; i < (int32_t)0; i++) {
         tmp_buf[3 + i] = *(strm + i);
      }
      for (i = 0; i < (int32_t)bytes_left; i++) {
         tmp_buf[3 + i] = *(strm + i);
      }

      for (i = 0; i < (int32_t)(bytes - bytes_left); i++) {
         tmp_buf[3 + bytes_left + i] = *(buf + i);
      }

      return tmp_buf + 3;
   } else {
      int32_t i;
      uint32_t left_byte = (uint32_t)((uint64_t)strm - (uint64_t)buf);

      for (i = 0; i < 2; i++) {
         tmp_buf[i] = *(buf + buf_size - 2 + i);
      }

      for (i = 0; i < (int32_t)(bytes + left_byte); i++) {
         tmp_buf[i + 2] = *(buf + i);
      }

      return (tmp_buf + 2 + left_byte);
   }
}

