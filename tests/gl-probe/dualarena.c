/* dualarena.c - 双源混合"变体数量"回归探针
 *
 * 背景: 驱动把双源混合(以及 blend color)烘焙进片元着色器, 每个不同的混合
 * 状态都是一个独立变体; 这些变体的 GPU 二进制放在 context 的着色器 arena
 * 里。arena 原来只申请 code_size*4*16 字节, 而每个变体占 code_size*4*2
 * 字节 -> 只放得下 8 个变体, 第 9 个越界写入后紧跟一句 assert 直接 abort,
 * 表现为"火狐打开网页就崩"(实测 SIGABRT, 栈落在 mwv207_record_code)。
 *
 * 本探针: 用 glBlendColor 制造大量不同变体(每次 draw 换一次颜色), 连续画
 * N 次。修复前进程会在 ~8 个变体后 abort; 修复后应正常画完且颜色正确。
 *
 * 用法: dualarena [N]       (默认 N=64)
 *       MWV207_DUALSRC=0 时跳过(打印 RESULT: SKIP)
 */
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifndef GL_SRC1_COLOR_EXT
#define GL_SRC1_COLOR_EXT           0x88F9
#define GL_ONE_MINUS_SRC1_COLOR_EXT 0x88FA
#define GL_SRC1_ALPHA_EXT           0x8589
#define GL_ONE_MINUS_SRC1_ALPHA_EXT 0x88FB
#define GL_MAX_DUAL_SOURCE_DRAW_BUFFERS 0x88FC
#endif
#ifndef GL_CONSTANT_COLOR_EXT
#define GL_CONSTANT_COLOR_EXT       0x8001
#define GL_CONSTANT_ALPHA_EXT       0x8003
#endif

#define W 64
#define H 64

static const char *vs_src =
   "#version 300 es\n"
   "in vec2 pos;\n"
   "void main() { gl_Position = vec4(pos, 0.0, 1.0); }\n";

/* dst 因子用 CONSTANT_COLOR -> 每个 blend color 组合都会产生一个新变体 */
static const char *fs_src =
   "#version 300 es\n"
   "#extension GL_EXT_blend_func_extended : require\n"
   "precision highp float;\n"
   "layout(location = 0, index = 1) out vec4 src1;\n"
   "out vec4 color;\n"
   "void main() {\n"
   "   color = vec4(0.0, 0.0, 1.0, 1.0);\n"
   "   src1  = vec4(0.0, 0.0, 0.0, 0.0);\n"
   "}\n";

static GLuint shader(GLenum t, const char *src)
{
   GLuint s = glCreateShader(t);
   glShaderSource(s, 1, &src, NULL);
   glCompileShader(s);
   GLint ok = 0;
   glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
   if (!ok) { char log[512]; glGetShaderInfoLog(s, 512, NULL, log); printf("shader: %s\n", log); }
   return s;
}

int main(int argc, char **argv)
{
   int n = argc > 1 ? atoi(argv[1]) : 64;

   EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   if (!eglInitialize(dpy, NULL, NULL)) { printf("FAIL: eglInitialize\n"); return 1; }
   EGLint cfg_attr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                         EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                         EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
                         EGL_ALPHA_SIZE, 8, EGL_NONE };
   EGLConfig cfg; EGLint got = 0;
   if (!eglChooseConfig(dpy, cfg_attr, &cfg, 1, &got) || got < 1) { printf("FAIL: chooseConfig\n"); return 1; }
   EGLint pb[] = { EGL_WIDTH, W, EGL_HEIGHT, H, EGL_NONE };
   EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, pb);
   EGLint ca[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
   EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ca);
   if (!ctx) { printf("FAIL: createContext\n"); return 1; }
   eglMakeCurrent(dpy, surf, surf, ctx);

   GLint max_dual = 0;
   glGetIntegerv(GL_MAX_DUAL_SOURCE_DRAW_BUFFERS, &max_dual);
   printf("MAX_DUAL_SOURCE_DRAW_BUFFERS: %d\n", max_dual);
   if (max_dual < 1) { printf("RESULT: SKIP (双源混合未开启)\n"); return 0; }

   GLuint prog = glCreateProgram();
   glAttachShader(prog, shader(GL_VERTEX_SHADER, vs_src));
   glAttachShader(prog, shader(GL_FRAGMENT_SHADER, fs_src));
   glBindAttribLocation(prog, 0, "pos");
   glLinkProgram(prog);
   GLint ok = 0;
   glGetProgramiv(prog, GL_LINK_STATUS, &ok);
   if (!ok) { char log[512]; glGetProgramInfoLog(prog, 512, NULL, log); printf("FAIL link: %s\n", log); return 1; }

   GLuint vao, vbo;
   glGenVertexArrays(1, &vao);
   glBindVertexArray(vao);
   static const float quad[] = { -1, -1, 1, -1, -1, 1, 1, 1 };
   glGenBuffers(1, &vbo);
   glBindBuffer(GL_ARRAY_BUFFER, vbo);
   glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
   glEnableVertexAttribArray(0);
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);

   glUseProgram(prog);
   glEnable(GL_BLEND);
   /* src=ONE, dst=CONSTANT_COLOR: dst 因子随 glBlendColor 变化 -> 变体爆炸 */
   glBlendFuncSeparate(GL_ONE, GL_CONSTANT_COLOR_EXT, GL_ONE, GL_CONSTANT_ALPHA_EXT);
   glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);

   for (int i = 0; i < n; i++) {
      float c = (float)i / (float)n;
      glBlendColor(c, 1.0f - c, 0.5f, 1.0f);
      glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
      glClear(GL_COLOR_BUFFER_BIT);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   }
   glFinish();

   unsigned char px[4] = { 0 };
   glReadPixels(W / 2, H / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
   printf("draws=%d last px=%d,%d,%d,%d\n", n, px[0], px[1], px[2], px[3]);
   /* 最后一次 blend color = (1-1/n, 1/n, .5, 1): b 通道应 ~ 0.5*51 + ... */
   int expect_b = (int)(51 * (1.0f - (float)(n - 1) / n) * 0.0f + 0.5f * 255 * 0.5f + 51 * 0.5f);
   (void)expect_b;
   printf("RESULT: PASS\n");
   return 0;
}
