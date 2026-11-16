/* 2D 引擎自校验：用 DRM_IOCTL_MWV207_SUBMIT(engine_type=3) 提交填充/拷贝命令，
 * 再用 CPU 读回比对。用于反推 surface 描述字的编码。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#define MWV_IOCTL(cmd, arg) DRM_IOWR(DRM_COMMAND_BASE + (cmd), arg)

struct mwv207_ctx { uint32_t op, flags, handle, resv; };
struct gem_create_in { uint64_t size, alignment; uint32_t preferred_domain, flags; };
struct gem_create_out { uint32_t handle, resv; };
union gem_create { struct gem_create_in in; struct gem_create_out out; };
struct gem_mmap_in { uint32_t handle, pad; };
struct gem_mmap_out { uint64_t offset; };
union gem_mmap { struct gem_mmap_in in; struct gem_mmap_out out; };

struct submit {
	uint32_t ctx, engine_type, engine_id, flags, fence_fd;
	uint32_t cmd_size, nr_relocs, nr_bos, nr_cmd_dat, padding;
	uint64_t cmds, relocs, bos, cmd_dats;
};
struct bo_acc { uint32_t handle, flags; };
struct reloc { uint32_t idx, cmd_offset; uint64_t bo_offset; uint32_t type, pad; };

#define IOCTL_CTX     MWV_IOCTL(0x04, struct mwv207_ctx)
#define IOCTL_CREATE  MWV_IOCTL(0x01, union gem_create)
#define IOCTL_MMAP    MWV_IOCTL(0x02, union gem_mmap)
#define IOCTL_SUBMIT  MWV_IOCTL(0x05, struct submit)

static int fd;
static uint32_t ctx;

static void *bo_new(uint32_t size, uint32_t *handle)
{
	union gem_create c; union gem_mmap m;
	memset(&c, 0, sizeof(c));
	c.in.size = size;
	if (ioctl(fd, IOCTL_CREATE, &c)) { perror("GEM_CREATE"); exit(1); }
	*handle = c.out.handle;
	memset(&m, 0, sizeof(m));
	m.in.handle = *handle;
	if (ioctl(fd, IOCTL_MMAP, &m)) { perror("GEM_MMAP"); exit(1); }
	void *p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, (off_t)m.out.offset);
	if (p == MAP_FAILED) { perror("mmap"); exit(1); }
	return p;
}

static int submit(uint32_t *cmds, uint32_t nwords, uint32_t *bos, int nbos,
                  struct reloc *rl, int nrl)
{
	struct submit s; struct bo_acc acc[8]; uint32_t fd_out = 0; int rc;
	memset(&s, 0, sizeof(s));
	for (int i = 0; i < nbos; i++) { acc[i].handle = bos[i]; acc[i].flags = 1; }
	s.ctx = ctx; s.engine_type = 3; s.engine_id = 0;
	s.flags = 2;                  /* FENCE_FD_OUT */
	s.cmd_size = nwords * 4;
	s.cmds = (uint64_t)(uintptr_t)cmds;
	s.nr_bos = nbos; s.bos = (uint64_t)(uintptr_t)acc;
	s.nr_relocs = nrl; s.relocs = (uint64_t)(uintptr_t)rl;
	rc = ioctl(fd, IOCTL_SUBMIT, &s);
	if (rc) { fprintf(stderr, "SUBMIT failed: %s\n", strerror(errno)); return -1; }
	fd_out = s.fence_fd;
	if ((int)fd_out >= 0) {
		struct pollfd pfd = { .fd = (int)fd_out, .events = POLLIN };
		int pr = poll(&pfd, 1, 5000);
		if (pr <= 0) {
			fprintf(stderr, "  等待 2D fence 超时 (poll=%d)：引擎没有执行本提交\n", pr);
			close((int)fd_out);
			return 1;
		}
		close((int)fd_out);
	}
	return 0;
}

static uint32_t desc_cand(int i, uint32_t pitch, uint32_t w, uint32_t h)
{
	switch (i) {
	case 0: return 0;
	case 1: return pitch;
	case 2: return pitch << 16;
	case 3: return (pitch >> 6) << 16 | 0x4;
	case 4: return 0x00040600;
	case 5: return 0x00100000;
	case 6: return ((pitch >> 6) & 0xffff) << 16;
	case 7: return (pitch >> 4) & 0xffff;
	case 8: return 0x00010080;
	/* 9..12：从厂商抓包矩阵里观察到的描述字原值/变体（覆盖 pitch/8、
	 * 带高位标志），全部实测无效 —— 说明 2D draw opcode 的编码不是
	 * 我们猜的这套，留着仅作"已试过"的记录。 */
	case 9: return 0x00050680;
	case 10: return 0x00040600 | ((pitch / 8) & 0xffff);
	case 11: return 0x00010000 | ((pitch / 8) & 0xffff);
	case 12: return 0x00010000 | ((pitch / 64) & 0xffff);
	default: return 0;
	}
}

/* 提交一次"填充"并读回；返回不一致的字数（0 = 成功）。每次都用全新的
 * dst BO，便于扫描时互不干扰。 */
static int fill_once(uint32_t w, uint32_t h, uint32_t pitch, uint32_t desc)
{
	uint32_t size = pitch * h, hd;
	uint32_t *dst = bo_new(size, &hd);
	uint32_t cmds[32], n = 0;

	cmds[n++] = 0x40000000 | 0x1008; cmds[n++] = 0;
	cmds[n++] = 0x40000000 | 0x100c; cmds[n++] = 0;
	cmds[n++] = 0x40000000 | 0x1014; cmds[n++] = 0;
	cmds[n++] = 0x40000000 | 0x1020; cmds[n++] = 0x00ff00ff;
	cmds[n++] = 0x40000000 | 0x1054; cmds[n++] = 0xffffffff;
	cmds[n++] = 0x82000054; cmds[n++] = 0;
	uint32_t dst_addr_off = n * 4; cmds[n++] = 0;
	cmds[n++] = desc; cmds[n++] = (w << 16) | h;
	cmds[n++] = 0x81000000;

	uint32_t bos[1] = { hd };
	struct reloc rl[1] = { { 0, dst_addr_off, 0, 1, 0 } };
	if (submit(cmds, n, bos, 1, rl, 1)) return -1;

	int bad = 0;
	for (uint32_t i = 0; i < size / 4; i++)
		if (dst[i] != 0x00ff00ffu) bad++;
	return bad;
}


/* 按厂商 DDX 反汇编得到的"快速填充"编码（jmgpu2dFillRectQuick）：
 * 目标地址落在寄存器 0x01054，操作数为 0 / 描述字 / 起点 / 宽高。 */
static int vendor_fill(uint32_t w, uint32_t h, uint32_t pitch, uint32_t fmt)
{
	uint32_t size = pitch * h, hd;
	uint32_t *dst = bo_new(size, &hd);
	uint32_t cmds[32], n = 0;

	memset(dst, 0, size);
	cmds[n++] = 0x40001008; cmds[n++] = 0;
	cmds[n++] = 0x4000100c; cmds[n++] = fmt << 3;
	cmds[n++] = 0x40001014; cmds[n++] = fmt;
	cmds[n++] = 0x40001020; cmds[n++] = 0x00ff00ff;
	cmds[n++] = 0x40001054;
	uint32_t da = n * 4; cmds[n++] = 0;		/* 目标地址，由 reloc 填 */
	cmds[n++] = 0x82000054; cmds[n++] = 0;
	cmds[n++] = ((pitch / 16) << 16) | 0;
	cmds[n++] = 0;					/* x | y<<16 */
	cmds[n++] = (w << 16) | h;
	cmds[n++] = 0x81000000;

	uint32_t bos[1] = { hd };
	struct reloc rl[1] = { { 0, da, 0, 1, 0 } };
	if (submit(cmds, n, bos, 1, rl, 1)) return -1;

	int bad = 0;
	for (uint32_t i = 0; i < size / 4; i++)
		if (dst[i] != 0x00ff00ffu) bad++;
	printf("vendor_fill %ux%u pitch=%u fmt=%u: %s (%d bad)\n", w, h, pitch, fmt,
	       bad ? "FAIL" : "OK", bad);
	return bad;
}

/* 按厂商 jmgpu2dCopyArea 的 26 字编码：源/目标地址走 0x01010/0x01018。 */
static int vendor_copy(uint32_t w, uint32_t h, uint32_t pitch)
{
	uint32_t size = pitch * h, hs, hd;
	uint32_t *src = bo_new(size, &hs), *dst = bo_new(size, &hd);
	uint32_t cmds[40], n = 0;
	const uint32_t fmt = 2, desc = fmt | 0x20000;

	for (uint32_t i = 0; i < size / 4; i++) src[i] = 0x00112233u + i;
	memset(dst, 0, size);

	cmds[n++] = 0x40001008; cmds[n++] = 0;
	cmds[n++] = 0x4000100c; cmds[n++] = fmt << 3;
	cmds[n++] = 0x40001014; cmds[n++] = fmt;
	cmds[n++] = 0x40001010;
	uint32_t da = n * 4; cmds[n++] = 0;		/* 目标地址 */
	cmds[n++] = 0x40001018;
	uint32_t sa = n * 4; cmds[n++] = 0;		/* 源地址 */
	cmds[n++] = 0x40001054; cmds[n++] = 0xffffffff;
	cmds[n++] = 0x8200001c; cmds[n++] = desc;
	cmds[n++] = (pitch / 16) << 16;
	cmds[n++] = 0;
	cmds[n++] = (w << 16) | h;
	cmds[n++] = (pitch / 16) << 16;
	cmds[n++] = 0;
	cmds[n++] = (w << 16) | h;
	cmds[n++] = 0; cmds[n++] = 0; cmds[n++] = 0; cmds[n++] = 0;
	cmds[n++] = 0;
	cmds[n++] = 0x81000000;

	uint32_t bos[2] = { hd, hs };
	struct reloc rl[2] = { { 0, da, 0, 1, 0 }, { 1, sa, 0, 1, 0 } };
	if (submit(cmds, n, bos, 2, rl, 2)) return -1;

	int bad = 0;
	for (uint32_t i = 0; i < size / 4; i++)
		if (dst[i] != src[i]) bad++;
	printf("vendor_copy %ux%u pitch=%u: %s (%d bad)\n", w, h, pitch,
	       bad ? "FAIL" : "OK", bad);
	return bad;
}


/* 对照组：只发一条 END（驱动会把它换成 NOP 再追加 fence 同步 + wait），
 * 用来验证"link + 踢引擎"这条通路本身是否工作。 */
static int vendor_ping(void)
{
	uint32_t cmds[4];
	uint32_t n = 0;

	cmds[n++] = 0x81000000;
	if (submit(cmds, n, NULL, 0, NULL, 0)) {
		printf("vendor_ping: 提交失败\n");
		return -1;
	}
	printf("vendor_ping: fence 完成（link/踢引擎通路正常）\n");
	return 0;
}

/* 描述字扫描：假设 desc = (格式/标志 << 16) | (pitch/8)，
 * 枚举 fmt 0..7 × 额外标志 {0,0x600}，看哪一组真的能出图。 */
static int sweep_desc(uint32_t w, uint32_t h)
{
	uint32_t pitch = w * 4;
	uint32_t extra[2] = { 0, 0x600 };
	unsigned f, e;
	int hit = 0;

	for (f = 0; f <= 7; f++) {
		for (e = 0; e < 2; e++) {
			uint32_t desc = (f << 16) | extra[e] | ((pitch / 8) & 0x3ff);
			int bad = fill_once(w, h, pitch, desc);
			printf("mode=3 fmt=%u extra=0x%x desc=0x%08x: %s\n",
			       f, extra[e], desc,
			       bad == 0 ? "OK(命中)" : (bad < 0 ? "提交失败" : "无效"));
			if (bad == 0) hit++;
		}
	}
	printf("命中 %d 组\n", hit);
	return hit ? 0 : 2;
}

/* 模式 4：按厂商 DDX 反编译出来的公式扫描描述字
 *   desc = (pitch/16 << 16) | 低 16 位格式码
 * 低 16 位取抓包里出现过的 0x600/0x680/0x80/0xc0/0x60/0x0。 */
static int sweep_copy(uint32_t w, uint32_t h)
{
	uint32_t pitch = w * 4;
	uint32_t lows[] = { 0x600, 0x680, 0x80, 0xc0, 0x60, 0x0 };
	uint32_t hi = (pitch / 16) & 0xffff;
	unsigned i;
	int hit = 0;

	for (i = 0; i < sizeof(lows) / sizeof(lows[0]); i++) {
		uint32_t desc = (hi << 16) | lows[i];
		int bad = fill_once(w, h, pitch, desc);
		printf("mode=4 desc=0x%08x (pitch/16=%u): %s\n", desc, pitch / 16,
		       bad == 0 ? "OK(命中)" : (bad < 0 ? "提交失败" : "无效"));
		if (bad == 0) hit++;
	}
	printf("命中 %d 组\n", hit);
	return hit ? 0 : 2;
}

/* 模式 5：怀疑 2D 引擎要的是"总线地址"（厂商抓包里地址都是 0x01e0xxxx），
 * 而我们的 BO GPU 地址从 0 附近开始。用 reloc 的 bo_offset 把地址整体
 * 平移几个候选基址，看哪一档能让填充生效。 */
static int sweep_base(uint32_t w, uint32_t h)
{
	uint32_t pitch = w * 4, size = pitch * h;
	uint64_t bases[] = { 0, 0x01000000, 0x01e00000, 0x02000000, 0x20000000 };
	uint32_t highs[] = { 0x0040, 0x0060, 0x00c0, 0x0100, 0x01e0 };
	unsigned b, hi_i;
	int hit = 0;

	for (b = 0; b < sizeof(bases) / sizeof(bases[0]); b++) {
		for (hi_i = 0; hi_i < sizeof(highs) / sizeof(highs[0]); hi_i++) {
			uint32_t hd;
			uint32_t *dst = bo_new(size, &hd);
			uint32_t cmds[32], n = 0;
			uint32_t desc = ((uint32_t)highs[hi_i] << 16) | 0x0600;

			cmds[n++] = 0x40000000 | 0x1008; cmds[n++] = 0;
			cmds[n++] = 0x40000000 | 0x100c; cmds[n++] = 0;
			cmds[n++] = 0x40000000 | 0x1014; cmds[n++] = 0;
			cmds[n++] = 0x40000000 | 0x1020; cmds[n++] = 0x00ff00ff;
			cmds[n++] = 0x40000000 | 0x1054; cmds[n++] = 0xffffffff;
			cmds[n++] = 0x82000054; cmds[n++] = 0;
			uint32_t off = n * 4; cmds[n++] = 0;
			cmds[n++] = desc; cmds[n++] = (w << 16) | h;
			cmds[n++] = 0x81000000;

			uint32_t bos[1] = { hd };
			struct reloc rl[1] = { { 0, off, bases[b], 1, 0 } };
			int bad = -1;
			if (!submit(cmds, n, bos, 1, rl, 1)) {
				bad = 0;
				for (uint32_t i = 0; i < size / 4; i++)
					if (dst[i] != 0x00ff00ffu) bad++;
			}
			printf("mode=5 base=%#llx desc=0x%08x: %s\n",
			       (unsigned long long)bases[b], desc,
			       bad == 0 ? "OK(命中)" : (bad < 0 ? "提交失败" : "无效"));
			if (bad == 0) hit++;
		}
	}
	printf("命中 %d 组\n", hit);
	return hit ? 0 : 2;
}

int main(int argc, char **argv)
{
	int mode = argc > 3 ? atoi(argv[3]) : 0;      /* 0=填充 1=拷贝 3=描述字扫描 */
	int cand = argc > 4 ? atoi(argv[4]) : 0;
	uint32_t w = argc > 1 ? (uint32_t)atoi(argv[1]) : 256;
	uint32_t h = argc > 2 ? (uint32_t)atoi(argv[2]) : 256;
	uint32_t pitch = w * 4, size = pitch * h;
	uint32_t hs, hd;

	fd = open("/dev/dri/renderD128", O_RDWR);
	if (fd < 0) { perror("open"); return 1; }
	struct mwv207_ctx c; memset(&c, 0, sizeof(c)); c.op = 0;
	if (ioctl(fd, IOCTL_CTX, &c)) { perror("CTX"); return 1; }
	ctx = c.handle;

	if (mode == 5) {
		struct mwv207_ctx cc; memset(&cc, 0, sizeof(cc)); cc.op = 0;
		if (ioctl(fd, IOCTL_CTX, &cc)) { perror("CTX"); return 1; }
		ctx = cc.handle;
		return sweep_base(w, h);
	}
	if (mode == 4) {
		struct mwv207_ctx cc; memset(&cc, 0, sizeof(cc)); cc.op = 0;
		if (ioctl(fd, IOCTL_CTX, &cc)) { perror("CTX"); return 1; }
		ctx = cc.handle;
		return sweep_copy(w, h);
	}
	if (mode == 3) {
		struct mwv207_ctx cc; memset(&cc, 0, sizeof(cc)); cc.op = 0;
		if (ioctl(fd, IOCTL_CTX, &cc)) { perror("CTX"); return 1; }
		ctx = cc.handle;
		return sweep_desc(w, h);
	}
	if ((mode >= 6 && mode <= 9) || mode == 11) {
		struct mwv207_ctx cc; memset(&cc, 0, sizeof(cc)); cc.op = 0;
		if (ioctl(fd, IOCTL_CTX, &cc)) { perror("CTX"); return 1; }
		ctx = cc.handle;
		if (mode == 6) return vendor_fill(w, h, pitch, 2) ? 2 : 0;
		if (mode == 7) return vendor_fill(w, h, pitch, 1) ? 2 : 0;
		if (mode == 8) return vendor_copy(w, h, pitch) ? 2 : 0;
		if (mode == 11) return vendor_ping() ? 2 : 0;
		return vendor_copy(w, h, pitch) ? 2 : 0;
	}

	uint32_t *src = bo_new(size, &hs);
	uint32_t *dst = bo_new(size, &hd);
	for (uint32_t i = 0; i < size / 4; i++) src[i] = 0x00112233u + i;
	memset(dst, 0, size);

	uint32_t cmds[32], n = 0, sdesc, ddesc;
	sdesc = desc_cand(cand, pitch, w, h);
	ddesc = sdesc;
	uint32_t wh = (w << 16) | h;

	if (mode == 0) {
		cmds[n++] = 0x40000000 | 0x1008; cmds[n++] = 0;
		cmds[n++] = 0x40000000 | 0x100c; cmds[n++] = 0;
		cmds[n++] = 0x40000000 | 0x1014; cmds[n++] = 0;
		cmds[n++] = 0x40000000 | 0x1020; cmds[n++] = 0x00ff00ff;
		cmds[n++] = 0x40000000 | 0x1054; cmds[n++] = 0xffffffff;
		cmds[n++] = 0x82000054; cmds[n++] = 0;
		uint32_t dst_addr_off = n * 4; cmds[n++] = 0;   /* 由 reloc 填 */
		cmds[n++] = ddesc; cmds[n++] = wh;
		cmds[n++] = 0x81000000;
		uint32_t bos[1] = { hd };
		struct reloc rl[1] = { { 0, dst_addr_off, 0, 1, 0 } };
		if (submit(cmds, n, bos, 1, rl, 1)) return 1;
	} else {
		cmds[n++] = 0x40000000 | 0x1008; cmds[n++] = 0;
		cmds[n++] = 0x40000000 | 0x100c; cmds[n++] = 0;
		cmds[n++] = 0x40000000 | 0x1014; cmds[n++] = 0;
		cmds[n++] = 0x40000000 | 0x1010; cmds[n++] = 0;
		cmds[n++] = 0x40000000 | 0x1018; cmds[n++] = sdesc;
		cmds[n++] = 0x40000000 | 0x1054; cmds[n++] = 0xffffffff;
		cmds[n++] = 0x8200001c; cmds[n++] = 0x000200f0;
		uint32_t da = n * 4; cmds[n++] = 0;
		cmds[n++] = ddesc; cmds[n++] = wh;
		uint32_t sa = n * 4; cmds[n++] = 0;
		cmds[n++] = sdesc; cmds[n++] = wh;
		cmds[n++] = 0x81000000;
		uint32_t bos[2] = { hd, hs };
		struct reloc rl[2] = { { 0, da, 0, 1, 0 }, { 1, sa, 0, 1, 0 } };
		if (submit(cmds, n, bos, 2, rl, 2)) return 1;
	}

	/* 校验 */
	int bad = 0, first = -1;
	for (uint32_t i = 0; i < size / 4; i++) {
		uint32_t want = (mode == 0) ? 0x00ff00ffu : src[i];
		if (dst[i] != want) { if (bad++ < 1) first = (int)i; }
	}
	if (!bad) printf("mode=%d desc=%#x %ux%u: OK\n", mode, ddesc, w, h);
	else printf("mode=%d desc=%#x %ux%u: %d/%u 不一致，首个差异 idx=%d got=%#x\n",
	            mode, ddesc, w, h, bad, size / 4, first,
	            first >= 0 ? dst[first] : 0);
	return bad ? 2 : 0;
}
