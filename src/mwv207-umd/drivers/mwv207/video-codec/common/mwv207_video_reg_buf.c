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

#include "mwv207_video_reg_buf.h"
#include "mwv207_video_dec_reg.h"
#include "mwv207_log.h"
#include "util/u_memory.h"

static void
bos_clear(struct mwv207_video_reg_buf *reg_buf)
{
   memset(reg_buf->regbuf, 0, reg_buf->size);
   memset(reg_buf->bos, 0, sizeof(struct mwv207_video_bo) * 500);
   reg_buf->nr_bos = 0;
   reg_buf->cur_bo_pos = 0;
}

static int
vbo_cmp(const void *a, const void *b)
{
   struct mwv207_video_bo *vboa = (struct mwv207_video_bo *)a;
   struct mwv207_video_bo *vbob = (struct mwv207_video_bo *)b;
   return (vboa->swreg - vbob->swreg);
}

static void
bos_sort(struct mwv207_video_reg_buf *reg_buf)
{
   qsort(reg_buf->bos, reg_buf->nr_bos, sizeof(reg_buf->bos[0]), vbo_cmp);
}

static void
add_bo(struct mwv207_video_reg_buf *reg_buf,
       uint32_t swreg,
       struct mwv207_bo *bo,
       uint64_t offset,
       uint32_t flags,
       uint32_t type)
{
   if (!bo)
      return;
   struct mwv207_video_bo vbo;
   vbo.handle = bo->handle;
   vbo.flags = flags;
   vbo.offset = offset;
   vbo.swreg = swreg;
   vbo.type = type;
   vbo.bo = bo;

   reg_buf->bos[reg_buf->nr_bos] = vbo;
   reg_buf->nr_bos++;
}

static void
dump_reg_stream_print(struct mwv207_video_reg_buf *reg_buf,const char *fmt, ...)
{
   if (reg_buf->dump_reg_stream) {
      va_list ap;
      va_start(ap, fmt);
      vfprintf(reg_buf->dump_reg_stream,fmt, ap);
      va_end(ap);
      
   }
}

struct mwv207_video_reg_buf *
mwv207_video_reg_buf_create(struct mwv207_winsys *ws)
{
   struct mwv207_video_reg_buf *reg_buf = CALLOC_STRUCT(mwv207_video_reg_buf);
   if (!reg_buf)
      return NULL;
   reg_buf->add_bo = add_bo;
   reg_buf->bos_clear = bos_clear;
   reg_buf->bos_sort = bos_sort;
   reg_buf->set_reg = mwv207_9200_set_dec_register;
   reg_buf->get_reg = mwv207_9200_get_dec_register;
   reg_buf->dump_regs = mwv207_9200_dump_register;
   reg_buf->dump_reg_stream_print = dump_reg_stream_print;
   reg_buf->set_addr_reg = mwv207_9200_set_addr_reg;
   reg_buf->set_addr_reg_hi = mwv207_9200_set_addr_reg_hi;
   reg_buf->regbuf = CALLOC(1, DEC_X170_REGISTERS * 4);
   reg_buf->size = DEC_X170_REGISTERS * 4;

   if (unlikely(ws->debug & MWV207_DEBUG_VIDEO)) {
      reg_buf->dump_reg_stream = fopen("./mwv207_video_reg.dump", "wb");
      if (!reg_buf->dump_reg_stream)
         printf("mwv207: failed to open video reg  stream log file \n");
   }

   return reg_buf;
}

void
mwv207_video_reg_buf_destroy(struct mwv207_video_reg_buf *reg_buf)
{
   if (reg_buf) {
      if (reg_buf->dump_reg_stream) {
         fclose(reg_buf->dump_reg_stream);
         reg_buf->dump_reg_stream = NULL;
      }
      if (reg_buf->regbuf) {
         FREE(reg_buf->regbuf);
      }
      FREE(reg_buf);
   }
}

