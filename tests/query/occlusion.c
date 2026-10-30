#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
static void draw_tri(void){
  glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT); glClear(GL_DEPTH_BUFFER_BIT);
  glBegin(GL_TRIANGLES); glVertex2f(-1,-1); glVertex2f(3,-1); glVertex2f(-1,3); glEnd();
  glFinish();
}
int main(int argc,char**argv){
  int mode = argc>1?atoi(argv[1]):0;
  Display *d=XOpenDisplay(NULL); if(!d){puts("no display");return 2;}
  int at[]={GLX_RGBA,GLX_DOUBLEBUFFER,GLX_DEPTH_SIZE,24,None};
  XVisualInfo *vi=glXChooseVisual(d,DefaultScreen(d),at);
  GLXContext ctx=glXCreateContext(d,vi,NULL,True);
  Window root=RootWindow(d,vi->screen); XSetWindowAttributes swa; memset(&swa,0,sizeof swa);
  swa.colormap=XCreateColormap(d,root,vi->visual,AllocNone);
  Window win=XCreateWindow(d,root,0,0,64,64,0,vi->depth,InputOutput,vi->visual,CWBorderPixel|CWColormap,&swa);
  glXMakeCurrent(d,win,ctx); glViewport(0,0,64,64);
  GLuint q=0; glGenQueries(1,&q);

  if(mode==0){ /* 连续 5 次遮挡查询，看是否任何一个槽位被写 */
    for(int k=0;k<5;k++){
      glBeginQuery(GL_SAMPLES_PASSED,q); draw_tri(); glEndQuery(GL_SAMPLES_PASSED);
      GLuint r=0; glGetQueryObjectuiv(q,GL_QUERY_RESULT,&r);
      printf("  round %d: GL_SAMPLES_PASSED=%u (expect ~4096) err=0x%x\n",k,r,glGetError());
    }
  } else if(mode==1){ /* 谓词版 */
    glBeginQuery(GL_ANY_SAMPLES_PASSED,q); draw_tri(); glEndQuery(GL_ANY_SAMPLES_PASSED);
    GLuint r=0; glGetQueryObjectuiv(q,GL_QUERY_RESULT,&r);
    printf("  GL_ANY_SAMPLES_PASSED=%u (expect 1) err=0x%x\n",r,glGetError());
  } else if(mode==2){ /* 顶点数（pipeline statistics）*/
    const char *e=(const char*)glGetString(GL_EXTENSIONS);
    printf("  ARB_pipeline_statistics_query=%s ARB_transform_feedback2=%s\n",
      (e&&strstr(e,"GL_ARB_pipeline_statistics_query"))?"yes":"no",
      (e&&strstr(e,"GL_ARB_transform_feedback2"))?"yes":"no");
    glBeginQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN,q); draw_tri();
    glEndQuery(GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN);
    GLuint r=0; glGetQueryObjectuiv(q,GL_QUERY_RESULT,&r);
    printf("  GL_TRANSFORM_FEEDBACK_PRIMITIVES_WRITTEN=%u (expect 1) err=0x%x\n",r,glGetError());
  } else if(mode==3){ /* 深度测试开启，看是否只在 depth 路径计数 */
    glEnable(GL_DEPTH_TEST);
    glBeginQuery(GL_SAMPLES_PASSED,q); draw_tri(); glEndQuery(GL_SAMPLES_PASSED);
    GLuint r=0; glGetQueryObjectuiv(q,GL_QUERY_RESULT,&r);
    printf("  depth-on GL_SAMPLES_PASSED=%u (expect ~4096)\n",r);
  }
  return 0;
}
