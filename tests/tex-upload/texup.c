/* glTexSubImage2D 吞吐与同步开销探针（1024x1024 RGBA8 = 4 MB/次）。 */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_ms(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}

int main(void)
{
   EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   eglInitialize(dpy, NULL, NULL);
   EGLint cfg_attr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE,
                         EGL_OPENGL_ES2_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
                         EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE };
   EGLConfig cfg; EGLint n;
   eglChooseConfig(dpy, cfg_attr, &cfg, 1, &n);
   EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
   EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
   EGLint pb[] = { EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE };
   EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, pb);
   eglMakeCurrent(dpy, surf, surf, ctx);
   printf("renderer: %s\n", glGetString(GL_RENDERER));

   GLuint tex;
   glGenTextures(1, &tex);
   glBindTexture(GL_TEXTURE_2D, tex);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1024, 1024, 0, GL_RGBA,
                GL_UNSIGNED_BYTE, NULL);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

   unsigned char *data = malloc(1024 * 1024 * 4);
   memset(data, 0x5a, 1024 * 1024 * 4);

   /* 1) 连续 50 次上传，只在最后同步 */
   double t0 = now_ms();
   for (int i = 0; i < 50; i++)
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1024, 1024, GL_RGBA,
                      GL_UNSIGNED_BYTE, data);
   glFinish();
   double t1 = now_ms();
   printf("50x upload, one sync : %.2f ms total, %.2f ms/次, %.2f GB/s\n",
          t1 - t0, (t1 - t0) / 50, 50 * 4.0 / (t1 - t0) * 1e3 / 1024);

   /* 2) 每次上传后同步（客户端逐帧上传的典型形态） */
   t0 = now_ms();
   for (int i = 0; i < 50; i++) {
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 1024, 1024, GL_RGBA,
                      GL_UNSIGNED_BYTE, data);
      glFinish();
   }
   t1 = now_ms();
   printf("50x upload, sync each: %.2f ms total, %.2f ms/次\n",
          t1 - t0, (t1 - t0) / 50);

   /* 3) 纯 CPU 拷贝同样大小，作为对照 */
   unsigned char *dst = malloc(1024 * 1024 * 4);
   t0 = now_ms();
   for (int i = 0; i < 50; i++)
      memcpy(dst, data, 1024 * 1024 * 4);
   t1 = now_ms();
   printf("50x memcpy 4MB      : %.2f ms total, %.2f ms/次\n", t1 - t0,
          (t1 - t0) / 50);

   /* 4) 只上传一小块（64x64），看固定开销 */
   t0 = now_ms();
   for (int i = 0; i < 200; i++)
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 64, 64, GL_RGBA,
                      GL_UNSIGNED_BYTE, data);
   glFinish();
   t1 = now_ms();
   printf("200x upload 64x64    : %.3f ms/次\n", (t1 - t0) / 200);
   return 0;
}
