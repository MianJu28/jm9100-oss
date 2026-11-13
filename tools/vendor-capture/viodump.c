/*
 * viodump — LD_PRELOAD 工具：在指定第 N 次 ioctl 调用的**之前**，把进程里
 * 512 B–4 MB 的可读写匿名映射整体落盘。
 *
 * 为什么不用拦 mmap：厂商 UMD 的 JPEG 辅助表/流缓冲是否走 mmap 不确定，
 * 而且这些缓冲常常在进程退出前就被 unmap/free 掉。这里改成"发 ioctl 的
 * 瞬间抓快照"，此时 UMD 已经把表填好、正要把地址交给内核，内容一定在内存里。
 *
 * 用法：
 *   cc -O2 -shared -fPIC -o ~/dev-bin/viodump.so tools/vendor-capture/viodump.c -ldl
 *   VIODUMP_DIR=/tmp/vid VIODUMP_AT=100 LD_PRELOAD=~/dev-bin/viodump.so ffmpeg ...
 *
 * 输出：$VIODUMP_DIR/00xx_<start>_<len>.bin 与 index.txt
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/stat.h>

static int (*real_ioctl)(int, unsigned long, void *);
static unsigned long call_count;
static int done;
static int dumped;

struct region {
   unsigned long start;
   unsigned long len;
};

static char dump_dir[256];
static int dump_seq;

static void
dump_regions(void)
{
   const char *dir = getenv("VIODUMP_DIR");
   if (!dir)
      return;
   size_t dl = strlen(dir);
   if (dl + 8 >= sizeof(dump_dir))
      return;
   ++dump_seq;
   snprintf(dump_dir, sizeof(dump_dir), "%s/s%04d", dir, dump_seq);
   mkdir(dump_dir, 0777);
   dir = dump_dir;
   size_t min = getenv("VIODUMP_MIN") ? strtoul(getenv("VIODUMP_MIN"), NULL, 0) : 512;
   size_t max = getenv("VIODUMP_MAX") ? strtoul(getenv("VIODUMP_MAX"), NULL, 0) : 4u << 20;

   FILE *maps = fopen("/proc/self/maps", "r");
   if (!maps)
      return;
   char index[512];
   snprintf(index, sizeof(index), "%s/index.txt", dir);
   FILE *idx = fopen(index, "a");
   if (!idx) {
      fclose(maps);
      return;
   }

   int mem = open("/proc/self/mem", O_RDONLY);
   if (mem < 0) {
      fclose(maps);
      fclose(idx);
      return;
   }

   char line[512];
   int n = 0;
   while (fgets(line, sizeof(line), maps)) {
      unsigned long start, end;
      char perms[8] = {0};
      if (sscanf(line, "%lx-%lx %7s", &start, &end, perms) != 3)
         continue;
      if (perms[0] != 'r' || perms[1] != 'w')
         continue;
      size_t len = end - start;
      if (len < min || len > max)
         continue;

      unsigned char *buf = malloc(len);
      if (!buf)
         continue;
      size_t got = 0;
      while (got < len) {
         ssize_t r = pread(mem, buf + got, len - got, (off_t)(start + got));
         if (r <= 0)
            break;
         got += (size_t)r;
      }
      if (got < min) {
         free(buf);
         continue;
      }
      char name[600];
      snprintf(name, sizeof(name), "%s/%.4d_%lx_%zu.bin", dir, n, start, got);
      FILE *f = fopen(name, "wb");
      if (f) {
         fwrite(buf, 1, got, f);
         fclose(f);
         fprintf(idx, "%d %lx %zu %s\n", n, start, got, perms);
         n++;
         dumped++;
      }
      free(buf);
   }
   fprintf(idx, "# ioctl#%lu dumped %d regions pid %d\n", call_count, n,
           (int)getpid());
   fclose(idx);
   close(mem);
   fclose(maps);
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

   call_count++;
   if (getenv("VIODUMP_PRINT"))
      fprintf(stderr, "[vio] #%lu fd=%d req=0x%lx size=%lu\n", call_count, fd,
              req, (unsigned long)((req >> 16) & 0x3fff));
   const char *at_s = getenv("VIODUMP_AT");
   unsigned long at = at_s ? strtoul(at_s, NULL, 0) : 0;
   const char *every_s = getenv("VIODUMP_EVERY");
   unsigned long every = every_s ? strtoul(every_s, NULL, 0) : 0;
   const char *maxd_s = getenv("VIODUMP_MAXDUMPS");
   unsigned long maxd = maxd_s ? strtoul(maxd_s, NULL, 0) : 8;
   if (at && call_count == at)
      dump_regions();
   else if (every && call_count >= every && (call_count % every) == 0 &&
            (unsigned long)dump_seq < maxd)
      dump_regions();

   if (getenv("VIODUMP_HEX") && arg) {
      unsigned long sz = (req >> 16) & 0x3fff;
      if (sz && sz <= 64) {
         unsigned char *b = (unsigned char *)arg;
         fprintf(stderr, "[hex] #%lu fd=%d req=0x%lx sz=%lu:", call_count, fd, req, sz);
         for (unsigned long i = 0; i < sz && i < 40; i++)
            fprintf(stderr, " %02x", b[i]);
         fprintf(stderr, "\n");
      }
   }

   return real_ioctl(fd, req, arg);
}