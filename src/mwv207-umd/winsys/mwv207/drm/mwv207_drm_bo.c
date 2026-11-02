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
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/mman.h>
#include <xf86drm.h>

#include "util/os_time.h"
#include "util/u_memory.h"
#include "util/u_inlines.h"
#include "util/u_hash_table.h"
#include "frontend/sw_winsys.h"
#include "util/os_mman.h"

#include "mwv207_drm_winsys.h"
#include "mwv207_drm_bo.h"

static void
mwv207_clean_up_buffer_managers(struct mwv207_winsys *ws)
{
   pb_slabs_reclaim(&ws->bo_slabs);

   pb_cache_release_all_buffers(&ws->bo_cache);
}

static void
mwv207_drm_bo_destory(struct mwv207_winsys *ws,
                      struct mwv207_bo *bo)
{
   struct drm_gem_close args;

   if(unlikely(ws->debug & 0x4))
      printf("%s bo handle %u size : %ld\n", __FUNCTION__, bo->handle, bo->base.base.size);

   if (bo->logical)
      drmUnmap(bo->logical, bo->base.base.size);

   simple_mtx_lock(&ws->bo_export_table_lock);
   if(bo->name)
      _mesa_hash_table_remove_key(ws->bo_name_table, &bo->name);
   _mesa_hash_table_remove_key(ws->bo_export_table, &bo->handle);
   simple_mtx_unlock(&ws->bo_export_table_lock);

   memset(&args, 0, sizeof(args));
   args.handle = bo->handle;
   drmIoctl(ws->fd, DRM_IOCTL_GEM_CLOSE, &args);

   FREE(bo);
}

static void
mwv207_bo_destroy(void *winsys, struct pb_buffer_lean *buf)
{
   struct mwv207_bo *bo = mwv207_bo(buf);

   mwv207_drm_bo_destory(bo->ws, bo);
}

static void
mwv207_bo_slab_destroy(void *winsys, struct pb_buffer *buf)
{
   struct mwv207_bo *bo = mwv207_bo(&buf->base);

   pb_slab_free(&bo->ws->bo_slabs, &bo->u.slab.entry);
}

static const struct pb_vtbl mwv207_winsys_bo_slab_vtbl = {
   mwv207_bo_slab_destroy

};

static void
mwv207_bo_destroy_or_cache(void *winsys, struct pb_buffer *buf)
{
   struct mwv207_bo *bo = mwv207_bo(&buf->base);

   if (bo->u.real.use_reusable_pool)
      pb_cache_add_buffer(&bo->ws->bo_cache, &bo->u.real.cache_entry);
   else
      mwv207_bo_destroy(bo->ws, &buf->base);
}

static const struct pb_vtbl mwv207_winsys_bo_vtbl = {
   mwv207_bo_destroy_or_cache

};

static struct mwv207_bo *
mwv207_drm_bo_create(struct mwv207_winsys *ws,
                     uint64_t size,
                     enum mwv207_bo_domain domain,
                     enum mwv207_bo_flag flag)
{
   union drm_mwv207_gem_create args;
   struct mwv207_bo *bo = NULL;
   int err;
   int heap = mwv207_get_heap_index(domain, flag);

   struct pb_slabs *last_slab = &ws->bo_slabs;
   unsigned max_slab_entry_size = 1 << (last_slab->min_order + last_slab->num_orders - 1);

   if ((flag & MWV207_FLAG_SUBALLOC) && size <= max_slab_entry_size) {
      struct pb_slab_entry *entry;
      if (heap < 0 || heap >= MWV207_MAX_SLAB_HEAPS)
         goto no_slab;

      struct pb_slabs *slabs = &ws->bo_slabs;
      entry = pb_slab_alloc(slabs, size, heap);
      if (!entry) {

         mwv207_clean_up_buffer_managers(ws);

         entry = pb_slab_alloc(slabs, size, heap);
      }
      if (!entry)
         return NULL;

      bo = container_of(entry, struct mwv207_bo, u.slab.entry);

      pipe_reference_init(&bo->base.base.reference, 1);

      if (unlikely(ws->debug & 0x4))
         printf("%s suballoc bo handle %u size : %lu\n", __FUNCTION__, bo->handle, bo->base.base.size);

      return bo;
   }
no_slab:

   if (flag & MWV207_FLAG_REUSE) {
      bo = (struct mwv207_bo*)
           pb_cache_reclaim_buffer(&ws->bo_cache, size, 0x1000, 0, heap);
      if (bo)
         return bo;
   }

   bo = CALLOC_STRUCT(mwv207_bo);
   if (!bo)
      return NULL;

   memset(&args, 0, sizeof(args));
   args.in.size = size;
   args.in.alignment = 0x1000;

   if (domain & MWV207_DOMAIN_VRAM)
      args.in.preferred_domain |= 0x2;
   if (domain & MWV207_DOMAIN_SYSTEM)
      args.in.preferred_domain |= 0x1;
   if (domain & MWV207_DOMAIN_GTT)
      args.in.preferred_domain |= 0x4;

   if (flag & MWV207_FLAG_CPU_ACCESS)
      args.in.flags |= 0x1;
   if (flag & MWV207_FLAG_NO_CPU_ACCESS)
      args.in.flags |= 0x2;

   if (flag & MWV207_FLAG_2D_ACCESS)
      args.in.alignment = 0x10000;

   err = drmCommandWriteRead(ws->fd, 0x1, &args, sizeof(args));
   if (err < 0) {
      fprintf(stderr, "mwv207: failed to create bo, size: 0x%lx, domain: 0x%x\n",
            size, domain);
      FREE(bo);
      return NULL;
   }

   pb_cache_init_entry(&ws->bo_cache, &bo->u.real.cache_entry, &bo->base.base, heap);

   bo->base.base.placement = args.in.alignment;
   bo->base.base.usage = 0;
   bo->base.base.size = size;
   bo->base.vtbl = &mwv207_winsys_bo_vtbl;

   bo->handle = args.out.handle;
   bo->logical = 0;
   bo->name = 0;
   bo->offset = 0;
   bo->u.slab.real = NULL;
   bo->ws = ws;
   if (flag & MWV207_FLAG_REUSE)
      bo->u.real.use_reusable_pool = true;
   else
      bo->u.real.use_reusable_pool = false;

   pipe_reference_init(&bo->base.base.reference, 1);

   if (unlikely(ws->debug & 0x4))
      printf("%s bo handle %u size : %lu\n", __FUNCTION__, bo->handle, bo->base.base.size);

   return bo;
}

static int64_t
get_abs_timeout(uint64_t timeout)
{
   uint64_t current_ns;

   if (timeout > INT64_MAX)
      return INT64_MAX;

   current_ns = os_time_get_nano();

   timeout += current_ns;
   if (timeout < current_ns)
      timeout = INT64_MAX;
   return (int64_t)timeout;
}

static int
mwv207_drm_bo_wait(struct mwv207_winsys *ws,
                   struct mwv207_bo *bo,
                   uint64_t timeout)
{
   struct drm_mwv207_gem_wait wait;
   int err;

   wait.handle = bo->handle;
   wait.op = 0x2;
   wait.timeout = get_abs_timeout(timeout);

   err = drmCommandWrite(ws->fd, 0x3, &wait, sizeof(wait));

   if(err < 0) {
      if(err != -EBUSY)
         printf("%s ret : %d\n", __FUNCTION__, err);
      return false;
   } else {
      return true;
   }
}

static void
mwv207_drm_bo_reference(struct mwv207_winsys *ws,
                        struct mwv207_bo **dst,
                        struct mwv207_bo *src)

{
   pb_reference((struct pb_buffer**)dst, (struct pb_buffer*)src);
}

static void *
mwv207_drm_bo_map(struct mwv207_winsys *ws, struct mwv207_bo *bo)
{
   union drm_mwv207_gem_mmap args;
   void *ptr;
   int err;

   if (bo->logical)
      return bo->logical;

   memset(&args, 0, sizeof(args));
   args.in.handle = bo->handle;

   err = drmCommandWriteRead(ws->fd, 0x2, &args, sizeof(args));
   if (err < 0)
      printf("%s ret : %d\n", __FUNCTION__, err);

   ptr = os_mmap(0, bo->base.base.size, PROT_READ | PROT_WRITE, MAP_SHARED, ws->fd, args.out.offset);

   if (ptr == MAP_FAILED)
     return NULL;

   bo->logical = (char *)ptr + bo->offset;

   /* FIX(jm9100-oss): O2 —— 返回值语义必须与上面的缓存分支一致。
    * 全函数以 bo->logical（= ptr + bo->offset）作为"该 BO 的 CPU 地址"的
    * 唯一语义, 而这里却返回未加偏移的 ptr。一旦 slab 子分配被启用
    * (bo->offset != 0), 同一 BO 首次映射与后续命中缓存会返回两个不同地址,
    * 调用方按 bo->offset 之外的口径写入就会写到相邻 BO 上。
    * 当前 MWV207_FLAG_SUBALLOC 从未写入 pipe_resource.flags, offset 恒为 0,
    * 故此 bug 处于潜伏状态（详见 docs/api/07-WINSYS-AUDIT.md）。 */
   return bo->logical;
}

static void
mwv207_drm_bo_unmap(struct mwv207_winsys *ws,
                    struct mwv207_bo *bo)
{
}

static uint32_t
mwv207_drm_bo_get_handle(struct mwv207_winsys *ws,
                         struct mwv207_bo *bo,
                         struct winsys_handle *whandle)
{
   if (whandle->type == WINSYS_HANDLE_TYPE_KMS){
      whandle->handle = bo->handle;
   } else if (whandle->type == WINSYS_HANDLE_TYPE_FD) {
      drmPrimeHandleToFD(ws->fd, bo->handle, DRM_RDWR | DRM_CLOEXEC, &bo->fd);
      whandle->handle = bo->fd;
   } else if (whandle->type == WINSYS_HANDLE_TYPE_SHARED) {
      int ret;
      struct drm_gem_flink args;
      memset(&args, 0, sizeof(args));
      args.handle = bo->handle;

      ret = drmIoctl(ws->fd, DRM_IOCTL_GEM_FLINK, &args);
      if (ret)
         return ret;
      whandle->handle = args.name;
      bo->name = args.name;
   }

   simple_mtx_lock(&ws->bo_export_table_lock);
   if (whandle->type == WINSYS_HANDLE_TYPE_SHARED)
      _mesa_hash_table_insert(ws->bo_name_table, &bo->name, bo);
   _mesa_hash_table_insert(ws->bo_export_table, &bo->handle, bo);
   simple_mtx_unlock(&ws->bo_export_table_lock);

   if(unlikely(ws->debug & 0x4))
      printf("%s bo handle %u type %u result : %u\n", __FUNCTION__, bo->handle, whandle->type, whandle->handle);

   return 0;
}

static struct mwv207_bo *
mwv207_drm_bo_from_handle(struct mwv207_winsys *ws,
                          struct winsys_handle *whandle,
                          uint32_t size)
{
   int err;
   uint32_t handle = 0;
   struct mwv207_bo *bo = NULL;

   simple_mtx_lock(&ws->bo_export_table_lock);

   if (whandle->type == WINSYS_HANDLE_TYPE_SHARED) {
      uint32_t name = whandle->handle;
      struct drm_gem_open args;

      bo = util_hash_table_get(ws->bo_name_table, &name);
      if(bo) {

         p_atomic_inc(&bo->base.base.reference.count);
         goto unlock;
      }

      args.name = name;
      if (drmIoctl(ws->fd, DRM_IOCTL_GEM_OPEN, &args)) {
         printf("gem-open failed: %s", strerror(errno));
         goto unlock;
      }

      handle = args.handle;
   } else if (whandle->type == WINSYS_HANDLE_TYPE_FD){
      int fd = whandle->handle;
      err = drmPrimeFDToHandle(ws->fd, fd, &handle);
      if(err < 0) {
         printf("%s ret : %d\n", __FUNCTION__, err);
         goto unlock;
      }
   }

   bo = util_hash_table_get(ws->bo_export_table, &handle);
   if(bo) {

      p_atomic_inc(&bo->base.base.reference.count);
      goto unlock;
   }

   bo = CALLOC_STRUCT(mwv207_bo);
   pb_cache_init_entry(&ws->bo_cache, &bo->u.real.cache_entry, &bo->base.base, 0);
   bo->base.base.placement = size;
   bo->base.base.usage = 0;
   bo->base.base.size = size;
   bo->base.vtbl = &mwv207_winsys_bo_vtbl;
   bo->handle = handle;
   bo->offset = 0;
   bo->u.slab.real = NULL;
   bo->u.real.use_reusable_pool = false;
   bo->ws = ws;

   pipe_reference_init(&bo->base.base.reference, 1);

   if (whandle->type == WINSYS_HANDLE_TYPE_SHARED) {
      bo->name = whandle->handle;
      _mesa_hash_table_insert(ws->bo_name_table, &bo->name, bo);
   }
   _mesa_hash_table_insert(ws->bo_export_table, &bo->handle, bo);

unlock:
   simple_mtx_unlock(&ws->bo_export_table_lock);

   if(unlikely(ws->debug & 0x4))
      printf("%s bo handle %u type %u result : %u\n", __FUNCTION__, bo->handle, whandle->type, whandle->handle);

   return bo;
}

static void
mwv207_drm_bo_dump(struct mwv207_winsys *ws,
                   struct mwv207_bo *bo, uint32_t size)
{
   int i = 0;
   uint32_t *buffer = (uint32_t *)ws->bo_map(ws, bo);
   uint32_t bo_size = bo->base.base.size / 4;

   if(bo_size < size || size == 0)
      size = bo_size;

   printf("================================================\n");
   printf("bo handle %u size : %u \n", bo->handle, size);
   printf("================================================\n");

   for (i = 1; i <= size; i++) {
      printf("0x%08x, ", *buffer++);
      if ((i % 8) == 0)  printf("\n");
   }

   printf("\n");
   printf("================================================\n");
   ws->bo_unmap(ws, bo);
}

static bool
mwv207_bo_can_reclaim(void *winsys, struct pb_buffer_lean *buf)
{
   struct mwv207_bo *bo = mwv207_bo(buf);

   if (bo->base.base.reference.count != 0) {
      return false;
   }

   return mwv207_drm_bo_wait(bo->ws, bo, OS_TIMEOUT_INFINITE);
}

static bool
mwv207_bo_can_reclaim_slab(void *priv, struct pb_slab_entry *entry)
{
   struct mwv207_winsys *ws = priv;
   struct mwv207_bo *bo = NULL;
   bo = container_of(entry, struct mwv207_bo, u.slab.entry);

   if (bo->base.base.reference.count != 0) {
      return false;
   }

   return mwv207_drm_bo_wait(ws, bo, OS_TIMEOUT_INFINITE);
}

static struct pb_slab *
mwv207_bo_slab_alloc(void *priv, unsigned heap,
                     unsigned entry_size,
                     unsigned group_index)
{
   struct mwv207_winsys *ws = priv;
   struct mwv207_slab *slab = CALLOC_STRUCT(mwv207_slab);
   enum mwv207_bo_domain domains = mwv207_domain_from_heap(heap);
   enum mwv207_bo_flag flags = mwv207_flags_from_heap(heap);
   unsigned slab_size = 0;

   if (!slab)
      return NULL;

   struct pb_slabs *slabs = &ws->bo_slabs;
   unsigned max_entry_size = 1 << (slabs->min_order + slabs->num_orders - 1);

   if (entry_size <= max_entry_size) {

      slab_size = max_entry_size * 2;
   }
   assert(slab_size != 0);

   slab->buffer = ws->bo_create(ws, slab_size, domains, flags);
   if (!slab->buffer)
      goto fail;

   ws->bo_map(ws, slab->buffer);

   slab->base.num_entries = slab->buffer->base.base.size / entry_size;
   slab->base.num_free = slab->base.num_entries;
   slab->entries = CALLOC(slab->base.num_entries, sizeof(*slab->entries));
   if (!slab->entries)
      goto fail_buffer;

   list_inithead(&slab->base.free);

   for (unsigned i = 0; i < slab->base.num_entries; ++i) {
      struct mwv207_bo *bo = &slab->entries[i];
      bo->base.base.placement = entry_size;
      bo->base.base.usage = 0;
      bo->base.base.size = entry_size;
      bo->base.vtbl = &mwv207_winsys_bo_slab_vtbl;

      bo->handle = slab->buffer->handle;
      bo->logical = 0;
      bo->name = 0;
      bo->offset = i * entry_size;
      bo->logical = (char *)slab->buffer->logical + bo->offset;
      bo->ws = ws;

      slab->base.group_index = group_index;
      bo->u.slab.entry.slab = &slab->base;

      bo->u.slab.real = slab->buffer;

      list_addtail(&bo->u.slab.entry.head, &slab->base.free);
   }

   return &slab->base;

fail_buffer:
   mwv207_drm_bo_destory(ws, slab->buffer);
fail:
   FREE(slab);
   return NULL;
}

static void
mwv207_bo_slab_free(void *priv, struct pb_slab *pslab)
{
   struct mwv207_winsys *ws = priv;
   struct mwv207_slab *slab = mwv207_slab(pslab);

   FREE(slab->entries);
   mwv207_drm_bo_destory(ws, slab->buffer);;
   FREE(slab);
}

void mwv207_bo_init_function(struct mwv207_winsys *ws)
{
   ws->bo_create      = mwv207_drm_bo_create;
   ws->bo_wait        = mwv207_drm_bo_wait;
   ws->bo_map         = mwv207_drm_bo_map;
   ws->bo_unmap       = mwv207_drm_bo_unmap;
   ws->bo_dump        = mwv207_drm_bo_dump;
   ws->bo_reference   = mwv207_drm_bo_reference;
   ws->bo_get_handle  = mwv207_drm_bo_get_handle;
   ws->bo_from_handle = mwv207_drm_bo_from_handle;

   pb_cache_init(&ws->bo_cache, MWV207_MAX_CACHED_HEAPS,
                 500000, 2.0f, 0, 0x200000,
                 offsetof(struct mwv207_bo, u.real.cache_entry), NULL,
                 mwv207_bo_destroy, mwv207_bo_can_reclaim);

   unsigned min_slab_order = 9;
   unsigned max_slab_order = 18;
   unsigned num_slab_orders_per_allocator = (max_slab_order - min_slab_order) / 0x3;

   unsigned min_order = min_slab_order;
   unsigned max_order = MIN2(min_order + num_slab_orders_per_allocator,
                             max_slab_order);

   if (!pb_slabs_init(&ws->bo_slabs,
                      min_order, max_order,
                      MWV207_MAX_SLAB_HEAPS, false,
                      ws,
                      mwv207_bo_can_reclaim_slab,
                      mwv207_bo_slab_alloc,
                      mwv207_bo_slab_free))
      printf("pb_slabs_init faile\n");

}

