/* bo-mmap-bench: 直接测 MWV207 GEM BO 的 mmap 读写与逐元素读带宽, 判断映射缓存属性。
 * 用法: bo-mmap-bench <domain: system|gtt|vram> [MB] [轮数] [设备节点] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <time.h>
#include <errno.h>
#include <xf86drm.h>

struct gem_create_in { unsigned long long size, alignment; unsigned int preferred_domain, flags; };
struct gem_create_out { unsigned int handle, resv; };
union gem_create { struct gem_create_in in; struct gem_create_out out; };
struct gem_mmap_in { unsigned int handle, pad; };
struct gem_mmap_out { unsigned long long offset; };
union gem_mmap { struct gem_mmap_in in; struct gem_mmap_out out; };

static double now_ms(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts);
   return ts.tv_sec*1000.0 + ts.tv_nsec/1e6; }

int main(int argc, char **argv)
{
   const char *dname = argc > 1 ? argv[1] : "gtt";
   size_t mb = argc > 2 ? strtoul(argv[2], NULL, 10) : 4;
   int R = argc > 3 ? atoi(argv[3]) : 20;
   const char *path = argc > 4 ? argv[4] : "/dev/dri/renderD128";
   unsigned dom = 0x4;
   if (!strcmp(dname, "system")) dom = 0x1;
   if (!strcmp(dname, "vram")) dom = 0x2;

   int fd = open(path, O_RDWR);
   if (fd < 0) { perror("open"); return 1; }
   size_t bytes = mb << 20;

   union gem_create c; memset(&c, 0, sizeof c);
   c.in.size = bytes; c.in.alignment = 0x1000; c.in.preferred_domain = dom; c.in.flags = 0;
   if (drmCommandWriteRead(fd, 0x01, &c, sizeof c) < 0) { perror("create"); return 1; }
   unsigned h = c.out.handle;

   union gem_mmap m; memset(&m, 0, sizeof m); m.in.handle = h;
   if (drmCommandWriteRead(fd, 0x02, &m, sizeof m) < 0) { perror("mmap ioctl"); return 1; }

   void *p = mmap(NULL, bytes, PROT_READ|PROT_WRITE, MAP_SHARED, fd, (off_t)m.out.offset);
   if (p == MAP_FAILED) { perror("mmap"); return 1; }
   void *buf = malloc(bytes);
   memset(buf, 0x5A, bytes);

   memset(p, 0xA5, bytes);                 /* 让页进来, 顺便测写 */
   for (int i = 0; i < 8; i++) memcpy(buf, p, bytes);   /* 预热 */

   static volatile unsigned long long sink;
   double t0 = now_ms();
   for (int i = 0; i < R; i++) { memcpy(buf, p, bytes); sink += ((unsigned char*)buf)[i & (bytes-1)]; }
   double tr = (now_ms()-t0)/R;

   t0 = now_ms();
   for (int i = 0; i < R; i++) { memcpy(p, buf, bytes); sink += ((unsigned char*)p)[i & (bytes-1)]; }
   double tw = (now_ms()-t0)/R;

   /* 稀疏逐元素读: cached 时会比 memcpy 快; WC/uncached 时一样慢 */
   t0 = now_ms();
   volatile unsigned long long acc = 0;
   unsigned long long *q = p;
   for (int i = 0; i < R; i++) for (size_t k = 0; k < bytes/8; k += 64) acc += q[k];
   double tv = (now_ms()-t0)/R;

   printf("domain=%s %s handle=%u %zu MB (sink=%llu)  读 %7.2f ms (%5.2f GB/s)  写 %7.2f ms (%5.2f GB/s)  稀疏读 %7.2f ms  首字节=%02x 校验=%llu\n",
      dname, path, h, mb, sink, tr, mb/tr*1000.0/1024.0, tw, mb/tw*1000.0/1024.0, tv,
      ((unsigned char*)p)[0], acc);
   fflush(stdout);
   munmap(p, bytes);
   free(buf);
   return 0;
}
