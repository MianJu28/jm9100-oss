#include <stdio.h>
#include <string.h>
#include <GL/glx.h>
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>
static const char *vs="#version 130\nout vec2 uv; void main(){vec2 p=vec2(gl_VertexID&1,gl_VertexID>>1);uv=p;gl_Position=vec4(p*2.0-1.0,0,1);}";
static const char *fs="#version 130\nin vec2 uv; uniform usampler2D t; out vec4 o; void main(){uvec4 v=texture(t,uv);o=vec4(v)/65535.0;}";
static GLuint comp(GLenum ty,const char*s){GLuint h=glCreateShader(ty);glShaderSource(h,1,&s,0);glCompileShader(h);GLint ok;glGetShaderiv(h,GL_COMPILE_STATUS,&ok);if(!ok){char l[512];glGetShaderInfoLog(h,512,0,l);printf("shader: %s\n",l);}return h;}
int main(void){
  Display *d=XOpenDisplay(NULL); if(!d){puts("no display");return 2;}
  int at[]={GLX_RGBA,GLX_RED_SIZE,8,GLX_GREEN_SIZE,8,GLX_BLUE_SIZE,8,GLX_DEPTH_SIZE,24,None};
  XVisualInfo *vi=glXChooseVisual(d,0,at); if(!vi)return 3;
  GLXContext c=glXCreateContext(d,vi,0,True);
  Window w=XCreateSimpleWindow(d,RootWindow(d,vi->screen),0,0,1,1,0,0,0);
  glXMakeCurrent(d,w,c);
  GLuint p=glCreateProgram(); glAttachShader(p,comp(GL_VERTEX_SHADER,vs)); glAttachShader(p,comp(GL_FRAGMENT_SHADER,fs)); glLinkProgram(p);
  GLint ok; glGetProgramiv(p,GL_LINK_STATUS,&ok); if(!ok){char l[512];glGetProgramInfoLog(p,512,0,l);printf("link: %s\n",l);}
  GLuint fbo,to; glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_FRAMEBUFFER,fbo);
  glGenTextures(1,&to); glBindTexture(GL_TEXTURE_2D,to);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,0);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,to,0);
  unsigned short u16[2]={0x3C00,0x4228};
  unsigned short rg16[4]={0x3C00,0x4228,0x1111,0x2222};
  unsigned short rgba16[4]={0x8000,0x4000,0x2000,0xFFFF};
  struct {const char*n; GLenum ifmt; GLenum fmt; const void*d; int ch;} cases[]={
    {"R16UI",GL_R16UI,GL_RED_INTEGER,u16,1},{"RG16UI",GL_RG16UI,GL_RG_INTEGER,rg16,2},{"RGBA16UI",GL_RGBA16UI,GL_RGBA_INTEGER,rgba16,4}};
  for(int i=0;i<3;i++){
    GLuint t; glGenTextures(1,&t); glBindTexture(GL_TEXTURE_2D,t);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,0,cases[i].ifmt,1,1,0,cases[i].fmt,GL_UNSIGNED_SHORT,cases[i].d);
    GLenum e=glGetError();
    glBindFramebuffer(GL_FRAMEBUFFER,fbo); glViewport(0,0,1,1); glUseProgram(p);
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,t); glUniform1i(glGetUniformLocation(p,"t"),0);
    glDrawArrays(GL_TRIANGLE_STRIP,0,4);
    float v[4]; glReadPixels(0,0,1,1,GL_RGBA,GL_FLOAT,v);
    printf("%-8s err=0x%x sampled=%f %f %f %f\n",cases[i].n,e,v[0],v[1],v[2],v[3]);
  }
  return 0;
}
