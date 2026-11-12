/* glsl 窥孔正确性探针：把 fdiv / mul+add / 比较转浮点 的表达式渲染成像素，
 * 与 llvmpipe 输出逐像素比较；用于评估 OPT_DIV / OPT_FFMA / OPT_SET 是否安全。
 * 用法：peep <输出.rgba>   （同时打印每像素最大绝对差 vs 参考，若给了参考） */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define W 256
#define H 256

static const char *vs =
   "attribute vec2 pos;\n"
   "varying vec2 uv;\n"
   "void main() { uv = pos * 0.5 + 0.5; gl_Position = vec4(pos, 0.0, 1.0); }\n";

static const char *fs =
   "precision highp float;\n"
   "varying vec2 uv;\n"
   "void main() {\n"
   "  float d1 = (uv.x + 0.123456) / 3.0;\n"        /* 除常数 -> frcp*fmul */
   "  float d2 = (uv.y * 7.0 + 0.25) / (uv.x * 3.0 + 11.0);\n"
   "  float d3 = 2.0 / (uv.x + 1.5);\n"
   "  float f1 = uv.x * 4.7 + 0.31;\n"                /* mul+add -> ffma */
   "  float f2 = uv.y * uv.x + 0.7;\n"
   "  float f3 = -uv.x * 2.5 + uv.y;\n"
   "  float c1 = (uv.x > 0.5) ? 1.0 : 0.0;\n"          /* cmp -> b2f32 -> set */
   "  float c2 = (uv.y < 0.25) ? 0.75 : 0.0;\n"
   "  float c3 = float(uv.x >= uv.y);\n"
   "  gl_FragColor = vec4(fract(d1), fract(d2), fract(d3), fract(f1 + f2 + f3 + c1 + c2 + c3));\n"
   "}\n";

int main(int argc, char **argv)
{
   EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   EGLint major, minor;
   if (!eglInitialize(dpy, &major, &minor)) { fprintf(stderr, "eglInitialize\n"); return 1; }
   if (!eglBindAPI(EGL_OPENGL_ES_API)) { fprintf(stderr, "bind API\n"); return 1; }
   EGLint cfg_attr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                         EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
                         EGL_NONE };
   EGLConfig cfg; EGLint n = 0;
   if (!eglChooseConfig(dpy, cfg_attr, &cfg, 1, &n) || !n) { fprintf(stderr, "no config\n"); return 1; }
   EGLint pb[] = { EGL_WIDTH, W, EGL_HEIGHT, H, EGL_NONE };
   EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, pb);
   EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
   EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
   if (!eglMakeCurrent(dpy, surf, surf, ctx)) { fprintf(stderr, "make current\n"); return 1; }

   GLuint vsid = glCreateShader(GL_VERTEX_SHADER);
   glShaderSource(vsid, 1, &vs, NULL); glCompileShader(vsid);
   GLuint fsid = glCreateShader(GL_FRAGMENT_SHADER);
   glShaderSource(fsid, 1, &fs, NULL); glCompileShader(fsid);
   GLint ok = 0;
   glGetShaderiv(fsid, GL_COMPILE_STATUS, &ok);
   if (!ok) { char log[4096]; glGetShaderInfoLog(fsid, sizeof log, NULL, log); fprintf(stderr, "fs compile: %s\n", log); return 1; }
   GLuint prog = glCreateProgram();
   glAttachShader(prog, vsid); glAttachShader(prog, fsid);
   glBindAttribLocation(prog, 0, "pos");
   glLinkProgram(prog);
   glGetProgramiv(prog, GL_LINK_STATUS, &ok);
   if (!ok) { char log[4096]; glGetProgramInfoLog(prog, sizeof log, NULL, log); fprintf(stderr, "link: %s\n", log); return 1; }
   glUseProgram(prog);

   const GLfloat quad[] = { -1, -1, 1, -1, -1, 1, 1, 1 };
   glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, quad);
   glEnableVertexAttribArray(0);
   glViewport(0, 0, W, H);
   glClearColor(0, 0, 0, 0);
   glClear(GL_COLOR_BUFFER_BIT);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glFinish();

   static unsigned char pix[W * H * 4];
   glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, pix);
   GLenum err = glGetError();
   const char *out = argc > 1 ? argv[1] : NULL;
   if (out) {
      FILE *f = fopen(out, "wb");
      if (f) { fwrite(pix, 1, sizeof pix, f); fclose(f); }
   }
   const char *r = (const char *)glGetString(GL_RENDERER);
   printf("renderer: %s glErr=0x%x first pix=%02x%02x%02x%02x\n", r ? r : "?", err,
          pix[0], pix[1], pix[2], pix[3]);
   if (argc > 2) {
      FILE *f = fopen(argv[2], "rb");
      if (f) {
         static unsigned char ref[W * H * 4];
         if (fread(ref, 1, sizeof ref, f) == sizeof ref) {
            int maxd = 0, ndiff = 0;
            for (int i = 0; i < W * H * 4; i++) {
               int d = abs((int)pix[i] - (int)ref[i]);
               if (d > maxd) maxd = d;
               if (d) ndiff++;
            }
            printf("compare vs %s: maxdiff=%d bytes_diff=%d/%d\n", argv[2], maxd, ndiff, W * H * 4);
         }
         fclose(f);
      }
   }
   return 0;
}
