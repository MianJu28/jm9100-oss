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

#include "mwv207_vp9hwd_bool.h"

#define MODULUS_PARAM 13

static int
Vp9InvRecenterNonneg(int v, int m)
{
   if (v > (m << 1))
      return v;
   else if ((v & 1) == 0)
      return (v >> 1) + m;
   else
      return m - ((v + 1) >> 1);
}
static int
MergeIndex(int v, int n, int modulus)
{
   int max1 = (n - 1 - modulus / 2) / modulus + 1;
   if (v < max1)
      v = v * modulus + modulus / 2;
   else {
      int w;
      v -= max1;
      w = v;
      v += (v + modulus - modulus / 2) / modulus;
      while (v % modulus == modulus / 2
             || w != v - (v + modulus - modulus / 2) / modulus)
         v++;
   }
   return v;
}
static int
InvRemapProb(int v, int m)
{
   const int n = 255;
   v = MergeIndex(v, n - 1, MODULUS_PARAM);
   m--;
   if ((m << 1) <= n) {
      return 1 + Vp9InvRecenterNonneg(v + 1, m);
   } else {
      return n - Vp9InvRecenterNonneg(v + 1, n - 1 - m);
   }
}
vp9_prob
mwv207_vp9_read_prob_diff_update(struct mwv207_vp_bool_coder *bc, int oldp)
{
   int delp = mwv207_vp9_decode_sub_exp(bc, 4, 255);
   
   return (vp9_prob)InvRemapProb(delp, oldp);
}

static uint8_t
DWLPrivateAreaReadByte(const uint8_t *p)
{
   return *p;
}

uint32_t
mwv207_vp9_decode_bool(struct mwv207_vp_bool_coder *br, int32_t probability)
{
   uint32_t bit = 0;
   uint32_t split;
   uint32_t bigsplit;
   uint32_t count = br->count;
   uint32_t range = br->range;
   uint32_t value = br->value;
   uint32_t value_tmp = value;
   uint32_t range_tmp = range;
   (void)value_tmp;
   (void)range_tmp;

   split = 1 + (((range - 1) * probability) >> 8);
   bigsplit = (split << 24);

#ifdef BOOLDEC_TRACE
   printf("    p=%3d %9d/%4d s=%3d", probability, value >> 24, range, split);
#endif

   range = split;

   if (value >= bigsplit) {
      range = br->range - split;
      value = value - bigsplit;
      bit = 1;
   }

#ifdef BOOLDEC_TRACE
   printf(" --> %d\n", bit);
#endif

   if (range >= 0x80) {
      br->value = value;
      br->range = range;
#ifdef BOOLPROBS_TRACE
      printf("%-4d%-4d%-4d%-4d%-4d%-4d\n",
             value_tmp >> 24,
             range_tmp,
             probability,
             value >> 24,
             range,
             bit);
#endif
      return bit;
   } else {
      do {
         range += range;
         value += value;

         if (!--count) {

            if (br->read_len >= br->stream_len) {
               br->strm_error = 1;
               break;
            }
            count = 8;
            value |= DWLPrivateAreaReadByte(br->buffer);
            br->buffer++;
            br->read_len++;
            if (br->buffer >= (br->buffer_start + br->buffer_len))
               br->buffer -= br->buffer_len;
         }
      } while (range < 0x80);
   }

   br->count = count;
   br->value = value;
   br->range = range;

#ifdef BOOLPROBS_TRACE
   printf("%-4d%-4d%-4d%-4d%-4d%-4d\n",
          value_tmp >> 24,
          range_tmp,
          probability,
          value >> 24,
          range,
          bit);
#endif

   return bit;
}

static uint32_t
vp9_decode_bool_128(struct mwv207_vp_bool_coder *br)
{
   uint32_t bit = 0;
   uint32_t split;
   uint32_t bigsplit;
   uint32_t count = br->count;
   uint32_t range = br->range;
   uint32_t value = br->value;
   uint32_t value_tmp = value;
   uint32_t range_tmp = range;
   (void)value_tmp;
   (void)range_tmp;

   split = (range + 1) >> 1;
   bigsplit = (split << 24);

#ifdef BOOLDEC_TRACE
   printf("    p=%3d %9d/%4d s=%3d", 128, value >> 24, range, split);
#endif

   range = split;

   if (value >= bigsplit) {
      range = (br->range - split);
      value = (value - bigsplit);
      bit = 1;
   }

#ifdef BOOLDEC_TRACE
   printf(" --> %d\n", bit);
#endif

   if (range >= 0x80) {
      br->value = value;
      br->range = range;
#ifdef BOOLPROBS_TRACE
      printf("%-4d%-4d%-4d%-4d%-4d%-4d\n",
             value_tmp >> 24,
             range_tmp,
             128,
             value >> 24,
             range,
             bit);
#endif
      return bit;
   } else {
      range <<= 1;
      value <<= 1;

      if (!--count) {

         if (br->read_len >= br->stream_len) {
            br->strm_error = 1;
            return 0;
         }
         count = 8;
         value |= DWLPrivateAreaReadByte(br->buffer);
         br->buffer++;
         br->read_len++;
         if (br->buffer >= (br->buffer_start + br->buffer_len))
            br->buffer -= br->buffer_len;
      }
   }

   br->count = count;
   br->value = value;
   br->range = range;

#ifdef BOOLPROBS_TRACE
   printf("%-4d%-4d%-4d%-4d%-4d%-4d\n",
          value_tmp >> 24,
          range_tmp,
          128,
          value >> 24,
          range,
          bit);
#endif

   return bit;
}

static int
GetUnsignedBits(uint32_t num_values)
{
   int32_t cat = 0;
   if (num_values <= 1)
      return 0;
   num_values--;
   while (num_values > 0) {
      cat++;
      num_values >>= 1;
   }
   return cat;
}

static uint32_t
BoolDecodeUniform(struct mwv207_vp_bool_coder *bc, uint32_t n)
{
   uint32_t value, v;
   int32_t l = GetUnsignedBits(n);
   int32_t m = (1 << l) - n;
   if (!l)
      return 0;
   value = mwv207_vp9_read_bits(bc, l - 1);
   if (value >= m) {
      v = mwv207_vp9_read_bits(bc, 1);
      value = (value << 1) - m + v;
   }
   return value;
}

uint32_t
mwv207_vp9_decode_sub_exp(struct mwv207_vp_bool_coder *bc, uint32_t k, uint32_t num_syms)
{
   int32_t i = 0, mk = 0, value = 0;

   while (1) {
      int32_t b = (i ? k + i - 1 : k);
      int32_t a = (1 << b);
      if (num_syms <= mk + 3 * a) {
         value = BoolDecodeUniform(bc, num_syms - mk) + mk;
         break;
      } else {
         value = mwv207_vp9_read_bits(bc, 1);
         if (value) {
            i++;
            mk += a;
         } else {
            value = mwv207_vp9_read_bits(bc, b) + mk;
            break;
         }
      }
   }
   return value;
}

void
mwv207_vp9_bool_start(struct mwv207_vp_bool_coder *br,
                     const uint8_t *stream,
                     uint32_t strm_len,
                     const uint8_t *strm_buf,
                     uint32_t buf_len)
{
   int marker_bit;
   uint8_t buffer[4];

   br->lowvalue = 0;
   br->range = 255;
   br->count = 8;
   br->buffer = stream;
   br->buffer_start = strm_buf;
   br->read_len = 0;

   for (uint32_t i = 0; i < 4; i++) {
      buffer[i] = DWLPrivateAreaReadByte(br->buffer);
      br->buffer++;
      if (br->buffer >= (br->buffer_start + buf_len)) {

         br->buffer -= buf_len;
      }
   }
   br->value =
      (buffer[0] << 24) + (buffer[1] << 16) + (buffer[2] << 8) + (buffer[3]);
   br->read_len += 4;

   br->buffer_len = buf_len;
   br->stream_len = strm_len;
   br->strm_error = br->read_len > br->stream_len;

   marker_bit = vp9_decode_bool_128(br);
   STREAM_TRACE("marker_bit", marker_bit);

   if (marker_bit != 0)
      br->strm_error = 1;
}

void
mwv207_vp9_bool_stop(struct mwv207_vp_bool_coder *bc)
{
   (void)bc;
}

uint32_t
mwv207_vp9_read_bits(struct mwv207_vp_bool_coder *br, int32_t bits)
{
   uint32_t z = 0;
   int32_t bit;

   for (bit = bits - 1; bit >= 0; bit--) {
      z |= (vp9_decode_bool_128(br) << bit);
   }

   return z;
}

