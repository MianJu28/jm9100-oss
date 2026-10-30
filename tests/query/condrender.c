#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>

typedef void (*PFNBCR)(GLuint, GLenum);
typedef void (*PFNECR)(void);

int main(void) {
  Display *d = XOpenDisplay(NULL);
  if (!d) { puts("no display"); return 2; }
  int attribs[] = { GLX_RGBA, GLX_DOUBLEBUFFER, None };
  XVisualInfo *vi = glXChooseVisual(d, DefaultScreen(d), attribs);
  if (!vi) { puts("no visual"); return 2; }
  GLXContext ctx = glXCreateContext(d, vi, NULL, True);
  Window root = RootWindow(d, vi->screen);
  XSetWindowAttributes swa; memset(&swa,0,sizeof swa);
  swa.colormap = XCreateColormap(d, root, vi->visual, AllocNone);
  Window win = XCreateWindow(d, root, 0,0, 64,64, 0, vi->depth, InputOutput, vi->visual,
                             CWBorderPixel|CWColormap, &swa);
  glXMakeCurrent(d, win, ctx);
  printf("GL_VERSION  = %s\n", glGetString(GL_VERSION));
  printf("GL_RENDERER = %s\n", glGetString(GL_RENDERER));
  const char *ext = (const char*)glGetString(GL_EXTENSIONS);
  printf("GL_NV_conditional_render : %s\n", (ext && strstr(ext,"GL_NV_conditional_render"))?"PRESENT":"absent");
  printf("GL_ARB_conditional_render_inverted : %s\n",
         (ext && strstr(ext,"GL_ARB_conditional_render_inverted"))?"PRESENT":"absent");
  PFNBCR pBCR = (PFNBCR)glXGetProcAddressARB((const GLubyte*)"glBeginConditionalRender");
  PFNECR pECR = (PFNECR)glXGetProcAddressARB((const GLubyte*)"glEndConditionalRender");
  printf("proc: begin=%p end=%p\n", (void*)pBCR, (void*)pECR);

  GLuint q=0; glGenQueries(1,&q);
  glBeginQuery(GL_SAMPLES_PASSED, q);
  glBegin(GL_TRIANGLES); glVertex2f(-1,-1); glVertex2f(3,-1); glVertex2f(-1,3); glEnd();
  glEndQuery(GL_SAMPLES_PASSED);
  GLuint r=0; glGetQueryObjectuiv(q, GL_QUERY_RESULT, &r);
  printf("occlusion query result = %u  (err=0x%x)\n", r, glGetError());

  printf(">>> 调用 glBeginConditionalRender(GL_QUERY_WAIT)\n"); fflush(stdout);
  pBCR(q, GL_QUERY_WAIT);
  printf(">>> begin 返回，err=0x%x\n", glGetError()); fflush(stdout);
  glClearColor(1,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
  pECR();
  printf(">>> end 返回，err=0x%x\n", glGetError()); fflush(stdout);
  printf("SURVIVED\n");
  return 0;
}
