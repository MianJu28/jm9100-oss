/* UBO 覆盖竞争检测: 连续 frames 帧, 每帧把 UBO 改成新颜色并画进大纹理的第 i 块,
 * 全程不 glFinish/glReadPixels (不做任何同步), 最后一次性读回所有块,
 * 逐块校验颜色是否等于该帧写入的值。若常量上传与在途 GPU 读取之间丢同步,
 * 会出现"后面的颜色覆盖了前面的块"。
 * 用法: uborace [frames] */
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <stdio.h>
#include <stdlib.h>

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
   if (frames > 256) frames = 256;
   int tiles_x = 16;
   int W = tiles_x * 16, H = ((frames + tiles_x - 1) / tiles_x) * 16;

   EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   eglInitialize(dpy, NULL, NULL);
   EGLint cfga[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
                     EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE };
   EGLConfig cfg; EGLint n = 0;
   eglChooseConfig(dpy, cfga, &cfg, 1, &n);
   EGLint ctxa[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
   EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxa);
   EGLint pba[] = { EGL_WIDTH, W, EGL_HEIGHT, H, EGL_NONE };
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
   glUniformBlockBinding(prog, glGetUniformBlockIndex(prog, "Blk"), 0);
   glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);

   GLuint vao; glGenVertexArrays(1, &vao); glBindVertexArray(vao);
   glClearColor(0, 0, 0, 1);
   glClear(GL_COLOR_BUFFER_BIT);

   unsigned char *exp = calloc((size_t)frames, 3);
   for (int i = 0; i < frames; i++) {
      unsigned char c0 = (unsigned char)(i * 7 + 3), c1 = (unsigned char)(i * 13 + 5), c2 = (unsigned char)(i * 29 + 11);
      exp[i*3+0] = c0; exp[i*3+1] = c1; exp[i*3+2] = c2;
      float c[4] = { c0 / 255.0f, c1 / 255.0f, c2 / 255.0f, 1.0f };
      glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(c), c);
      int tx = (i % tiles_x) * 16, ty = (i / tiles_x) * 16;
      glViewport(tx, ty, 16, 16);
      glDrawArrays(GL_TRIANGLES, 0, 3);
   }

   unsigned char *px = malloc((size_t)W * H * 4);
   glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, px);   /* 唯一的同步点 */

   int bad = 0, first_bad = -1;
   for (int i = 0; i < frames; i++) {
      int tx = (i % tiles_x) * 16, ty = (i / tiles_x) * 16;
      size_t off = ((size_t)ty * W + tx) * 4;
      unsigned char *p = px + off;
      if (p[0] != exp[i*3+0] || p[1] != exp[i*3+1] || p[2] != exp[i*3+2]) {
         if (!bad) first_bad = i;
         bad++;
      }
   }
   printf("RESULT: %s (%d/%d tiles mismatched%s)\n", bad ? "FAIL" : "PASS", bad, frames,
          bad ? "" : ", 无丢失同步");
   if (bad) printf("first mismatch tile %d\n", first_bad);
   return bad ? 1 : 0;
}
