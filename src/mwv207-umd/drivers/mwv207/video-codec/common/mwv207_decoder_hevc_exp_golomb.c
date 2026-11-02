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

#include "mwv207_decoder_hevc_exp_golomb.h"
#include <stdint.h>
#include "mwv207_decoder_sw_stream.h"
#include "mwv207_decoder_sw_util.h"
/* definition of special code num, this along with the return value is used
 * to handle code num in the range [0, 2^32] in the DecodeExpGolombUnsigned
 * function */
#define BIG_CODE_NUM 0xFFFFFFFFU
#define END_OF_STREAM 0xFFFFFFFFU
#define MWV207_OK 0
#define MWV207_NOK 1

#define MWV207_FALSE (0U)
#define MWV207_TRUE (1U)

/* Decodes unsigned Exp-Golomb codeword. This is the same as code_num used in
 * other Exp-Golomb code mappings.
 *
 * Code num (i.e. the decoded symbol) is determined as
 *
 * code_num = 2^leading_zeros - 1 + mwv207_sw_get_bits(leading_zeros)
 *
 * Normal decoded symbols are in the range [0, 2^32 - 2]. Symbol
 * 2^32-1 is indicated by BIG_CODE_NUM with return value MWV207_OK
 * while symbol 2^32  is indicated by BIG_CODE_NUM with return value
 * MWV207_NOK.  These two symbols are special cases with code length
 * of 65, i.e.  32 '0' bits, a '1' bit, and either 0 or 1 represented
 * by 32 bits.
 *
 * Symbol 2^32 is out of unsigned 32-bit range but is needed for
 * DecodeExpGolombSigned to express value -2^31. */
uint32_t
mwv207_hevc_decode_exp_golomb_unsigned(struct mwv207_strm_data *stream,
                                      uint32_t *code_num)
{
   uint32_t bits, num_zeros;

   bits = mwv207_sw_show_bits(stream, 32);

   if (bits >= 0x80000000) {
      if (mwv207_sw_flush_bits(stream, 1) == END_OF_STREAM)
         return (MWV207_NOK);
      *code_num = 0;
      return (MWV207_OK);
   }

   else if (bits >= 0x40000000) {
      if (mwv207_sw_flush_bits(stream, 3) == END_OF_STREAM)
         return (MWV207_NOK);
      *code_num = 1 + ((bits >> 29) & 0x1);
      return (MWV207_OK);
   }

   else if (bits >= 0x20000000) {
      if (mwv207_sw_flush_bits(stream, 5) == END_OF_STREAM)
         return (MWV207_NOK);
      *code_num = 3 + ((bits >> 27) & 0x3);
      return (MWV207_OK);
   }

   else if (bits >= 0x10000000) {
      if (mwv207_sw_flush_bits(stream, 7) == END_OF_STREAM)
         return (MWV207_NOK);
      *code_num = 7 + ((bits >> 25) & 0x7);
      return (MWV207_OK);
   }

   else {
      num_zeros = 4 + mwv207_sw_count_leading_zeros(bits, 28);

      if (num_zeros == 32) {
         *code_num = 0;
         if (mwv207_sw_flush_bits(stream, 32) == END_OF_STREAM)
            return (MWV207_NOK);
         bits = mwv207_sw_get_bits(stream, 1);

         if (bits == 1) {

            bits = mwv207_sw_show_bits(stream, 32);
            if (mwv207_sw_flush_bits(stream, 32) == END_OF_STREAM)
               return (MWV207_NOK);

            if (bits == 0) {
               *code_num = BIG_CODE_NUM;
               return (MWV207_OK);
            }
            /* code num 2^32, needed for unsigned mapping
             * (results in -2^31) */
            else if (bits == 1) {
               *code_num = BIG_CODE_NUM;
               return (MWV207_NOK);
            }
         }

         return (MWV207_NOK);
      } else if (mwv207_sw_flush_bits(stream, num_zeros + 1) == END_OF_STREAM)
         return (MWV207_NOK);

      bits = mwv207_sw_get_bits(stream, num_zeros);
      if (bits == END_OF_STREAM)
         return (MWV207_NOK);

      *code_num = (1 << num_zeros) - 1 + bits;
   }

   return (MWV207_OK);
}

/* Decode signed Exp-Golomb code. Code num is determined by
 * mwv207_hevc_decode_exp_golomb_unsigned and then mapped to signed
 * representation as
 *
 * symbol = (-1)^(code_num+1) * (code_num+1)/2
 *
 * Signed symbols shall be in the range [-2^31, 2^31 - 1]. Symbol -2^31 is
 * obtained when code_num is 2^32, which cannot be expressed by unsigned
 * 32-bit value. This is signaled as a special case from the
 * mwv207_hevc_decode_exp_golomb_unsigned by setting code_num to BIG_CODE_NUM
 * and returning MWV207_NOK status. */
uint32_t
mwv207_hevc_decode_exp_golomb_signed(struct mwv207_strm_data *stream,
                                    int32_t *value)
{
   uint32_t status, code_num = 0;

   status = mwv207_hevc_decode_exp_golomb_unsigned(stream, &code_num);

   if (code_num == BIG_CODE_NUM) {
      /* BIG_CODE_NUM and MWV207_OK status means code_num 2^32-1 which would
       * result in signed integer valued 2^31 (i.e. out of 32-bit signed
       * integer range) */
      if (status == MWV207_OK)
         return (MWV207_NOK);
      /* BIG_CODE_NUM and MWV207_NOK status means code_num 2^32 which results
       * in signed integer valued -2^31 */
      else {
         *value = (int32_t)(2147483648U);
         return (MWV207_OK);
      }
   } else if (status == MWV207_OK) {
      /* (-1)^(code_num+1) results in positive sign if code_num is odd,
       * negative when it is even. (code_num+1)/2 is obtained as
       * (code_num+1)>>1 when value is positive and as (-code_num)>>1 for
       * negative value */

      *value = (code_num & 0x1) ? (int32_t)((code_num + 1) >> 1)
                                : -(int32_t)((code_num + 1) >> 1);

      return (MWV207_OK);
   }

   return (MWV207_NOK);
}

