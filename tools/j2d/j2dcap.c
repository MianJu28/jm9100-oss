/*
 * j2dcap —— 用 LD_PRELOAD 截获厂商驱动的 2D 引擎命令流。
 *
 * 目的：厂商 2D 加速（EXA 拷贝）走 DRM_IOCTL_JM_J2D_SEND_CMD，命令流由闭源
 * userspace 拼装、内核只负责落环。我们要在自己驱动里复刻 2D 拷贝，就必须先
 * 拿到这套命令编码。struct drm_jm_j2d_send_cmd { u64 cmd; u32 size; u32 flags; }
 * ⇒ ioctl 请求号 = _IOWR('d', 0x4B, 16) = 0xc010644b。
 *
 * 用法：
 *   J2D_CAPTURE_DIR=/tmp/j2d-cap LD_PRELOAD=/path/j2dcap.so Xorg :2 ...
 * 产物：$J2D_CAPTURE_DIR/NNNNN-<size>.bin（原始命令流）+ log.txt（序号/size/flags）
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#define J2D_SEND_CMD 0xc010644bUL

struct j2d_cmd {
	unsigned long long cmd;
	unsigned int size;
	unsigned int flags;
};

static int (*real_ioctl)(int, unsigned long, ...);
static unsigned counter;

static const char *cap_dir(void)
{
	const char *d = getenv("J2D_CAPTURE_DIR");

	return (d && *d) ? d : "/tmp/j2d-cap";
}

static void dump(const struct j2d_cmd *c)
{
	const char *dir = cap_dir();
	char path[512];
	int fd, n;

	mkdir(dir, 0777);
	snprintf(path, sizeof(path), "%s/%05u-%u.bin", dir, counter, c->size);
	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd >= 0) {
		if (c->cmd && c->size)
			n = (int)write(fd, (const void *)(unsigned long)c->cmd, c->size);
		(void)n;
		close(fd);
	}

	snprintf(path, sizeof(path), "%s/log.txt", dir);
	fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
	if (fd >= 0) {
		char line[256];
		int len = snprintf(line, sizeof(line), "%05u size=%u flags=0x%x\n",
				   counter, c->size, c->flags);
		if (len > 0)
			n = (int)write(fd, line, (size_t)len);
		(void)n;
		close(fd);
	}
	counter++;
}

int ioctl(int fd, unsigned long req, ...)
{
	va_list ap;
	void *arg;

	va_start(ap, req);
	arg = va_arg(ap, void *);
	va_end(ap);

	if (!real_ioctl)
		real_ioctl = dlsym(RTLD_NEXT, "ioctl");

	if (req == J2D_SEND_CMD && arg)
		dump((const struct j2d_cmd *)arg);

	return real_ioctl(fd, req, arg);
}
