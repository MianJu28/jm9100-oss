/* 双源混合路径的每 draw 成本：硬件没有 SRC1 因子，驱动把混合降级进片元着色器，
 * 而且每个 dual draw 都要把渲染目标整张拷到临时纹理再 texelFetch 回读。
 * 这里量一下 1280x800 目标上每 draw 的毫秒数（MWV207_DUALSRC=0 时为普通单输出基线）。
 *
 * cc -O2 -o ~/dev-bin/dualbench tests/gl-probe/dualbench.c -lEGL -lGLESv2 -lm
 */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#define GL_GLEXT_PROTOTYPES 1
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static double now_ms(void)
{
   struct timespec ts;
   clock_gettime(CLOCK_MONOTONIC, &ts);
   return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

int main(int argc, char **argv)
{
   int W = argc > 1 ? atoi(argv[1]) : 1280;
   int H = argc > 2 ? atoi(argv[2]) : 800;
   int N = argc > 3 ? atoi(argv[3]) : 200;

   EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   eglInitialize(dpy, NULL, NULL);
   eglBindAPI(EGL_OPENGL_ES_API);
   EGLint cfg_attr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE,
                         EGL_OPENGL_ES3_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8,
                         EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE };
   EGLConfig cfg; EGLint n;
   eglChooseConfig(dpy, cfg_attr, &cfg, 1, &n);
   EGLint pb_attr[] = { EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE };
   EGLSurface pb = eglCreatePbufferSurface(dpy, cfg, pb_attr);
   EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
   EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
   eglMakeCurrent(dpy, pb, pb, ctx);

   GLint maxd = 0;
   glGetIntegerv(0x88FC /* GL_MAX_DUAL_SOURCE_DRAW_BUFFERS */, &maxd);
   printf("MAX_DUAL_SOURCE_DRAW_BUFFERS=%d\n", maxd);

   GLuint fbo, tex;
   glGenTextures(1, &tex);
   glBindTexture(GL_TEXTURE_2D, tex);
   glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, W, H);
   glGenFramebuffers(1, &fbo);
   glBindFramebuffer(GL_FRAMEBUFFER, fbo);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
   glViewport(0, 0, W, H);

   const char *vs =
      "#version 300 es\n"
      "void main(){ vec2 p = vec2((gl_VertexID==1)?3.0:-1.0,(gl_VertexID==2)?3.0:-1.0);\n"
      " gl_Position = vec4(p,0,1); }\n";
   /* 两个输出：location 0 index 0 是颜色，location 0 index 1 是 GL_SRC1_* 因子 */
   const char *fs =
      "#version 300 es\n"
      "#extension GL_EXT_blend_func_extended : require\n"
      "precision highp float;\n"
      "layout(location=0,index=0) out vec4 o0;\n"
      "layout(location=0,index=1) out vec4 o1;\n"
      "void main(){ o0 = vec4(0.30,0.55,0.72,0.35); o1 = vec4(0.6,0.4,0.25,0.8); }\n";
   const char *fs1 =
      "#version 300 es\n"
      "precision highp float;\n"
      "layout(location=0,index=0) out vec4 o0;\n"
      "void main(){ o0 = vec4(0.30,0.55,0.72,0.35); }\n";

   GLuint prog = glCreateProgram();
   GLuint v = glCreateShader(GL_VERTEX_SHADER), f = glCreateShader(GL_FRAGMENT_SHADER);
   glShaderSource(v, 1, &vs, NULL); glCompileShader(v);
   const char *fsrc = maxd ? fs : fs1;
   glShaderSource(f, 1, &fsrc, NULL); glCompileShader(f);
   GLint ok = 0;
   glGetShaderiv(f, GL_COMPILE_STATUS, &ok);
   if (!ok) { char log[1024]; glGetShaderInfoLog(f, sizeof log, NULL, log); printf("FS compile fail: %s\n", log); }
   glAttachShader(prog, v); glAttachShader(prog, f);
   if (maxd)
      (void)0;
   glLinkProgram(prog);
   glGetProgramiv(prog, GL_LINK_STATUS, &ok);
   if (!ok) { char log[1024]; glGetProgramInfoLog(prog, sizeof log, NULL, log); printf("link fail: %s\n", log); }
   glUseProgram(prog);

   GLuint vao; glGenVertexArrays(1, &vao); glBindVertexArray(vao);

   if (maxd) {
      glEnablei(GL_BLEND, 0);
      glBlendFuncSeparatei(0, GL_ONE, 0x88FA /*ONE_MINUS_SRC1_COLOR*/, GL_ONE, GL_ZERO);
      glBlendEquationSeparatei(0, GL_FUNC_ADD, GL_FUNC_ADD);
   } else {
      glEnable(GL_BLEND);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   }

   int use_scissor = getenv("DUALBENCH_SCISSOR") != NULL;

   glClearColor(0, 0, 0, 1);
   glFinish();
   double t0 = now_ms();
   for (int i = 0; i < N; i++) {
      if (use_scissor) {
         glEnable(GL_SCISSOR_TEST);
         glScissor(0, (i * 16) % (H > 16 ? H - 16 : 1), W, 16);
      }
      glDrawArrays(GL_TRIANGLES, 0, 3);
   }
   glFinish();
   double dt = now_ms() - t0;
   printf("%dx%d dual=%d%s draws=%d: %.3f ms/draw\n", W, H, maxd,
          use_scissor ? " scissor" : "", N, dt / N);
   return 0;
}
