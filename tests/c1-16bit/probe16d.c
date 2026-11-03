#include <stdio.h>
#include <string.h>
#include <GL/glx.h>
#define GL_GLEXT_PROTOTYPES 1
#include <GL/gl.h>
#include <GL/glext.h>

static const char *vs =
"#version 130\nout vec2 uv; void main(){ vec2 p=vec2(gl_VertexID&1, gl_VertexID>>1); uv=p; gl_Position=vec4(p*2.0-1.0,0,1);}";
static const char *fs =
"#version 130\nin vec2 uv; uniform sampler2D t; out vec4 o; void main(){ o=texture(t,uv); }";

static GLuint compile(GLenum ty,const char*s){GLuint h=glCreateShader(ty);glShaderSource(h,1,&s,0);glCompileShader(h);return h;}

static void draw_read(GLuint tex, float out[16])
{
   static GLuint prog, fbo, texout; static int init;
   if(!init){
      GLuint v=compile(GL_VERTEX_SHADER,vs), f=compile(GL_FRAGMENT_SHADER,fs);
      GLint ok=0; glGetShaderiv(v,GL_COMPILE_STATUS,&ok); if(!ok){char l[512];glGetShaderInfoLog(v,512,0,l);printf("vs: %s\n",l);}
      glGetShaderiv(f,GL_COMPILE_STATUS,&ok); if(!ok){char l[512];glGetShaderInfoLog(f,512,0,l);printf("fs: %s\n",l);}
      prog=glCreateProgram(); glAttachShader(prog,v); glAttachShader(prog,f); glLinkProgram(prog);
      glGetProgramiv(prog,GL_LINK_STATUS,&ok); if(!ok){char l[512];glGetProgramInfoLog(prog,512,0,l);printf("link: %s\n",l);}
      glGenFramebuffers(1,&fbo); glBindFramebuffer(GL_FRAMEBUFFER,fbo);
      glGenTextures(1,&texout); glBindTexture(GL_TEXTURE_2D,texout);
      glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA32F,1,1,0,GL_RGBA,GL_FLOAT,0);
      glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texout,0);
      init=1;
   }
   glBindFramebuffer(GL_FRAMEBUFFER,fbo);
   { GLenum st=glCheckFramebufferStatus(GL_FRAMEBUFFER); if(st!=GL_FRAMEBUFFER_COMPLETE) printf("   fbo status=0x%x\n",st); }
   glViewport(0,0,1,1);
   glUseProgram(prog); glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D,tex);
   glUniform1i(glGetUniformLocation(prog,"t"),0);
   glDrawArrays(GL_TRIANGLE_STRIP,0,4);
   glReadPixels(0,0,1,1,GL_RGBA,GL_FLOAT,out);
   GLenum e=glGetError(); if(e) printf("   GL error 0x%x\n", e);
}

static void one(const char *name, GLenum ifmt, GLenum fmt, GLenum type, const void *data, int bpp, int w, int h)
{
   GLuint t; glGenTextures(1,&t); glBindTexture(GL_TEXTURE_2D,t);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
   glTexImage2D(GL_TEXTURE_2D,0,ifmt,w,h,0,fmt,type,data);
   GLenum e=glGetError();
   float v[16]; memset(v,0,sizeof v); draw_read(t,v);
   printf("%-18s err=0x%x  sampled=%.6f %.6f %.6f %.6f\n",name,e,v[0],v[1],v[2],v[3]);
}

int main(void)
{
   Display *d=XOpenDisplay(NULL); if(!d){puts("no display");return 2;}
   int at[]={GLX_RGBA,GLX_RED_SIZE,8,GLX_GREEN_SIZE,8,GLX_BLUE_SIZE,8,GLX_DEPTH_SIZE,24,None};
   XVisualInfo *vi=glXChooseVisual(d,0,at); if(!vi)return 3;
   GLXContext c=glXCreateContext(d,vi,0,True);
   Window w=XCreateSimpleWindow(d,RootWindow(d,vi->screen),0,0,1,1,0,0,0);
   glXMakeCurrent(d,w,c);
   printf("%s | %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));
   { GLuint ctl; unsigned char px[4]={255,64,16,255}; one("RGBA8_control",GL_RGBA8,GL_RGBA,GL_UNSIGNED_BYTE,px,4,1,1); (void)ctl; }

   unsigned short u16[2]={0x3C00,0xFFFF};
   short s16[2]={0x4000,(short)0xC000};
   unsigned short rg[4]={0x8000,0xFFFF,0x1234,0x5678};
   short rgs[4]={0x4000,(short)0xC000,0x2000,(short)0xE000};
   unsigned short rgba[4]={0x8000,0x4000,0x2000,0xFFFF};
   unsigned char r8[2]={0x80,0xFF};
   unsigned char rg8[4]={0x80,0xFF,0x40,0x20};
   unsigned short hf[2]={0x3C00,0x4000};

   one("R8_UNORM",    GL_R8,        GL_RED,  GL_UNSIGNED_BYTE,  r8, 1,1,1);
   one("RG8_UNORM",   GL_RG8,       GL_RG,   GL_UNSIGNED_BYTE,  rg8,2,1,1);
   one("R16F_data1.0",GL_R16F,      GL_RED,  GL_HALF_FLOAT,     hf, 2,1,1);
   one("R16_UNORM",   GL_R16,       GL_RED,  GL_UNSIGNED_SHORT, u16, 2,1,1);
   one("R16_SNORM",   GL_R16_SNORM, GL_RED,  GL_SHORT,          s16, 2,1,1);
   one("R16_UINT",    GL_R16UI,     GL_RED_INTEGER, GL_UNSIGNED_SHORT, u16, 2,1,1);
   one("R16_SINT",    GL_R16I,      GL_RED_INTEGER, GL_SHORT,          s16, 2,1,1);
   one("R16_FLOAT",   GL_R16F,      GL_RED,  GL_FLOAT,          0,0,1,1);
   one("RG16_UNORM",  GL_RG16,      GL_RG,   GL_UNSIGNED_SHORT, rg, 4,1,1);
   one("RG16_SNORM",  GL_RG16_SNORM,GL_RG,   GL_SHORT,          rgs,4,1,1);
   one("RGBA16_UNORM",GL_RGBA16,    GL_RGBA, GL_UNSIGNED_SHORT, rgba,8,1,1);
   return 0;
}
