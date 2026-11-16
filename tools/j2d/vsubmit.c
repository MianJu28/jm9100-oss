/* 通过厂商 KMD 的 2D 提交 ioctl 发一条命令流，用来在厂商栈上验证命令编码。
 *
 * 厂商接口（libdrm_jmgpu.so 的 drm_jmgpu_j2d_send_cmd）：
 *   struct { void *cmd; uint32_t size; uint32_t flags; } arg = { cmd, size, 1 };
 *   drmIoctl(fd, 0xc010644b, &arg);
 *
 * 用法：vsubmit <hex words...>    例：vsubmit 0x40001010 0x... 0x81000000
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

int main(int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "usage: %s <hex words...>\n", argv[0]);
		return 2;
	}
	uint32_t buf[4096];
	int n = argc - 1;
	if (n > 4096) n = 4096;
	for (int i = 0; i < n; i++)
		buf[i] = (uint32_t)strtoul(argv[i + 1], NULL, 0);

	int fd = open("/dev/dri/card0", O_RDWR);
	if (fd < 0) { perror("open /dev/jmgpu"); return 1; }

	struct { void *cmd; uint32_t size; uint32_t flags; } arg = { buf, (uint32_t)(n * 4), 1 };
	int rc = ioctl(fd, 0xc010644b, &arg);
	if (rc) perror("j2d_send_cmd");
	else printf("sent %d words\n", n);
	close(fd);
	return rc ? 1 : 0;
}