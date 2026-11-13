/* UBO 逐帧写读校验: 每帧把 GL_UNIFORM_BUFFER 的内容改掉并立刻画, 读回验证
 * 每帧颜色都是新的 (若常量上传与在途 GPU 读取之间丢了同步, 会出现旧颜色/错色)。
 * 用法: ubosync [frames]   打印 PASS/FAIL 与首个不符帧 */
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *vs =
   "#version 300 es\n"
   "void main(){ vec2 p = vec2((gl_VertexID<<1)&2, gl_VertexID&2);"
   " gl_Position = vec4(p*2.0-1.0, 0.0, 1.0); }\n";
static const char *fs =
   "#version 300 es\n"
   "precision mediump float;\n"
   "layout(std140) uniform Blk { vec4 color; } blk;\n"
   "out vec4 o;\n"
   "void main(){ o = blk.color; }\n";

static GLuint mksh(GLenum t, const char *s)
{
   GLuint h = glCreateShader(t);
   glShaderSource(h, 1, &s, NULL);
   glCompileShader(h);
   GLint ok = 0; glGetShaderiv(h, GL_COMPILE_STATUS, &ok);
   if (!ok) { char log[512]; glGetShaderInfoLog(h, 512, NULL, log); fprintf(stderr, "shader: %s\n", log); exit(2); }
   return h;
}

int main(int argc, char **argv)
{
   int frames = argc > 1 ? atoi(argv[1]) : 64;
   EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   eglInitialize(dpy, NULL, NULL);
   EGLint cfga[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                     EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE };
   EGLConfig cfg; EGLint n = 0;
   eglChooseConfig(dpy, cfga, &cfg, 1, &n);
   EGLint ctxa[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
   EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxa);
   EGLint pba[] = { EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE };
   EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, pba);
   eglMakeCurrent(dpy, surf, surf, ctx);

   GLuint prog = glCreateProgram();
   glAttachShader(prog, mksh(GL_VERTEX_SHADER, vs));
   glAttachShader(prog, mksh(GL_FRAGMENT_SHADER, fs));
   glLinkProgram(prog);
   GLint ok = 0; glGetProgramiv(prog, GL_LINK_STATUS, &ok);
   if (!ok) { char log[512]; glGetProgramInfoLog(prog, 512, NULL, log); fprintf(stderr, "link: %s\n", log); return 2; }
   glUseProgram(prog);

   GLuint ubo; glGenBuffers(1, &ubo);
   glBindBuffer(GL_UNIFORM_BUFFER, ubo);
   glBufferData(GL_UNIFORM_BUFFER, 256, NULL, GL_DYNAMIC_DRAW);
   GLuint idx = glGetUniformBlockIndex(prog, "Blk");
   glUniformBlockBinding(prog, idx, 0);
   glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);

   GLuint vao; glGenVertexArrays(1, &vao); glBindVertexArray(vao);
   glViewport(0, 0, 64, 64);

   int bad = 0, first_bad = -1;
   for (int i = 0; i < frames; i++) {
      unsigned char exp[4] = { (unsigned char)(i * 7 + 3), (unsigned char)(i * 13 + 5),
                               (unsigned char)(i * 29 + 11), 255 };
      float c[4] = { exp[0] / 255.0f, exp[1] / 255.0f, exp[2] / 255.0f, 1.0f };
      glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(c), c);
      glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
      glDrawArrays(GL_TRIANGLES, 0, 3);
      unsigned char px[4] = {0};
      glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
      if (px[0] != exp[0] || px[1] != exp[1] || px[2] != exp[2]) {
         if (!bad) first_bad = i;
         bad++;
      }
   }
   printf("RESULT: %s (%d/%d frames mismatched%s)\n", bad ? "FAIL" : "PASS", bad, frames,
          bad ? "" : ", UBO 更新逐帧生效");
   if (bad) printf("first mismatch at frame %d\n", first_bad);
   return bad ? 1 : 0;
}
