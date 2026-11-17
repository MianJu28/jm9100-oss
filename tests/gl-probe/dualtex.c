/* dualtex.c - 双源混合 + "渲染目标读回槽位"回归探针
 *
 * 背景: 火狐的 LCD 次像素文字用双源混合(GL_SRC1_COLOR)。硬件没有 SRC1 因子,
 * 驱动把混合降级进片元着色器, 并把渲染目标先 blit 到一张拷贝纹理, 用
 * "当前已绑定的视图数量" 作为采样器槽号去 texelFetch。
 *
 * 这个槽号随每次 draw 的视图数量变化。如果着色器变体键里不含它, 同一个程序
 * 会在"绑 1 张纹理"和"绑 2 张纹理"的 draw 之间复用 → texelFetch 落到应用自己
 * 的第 2 张纹理上 → 文字整片乱码(实测火狐 cnb.cool 页面)。
 *
 * 本探针: 同一套着色器 + 同一混合状态, 先只绑 1 张纹理画一次, 再绑 2 张
 * (第 2 张是明显不同的噪声纹理)画一次。两次的源颜色相同、目标内容相同,
 * 结果必须一致。修复前第 2 次会把噪声纹理当成 dst 读回, 结果不同。
 *
 * 用法: dualtex        (打印 RESULT: PASS/FAIL)
 *       MWV207_DUALSRC=0 时跳过(打印 RESULT: SKIP)
 */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* GLES3 头里没有 EXT 的枚举名, 手工补上 */
#ifndef GL_SRC1_COLOR_EXT
#define GL_SRC1_COLOR_EXT          0x88F9
#define GL_ONE_MINUS_SRC1_COLOR_EXT 0x88FA
#define GL_SRC1_ALPHA_EXT          0x8589
#define GL_ONE_MINUS_SRC1_ALPHA_EXT 0x88FB
#define GL_MAX_DUAL_SOURCE_DRAW_BUFFERS 0x88FC
#endif

#define W 64
#define H 64

static const char *vs_src =
   "#version 300 es\n"
   "in vec2 pos;\n"
   "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";

/* 第二输出 src1 = 蓝色, 让 dst 通过 ONE_MINUS_SRC1_COLOR 影响结果 */
static const char *fs_src =
   "#version 300 es\n"
   "#extension GL_EXT_blend_func_extended : require\n"
   "precision highp float;\n"
   "uniform sampler2D tex;\n"
   "layout(location = 0, index = 1) out vec4 src1;\n"
   "out vec4 color;\n"
   "void main() {\n"
   "   vec4 t = texture(tex, vec2(0.5));\n"
   "   color = t;\n"
   "   src1 = vec4(0.0, 0.0, 0.5, 1.0);\n"
   "}\n";

static GLuint make_prog(void)
{
   GLuint vs = glCreateShader(GL_VERTEX_SHADER);
   glShaderSource(vs, 1, &vs_src, NULL);
   glCompileShader(vs);
   GLint ok = 0;
   glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
   if (!ok) { char log[512]; glGetShaderInfoLog(vs, 512, NULL, log); printf("VS: %s\n", log); }

   GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
   glShaderSource(fs, 1, &fs_src, NULL);
   glCompileShader(fs);
   glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
   if (!ok) { char log[512]; glGetShaderInfoLog(fs, 512, NULL, log); printf("FS: %s\n", log); }

   GLuint p = glCreateProgram();
   glAttachShader(p, vs);
   glAttachShader(p, fs);
   glBindAttribLocation(p, 0, "pos");
   glLinkProgram(p);
   glGetProgramiv(p, GL_LINK_STATUS, &ok);
   if (!ok) { char log[512]; glGetProgramInfoLog(p, 512, NULL, log); printf("LINK: %s\n", log); }
   return p;
}

static GLuint make_tex(unsigned char r, unsigned char g, unsigned char b, int noise)
{
   unsigned char px[16 * 16 * 4];
   unsigned seed = 12345;
   for (int i = 0; i < 16 * 16; i++) {
      if (noise) {
         seed = seed * 1103515245u + 12345u;
         px[i * 4 + 0] = (seed >> 16) & 0xff;
         px[i * 4 + 1] = (seed >> 8) & 0xff;
         px[i * 4 + 2] = seed & 0xff;
      } else {
         px[i * 4 + 0] = r; px[i * 4 + 1] = g; px[i * 4 + 2] = b;
      }
      px[i * 4 + 3] = 255;
   }
   GLuint t;
   glGenTextures(1, &t);
   glBindTexture(GL_TEXTURE_2D, t);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 16, 16, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   return t;
}

/* 画一次并读回第一个像素 */
static void draw_once(GLuint prog, GLuint *texs, int ntex, unsigned char out[4])
{
   glClearColor(0.25f, 0.25f, 0.25f, 1.0f);
   glClear(GL_COLOR_BUFFER_BIT);

   glUseProgram(prog);
   for (int i = 0; i < ntex; i++) {
      glActiveTexture(GL_TEXTURE0 + i);
      glBindTexture(GL_TEXTURE_2D, texs[i]);
   }
   glUniform1i(glGetUniformLocation(prog, "tex"), 0);

   glEnable(GL_BLEND);
   glBlendFuncSeparate(GL_ONE, GL_ONE_MINUS_SRC1_COLOR_EXT,
                       GL_ONE, GL_ONE_MINUS_SRC1_ALPHA_EXT);
   glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);

   GLuint vao, vbo;
   glGenVertexArrays(1, &vao);
   glBindVertexArray(vao);
   static const float quad[] = { -1, -1, 1, -1, -1, 1, 1, 1 };
   glGenBuffers(1, &vbo);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
   glEnableVertexAttribArray(0);
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glFinish();

   glReadPixels(W / 2, H / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, out);
}

int main(void)
{
   EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   if (!eglInitialize(dpy, NULL, NULL)) { printf("FAIL: eglInitialize\n"); return 1; }
   EGLint cfg_attr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                         EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                         EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                         EGL_ALPHA_SIZE, 8, EGL_NONE };
   EGLConfig cfg;
   EGLint n = 0;
   if (!eglChooseConfig(dpy, cfg_attr, &cfg, 1, &n) || n < 1) { printf("FAIL: chooseConfig\n"); return 1; }
   EGLint pb[] = { EGL_WIDTH, W, EGL_HEIGHT, H, EGL_NONE };
   EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, pb);
   EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
   EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
   if (!ctx) { printf("FAIL: createContext\n"); return 1; }
   eglMakeCurrent(dpy, surf, surf, ctx);

   printf("RENDERER: %s\n", (const char *)glGetString(GL_RENDERER));

   GLint max_dual = 0;
   glGetIntegerv(GL_MAX_DUAL_SOURCE_DRAW_BUFFERS, &max_dual);
   printf("MAX_DUAL_SOURCE_DRAW_BUFFERS: %d\n", max_dual);
   if (max_dual < 1) { printf("RESULT: SKIP (双源混合未开启)\n"); return 0; }

   GLuint prog = make_prog();
   GLuint tex_a = make_tex(230, 40, 40, 0);      /* 源纹理: 红 */
   GLuint tex_b = make_tex(0, 0, 0, 1);          /* 噪声纹理: 只用于占第 2 个槽 */

   unsigned char one[4] = { 0 }, two[4] = { 0 };
   GLuint set1[1] = { tex_a };
   GLuint set2[2] = { tex_a, tex_b };

   draw_once(prog, set1, 1, one);
   draw_once(prog, set2, 2, two);

   int diff = abs((int)one[0] - (int)two[0]) + abs((int)one[1] - (int)two[1]) +
              abs((int)one[2] - (int)two[2]);
   printf("1-view: %d,%d,%d  2-view: %d,%d,%d  diff=%d\n",
          one[0], one[1], one[2], two[0], two[1], two[2], diff);

   if (diff <= 2)
      printf("RESULT: PASS\n");
   else {
      printf("RESULT: FAIL (绑 2 张纹理时复用了按 1 张纹理编译的变体 → dst 读回错纹理)\n");
      return 1;
   }
   return 0;
}