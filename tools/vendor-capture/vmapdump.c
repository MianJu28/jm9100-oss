/*
 * vmapdump — LD_PRELOAD 工具：记录进程的 mmap 区间，退出时把各区间内容落盘。
 *
 * 用途：在厂商驱动栈上跑一次 JPEG 硬解，抓出厂商 UMD 映射进来的 GEM BO
 * 内容（544 B 的 JPEG 辅助表、ECS 流缓冲等），用于和我们自己的实现逐字节
 * 对比。厂商 UMD 的寄存器数组走内核 HAL，ioctl 拦不到内容；但 BO 是要
 * mmap 进来的，所以这里拦 mmap 再在退出时 dump。
 *
 * 用法：
 *   cc -O2 -shared -fPIC -o ~/dev-bin/vmapdump.so tools/vendor-capture/vmapdump.c -ldl
 *   VMAPDUMP_DIR=/tmp/vmd LD_PRELOAD=~/dev-bin/vmapdump.so \
 *     VMAPDUMP_MIN=256 VMAPDUMP_MAX=4194304 ffmpeg ...
 *
 * 输出：$VMAPDUMP_DIR/NNNN_addr-size.bin + index.txt（addr size prot fd offset）
 * 过滤：只 dump [VMAPDUMP_MIN, VMAPDUMP_MAX] 长度的区间（默认 256 B–4 MB），
 *       跳过 PROT_NONE 与 MAP_FIXED 的替换（新映射同样会记录）。
 */
#define _GNU_SOURCE
#include <dlfcn.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#define MAX_ENTRIES 4096

struct entry {
   void *addr;
   size_t len;
   int prot;
   int fd;
   long offset;
};

static struct entry entries[MAX_ENTRIES];
static int nr_entries;
static int inited;
static int dumping;

static void *(*real_mmap)(void *, size_t, int, int, int, long);
static int (*real_munmap)(void *, size_t);

static void
init(void)
{
   if (inited)
      return;
   inited = 1;
   real_mmap = dlsym(RTLD_NEXT, "mmap");
   real_munmap = dlsym(RTLD_NEXT, "munmap");
}

static void
record(void *addr, size_t len, int prot, int fd, long off)
{
   if (addr == MAP_FAILED || nr_entries >= MAX_ENTRIES)
      return;
   entries[nr_entries].addr = addr;
   entries[nr_entries].len = len;
   entries[nr_entries].prot = prot;
   entries[nr_entries].fd = fd;
   entries[nr_entries].offset = off;
   nr_entries++;
   if (getenv("VMAPDUMP_VERBOSE"))
      fprintf(stderr, "[vmapdump] %p +%zu prot=%d fd=%d off=%ld\n", addr, len,
              prot, fd, off);
}

void *
mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off)
{
   init();
   void *r = real_mmap(addr, len, prot, flags, fd, off);
   record(r, len, prot, fd, (long)off);
   return r;
}

void *
mmap64(void *addr, size_t len, int prot, int flags, int fd, off64_t off)
{
   init();
   void *(*real)(void *, size_t, int, int, int, off64_t) =
      dlsym(RTLD_NEXT, "mmap64");
   void *r = real(addr, len, prot, flags, fd, off);
   record(r, len, prot, fd, (long)off);
   return r;
}

static int dumped_count;

static void
dump_one(struct entry *e)
{
   const char *dir = getenv("VMAPDUMP_DIR");
   if (!dir || !e->addr || e->len < 64)
      return;
   size_t min = getenv("VMAPDUMP_MIN") ? strtoul(getenv("VMAPDUMP_MIN"), NULL, 0) : 256;
   size_t max = getenv("VMAPDUMP_MAX") ? strtoul(getenv("VMAPDUMP_MAX"), NULL, 0) : 4u << 20;
   if (e->len < min || e->len > max || e->prot == PROT_NONE)
      return;
   char index[512], name[600];
   snprintf(index, sizeof(index), "%s/index.txt", dir);
   FILE *idx = fopen(index, "a");
   if (!idx)
      return;
   snprintf(name, sizeof(name), "%s/%.4d_%p_%zu.bin", dir, dumped_count,
            e->addr, e->len);
   FILE *f = fopen(name, "wb");
   if (f) {
      size_t w = fwrite(e->addr, 1, e->len, f);
      fprintf(idx, "%d %p %zu %d %d %ld w=%zu\n", dumped_count, e->addr,
              e->len, e->prot, e->fd, e->offset, w);
      fclose(f);
      dumped_count++;
   }
   fclose(idx);
}

int
munmap(void *addr, size_t len)
{
   init();
   for (int i = 0; i < nr_entries; i++) {
      if (entries[i].addr == addr) {
         dump_one(&entries[i]);
         entries[i].addr = NULL;
         break;
      }
   }
   return real_munmap(addr, len);
}

static void
dump_all(void)
{
   if (dumping)
      return;
   dumping = 1;

   const char *dir = getenv("VMAPDUMP_DIR");
   if (!dir)
      return;
   size_t min = getenv("VMAPDUMP_MIN") ? strtoul(getenv("VMAPDUMP_MIN"), NULL, 0) : 256;
   size_t max = getenv("VMAPDUMP_MAX") ? strtoul(getenv("VMAPDUMP_MAX"), NULL, 0) : 4u << 20;

   char path[512], name[600];
   snprintf(path, sizeof(path), "%s/index.txt", dir);
   FILE *idx = fopen(path, "a");
   if (!idx)
      return;

   int n = 0;
   for (int i = 0; i < nr_entries; i++) {
      if (!entries[i].addr || entries[i].len < min || entries[i].len > max)
         continue;
      if (entries[i].prot == PROT_NONE)
         continue;
      snprintf(name, sizeof(name), "%s/%.4d_%p_%zu.bin", dir, n,
               entries[i].addr, entries[i].len);
      FILE *f = fopen(name, "wb");
      if (!f)
         continue;
      /* 直接读：区间已映射可读（PROT_WRITE 才有 PROT_READ 则跳过） */
      if ((entries[i].prot & PROT_READ) || (entries[i].prot & PROT_WRITE)) {
         size_t w = fwrite(entries[i].addr, 1, entries[i].len, f);
         fprintf(idx, "%d %p %zu %d %d %ld w=%zu\n", n, entries[i].addr,
                 entries[i].len, entries[i].prot, entries[i].fd,
                 entries[i].offset, w);
      }
      fclose(f);
      n++;
   }
   fprintf(idx, "# pid %d total %d\n", (int)getpid(), n);
   fclose(idx);
}

__attribute__((destructor)) static void
vmapdump_fini(void)
{
   dump_all();
}