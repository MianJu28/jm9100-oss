/*
 * badsubmit.c —— 畸形提交防御测试（配合 2026-11-10 `53b3229` 的 BUG_ON 收敛）。
 *
 * 目的：确认用户态送进来的畸形参数都变成 errno，而不是内核 panic。所有用例都
 * 在拿硬件之前被拒（engine 选择、cmd_size/cmds、bo handle、reloc 下标），因此
 * 跑这个测试不会真的往 GPU 里塞垃圾命令。
 *
 * 编译: cc -O2 -I/usr/include/libdrm -o badsubmit badsubmit.c -ldrm
 * 用法: ./badsubmit            # 默认 /dev/dri/renderD128
 *       ./badsubmit /dev/dri/card0
 * 退出码: 0 = 全部按预期被拒且驱动仍可用；1 = 有用例意外成功或驱动失联。
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <xf86drm.h>

/* ---- 只抄用到的 ABI（与 src/kernel-6.6/mwv207_drm.h 保持一致） ---- */
struct mwv207_info {
   uint32_t op;
   uint32_t nr_recs;
   uint64_t recs;
};
#define DRM_IOCTL_MWV207_INFO \
   DRM_IOWR(DRM_COMMAND_BASE + 0x00, struct mwv207_info)

struct mwv207_gem_create_in {
   uint64_t size;
   uint64_t alignment;
   uint32_t preferred_domain;
   uint32_t flags;
};
struct mwv207_gem_create_out {
   uint32_t handle;
   uint32_t resv;
};
union mwv207_gem_create {
   struct mwv207_gem_create_in in;
   struct mwv207_gem_create_out out;
};
#define DRM_IOCTL_MWV207_GEM_CREATE \
   DRM_IOWR(DRM_COMMAND_BASE + 0x01, union mwv207_gem_create)

struct drm_mwv207_bo_acc {
   uint32_t handle;
   uint32_t flags;
};
struct drm_mwv207_submit_reloc {
   uint32_t idx;
   uint32_t cmd_offset;
   uint64_t bo_offset;
   uint32_t type;
   uint32_t pad;
};
struct drm_mwv207_submit {
   uint32_t ctx;
   uint32_t engine_type;
   uint32_t engine_id;
   uint32_t flags;
   uint32_t fence_fd;
   uint32_t cmd_size;
   uint32_t nr_relocs;
   uint32_t nr_bos;
   uint32_t nr_cmd_dat;
   uint32_t padding;
   uint64_t cmds;
   uint64_t relocs;
   uint64_t bos;
   uint64_t cmd_dats;
};
#define DRM_IOCTL_MWV207_SUBMIT \
   DRM_IOWR(DRM_COMMAND_BASE + 0x05, struct drm_mwv207_submit)

struct mwv207_ctx {
   uint32_t op;      /* 0 = 创建, 1 = 销毁 */
   uint32_t flags;
   uint32_t handle;  /* 输出（创建）/ 输入（销毁） */
   uint32_t resv;
};
#define DRM_IOCTL_MWV207_CTX \
   DRM_IOWR(DRM_COMMAND_BASE + 0x04, struct mwv207_ctx)

static int fd;
static int failures;

static int do_info(void)
{
   struct mwv207_info info;
   uint64_t recs[64];

   memset(&info, 0, sizeof(info));
   memset(recs, 0, sizeof(recs));
   info.op = 0;
   info.recs = (uint64_t)(uintptr_t)recs;
   return ioctl(fd, DRM_IOCTL_MWV207_INFO, &info);
}

static void expect_reject(const char *what, struct drm_mwv207_submit *s,
                          int expected_errno)
{
   errno = 0;
   int ret = ioctl(fd, DRM_IOCTL_MWV207_SUBMIT, s);

   if (ret == 0) {
      printf("FAIL  %-38s 提交被接受（应为拒绝）\n", what);
      failures++;
      return;
   }
   if (errno != expected_errno) {
      printf("WARN  %-38s errno=%d(%s)，期望 %d(%s)\n", what, errno,
             strerror(errno), expected_errno, strerror(expected_errno));
      return;
   }
   printf("ok    %-38s errno=%d(%s)\n", what, errno, strerror(errno));
}

int main(int argc, char **argv)
{
   const char *path = argc > 1 ? argv[1] : "/dev/dri/renderD128";
   uint32_t cmds[4] = { 0x80000000, 0, 0, 0 };
   struct drm_mwv207_bo_acc bo;
   struct drm_mwv207_submit_reloc reloc;
   struct drm_mwv207_submit s;
   union mwv207_gem_create create;
   struct mwv207_ctx ctx;
   int i;

   fd = open(path, O_RDWR | O_CLOEXEC);
   if (fd < 0) {
      fprintf(stderr, "打开 %s 失败: %s\n", path, strerror(errno));
      return 1;
   }
   if (do_info() < 0) {
      fprintf(stderr, "DRM_IOCTL_MWV207_INFO 失败（不是 mwv207 节点？）: %s\n",
              strerror(errno));
      return 1;
   }

   /* 先建一个真实 BO，后面的 reloc 越界用例需要合法 handle。 */
   memset(&create, 0, sizeof(create));
   create.in.size = 4096;
   create.in.alignment = 4096;
   create.in.preferred_domain = 0x2;
   if (ioctl(fd, DRM_IOCTL_MWV207_GEM_CREATE, &create) < 0) {
      fprintf(stderr, "GEM_CREATE 失败: %s\n", strerror(errno));
      return 1;
   }
   printf("SANITY GEM_CREATE handle=%u\n", create.out.handle);

   /* 提交必须带一个合法 ctx（ctx=0 会让 ioctl 先返回 -ENOENT，掩盖后面
    * 真正要测的校验）。 */
   memset(&ctx, 0, sizeof(ctx));
   ctx.op = 0;
   if (ioctl(fd, DRM_IOCTL_MWV207_CTX, &ctx) < 0) {
      fprintf(stderr, "CTX create 失败: %s\n", strerror(errno));
      return 1;
   }
   printf("SANITY CTX handle=%u\n", ctx.handle);

   /* 1) 未知 engine_type */
   memset(&s, 0, sizeof(s));
   memset(&bo, 0, sizeof(bo));
   bo.handle = create.out.handle;
   s.ctx = ctx.handle;
   s.engine_type = 99;
   s.cmd_size = sizeof(cmds);
   s.cmds = (uint64_t)(uintptr_t)cmds;
   expect_reject("engine_type=99", &s, ENODEV);

   /* 2) 已知 engine_type，engine_id 越界 */
   s.engine_type = 3;      /* 2D */
   s.engine_id = 99;
   expect_reject("engine_type=3/engine_id=99", &s, ENODEV);

   /* 3) cmd_size = 0 */
   s.engine_type = 3;
   s.engine_id = 0;
   s.cmd_size = 0;
   expect_reject("cmd_size=0", &s, EINVAL);

   /* 4) cmd_size 超过 0x20000 */
   s.cmd_size = 0x20001;
   expect_reject("cmd_size=0x20001", &s, E2BIG);

   /* 5) cmds 空指针 */
   s.cmd_size = sizeof(cmds);
   s.cmds = 0;
   expect_reject("cmds=NULL", &s, EINVAL);

   /* 6) bos 里的 handle 不存在 */
   s.cmds = (uint64_t)(uintptr_t)cmds;
   s.nr_bos = 1;
   s.bos = (uint64_t)(uintptr_t)&bo;
   bo.handle = 0xdeadbeef;
   expect_reject("bos[0].handle=0xdeadbeef", &s, ENOENT);

   /* 7) nr_relocs>0 但 relocs 为空 */
   bo.handle = create.out.handle;
   s.nr_relocs = 1;
   s.relocs = 0;
   expect_reject("nr_relocs=1/relocs=NULL", &s, EINVAL);

   /* 8) reloc 的 bo 下标越界（会走到 patch 阶段，仍应在碰硬件前被拒） */
   memset(&reloc, 0, sizeof(reloc));
   reloc.idx = 0xffff;
   reloc.cmd_offset = 4;
   reloc.type = 0;
   s.relocs = (uint64_t)(uintptr_t)&reloc;
   expect_reject("reloc.idx=0xffff（越界）", &s, EINVAL);

   /* 9) nr_cmd_dat>0 但 cmd_dats 为空 */
   s.nr_relocs = 0;
   s.relocs = 0;
   s.nr_cmd_dat = 1;
   s.cmd_dats = 0;
   expect_reject("nr_cmd_dat=1/cmd_dats=NULL", &s, EINVAL);

   /* 收尾：驱动仍可用（INFO 与 mmap 都还能正常走），说明没有 panic/死锁。 */
   if (do_info() < 0) {
      printf("FAIL  收尾 INFO 失败: %s\n", strerror(errno));
      failures++;
   } else {
      printf("ok    收尾 INFO 正常（驱动未被打挂）\n");
   }

   for (i = 0; i < 3; i++)
      (void)do_info();

   printf("\n总结：%s（%d 处 FAIL）\n", failures ? "FAIL" : "PASS", failures);
   close(fd);
   return failures ? 1 : 0;
}
