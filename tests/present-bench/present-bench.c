#include <epoxy/gl.h>
#include <epoxy/glx.h>
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
static double now(void){struct timespec a;clock_gettime(CLOCK_MONOTONIC,&a);return a.tv_sec*1e3+a.tv_nsec/1e6;}
int main(int argc,char**argv){
  int w=atoi(argv[1]),h=atoi(argv[2]),n=atoi(argv[3]),draw=atoi(argv[4]);
  Display *d=XOpenDisplay(NULL); int scr=DefaultScreen(d);
  int attr[]={GLX_RGBA,GLX_DOUBLEBUFFER,GLX_RED_SIZE,8,GLX_GREEN_SIZE,8,GLX_BLUE_SIZE,8,None};
  XVisualInfo *vi=glXChooseVisual(d,scr,attr);
  Window win=XCreateSimpleWindow(d,RootWindow(d,scr),0,0,w,h,0,0,0); XMapWindow(d,win); XSync(d,0);
  GLXContext c=glXCreateContext(d,vi,NULL,True); glXMakeCurrent(d,win,c);
  glViewport(0,0,w,h);
  for(int i=0;i<100;i++){glClear(GL_COLOR_BUFFER_BIT);glXSwapBuffers(d,win);}
  double t0=now();
  for(int i=0;i<n;i++){ if(draw){glClear(GL_COLOR_BUFFER_BIT);} glXSwapBuffers(d,win); }
  double ms=(now()-t0)/n;
  printf("%dx%d %s: %6.3f ms/frame (%7.1f FPS)\n",w,h,draw?"clear+swap":"swap-only",ms,1000/ms);
  return 0;
}
