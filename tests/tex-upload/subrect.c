/* subrect: 复现"字形图集小矩形上传"路径 —— 在非平铺对齐的任意小矩形上反复
 * glTexSubImage2D，然后整张读回与 CPU 端期望值逐像素比对。
 *
 * 背景：Firefox 的 WebRender 把字形栅格化进一张大图集纹理，然后按每个字形
 * 的小矩形做 glTexSubImage2D。驱动侧该路径是"linear staging -> PE blit ->
 * supertiled 纹理"，如果 PE 拷贝对非 tile 对齐的小矩形处理不对，图集里相邻
 * 字形会被写坏，屏幕上的文字就会出现局部乱码/条纹。
 *
 * 用法: subrect [sz] [rounds] [maxrect]
 * 判据: 最后一轮结束后 mismatch 必须为 0，否则打印前若干个坏像素坐标。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

static int SZ = 1024;
static int ROUNDS = 8;

static uint32_t rnd_state = 12345;
static uint32_t rnd(void)
{
   rnd_state = rnd_state * 1103515245u + 12345u;
   return rnd_state >> 8;
}

int main(int argc, char **argv)
{
   if (argc > 1) SZ = atoi(argv[1]);
   if (argc > 2) ROUNDS = atoi(argv[2]);

   EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   if (!eglInitialize(dpy, NULL, NULL)) { fprintf(stderr, "eglInitialize failed\n"); return 1; }
   EGLint cfg_attr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE,
      EGL_OPENGL_ES2_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8, EGL_NONE };
   EGLConfig cfg; EGLint n;
   if (!eglChooseConfig(dpy, cfg_attr, &cfg, 1, &n)) { fprintf(stderr, "chooseConfig failed\n"); return 1; }
   EGLint pb[] = { EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE };
   EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, pb);
   EGLint ctxattr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
   EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxattr);
   if (!eglMakeCurrent(dpy, surf, surf, ctx)) { fprintf(stderr, "makeCurrent failed\n"); return 1; }

   printf("[subrect] %dx%d rounds=%d renderer=%s\n", SZ, SZ, ROUNDS, glGetString(GL_RENDERER));

   GLuint tex;
   glGenTextures(1, &tex);
   glBindTexture(GL_TEXTURE_2D, tex);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SZ, SZ, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

   uint8_t *ref = malloc((size_t)SZ * SZ * 4);
   uint8_t *got = malloc((size_t)SZ * SZ * 4);
   memset(ref, 0xff, (size_t)SZ * SZ * 4);

   /* 整张上传一次基准图案 */
   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SZ, SZ, GL_RGBA, GL_UNSIGNED_BYTE, ref);

   /* FBO 用于读回 */
   GLuint fbo; glGenFramebuffers(1, &fbo);
   glBindFramebuffer(GL_FRAMEBUFFER, fbo);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
   if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      fprintf(stderr, "FBO incomplete\n"); return 1;
   }

   int total_bad = 0;
   for (int r = 0; r < ROUNDS; r++) {
      /* 小矩形上传：尺寸 2..20、坐标任意（故意不对齐） */
      for (int k = 0; k < 400; k++) {
         int w = 2 + (int)(rnd() % 19);
         int h = 2 + (int)(rnd() % 19);
         int x = (int)(rnd() % (SZ - w));
         int y = (int)(rnd() % (SZ - h));
         uint8_t pat[32 * 32 * 4];
         for (int py = 0; py < h; py++)
            for (int px = 0; px < w; px++) {
               uint8_t *p = pat + (py * w + px) * 4;
               p[0] = (uint8_t)(x + px); p[1] = (uint8_t)(y + py);
               p[2] = (uint8_t)(r * 7 + k); p[3] = 0xff;
            }
         glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RGBA, GL_UNSIGNED_BYTE, pat);
         /* 同步更新参照 */
         for (int py = 0; py < h; py++)
            memcpy(ref + ((size_t)(y + py) * SZ + x) * 4, pat + py * w * 4, (size_t)w * 4);
      }
      glFinish();
      glPixelStorei(GL_PACK_ALIGNMENT, 1);
      glReadPixels(0, 0, SZ, SZ, GL_RGBA, GL_UNSIGNED_BYTE, got);
      int bad = 0, first = -1;
      for (int i = 0; i < SZ * SZ * 4; i += 4) {
         if (got[i] != ref[i] || got[i + 1] != ref[i + 1] ||
             got[i + 2] != ref[i + 2] || got[i + 3] != ref[i + 3]) {
            if (first < 0) first = i / 4;
            bad++;
         }
      }
      printf("[subrect] round %d: mismatch=%d", r, bad);
      if (bad) printf(" first=(%d,%d) got=%02x%02x%02x%02x ref=%02x%02x%02x%02x",
                      first % SZ, first / SZ,
                      got[first * 4], got[first * 4 + 1], got[first * 4 + 2], got[first * 4 + 3],
                      ref[first * 4], ref[first * 4 + 1], ref[first * 4 + 2], ref[first * 4 + 3]);
      printf("\n");
      total_bad += bad;
   }

   /* 采样路径: 把图集 1:1 画进 FBO 再读回, 走的是 sampler 而非 FBO 附件的 readback */
   {
      static const char *vs =
         "attribute vec2 a; varying vec2 uv;"
         "void main(){ uv = a * 0.5 + 0.5; gl_Position = vec4(a, 0.0, 1.0); }";
      static const char *fs =
         "precision highp float; varying vec2 uv; uniform sampler2D t;"
         "void main(){ gl_FragColor = texture2D(t, uv); }";
      GLuint p = glCreateProgram();
      GLuint v = glCreateShader(GL_VERTEX_SHADER);
      glShaderSource(v, 1, &vs, NULL); glCompileShader(v); glAttachShader(p, v);
      GLuint f = glCreateShader(GL_FRAGMENT_SHADER);
      glShaderSource(f, 1, &fs, NULL); glCompileShader(f); glAttachShader(p, f);
      glBindAttribLocation(p, 0, "a"); glLinkProgram(p);
      GLint ok; glGetProgramiv(p, GL_LINK_STATUS, &ok);
      if (ok) {
         GLuint rt; glGenTextures(1, &rt);
         glBindTexture(GL_TEXTURE_2D, rt);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
         glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
         glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, SZ, SZ, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
         glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rt, 0);
         glViewport(0, 0, SZ, SZ);
         glUseProgram(p);
         glActiveTexture(GL_TEXTURE0);
         glBindTexture(GL_TEXTURE_2D, tex);
         glUniform1i(glGetUniformLocation(p, "t"), 0);
         static const float quad[] = { -1,-1, 1,-1, -1,1, 1,1 };
         glEnableVertexAttribArray(0);
         glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, quad);
         glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
         glFinish();
         glReadPixels(0, 0, SZ, SZ, GL_RGBA, GL_UNSIGNED_BYTE, got);
         int sbad = 0;
         for (int i = 0; i < SZ * SZ * 4; i++) if (got[i] != ref[i]) sbad++;
         printf("[subrect] sample-path mismatch=%d\n", sbad);
         total_bad += sbad;
      }
   }

   printf("RESULT: %s (total mismatch=%d)\n", total_bad ? "FAIL" : "PASS", total_bad);
   return total_bad ? 1 : 0;
}