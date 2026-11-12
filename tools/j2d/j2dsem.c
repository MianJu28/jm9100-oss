/* j2dsem: 在厂商栈上 hook libdrm_jmgpu 的 2D 硬件层，打印"语义调用 + 命令字"，
 * 用来替代读反汇编：把 jmo_2D_* 高层的每一步落成 (寄存器, 宽度, 值) 与追加到
 * 命令缓冲里的字，从而反推 0x820000xx 这类 draw opcode 的确切编码。
 *
 * 用法（厂商栈，停掉 lightdm 后起临时 X 时）：
 *   J2D_SEM_LOG=/tmp/j2dsem.log LD_PRELOAD=/tmp/j2dsem.so \
 *     /usr/lib/xorg/Xorg :0 -noreset -nolisten tcp
 * 然后跑 tools/j2d/xcopy 制造一次已知拷贝，日志里就能看到对应的
 *   Load2DState(reg=…, width=…, vals=[…])
 * 以及缓冲区新增的字（由 dump_tail 打印）。
 *
 * 说明：libdrm_jmgpu 未 strip，内部调用走 PLT，所以 LD_PRELOAD 能拦住这些
 * 内部符号；拿不到符号时只是不打印，不影响功能。
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <dlfcn.h>
#include <unistd.h>
#include <execinfo.h>

/* 上下文里命令缓冲的位置（从 jmo_HARDWARE_Load2DState 的反汇编得到） */
#define CTX_BUF_PTR 0x3768
#define CTX_BUF_CUR 0x3770

static FILE *out;
static unsigned long seq;
static int enabled = -1;

static void open_log(void)
{
	const char *p = getenv("J2D_SEM_LOG");
	enabled = 0;
	if (!p || !*p)
		return;
	out = fopen(p, "a");
	if (!out)
		return;
	setvbuf(out, NULL, _IOLBF, 0);
	enabled = 1;
}

static void dump_tail(void *ctx, const char *tag)
{
	void *base = NULL;
	uint32_t cur = 0;

	if (!enabled || !ctx)
		return;
	memcpy(&base, (char *)ctx + CTX_BUF_PTR, sizeof(base));
	memcpy(&cur, (char *)ctx + CTX_BUF_CUR, sizeof(cur));
	if (!base || !cur || cur > 4096)
		return;
	fprintf(out, "  [%s] 缓冲区游标=%u 尾部 8 字:", tag, cur);
	for (unsigned i = (cur > 8 ? cur - 8 : 0); i < cur; i++)
		fprintf(out, " %08x", ((uint32_t *)base)[i]);
	fprintf(out, "\n");
}

/* 记录 before/after 游标差，把新增的字全部打出来 */
static void dump_delta(void *ctx, uint32_t before, const char *tag)
{
	void *base = NULL;
	uint32_t cur = 0;

	if (!enabled || !ctx)
		return;
	memcpy(&base, (char *)ctx + CTX_BUF_PTR, sizeof(base));
	memcpy(&cur, (char *)ctx + CTX_BUF_CUR, sizeof(cur));
	if (!base || cur <= before || cur > 4096)
		return;
	fprintf(out, "  追加 %u 字 [%s]:", cur - before, tag);
	for (unsigned i = before; i < cur; i++)
		fprintf(out, " %08x", ((uint32_t *)base)[i]);
	fprintf(out, "\n");
}

static uint32_t cursor_of(void *ctx)
{
	uint32_t cur = 0;
	if (enabled && ctx)
		memcpy(&cur, (char *)ctx + CTX_BUF_CUR, sizeof(cur));
	return cur;
}

typedef int (*st_fn)(void *ctx, uint32_t reg, uint32_t width, const void *vals);

int jmo_HARDWARE_Load2DState(void *ctx, uint32_t reg, uint32_t width,
			     const void *vals)
{
	static st_fn real;
	uint32_t before = cursor_of(ctx);
	int rc;

	if (!real)
		real = (st_fn)dlsym(RTLD_NEXT, "jmo_HARDWARE_Load2DState");
	if (enabled) {
		fprintf(out, "#%lu Load2DState(ctx=%p reg=0x%x width=%u vals=",
			++seq, ctx, reg, width);
		for (uint32_t i = 0; i < width && vals; i++)
			fprintf(out, " %08x", ((const uint32_t *)vals)[i]);
		fprintf(out, ")\n");
	}
	rc = real ? real(ctx, reg, width, vals) : -1;
	dump_delta(ctx, before, "Load2DState");
	return rc;
}

int jmo_HARDWARE_StartDE(void *ctx, void *a, void *b, void *c, void *d,
			 void *e, void *f)
{
	static int (*real)(void *, void *, void *, void *, void *, void *, void *);
	uint32_t before = cursor_of(ctx);
	int rc;

	if (!real)
		real = dlsym(RTLD_NEXT, "jmo_HARDWARE_StartDE");
	if (enabled)
		fprintf(out, "#%lu StartDE(ctx=%p)\n", ++seq, ctx);
	rc = real ? real(ctx, a, b, c, d, e, f) : -1;
	dump_delta(ctx, before, "StartDE");
	dump_tail(ctx, "StartDE-after");
	return rc;
}

int jmo_HARDWARE_Clear2D(void *ctx, void *a, uint32_t b, void *c)
{
	static int (*real)(void *, void *, uint32_t, void *);
	uint32_t before = cursor_of(ctx);
	int rc;

	if (!real)
		real = dlsym(RTLD_NEXT, "jmo_HARDWARE_Clear2D");
	if (enabled)
		fprintf(out, "#%lu Clear2D(ctx=%p a=%p b=%u c=%p)\n",
			++seq, ctx, a, b, c);
	rc = real ? real(ctx, a, b, c) : -1;
	dump_delta(ctx, before, "Clear2D");
	return rc;
}

int jmo_HARDWARE_2DChipEnable(void *ctx, uint32_t on)
{
	static int (*real)(void *, uint32_t);
	uint32_t before = cursor_of(ctx);
	int rc;

	if (!real)
		real = dlsym(RTLD_NEXT, "jmo_HARDWARE_2DChipEnable");
	if (enabled)
		fprintf(out, "#%lu 2DChipEnable(ctx=%p on=%u)\n", ++seq, ctx, on);
	rc = real ? real(ctx, on) : -1;
	dump_delta(ctx, before, "2DChipEnable");
	return rc;
}

/* 判断"命令流到底是谁拼的"：如果 DDX 调这两个高层 API，
 * 那实现就在库里；否则在 DDX 自己的 EXA 代码里。 */
static void bt(const char *tag)
{
	void *buf[24];
	int n;

	if (!enabled)
		return;
	n = backtrace(buf, 24);
	fprintf(out, "#%lu %s 调用栈:\n", ++seq, tag);
	backtrace_symbols_fd(buf, n, fileno(out));
}

int jmo_2D_Clear(void *ctx, void *a, void *b, void *c, void *d, void *e)
{
	static int (*real)(void *, void *, void *, void *, void *, void *);
	int rc;

	if (!real)
		real = dlsym(RTLD_NEXT, "jmo_2D_Clear");
	if (enabled) {
		fprintf(out, "#%lu jmo_2D_Clear(ctx=%p)\n", ++seq, ctx);
		bt("jmo_2D_Clear");
	}
	/* 只记录不转发：这两个 API 的参数个数不确定（Clear 用到 x0..x6），
	 * 用猜的参数去转发有崩溃风险；这里只需要"有没有被调用"这一个事实。 */
	(void)real; (void)a; (void)b; (void)c; (void)d; (void)e;
	rc = 0;
	return rc;
}

int jmo_2D_Blit(void *ctx, void *a, void *b, void *c, void *d, void *e, void *f)
{
	static int (*real)(void *, void *, void *, void *, void *, void *, void *);
	int rc;

	if (!real)
		real = dlsym(RTLD_NEXT, "jmo_2D_Blit");
	if (enabled) {
		fprintf(out, "#%lu jmo_2D_Blit(ctx=%p)\n", ++seq, ctx);
		bt("jmo_2D_Blit");
	}
	(void)real; (void)a; (void)b; (void)c; (void)d; (void)e; (void)f;
	rc = 0;
	return rc;
}

int drm_jmgpu_j2d_send_cmd(int fd, void *cmd)
{
	static int (*real)(int, void *);
	uint64_t addr = 0;
	uint32_t size = 0, flags = 0;
	int rc;

	if (!real)
		real = dlsym(RTLD_NEXT, "drm_jmgpu_j2d_send_cmd");
	if (enabled && cmd) {
		memcpy(&addr, cmd, 8);
		memcpy(&size, (char *)cmd + 8, 4);
		memcpy(&flags, (char *)cmd + 12, 4);
		fprintf(out, "#%lu j2d_send_cmd(fd=%d cmd=%p addr=%#llx size=%u flags=%u)\n",
			++seq, fd, cmd, (unsigned long long)addr, size, flags);
		bt("j2d_send_cmd");
	}
	rc = real ? real(fd, cmd) : -1;
	return rc;
}

__attribute__((constructor)) static void init(void)
{
	open_log();
	if (enabled)
		fprintf(out, "==== j2dsem 启动 (pid %d) ====\n", (int)getpid());
}
