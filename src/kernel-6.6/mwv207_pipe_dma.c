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
#include <linux/dma-fence.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/freezer.h>
#include <linux/list.h>
#include <linux/irqdomain.h>
#include <linux/workqueue.h>
#include "mwv207_bo.h"
#include "mwv207_sched.h"

#undef MWV207_DMA_DEBUG

#define DMA_MAX_LLI_COUNT    (0x400000 / sizeof(struct mwv207_lli))

#define DMA_TS_NOT_ALIGNED(size)  ((size) - 1)
#define DMA_TS_ALIGNED(size, alignment)  (((size) >= (alignment)) ? ((size) / (alignment) - 1) : 0)
#define DMA_CTL_NOT_ALIGNED ((1UL<<(63)) \
				| (1UL<<(58)) \
				| (1UL<<(30)) \
				| (1UL<<(38)) \
				| (1UL<<(47)) \
				| ((32ul & (0xFF))<<(39)) \
				| ((32ul & (0xFF))<<(48)) \
				| ((0UL & (0x7))<<(8)) \
				| ((0UL & (0x7))<<(11)))

#define DMA_CTL_ALIGNED(a)  ((1UL<<(63)) \
				| (1UL<<(58)) \
				| (1UL<<(30)) \
				| (1UL<<(38)) \
				| (1UL<<(47)) \
				| ((32ul & (0xFF))<<(39)) \
				| ((32ul & (0xFF))<<(48)) \
				| (((((a) == 64)?6UL:5UL) & (0x7))<<(8)) \
				| (((((a) == 64)?6UL:5UL) & (0x7))<<(11)))

#define mwv207_timed_loop(tick, cond, timeout) \
	for ((tick) = jiffies; (cond) && time_before(jiffies, (tick) + msecs_to_jiffies(timeout));)

struct mwv207_lli {
	u64 sar;
	u64 dar;
	u32 ts;
	u32 resv1;
	u64 llp;
	u64 ctl;
	u32 sstat;
	u32 dstat;
	u64 llpstat;
	u32 resv2;
	u32 resv3;
};

#define to_dma_pipe(pipe) container_of(pipe, struct mwv207_pipe_dma, base)

/*
 * FIX(jm9100-oss,N4): 提交不再在 run_job 里自旋。
 *
 * 原来 pipe_dma_commit() 在 drm_sched 的 run_job 工作线程里 cpu_relax() 自旋
 * 最多 5 s 等引擎完成，再返回 dma_fence_get_stub()。后果有两个：
 *   1) 引擎真挂住时，调度器工作线程被白白占住 5 s（同一 wq 上的其它任务全被拖延）；
 *   2) stub fence 在提交时就已经是 signaled 状态，fence 语义退化（永远看不到
 *      真实的 -ETIMEDOUT / -EIO）。
 *
 * 现在改成：submit 只编程 LLI 并启动引擎，然后返回一个真实的 dma_fence；
 * “轮询完成 → 清通道 → 失效目的端缓存 → signal”搬到本管线自己的有序工作队列里
 * 做（usleep_range 睡眠轮询，不再抢占 CPU）。
 * 复位/卸载路径沿用 N14 的范式：cancel_work_sync() 后 xchg(&current_fence)
 * 抢到槽再以 -EIO 完成，避免 worker 与 reset 各 put 一次。
 *
 * 轮询上限特意取 1500 ms（调度器的 job 超时是 2000 ms）：超时先由本 worker 以
 * -ETIMEDOUT 收尾，不让 TDR 在 worker 还没收场时插进来复位硬件。
 */
#define MWV207_DMA_POLL_TIMEOUT_MS	1500

struct mwv207_pipe_dma {
	struct mwv207_pipe base;
	int   unit;
	u32 alignment;
	struct mwv207_lli   *lli;
	struct mwv207_lli   *last_lli;
	dma_addr_t    lli_bus_addr;
	int cur_idx;

	bool coherent;

	/* N4: 一次只有一个在飞的 DMA job（drm_sched 按引擎串行），故单槽即可 */
	struct dma_fence  *current_fence;
	spinlock_t         fence_lock;
	u64                fence_ctx;
	unsigned int       next_fence;
	struct workqueue_struct *complete_wq;
	struct work_struct complete_work;
	struct mwv207_dma_cmd pending_cmd;
};

struct mwv207_dma_cursor {
	bool is_vram;
	u64 line_pos;
	u64 cur_pos;

	dma_addr_t *dma_address;
	u64 nr_pages;

	u64 width;
	u64 stride;
};
static inline void pipe_dma_write(struct mwv207_pipe_dma *pipe, u32 reg, u32 value)
{
	pipe_write(&pipe->base, reg, value);
}

static inline u32 pipe_dma_read(struct mwv207_pipe_dma *pipe, u32 reg)
{
	return pipe_read(&pipe->base, reg);
}

static inline void pipe_dma_chan_write(struct mwv207_pipe_dma *pipe, int chan, u32 reg, u32 val)
{
	pipe_dma_write(pipe, chan * 0x200 + reg, val);
}

static inline u32 pipe_dma_chan_read(struct mwv207_pipe_dma *pipe, int chan, u32 reg)
{
	return pipe_dma_read(pipe, chan * 0x200 + reg);
}

static inline int pipe_dma_chan_done(struct mwv207_pipe_dma *pipe, int chan)
{
	u32 status;

	if (pipe->last_lli->ctl & (1UL << (63)))
		return 0;

	status = pipe_dma_chan_read(pipe, chan, (0x188));
	return !!(status & (1 << (1)));
}

static inline void pipe_dma_chan_clear(struct mwv207_pipe_dma *pipe, int chan)
{
	pipe_dma_chan_write(pipe, chan, (0x198), 1 << (1));
}

static inline int pipe_dma_stop(struct mwv207_pipe_dma *pipe)
{
	pr_info("%s TBD", __func__);
	return 0;
}

/* 编程 LLI 并启动通道；不等待完成 */
static void pipe_dma_start(struct mwv207_pipe_dma *pipe)
{
	int chan = 0;
	u64 llp;

	if (pipe->cur_idx == 0)
		return;

	pipe->last_lli = &pipe->lli[pipe->cur_idx - 1];
	pipe->last_lli->ctl |=  (1UL << (62));

	mb();

	pipe_dma_chan_write(pipe, chan, (0x120),
			       ((0x3)<<(2))
			       |((0x3)<<(0)));
	pipe_dma_chan_write(pipe, chan, (0x124), 0x3b8e0000);

	llp = pipe->lli_bus_addr + 0x800000000ULL;
	pipe_dma_chan_write(pipe, chan, (0x128), llp & 0xFFFFFFFF);
	pipe_dma_chan_write(pipe, chan, (0x12C),  llp >> 32);

	pipe_dma_write(pipe, (0x10),
			(1 << (1))
			|(1 << (0)));

	pipe_dma_write(pipe, (0x18),
			    ((1<<chan)<<(0))
			    |((1<<chan)<<(8)));
}

/*
 * 等待完成；返回 false 表示超时。
 * 前 200 次先短自旋（典型搬移几十个读周期内就结束，不额外增加延迟），
 * 之后转成睡眠轮询，避免长时间占 CPU。
 */
static bool pipe_dma_wait(struct mwv207_pipe_dma *pipe, unsigned int timeout_ms)
{
	unsigned long tick = jiffies;
	int spin = 0;

	while (!pipe_dma_chan_done(pipe, 0)) {
		if (time_after(jiffies, tick + msecs_to_jiffies(timeout_ms)))
			return false;
		if (++spin < 200)
			cpu_relax();
		else
			usleep_range(50, 200);
	}

	return true;
}

static void pipe_dma_clear_chan(struct mwv207_pipe_dma *pipe)
{
	pipe_dma_chan_clear(pipe, 0);
	pipe->cur_idx = 0;
}

/*
 * LLI 数组是复用的：中间批次（LLI 用满）必须先等在飞批次完成再重新装填，
 * 所以这里仍然同步等待。真正的大块搬移一次就能装进 DMA_MAX_LLI_COUNT 条，
 * 中间批次只是兜底路径。
 */
static void pipe_dma_commit(struct mwv207_pipe_dma *pipe)
{
	if (pipe->cur_idx == 0)
		return;

	pipe_dma_start(pipe);

	if (!pipe_dma_wait(pipe, 5000))
		pr_err("mwv207: wait for completion of DMA timeout: 0x%x",
				pipe_dma_chan_read(pipe, 0, (0x188)));

	pipe_dma_clear_chan(pipe);
}

static void pipe_dma_fill_lli(struct mwv207_pipe_dma *pipe, u64 src, u64 dst, u64 len)
{
	struct mwv207_lli *lli;

	if (pipe->cur_idx >= DMA_MAX_LLI_COUNT)
		pipe_dma_commit(pipe);

	lli = &pipe->lli[pipe->cur_idx];
	lli->sar = src;
	lli->dar = dst;

	if (IS_ALIGNED(src, pipe->alignment) &&
	    IS_ALIGNED(dst, pipe->alignment) &&
	    IS_ALIGNED(len, pipe->alignment)) {
		lli->ts = DMA_TS_ALIGNED(len, pipe->alignment);
		lli->ctl = DMA_CTL_ALIGNED(pipe->alignment);
	} else {
		lli->ts = DMA_TS_NOT_ALIGNED(len);
		lli->ctl = DMA_CTL_NOT_ALIGNED;
	}

#ifdef MWV207_DMA_DEBUG
	{
		pr_info("[dma]: lli[%04d] sar  = 0x%010llx", pipe->cur_idx, lli->sar);
		pr_info("[dma]: lli[%04d] dar  = 0x%010llx", pipe->cur_idx, lli->dar);
		pr_info("[dma]: lli[%04d] ts   = 0x%08x", pipe->cur_idx, lli->ts);
		pr_info("[dma]: lli[%04d] llp  = 0x%010llx", pipe->cur_idx, lli->llp);
		pr_info("[dma]: lli[%04d] ctl  = 0x%010llx", pipe->cur_idx, lli->ctl);
	}
#endif
	pipe->cur_idx++;
}

static void mwv207_cursor_init(struct mwv207_dma_cursor *cursor, struct mwv207_dma_loc *loc,
		u64 width, u64 stride)
{
	cursor->width = width;
	cursor->stride = stride;
	cursor->is_vram = MWV207_DMA_IS_VRAM(loc->pg_nr_type);
	if (cursor->is_vram) {
		cursor->line_pos = loc->base + loc->offset;
		cursor->cur_pos = cursor->line_pos;
		cursor->dma_address = NULL;
		cursor->nr_pages = MWV207_DMA_NR_PAGES(loc->pg_nr_type);
	} else {
		cursor->line_pos = loc->offset;
		cursor->cur_pos = cursor->line_pos;
		cursor->dma_address = (dma_addr_t *)loc->base;
		cursor->nr_pages = MWV207_DMA_NR_PAGES(loc->pg_nr_type);
	}
}

static void mwv207_cursor_reset(struct mwv207_dma_cursor *cursor, struct mwv207_dma_loc *loc)
{
	if (cursor->is_vram) {
		cursor->line_pos = loc->base + loc->offset;
		cursor->cur_pos = cursor->line_pos;
	} else {
		cursor->line_pos = loc->offset;
		cursor->cur_pos = cursor->line_pos;
	}
}

static inline u64 cursor_seg_len(struct mwv207_dma_cursor *cursor)
{
	u64 len = cursor->line_pos + cursor->width - cursor->cur_pos;
	u64 offset, idx, contig_len;

	if (cursor->is_vram)
		return len;
	offset = cursor->cur_pos & (PAGE_SIZE - 1);
	idx = cursor->cur_pos >> PAGE_SHIFT;

	contig_len = PAGE_SIZE - offset;
	while (contig_len < len &&
			idx + 1 < cursor->nr_pages &&
			cursor->dma_address[idx] + PAGE_SIZE == cursor->dma_address[idx + 1]) {
		contig_len += PAGE_SIZE;
		idx++;
	}
	return min_t(u64, len, contig_len);
}

static inline u64 cursor_axi(struct mwv207_dma_cursor *cursor)
{
	u64 idx, offset;

	if (cursor->is_vram)
		return cursor->cur_pos;

	idx = cursor->cur_pos >> PAGE_SHIFT;
	offset = cursor->cur_pos & (PAGE_SIZE - 1);
	return 0x800000000ULL + cursor->dma_address[idx] + offset;
}

static inline void cursor_advance(struct mwv207_dma_cursor *cursor, u64 len)
{
	BUG_ON(cursor->cur_pos + len > cursor->line_pos + cursor->width);

	if (cursor->cur_pos + len == cursor->line_pos + cursor->width) {
		cursor->line_pos += cursor->stride;
		cursor->cur_pos = cursor->line_pos;
	} else
		cursor->cur_pos += len;
}

static void pipe_dma_flush(struct mwv207_pipe_dma *pipe,
		struct mwv207_dma_cursor *cursor, u64 size)
{
	u64 remain, len;

	if (cursor->is_vram || pipe->coherent)
		return;

	for (remain = size; remain > 0; remain -= len) {
		len = min_t(u64, cursor_seg_len(cursor), remain);
		dma_sync_single_for_device(pipe->base.jdev->dev,
				cursor_axi(cursor) - 0x800000000ULL,
				len, DMA_TO_DEVICE);
		cursor_advance(cursor, len);
	}
}

static void pipe_dma_invalidate(struct mwv207_pipe_dma *pipe,
		struct mwv207_dma_cursor *cursor, u64 size)
{
	u64 remain, len;

	if (cursor->is_vram || pipe->coherent)
		return;

	for (remain = size; remain > 0; remain -= len) {
		len = min_t(u64, cursor_seg_len(cursor), remain);
		dma_sync_single_for_cpu(pipe->base.jdev->dev,
				cursor_axi(cursor) - 0x800000000ULL,
				len, DMA_FROM_DEVICE);
		cursor_advance(cursor, len);
	}
}

struct pipe_dma_fence {
	struct dma_fence base;
	struct mwv207_pipe_dma *pipe;
};

static const char *pipe_dma_fence_get_driver_name(struct dma_fence *fence)
{
	return "mwv207_dma";
}

static const char *pipe_dma_fence_get_timeline_name(struct dma_fence *fence)
{
	struct pipe_dma_fence *f = container_of(fence, struct pipe_dma_fence, base);

	return f->pipe->base.fname;
}

static void pipe_dma_fence_release(struct dma_fence *fence)
{
	struct pipe_dma_fence *f = container_of(fence, struct pipe_dma_fence, base);

	kfree_rcu(f, base.rcu);
}

static const struct dma_fence_ops pipe_dma_fence_ops = {
	.get_driver_name = pipe_dma_fence_get_driver_name,
	.get_timeline_name = pipe_dma_fence_get_timeline_name,
	.release = pipe_dma_fence_release,
};

static struct dma_fence *pipe_dma_fence_alloc(struct mwv207_pipe_dma *pipe)
{
	struct pipe_dma_fence *fence;

	fence = kzalloc(sizeof(*fence), GFP_KERNEL);
	if (!fence) {
		pr_err("mwv207: failed to alloc fence for %s\n", pipe->base.fname);
		return ERR_PTR(-ENOMEM);
	}

	dma_fence_init(&fence->base, &pipe_dma_fence_ops, &pipe->fence_lock,
		       pipe->fence_ctx, ++pipe->next_fence);
	fence->pipe = pipe;

	return &fence->base;
}

/* 完成工作：睡眠式轮询 → 清通道 → 失效目的端缓存 → signal fence */
static void pipe_dma_complete_work(struct work_struct *work)
{
	struct mwv207_pipe_dma *pipe = container_of(work, struct mwv207_pipe_dma,
						    complete_work);
	struct mwv207_dma_cursor dst;
	struct dma_fence *fence;
	bool done;

	done = pipe_dma_wait(pipe, MWV207_DMA_POLL_TIMEOUT_MS);
	if (!done)
		pr_err("mwv207: wait for completion of DMA timeout: 0x%x\n",
				pipe_dma_chan_read(pipe, 0, (0x188)));

	/* 先清通道、再失效缓存，最后才 signal：消费者拿到 fence 时数据已可用 */
	pipe_dma_clear_chan(pipe);

	mwv207_cursor_init(&dst, &pipe->pending_cmd.dst, pipe->pending_cmd.width,
			pipe->pending_cmd.dst.stride);
	pipe_dma_invalidate(pipe, &dst, pipe->pending_cmd.width * pipe->pending_cmd.height);

	spin_lock_irq(&pipe->fence_lock);
	fence = xchg(&pipe->current_fence, NULL);
	spin_unlock_irq(&pipe->fence_lock);

	if (!fence)
		return;

	if (!done)
		dma_fence_set_error(fence, -ETIMEDOUT);

	dma_fence_signal(fence);
	dma_fence_put(fence);
}

/*
 * 复位/卸载时收掉在飞 fence：worker 与 reset 用 xchg 抢槽，谁抢到谁 put，
 * 避免双重完成/UAF（与 N14 各管线的做法一致）。
 */
static void pipe_dma_force_complete(struct mwv207_pipe_dma *pipe)
{
	struct dma_fence *fence;

	if (pipe->complete_wq)
		cancel_work_sync(&pipe->complete_work);

	spin_lock_irq(&pipe->fence_lock);
	fence = xchg(&pipe->current_fence, NULL);
	spin_unlock_irq(&pipe->fence_lock);

	if (!fence)
		return;

	pipe_dma_clear_chan(pipe);

	dma_fence_set_error(fence, -EIO);
	dma_fence_signal(fence);
	dma_fence_put(fence);
}

static void mwv207_pipe_dma_reset(struct mwv207_pipe *mpipe)
{
	struct mwv207_pipe_dma *pipe = to_dma_pipe(mpipe);

	/* 复位语义：在飞 fence 一律以 -EIO 结束，等待者不会永久阻塞 */
	pipe_dma_force_complete(pipe);

	pipe_dma_chan_clear(pipe, 0);
	pipe->cur_idx = 0;
}

static struct dma_fence *mwv207_pipe_dma_submit(struct mwv207_pipe *mpipe,
		struct mwv207_job *mjob)
{
	struct mwv207_pipe_dma *pipe = to_dma_pipe(mpipe);
	struct mwv207_dma_cmd *cmd = (struct mwv207_dma_cmd *)mjob->cmds;
	struct mwv207_dma_cursor src, dst;
	struct dma_fence *fence;
	u64 remain, len;

	BUG_ON(mjob->cmd_size != sizeof(struct mwv207_dma_cmd));

	/*
	 * 防御：drm_sched 按引擎串行，上一个 fence 完成前不会再次 run_job。
	 * 真出现重入说明调度器状态已经错乱，这里先把旧 fence 以 -EIO 收掉，
	 * 至少不让等待者永久阻塞。
	 */
	if (WARN_ON(pipe->current_fence))
		pipe_dma_force_complete(pipe);

	mwv207_cursor_init(&src, &cmd->src, cmd->width, cmd->src.stride);
	pipe_dma_flush(pipe, &src, cmd->width * cmd->height);

	mwv207_cursor_reset(&src, &cmd->src);
	mwv207_cursor_init(&dst, &cmd->dst, cmd->width, cmd->dst.stride);
	for (remain = cmd->width * cmd->height; remain > 0; remain -= len) {
		len = min_t(u64, cursor_seg_len(&src), 0x400000);
		len = min_t(u64, cursor_seg_len(&dst), len);
		len = min_t(u64, len, remain);

		pipe_dma_fill_lli(pipe, cursor_axi(&src), cursor_axi(&dst), len);

		cursor_advance(&src, len);
		cursor_advance(&dst, len);
	}

	if (pipe->cur_idx == 0) {
		/* 空搬移：没有要启动的东西，仍然给一个已完成的 fence */
		return dma_fence_get_stub();
	}

	/* N4: 只启动，不在这里等；完成、清通道、invalidate、signal 全在 worker 里 */
	pipe_dma_start(pipe);

	fence = pipe_dma_fence_alloc(pipe);
	if (IS_ERR(fence)) {
		/* 分配失败也不能留下一个没人收的硬件任务：等它跑完再清通道 */
		if (!pipe_dma_wait(pipe, MWV207_DMA_POLL_TIMEOUT_MS))
			pr_err("mwv207: wait for completion of DMA timeout: 0x%x\n",
					pipe_dma_chan_read(pipe, 0, (0x188)));
		pipe_dma_clear_chan(pipe);
		return fence;
	}

	pipe->pending_cmd = *cmd;

	spin_lock_irq(&pipe->fence_lock);
	pipe->current_fence = fence;
	spin_unlock_irq(&pipe->fence_lock);

	queue_work(pipe->complete_wq, &pipe->complete_work);

	/* 管线自己持有一份引用（worker/reset 里 put），返回的这份给 drm_sched */
	return dma_fence_get(fence);
}

static void mwv207_pipe_dma_dump_state(struct mwv207_pipe *mpipe)
{
	struct mwv207_pipe_dma *pipe = to_dma_pipe(mpipe);
	pr_info("%s todo: %p", __func__, pipe);
}

static void mwv207_pipe_dma_destroy(struct mwv207_pipe *mpipe)
{
	struct mwv207_pipe_dma *pipe = to_dma_pipe(mpipe);

	/* 排空在飞 fence 与工作项，再拆工作队列（N4） */
	pipe_dma_force_complete(pipe);

	if (pipe->complete_wq) {
		/* force_complete 里已 cancel_work_sync，这里只负责销毁 */
		destroy_workqueue(pipe->complete_wq);
		pipe->complete_wq = NULL;
	}
}

struct mwv207_pipe *mwv207_pipe_dma_create(struct mwv207_device *jdev, int unit, u32 regbase,
		const char *fname)
{
	struct mwv207_pipe_dma *pipe;
	u64 llp;
	int i;

	pipe = devm_kzalloc(jdev->dev, sizeof(struct mwv207_pipe_dma), GFP_KERNEL);
	if (!pipe)
		return NULL;

	pipe->base.jdev = jdev;
	pipe->base.fname = fname;
	pipe->base.regbase = jdev->mmio + regbase;
	pipe->base.iosize = 16 * 1024;
	pipe->base.submit = mwv207_pipe_dma_submit;
	pipe->base.reset  = mwv207_pipe_dma_reset;
	pipe->base.destroy = mwv207_pipe_dma_destroy;
	pipe->base.dump_state = mwv207_pipe_dma_dump_state;

	/* current TTM impl uses ttm_dma_populate which is coherent.
	 * set to false if ttm_populate_and_map_pages is used
	 * */
	pipe->coherent = true;

	pipe->alignment = jdev->lite ? 32 : 64;
	pipe->unit = unit;

	/* N4: fence / 完成工作队列必须在下面那次 reset 之前就绪 */
	spin_lock_init(&pipe->fence_lock);
	pipe->fence_ctx = dma_fence_context_alloc(1);
	pipe->next_fence = 0;
	pipe->current_fence = NULL;
	INIT_WORK(&pipe->complete_work, pipe_dma_complete_work);

	pipe->complete_wq = alloc_ordered_workqueue("mwv207-dma-%d", WQ_MEM_RECLAIM, unit);
	if (!pipe->complete_wq) {
		dev_err(jdev->dev, "mwv207: failed to alloc dma workqueue for %s\n", fname);
		return NULL;
	}

	pipe->lli = dmam_alloc_coherent(jdev->dev,
			sizeof(struct mwv207_lli) * DMA_MAX_LLI_COUNT,
			&pipe->lli_bus_addr, GFP_KERNEL);
	if (!pipe->lli) {
		destroy_workqueue(pipe->complete_wq);
		pipe->complete_wq = NULL;
		return NULL;
	}

	memset(pipe->lli, 0, sizeof(struct mwv207_lli) * DMA_MAX_LLI_COUNT);
	llp = pipe->lli_bus_addr + 0x800000000ULL;
	for (i = 0; i < DMA_MAX_LLI_COUNT; i++) {
		llp += sizeof(struct mwv207_lli);
		pipe->lli[i].llp = llp;
	}
	pipe->cur_idx = 0;

	mwv207_pipe_dma_reset(&pipe->base);
	return &pipe->base;
}
