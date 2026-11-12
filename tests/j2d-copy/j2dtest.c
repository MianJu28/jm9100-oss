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
		if (pr <= 0) fprintf(stderr, "  等待 2D fence 超时 (poll=%d)\n", pr);
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

int main(int argc, char **argv)
{
	int mode = argc > 3 ? atoi(argv[3]) : 0;      /* 0=填充 1=拷贝 */
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
