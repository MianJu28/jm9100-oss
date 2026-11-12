/* readback-scale: 量 glReadPixels 的耗时随读回字节数的变化, 判断是带宽还是固定开销。
 * 用法: readback-scale  (内部循环 128/256/512/1024/2048 五种 pbuffer) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

static double now_ms(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts);
   return ts.tv_sec*1000.0 + ts.tv_nsec/1e6; }

static EGLDisplay dpy;
static EGLConfig cfg;

static void run(int n, int rounds)
{
   int bytes = n*n*4;
   unsigned char *buf = malloc(bytes);
   EGLint pa[]={EGL_WIDTH,n,EGL_HEIGHT,n,EGL_NONE};
   EGLint cattr[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
   EGLContext ctx=eglCreateContext(dpy,cfg,EGL_NO_CONTEXT,cattr);
   EGLSurface surf=eglCreatePbufferSurface(dpy,cfg,pa);
   eglMakeCurrent(dpy,surf,surf,ctx);
   glClearColor(0.1f,0.2f,0.3f,1); glClear(GL_COLOR_BUFFER_BIT); glFinish();
   glReadPixels(0,0,n,n,GL_RGBA,GL_UNSIGNED_BYTE,buf);   /* 预热 */
   double t0=now_ms();
   for(int i=0;i<rounds;i++) glReadPixels(0,0,n,n,GL_RGBA,GL_UNSIGNED_BYTE,buf);
   glFinish();
   double ms=(now_ms()-t0)/rounds;
   printf("%4dx%-4d %6.2f MB  %8.3f ms/次  %6.2f GB/s  px=%d,%d,%d err=0x%x\n",
      n,n, bytes/1048576.0, ms, (bytes/1048576.0)/ms*1000.0/1024.0,
      buf[0],buf[1],buf[2], glGetError());
   fflush(stdout);
   eglMakeCurrent(dpy,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);
   eglDestroySurface(dpy,surf); eglDestroyContext(dpy,ctx);
   free(buf);
}

int main(int argc, char **argv){
   int rounds = argc>1 ? atoi(argv[1]) : 20;
   dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   eglInitialize(dpy,NULL,NULL);
   EGLint ca[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,
      EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};
   EGLint nc; eglChooseConfig(dpy,ca,&cfg,1,&nc);
   int sizes[]={128,256,512,1024,2048};
   for (unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);i++) run(sizes[i],rounds);
   return 0;
}