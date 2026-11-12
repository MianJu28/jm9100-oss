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

#include "mwv207_bo.h"
#include "mwv207_sched.h"
#include "mwv207_vbios.h"
#include "mwv207_devfreq.h"

#define DEFAULT_MAX_FREQ_2D 600

/*
 * 2D 引擎诊断（默认关闭）：提交前后各读一次完成序号 0x6000，并 dump
 * 这次命令描述符的前 32 个字。用来区分"引擎根本没执行命令"和"执行了
 * 但写到了别的地址"——这两种情况在用户态看起来都是目标缓冲没变。
 */
static int diag;
module_param(diag, int, 0644);
MODULE_PARM_DESC(diag, "2D 引擎诊断：提交后 dump 环内容与完成序号（默认 0）");

#define wait_for(COND, MS) ({                                           \
	unsigned long timeout__ = jiffies + msecs_to_jiffies(MS) + 1;   \
	int ret__ = 0;                                                  \
	while (!(COND)) {                                               \
		if (time_after(jiffies, timeout__)) {                   \
			if (!(COND))                                    \
				ret__ = -ETIMEDOUT;                     \
			break;                                          \
		}                                                       \
		msleep(1);                                              \
	}                                                               \
	ret__;                                                          \
})

struct mwv207_pipe_2d {
	struct mwv207_pipe base;
	struct mwv207_bo *ring_bo;
	struct mwv207_device *jdev;
	u32 *ring_start;
	u32 *ring_end;
	u32 *head;
	u32 *tail;
	u32 *last_wait;
	u64 ring_start_addr;
	int id;
	unsigned int irq;

	u64 fence_ctx;
	u32 next_fence;
	u32 completed_fence;
	spinlock_t fence_lock;
	spinlock_t fence_queue_lock;
	struct list_head fence_queue;
};

struct pipe_2d_fence {
	struct dma_fence base;
	struct mwv207_pipe_2d *pipe;
	u32 *rpos;

	struct list_head q;
};

#define to_pipe_2d(p) container_of(p, struct mwv207_pipe_2d, base)
#define to_pipe_2d_fence(f) container_of(f, struct pipe_2d_fence, base)

static inline void pipe_2d_write(struct mwv207_pipe_2d *pipe,
				 u32 reg, u32 value)
{
	pipe_write(&pipe->base, reg, value);
}

static inline u32 pipe_2d_read(struct mwv207_pipe_2d *pipe, u32 reg)
{
	return pipe_read(&pipe->base, reg);
}

static void pipe_2d_probe(struct mwv207_pipe_2d *pipe);

static inline void pipe_2d_barrier(struct mwv207_pipe_2d *pipe)
{
	pipe_2d_read(pipe, 0x4100);
	mb();
}

static inline bool fence16_after(u16 a, u16 b)
{
	return (s16)(a - b) > 0;
}

static inline bool fence_after(u32 a, u32 b)
{
	return (s32)(a - b) > 0;
}

static const char *pipe_2d_fence_get_driver_name(struct dma_fence *fence)
{
	return "mwv207";
}

static const char *pipe_2d_fence_get_timeline_name(struct dma_fence *fence)
{
	return to_pipe_2d_fence(fence)->pipe->base.fname;
}

static bool pipe_2d_fence_signaled(struct dma_fence *fence)
{
	struct pipe_2d_fence *f = to_pipe_2d_fence(fence);

	return fence_after(f->pipe->completed_fence, f->base.seqno);
}

static void pipe_2d_fence_release(struct dma_fence *fence)
{
	struct pipe_2d_fence *f = to_pipe_2d_fence(fence);

	kfree_rcu(f, base.rcu);
}

static const struct dma_fence_ops pipe_2d_fence_ops = {
	.get_driver_name = pipe_2d_fence_get_driver_name,
	.get_timeline_name = pipe_2d_fence_get_timeline_name,
	.signaled = pipe_2d_fence_signaled,
	.release = pipe_2d_fence_release,
};

static inline u32 pipe_2d_ptr_span(void *end, void *start)
{
	return (unsigned long)end - (unsigned long)start;
}

static inline u32 pipe_2d_gpu_addr(struct mwv207_pipe_2d *pipe, u32 *ptr)
{
	return pipe->ring_start_addr + pipe_2d_ptr_span(ptr, pipe->ring_start);
}

static inline bool pipe_2d_idle_on_noc(struct mwv207_pipe_2d *pipe)
{
	struct mwv207_device *jdev = pipe->jdev;
	u32 pending1, pending2;
	u32 val;

	pending1 = jdev->lite ? 0x70038000 : 0xF87FC000;
	pending2 = jdev->lite ? 0x00000000 : 0x0000000F;

	val = jdev_read(jdev, (0x009B0194));
	if ((val & pending1) != pending1)
		return false;
	val = jdev_read(jdev, (0x009B0198));
	if ((val & pending2) != pending2)
		return false;
	return true;
}

/*
 * 一次性探测（diag >= 4，默认关闭）：在创建阶段、进程上下文里做，可以
 * msleep。目的：找出"启动 2D 引擎"到底写哪个寄存器。把自包含的小命令
 * 缓冲写在环开头（header + 一条同步寄存器写 + NOP），再逐个候选寄存器
 * 写 1，等 50 ms 后读 0x6000 看同步值是否被写进去。
 */
static const u32 pipe_2d_trig_cand[] = {
	0x4C00, 0x4C24, 0x4C04, 0x4C20, 0x4024, 0x4224, 0x4800, 0x4300,
};

static void pipe_2d_probe(struct mwv207_pipe_2d *pipe)
{
	const u32 magic = 0x5a5a0001;
	u32 *buf = pipe->ring_start;
	unsigned int i;
	u32 self;

	buf[0] = 0x80000000;
	buf[1] = 3;			/* (64 / 16) - 1 */
	buf[2] = 0x40006000;
	buf[3] = magic;
	for (i = 4; i < 16; i++)
		buf[i] = 0x80000000;

	pipe_2d_write(pipe, 0x4200, 0xdead0001);
	pipe_2d_write(pipe, 0x4C00, 0xbeef0002);
	pr_info("mwv207: %s probe: 4200/4c00 alias? read 4200=%08x 4c00=%08x",
		pipe->base.fname, pipe_2d_read(pipe, 0x4200),
		pipe_2d_read(pipe, 0x4C00));

	pr_info("mwv207: %s probe: ring gpu=%llx", pipe->base.fname,
		(unsigned long long)pipe->ring_start_addr);

	for (i = 0; i < ARRAY_SIZE(pipe_2d_trig_cand); i++) {
		u32 seq;

		pipe_2d_write(pipe, 0x4200, pipe->ring_start_addr >> 4);
		pipe_2d_write(pipe, 0x4800, 0);
		pipe_2d_barrier(pipe);
		pipe_2d_write(pipe, pipe_2d_trig_cand[i], 1);
		pipe_2d_barrier(pipe);
		msleep(50);
		seq = pipe_2d_read(pipe, 0x6000);
		self = pipe_2d_read(pipe, pipe_2d_trig_cand[i]);
		pr_info("mwv207: %s probe: trig %04x=1 -> seq=%08x%s self=%08x",
			pipe->base.fname, pipe_2d_trig_cand[i], seq,
			seq == magic ? " [HIT]" : "", self);
	}
}

static int pipe_2d_hw_init(struct mwv207_pipe_2d *pipe)
{
	int ret;
	u32 val;

	ret = wait_for(pipe_2d_idle_on_noc(pipe), 5000);
	if (ret) {
		pr_err("mwv207: failed to wait %s idle, %#x %#x",
			pipe->base.fname,
			jdev_read(pipe->jdev, (0x009B0194)),
			jdev_read(pipe->jdev, (0x009B0198)));
		return ret;
	}

	val = jdev_read(pipe->jdev, (0x009B0048));
	val &= ~(1 << 8);
	jdev_write(pipe->jdev, (0x009B0048), val);

	val = jdev_read(pipe->jdev, (0x009B0014));
	val &= ~(1 << 16);
	jdev_write(pipe->jdev, (0x009B0014), val);

	val = jdev_read(pipe->jdev, (0x009B0048));
	val |= 1 << 8;
	jdev_write(pipe->jdev, (0x009B0048), val);

	val = jdev_read(pipe->jdev, (0x009B0014));
	val |= 1 << 16;
	jdev_write(pipe->jdev, (0x009B0014), val);

	msleep(10);

	pipe_2d_write(pipe, 0x4800, 0);

	ret = wait_for(pipe_2d_read(pipe, 0x4800), 5000);
	if (ret) {
		pr_err("mwv207: failed to reset %s status RAM",
			pipe->base.fname);
		return ret;
	}

	return 0;
}

/*
 * N14: reset/destroy 时在飞 fence 必须 signal 后再 put。
 * 旧代码只 dma_fence_put()，fence 永远停在未完成状态，任何持有它的
 * 等待者（dma_resv 的隐含 fence、sync_file、上层 TTM/客户端）会永久睡眠。
 * 先把队列摘到本地链表再在锁外 signal，避免在持有 fence_queue_lock 时
 * 跑 fence 回调（ISR 里是同一个队列，锁序保持一致）。
 */
static void pipe_2d_force_complete(struct mwv207_pipe_2d *pipe)
{
	struct pipe_2d_fence *fence, *p;
	LIST_HEAD(dead);
	unsigned long flags;

	spin_lock_irqsave(&pipe->fence_queue_lock, flags);
	list_splice_init(&pipe->fence_queue, &dead);
	spin_unlock_irqrestore(&pipe->fence_queue_lock, flags);

	list_for_each_entry_safe(fence, p, &dead, q) {
		list_del(&fence->q);
		dma_fence_set_error(&fence->base, -EIO);
		dma_fence_signal(&fence->base);
		dma_fence_put(&fence->base);
	}
}

static void pipe_2d_reset_ring(struct mwv207_pipe_2d *pipe)
{
	pipe->head = pipe->ring_start;
	pipe->tail = pipe->ring_start;
}

static u32 *pipe_2d_append_wl(struct mwv207_pipe_2d *pipe)
{
	u32 *wait = pipe->tail;

	*pipe->tail++ = 0x88000000;
	*pipe->tail++ = 0x80000000;
	*pipe->tail++ = 0x89000000;
	*pipe->tail++ = pipe_2d_gpu_addr(pipe, wait);;

	return wait;
}

static void pipe_2d_start(struct mwv207_pipe_2d *pipe)
{
	u32 regval = pipe->ring_start_addr >> 4 | 1 << 29;

	pipe_2d_write(pipe, 0x4200, regval);

	*pipe->tail++ = 0x80000000;
	*pipe->tail++ = 1;
	*pipe->tail++ = 0x80000000;
	*pipe->tail++ = 0x80000000;

	pipe->last_wait = pipe_2d_append_wl(pipe);
	/* 环形缓冲刚被重置：起点到 tail 必须正好 32 字节（8 条描述符）。
	 * 不成立说明 ring 状态已被破坏，此时继续写下去会越界，直接停在这里；
	 * 之后的 job 会由 2 s TDR 回收（N14 保证在飞 fence 会被 signal）。 */
	if (WARN_ON_ONCE(pipe_2d_ptr_span(pipe->tail, pipe->ring_start) != 32))
		return;
	pipe_2d_barrier(pipe);
	pipe_2d_write(pipe, 0x4C00, 1);

	dev_dbg(pipe->base.jdev->dev, "start 2d wait link at 0x%llx",
		pipe->ring_start_addr);
}

static inline void pipe_2d_stop(struct mwv207_pipe_2d *pipe)
{
	pr_info("mwv207: %s %s TBD", pipe->base.fname, __func__);
}

static void mwv207_pipe_2d_reset(struct mwv207_pipe *mpipe)
{
	struct mwv207_pipe_2d *pipe = to_pipe_2d(mpipe);

	/*
	 * N14: 无论下面 hw_init() 结果如何（失败会 early return，硬件可能
	 * 复位不掉），在飞 fence 都必须先完成，否则等待者永远无法恢复。
	 */
	pipe_2d_force_complete(pipe);

	if (pipe_2d_hw_init(pipe))
		return;
	pipe_2d_reset_ring(pipe);
	pipe_2d_start(pipe);
}

static u32 *pipe_2d_wait_for_space(struct mwv207_pipe_2d *pipe, u32 size)
{
	int i;

	for (i = 0; i < 10000; i++) {
		u32 *head = READ_ONCE(pipe->head);
		if (head <= pipe->tail) {
			if (pipe_2d_ptr_span(pipe->ring_end, pipe->tail) >= size)
				return pipe->tail;
			if (pipe_2d_ptr_span(head, pipe->ring_start) >= size)
				return pipe->ring_start;
		} else {
			if (pipe_2d_ptr_span(head, pipe->tail) >= size)
				return pipe->tail;
		}
		usleep_range(200, 200);
	}

	return NULL;
}

static struct dma_fence *pipe_2d_append_fence(struct mwv207_pipe_2d *pipe)
{
	struct pipe_2d_fence *fence;
	unsigned long flags;
	u32 regno;

	fence = kzalloc(sizeof(struct pipe_2d_fence), GFP_KERNEL);
	if (!fence) {
		pr_err("mwv207: failed to alloc memory for %s",
			pipe->base.fname);
		return ERR_PTR(-ENOMEM);
	}
	dma_fence_init(&fence->base, &pipe_2d_fence_ops, &pipe->fence_lock,
		       pipe->fence_ctx, ++pipe->next_fence);
	fence->pipe  = pipe;
	fence->rpos  = pipe->tail;
	regno = (pipe->next_fence & 0x1f) + 1;

	spin_lock_irqsave(&pipe->fence_queue_lock, flags);
	list_add_tail(&fence->q, &pipe->fence_queue);
	spin_unlock_irqrestore(&pipe->fence_queue_lock, flags);

	*pipe->tail++ = (0x40000000 | 0x6000 | (0) << 2);
	*pipe->tail++ = (u32)(fence->base.seqno & 0xffff);
	*pipe->tail++ = (0x40000000 | 0x6000 | (regno) << 2);
	*pipe->tail++ = regno;

	pipe_2d_write(pipe, 0x6000 + regno * 4, regno);

	dma_fence_get(&fence->base);
	return &fence->base;
}

static void pipe_2d_wtol(struct mwv207_pipe_2d *pipe, u32 *start, u32 *new_wait)
{
	u32 *pos = pipe->last_wait;

	pos[1] = pipe_2d_gpu_addr(pipe, start);
	pipe_2d_barrier(pipe);
	pos[0] = 0x89000000;
	pipe_2d_barrier(pipe);

	pipe->last_wait = new_wait;
}

static void mwv207_pipe_2d_dump_state(struct mwv207_pipe *mpipe)
{
	pr_info("%s %08x: %08x, %08x: %08x", mpipe->fname,
		0x4100, pipe_read(mpipe, 0x4100),
		0x961100, jdev_read(mpipe->jdev, 0x961100));
}

static struct dma_fence *
mwv207_pipe_2d_submit(struct mwv207_pipe *mpipe, struct mwv207_job *mjob)
{
	struct mwv207_pipe_2d *pipe = to_pipe_2d(mpipe);
	u32 size = ALIGN(mjob->cmd_size, 16) + 128;
	u32 *start, *wait, *last_tail, nop_cnt;
	struct dma_fence *fence;

	start = pipe_2d_wait_for_space(pipe, size);
	if (!start) {
		pr_err("mwv207: %s is stuck", mpipe->fname);
		return ERR_PTR(-EBUSY);
	}
	last_tail = pipe->tail;
	pipe->tail = start;

	*pipe->tail++ = 0x80000000;
	pipe->tail++;
	*pipe->tail++ = 0x80000000;
	*pipe->tail++ = 0x80000000;

	memcpy_toio(pipe->tail, mjob->cmds, mjob->cmd_size);
	pipe->tail += mjob->cmd_size >> 2;
	for (nop_cnt = (ALIGN(mjob->cmd_size, 16) - mjob->cmd_size);
	     nop_cnt > 0; nop_cnt -= 4)
		*pipe->tail++ = 0x80000000;

	fence = pipe_2d_append_fence(pipe);
	if (IS_ERR(fence)) {
		pipe->tail = last_tail;
		return fence;
	}

	wait = pipe_2d_append_wl(pipe);

	/* 描述符长度以 16 字节为单位写回 start[1]，因此跨度必须 16 对齐、
	 * 且不能超过预留空间 size。两者都是内部不变量（size 来自用户提交
	 * 的 cmd_size 向上取整），破坏时不能让内核 panic：
	 * 回滚 tail、作废这次提交，由调度器把 job 标成失败。 */
	if (WARN_ON_ONCE((pipe_2d_ptr_span(pipe->tail, start) & 0xf) ||
			 pipe_2d_ptr_span(pipe->tail, start) > size)) {
		/* tail 不回滚：描述符确实已经写进环形缓冲，保持记账一致。
		 * fence 已入队，由 ISR/reset 统一回收；本次提交报错交给上层。 */
		return ERR_PTR(-EIO);
	}
	start[1] = pipe_2d_ptr_span(pipe->tail, start) / 16 - 1;

	{
		u32 i, n;

		if (diag) {
			u32 seq_pre = pipe_2d_read(pipe, 0x6000);
			char buf[128];

			pr_info("mwv207: %s diag arm: start=%px gpu=%llx seqno=%08x nwords=%u",
				mpipe->fname, start,
				(unsigned long long)pipe_2d_gpu_addr(pipe, start),
				seq_pre, (u32)(pipe->tail - start));

			n = (u32)(pipe->tail - start);
			for (i = 0; i < n && i < 32; i += 8) {
				snprintf(buf, sizeof(buf), "%08x %08x %08x %08x %08x %08x %08x %08x",
					 start[i + 0], start[i + 1], start[i + 2], start[i + 3],
					 start[i + 4], start[i + 5], start[i + 6], start[i + 7]);
				pr_info("mwv207: %s diag w[%u] %s", mpipe->fname, i, buf);
			}
		}

		if (diag >= 2) {
			/* 单缓冲模式：CMDADDR 直接指本描述符、MODE=RINGBUFFER，
			 * 不经过 wait-link 链，用来区分"链坏了"与"引擎没启动"。 */
			pipe_2d_barrier(pipe);
			pipe_2d_write(pipe, 0x4200,
				      pipe_2d_gpu_addr(pipe, start) >> 4);
			pipe_2d_write(pipe, 0x4C00, 1);
		} else {
			pipe_2d_wtol(pipe, start, wait);
		}

		if (diag)
			pr_info("mwv207: %s diag armed: seqno=%08x cmdaddr=%08x trig=%08x stat=%08x noc=%08x/%08x",
				mpipe->fname, pipe_2d_read(pipe, 0x6000),
				pipe_2d_read(pipe, 0x4200), pipe_2d_read(pipe, 0x4C00),
				pipe_2d_read(pipe, 0x4800),
				jdev_read(mpipe->jdev, 0x9B0194),
				jdev_read(mpipe->jdev, 0x9B0198));
	}

	return fence;
}

static irqreturn_t mwv207_pipe_2d_isr(int irq_unused, void *data)
{
	struct mwv207_pipe_2d *pipe = data;
	struct pipe_2d_fence *fence, *p;
	u32 seqno;

	pipe_2d_write(pipe, 0x4308, 0xFFFFFFFF);
	seqno = pipe_2d_read(pipe, 0x6000);
	if (diag)
		pr_info("mwv207: %s diag isr seqno=%08x", pipe->base.fname, seqno);

	spin_lock(&pipe->fence_queue_lock);
	list_for_each_entry_safe(fence, p, &pipe->fence_queue, q) {
		if (fence16_after(fence->base.seqno & 0xffff, seqno & 0xffff))
			break;
		list_del(&fence->q);
		pipe->completed_fence = fence->base.seqno;
		WRITE_ONCE(pipe->head, fence->rpos);
		mwv207_devfreq_record_idle(&pipe->base);
		dma_fence_signal(&fence->base);
		dma_fence_put(&fence->base);
	}
	spin_unlock(&pipe->fence_queue_lock);

	return IRQ_HANDLED;
}

static void mwv207_pipe_2d_destroy(struct mwv207_pipe *mpipe)
{
	struct mwv207_pipe_2d *pipe = to_pipe_2d(mpipe);
	struct mwv207_bo *mbo = pipe->ring_bo;
	int ret;

	free_irq(pipe->irq, pipe);
	pipe_2d_stop(pipe);

	/*
	 * N14: free_irq() 之后不会再有 ISR 推进 fence，残留节点必须在这里
	 * 以 -EIO 完成，否则卸载后这些 fence 的等待者永远醒不过来。
	 */
	pipe_2d_force_complete(pipe);

	ret = mwv207_bo_reserve(mbo, true);
	if (ret) {
		pr_err("mwv207: failed to reserve bo");
		return;
	}
	mwv207_bo_kunmap_reserved(mbo);
	mwv207_bo_unpin_reserved(mbo);
	mwv207_bo_unreserve(mbo);
	mwv207_bo_unref(mbo);

	mwv207_devfreq_unregister(mpipe);
}

static unsigned long pipe_2d_max_freq_get(struct mwv207_device *jdev)
{
	unsigned long  freq;

	switch (jdev->pdev->subsystem_device)
	{
	case 0x9103:
		freq = 600;
		break;
	case 0x9210:
	case 0x9211:
		freq = 800;
		break;
	case 0x930B:
		freq = 1000;
		break;
	case 0x9101:
	case 0x9102:
	case 0x910A:
	case 0x910B:
	case 0x910C:
	case 0x9200:
	case 0x920A:
	case 0x920B:
	case 0x920C:
	case 0x9230:
	case 0x9231:
	case 0x930A:
	case 0x930C:
		freq = 1200;
		break;
	default:
		freq = DEFAULT_MAX_FREQ_2D;
		break;
	}

	return freq;
}

struct mwv207_pipe *mwv207_pipe_2d_create(struct mwv207_device *jdev,
					  int id, u32 regbase,
					  const char *fname)
{
	struct mwv207_pipe_2d *pipe;
	struct mwv207_bo *mbo;
	void *addr;
	int ret;

	pipe = devm_kzalloc(jdev->dev, sizeof(*pipe), GFP_KERNEL);
	if (!pipe)
		return NULL;
	pipe->base.pll_id = MWV207_PLL_GU2D_PLL;
	pipe->base.jdev = jdev;
	pipe->base.fname = fname;
	pipe->base.regbase = jdev->mmio + regbase;
	pipe->base.iosize  = 32 * 1024;
	pipe->base.submit = mwv207_pipe_2d_submit;
	pipe->base.reset  = mwv207_pipe_2d_reset;
	pipe->base.destroy = mwv207_pipe_2d_destroy;
	pipe->base.dump_state = mwv207_pipe_2d_dump_state;

	pipe->jdev = jdev;
	pipe->id = id;
	pipe->fence_ctx = dma_fence_context_alloc(1);
	INIT_LIST_HEAD(&pipe->fence_queue);
	spin_lock_init(&pipe->fence_lock);
	spin_lock_init(&pipe->fence_queue_lock);

	ret = mwv207_devfreq_register(&pipe->base, pipe_2d_max_freq_get(jdev));
	if (ret)
		pipe->base.devfreq = NULL;

	ret = mwv207_bo_create(jdev, 0x40000, 0x1000,
			       ttm_bo_type_kernel, 0x2,
			       (1<<0), &mbo);
	if (ret)
		goto unregister_devfreq;

	ret = mwv207_bo_reserve(mbo, true);
	if (ret)
		goto free_bo;
	ret = mwv207_bo_pin_reserved(mbo, 0x2);
	if (ret)
		goto unreserve_bo;
	ret = mwv207_bo_kmap_reserved(mbo, &addr);
	if (ret)
		goto unpin_bo;
	pipe->ring_bo = mbo;

	pipe->ring_start = (u32 *)addr;
	pipe->ring_start_addr = mwv207_bo_gpu_phys(mbo);
	pipe->ring_end = pipe->ring_start + (0x40000 >> 2);

	mwv207_pipe_2d_reset(&pipe->base);
	if (diag >= 4)
		pipe_2d_probe(pipe);

	pipe->irq = irq_find_mapping(jdev->irq_domain, 17 + id);
	if (pipe->irq == 0) {
		pr_err("mwv207: failed to find %s irq mapping", fname);
		goto unmap_bo;
	}

	ret = request_irq(pipe->irq, mwv207_pipe_2d_isr, 0, fname, pipe);
	if (ret) {
		pr_err("mwv207: failed to request irq for %s", fname);
		goto unmap_bo;
	}

	mwv207_bo_unreserve(mbo);

	return &pipe->base;
unmap_bo:
	mwv207_bo_kunmap_reserved(mbo);
unpin_bo:
	mwv207_bo_unpin_reserved(mbo);
unreserve_bo:
	mwv207_bo_unreserve(mbo);
free_bo:
	mwv207_bo_unref(mbo);
unregister_devfreq:
	mwv207_devfreq_unregister(&pipe->base);
	return NULL;
}
