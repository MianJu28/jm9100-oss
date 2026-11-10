/*
 * engines.c —— 通过 DRM_IOCTL_MWV207_INFO 读取 KMD 上报的引擎数量。
 *
 * 用途（N1 回归）：`mwv207_sched_init()` 的槽位布局改动后（7 个调度器：
 * 3d×2 | dec×2 | enc×1 | 2d×1 | dma×1），nr_3d/nr_dec/nr_enc/nr_2d/nr_dma
 * 必须与硬件实际能力一致 —— 例如 lite 版 JM9100 只有 3d0/dec0/2d0/dma0
 * （`mwv207_pipe_{3d,dec}_create()` 对 unit>0 且 lite 直接返回 NULL，
 * `mwv207_pipe_enc_create()` 在 lite 上返回 NULL），所以期望值是
 * "3d=1,dec=1,enc=0,2d=1,dma=1"。非 lite 的 MWV207 应是 3d=2,dec=2,enc=1。
 *
 * 编译: cc -O2 -I/usr/include/libdrm -o engines engines.c -ldrm
 * 用法: ./engines [期望值，如 "3d=1,dec=1,enc=0,2d=1,dma=1"]
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <xf86drm.h>

struct mwv207_info {
   uint32_t op;       /* 0 = 查询记录数; 1 = 按 key 查询单条 */
   uint32_t nr_recs;  /* op=0: 输出记录数; op=1: 输入 key */
   uint64_t recs;     /* op=0: 输出缓冲地址; op=1: 输出值 */
};

#define DRM_IOCTL_MWV207_INFO \
   DRM_IOWR(DRM_COMMAND_BASE + 0x00, struct mwv207_info)

static const char *const names[] = {
   "3d", "dec", "enc", "2d", "dma"
};

int main(int argc, char **argv)
{
   int fd = open("/dev/dri/renderD128", O_RDWR | O_CLOEXEC);
   if (fd < 0) {
      fprintf(stderr, "打开 /dev/dri/renderD128 失败: %s\n", strerror(errno));
      return 2;
   }

   struct mwv207_info q = { .op = 0, .nr_recs = 0, .recs = 0 };
   if (ioctl(fd, DRM_IOCTL_MWV207_INFO, &q) < 0) {
      fprintf(stderr, "查询记录数失败: %s\n", strerror(errno));
      return 3;
   }
   if (q.nr_recs < 5) {
      fprintf(stderr, "记录数不足: %u（期望 >= 5）\n", q.nr_recs);
      return 4;
   }

   unsigned caps[5] = { 0 };
   for (unsigned key = 0; key < 5; ++key) {
      struct mwv207_info one = { .op = 1, .nr_recs = key, .recs = 0 };
      if (ioctl(fd, DRM_IOCTL_MWV207_INFO, &one) < 0) {
         fprintf(stderr, "按 key 查询 %u 失败: %s\n", key, strerror(errno));
         close(fd);
         return 5;
      }
      caps[key] = (unsigned)one.recs;
   }
   close(fd);

   printf("引擎数量: ");
   for (unsigned i = 0; i < 5; ++i) {
      printf("%s=%u%s", names[i], caps[i], i < 4 ? " " : "\n");
   }

   if (argc > 1) {
      char got[128], want[128];
      snprintf(got, sizeof(got), "3d=%u,dec=%u,enc=%u,2d=%u,dma=%u",
               caps[0], caps[1], caps[2], caps[3], caps[4]);
      snprintf(want, sizeof(want), "%s", argv[1]);
      if (strcmp(got, want) != 0) {
         printf("  [FAIL] 期望 %s，实际 %s\n", want, got);
         return 1;
      }
      printf("  [PASS] 与期望一致\n");
   }
   return 0;
}
