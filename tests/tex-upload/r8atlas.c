/*
 * r8atlas.c —— 单通道（GL_R8 / GL_ALPHA 语义）字形图集的"小矩形上传 + 采样"正确性探针
 *
 * 背景：Firefox WebRender 在 GLES 上把字形放在 R8（Alpha8）图集里，用 2–20 px 的
 * 小矩形 glTexSubImage2D 更新。此前 tests/tex-upload/subrect.c 只覆盖了 RGBA8，
 * 火狐的乱码现象在关掉"平铺 + 影子"和"映射免等待"后依然复现，因此怀疑落到
 * 单通道纹理的平铺布局/采样路径上。
 *
 * 用法: r8atlas <size> [rounds]
 *   size   : 1024 / 2048 / 4096（图集边长）
 *   rounds : 轮数，默认 3
 *
 * 判据: 每轮 "RESULT: PASS (total mismatch=0)", 采样路径也必须是 0。
 */
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static unsigned long rng = 0x2545f491;
static unsigned rnd(void) { rng ^= rng << 13; rng ^= rng >> 7; rng ^= rng << 17; return (unsigned)(rng >> 8); }

static GLuint compile(GLenum type, const char *src)
{
   GLuint s = glCreateShader(type);
   glShaderSource(s, 1, &src, NULL);
   glCompileShader(s);
   GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
   if (!ok) { char log[512]; glGetShaderInfoLog(s, 512, NULL, log); fprintf(stderr, "shader: %s\n", log); exit(1); }
   return s;
}

int main(int argc, char **argv)
{
   const int SZ = (argc > 1) ? atoi(argv[1]) : 1024;
   const int ROUNDS = (argc > 2) ? atoi(argv[2]) : 3;

   EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   if (!eglInitialize(dpy, NULL, NULL)) { fprintf(stderr, "eglInitialize failed\n"); return 1; }
   EGLint cfg_attr[] = {
      EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
      EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
      EGL_NONE
   };
   EGLConfig cfg; EGLint n = 0;
   if (!eglChooseConfig(dpy, cfg_attr, &cfg, 1, &n) || !n) { fprintf(stderr, "chooseConfig failed\n"); return 1; }
   EGLint pb[] = { EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE };
   EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, pb);
   EGLint ctxattr[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
   EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxattr);
   if (!eglMakeCurrent(dpy, surf, surf, ctx)) { fprintf(stderr, "makeCurrent failed\n"); return 1; }

   printf("[r8atlas] %dx%d rounds=%d renderer=%s\n", SZ, SZ, ROUNDS, glGetString(GL_RENDERER));

   /* ---- 1. 单通道图集 + 小矩形上传 ---- */
   GLuint tex; glGenTextures(1, &tex); glBindTexture(GL_TEXTURE_2D, tex);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, SZ, SZ, 0, GL_RED, GL_UNSIGNED_BYTE, NULL);

   uint8_t *ref = malloc((size_t)SZ * SZ);
   uint8_t *got = malloc((size_t)SZ * SZ);
   memset(ref, 0xff, (size_t)SZ * SZ);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
   glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SZ, SZ, GL_RED, GL_UNSIGNED_BYTE, ref);

   GLuint fbo; glGenFramebuffers(1, &fbo);
   glBindFramebuffer(GL_FRAMEBUFFER, fbo);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
   if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      fprintf(stderr, "FBO incomplete (R8 color attachment)\n"); return 1;
   }

   int total_bad = 0;
   for (int r = 0; r < ROUNDS; r++) {
      for (int k = 0; k < 400; k++) {
         int w = 2 + (int)(rnd() % 19);
         int h = 2 + (int)(rnd() % 19);
         int x = (int)(rnd() % (SZ - w));
         int y = (int)(rnd() % (SZ - h));
         uint8_t pat[32 * 32];
         for (int py = 0; py < h; py++)
            for (int px = 0; px < w; px++)
               pat[py * w + px] = (uint8_t)((x + px) * 3 + (y + py) * 5 + r * 11 + k);
         glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, w, h, GL_RED, GL_UNSIGNED_BYTE, pat);
         for (int py = 0; py < h; py++)
            memcpy(ref + (size_t)(y + py) * SZ + x, pat + py * w, (size_t)w);
      }
      glFinish();
      glPixelStorei(GL_PACK_ALIGNMENT, 1);
      glReadPixels(0, 0, SZ, SZ, GL_RED, GL_UNSIGNED_BYTE, got);
      int bad = 0, first = -1;
      for (int i = 0; i < SZ * SZ; i++)
         if (got[i] != ref[i]) { if (first < 0) first = i; bad++; }
      printf("[r8atlas] round %d: mismatch=%d", r, bad);
      if (bad) printf(" first=(%d,%d) got=%u ref=%u", first % SZ, first / SZ, got[first], ref[first]);
      printf("\n");
      total_bad += bad;
   }

   /* ---- 2. 采样路径：R8 图集贴到全屏 quad，写进 RGBA8 FBO 再读回 ---- */
   const int SS = SZ;   /* 1:1 采样，便于逐像素对照 */
   GLuint rtex; glGenTextures(1, &rtex); glBindTexture(GL_TEXTURE_2D, rtex);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, SS, SS, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
   GLuint rfbo; glGenFramebuffers(1, &rfbo); glBindFramebuffer(GL_FRAMEBUFFER, rfbo);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rtex, 0);

   static const char *vs =
      "attribute vec2 p; varying vec2 uv; void main(){ uv = p * 0.5 + 0.5; gl_Position = vec4(p, 0.0, 1.0); }";
   static const char *fs =
      "precision highp float; varying vec2 uv; uniform sampler2D t;"
      "void main(){ float a = texture2D(t, uv).r; gl_FragColor = vec4(a, a, a, 1.0); }";
   GLuint prog = glCreateProgram();
   glAttachShader(prog, compile(GL_VERTEX_SHADER, vs));
   glAttachShader(prog, compile(GL_FRAGMENT_SHADER, fs));
   glBindAttribLocation(prog, 0, "p");
   glLinkProgram(prog);
   glUseProgram(prog);
   glUniform1i(glGetUniformLocation(prog, "t"), 0);

   static const float quad[] = { -1,-1, 1,-1, -1,1, 1,1 };
   GLuint vbo; glGenBuffers(1, &vbo); glBindBuffer(GL_ARRAY_BUFFER, vbo);
   glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
   glEnableVertexAttribArray(0);
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
   glViewport(0, 0, SS, SS);

   /* 图集按 1:1 采样到同尺寸 FBO */
   glBindFramebuffer(GL_FRAMEBUFFER, rfbo);
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D, tex);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glFinish();

   uint8_t *sgot = malloc((size_t)SS * SS * 4);
   glReadPixels(0, 0, SS, SS, GL_RGBA, GL_UNSIGNED_BYTE, sgot);
   int sbad = 0, sfirst = -1;
   for (int y = 0; y < SS; y++)
      for (int x = 0; x < SS; x++) {
         uint8_t want = ref[(size_t)y * SZ + x];
         uint8_t a = sgot[((size_t)y * SS + x) * 4];
         if (a != want) { if (sfirst < 0) sfirst = y * SS + x; sbad++; }
      }
   printf("[r8atlas] sample-path mismatch=%d", sbad);
   if (sbad) printf(" first=(%d,%d) got=%u ref=%u", sfirst % SS, sfirst / SS,
                    sgot[(size_t)sfirst * 4], ref[(size_t)(sfirst / SS) * SZ + (sfirst % SS)]);
   printf("\n");

   printf("RESULT: %s (total mismatch=%d)\n", (total_bad + sbad) ? "FAIL" : "PASS", total_bad + sbad);
   return (total_bad + sbad) ? 1 : 0;
}