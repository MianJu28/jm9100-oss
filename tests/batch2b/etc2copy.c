#include <stdio.h>
#include <string.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl32.h>

static GLuint mktex(const void *data, int compressed)
{
   GLuint t; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
   glTexStorage2D(GL_TEXTURE_2D, 1, GL_COMPRESSED_RGBA8_ETC2_EAC, 8, 8);
   if (compressed)
      glCompressedTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 8, 8,
                                GL_COMPRESSED_RGBA8_ETC2_EAC, 64, data);
   return t;
}

static const char *vs_src =
"#version 300 es\n"
"out vec2 uv;\n"
"void main(){ vec2 p = vec2(gl_VertexID & 1, gl_VertexID >> 1);\n"
"  uv = p; gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0); }\n";
static const char *fs_src =
"#version 300 es\n"
"precision mediump float; in vec2 uv; uniform sampler2D tex; out vec4 o;\n"
"void main(){ o = texture(tex, uv); }\n";

static GLuint mksh(GLenum type, const char *src)
{
   GLuint s = glCreateShader(type);
   glShaderSource(s, 1, &src, NULL); glCompileShader(s);
   GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
   if (!ok) { char log[1024]; glGetShaderInfoLog(s, sizeof log, NULL, log); fprintf(stderr, "shader: %s\n", log); }
   return s;
}

static void draw_and_read(GLuint tex, unsigned char *out)
{
   static GLuint prog, vao, fbo, rb; static int init = 0;
   if (!init) {
      prog = glCreateProgram();
      glAttachShader(prog, mksh(GL_VERTEX_SHADER, vs_src));
      glAttachShader(prog, mksh(GL_FRAGMENT_SHADER, fs_src));
      glLinkProgram(prog);
      glGenVertexArrays(1, &vao); glBindVertexArray(vao);
      glGenTextures(1, &rb); glBindTexture(GL_TEXTURE_2D, rb);
      glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 8, 8);
      glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rb, 0);
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      init = 1;
   }
   glBindFramebuffer(GL_FRAMEBUFFER, fbo);
   glViewport(0, 0, 8, 8);
   glClearColor(0, 0, 0, 1); glClear(GL_COLOR_BUFFER_BIT);
   glUseProgram(prog);
   glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, tex);
   glUniform1i(glGetUniformLocation(prog, "tex"), 0);
   glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
   glReadPixels(0, 0, 8, 8, GL_RGBA, GL_UNSIGNED_BYTE, out);
   glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

int main(void)
{
   EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   if (!eglInitialize(dpy, NULL, NULL)) { fprintf(stderr, "eglInitialize failed\n"); return 2; }
   EGLint cfg_attr[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                         EGL_RED_SIZE,8, EGL_GREEN_SIZE,8, EGL_BLUE_SIZE,8, EGL_ALPHA_SIZE,8, EGL_NONE };
   EGLConfig cfg; EGLint n;
   if (!eglChooseConfig(dpy, cfg_attr, &cfg, 1, &n) || !n) { fprintf(stderr, "no config\n"); return 3; }
   eglBindAPI(EGL_OPENGL_ES_API);
   EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
   EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctx_attr);
   if (ctx == EGL_NO_CONTEXT) { fprintf(stderr, "no ctx\n"); return 4; }
   EGLint pb[] = { EGL_WIDTH, 8, EGL_HEIGHT, 8, EGL_NONE };
   EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, pb);
   eglMakeCurrent(dpy, surf, surf, ctx);
   printf("GL=%s GLSL=%s\n", glGetString(GL_VERSION), glGetString(GL_SHADING_LANGUAGE_VERSION));

   unsigned char block[64];
   for (int i = 0; i < 64; i++) block[i] = (unsigned char)(i * 37 + 11);
   GLuint src = mktex(block, 1);
   GLuint dst = mktex(block, 0);
   glCopyImageSubData(src, GL_TEXTURE_2D, 0, 0, 0, 0,
                      dst, GL_TEXTURE_2D, 0, 0, 0, 0, 8, 8, 1);
   GLenum e = glGetError();
   printf("after glCopyImageSubData err=0x%x\n", e);

   unsigned char a[8*8*4], b[8*8*4];
   draw_and_read(src, a);
   draw_and_read(dst, b);
   printf("src_sum=%u dst_sum=%u\n",
          (unsigned)(a[0]+a[1]+a[2]+a[3]), (unsigned)(b[0]+b[1]+b[2]+b[3]));
   printf("DIFF=%d\n", memcmp(a, b, sizeof a) != 0);
   for (int i = 0; i < 16; i++) printf("%02x ", a[i]); printf("\n");
   for (int i = 0; i < 16; i++) printf("%02x ", b[i]); printf("\n");
   return 0;
}
