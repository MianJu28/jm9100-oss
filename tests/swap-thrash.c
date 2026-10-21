#include <EGL/egl.h>
#include <gbm.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
static double now(){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
int main(int argc,char**argv){
  int W=argc>1?atoi(argv[1]):800, H=argc>2?atoi(argv[2]):600;
  int N=argc>3?atoi(argv[3]):2000;
  int fd=open("/dev/dri/renderD128",O_RDWR);
  struct gbm_device*gbm=gbm_create_device(fd);
  EGLDisplay d=eglGetPlatformDisplay(0x31D7,(void*)gbm,NULL);
  eglInitialize(d,NULL,NULL);
  eglBindAPI(EGL_OPENGL_ES_API);
  EGLConfig c;EGLint n;EGLint attr[]={EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};
  eglChooseConfig(d,attr,&c,1,&n);
  struct gbm_surface*gs=gbm_surface_create(gbm,W,H,GBM_FORMAT_ARGB8888,GBM_BO_USE_RENDERING);
  EGLSurface s=eglCreateWindowSurface(d,c,(EGLNativeWindowType)gs,NULL);
  EGLint ctxattr[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
  EGLContext ctx=eglCreateContext(d,c,EGL_NO_CONTEXT,ctxattr);
  eglMakeCurrent(d,s,s,ctx);
  double t0=now();
  for(int i=0;i<N;i++){
    glClearColor((i&1)?1.f:0.f,0.5f,0.25f,1.f);
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(d,s);
  }
  double t1=now();
  printf("clear+swap %dx%d: %.1f FPS (%.3f ms) → %.2f GB/s\n",W,H,N/(t1-t0),(t1-t0)*1000/N,(double)N*W*H*4/(t1-t0)/1e9);
  return 0;
}
