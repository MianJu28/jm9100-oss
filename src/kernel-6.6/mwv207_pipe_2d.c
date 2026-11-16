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
	struct mwv207_bo *pt_bo;
	struct mwv207_device *jdev;
	u32 *ring_start;
	void __iomem *ring_mmio;	/* ring 的 Device 映射，用来写 wait link */
	u32 *ring_end;
	u32 *head;
	u32 *tail;
	u32 *last_wait;
	u64 ring_start_addr;
	int id;
	unsigned int irq;

	bool cold_reset_done;
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
	u32 sync_reg;
	u32 sync_val;

	struct list_head q;
};

#define PIPE_2D_SYNC_REG_BASE	0x6004
#define PIPE_2D_SYNC_REG_NR	16

static u32 pipe_2d_sync_val(u32 seqno)
{
	return (((seqno & 0xffff) ^ 0x5555) | 0x1) & 0xffff;
}


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

/*
 * 踢 2D 引擎（GROUNDWAVE = 1）。
 *
 * pipe_write() 是 writel_relaxed()，不保证这一下写排在之前对显存的所有
 * 写之后（link 的两个字、链内容都在显存里）；厂商 engine_write_reg() 用的
 * 是带屏障的 writel()。这里改成 writel() 并回读 0x4100（AD_STATUS）强制
 * 事务在总线上完成，确保引擎被踢时能看到已经改好的 wait/link 描述符。
 */
static inline void pipe_2d_kick(struct mwv207_pipe_2d *pipe)
{
	writel(1, pipe->base.regbase + 0x4C00);
	(void)readl(pipe->base.regbase + 0x4100);
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
	buf[2] = 0x40000000 | PIPE_2D_SYNC_REG_BASE;
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
		seq = pipe_2d_read(pipe, PIPE_2D_SYNC_REG_BASE);
		self = pipe_2d_read(pipe, pipe_2d_trig_cand[i]);
		pr_info("mwv207: %s probe: trig %04x=1 -> seq=%08x%s self=%08x",
			pipe->base.fname, pipe_2d_trig_cand[i], seq,
			seq == magic ? " [HIT]" : "", self);
	}
}

/*
 * 2D 引擎有自己的 MMU（厂商 jmgpu_define.c 的 j9_handle_prebendate() 与
 * j9_handle_trinketing()），命令流和算图数据都必须经过它。没有这一步，
 * 引擎取不到命令：表现正是"提交成功、sync 寄存器永远不写、2 s 后 TDR"。
 *
 * 页表是两级结构：
 *   主表 4 KB（1024 项，每项指向一张从表、覆盖 4 MB）
 *   从表 4 KB（1024 项 × 4 KB 页）
 *   主表项 = 从表物理地址(31:6) | 1
 *   从表项 = 物理地址(31:12) | 0x85  （valid | 0x80 | 0x4，见厂商原始位域）
 *   0x9E83B4 = (主表地址 >> 12) << 4（MODE=4K、SHARABLE=0、SECURE=0）
 *   0x9E8388 = ENABLE | OUTOFBOUND_EXC | SETUP_FROM_REG = 0x25
 *
 * 我们按"虚拟地址 == VRAM 偏移"恒等映射整个 VRAM，这样 BO 的 gpu_phys
 * 可以直接当 2D 命令里的地址用，不需要额外的 VA 分配器。
 */
#define PIPE_2D_MMU_PD_REG	0x9E83B4
#define PIPE_2D_MMU_CTRL_REG	0x9E8388
#define PIPE_2D_MMU_PTE_FLAGS	0x85
#define PIPE_2D_MMU_COVER	(1024ULL * 4096)

static int pipe_2d_mmu_init(struct mwv207_pipe_2d *pipe)
{
	struct mwv207_device *jdev = pipe->jdev;
	struct mwv207_bo *ptbo;
	u32 slaves, pt_size, i, j, *pt, *stlb;
	u64 gpu;
	int ret;

	/* 备用路径：恒等映射 VRAM（VA == VRAM 偏移）。waitlink 模式不会
	 * 使能这套页表，见 mwv207_pipe_2d_reset() 里的说明。 */
	slaves = (u32)DIV_ROUND_UP(jdev->vram_size, PIPE_2D_MMU_COVER);
	if (!slaves)
		slaves = 1;
	pt_size = 4096 + slaves * 4096;

	ret = mwv207_bo_create(jdev, pt_size, 0x1000, ttm_bo_type_kernel,
			       0x2, (1 << 0), &ptbo);
	if (ret) {
		pr_err("mwv207: %s failed to alloc page table", pipe->base.fname);
		return ret;
	}
	ret = mwv207_bo_reserve(ptbo, true);
	if (ret)
		goto free_bo;
	ret = mwv207_bo_pin_reserved(ptbo, 0x2);
	if (ret)
		goto unreserve_bo;
	ret = mwv207_bo_kmap_reserved(ptbo, (void **)&pt);
	if (ret)
		goto unpin_bo;

	gpu = mwv207_bo_gpu_phys(ptbo);

	/* 主表：每项指向一张 4 KB 从表，覆盖 4 MB */
	for (i = 0; i < 1024; i++)
		pt[i] = 0;
	for (i = 0; i < slaves; i++) {
		u32 addr = (u32)gpu + 4096 + i * 4096;

		pt[i] = (addr & ~0x3fU) | 1;
	}

	/* 从表：恒等映射 VRAM 偏移 */
	stlb = pt + 1024;
	for (i = 0; i < slaves; i++)
		for (j = 0; j < 1024; j++)
			stlb[i * 1024 + j] =
				((i * 1024 + j) * 4096) | PIPE_2D_MMU_PTE_FLAGS;

	pipe->pt_bo = ptbo;
	pr_info("mwv207: %s 2D MMU 页表已建(未使能): pt@0x%llx %u 张从表, 恒等映射 %llu MB",
		pipe->base.fname, (unsigned long long)gpu, slaves,
		(unsigned long long)(jdev->vram_size >> 20));
	return 0;

unpin_bo:
	mwv207_bo_unpin_reserved(ptbo);
unreserve_bo:
	mwv207_bo_unreserve(ptbo);
free_bo:
	mwv207_bo_unref(ptbo);
	return ret;
}

/*
 * 2D 硬件的软复位（pipe_2d_hw_init 里的 0x9B0048/0x9B0014）会把 2D 私有
 * MMU 的 PD/CTRL 清零，所以使能必须放在每次 HW_INIT 之后。厂商
 * j9_incorporality() 的顺序也是「复位 → 写页表 → j9_handle_trinketing()
 * 使能」；旧代码只在 create() 里写一次、紧接着被 reset 清掉，于是引擎
 * 从头到尾都取不到命令（j2dtest 永远 poll=0、2s 后 TDR）。
 */
static void pipe_2d_mmu_enable(struct mwv207_pipe_2d *pipe)
{
	struct mwv207_device *jdev = pipe->jdev;
	u32 gpu;

	if (!pipe->pt_bo)
		return;
	gpu = (u32)mwv207_bo_gpu_phys(pipe->pt_bo);

	mb();
	jdev_write(jdev, PIPE_2D_MMU_PD_REG, (gpu >> 12) << 4);
	mb();
	jdev_write(jdev, PIPE_2D_MMU_CTRL_REG, 0x25);
	msleep(1);
	pr_info("mwv207: %s 2D MMU 已使能: PD=0x%08x CTRL=0x%08x",
		pipe->base.fname, jdev_read(jdev, PIPE_2D_MMU_PD_REG),
		jdev_read(jdev, PIPE_2D_MMU_CTRL_REG));
}

static int pipe_2d_hw_init(struct mwv207_pipe_2d *pipe)
{
	int ret;
	u32 val;

	/*
	 * 厂商 j9_unexpressively() 在清/置 NOC 与 GU2D 复位位之前，先把 2D 的
	 * 命令取指切到 AHB（0x96C200 = FROM_AHB），让引擎停止从内存取流，再等
	 * NOC 静默。缺这一步时软复位不完整：引擎把启动链跑完后会停在 wait 上，
	 * 之后驱动 patch link + 写 0x4C00 都唤不醒它（手工实验里只有刚被 TDR
	 * 复位过的短暂窗口能唤醒）。冷复位只在 probe（X 尚未起来）时做一次，
	 * 避免运行期切换取指路径影响正在扫描出的显示。
	 */
	if (!pipe->cold_reset_done) {
		pipe_2d_write(pipe, 0x96C200, 0x80000000);
		pipe->cold_reset_done = true;
	}

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

/*
 * 尾部描述符，与厂商 jmu_wl_tail 的 wait 形态一致：
 *   { 0x88000000 (wait), 0x80000000 (nop), 0x80000000 (nop), 0x80000000 (nop) }
 * 引擎执行到这里会挂住轮询；下一次提交时由 pipe_2d_wtol() 就地改写成
 *   { 0x89000000 (link), 下一段 gpu 地址, ... }
 * 引擎随之跳到下一段继续执行。旧实现多写了两个字（0x89000000 + 自指
 * 地址），与厂商布局不符，已按厂商形态收敛。
 */
static u32 *pipe_2d_append_wl(struct mwv207_pipe_2d *pipe)
{
	u32 *wait = pipe->tail;

	*pipe->tail++ = 0x88000000;
	*pipe->tail++ = 0x80000000;
	*pipe->tail++ = 0x80000000;
	*pipe->tail++ = 0x80000000;

	return wait;
}

/*
 * 启动等待链，逐字对齐厂商 j9_nipas()（jmgpu_accessible.c）：
 *
 *   header { nop, byte16_cnt, nop, nop }
 *   write 0x40000000|sync_reg, sync_val      <- 让引擎能回写完成值
 *   tail   { 0x88000000, nop, nop, nop }     <- 挂住，等下一次 link
 *
 * 然后写 0x4200 = MODE_WAITLINK(bit29) | FROM_MEM(bit31=0) | cmdaddr>>4，
 * 再写 0x4c00 = 1（GROUNDWAVE）踢引擎。厂商在这里是 12 个字 / 48 字节，
 * byte16_cnt = 2。
 */
static void pipe_2d_start(struct mwv207_pipe_2d *pipe)
{
	u32 *p = pipe->tail;
	u32 regval;

	*p++ = 0x80000000;
	*p++ = 2;			/* 48 字节 / 16 - 1 */
	*p++ = 0x80000000;
	*p++ = 0x80000000;
	*p++ = 0x40000000 | PIPE_2D_SYNC_REG_BASE;
	*p++ = pipe_2d_sync_val(0);
	/* 补两个 NOP 把 sync 段抬到 16 字节边界：厂商 j9_nipas() 在这里写
	 * sync(8B) + 2×NOP(8B)，整条链才是 12 字 = 48 字节、byte16_cnt = 2。
	 * 少了这两个字链只有 40 字节，start() 的 48 字节断言会命中 WARN 并
	 * 直接 return，于是 0x4200/0x4C00 永远不写、引擎从不启动。 */
	*p++ = 0x80000000;
	*p++ = 0x80000000;
	pipe->last_wait = p;
	*p++ = 0x88000000;
	*p++ = 0x80000000;
	*p++ = 0x80000000;
	*p++ = 0x80000000;
	pipe->tail = p;

	/* 环形缓冲刚被重置：起点到 tail 必须正好 48 字节（12 个字）。
	 * 不成立说明 ring 状态已被破坏，此时继续写下去会越界，直接停在这里；
	 * 之后的 job 会由 2 s TDR 回收（N14 保证在飞 fence 会被 signal）。 */
	if (WARN_ON_ONCE(pipe_2d_ptr_span(pipe->tail, pipe->ring_start) != 48))
		return;

	regval = pipe->ring_start_addr >> 4 | 1 << 29;
	pipe_2d_barrier(pipe);
	pipe_2d_write(pipe, 0x4200, regval);
	pipe_2d_barrier(pipe);
	pipe_2d_kick(pipe);

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
	/*
	 * 不使能 2D 私有 MMU：厂商 waitlink 模式（j2dmode=1，默认）从不用它，
	 * 只有 ringbuffer 模式才通过 j9_handle_prebendate() 建表并使能
	 * （日志 "cmd mmu(@0x3b4)"）。命令行里的面地址是 VRAM 偏移，由引擎
	 * 数据通路直接解释；若把 MMU 打开并恒等映射，取指地址会被再翻译一次，
	 * 引擎读到的就是垃圾。
	 */
	pipe_2d_reset_ring(pipe);
	pipe_2d_start(pipe);
}

/*
 * 硬件要求 link 的目标（以及链的起始地址）落在 4 KB 边界上：手工实验里
 * 把 wait 改成链接到 0x40600/0x40500 这类 16 字节对齐但非 4K 对齐的链，
 * 引擎一律静默不执行；改成 0x41000（4K 对齐）后立刻执行完整条链。所以
 * 每条提交都放进一个 4K 槽，槽基址就是 link 目标。
 */
#define PIPE_2D_SLOT_SIZE	0x1000u
#define PIPE_2D_SLOT_WORDS	(PIPE_2D_SLOT_SIZE / 4)

static u32 *pipe_2d_slot_align(struct mwv207_pipe_2d *pipe, u32 *pos, u32 size)
{
	u32 off = (u32)((u8 *)pos - (u8 *)pipe->ring_start);
	u32 pad = (PIPE_2D_SLOT_SIZE - (off & (PIPE_2D_SLOT_SIZE - 1))) &
		  (PIPE_2D_SLOT_SIZE - 1);

	if (pipe_2d_ptr_span(pipe->ring_end, pos) < pad + size)
		return NULL;

	return pos + pad / 4;
}

static u32 *pipe_2d_wait_for_space(struct mwv207_pipe_2d *pipe, u32 size)
{
	int i;

	if (size > PIPE_2D_SLOT_SIZE)
		return NULL;

	for (i = 0; i < 10000; i++) {
		u32 *head = READ_ONCE(pipe->head);
		u32 *pos = NULL;

		if (head <= pipe->tail) {
			pos = pipe_2d_slot_align(pipe, pipe->tail, size);
			if (pos)
				return pos;
			if (pipe_2d_ptr_span(head, pipe->ring_start) >=
			    PIPE_2D_SLOT_SIZE + size)
				return pipe->ring_start;
		} else {
			pos = pipe_2d_slot_align(pipe, pipe->tail, size);
			if (pos)
				return pos;
		}
		usleep_range(200, 200);
	}

	return NULL;
}

/*
 * 完成信号与厂商 waitlink 引擎一致（jmgpu_accessible.c 的 j9_gelatinizable /
 * j9_disavowedly）：
 *
 *   命令流末尾追加一条 "写寄存器" 命令  0x40000000 | sync_reg, sync_val
 *   引擎执行到这条命令时把 sync_val 写进 sync_reg，内核读回来比对，相等
 *   即这一帧已完成。sync_val 先按 J9_METERSTICK 变换（^0x5555 | 1，取低
 *   16 位），保证 0 不会被当成"未提交"，也避免 0 初值误判为完成。
 *
 * 旧代码把 seqno 裸写进 0x6000 并用 16 位回绕比较，引擎不执行时会误判，
 * 执行了又依赖 0x6000 的共享槽；现在每个 fence 占一个独立 sync 寄存器，
 * 由 IRQ 里逐个比对，语义与厂商一致。
 */
static struct dma_fence *pipe_2d_append_fence(struct mwv207_pipe_2d *pipe)
{
	struct pipe_2d_fence *fence;
	unsigned long flags;

	fence = kzalloc(sizeof(struct pipe_2d_fence), GFP_KERNEL);
	if (!fence) {
		pr_err("mwv207: failed to alloc memory for %s",
			pipe->base.fname);
		return ERR_PTR(-ENOMEM);
	}
	dma_fence_init(&fence->base, &pipe_2d_fence_ops, &pipe->fence_lock,
		       pipe->fence_ctx, ++pipe->next_fence);
	fence->pipe = pipe;
	fence->rpos = pipe->tail;
	fence->sync_reg = PIPE_2D_SYNC_REG_BASE +
			  (fence->base.seqno & (PIPE_2D_SYNC_REG_NR - 1)) * 4;
	fence->sync_val = pipe_2d_sync_val(fence->base.seqno);

	spin_lock_irqsave(&pipe->fence_queue_lock, flags);
	list_add_tail(&fence->q, &pipe->fence_queue);
	spin_unlock_irqrestore(&pipe->fence_queue_lock, flags);

	*pipe->tail++ = 0x40000000 | fence->sync_reg;
	*pipe->tail++ = fence->sync_val;
	/*
	 * 同步写命令只有 8 字节，后面必须补两个 NOP 才能让整段（header +
	 * payload + sync + wait 尾）保持 16 字节对齐——厂商 j9_gelatinizable()
	 * 也是先 sync 写、再按 16 对齐补 J9_EMBASSADRESS，然后才放 wait 尾。
	 * 少这两个字会让 header.byte16_cnt 与真实长度不符，引擎把 wait 尾当
	 * 载荷的一部分解析。
	 */
	*pipe->tail++ = 0x80000000;
	*pipe->tail++ = 0x80000000;

	dma_fence_get(&fence->base);
	return &fence->base;
}

/*
 * 把上一段末尾的 wait 就地改写成 link 指向本段，然后踢引擎。
 *
 * 厂商 j9_gelatinizable() 的顺序：先写 link.addr、barrier、再写 link.link，
 * 最后 engine_write_reg(GROUNDWAVE=0x4c00, 1)。**每一次提交都要踢一次**，
 * 只改 link 不踢是旧实现最大的问题：引擎在 wait 上停住后不会自己醒。
 */
static void pipe_2d_wtol(struct mwv207_pipe_2d *pipe, u32 *start, u32 *new_wait)
{
	u32 *pos = pipe->last_wait;
	u32 off = (u32)((u8 *)pos - (u8 *)pipe->ring_start);

	/*
	 * link 的两个字必须真到内存之后才能踢引擎，否则引擎读到的还是
	 * 0x88000000（wait），会继续原地等下一次世代。走 Device 映射写，
	 * 顺序由 nGnRnE 保证；映射不可用时退回 CPU 直写 + barrier。
	 */
	if (pipe->ring_mmio) {
		writel(pipe_2d_gpu_addr(pipe, start), pipe->ring_mmio + off + 4);
		wmb();
		writel(0x89000000, pipe->ring_mmio + off);
		wmb();
	} else {
		pos[1] = pipe_2d_gpu_addr(pipe, start);
		pipe_2d_barrier(pipe);
		pos[0] = 0x89000000;
		pipe_2d_barrier(pipe);
	}

	pipe->last_wait = new_wait;

	if (diag)
		pr_info("mwv207: %s wtol off=%#x gpu=%#x mmio=%d cpu=%08x/%08x mmio_rd=%08x/%08x",
			pipe->base.fname, off, pipe_2d_gpu_addr(pipe, start),
			pipe->ring_mmio != NULL, pos[0], pos[1],
			pipe->ring_mmio ? readl(pipe->ring_mmio + off) : 0,
			pipe->ring_mmio ? readl(pipe->ring_mmio + off + 4) : 0);

	pipe_2d_kick(pipe);

	if (diag)
		pr_info("mwv207: %s kick: globals=%08x adstat=%08x sync=%08x",
			pipe->base.fname, pipe_2d_read(pipe, 0x4200),
			pipe_2d_read(pipe, 0x4100),
			pipe_2d_read(pipe, PIPE_2D_SYNC_REG_BASE));
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
	u32 *start, *wait, *last_tail, *payload, nop_cnt, i;
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

	payload = pipe->tail;
	memcpy_toio(pipe->tail, mjob->cmds, mjob->cmd_size);
	pipe->tail += mjob->cmd_size >> 2;

	/* mjob->cmds 是 char *（字节缓冲），必须按 32 位字解释。用户命令流
	 * 通常以 0x81000000（END）结尾，2D 引擎执行到 END 就停止，其后的
	 * fence sync 写与 wait 尾永远不会执行，于是 fence 永不完成、提交按
	 * 超时失败（j2dtest 全零的根因）。把尾部 END 换成 NOP，让控制流落到
	 * fence sync 上；尾部的零填字一并跳过。 */
	{
		const u32 *cmds = (const u32 *)mjob->cmds;

		for (i = mjob->cmd_size >> 2; i > 0; i--) {
			u32 w = cmds[i - 1];

			if (w == 0x81000000u) {
				WRITE_ONCE(payload[i - 1], 0x80000000u);
				break;
			}
			if (w != 0x00000000u)
				break;
		}
	}

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

	/*
	 * 链内容先写在 ring 的 write-combined 直映射里（memcpy_toio/普通 store），
	 * 这些写进入写合并缓冲后并不保证在随后的 MMIO 踢之前对设备可见——引擎
	 * 会读到半条链（表现为执行到某条命令就停住、fence 永不完成）。这里在
	 * 武装之前把这条链再经 Device 映射（pipe->ring_mmio，nGnRnE）逐字重写
	 * 一遍：强序、立即落到显存，之后紧接的 link patch 与踢才一定看到完整链。
	 */
	if (pipe->ring_mmio) {
		u32 off = (u32)((u8 *)start - (u8 *)pipe->ring_start);
		u32 n = (u32)(pipe->tail - start);
		u32 k;

		for (k = 0; k < n; k++)
			writel(start[k], pipe->ring_mmio + off + k * 4);
		wmb();
	}

	{
		u32 i, n;

		if (diag) {
			u32 seq_pre = pipe_2d_read(pipe, PIPE_2D_SYNC_REG_BASE);
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
			pipe_2d_kick(pipe);
		} else {
			pipe_2d_wtol(pipe, start, wait);
		}

		if (diag)
			pr_info("mwv207: %s diag armed: seqno=%08x cmdaddr=%08x trig=%08x stat=%08x noc=%08x/%08x",
				mpipe->fname, pipe_2d_read(pipe, PIPE_2D_SYNC_REG_BASE),
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

	pipe_2d_write(pipe, 0x4308, 0xFFFFFFFF);

	spin_lock(&pipe->fence_queue_lock);
	list_for_each_entry_safe(fence, p, &pipe->fence_queue, q) {
		u32 val = pipe_2d_read(pipe, fence->sync_reg) & 0xffff;

		if (diag)
			pr_info("mwv207: %s diag isr reg=%04x val=%08x want=%08x",
				pipe->base.fname, fence->sync_reg, val,
				fence->sync_val);
		if (val != fence->sync_val)
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
	if (pipe->ring_mmio)
		iounmap(pipe->ring_mmio);

	/*
	 * N14: free_irq() 之后不会再有 ISR 推进 fence，残留节点必须在这里
	 * 以 -EIO 完成，否则卸载后这些 fence 的等待者永远醒不过来。
	 */
	pipe_2d_force_complete(pipe);

	if (pipe->ring_bo) {
		struct mwv207_bo *mbo = pipe->ring_bo;

		ret = mwv207_bo_reserve(mbo, true);
		if (!ret) {
			mwv207_bo_kunmap_reserved(mbo);
			mwv207_bo_unpin_reserved(mbo);
			mwv207_bo_unreserve(mbo);
		}
		mwv207_bo_unref(mbo);
	}

	if (pipe->pt_bo) {
		struct mwv207_bo *ptbo = pipe->pt_bo;

		if (!mwv207_bo_reserve(ptbo, true)) {
			mwv207_bo_kunmap_reserved(ptbo);
			mwv207_bo_unpin_reserved(ptbo);
			mwv207_bo_unreserve(ptbo);
		}
		mwv207_bo_unref(ptbo);
		pipe->pt_bo = NULL;
	}

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
	struct mwv207_bo *mbo = NULL;
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

	/*
	 * ring 放 VRAM：厂商 waitlink 模式（默认 j2dmode=1）的命令内存来自
	 * jmkKERNEL_AllocateVideoMemory()，cmem_gpu_addr 就是 VRAM 偏移，
	 * 直接写进 CMDADDR。2D 私有 MMU（日志里的 "cmd mmu"）只在
	 * ringbuffer 模式下用来翻译取指地址，waitlink 模式根本不使能它。
	 */
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

	/*
	 * ring 的 CPU 直映射经 TTM 走 ttm_write_combined（Normal-NC）：普通
	 * store 进写合并缓冲后不保证在随后的 MMIO "踢引擎"之前对设备可见，
	 * 于是引擎可能读到还没改写的 wait 描述符、停在原地。这里再建一份
	 * Device 映射（ioremap，nGnRnE、强序），link 的两个字通过它写，
	 * 与厂商 DDX 用 BAR mmap 写世代的语义一致。
	 */
	pipe->ring_mmio = ioremap(jdev->vram_bar_base + pipe->ring_start_addr,
				  0x40000);
	if (!pipe->ring_mmio)
		pr_warn("mwv207: %s cannot ioremap ring for doorbell, fallback\n",
			fname);

	ret = pipe_2d_mmu_init(pipe);
	if (ret)
		goto unmap_bo;

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
