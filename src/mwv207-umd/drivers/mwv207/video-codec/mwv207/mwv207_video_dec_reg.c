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

#include "mwv207_video_dec_reg.h"
#include <stdint.h>
#include "mwv207_log.h"

char registername[JMDEC_LAST_REG][50] = {
#include "mwv207_dec_register_array_7020.h"
};

static const uint32_t reg_mask[33] = {
   0x00000000, 0x00000001, 0x00000003, 0x00000007, 0x0000000F, 0x0000001F,
   0x0000003F, 0x0000007F, 0x000000FF, 0x000001FF, 0x000003FF, 0x000007FF,
   0x00000FFF, 0x00001FFF, 0x00003FFF, 0x00007FFF, 0x0000FFFF, 0x0001FFFF,
   0x0003FFFF, 0x0007FFFF, 0x000FFFFF, 0x001FFFFF, 0x003FFFFF, 0x007FFFFF,
   0x00FFFFFF, 0x01FFFFFF, 0x03FFFFFF, 0x07FFFFFF, 0x0FFFFFFF, 0x1FFFFFFF,
   0x3FFFFFFF, 0x7FFFFFFF, 0xFFFFFFFF};

static const uint32_t hw_dec_reg_spec[JMDEC_LAST_REG + 1][4] = {

#include "mwv207_video_dec_8170table.h"
{1, 13, 11, 0},
{60, 2, 12, 0},
{0, 0, 0, 0}};

typedef uint32_t (*hw_reg_sepc_ptr)[JMDEC_LAST_REG + 1][4];

void
mwv207_9200_set_dec_register(struct mwv207_video_reg_buf *reg_buf,
                            uint32_t id,
                            uint32_t value)
{
   uint32_t *reg_base = reg_buf->regbuf;
   uint32_t tmp;
   uint32_t(*hw_reg_spec)[JMDEC_LAST_REG + 1][4] =
      (hw_reg_sepc_ptr)hw_dec_reg_spec;
   MWV207_LOG(
      MWV207_LEVEL_TRACE_REG, "%-30s-%d-%9d\n", registername[id], id, value);

   ASSERT(id < JMDEC_LAST_REG);

   if ((*hw_reg_spec)[id][0] == 0) {

      return;
   }
   tmp = reg_base[(*hw_reg_spec)[id][0]];
   tmp &= ~(reg_mask[(*hw_reg_spec)[id][1]] << (*hw_reg_spec)[id][2]);
   tmp |= (value & reg_mask[(*hw_reg_spec)[id][1]]) << (*hw_reg_spec)[id][2];
   reg_base[(*hw_reg_spec)[id][0]] = tmp;

   if (id == JMDEC_DEC_E) {
      MWV207_LOG(MWV207_LEVEL_TRACE_REG,
                "vdec set reg addr : %p, value :%x \n",
                reg_base + (*hw_reg_spec)[id][0],
                reg_base[(*hw_reg_spec)[id][0]]);
   }
}

void
mwv207_9200_set_addr_reg(struct mwv207_video_reg_buf *reg_buf,
                        uint32_t id,
                        struct mwv207_bo *bo,
                        uint64_t offset,
                        uint32_t flags)
{
   mwv207_9200_set_dec_register(reg_buf, id, offset);

   uint32_t(*hw_reg_spec)[JMDEC_LAST_REG + 1][4] =
      (hw_reg_sepc_ptr)hw_dec_reg_spec;

   reg_buf->add_bo(reg_buf,
                   (*hw_reg_spec)[id][0],
                   bo,
                   offset,
                   flags,
                   MWV207_SUBMIT_RELOC_LOW32);
}

void
mwv207_9200_set_addr_reg_hi(struct mwv207_video_reg_buf *reg_buf,
                           uint32_t id,
                           struct mwv207_bo *bo,
                           uint64_t offset,
                           uint32_t flags)
{
   mwv207_9200_set_dec_register(reg_buf, id, (offset >> 32) & 0xffffffff);

   uint32_t(*hw_reg_spec)[JMDEC_LAST_REG + 1][4] =
      (hw_reg_sepc_ptr)hw_dec_reg_spec;

   reg_buf->add_bo(reg_buf,
                   (*hw_reg_spec)[id][0],
                   bo,
                   (offset >> 32) & 0xffffffff,
                   flags,
                   MWV207_SUBMIT_RELOC_HIGH32);
}

uint32_t
mwv207_9200_get_dec_register(struct mwv207_video_reg_buf *reg_buf, uint32_t id)
{
   uint32_t *reg_base = reg_buf->regbuf;
   uint32_t tmp = 0;
   uint32_t(*hw_reg_spec)[JMDEC_LAST_REG + 1][4] =
      (hw_reg_sepc_ptr)hw_dec_reg_spec;
   ASSERT(id < JMDEC_LAST_REG);

   if ((*hw_reg_spec)[id][0] == 0)
      return 0;

   tmp = reg_base[(*hw_reg_spec)[id][0]];
   tmp = tmp >> (*hw_reg_spec)[id][2];
   tmp &= reg_mask[(*hw_reg_spec)[id][1]];

   return (tmp);
}

void
mwv207_9200_dump_register(struct mwv207_video_reg_buf *reg_buf)
{
   uint32_t(*hw_reg_spec)[JMDEC_LAST_REG + 1][4] = {NULL};
   hw_reg_spec = (hw_reg_sepc_ptr)hw_dec_reg_spec;

   int i = 0;
   uint32_t value;
   return;

   

   static int count = 0;
   if (count > 1) {
      return;
   }
   count++;

   for (i = 0; i < JMDEC_LAST_REG; i++) {
      value = mwv207_9200_get_dec_register(reg_buf, i);
      reg_buf->dump_reg_stream_print(
              reg_buf,"dump reg : %d %-30s-%d-%9d-%x\n",
              (*hw_reg_spec)[i][0],
              registername[i],
              i,
              value,
              value);
   }
   reg_buf->dump_reg_stream_print(
              reg_buf,
              "dump video reg end !\n");
}

