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





#ifndef __MWV207_DRM_H__
#define __MWV207_DRM_H__

#include "drm/drm.h"

#if defined(__cplusplus)
extern "C" {
#endif

/**********************GEM interface*************************/
#define MWV207_GEM_DOMAIN_SYSTEM         0x1
#define MWV207_GEM_DOMAIN_VRAM           0x2
#define MWV207_GEM_DOMAIN_GTT            0x4
#define MWV207_GEM_DOMAIN_MASK           (MWV207_GEM_DOMAIN_SYSTEM | MWV207_GEM_DOMAIN_VRAM | MWV207_GEM_DOMAIN_GTT)

#define MWV207_GEM_CREATE_CPU_VISIBLE    (1 << 0)
#define MWV207_GEM_CREATE_CPU_INVISIBLE  (1 << 1)
#define MWV207_GEM_CREATE_GPU_PHYS32     (1 << 2)
#define MWV207_GEM_CREATE_MASK           (MWV207_GEM_CREATE_CPU_VISIBLE | MWV207_GEM_CREATE_CPU_INVISIBLE | MWV207_GEM_CREATE_GPU_PHYS32)
struct drm_mwv207_gem_create_in {
	__u64 size;
	__u64 alignment;
	__u32 preferred_domain;
	__u32 flags;
};
struct drm_mwv207_gem_create_out {
	__u32 handle;
	__u32 resv;
};
union drm_mwv207_gem_create {
	struct drm_mwv207_gem_create_in in;
	struct drm_mwv207_gem_create_out out;
};

struct drm_mwv207_gem_mmap_in {
	__u32 handle;
	__u32 pad;
};
struct drm_mwv207_gem_mmap_out {
	__u64 offset;
};
union drm_mwv207_gem_mmap {
	struct drm_mwv207_gem_mmap_in in;
	struct drm_mwv207_gem_mmap_out out;
};

#define MWV207_GEM_WAIT_READ  0x00000001
#define MWV207_GEM_WAIT_WRITE 0x00000002
struct drm_mwv207_gem_wait {
	__u32 handle;
	__u32 op;
	__s64 timeout;
};

/**********************context interface*************************/
#define MWV207_CTX_OP_CREATE  0
#define MWV207_CTX_OP_DESTROY 1
struct drm_mwv207_ctx {
	__u32 op;
	__u32 flags;
	__u32 handle;
	__u32 resv;
};

/**********************submission interface*************************
 * 1. a submission is sent to a single ctx
 * 2. a submission optionally contains a relocation table and a bo table
 * 3. bo table should not contain duplicates;
 * 4. cmd buffer size should be limited to 16K
 */

#define MWV207_SUBMIT_ENGINE_ID_ANY   0xffffffff
#define MWV207_SUBMIT_ENGINE_TYPE_3D  0
#define MWV207_SUBMIT_ENGINE_TYPE_DEC 1
#define MWV207_SUBMIT_ENGINE_TYPE_ENC 2
#define MWV207_SUBMIT_ENGINE_TYPE_2D  3
#define MWV207_SUBMIT_ENGINE_TYPE_DMA 4

#define MWV207_SUBMIT_FLAG_FENCE_FD_IN   0x00000001
#define MWV207_SUBMIT_FLAG_FENCE_FD_OUT  0x00000002
#define MWV207_SUBMIT_FLAG_MASK   (MWV207_SUBMIT_FLAG_FENCE_FD_IN | MWV207_SUBMIT_FLAG_FENCE_FD_OUT)

struct drm_mwv207_submit {
	__u32 ctx;
	__u32 engine_type;
	__u32 engine_id;
	__u32 flags;
	__u32 fence_fd;    /* in/out fence fd, according to MWV207_SUBMIT_FLAG_FENCE_IN/OUT */
	__u32 cmd_size;
	__u32 nr_relocs;
	__u32 nr_bos;
	__u32 nr_cmd_dat;
	__u32 padding;
	__u64 cmds;
	__u64 relocs;
	__u64 bos;
	__u64 cmd_dats;
	/* syncobj support, reserved for vulkan
	 * __u32 nr_in_syncobjs;
	 * __u32 nr_out_syncobjs;
	 * __u64 in_syncobjs;
	 * __u64 out_syncobjs;
	 */
};

/* in_syncobjs/out_syncobjs array element, reserved for vulkan */
struct drm_mwv207_submit_syncobj {
	__u32 handle;
	__u32 flags;
	__u64 point;
};

#define MWV207_BO_ACC_FLAG_READ      0x00000000
#define MWV207_BO_ACC_FLAG_WRITE     0x00000001
struct drm_mwv207_bo_acc {
	__u32 handle;
	__u32 flags;
};

#define MWV207_SUBMIT_RELOC_WORD64    0
#define MWV207_SUBMIT_RELOC_LOW32     1
#define MWV207_SUBMIT_RELOC_HIGH32    2
#define MWV207_SUBMIT_RELOC_32_16     3
#define MWV207_SUBMIT_RELOC_FLAG_CMD_DAT  0x80000000
struct drm_mwv207_submit_reloc {
	__u32 idx;    /* bo/cmd_dat idx in submit bos/cmd_dats */
	__u32 cmd_offset;
	__u64 bo_offset;
	__u32 type;
	__u32 pad;
};


struct drm_mwv207_submit_cmd_dat {
	__u32 nr_relocs;
	__u32 dat_size;
	__u64 relocs;
	__u64 dat;
};

/**************** card capability interface *****************/
/* XXX: keys are user/kernel ABI, DO THINK TWICE when
 * add keys, once added it can never be removed.
 */
enum drm_mwv207_info_key {
	DRM_MWV207_ACTIVE_3D_NR = 0,
	DRM_MWV207_ACTIVE_DEC_NR,
	DRM_MWV207_ACTIVE_ENC_NR,
	DRM_MWV207_ACTIVE_2D_NR,
	DRM_MWV207_ACTIVE_DMA_NR,
};

struct drm_mwv207_info_rec {
	__u32 key;
	__u32 val;
};

#define MWV207_INFO_OP_QUERY_ALL     0
#define MWV207_INFO_OP_QUERY_REC     1
struct drm_mwv207_info {
	__u32 op;
	__u32 nr_recs;
	__u64 recs;
};

#define DRM_MWV207_INFO          0x00
#define DRM_MWV207_GEM_CREATE    0x01
#define DRM_MWV207_GEM_MMAP      0x02
#define DRM_MWV207_GEM_WAIT      0x03
#define DRM_MWV207_CTX           0x04
#define DRM_MWV207_SUBMIT        0x05

#define DRM_IOCTL_MWV207_INFO        DRM_IOWR(DRM_COMMAND_BASE + DRM_MWV207_INFO, struct drm_mwv207_info)
#define DRM_IOCTL_MWV207_GEM_CREATE  DRM_IOWR(DRM_COMMAND_BASE + DRM_MWV207_GEM_CREATE, union drm_mwv207_gem_create)
#define DRM_IOCTL_MWV207_GEM_MMAP    DRM_IOWR(DRM_COMMAND_BASE + DRM_MWV207_GEM_MMAP, union drm_mwv207_gem_mmap)
#define DRM_IOCTL_MWV207_GEM_WAIT    DRM_IOWR(DRM_COMMAND_BASE + DRM_MWV207_GEM_WAIT, struct drm_mwv207_gem_wait)
#define DRM_IOCTL_MWV207_CTX         DRM_IOWR(DRM_COMMAND_BASE + DRM_MWV207_CTX, struct drm_mwv207_ctx)
#define DRM_IOCTL_MWV207_SUBMIT      DRM_IOWR(DRM_COMMAND_BASE + DRM_MWV207_SUBMIT, struct drm_mwv207_submit)
#if defined(__cplusplus)
}
#endif

#endif

