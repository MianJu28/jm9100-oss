#include <EGL/egl.h>
#include <gbm.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
static double now(){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
int main(int argc,char**argv){
  int passes=argc>1?atoi(argv[1]):1;
  int N=argc>2?atoi(argv[2]):2000;
  int fd=open("/dev/dri/renderD128",O_RDWR);
  struct gbm_device*gbm=gbm_create_device(fd);
  EGLDisplay d=eglGetPlatformDisplay(0x31D7,(void*)gbm,NULL);
  eglInitialize(d,NULL,NULL);
  eglBindAPI(EGL_OPENGL_ES_API);
  EGLConfig c;EGLint n;EGLint attr[]={EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES2_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};
  eglChooseConfig(d,attr,&c,1,&n);
  struct gbm_surface*gs=gbm_surface_create(gbm,1600,1200,GBM_FORMAT_ARGB8888,GBM_BO_USE_RENDERING);
  EGLSurface s=eglCreateWindowSurface(d,c,(EGLNativeWindowType)gs,NULL);
  EGLint ctxattr[]={EGL_CONTEXT_CLIENT_VERSION,2,EGL_NONE};
  EGLContext ctx=eglCreateContext(d,c,EGL_NO_CONTEXT,ctxattr);
  eglMakeCurrent(d,s,s,ctx);
  printf("GL %s | %s\n",glGetString(GL_VERSION),glGetString(GL_RENDERER));
  GLuint fbt,ctxr;
  glGenFramebuffers(1,&fbt);glBindFramebuffer(GL_FRAMEBUFFER,fbt);
  glGenTextures(1,&ctxr);glBindTexture(GL_TEXTURE_2D,ctxr);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1600,1200,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,ctxr,0);
  if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE){printf("fbo fail\n");return 1;}
  const char*vs="attribute vec2 p;attribute vec2 uv;varying vec2 t;uniform float u_f;void main(){gl_Position=vec4(p,0.,1.);t=uv+vec2(u_f);}";
  char fsbuf[8192];
  const char*head="precision mediump float;varying vec2 t;void main(){vec4 c=vec4(t.x,t.y,.5,1.);";
  const char*tail="gl_FragColor=c;}";
  char body[6400]="";for(int i=0;i<passes;i++)strcat(body,"c=c*0.99+0.004;");
  snprintf(fsbuf,sizeof(fsbuf),"%s%s%s",head,body,tail);
  GLuint v=glCreateShader(GL_VERTEX_SHADER);{const GLchar*const s0[1]={vs};glShaderSource(v,1,s0,NULL);}glCompileShader(v);
  GLuint f=glCreateShader(GL_FRAGMENT_SHADER);{const GLchar*const s0[1]={fsbuf};glShaderSource(f,1,s0,NULL);}glCompileShader(f);
  {GLint ok;glGetShaderiv(f,GL_COMPILE_STATUS,&ok);if(!ok){char log[1024];glGetShaderInfoLog(f,1024,0,log);printf("fs fail: %s\n",log);return 1;}}
  GLuint pr=glCreateProgram();glAttachShader(pr,v);glAttachShader(pr,f);glLinkProgram(pr);glUseProgram(pr);
  GLint lu_f=glGetUniformLocation(pr,"u_f");
  GLfloat verts[]={-1,-1,0,0, 3,-1,2,0, -1,3,0,2};
  GLuint b;glGenBuffers(1,&b);glBindBuffer(GL_ARRAY_BUFFER,b);
  glBufferData(GL_ARRAY_BUFFER,sizeof(verts),verts,GL_STATIC_DRAW);
  GLint lp=glGetAttribLocation(pr,"p"),lu=glGetAttribLocation(pr,"uv");
  glEnableVertexAttribArray(lp);glVertexAttribPointer(lp,2,GL_FLOAT,0,16,0);
  glEnableVertexAttribArray(lu);glVertexAttribPointer(lu,2,GL_FLOAT,0,16,(void*)8);
  GLubyte probe[4];
  double t0=now();
  for(int i=0;i<N;i++){
    glUniform1f(lu_f,(float)(i&0xFF)/256.0f);
    glDrawArrays(GL_TRIANGLES,0,3);glFinish();
  }
  double t1=now();
  glReadPixels(400,300,1,1,GL_RGBA,GL_UNSIGNED_BYTE,probe);
  printf("probe: %d,%d,%d,%d\n",probe[0],probe[1],probe[2],probe[3]);
  double pxps=(double)N*1600*1200*passes/(t1-t0);
  printf("uniform+draw+finish %d-pass 800x600 FBO: %.1f FPS (%.3f ms) → %.2f Gpx-eq/s\n",passes,N/(t1-t0),(t1-t0)*1000/N,pxps/1e9);
  return 0;
}
