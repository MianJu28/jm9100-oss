/* texbench: 量 glTexSubImage2D 的每调用开销。
 * 用法: texbench <texsize> <upsize> <count> <ntex> [finish_each]
 *   纹理 texsize x texsize RGBA8, 每次上传 upsize x upsize 到第 i%ntex 张纹理。
 *   默认在循环结束后 glFinish 一次; finish_each=1 则每次上传后 glFinish。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

static double now_ms(void){
   struct timespec ts; clock_gettime(CLOCK_MONOTONIC,&ts);
   return ts.tv_sec*1000.0 + ts.tv_nsec/1e6;
}

int main(int argc, char **argv){
   int texsize = argc>1 ? atoi(argv[1]) : 1024;
   int upsize  = argc>2 ? atoi(argv[2]) : 1024;
   int count   = argc>3 ? atoi(argv[3]) : 50;
   int ntex    = argc>4 ? atoi(argv[4]) : 1;
   int finish_each = argc>5 ? atoi(argv[5]) : 0;

   EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   if (!eglInitialize(dpy,NULL,NULL)) { fprintf(stderr,"eglInitialize failed\n"); return 1; }
   EGLint cfg_attr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE,
      EGL_OPENGL_ES2_BIT, EGL_RED_SIZE,8, EGL_GREEN_SIZE,8, EGL_BLUE_SIZE,8,
      EGL_ALPHA_SIZE,8, EGL_NONE };
   EGLConfig cfg; EGLint ncfg;
   if (!eglChooseConfig(dpy,cfg_attr,&cfg,1,&ncfg) || !ncfg) { fprintf(stderr,"no config\n"); return 1; }
   EGLint pb_attr[] = { EGL_WIDTH,64, EGL_HEIGHT,64, EGL_NONE };
   EGLContext ctx = eglCreateContext(dpy,cfg,EGL_NO_CONTEXT,NULL);
   EGLSurface surf = eglCreatePbufferSurface(dpy,cfg,pb_attr);
   if (!eglMakeCurrent(dpy,surf,surf,ctx)) { fprintf(stderr,"makeCurrent failed\n"); return 1; }
   printf("renderer: %s\n", glGetString(GL_RENDERER));

   GLuint *tex = calloc(ntex,sizeof(GLuint));
   glGenTextures(ntex,tex);
   for (int i=0;i<ntex;i++){
      glBindTexture(GL_TEXTURE_2D,tex[i]);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
      glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
      glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,texsize,texsize,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
   }
   glFinish();

   size_t bytes = (size_t)upsize*upsize*4;
   unsigned char *pix = malloc(bytes);
   memset(pix,0xA5,bytes);

   /* 预热一次, 排除首帧建 staging 的分配 */
   glBindTexture(GL_TEXTURE_2D,tex[0]);
   glTexSubImage2D(GL_TEXTURE_2D,0,0,0,upsize,upsize,GL_RGBA,GL_UNSIGNED_BYTE,pix);
   glFinish();

   double t0 = now_ms();
   for (int i=0;i<count;i++){
      glBindTexture(GL_TEXTURE_2D,tex[i%ntex]);
      glTexSubImage2D(GL_TEXTURE_2D,0,0,0,upsize,upsize,GL_RGBA,GL_UNSIGNED_BYTE,pix);
      if (finish_each) glFinish();
   }
   glFinish();
   double t1 = now_ms();

   double per = (t1-t0)/count;
   printf("tex=%d up=%d ntex=%d count=%d finish_each=%d : total=%.2f ms  per=%.2f ms  %.2f GB/s\n",
          texsize,upsize,ntex,count,finish_each,t1-t0,per,bytes*count/per/1e6/1e3);
   return 0;
}
