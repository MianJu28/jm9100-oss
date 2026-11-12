/* readback-bench: 量 glReadPixels (glReadPixels 走 transfer_map 的读方向) 与
 * glBufferSubData 的固定开销。用法: readback-bench [轮数] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

static double now_ms(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts);
   return ts.tv_sec*1000.0 + ts.tv_nsec/1e6; }

int main(int argc, char **argv){
   int n = argc>1 ? atoi(argv[1]) : 50;
   EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   eglInitialize(dpy,NULL,NULL);
   EGLint ca[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,
      EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};
   EGLConfig cfg; EGLint nc; eglChooseConfig(dpy,ca,&cfg,1,&nc);
   EGLint pa[]={EGL_WIDTH,1024,EGL_HEIGHT,1024,EGL_NONE};
   EGLint cattr[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
   EGLContext ctx=eglCreateContext(dpy,cfg,EGL_NO_CONTEXT,cattr);
   EGLSurface surf=eglCreatePbufferSurface(dpy,cfg,pa);
   eglMakeCurrent(dpy,surf,surf,ctx);
   printf("renderer: %s\n", glGetString(GL_RENDERER));

   unsigned char *buf = malloc(1024*1024*4);
   /* 小读回 64x64 */
   glClearColor(0.1f,0.2f,0.3f,1); glClear(GL_COLOR_BUFFER_BIT); glFinish();
   glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,buf);   /* 预热 */
   double t0=now_ms();
   for(int i=0;i<n;i++) glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,buf);
   glFinish();
   printf("readback 64x64 x%d: %.2f ms/次\n", n, (now_ms()-t0)/n);

   /* 全屏读回 1024x1024 = 4 MB */
   t0=now_ms();
   for(int i=0;i<n/10;i++) glReadPixels(0,0,1024,1024,GL_RGBA,GL_UNSIGNED_BYTE,buf);
   glFinish();
   printf("readback 1024x1024 x%d: %.2f ms/次 (%.2f GB/s) err=0x%x px=%d,%d,%d\n", n/10,
      (now_ms()-t0)/(n/10), 4.0*(n/10)/((now_ms()-t0)/1000.0)/1024.0,
      glGetError(), buf[0],buf[1],buf[2]);
   /* buffer 上传 */
   GLuint b; glGenBuffers(1,&b); glBindBuffer(GL_ARRAY_BUFFER,b);
   glBufferData(GL_ARRAY_BUFFER,4*1024*1024,NULL,GL_DYNAMIC_DRAW);
   glBufferSubData(GL_ARRAY_BUFFER,0,4*1024*1024,buf); glFinish();
   t0=now_ms();
   for(int i=0;i<n;i++) glBufferSubData(GL_ARRAY_BUFFER,0,4*1024*1024,buf);
   glFinish();
   printf("glBufferSubData 4MB x%d: %.2f ms/次 (%.2f GB/s)\n", n,
      (now_ms()-t0)/n, 4.0*n/((now_ms()-t0)/1000.0)/1024.0);

   return 0;
}
