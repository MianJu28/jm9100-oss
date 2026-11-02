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

#include <libsync.h>
#include <limits.h>
#include "util/u_memory.h"
#include "util/u_inlines.h"
#include "util/u_hash_table.h"

#include "time.h"
#include <xf86drm.h>

#include "mwv207_drm_winsys.h"
#include "mwv207_drm_cs.h"

struct pipe_fence_handle {
   struct pipe_reference reference;
   int fd;
};

static void
mwv207_drm_destroy_fence(struct pipe_fence_handle *fence)
{
   if (fence->fd >= 0)
      close(fence->fd);
   FREE(fence);
}

static struct pipe_fence_handle *
mwv207_drm_fence_create(struct mwv207_winsys *ws, int fd)
{
   struct pipe_fence_handle *fence = CALLOC_STRUCT(pipe_fence_handle);

   if (!fence) {
      debug_printf("CALLOC_STRUCT failed\n");
      return NULL;
   }

   fence->fd = fd;
   pipe_reference_init(&fence->reference, 1);

   return fence;
}

static void
mwv207_drm_fence_reference(struct pipe_fence_handle **dst,
                           struct pipe_fence_handle *src)
{
   struct pipe_fence_handle *old_dst = *dst;

   if(!old_dst && !src)
      return;

   if (pipe_reference(old_dst ? &old_dst->reference : NULL, src ? &src->reference : NULL))
      mwv207_drm_destroy_fence(*dst);

   *dst = src;
}

static int
mwv207_drm_fence_wait(struct mwv207_winsys *ws,
                      struct pipe_fence_handle *fence,
                      uint64_t timeout)
{
   if (fence->fd >= 0) {
      int timeout_ms;

      if (timeout == OS_TIMEOUT_INFINITE)
         timeout_ms = -1;   /* sync_wait(): -1 waits forever */
      else {
         timeout_ms = (int)(timeout / 1000000);
         if (timeout_ms < 0)
            timeout_ms = INT_MAX; /* clamp; avoid int overflow UB */
      }

      return !sync_wait(fence->fd, timeout_ms);
   }

   return true;
}

static int
mwv207_drm_fence_get_fd(struct mwv207_winsys *ws,
                        struct pipe_fence_handle *fence)
{
   return dup(fence->fd);
}

static void
mwv207_drm_fence_accumulate(struct mwv207_winsys *ws,
                            int *target_fd,
                            struct pipe_fence_handle *fence)
{
   if (fence->fd >= 0)
      sync_accumulate("mwv207", target_fd, fence->fd);
}

static struct mwv207_cmd *
mwv207_drm_cmdbuf_create(struct mwv207_winsys *ws)
{
   struct mwv207_cmd *cmd = CALLOC_STRUCT(mwv207_cmd);

   /* FIX(jm9100-oss): W12 —— 四处分配都要判空，并把原来的
    * assert(cmd->buf) 换成显式错误返回。release 构建会定义 NDEBUG，
    * assert 被整个编译掉，届时分配失败会一路带到 GPU 提交阶段才炸，
    * 现场完全看不出是内存不足。调用方（mwv207_create_cmd）已 assert
    * 返回值非空，故这里返回 NULL 即可。 */
   if (!cmd)
      return NULL;

   cmd->ws = ws;

   cmd->buf = (uint8_t *)MALLOC(0x8000);
   if (!cmd->buf) {
      FREE(cmd);
      return NULL;
   }

   cmd->size = 0x8000;
   cmd->offset = 0;
   cmd->stamp = NULL;

   cmd->nr_relocs = 0;
   cmd->nr_max_relocs = 0x80;
   cmd->relocs = CALLOC(cmd->nr_max_relocs, sizeof(struct drm_mwv207_submit_reloc));

   cmd->nr_bos = 0;
   cmd->nr_max_bos = 0x80;
   cmd->mwv207_bos = CALLOC(cmd->nr_max_bos, sizeof(struct mwv207_bo *));
   cmd->bos = CALLOC(cmd->nr_max_bos, sizeof(struct drm_mwv207_bo_acc));

   if (!cmd->relocs || !cmd->mwv207_bos || !cmd->bos) {
      FREE(cmd->relocs);
      FREE(cmd->mwv207_bos);
      FREE(cmd->bos);
      FREE(cmd->buf);
      FREE(cmd);
      return NULL;
   }

   cmd->mwv207_dats.size = 0;
   cmd->mwv207_dats.nr_relocs = 0;
   cmd->nr_dats = 0;

   return cmd;
}

static void
mwv207_drm_cmdbuf_destroy(struct mwv207_winsys *ws, struct mwv207_cmd *cmd)
{
   for (int i = 0; i <cmd->nr_bos; i++)
      cmd->ws->bo_reference(cmd->ws, &cmd->mwv207_bos[i], NULL);

   if (cmd->buf)
      FREE(cmd->buf);

   if (cmd->relocs)
      FREE(cmd->relocs);

   if (cmd->mwv207_bos)
      FREE(cmd->mwv207_bos);

   if (cmd->bos)
      FREE(cmd->bos);

   if (cmd->nr_dats){
      FREE(cmd->mwv207_dats.data);
      FREE(cmd->mwv207_dats.relocs);
   }

   FREE(cmd);
}

static int
mwv207_drm_cmdbuf_add_bo(struct mwv207_cmd *cmd, struct mwv207_bo *bo, uint32_t flags)
{
   int i;

   for (i = 0; i < cmd->nr_bos; ++i) {
      if (cmd->mwv207_bos[i] == bo) {
         cmd->bos[i].flags |= flags;
         return i;
      }
   }

   if (i >= cmd->nr_max_bos) {
      /* leak-safe growth: never overwrite the live pointer directly */
      struct mwv207_bo **new_bos;
      struct drm_mwv207_bo_acc *new_accs;

      cmd->nr_max_bos *= 2;
      new_bos = realloc(cmd->mwv207_bos,
            cmd->nr_max_bos * sizeof(struct mwv207_bo *));
      if (!new_bos)
         return -PIPE_ERROR_OUT_OF_MEMORY;
      cmd->mwv207_bos = new_bos;
      new_accs = realloc(cmd->bos,
         cmd->nr_max_bos * sizeof(struct drm_mwv207_bo_acc));
      if (!new_accs)
         return -PIPE_ERROR_OUT_OF_MEMORY;
      cmd->bos = new_accs;
   }

   /* keep per-cmdbuf access flags in cmd->bos[i]; writing to the shared
    * bo corrupts slab-backed state for other references */
   cmd->bos[i].flags = flags;
   cmd->mwv207_bos[i] = NULL;
   cmd->ws->bo_reference(cmd->ws, &cmd->mwv207_bos[i], bo);
   cmd->nr_bos = i + 1;

   return i;
}

static int
mwv207_drm_cmdbuf_add_dat(struct mwv207_cmd *cmd, struct cmd_dat *src_dat)
{
   struct cmd_dat_array *dest_dat = &cmd->mwv207_dats;

   if (cmd->nr_dats == 0) {
      dest_dat->size = 0;
      dest_dat->max_size = 0x8000;
      dest_dat->data = (uint8_t *)MALLOC(dest_dat->max_size);
      if (!dest_dat->data)
         return -PIPE_ERROR_OUT_OF_MEMORY;

      dest_dat->nr_relocs = 0;
      dest_dat->nr_max_relocs = 0x80;
      dest_dat->relocs = CALLOC(dest_dat->nr_max_relocs, sizeof(struct drm_mwv207_submit_reloc));
      if (!dest_dat->relocs)
         return -PIPE_ERROR_OUT_OF_MEMORY;
      cmd->nr_dats = 1;
   }

   while ((dest_dat->size + src_dat->size) > dest_dat->max_size) {
      uint8_t *new_data;

      dest_dat->max_size *= 2;
      new_data = realloc(dest_dat->data, dest_dat->max_size);
      if (!new_data)
         return -PIPE_ERROR_OUT_OF_MEMORY;
      dest_dat->data = new_data;
   }

   while ((dest_dat->nr_relocs + src_dat->nr_relocs) >= dest_dat->nr_max_relocs) {
      struct drm_mwv207_submit_reloc *new_relocs;

      dest_dat->nr_max_relocs *= 2;
      new_relocs = realloc(dest_dat->relocs,
           dest_dat->nr_max_relocs * sizeof(struct drm_mwv207_submit_reloc));
      if (!new_relocs)
         return -PIPE_ERROR_OUT_OF_MEMORY;
      dest_dat->relocs = new_relocs;
   }

   for (int i = 0; i < src_dat->nr_relocs; ++i){
       struct mwv207_bo *bo = src_dat->bos[src_dat->relocs[i].idx];
       dest_dat->relocs[dest_dat->nr_relocs + i] = src_dat->relocs[i];
       dest_dat->relocs[dest_dat->nr_relocs + i].cmd_offset += dest_dat->size;
       dest_dat->relocs[dest_dat->nr_relocs + i].idx = mwv207_drm_cmdbuf_add_bo(cmd, bo, 0);
    }

    dest_dat->nr_relocs += src_dat->nr_relocs;
    /* FIX(jm9100-oss): O3 —— 每个 0x100 字节槽里只有前 src_dat->size 字节
     * 是有效数据（sizeof(struct mwv207_texture_config) = 156 = 0x9C），
     * 而 dest_dat->size 恒按 0x100 步进，于是每槽尾部约 100 字节保持
     * 未初始化；KMD 会把整个 dat BO 逐字节拷进内核并交给硬件
     * （mwv207_submit.c 的 copy_from_user 路径），造成 GPU 可见的
     * 未初始化堆内容。先把整槽清零再写入，尾部即为确定值 0。
     * 注意：不能改成 "dest_dat->size += src_dat->size" —— 槽步进必须保持
     * 0x100，否则 cmd_offset 与 reloc 偏移会全部错位（详见
     * docs/api/07-WINSYS-AUDIT.md）。 */
    memset(dest_dat->data + dest_dat->size, 0, 0x100);
    memcpy(dest_dat->data + dest_dat->size, src_dat->data, MIN2(src_dat->size, 0x100u));
    dest_dat->size += 0x100;

    return (dest_dat->size - 0x100);
}

static int
mwv207_drm_cmdbuf_add_relloc(struct mwv207_cmd *cmd,
                             uint32_t idx,
                             uint32_t cmd_offset,
                             uint32_t bo_offset,
                             uint32_t type,
                             uint32_t pad)
{
   if (cmd->nr_relocs >= cmd->nr_max_relocs) {
      struct drm_mwv207_submit_reloc *new_relocs;

      cmd->nr_max_relocs *= 2;
      new_relocs = realloc(cmd->relocs,
           cmd->nr_max_relocs * sizeof(struct drm_mwv207_submit_reloc));
      if (!new_relocs)
         return -PIPE_ERROR_OUT_OF_MEMORY;
      cmd->relocs = new_relocs;
   }

   cmd->relocs[cmd->nr_relocs].idx = idx;
   cmd->relocs[cmd->nr_relocs].cmd_offset = cmd_offset;
   cmd->relocs[cmd->nr_relocs].bo_offset = bo_offset;
   cmd->relocs[cmd->nr_relocs].type = type;
   cmd->relocs[cmd->nr_relocs].pad = pad;
   cmd->nr_relocs++;

   return PIPE_OK;
}

static void
mwv207_drm_cmdbuf_print(struct mwv207_winsys *ws, struct mwv207_cmd *cmd)
{
   int i = 0;
   int  commandBufferSize = (cmd->offset) / 4;
   uint32_t *cmdbuffer = (uint32_t *)(cmd->buf);
   static bool init = false;
   static int cmd_count = 0;

   if (!init){
      const char *result = debug_get_option("mwv207_count", "");

      if (strstr(result, "top5") != NULL)
         cmd_count = 5;
      else if (strstr(result, "top10") != NULL)
         cmd_count = 10;
      else if (strstr(result, "top20") != NULL)
         cmd_count = 20;
      else if (strstr(result, "top50") != NULL)
         cmd_count = 50;
      else
         cmd_count = 100000;

      init = true;
   }

   if (cmd_count == 0) return;

   cmd_count--;

   printf("================================================\n");
   printf("size : %d \n", commandBufferSize);

   if(ws->debug & 0x10) return;

   for (i = 0; i < commandBufferSize; i++)
      printf("0x%08x,\n", *cmdbuffer++);

   printf("================================================\n");
}

static void mwv207_print_dat(struct cmd_dat_array *dat)
{
   for (int i = 0; i < dat->nr_relocs; ++i)
      printf("reloc[%d] ids %d offset %d\n", i, dat->relocs[i].idx, dat->relocs[i].cmd_offset);

   for (int i = 0; i < (dat->size / sizeof(uint32_t)); ++i)
      printf("0x%x\t", dat->data[i]);
   printf("\n");
}

static void
mwv207_drm_context_print(struct mwv207_cmd *cmd){
   for (int i = 0; i <cmd->nr_bos; i++){
      printf("bo %d handle %u size %lu\n", i, cmd->mwv207_bos[i]->handle, cmd->mwv207_bos[i]->base.base.size);
   }

   for(int i = 0; i <cmd->nr_relocs; i++){
      if(cmd->relocs[i].type & 0x80000000)
         printf("reloc %d type :   dat ", i);
      else
         printf("reloc %d type : reloc ", i);
      printf("idx %d cmd offset %d bo offset %lld\n",
         cmd->relocs[i].idx, cmd->relocs[i].cmd_offset, cmd->relocs[i].bo_offset);
   }

   mwv207_print_dat(&cmd->mwv207_dats);
}

static void
mwv207_drm_cmdbuf_commit(struct mwv207_winsys *ws,
                         struct mwv207_cmd *cmd,
                         int in_fence_fd,
                         int *out_fence_fd)
{

   struct drm_mwv207_submit submit = {0};
   int err;

   if(unlikely((ws->debug & 0x8) || (ws->debug & 0x10)))
      mwv207_drm_cmdbuf_print(ws, cmd);

   submit.ctx = ws->ctx;
   submit.engine_type = cmd ->engine_type;
   submit.engine_id = 0;
   if (in_fence_fd >= 0) {
      submit.flags |= 0x1;
      submit.fence_fd = in_fence_fd;
   }
   if (out_fence_fd)
      submit.flags |= 0x2;
   submit.cmd_size = cmd->offset;
   submit.nr_relocs = cmd->nr_relocs;
   submit.nr_bos = cmd->nr_bos;
   submit.relocs = (__u64)cmd->relocs;
   submit.cmds = (__u64)cmd->buf;

   for(int i = 0; i <cmd->nr_bos; i++){
      cmd->bos[i].handle = cmd->mwv207_bos[i]->handle;
      /* flags are maintained in cmd->bos[i] by add_bo */
   }
   submit.bos = (__u64)cmd->bos;

   if(cmd->mwv207_dats.size != 0)
      submit.nr_cmd_dat = cmd->nr_dats;
   else
      submit.nr_cmd_dat = 0;

   cmd->dats.nr_relocs = cmd->mwv207_dats.nr_relocs;
   cmd->dats.dat_size = cmd->mwv207_dats.size;
   cmd->dats.relocs = (__u64)cmd->mwv207_dats.relocs;
   cmd->dats.dat = (__u64)cmd->mwv207_dats.data;
   submit.cmd_dats = (__u64)&cmd->dats;

   err = drmCommandWriteRead(ws->fd, 0x5, &submit, sizeof(submit));

   /* FIX(jm9100-oss): O4 —— 原来的 -EBUSY 重试是没有上限的 busy-loop，
    * 内核一旦因为队列满而长期返回 -EBUSY（例如上一个 job 卡住），这里就
    * 永远转下去，用户态表现为 firefox/游戏直接冻住且无法 Ctrl-C。
    * 另外每次重试都会让 KMD 重新走一遍 submit 路径；如果内核已经
    * fd_install 过 fence_fd 才失败，重试会把它覆盖掉 → fd 泄漏。
    * 改成有上限（约 2 秒）并直接放弃，同时把错误上报给调用方。 */
   for (int retry = 0; err == -EBUSY && retry < 2000; retry++) {
      usleep(1000);
      err = drmCommandWriteRead(ws->fd, 0x5, &submit, sizeof(submit));
   }

   if (err) {
      fprintf(stderr, "%s: submit failed: %d (%s)\n",
              __func__, err, strerror(-err));
      mwv207_drm_context_print(cmd);
      /* 失败时 KMD 可能已经 fd_install 过 out-fence（KMD 的
       * mwv207_submit_commit() 是"先 fd_install 再 push job"，push 失败
       * 时 fd 已经装好却没人接管）→ 这里主动关掉避免泄漏。
       * 注意只能关"KMD 新装的那个"：如果本次带了 in-fence，
       * submit.fence_fd 上原本就是调用者的输入 fd，关它会直接破坏
       * 调用者的 fence。判据是"请求了 out-fence 且值不等于输入 fd"。 */
      if (out_fence_fd && submit.fence_fd >= 0 &&
          (in_fence_fd < 0 || submit.fence_fd != (unsigned) in_fence_fd))
         close(submit.fence_fd);
   }

   if (!err && out_fence_fd)
      *out_fence_fd = submit.fence_fd;

   for (int i = 0; i < cmd->nr_bos; i++)
      cmd->ws->bo_reference(cmd->ws, &cmd->mwv207_bos[i], NULL);

   cmd->nr_bos = 0;
   cmd->nr_relocs = 0;
   cmd->mwv207_dats.size = 0;
   cmd->mwv207_dats.nr_relocs = 0;
   cmd->offset = 0;

}

void mwv207_cs_init_function(struct mwv207_winsys *ws)
{

   ws->fence_create     = mwv207_drm_fence_create;
   ws->fence_reference  = mwv207_drm_fence_reference;
   ws->fence_wait       = mwv207_drm_fence_wait;
   ws->fence_get_fd     = mwv207_drm_fence_get_fd;
   ws->fence_accumulate = mwv207_drm_fence_accumulate;

   ws->cmdbuf_create     = mwv207_drm_cmdbuf_create;
   ws->cmdbuf_destroy    = mwv207_drm_cmdbuf_destroy;
   ws->cmdbuf_add_bo     = mwv207_drm_cmdbuf_add_bo;
   ws->cmdbuf_add_dat    = mwv207_drm_cmdbuf_add_dat;
   ws->cmdbuf_add_relloc = mwv207_drm_cmdbuf_add_relloc;
   ws->cmdbuf_commit     = mwv207_drm_cmdbuf_commit;
}

