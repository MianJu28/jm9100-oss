/* dualblend.c —— 双源混合(dual source blending)正确性探针。
 *
 * 硬件混合单元没有 SRC1 因子, 驱动把双源混合降级进片元着色器(见
 * src/mwv207-umd/drivers/mwv207/nir/mwv207_lower_dual.c)。这个探针把
 * 几种混合状态依次渲染到预置底色、64x64 的 RGBA8 FBO, 读回后与 CPU 按
 * GL 规格算出的期望值逐像素/逐通道比较。
 *
 * 覆盖点:
 *   - SRC1_COLOR / ONE_MINUS_SRC1_ALPHA (火狐 LCD 文字用的形态)
 *   - SRC1_ALPHA / ONE
 *   - 非双源形态(control): SRC_ALPHA / ONE_MINUS_SRC_ALPHA, 以及 SUBTRACT
 *   - 状态切换顺序: 先跑一次非双源, 再跑双源(专门抓"着色器缓存键没带混合
 *     参数 -> 复用旧二进制"这种错误)
 *   - 最后把每例像素 dump 到文件, 便于驱动 A/B 逐字节比较
 *
 * 用法: dualblend [dump.txt]     (dump 时可与 LIBGL_ALWAYS_SOFTWARE=1 的
 *                                  llvmpipe 输出 cmp 比对)
 * 编译: cc -O2 -o dualblend tests/gl-probe/dualblend.c -lEGL -lGLESv2
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>

#define SZ 64
#define SRC1_ALPHA_EXT 0x8589

static const char *vs_src =
   "#version 300 es\n"
   "void main(void){\n"
   "  vec2 p = vec2((gl_VertexID == 1) ? 3.0 : -1.0,\n"
   "                (gl_VertexID == 2) ? 3.0 : -1.0);\n"
   "  gl_Position = vec4(p, 0.0, 1.0);\n"
   "}\n";

static const char *fs_dual_src =
   "#version 300 es\n"
   "#extension GL_EXT_blend_func_extended : require\n"
   "precision highp float;\n"
   "layout(location = 0, index = 0) out highp vec4 color_out;\n"
   "layout(location = 0, index = 1) out highp vec4 mask_out;\n"
   "void main(void){\n"
   "  color_out = vec4(0.2, 0.4, 0.6, 0.8);\n"
   "  mask_out  = vec4(0.9, 0.5, 0.1, 0.3);\n"
   "}\n";

static const char *fs_single_src =
   "#version 300 es\n"
   "precision highp float;\n"
   "layout(location = 0) out highp vec4 color_out;\n"
   "void main(void){\n"
   "  color_out = vec4(0.2, 0.4, 0.6, 0.8);\n"
   "}\n";

struct case_desc {
   const char *name;
   int dual;
   int striped;      /* 底色按行变化, 用来验 "采样到的 dst 是不是本像素" */
   GLenum rs, rd, as, ad, rf, af;
};

static const struct case_desc cases[] = {
   /* 顺序故意打乱: 先一个非双源 control, 再双源, 再非双源, 再双源 */
   { "control_src_alpha",   0, 0, GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA,
                                GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_FUNC_ADD, GL_FUNC_ADD },
   { "dual_one_invsrc1c",   1, 0, GL_ONE, GL_ONE_MINUS_SRC1_COLOR_EXT,
                                GL_ONE, GL_ONE_MINUS_SRC1_COLOR_EXT, GL_FUNC_ADD, GL_FUNC_ADD },
   { "control_subtract",    0, 0, GL_SRC_ALPHA, GL_ONE,
                                GL_SRC_ALPHA, GL_ONE, GL_FUNC_SUBTRACT, GL_FUNC_ADD },
   { "dual_src1c_invsrc1a", 1, 0, GL_SRC1_COLOR_EXT, GL_ONE_MINUS_SRC1_ALPHA_EXT,
                                GL_SRC1_ALPHA_EXT, GL_ONE_MINUS_SRC1_ALPHA_EXT,
                                GL_FUNC_ADD, GL_FUNC_ADD },
   { "dual_src1a_one",      1, 0, GL_SRC1_ALPHA_EXT, GL_ONE,
                                GL_SRC1_ALPHA_EXT, GL_ONE, GL_FUNC_ADD, GL_FUNC_ADD },
   { "dual_min",            1, 0, GL_ONE, GL_ONE, GL_ONE, GL_ONE, GL_MIN, GL_MIN },
   /* dst 直通: src 因子 ZERO + dst 因子 ONE, 输出必须与预置底色逐字节相同 ——
    * 若 lower_dual 采到的 dst 不是本像素(方向反了/差一行), 这里立刻暴露 */
   { "dst_passthrough",     1, 1, GL_ZERO, GL_ONE, GL_ZERO, GL_ONE, GL_FUNC_ADD, GL_FUNC_ADD },
};

#define NCASES (int)(sizeof(cases)/sizeof(cases[0]))

static GLuint mk_shader(GLenum t, const char *src)
{
   GLuint s = glCreateShader(t);
   glShaderSource(s, 1, &src, NULL);
   glCompileShader(s);
   GLint ok = 0;
   glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
   if (!ok) {
      char log[4096];
      glGetShaderInfoLog(s, sizeof(log), NULL, log);
      fprintf(stderr, "shader compile failed (%s): %s\n",
              t == GL_VERTEX_SHADER ? "vs" : "fs", log);
      exit(2);
   }
   return s;
}

static GLuint mk_prog(const char *fs)
{
   GLuint p = glCreateProgram();
   glAttachShader(p, mk_shader(GL_VERTEX_SHADER, vs_src));
   glAttachShader(p, mk_shader(GL_FRAGMENT_SHADER, fs));
   glLinkProgram(p);
   GLint ok = 0;
   glGetProgramiv(p, GL_LINK_STATUS, &ok);
   if (!ok) {
      char log[4096];
      glGetProgramInfoLog(p, sizeof(log), NULL, log);
      fprintf(stderr, "link failed: %s\n", log);
      exit(2);
   }
   return p;
}

static float clampf(float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); }

static void factor_val(GLenum f, const float src[4], const float src1[4],
                       const float dst[4], float out_rgb[3], float *out_a)
{
   switch (f) {
   case GL_ZERO: for (int i = 0; i < 3; i++) out_rgb[i] = 0; *out_a = 0; break;
   case GL_ONE:  for (int i = 0; i < 3; i++) out_rgb[i] = 1; *out_a = 1; break;
   case GL_SRC_COLOR: for (int i = 0; i < 3; i++) out_rgb[i] = src[i]; *out_a = src[3]; break;
   case GL_ONE_MINUS_SRC_COLOR: for (int i = 0; i < 3; i++) out_rgb[i] = 1 - src[i]; *out_a = 1 - src[3]; break;
   case GL_SRC_ALPHA: for (int i = 0; i < 3; i++) out_rgb[i] = src[3]; *out_a = src[3]; break;
   case GL_ONE_MINUS_SRC_ALPHA: for (int i = 0; i < 3; i++) out_rgb[i] = 1 - src[3]; *out_a = 1 - src[3]; break;
   case GL_DST_ALPHA: for (int i = 0; i < 3; i++) out_rgb[i] = dst[3]; *out_a = dst[3]; break;
   case GL_ONE_MINUS_DST_ALPHA: for (int i = 0; i < 3; i++) out_rgb[i] = 1 - dst[3]; *out_a = 1 - dst[3]; break;
   case GL_DST_COLOR: for (int i = 0; i < 3; i++) out_rgb[i] = dst[i]; *out_a = dst[3]; break;
   case GL_ONE_MINUS_DST_COLOR: for (int i = 0; i < 3; i++) out_rgb[i] = 1 - dst[i]; *out_a = 1 - dst[3]; break;
   case GL_SRC1_COLOR_EXT: for (int i = 0; i < 3; i++) out_rgb[i] = src1[i]; *out_a = src1[3]; break;
   case GL_ONE_MINUS_SRC1_COLOR_EXT: for (int i = 0; i < 3; i++) out_rgb[i] = 1 - src1[i]; *out_a = 1 - src1[3]; break;
   case SRC1_ALPHA_EXT: for (int i = 0; i < 3; i++) out_rgb[i] = src1[3]; *out_a = src1[3]; break;
   case GL_ONE_MINUS_SRC1_ALPHA_EXT: for (int i = 0; i < 3; i++) out_rgb[i] = 1 - src1[3]; *out_a = 1 - src1[3]; break;
   default:
      fprintf(stderr, "unsupported factor 0x%x\n", f);
      exit(2);
   }
}

static void factor_val_sep(GLenum f, const float src[4], const float src1[4],
                           const float dst[4], float out[4])
{
   float rgb[3], a;
   factor_val(f, src, src1, dst, rgb, &a);
   /* 对 rgb 因子, alpha 通道按 GL 规格取该因子的 alpha 分量; 对 alpha 因子
    * 需要单独调用(用 out_a) */
   out[0] = rgb[0]; out[1] = rgb[1]; out[2] = rgb[2]; out[3] = a;
}

static float blend_apply(GLenum func, float s, float d)
{
   switch (func) {
   case GL_FUNC_ADD: return s + d;
   case GL_FUNC_SUBTRACT: return s - d;
   case GL_FUNC_REVERSE_SUBTRACT: return d - s;
   case GL_MIN: return s < d ? s : d;
   case GL_MAX: return s > d ? s : d;
   }
   return s + d;
}

int main(int argc, char **argv)
{
   FILE *dump = NULL;
   if (argc > 1) dump = fopen(argv[1], "wb");

   EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   eglInitialize(dpy, NULL, NULL);
   EGLint cfg_attr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                         EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                         EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                         EGL_ALPHA_SIZE, 8, EGL_NONE };
   EGLConfig cfg; EGLint n = 0;
   eglChooseConfig(dpy, cfg_attr, &cfg, 1, &n);
   EGLint pb_attr[] = { EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE };
   EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, pb_attr);
   EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
   EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
   if (!eglMakeCurrent(dpy, surf, surf, ctx)) { fprintf(stderr, "no context\n"); return 2; }

   GLint max_dual = 0;
   glGetIntegerv(GL_MAX_DUAL_SOURCE_DRAW_BUFFERS_EXT, &max_dual);
   printf("GL_MAX_DUAL_SOURCE_DRAW_BUFFERS = %d (%s)\n", max_dual, glGetString(GL_RENDERER));
   if (max_dual < 1) { printf("双源混合未启用, 跳过\n"); return 0; }

   GLuint tex, fbo;
   glGenTextures(1, &tex);
   glBindTexture(GL_TEXTURE_2D, tex);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, SZ, SZ, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glGenFramebuffers(1, &fbo);
   glBindFramebuffer(GL_FRAMEBUFFER, fbo);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
   if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      fprintf(stderr, "fbo incomplete\n"); return 2;
   }

   GLuint prog_dual = mk_prog(fs_dual_src);
   GLuint prog_single = mk_prog(fs_single_src);

   const float src_c[4] = { 0.2f, 0.4f, 0.6f, 0.8f };
   const float src1_c[4] = { 0.9f, 0.5f, 0.1f, 0.3f };
   const float dst_c[4] = { 0.25f, 0.5f, 0.75f, 0.5f };
   unsigned char dst_bytes[4] = { 64, 128, 191, 128 };   /* 0.25/0.5/0.75/0.5 */

   unsigned char *px = malloc(SZ * SZ * 4);
   int fails = 0;

   for (int c = 0; c < NCASES; c++) {
      /* 预置底色 */
      unsigned char *buf = malloc(SZ * SZ * 4);
      for (int y = 0; y < SZ; y++)
         for (int x = 0; x < SZ; x++) {
            unsigned char *p = buf + (y * SZ + x) * 4;
            if (cases[c].striped) {
               p[0] = (unsigned char)(y * 4);
               p[1] = (unsigned char)(255 - y * 4);
               p[2] = (unsigned char)((y & 1) ? 255 : 0);
               p[3] = 200;
            } else {
               memcpy(p, dst_bytes, 4);
            }
         }
      glBindTexture(GL_TEXTURE_2D, tex);
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SZ, SZ, GL_RGBA, GL_UNSIGNED_BYTE, buf);
      free(buf);

      glViewport(0, 0, SZ, SZ);
      glUseProgram(cases[c].dual ? prog_dual : prog_single);
      glEnable(GL_BLEND);
      glBlendFuncSeparate(cases[c].rs, cases[c].rd, cases[c].as, cases[c].ad);
      glBlendEquationSeparate(cases[c].rf, cases[c].af);
      glDrawArrays(GL_TRIANGLES, 0, 3);
      glFinish();
      { GLenum e = glGetError();
        if (e != GL_NO_ERROR) printf("%-22s GL error 0x%x\n", cases[c].name, e); }
      glReadPixels(0, 0, SZ, SZ, GL_RGBA, GL_UNSIGNED_BYTE, px);

      /* 期望值 */
      float sf[4], df[4], sa[4], da[4];
      const float *dup = cases[c].dual ? src1_c : NULL;
      factor_val_sep(cases[c].rs, src_c, dup ? dup : src_c, dst_c, sf);
      factor_val_sep(cases[c].rd, src_c, dup ? dup : src_c, dst_c, df);
      factor_val_sep(cases[c].as, src_c, dup ? dup : src_c, dst_c, sa);
      factor_val_sep(cases[c].ad, src_c, dup ? dup : src_c, dst_c, da);
      float expct[4];
      for (int i = 0; i < 3; i++) {
         float s = src_c[i], d = dst_c[i];
         if (cases[c].rf == GL_MIN || cases[c].rf == GL_MAX)
            expct[i] = blend_apply(cases[c].rf, s, d);
         else
            expct[i] = blend_apply(cases[c].rf, s * sf[i], d * df[i]);
      }
      if (cases[c].af == GL_MIN || cases[c].af == GL_MAX)
         expct[3] = blend_apply(cases[c].af, src_c[3], dst_c[3]);
      else
         expct[3] = blend_apply(cases[c].af, src_c[3] * sa[3], dst_c[3] * da[3]);

      /* 比较中心像素(整屏同色) */
      int cx = SZ / 2, cy = SZ / 2;
      const unsigned char *got = px + (cy * SZ + cx) * 4;
      int bad = 0, maxdiff = 0;
      for (int i = 0; i < 4 && !cases[c].striped; i++) {
         int want = (int)lrintf(clampf(expct[i]) * 255.0f);
         int diff = got[i] - want;
         if (diff < 0) diff = -diff;
         if (diff > maxdiff) maxdiff = diff;
         if (diff > 1) bad = 1;
      }
      if (!cases[c].striped)
      printf("%-22s got %3d %3d %3d %3d  want %3d %3d %3d %3d  %s (maxdiff=%d)\n",
             cases[c].name, got[0], got[1], got[2], got[3],
             (int)lrintf(clampf(expct[0]) * 255.f), (int)lrintf(clampf(expct[1]) * 255.f),
             (int)lrintf(clampf(expct[2]) * 255.f), (int)lrintf(clampf(expct[3]) * 255.f),
             bad ? "FAIL" : "PASS", maxdiff);
      if (cases[c].striped) {
         /* 逐像素与预置底色比对(输出应当就是 dst 本身) */
         int mism = 0;
         for (int y = 0; y < SZ; y++)
            for (int x = 0; x < SZ; x++) {
               const unsigned char *g = px + (y * SZ + x) * 4;
               int want[4] = { y * 4, 255 - y * 4, (y & 1) ? 255 : 0, 200 };
               for (int i = 0; i < 4; i++) {
                  int d2 = g[i] - want[i];
                  if (d2 < 0) d2 = -d2;
                  if (d2 > 1) mism++;
               }
            }
         printf("%-22s striped compare mismatched channels = %d %s\n",
                cases[c].name, mism, mism ? "FAIL" : "PASS");
         if (mism) { bad = 1; }
      }
      if (bad) fails++;

      if (dump) fwrite(px, 1, SZ * SZ * 4, dump);
   }

   if (dump) fclose(dump);
   printf("RESULT: %s (%d/%d)\n", fails ? "FAIL" : "PASS", NCASES - fails, NCASES);
   return fails ? 1 : 0;
}
