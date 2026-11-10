/*
* SPDX-License-Identifier: GPL
*
* Copyright (c) 2020 ChangSha JingJiaMicro Electronics Co., Ltd.
* All rights reserved.
*
* Author:
*      shanjinkui <shanjinkui@jingjiamicro.com>
*
* The software and information contained herein is proprietary and
* confidential to JingJiaMicro Electronics. This software can only be
* used by JingJiaMicro Electronics Corporation. Any use, reproduction,
* or disclosure without the written permission of JingJiaMicro
* Electronics Corporation is strictly prohibited.
*/
#ifndef MWV207_H_VTIQLF2Y
#define MWV207_H_VTIQLF2Y

#include <linux/version.h>
#include <linux/pci.h>
#include <drm/drm_device.h>
#include <drm/ttm/ttm_bo.h>
#include <linux/mutex.h>
#include <linux/idr.h>

struct mwv207_db;

struct mwv207_vbios {

	struct mutex vcmd_lock;

	struct mutex cfg_lock;
	struct idr   cfg_table;

	int          indexer_valid;
	int          sector_count;
};

#define  ddev_to_jdev(ddev) ((struct mwv207_device *) (ddev)->dev_private)
#define  bdev_to_jdev(dev)  container_of(dev, struct mwv207_device, bdev)

/*
 * 每个引擎（不是每块卡）一个 drm_gpu_scheduler，拓扑为
 * 3d0 3d1 dec0 dec1 enc0 2d0 dma0 —— 共 7 个。
 * 原先数组只有 6 槽，且 sched_enc 被摆到 &sched_dec[1]，
 * 与 dec1 撞在同一槽（N1）：enc0 覆盖 dec1，nr_dec 仍报 2。
 */
#define MWV207_MAX_SCHED_NR 7
struct mwv207_device {
	struct drm_device  base;
	struct pci_dev    *pdev;
	struct ttm_device bdev;

	void __iomem   *mmio;
	void __iomem   *iatu;

	void           *win;

	spinlock_t win_lock;
	u64     visible_vram_size;
	u64     vram_size;
	u64     vram_bar_base;
	u64     vram_last_win;
	u64     pci_win_base;

	struct device    *dev;
	struct drm_fb_helper *fb_helper;

	struct drm_gpu_scheduler *sched[MWV207_MAX_SCHED_NR];

	struct drm_gpu_scheduler **sched_3d;
	struct drm_gpu_scheduler **sched_dec;
	struct drm_gpu_scheduler **sched_enc;
	struct drm_gpu_scheduler **sched_2d;
	struct drm_gpu_scheduler **sched_dma;
	int    nr_3d;
	int    nr_2d;
	int    nr_dec;
	int    nr_enc;
	int    nr_dma;

	struct drm_sched_entity *dma_entity;

	spinlock_t irq_lock;
	struct irq_domain *irq_domain;
	u32 irq_enable_reg[2];
	int va_irq;

	/* key/value database */
	struct mwv207_db *db;

	struct mwv207_vbios vbios;

	struct mutex gpio_lock;

	bool lite;
};

struct mwv207_ctx_mgr {
	struct mutex lock;
	struct idr  handle_table;
};

struct mwv207_fpriv {
	struct mwv207_ctx_mgr ctx_mgr;
};

int mwv207_test(struct mwv207_device *jdev);
void jdev_write_vram(struct mwv207_device *jdev, u64 vram_addr, void *buf, int size);

static inline void jdev_write(struct mwv207_device *jdev, u32 reg, u32 value)
{
	/* 寄存器窗口从 0x100000 起；<= 0x100000 多半是把 VRAM 地址/裸偏移当
	 * 寄存器用了。原实现 BUG_ON，会 panic 整个内核；改成丢弃这次写。 */
	if (WARN_ON_ONCE(reg <= 0x100000))
		return;
	writel_relaxed(value, jdev->mmio + reg);
}

static inline u32 jdev_read(struct mwv207_device *jdev, u32 reg)
{
	return readl(jdev->mmio + reg);
}

static inline void jdev_modify(struct mwv207_device *jdev, u32 reg, u32 mask, u32 value)
{
	u32 rvalue;

	if (WARN_ON_ONCE(reg <= 0x100000))
		return;

	rvalue = jdev_read(jdev, reg);
	rvalue = (rvalue & ~mask) | (value & mask);
	jdev_write(jdev, reg, rvalue);
}

#define to_fpriv(drm_file)  ((drm_file)->driver_priv)

#endif
