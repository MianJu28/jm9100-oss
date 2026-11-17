/*
 * copysub.c —— 图集"复制式更新"路径的正确性探针
 *
 * 背景：Firefox 更新字形图集除了 glTexSubImage2D，还可能用
 *   - glCopyTexSubImage2D（把渲染好的字形从临时纹理/FBO 拷进图集，任意偏移）
 *   - glCopyImageSubData（GLES3 的纹理间块拷贝）
 * 这两条路径都要做"非对齐子矩形 + 相邻矩形"的偏移换算，一旦 pitch/tile 换算错，
 * 表现就是文字被相邻图集行覆盖（竖直条纹/串字）—— 正是火狐乱码的形态。
 *
 * 用法: copysub <size> [rounds]
 * 判据: 每轮 "round N: mismatch=0"，最后 "RESULT: PASS (total mismatch=0)"。
 */
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static unsigned long rng = 0x9e3779b97f4a7c15UL;
static unsigned rnd(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (unsigned)(rng >> 8); }

static GLuint mktex(int sz)
{
   GLuint t; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, sz, sz, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);
   return t;
}

int main(int argc, char **argv)
{
   const int SZ = (argc > 1) ? atoi(argv[1]) : 2048;
   const int ROUNDS = (argc > 2) ? atoi(argv[2]) : 2;

   EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   if (!eglInitialize(dpy, NULL, NULL)) { fprintf(stderr, "eglInitialize failed\n"); return 1; }
   EGLint cfg_attr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                         EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE };
   EGLConfig cfg; EGLint n = 0;
   if (!eglChooseConfig(dpy, cfg_attr, &cfg, 1, &n) || !n) { fprintf(stderr, "chooseConfig failed\n"); return 1; }
   EGLint pb[] = { EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE };
   EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, pb);
   EGLint ca[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
   EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ca);
   if (!eglMakeCurrent(dpy, surf, surf, ctx)) { fprintf(stderr, "makeCurrent failed\n"); return 1; }

   printf("[copysub] %dx%d rounds=%d renderer=%s\n", SZ, SZ, ROUNDS, glGetString(GL_RENDERER));
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
   glPixelStorei(GL_PACK_ALIGNMENT, 1);

   GLuint atlas = mktex(SZ);
   /* 先把图集清零（否则未覆盖区域是未定义内容，无法判定） */
   {
      uint8_t *zero = calloc((size_t)SZ * SZ, 1);
      glBindTexture(GL_TEXTURE_2D, atlas);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SZ, SZ, GL_RED, GL_UNSIGNED_BYTE, zero);
      free(zero);
   }
   GLuint src = mktex(SZ);
   uint8_t *want = calloc((size_t)SZ * SZ, 1);
   uint8_t *got = malloc((size_t)SZ * SZ);
   uint8_t *srcbuf = malloc((size_t)SZ * SZ);

   GLuint sfbo, afbo;
   glGenFramebuffers(1, &sfbo); glGenFramebuffers(1, &afbo);
   glBindFramebuffer(GL_FRAMEBUFFER, sfbo);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, src, 0);
   if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { fprintf(stderr, "src FBO incomplete\n"); return 1; }
   glBindFramebuffer(GL_FRAMEBUFFER, afbo);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, atlas, 0);
   if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { fprintf(stderr, "atlas FBO incomplete\n"); return 1; }

   int total = 0;
   for (int r = 0; r < ROUNDS; r++) {
      /* 源纹理：CPU 生成确定图案后整张上传（这条路径已由 r8atlas 证明正确） */
      for (int y = 0; y < SZ; y++)
         for (int x = 0; x < SZ; x++)
            srcbuf[(size_t)y * SZ + x] = (uint8_t)((x * 7 + y * 13 + r * 29) & 0xff);
      glBindTexture(GL_TEXTURE_2D, src);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SZ, SZ, GL_RED, GL_UNSIGNED_BYTE, srcbuf);

      /* 用 glCopyTexSubImage2D 把任意小矩形从 src 复制到 atlas */
      for (int k = 0; k < 200; k++) {
         int w = 2 + (int)(rnd() % 19), h = 2 + (int)(rnd() % 19);
         int sx = (int)(rnd() % (SZ - w)), sy = (int)(rnd() % (SZ - h));
         int dx = (int)(rnd() % (SZ - w)), dy = (int)(rnd() % (SZ - h));
         glBindFramebuffer(GL_READ_FRAMEBUFFER, sfbo);
         glBindTexture(GL_TEXTURE_2D, atlas);
         glCopyTexSubImage2D(GL_TEXTURE_2D, 0, dx, dy, sx, sy, w, h);
         for (int py = 0; py < h; py++)
            memcpy(want + (size_t)(dy + py) * SZ + dx, srcbuf + (size_t)(sy + py) * SZ + sx, (size_t)w);
      }
      glFinish();
      GLenum err = glGetError();
      if (err != GL_NO_ERROR) printf("[copysub] round %d GL error 0x%x after copies\n", r, err);

      glBindFramebuffer(GL_FRAMEBUFFER, afbo);
      glReadPixels(0, 0, SZ, SZ, GL_RED, GL_UNSIGNED_BYTE, got);
      { int hist[257] = {0}; for (int i = 0; i < SZ*SZ; i++) hist[got[i]]++;
        printf("[copysub] round %d: got[0]=%u hist(0)=%d hist(249)=%d\n", r, got[0], hist[0], hist[249]); }
      int bad = 0, first = -1;
      for (int i = 0; i < SZ * SZ; i++)
         if (got[i] != want[i]) { if (first < 0) first = i; bad++; }
      printf("[copysub] round %d: mismatch=%d", r, bad);
      if (bad) printf(" first=(%d,%d) got=%u want=%u", first % SZ, first / SZ, got[first], want[first]);
      printf("\n");
      total += bad;
   }

   printf("RESULT: %s (total mismatch=%d)\n", total ? "FAIL" : "PASS", total);
   return total ? 1 : 0;
}