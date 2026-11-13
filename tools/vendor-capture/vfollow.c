/*
 * vfollow — 跟着厂商 ioctl 里传的用户指针走，把指针指向的内容落盘。
 *
 * 观察：`jmgpu_drv_video.so` 对 /dev/jmgpu 的"写寄存器/写表"接口都用 8 字节
 * 结构 {ptr, size}（`_IOC(0x6c, 0x19, 8)` 等），真正的数据在用户内存里。
 *
 * 用法：
 *   cc -O2 -shared -fPIC -o ~/dev-bin/vfollow.so tools/vendor-capture/vfollow.c -ldl
 *   VF_DIR=/tmp/vf LD_PRELOAD=~/dev-bin/vfollow.so ffmpeg -hwaccel vaapi ... -i x.jpg -f null -
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>

static int (*real_ioctl)(int, unsigned long, void *);
static unsigned long seq;
static int nfiles;

static int
looks_like_ptr(uint64_t v)
{
   return v > 0x7f0000000000ull && v < 0xffffffffffffull;
}

int
ioctl(int fd, unsigned long req, ...)
{
   if (!real_ioctl)
      real_ioctl = dlsym(RTLD_NEXT, "ioctl");
   va_list ap;
   va_start(ap, req);
   void *arg = va_arg(ap, void *);
   va_end(ap);

   seq++;
   unsigned long sz = (req >> 16) & 0x3fff;
   const char *dir = getenv("VF_DIR");
   int r = real_ioctl(fd, req, arg);

   if (!dir)
      return r;
   if (sz == 8 && arg) {
      uint64_t w[2] = {0, 0};
      memcpy(w, arg, 8);
      size_t max = getenv("VF_MAX") ? strtoul(getenv("VF_MAX"), NULL, 0) : 65536;
      for (int k = 0; k < 4; k++) {
         uint64_t p = ((uint64_t *)((char *)arg + 0))[0];
         if (k == 1)
            p = ((uint64_t *)arg)[1];
         if (!looks_like_ptr(p))
            continue;
         size_t len = max;
         char name[600];
         snprintf(name, sizeof(name), "%s/%04lu_%03lu_req%lx_p%llx.bin", dir,
                  seq, (unsigned long)k, req, (unsigned long long)p);
         int fdw = open(name, O_WRONLY | O_CREAT | O_TRUNC, 0644);
         if (fdw < 0)
            continue;
         /* 用户指针：直接读本进程地址空间即可 */
         ssize_t got = write(fdw, (void *)(uintptr_t)p, len);
         close(fdw);
         if (got > 0)
            nfiles++;
         break;
      }
   }
   return r;
}
