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
#include "mwv207_decoder_sw_stream.h"
#include "mwv207_decoder_sw_util.h"

extern volatile struct mwv207_strm_info stream_info;
uint32_t
mwv207_sw_get_bits(struct mwv207_strm_data *stream, uint32_t num_bits)
{
   SLIECE_DEBUG("num_bits:%d \n", num_bits);
   uint32_t out;

   ASSERT(stream);
   ASSERT(num_bits < 32);

   if (num_bits == 0)
      return 0;

   out = mwv207_sw_show_bits(stream, 32) >> (32 - num_bits);

   if (mwv207_sw_flush_bits(stream, num_bits) == MWV207_OK) {
      return (out);
   } else {
      return (END_OF_STREAM);
   }
}

uint32_t
mwv207_sw_get_bits_unsigned_max(struct mwv207_strm_data *stream, uint32_t max_value)
{
   int32_t bits = 0;
   uint32_t num_values = max_value;
   uint32_t value;

   if (num_values > 1) {
      num_values--;
      while (num_values > 0) {
         bits++;
         num_values >>= 1;
      }
   }
   SLIECE_DEBUG("bits : %d \n", bits);
   value = mwv207_sw_get_bits(stream, bits);
   return (value > max_value) ? max_value : value;
}

uint32_t
mwv207_sw_show_bits(struct mwv207_strm_data *stream, uint32_t num_bits)
{
   int32_t bits;
   uint32_t out, out_bits;
   uint32_t tmp_read_bits;
   uint32_t turn = 0;
   const uint8_t *strm;

   uint8_t tmp_strm_buf[32], *tmp;

   ASSERT(stream);
   ASSERT(stream->strm_curr_pos);
   ASSERT(stream->bit_pos_in_word < 8);
   ASSERT(stream->bit_pos_in_word == (stream->strm_buff_read_bits & 0x7));
   ASSERT(num_bits <= 32);

   strm = stream->strm_curr_pos;

   bits = (int32_t)stream->strm_data_size * 8 - (int32_t)stream->strm_buff_read_bits;
   if (!bits) {
      return (0);
   }

   tmp = mwv207_sw_turn_around(stream->strm_curr_pos,
                              stream->strm_buff_start,
                              tmp_strm_buf,
                              stream->strm_buff_size,
                              num_bits + stream->bit_pos_in_word + 32);

   if (tmp != NULL) {
      strm = tmp;
      turn = 1;
   }
   if (!stream->remove_emul3_byte) {
      out = out_bits = 0;
      tmp_read_bits = stream->strm_buff_read_bits;

      if (stream->bit_pos_in_word) {
         out = (turn ? *(strm) : *(strm)) << (24 + stream->bit_pos_in_word);
         strm++;
         out_bits = 8 - stream->bit_pos_in_word;
         bits -= out_bits;
         tmp_read_bits += out_bits;
      }

      while (bits && out_bits < num_bits) {

         if (tmp_read_bits >= 16 && (turn ? *(strm - 2) : *(strm - 2)) == 0x0
             && (turn ? *(strm - 1) : *(strm - 1)) == 0x0
             && (turn ? *(strm) : *(strm)) == 0x3) {
            strm++;
            tmp_read_bits += 8;
            bits -= 8;
            /* emulation prevention byte shall not be the last byte of the
             * stream */
            if (bits <= 0)
               break;
         }

         tmp_read_bits += 8;

         if (out_bits <= 24) {
            out |= (uint32_t)(turn ? *(strm) : *(strm)) << (24 - out_bits);
            strm++;
         } else {
            out |= (uint32_t)(turn ? *(strm) : *(strm)) >> (out_bits - 24);
            strm++;
         }

         out_bits += 8;
         bits -= 8;
      }

      return (out >> (32 - num_bits));

   } else {
      uint32_t shift;

      if (bits >= 32) {
         uint32_t bit_pos_in_word = stream->bit_pos_in_word;

         out = ((uint32_t)(turn ? *(strm + 3) : *(strm + 3)))
            | ((uint32_t)(turn ? *(strm + 2) : *(strm + 2)) << 8)
            | ((uint32_t)(turn ? *(strm + 1) : *(strm + 1)) << 16)
            | ((uint32_t)(turn ? *(strm) : *(strm)) << 24);

         if (bit_pos_in_word) {
            out <<= bit_pos_in_word;
            out |= (uint32_t)(turn ? *(strm + 4) : *(strm + 4))
               >> (8 - bit_pos_in_word);
         }

         return (out >> (32 - num_bits));
      }

      else if (bits > 0) {
         shift = (int32_t)(24 + stream->bit_pos_in_word);
         out = (uint32_t)(turn ? *(strm) : *(strm)) << shift;
         strm++;
         bits -= (int32_t)(8 - stream->bit_pos_in_word);
         while (bits > 0) {
            shift -= 8;
            out |= (uint32_t)(turn ? *(strm) : *(strm)) << shift;
            strm++;
            bits -= 8;
         }
         return (out >> (32 - num_bits));
      } else
         return (0);
   }
}

uint32_t
mwv207_sw_flush_bits(struct mwv207_strm_data *stream, uint32_t num_bits)
{
   uint32_t bytes_left;
   const uint8_t *strm, *strm_bak;
   uint8_t tmp_strm_buf[32], *tmp;
   uint32_t turn = 0;

   ASSERT(stream);
   ASSERT(stream->strm_buff_start);
   ASSERT(stream->strm_curr_pos);
   ASSERT(stream->bit_pos_in_word < 8);
   ASSERT(stream->bit_pos_in_word == (stream->strm_buff_read_bits & 0x7));

   tmp = mwv207_sw_turn_around(stream->strm_curr_pos,
                              stream->strm_buff_start,
                              tmp_strm_buf,
                              stream->strm_buff_size,
                              num_bits + stream->bit_pos_in_word + 32);
   if (tmp != NULL)
      turn = 1;

   if (!stream->remove_emul3_byte) {
      if ((stream->strm_buff_read_bits + num_bits)
          > (8 * stream->strm_data_size)) {
         stream->strm_buff_read_bits = 8 * stream->strm_data_size;
         SLIECE_DEBUG("position 1 strm_buff_read_bits:%d \n",
                      stream->strm_buff_read_bits);
         stream->bit_pos_in_word = 0;
         stream->strm_curr_pos =
            stream->strm_buff_start + stream->strm_buff_size;
         return (END_OF_STREAM);
      } else {
         bytes_left =
            (8 * stream->strm_data_size - stream->strm_buff_read_bits) / 8;
         if (tmp != NULL)
            strm = tmp;
         else
            strm = stream->strm_curr_pos;
         strm_bak = strm;

         if (stream->bit_pos_in_word) {
            if (num_bits < 8 - stream->bit_pos_in_word) {
               stream->strm_buff_read_bits += num_bits;
               SLIECE_DEBUG("position 2 strm_buff_read_bits:%d \n",
                            stream->strm_buff_read_bits);
               stream->bit_pos_in_word += num_bits;
               return (MWV207_OK);
            }
            num_bits -= 8 - stream->bit_pos_in_word;
            stream->strm_buff_read_bits += 8 - stream->bit_pos_in_word;
            SLIECE_DEBUG("position 3 strm_buff_read_bits:%d \n",
                         stream->strm_buff_read_bits);
            stream->bit_pos_in_word = 0;
            strm++;

            if (stream->strm_buff_read_bits >= 16 && bytes_left
                && (turn ? *(strm - 2) : *(strm - 2)) == 0x0
                && (turn ? *(strm - 1) : *(strm - 1)) == 0x0
                && (turn ? *(strm) : *(strm)) == 0x3) {
               strm++;
               stream->strm_buff_read_bits += 8;
               SLIECE_DEBUG("position 4 strm_buff_read_bits:%d \n",
                            stream->strm_buff_read_bits);
               bytes_left--;
               stream->emul_byte_count++;
            }
         }

         while (num_bits >= 8 && bytes_left) {
            if (bytes_left > 2 && (turn ? *(strm) : *(strm)) == 0
                && (turn ? *(strm + 1) : *(strm + 1)) == 0
                && (turn ? *(strm + 2) : *(strm + 2)) <= 1) {

               return (MWV207_NOK);
            }

            strm++;
            stream->strm_buff_read_bits += 8;
            SLIECE_DEBUG("position 5 strm_buff_read_bits:%d \n",
                         stream->strm_buff_read_bits);
            bytes_left--;

            if (stream->strm_buff_read_bits >= 16 && bytes_left
                && (turn ? *(strm - 2) : *(strm - 2)) == 0x0
                && (turn ? *(strm - 1) : *(strm - 1)) == 0x0
                && (turn ? *(strm) : *(strm)) == 0x3) {
               strm++;
               stream->strm_buff_read_bits += 8;
               SLIECE_DEBUG("position 6 strm_buff_read_bits:%d \n",
                            stream->strm_buff_read_bits);
               bytes_left--;
               stream->emul_byte_count++;
            }
            num_bits -= 8;
         }

         if (num_bits && bytes_left) {
            if (bytes_left > 2 && (turn ? *(strm) : *(strm)) == 0
                && (turn ? *(strm + 1) : *(strm + 1)) == 0
                && (turn ? *(strm + 2) : *(strm + 2)) <= 1) {

               return (MWV207_NOK);
            }

            stream->strm_buff_read_bits += num_bits;
            SLIECE_DEBUG("position 7 strm_buff_read_bits:%d \n",
                         stream->strm_buff_read_bits);
            stream->bit_pos_in_word = num_bits;
            num_bits = 0;
         }

         stream->strm_curr_pos += strm - strm_bak;
         if (stream->is_rb
             && stream->strm_curr_pos
                >= (stream->strm_buff_start + stream->strm_buff_size))
            stream->strm_curr_pos -= stream->strm_buff_size;

         if (num_bits)
            return (END_OF_STREAM);
         else
            return (MWV207_OK);
      }
   } else {
      uint32_t bytes_shift = (stream->bit_pos_in_word + num_bits) >> 3;
      stream->strm_buff_read_bits += num_bits;
      SLIECE_DEBUG("position 8 strm_buff_read_bits:%d \n",
                   stream->strm_buff_read_bits);
      stream->bit_pos_in_word = stream->strm_buff_read_bits & 0x7;
      if ((stream->strm_buff_read_bits) <= (8 * stream->strm_data_size)) {
         stream->strm_curr_pos += bytes_shift;
         if (stream->is_rb
             && stream->strm_curr_pos
                >= (stream->strm_buff_start + stream->strm_buff_size))
            stream->strm_curr_pos -= stream->strm_buff_size;
         return (MWV207_OK);
      } else
         return (END_OF_STREAM);
   }
}

uint32_t
mwv207_sw_is_byte_aligned(const struct mwv207_strm_data *stream)
{
   if (!stream->bit_pos_in_word)
      return (MWV207_TRUE);
   else
      return (MWV207_FALSE);
}

