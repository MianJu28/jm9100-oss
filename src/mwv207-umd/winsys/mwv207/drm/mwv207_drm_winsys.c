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

#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <errno.h>
#include <xf86drm.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include "util/futex.h"
#include "util/macros.h"
#include "util/u_memory.h"
#include "util/u_inlines.h"
#include "util/os_file.h"
#include "util/u_hash_table.h"
#include "util/u_pointer.h"
#include "util/simple_mtx.h"

#include "mwv207_drm_winsys.h"
#include "mwv207_drm_bo.h"
#include "mwv207_drm_cs.h"
#include "mwv207_drm_public.h"
#include "mwv207/include/mwv207_screen.h"

static struct hash_table *mwv207_ws_tab = NULL;

static mtx_t mwv207_ws_mutex = SIMPLE_MTX_INITIALIZER;

int mwv207_debug = 0;

static const struct debug_named_value
   debug_options[] = {
   { "nir",     0x1,     "Dump NIR during program compile" },
   { "nir_all", 0x201, "Dump NIR after translate" },
   { "tgsi",    0x2,    "Dump TGSI during program compile" },
   { "bo",      0x4,      "Dump bo info" },
   { "cmd",     0x8,     "Dump cmd, use mwv207_count=top[5|10|20|50] to limit print" },
   { "size",    0x10,    "Dump cmd size" },
   { "tex",     0x20,     "Dump tex config" },
   { "flush",   0x40,   "Flush all state after draw" },
   { "video",   0x80,   "Dump video command stream to mwv207_video_reg.dump" },
   { "spill",   0x100,   "Dump spill info" },
   DEBUG_NAMED_VALUE_END
};

DEBUG_GET_ONCE_FLAGS_OPTION(mwv207_debug, "mwv207_debug", debug_options, 0)

static void
mwv207_cluster_init(struct mwv207_winsys *ws)
{
   ws->cluster_mask = 0xf;
   ws->cluster_count = 0;

   if (ws->cluster_mask) {
      uint32_t i = 0;
      uint32_t clusterMask = ws->cluster_mask;

      while (clusterMask) {
         if (clusterMask & (1 << i)) {
            ws->cluster_count++;

            if (ws->cluster_count == 1)
               ws->clusterMinID = i;

            ws->clusterMaxID = i;
            clusterMask &= ~(1 << i);
         }
         ++i;
      }
   }

   if (ws->cluster_count < 1)
      ws->cluster_count = 1;

   assert(ws->cluster_mask);
   assert(ws->cluster_count > 0);

}

static uint32_t
hash_uint32_t(const void *key)
{
   return *(uint32_t *)key;
}

static bool
handle_equals(const void *key1, const void *key2)
{
   return (*(uint32_t *)key1 == *(uint32_t *)key2);
}

static bool
mwv207_ws_init(struct mwv207_winsys *ws)
{
   int err;
   struct drm_mwv207_ctx args;

   ws->debug = debug_get_option_mwv207_debug();

   mwv207_cluster_init(ws);

   ws->bo_export_table =
      _mesa_hash_table_create(NULL, hash_uint32_t, handle_equals);
   ws->bo_name_table =
      _mesa_hash_table_create(NULL, hash_uint32_t, handle_equals);

   (void) simple_mtx_init(&ws->bo_export_table_lock, mtx_plain);

   memset(&args, 0, sizeof(args));
   args.op = 0x0;
   err = drmCommandWriteRead(ws->fd, 0x4, &args, sizeof(args));

   /* FIX(jm9100-oss): 恢复 GBM/Wayland 平台。
    * 我们这 6 个私有 ioctl（0x0..0x5 号 drmCommand）在 KMD 里声明成
    * DRM_AUTH | DRM_RENDER_ALLOW（src/kernel-6.6/mwv207_drv.c），
    * drm_ioctl_permit() 只放行"已认证的 card 客户端或 render 客户端"。
    * X11/EGL 桌面走 DRI3 拿到的是 renderD128，天然放行；
    * 而 GBM 平台（egl_dri2 的 platform_gbm）交给 winsys 的 fstat 是
    * /dev/dri/card0，未经认证 → 这里恒返回 -EACCES，于是
    * "mwv207: failed to create kernel context, fd=5 err=-13" 之后
    * gbm 后端整体不可用。实测：renderD128 上 ioctl ret=0，card0 上 ret=-1 errno=13。
    * 修法：碰到 -EACCES 就换成同一张卡对应的 render 节点重试（只换我们自己
    * dup 出来的 ws->fd，不动调用者的 fd）。X11 路径 fd 本来就是 render
    * 节点，不会进这个分支，行为不变。 */
   if (err == -EACCES) {
      char *rname = drmGetRenderDeviceNameFromFd(ws->fd);
      if (rname) {
         int rfd = open(rname, O_RDWR | O_CLOEXEC);
         if (rfd >= 0) {
            int nfd = fcntl(rfd, F_DUPFD_CLOEXEC, 3);
            close(rfd);
            if (nfd >= 0) {
               close(ws->fd);
               ws->fd = nfd;
               memset(&args, 0, sizeof(args));
               args.op = 0x0;
               err = drmCommandWriteRead(ws->fd, 0x4, &args, sizeof(args));
               if (err == 0)
                  fprintf(stderr, "mwv207: 私有 ioctl 被拒(EACCES)，"
                                  "已改用 render 节点 %s\n", rname);
            }
         }
         free(rname);
      }
   }

   if (err < 0) {
      fprintf(stderr, "mwv207: failed to create kernel context, fd=%d err=%d\n",
              ws->fd, err);
      return false;
   }

   ws->ctx = args.handle;
   return true;
}

static bool
mwv207_drm_destroy(struct mwv207_winsys *ws)
{
   bool destroy;

   mtx_lock(&mwv207_ws_mutex);
   destroy = pipe_reference(&ws->reference, NULL);

   if (destroy) {
      struct drm_mwv207_ctx args;
      memset(&args, 0, sizeof(args));
      args.op = 0x1;
      args.handle = ws->ctx;
      (void) drmCommandWriteRead(ws->fd, 0x4, &args, sizeof(args));

      if (ws->bo_slabs.groups)
         pb_slabs_deinit(&ws->bo_slabs);
      pb_cache_deinit(&ws->bo_cache);

      simple_mtx_destroy(&ws->bo_export_table_lock);
      _mesa_hash_table_destroy(ws->bo_export_table, NULL);
      _mesa_hash_table_destroy(ws->bo_name_table, NULL);

      _mesa_hash_table_remove_key(mwv207_ws_tab, intptr_to_pointer(ws->fd));

      close(ws->fd);
      FREE(ws);
   }

   mtx_unlock(&mwv207_ws_mutex);

   return destroy;
}

static unsigned hash_fd(void *key)
{
   int fd = pointer_to_intptr(key);
   struct stat stat;

   fstat(fd, &stat);

   return stat.st_dev ^ stat.st_ino ^ stat.st_rdev;
}

static int compare_fd(void *key1, void *key2)
{
   int fd1 = pointer_to_intptr(key1);
   int fd2 = pointer_to_intptr(key2);
   struct stat stat1, stat2;

   fstat(fd1, &stat1);
   fstat(fd2, &stat2);

   return stat1.st_dev != stat2.st_dev ||
          stat1.st_ino != stat2.st_ino ||
          stat1.st_rdev != stat2.st_rdev;
}

struct pipe_screen *
mwv207_drm_create_screen(int fd)
{
   struct mwv207_winsys *ws;

   mtx_lock(&mwv207_ws_mutex);

   if (!mwv207_ws_tab) {
      mwv207_ws_tab = util_hash_table_create_fd_keys();

      if (!mwv207_ws_tab) {
         mtx_unlock(&mwv207_ws_mutex);
         return NULL;
      }
   }

   ws = util_hash_table_get(mwv207_ws_tab, intptr_to_pointer(fd));

   if (ws) {
      pipe_reference(NULL, &ws->reference);
      goto unlock;
   }

   ws = CALLOC_STRUCT(mwv207_winsys);

   if (!ws) {
      mtx_unlock(&mwv207_ws_mutex);
      return NULL;
   }

   ws->fd = fcntl(fd, F_DUPFD_CLOEXEC, 3);
   if (ws->fd < 0) {
      fprintf(stderr, "mwv207: F_DUPFD_CLOEXEC failed: %s\n", strerror(errno));
      FREE(ws);
      mtx_unlock(&mwv207_ws_mutex);
      return NULL;
   }

   if (!mwv207_ws_init(ws)) {
      close(ws->fd);
      FREE(ws);
      mtx_unlock(&mwv207_ws_mutex);
      return NULL;
   }

   mwv207_bo_init_function(ws);
   mwv207_cs_init_function(ws);

   ws->screen = mwv207_create_screen(ws);
   if (!ws->screen) {
      /* never cache a half-initialised winsys */
      close(ws->fd);
      FREE(ws);
      mtx_unlock(&mwv207_ws_mutex);
      return NULL;
   }

   ws->destroy = mwv207_drm_destroy;

   pipe_reference_init(&ws->reference, 1);

   _mesa_hash_table_insert(mwv207_ws_tab, intptr_to_pointer(ws->fd), ws);

unlock:
   mtx_unlock(&mwv207_ws_mutex);
   return ws->screen;

}

