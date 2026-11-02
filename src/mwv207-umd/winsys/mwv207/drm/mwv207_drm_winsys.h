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

#ifndef MWV207_DRM_WINSYS_H
#define MWV207_DRM_WINSYS_H


/* debug flag bits (must match winsys debug_options[]) */
#define MWV207_DEBUG_NIR     (1 << 0)
#define MWV207_DEBUG_TGSI    (1 << 1)
#define MWV207_DEBUG_BO      (1 << 2)
#define MWV207_DEBUG_CMD     (1 << 3)
#define MWV207_DEBUG_SIZE    (1 << 4)
#define MWV207_DEBUG_TEX     (1 << 5)
#define MWV207_DEBUG_FLUSH   (1 << 6)
#define MWV207_DEBUG_VIDEO   (1 << 7)
#include "pipe/p_state.h"
#include "util/simple_mtx.h"
#include "util/u_hash_table.h"
#include "pipebuffer/pb_slab.h"
#include "pipebuffer/pb_cache.h"
#include "frontend/winsys_handle.h"

#include "mwv207_drm.h"

struct pipe_fence_handle;

enum mwv207_bo_domain {
   MWV207_DOMAIN_GTT    = (1 << 0),
   MWV207_DOMAIN_VRAM   = (1 << 1),
   MWV207_DOMAIN_SYSTEM = (1 << 2)
};

enum mwv207_bo_flag {
   MWV207_FLAG_CPU_ACCESS    = (1 << 0),
   MWV207_FLAG_NO_CPU_ACCESS = (1 << 1),
   MWV207_FLAG_2D_ACCESS     = (1 << 2),
   MWV207_FLAG_SUBALLOC      = (1 << 3),
   MWV207_FLAG_REUSE         = (1 << 4),
};

enum mwv207_heap {
    MWV207_HEAP_3D,
    MWV207_HEAP_DMA,
    MWV207_MAX_SLAB_HEAPS,
    MWV207_MAX_CACHED_HEAPS = MWV207_MAX_SLAB_HEAPS,
};

static inline enum mwv207_bo_domain
mwv207_domain_from_heap(enum mwv207_heap heap)
{
   switch (heap) {
   case MWV207_HEAP_3D:
      return MWV207_DOMAIN_VRAM;
   case MWV207_HEAP_DMA:
      return MWV207_DOMAIN_GTT;
   default:
      assert(0);
      return (enum mwv207_bo_domain)0;
   }
}

static inline enum mwv207_bo_flag
mwv207_flags_from_heap(enum mwv207_heap heap)
{
   switch (heap) {
   case MWV207_HEAP_3D:
      return MWV207_FLAG_CPU_ACCESS;
   case MWV207_HEAP_DMA:
      return MWV207_FLAG_CPU_ACCESS | MWV207_FLAG_REUSE;
   default:
      assert(0);
      return (enum mwv207_bo_flag)0;
   }
}

static inline enum mwv207_heap
mwv207_get_heap_index(enum mwv207_bo_domain domain,
                      enum mwv207_bo_flag flags)
{
   if (domain == MWV207_DOMAIN_GTT)
      return MWV207_HEAP_DMA;
   else
      return MWV207_HEAP_3D;
}

struct mwv207_bo {
   struct pb_buffer base;

   int fd;
   uint32_t name;
   uint32_t handle;

   uint32_t offset;

   void * logical;
   uint32_t flags;

   struct mwv207_winsys *ws;

   union {
      struct {
         struct pb_cache_entry cache_entry;
         bool use_reusable_pool;
      } real;
      struct {
         struct pb_slab_entry entry;
         struct mwv207_bo *real;
      } slab;
   } u;

};

struct mwv207_slab {
   struct pb_slab base;
   struct mwv207_bo *buffer;
   struct mwv207_bo *entries;
};

struct cmd_dat {
   uint32_t data[64];
   uint32_t size;

   struct mwv207_bo *bos[16];
   struct drm_mwv207_submit_reloc relocs[16];
   int nr_relocs;
};

struct cmd_dat_array {
   uint8_t *data;
   uint32_t size;
   int max_size;

   struct drm_mwv207_submit_reloc *relocs;
   int nr_relocs;
   int nr_max_relocs;
};

struct mwv207_cmd {
   struct mwv207_winsys *ws;
   uint8_t *buf;
   uint32_t size;
   uint32_t offset;

   uint32_t *stamp;

   struct drm_mwv207_submit_reloc *relocs;
   int nr_relocs;
   int nr_max_relocs;

   struct mwv207_bo **mwv207_bos;
   struct drm_mwv207_bo_acc *bos;
   int nr_bos;
   int nr_max_bos;

   struct cmd_dat_array mwv207_dats;
   struct drm_mwv207_submit_cmd_dat dats;
   int    nr_dats;

   int poisoned;
   uint32_t engine_type;
};

struct mwv207_winsys {

   struct pipe_screen *screen;
   struct pipe_reference reference;

   int fd;
   int ctx;
   int device;
   uint32_t debug;

   uint32_t cluster_mask;
   uint32_t cluster_count;
   uint32_t clusterMinID;
   uint32_t clusterMaxID;

   struct pb_cache bo_cache;

   struct pb_slabs bo_slabs;

   struct hash_table *bo_export_table;
   struct hash_table *bo_name_table;
   simple_mtx_t bo_export_table_lock;

   bool (*destroy)(struct mwv207_winsys *ws);

   struct mwv207_bo *(*bo_create)(struct mwv207_winsys *ws,
                                 uint64_t size,
                                 enum mwv207_bo_domain domain,
                                 enum mwv207_bo_flag);

   int (*bo_wait)(struct mwv207_winsys *ws, struct mwv207_bo *bo, uint64_t timeout);

   void (*bo_reference)(struct mwv207_winsys *ws, struct mwv207_bo **dst, struct mwv207_bo *src);

   void *(*bo_map)(struct mwv207_winsys *ws, struct mwv207_bo *bo);

   void (*bo_unmap)(struct mwv207_winsys *ws, struct mwv207_bo *bo);

   uint32_t (*bo_get_handle)(struct mwv207_winsys *ws,
                             struct mwv207_bo *bo,
                             struct winsys_handle *whandle);

   struct mwv207_bo *(*bo_from_handle)(struct mwv207_winsys *ws,
                                      struct winsys_handle *whandle,
                                      uint32_t size);

   void (*bo_dump)(struct mwv207_winsys *ws, struct mwv207_bo *bo, uint32_t size);

   int (*fence_wait)(struct mwv207_winsys *ws,
                     struct pipe_fence_handle *fence,
                     uint64_t timeout);
   int (*fence_get_fd)(struct mwv207_winsys *ws,
                       struct pipe_fence_handle *fence);

   void (*fence_accumulate)(struct mwv207_winsys *pipe,
                            int *target_fd,
                            struct pipe_fence_handle *fence);

   void (*fence_reference)(struct pipe_fence_handle **dst,
                           struct pipe_fence_handle *src);

   struct pipe_fence_handle *(*fence_create)(struct mwv207_winsys *ws, int fd);

   struct mwv207_cmd *(*cmdbuf_create)(struct mwv207_winsys *ws);

   void (*cmdbuf_destroy)(struct mwv207_winsys *ws, struct mwv207_cmd *cmd);

   int (*cmdbuf_add_bo)(struct mwv207_cmd *cmd, struct mwv207_bo *bo, uint32_t flags);

   int (*cmdbuf_add_dat)(struct mwv207_cmd *cmd, struct cmd_dat *src_dat);

   int (*cmdbuf_add_relloc)(struct mwv207_cmd *cmd,
                            uint32_t idx,
                            uint32_t cmd_offset,
                            uint32_t bo_offset,
                            uint32_t type,
                            uint32_t pad);

   void (*cmdbuf_commit)(struct mwv207_winsys *ws,
                         struct mwv207_cmd *cmd,
                         int in_fence_fd,
                         int *out_fence_fd);

};

static inline
struct mwv207_bo *mwv207_bo(struct pb_buffer_lean *bo)
{
   return (struct mwv207_bo *)bo;
}

static inline
struct mwv207_slab *mwv207_slab(struct pb_slab *slab)
{
   return (struct mwv207_slab *)slab;
}

#endif
