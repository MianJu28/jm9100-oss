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

#include "mwv207_video_cs.h"
#include "mwv207_log.h"
#include <sys/ioctl.h>

#include "pipe/p_defines.h"
#include "util/u_memory.h"

void
mwv207_video_cs_init(struct mwv207_video_cs *cs, struct mwv207_winsys *ws)
{
   MWV207_LOG(MWV207_LEVEL_LOG, " \n");
   cs->cmd = ws->cmdbuf_create(ws);
   cs->cmd->engine_type = MWV207_SUBMIT_ENGINE_TYPE_DEC;
   cs->ws = ws;
}

void
mwv207_video_cs_release(struct mwv207_video_cs *cs)
{
   if (cs->cmd) {
      cs->ws->cmdbuf_destroy(cs->ws, cs->cmd);
   }
}

static void
copy_one_cmd(struct mwv207_video_cs *cs, struct mwv207_video_reg_buf *regbuf)
{
   int offset = 0;
   struct drm_mwv207_submit_reloc reloc;

   *cs->cmdbuf_cur = *cs->regbuf_cur; 

   if (regbuf->cur_bo_pos < regbuf->nr_bos) {
      offset = cs->regbuf_cur - regbuf->regbuf;
      struct mwv207_video_bo *cur_bo = &regbuf->bos[regbuf->cur_bo_pos];
      if (cur_bo && offset == cur_bo->swreg) { 
         reloc.idx = cs->ws->cmdbuf_add_bo(
            cs->cmd, cur_bo->bo, cur_bo->flags); 
         reloc.cmd_offset =
            (uint8_t *)cs->cmdbuf_cur - (uint8_t *)cs->cmdbuf_start;
         reloc.bo_offset = cs->cmdbuf_cur[0];
         reloc.type = cur_bo->type;
         reloc.pad = 0;

         cs->ws->cmdbuf_add_relloc(
            cs->cmd, reloc.idx, reloc.cmd_offset, reloc.bo_offset, reloc.type, reloc.pad);
         regbuf->cur_bo_pos++;
      }
   }
   cs->cmdbuf_cur++;
   cs->regbuf_cur++;
}

void
mwv207_video_cs_submit(struct mwv207_video_cs *cs,
                      struct mwv207_video_reg_buf *regbuf)
{
   int i = 0;
   cs->cmdbuf_start = (uint32_t *)cs->cmd->buf;
   cs->cmdbuf_cur = cs->cmdbuf_start;
   cs->regbuf_cur = regbuf->regbuf;

   uint32_t buf_start = 0x00000003; 

   *cs->cmdbuf_cur++ = (0x08ff0000 + buf_start);

   cs->regbuf_cur += buf_start;

   for (i = 0; i < 0xff; i++) 
   {
      copy_one_cmd(cs, regbuf);
   }

   *cs->cmdbuf_cur++ = 0x08f50102;
   for (i = 0; i < 0xf5; i++) 
   {
      copy_one_cmd(cs, regbuf);
   }

   *cs->cmdbuf_cur++ = 0x08010002;
   cs->regbuf_cur = (regbuf->regbuf + 2); 
   copy_one_cmd(cs, regbuf);

   *cs->cmdbuf_cur++ = 0x08010001;
   cs->regbuf_cur = (regbuf->regbuf + 1); 

   copy_one_cmd(cs, regbuf);

   cs->cmd->offset = (cs->cmdbuf_cur - cs->cmdbuf_start) * sizeof(uint32_t);

   if (unlikely(cs->ws->debug & MWV207_DEBUG_VIDEO)) {
      /* FIX(jm9100-oss): 必须初始化为 -1。submit 失败时 cmdbuf_commit()
       * 不再写 *out_fence_fd（见 mwv207_drm_cs.c 的 O4 修复），
       * 未初始化的话下面 fence_create() 会拿一个栈上垃圾 fd 去 sync_wait()。 */
      int out_fence_fd = -1;

      cs->codec_time->codec_wait_hw_start_time = os_time_get();

      cs->ws->cmdbuf_commit(cs->ws, cs->cmd, -1, &out_fence_fd);
      struct pipe_fence_handle *out_fence;
      out_fence = cs->ws->fence_create(cs->ws, out_fence_fd);
      if (out_fence) {
         cs->ws->fence_wait(cs->ws, out_fence, OS_TIMEOUT_INFINITE);
         cs->ws->fence_reference(&out_fence, NULL);
      }
   
      cs->codec_time->codec_wait_hw_finish_time = os_time_get();
   } else {
      cs->ws->cmdbuf_commit(cs->ws, cs->cmd, -1, NULL);
   }
}

