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

#ifndef SW_UTIL_H_
#define SW_UTIL_H_

#include <stdint.h>
#include <stdio.h>

#ifdef JMD_SILECE_DEBUG
#define SLIECE_DEBUG(fmt, args...)                                           \
   printf("%d  slice debug  " fmt, __LINE__, ##args)
#else
#define SLIECE_DEBUG(fmt, args...)
#endif

#define MWV207_OK 0
#define MWV207_NOK 1

struct mwv207_strm_info {
   uint32_t low_latency;
   uint32_t send_len;
   uint32_t strm_bus_addr;
   uint32_t strm_bus_start_addr;
   uint8_t *strm_vir_addr;
   uint8_t *strm_vir_start_addr;
   uint32_t last_flag;
};

#define MWV207_FALSE (0U)
#define MWV207_TRUE (1U)

#define MEMORY_ALLOCATION_ERROR 0xFFFF
#define PARAM_SET_ERROR 0xFFF0

#define END_OF_STREAM 0xFFFFFFFFU

#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define ABS(a) (((a) < 0) ? -(a) : (a))

#define CLIP3(x, y, z) (((z) < (x)) ? (x) : (((z) > (y)) ? (y) : (z)))

#define CLIP1(z) (((z) < 0) ? 0 : (((z) > 255) ? 255 : (z)))

#define ALLOCATE(ptr, count, type)                                           \
   {                                                                         \
      ptr = DWLmalloc((count) * sizeof(type));                               \
   }

#define FREE(ptr)                                                            \
   {                                                                         \
      if (ptr != NULL) {                                                     \
         DWLfree(ptr);                                                       \
         ptr = NULL;                                                         \
      }                                                                      \
   }

#define SW_CHECK_SYNTAX_ELEMENT(rv, elem)                                    \
   if (rv != MWV207_OK) {                                                     \
      fprintf(stderr, "error in syntax element '%s'\n", elem);               \
      return rv;                                                             \
   }

#define SW_CHECK_END_OF_STREAM(rv, elem)                                     \
   if (rv == END_OF_STREAM) {                                                \
      fprintf(stderr, "end of stream in syntax element '%s'\n", elem);       \
      return rv;                                                             \
   }

#define NEXT_MULTIPLE(value, n) (((value) + (n)-1) & ~((n)-1))

#define MAJOR_VERSION(hw_id) (((hw_id)&0xF000) >> 12)
#define MINOR_VERSION(hw_id) (((hw_id)&0x0FF0) >> 4)

#define IS_G1_LEGACY(hw_id) (((hw_id) >> 16) == 0x6731)
#define IS_G2_LEGACY(hw_id) (((hw_id) >> 16) == 0x6732)

#define IS_LEGACY(hw_id)                                                     \
   ((((hw_id) >> 16) == 0x6731) || (((hw_id) >> 16) == 0x6732))
#define IS_JMDEC(asic_id) (((asic_id) >> 16) == 0x8001)
#define IS_KMB_A0(asic_id) (((asic_id)&0xffff) == 0x6010)

uint32_t 
mwv207_sw_count_leading_zeros(uint32_t value, uint32_t length);

uint32_t 
mwv207_sw_num_bits(uint32_t value);

uint8_t *
mwv207_sw_turn_around(
   const uint8_t *strm, const uint8_t *buf, uint8_t *tmp_buf, uint32_t buf_size, uint32_t num_bits);

#endif

