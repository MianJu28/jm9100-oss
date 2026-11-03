#include <stdio.h>
#include <GL/glx.h>
#include <GL/gl.h>
#ifndef GL_MAX_VIEWPORTS
#define GL_MAX_VIEWPORTS 0x963B
#endif
int main(void){
  Display *d = XOpenDisplay(NULL);
  if (!d) return 2;
  int attribs[] = { GLX_RGBA, GLX_RED_SIZE,8, GLX_GREEN_SIZE,8, GLX_BLUE_SIZE,8, GLX_DEPTH_SIZE,24, None };
  XVisualInfo *vi = glXChooseVisual(d, 0, attribs);
  if (!vi) return 3;
  GLXContext c = glXCreateContext(d, vi, NULL, True);
  Window w = XCreateSimpleWindow(d, RootWindow(d, vi->screen), 0,0, 16,16, 0, 0, 0);
  glXMakeCurrent(d, w, c);
  GLint vp = -12345, err;
  glGetIntegerv(GL_MAX_VIEWPORTS, &vp);
  err = glGetError();
  printf("GL_MAX_VIEWPORTS=%d err=0x%x ext=%s\n", vp, err,
         (const char*)glGetString(GL_EXTENSIONS));
  return 0;
}
