/* 验证遮挡查询真的在计数：不同大小的三角形应给出不同计数 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
static void run(Display*d,GLXContext c,Window w,float scale,const char*tag){
  glXMakeCurrent(d,w,c); glViewport(0,0,64,64);
  glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  GLuint q=0; glGenQueries(1,&q);
  glBeginQuery(GL_SAMPLES_PASSED,q);
  glBegin(GL_TRIANGLES);
  glVertex2f(-1,-1); glVertex2f(-1+2*scale,-1); glVertex2f(-1,-1+2*scale);
  glEnd();
  glEndQuery(GL_SAMPLES_PASSED);
  GLuint r=0; glGetQueryObjectuiv(q,GL_QUERY_RESULT,&r);
  printf("  %-12s scale=%.2f  count=%u\n", tag, scale, r);
}
int main(void){
  Display *d=XOpenDisplay(NULL); if(!d){puts("no display");return 2;}
  int at[]={GLX_RGBA,GLX_DOUBLEBUFFER,GLX_DEPTH_SIZE,24,None};
  XVisualInfo *vi=glXChooseVisual(d,DefaultScreen(d),at);
  GLXContext ctx=glXCreateContext(d,vi,NULL,True);
  Window root=RootWindow(d,vi->screen); XSetWindowAttributes swa; memset(&swa,0,sizeof swa);
  swa.colormap=XCreateColormap(d,root,vi->visual,AllocNone);
  Window win=XCreateWindow(d,root,0,0,64,64,0,vi->depth,InputOutput,vi->visual,CWBorderPixel|CWColormap,&swa);
  run(d,ctx,win,0.25f,"1/4 屏"); run(d,ctx,win,0.5f,"半屏"); run(d,ctx,win,1.0f,"全屏");
  return 0;
}
