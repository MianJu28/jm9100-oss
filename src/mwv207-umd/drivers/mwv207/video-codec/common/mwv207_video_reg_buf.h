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

#ifndef MWV207_VDIEO_CMD_BUF_H
#define MWV207_VDIEO_CMD_BUF_H

#include "mwv207_drm_winsys.h"
#include "util/u_inlines.h"
#include <stdint.h>
#include <stdio.h>

struct mwv207_video_bo {
   struct mwv207_bo *bo;
   uint32_t handle;
   uint32_t offset;
   uint32_t flags;
   uint32_t swreg;
   uint32_t type;
};

struct mwv207_video_reg_buf {
   uint32_t *regbuf;
   uint32_t size;

   struct mwv207_video_bo bos[500];
   int nr_bos;
   int cur_bo_pos;

   FILE *dump_reg_stream;

   void (*add_bo)(struct mwv207_video_reg_buf *reg_buf,
                  uint32_t swreg,
                  struct mwv207_bo *bo,
                  uint64_t offset,
                  uint32_t flags,
                  uint32_t type);
   void (*bos_clear)(struct mwv207_video_reg_buf *reg_buf);
   void (*bos_sort)(struct mwv207_video_reg_buf *reg_buf);

   void (*set_reg)(struct mwv207_video_reg_buf *reg_buf,
                   uint32_t id,
                   uint32_t value);
   void (*set_addr_reg)(struct mwv207_video_reg_buf *reg_buf,
                        uint32_t id,
                        struct mwv207_bo *bo,
                        uint64_t offset,
                        uint32_t flags);
   void (*set_addr_reg_hi)(struct mwv207_video_reg_buf *reg_buf,
                           uint32_t id,
                           struct mwv207_bo *bo,
                           uint64_t offset,
                           uint32_t flags);
   uint32_t (*get_reg)(struct mwv207_video_reg_buf *reg_buf, uint32_t id);
   void (*dump_reg_stream_print)(struct mwv207_video_reg_buf *reg_buf,const char *fmt, ...);
   void (*dump_regs)(struct mwv207_video_reg_buf *reg_buf);
};

struct mwv207_video_reg_buf *mwv207_video_reg_buf_create(struct mwv207_winsys *ws);
void mwv207_video_reg_buf_destroy(struct mwv207_video_reg_buf *reg_buf);

#endif

