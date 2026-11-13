/*
 * vcap —— 用 LD_PRELOAD 记录厂商 UMD 发给内核的 ioctl（用于反推厂商的寄存器序列）。
 *
 * 起因：JPEG baseline 解码在厂商栈上可用、在我们驱动上硬件直接报 DEC_ERROR_INT，
 * 需要看厂商到底往 DEC 写了哪些寄存器（尤其是有没有我们没找到的启动位）。
 * 厂商 UMD 的提交接口不公开，这里把当次进程里所有"带缓冲区的大 ioctl"都落盘，
 * 事后按请求号分类、再用 tools/j2d/j2d-decode.py 之类的小工具解命令字。
 *
 * 用法：
 *   VCAP_DIR=/tmp/vcap LD_PRELOAD=./vcap.so ffmpeg -hwaccel vaapi ... -i x.jpg -f null -
 *   ls /tmp/vcap            # NNNNN-req<hex>-sz<n>.bin + log.txt
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

static int (*real_ioctl)(int, unsigned long, ...);
static unsigned counter;
static FILE *logf;

static const char *cap_dir(void)
{
   const char *d = getenv("VCAP_DIR");
   return (d && *d) ? d : "/tmp/vcap";
}

static void ensure_dir(void)
{
   static int done;
   if (done) return;
   mkdir(cap_dir(), 0777);
   done = 1;
}

static void dump_buf(unsigned long req, unsigned sz, const void *buf)
{
   char path[512];
   int fd;
   ensure_dir();
   snprintf(path, sizeof(path), "%s/%05u-req%08lx-sz%u.bin", cap_dir(), counter, req, sz);
   fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
   if (fd >= 0) {
      if (buf && sz) {
         ssize_t n = write(fd, buf, sz);
         (void)n;
      }
      close(fd);
   }
   if (!logf) {
      char lp[512];
      snprintf(lp, sizeof(lp), "%s/log.txt", cap_dir());
      logf = fopen(lp, "a");
   }
   if (logf) {
      fprintf(logf, "%05u req=0x%08lx dir=%c type=%c nr=0x%02x size=%u pc=%p\n",
              counter, req, _IOC_DIR(req) == _IOC_WRITE ? 'W' :
              _IOC_DIR(req) == _IOC_READ ? 'R' : _IOC_DIR(req) ? 'B' : '-',
              _IOC_TYPE(req), _IOC_NR(req), sz, __builtin_return_address(0));
      fflush(logf);
   }
   counter++;
}

int ioctl(int fd, unsigned long req, ...)
{
   va_list ap;
   void *arg;
   unsigned sz = _IOC_SIZE(req);

   if (!real_ioctl)
      real_ioctl = dlsym(RTLD_NEXT, "ioctl");

   va_start(ap, req);
   arg = va_arg(ap, void *);
   va_end(ap);

   /* 只记录"带一块 ≥64 字节缓冲区"的 ioctl，避免刷屏；submit 类请求不管大小都记。 */
   if (arg && sz >= 64) {
      dump_buf(req, sz < 65536 ? sz : 4096, arg);
   }

   return real_ioctl(fd, req, arg);
}

int drmCommandWrite(int fd, unsigned long drmCommandIndex, const void *data, unsigned long size)
{
   static int (*real)(int, unsigned long, const void *, unsigned long);
   unsigned long req = 0x40000000UL | (size << 16) | drmCommandIndex;
   if (!real) real = dlsym(RTLD_NEXT, "drmCommandWrite");
   if (data && size >= 64) dump_buf(req, size < 65536 ? size : 4096, data);
   return real ? real(fd, drmCommandIndex, data, size) : -1;
}

int drmCommandWriteRead(int fd, unsigned long drmCommandIndex, void *data, unsigned long size)
{
   static int (*real)(int, unsigned long, void *, unsigned long);
   unsigned long req = 0xc0000000UL | (size << 16) | drmCommandIndex;
   if (!real) real = dlsym(RTLD_NEXT, "drmCommandWriteRead");
   if (data && size >= 64) dump_buf(req, size < 65536 ? size : 4096, data);
   return real ? real(fd, drmCommandIndex, data, size) : -1;
}
