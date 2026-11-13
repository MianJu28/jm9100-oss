/* 每 draw 的 CPU 开销: 视口 8x8 的小三角形, 重复 N 次; 打印 draws/s 与 us/draw */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1e3+t.tv_nsec/1e6;}
static const char *vs="#version 300 es\nin vec2 p;void main(){gl_Position=vec4(p,0.,1.);}\n";
static const char *fs="#version 300 es\nprecision mediump float;out vec4 o;void main(){o=vec4(1.,0.,0.,1.);}\n";
static GLuint mksh(GLenum t,const char*s){GLuint h=glCreateShader(t);glShaderSource(h,1,&s,0);glCompileShader(h);return h;}
int main(int argc,char**argv){
  int n=argc>1?atoi(argv[1]):20000, frames=argc>2?atoi(argv[2]):30;
  EGLDisplay d=eglGetDisplay(EGL_DEFAULT_DISPLAY); eglInitialize(d,0,0);
  EGLint ca[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_NONE};
  EGLConfig c;EGLint ncfg;eglChooseConfig(d,ca,&c,1,&ncfg);
  EGLint pa[]={EGL_WIDTH,64,EGL_HEIGHT,64,EGL_NONE};EGLSurface s=eglCreatePbufferSurface(d,c,pa);
  eglBindAPI(EGL_OPENGL_ES_API);
  EGLint xa[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};EGLContext x=eglCreateContext(d,c,EGL_NO_CONTEXT,xa);
  eglMakeCurrent(d,s,s,x);
  GLuint p=mksh(GL_VERTEX_SHADER,vs), f=mksh(GL_FRAGMENT_SHADER,fs), pr=glCreateProgram();
  glAttachShader(pr,p);glAttachShader(pr,f);glBindAttribLocation(pr,0,"p");glLinkProgram(pr);glUseProgram(pr);
  glViewport(0,0,8,8); glEnable(GL_SCISSOR_TEST); glScissor(0,0,8,8);
  float v[6]={-1,-1, 1,-1, 0,1};
  if (argc > 3 && atoi(argv[3])) {
    GLuint vb; glGenBuffers(1, &vb); glBindBuffer(GL_ARRAY_BUFFER, vb);
    glBufferData(GL_ARRAY_BUFFER, sizeof(v), v, GL_STATIC_DRAW);
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,0);
  } else {
    glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,v);
  }
  glEnableVertexAttribArray(0);
  glClear(GL_COLOR_BUFFER_BIT);glFinish();
  double t0=now(); long total=0;
  for(int fr=0;fr<frames;fr++){ for(int i=0;i<n;i++) glDrawArrays(GL_TRIANGLES,0,3); glFinish(); total+=n; }
  double ms=now()-t0, per=ms*1000.0/total;
  printf("draws=%ld frames=%d total=%.1f ms  %.2f us/draw  %.0f draws/s\n", total, frames, ms, per, total/(ms/1000.0));
  return 0;
}
