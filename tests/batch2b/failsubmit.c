#define _GNU_SOURCE
#include <dlfcn.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#include <linux/ioctl.h>
#include <stdio.h>

static int (*real_ioctl)(int, unsigned long, ...);
int ioctl(int fd, unsigned long req, ...)
{
   va_list ap; va_start(ap, req); void *arg = va_arg(ap, void *); va_end(ap);
   if (_IOC_TYPE(req) == 'd' && _IOC_NR(req) == 0x45) {  /* MWV207_SUBMIT */
      fprintf(stderr, "[failsubmit] fail submit ioctl fd=%d\n", fd);
      errno = EIO;
      return -1;
   }
   if (!real_ioctl) real_ioctl = dlsym(RTLD_NEXT, "ioctl");
   return real_ioctl(fd, req, arg);
}
