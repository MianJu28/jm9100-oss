/* buffer-bench: 量 buffer 的 CPU 侧读写 (glBufferSubData / glMapBufferRange),
 * 用来对比 buffer 放在 VRAM(write-combined 映射) 与 GTT(cached 映射) 的差别。
 * 用法: buffer-bench [轮数] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>

static double now_ms(void){ struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts);
   return ts.tv_sec*1000.0 + ts.tv_nsec/1e6; }

int main(int argc, char **argv){
   int n = argc>1 ? atoi(argv[1]) : 20;
   EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   eglInitialize(dpy,NULL,NULL);
   EGLint ca[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,
      EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_NONE};
   EGLConfig cfg; EGLint nc; eglChooseConfig(dpy,ca,&cfg,1,&nc);
   EGLint pa[]={EGL_WIDTH,64,EGL_HEIGHT,64,EGL_NONE};
   EGLint cattr[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
   EGLContext ctx=eglCreateContext(dpy,cfg,EGL_NO_CONTEXT,cattr);
   EGLSurface surf=eglCreatePbufferSurface(dpy,cfg,pa);
   if(!eglMakeCurrent(dpy,surf,surf,ctx)){ printf("makeCurrent 失败\n"); return 1; }
   printf("renderer: %s / %s\n", glGetString(GL_VERSION), glGetString(GL_RENDERER));

   size_t bytes = 4u<<20;
   unsigned char *buf = malloc(bytes); memset(buf,0x3C,bytes);
   GLuint b; glGenBuffers(1,&b); glBindBuffer(GL_ARRAY_BUFFER,b);
   glBufferData(GL_ARRAY_BUFFER,bytes,buf,GL_DYNAMIC_DRAW); glFinish();

   double t0=now_ms();
   for(int i=0;i<n;i++) glBufferSubData(GL_ARRAY_BUFFER,0,bytes,buf);
   glFinish();
   printf("glBufferSubData   4MB x%d: %7.2f ms (%6.2f GB/s)\n", n,
      (now_ms()-t0)/n, 4.0*n/((now_ms()-t0)/1000.0)/1024.0);

   t0=now_ms();
   for(int i=0;i<n;i++){
      void *p = glMapBufferRange(GL_ARRAY_BUFFER,0,bytes,GL_MAP_READ_BIT);
      if(!p){ printf("map READ 失败 err=0x%x\n", glGetError()); break; }
      memcpy(buf,p,bytes);
      glUnmapBuffer(GL_ARRAY_BUFFER);
   }
   glFinish();
   printf("map READ + memcpy 4MB x%d: %7.2f ms (%6.2f GB/s)\n", n,
      (now_ms()-t0)/n, 4.0*n/((now_ms()-t0)/1000.0)/1024.0);

   t0=now_ms();
   for(int i=0;i<n;i++){
      void *p = glMapBufferRange(GL_ARRAY_BUFFER,0,bytes,GL_MAP_WRITE_BIT|GL_MAP_INVALIDATE_BUFFER_BIT);
      if(!p){ printf("map WRITE 失败 err=0x%x\n", glGetError()); break; }
      memcpy(p,buf,bytes);
      glUnmapBuffer(GL_ARRAY_BUFFER);
   }
   glFinish();
   printf("map WRITE + memcpy 4MB x%d: %7.2f ms (%6.2f GB/s)\n", n,
      (now_ms()-t0)/n, 4.0*n/((now_ms()-t0)/1000.0)/1024.0);
   return 0;
}
