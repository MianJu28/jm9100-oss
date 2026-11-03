#include <stdio.h>
#include <string.h>
#include <GL/glx.h>
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
static const char *vs="#version 130\nuniform sampler2D t; out vec4 c; void main(){vec2 p=vec2(gl_VertexID&1,gl_VertexID>>1);c=textureLod(t,vec2(0.5),0.0);gl_Position=vec4(p*2.0-1.0,0,1);}";
static const char *fs="#version 130\nin vec4 c; out vec4 o; void main(){o=c;}";
static GLuint comp(GLenum ty,const char*s){GLuint h=glCreateShader(ty);glShaderSource(h,1,&s,0);glCompileShader(h);GLint ok;glGetShaderiv(h,GL_COMPILE_STATUS,&ok);if(!ok){char l[512];glGetShaderInfoLog(h,512,0,l);printf("shader %s\n",l);}return h;}
int main(void){
  Display *d=XOpenDisplay(NULL); if(!d)return 2;
  int at[]={GLX_RGBA,GLX_RED_SIZE,8,GLX_GREEN_SIZE,8,GLX_BLUE_SIZE,8,GLX_DEPTH_SIZE,24,None};
  XVisualInfo *vi=glXChooseVisual(d,0,at); GLXContext c=glXCreateContext(d,vi,0,True);
  Window w=XCreateSimpleWindow(d,RootWindow(d,vi->screen),0,0,1,1,0,0,0); glXMakeCurrent(d,w,c);
  GLuint p=glCreateProgram(); glAttachShader(p,comp(GL_VERTEX_SHADER,vs)); glAttachShader(p,comp(GL_FRAGMENT_SHADER,fs)); glLinkProgram(p);
  GLuint fbo,to; glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_FRAMEBUFFER,fbo);
  glGenTextures(1,&to); glBindTexture(GL_TEXTURE_2D,to); glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,0);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,to,0);
  unsigned short u[2]={0x3C00,0};
  GLuint t; glGenTextures(1,&t); glBindTexture(GL_TEXTURE_2D,t);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D,0,GL_R16,1,1,0,GL_RED,GL_UNSIGNED_SHORT,u);
  printf("upload err=0x%x\n",glGetError());
  glViewport(0,0,1,1); glUseProgram(p); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,t);
  glUniform1i(glGetUniformLocation(p,"t"),0); glDrawArrays(GL_TRIANGLE_STRIP,0,4);
  float v[4]; glReadPixels(0,0,1,1,GL_RGBA,GL_FLOAT,v);
  printf("vertex-sample: %f %f %f %f err=0x%x\n",v[0],v[1],v[2],v[3],glGetError());
  return 0;
}
