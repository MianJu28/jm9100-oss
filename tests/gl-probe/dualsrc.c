/* dualsrc.c —— 查询 GL_MAX_DUAL_SOURCE_DRAW_BUFFERS 与 GL_EXT_blend_func_extended。
 * 用法: dualsrc        (期望 MAX=0, ext=no —— 驱动默认关闭双源混合)
 *       MWV207_DUALSRC=1 dualsrc  (实验用, 期望 MAX=1, ext=yes)
 * 编译: cc -O2 -o dualsrc tests/gl-probe/dualsrc.c -lEGL -lGLESv2
 */
#include <string.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <stdio.h>
int main(void){
  EGLDisplay d=eglGetDisplay(EGL_DEFAULT_DISPLAY); eglInitialize(d,NULL,NULL);
  EGLint ca[]={EGL_SURFACE_TYPE,EGL_PBUFFER_BIT,EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_NONE};
  EGLConfig c; EGLint n; eglChooseConfig(d,ca,&c,1,&n);
  EGLint pb[]={EGL_WIDTH,16,EGL_HEIGHT,16,EGL_NONE};
  EGLSurface s=eglCreatePbufferSurface(d,c,pb);
  EGLint xa[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
  EGLContext x=eglCreateContext(d,c,EGL_NO_CONTEXT,xa); eglMakeCurrent(d,s,s,x);
  GLint v=0; glGetIntegerv(0x88FC,&v);
  printf("MAX_DUAL_SOURCE_DRAW_BUFFERS=%d blend_func_extended=%s\n", v,
         strstr((const char*)glGetString(GL_EXTENSIONS),"GL_EXT_blend_func_extended")?"yes":"no");
  return 0;
}
