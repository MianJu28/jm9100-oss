#include <stdio.h>
#include <string.h>
#include <GL/glx.h>
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
int main(void){
  Display *d=XOpenDisplay(NULL); if(!d){puts("no display");return 2;}
  int at[]={GLX_RGBA,GLX_RED_SIZE,8,GLX_GREEN_SIZE,8,GLX_BLUE_SIZE,8,GLX_DEPTH_SIZE,24,None};
  XVisualInfo *vi=glXChooseVisual(d,0,at); if(!vi)return 3;
  GLXContext c=glXCreateContext(d,vi,0,True);
  Window w=XCreateSimpleWindow(d,RootWindow(d,vi->screen),0,0,1,1,0,0,0);
  glXMakeCurrent(d,w,c);
  unsigned short in[4]={0x3C00,0x4228,0x1111,0x2222};
  GLuint t; glGenTextures(1,&t); glBindTexture(GL_TEXTURE_2D,t);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D,0,GL_R16,4,1,0,GL_RED,GL_UNSIGNED_SHORT,in);
  printf("upload err=0x%x\n", glGetError());
  unsigned short out[4]={0,0,0,0};
  glGetTexImage(GL_TEXTURE_2D,0,GL_RED,GL_UNSIGNED_SHORT,out);
  printf("readback err=0x%x: %04x %04x %04x %04x\n", glGetError(),out[0],out[1],out[2],out[3]);
  return 0;
}
