/* texcheck: 验证 staging 复用后上传内容不会撕裂/串数据。
 * 每轮: 整张纹理上传模式 A, 再整张上传模式 B, 立刻读回并逐字节比对是否全为 B。
 * 同时做小块更替 (64x64) 的快速交叉验证。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <EGL/egl.h>
#include <GLES2/gl2.h>

#define SZ 1024
#define ROUNDS 12
#define CK(s) do{ GLenum e_=glGetError(); if(e_) printf("[GLERR] %s -> 0x%x\n", s, e_); }while(0)

int main(void){
   EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   if (!eglInitialize(dpy,NULL,NULL)) { fprintf(stderr,"eglInitialize failed\n"); return 1; }
   EGLint cfg_attr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE,
      EGL_OPENGL_ES2_BIT, EGL_RED_SIZE,8, EGL_GREEN_SIZE,8, EGL_BLUE_SIZE,8,
      EGL_ALPHA_SIZE,8, EGL_NONE };
   EGLConfig cfg; EGLint ncfg;
   eglChooseConfig(dpy,cfg_attr,&cfg,1,&ncfg);
   EGLint pb_attr[] = { EGL_WIDTH,SZ, EGL_HEIGHT,SZ, EGL_NONE };
   /* 必须显式要 ES2 上下文: 传 NULL 属性时本驱动给出一个 dispatch 全是 noop
    * 的上下文 (所有 GL 调用报 "unsupported function called")。 */
   EGLint ctx_attr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
   EGLContext ctx = eglCreateContext(dpy,cfg,EGL_NO_CONTEXT,ctx_attr);
   EGLSurface surf = eglCreatePbufferSurface(dpy,cfg,pb_attr);
   eglMakeCurrent(dpy,surf,surf,ctx);
   CK("init");
   printf("renderer: %s\n", glGetString(GL_RENDERER));

   GLuint tex;
   glGenTextures(1,&tex);
   glBindTexture(GL_TEXTURE_2D,tex);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
   glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,SZ,SZ,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);

   /* 直接渲染到 1024x1024 的 pbuffer 上再读回, 避开 renderbuffer 扩展问题 */

   /* 用纹理渲染到 FBO, 然后 glReadPixels 读回 */
   const char *vs =
      "attribute vec2 p; varying vec2 uv;"
      "void main(){ uv=p*0.5+0.5; gl_Position=vec4(p,0.0,1.0); }";
   const char *fs =
      "precision mediump float; varying vec2 uv; uniform sampler2D t;"
      "void main(){ gl_FragColor=texture2D(t,uv); }";
   GLuint p = glCreateProgram();
   GLuint v = glCreateShader(GL_VERTEX_SHADER), f = glCreateShader(GL_FRAGMENT_SHADER);
   glShaderSource(v,1,&vs,NULL); glCompileShader(v);
   glShaderSource(f,1,&fs,NULL); glCompileShader(f);
   glAttachShader(p,v); glAttachShader(p,f); glLinkProgram(p); glUseProgram(p);
   GLuint loc = glGetAttribLocation(p,"p"); CK("attribloc");
   static const float quad[] = { -1,-1, 1,-1, -1,1, 1,1 };
   glEnableVertexAttribArray(loc);
   glVertexAttribPointer(loc,2,GL_FLOAT,GL_FALSE,0,quad);
   glUniform1i(glGetUniformLocation(p,"t"),0);
   glActiveTexture(GL_TEXTURE0);
   glBindTexture(GL_TEXTURE_2D,tex);
   glViewport(0,0,SZ,SZ); CK("setup");

   size_t bytes = (size_t)SZ*SZ*4;
   unsigned char *src = malloc(bytes), *dst = malloc(bytes);
   int fail = 0;

   for (int r=0; r<ROUNDS && !fail; r++) {
      unsigned char a = (unsigned char)(r*2+1), b = (unsigned char)(r*2+2);
      memset(src,a,bytes);
      glBindTexture(GL_TEXTURE_2D,tex);
      glTexSubImage2D(GL_TEXTURE_2D,0,0,0,SZ,SZ,GL_RGBA,GL_UNSIGNED_BYTE,src); CK("upA");
      memset(src,b,bytes);
      glTexSubImage2D(GL_TEXTURE_2D,0,0,0,SZ,SZ,GL_RGBA,GL_UNSIGNED_BYTE,src); CK("upA");

      /* 小块上传: 只覆盖左上 64x64 区域为 c */
      unsigned char c = (unsigned char)(r*2+3);
      static unsigned char small[64*64*4];
      memset(small,c,sizeof(small));
      glTexSubImage2D(GL_TEXTURE_2D,0,0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,small);

      glDrawArrays(GL_TRIANGLE_STRIP,0,4); CK("draw");
      glFinish();
      glReadPixels(0,0,SZ,SZ,GL_RGBA,GL_UNSIGNED_BYTE,dst); CK("read");

      int bad = 0, firstx=-1, firsty=-1;
      for (int y=0;y<SZ && !bad;y++)
         for (int x=0;x<SZ;x++) {
            unsigned char exp = (y<64 && x<64) ? c : b; /* GL 原点在左下; 小块覆盖 (0,0)-(63,63) */
            if (dst[((size_t)y*SZ+x)*4] != exp) { bad=1; firstx=x; firsty=y; break; }
         }
      printf("round %2d: a=%3d b=%3d c=%3d  %s%s", r,a,b,c, bad?"MISMATCH":"ok",
             bad?"": "\n");
      if (bad) { printf("  at (%d,%d) got %d\n",firstx,firsty,dst[((size_t)firsty*SZ+firstx)*4]); fail=1; }
      fflush(stdout);
   }
   /* 第二阶段: 多张纹理背靠背小块上传 (不插入任何同步), 结束后逐张读回校验。
    * staging 复用时若在 GPU 读完前就被下一次上传覆盖, 这里会看到串数据。 */
   if (!fail) {
      GLuint ts[8];
      glGenTextures(8,ts);
      for (int t=0;t<8;t++){
         glBindTexture(GL_TEXTURE_2D,ts[t]);
         glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
         glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
         glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,SZ,SZ,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
      }
      static unsigned char pat[64*64*4];
      for (int pass=0; pass<4; pass++)
         for (int t=0;t<8;t++){
            memset(pat, pass*8+t+1, sizeof(pat));
            glBindTexture(GL_TEXTURE_2D,ts[t]);
            glTexSubImage2D(GL_TEXTURE_2D,0,0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,pat);
         }
      for (int t=0;t<8;t++){
         unsigned char want = (unsigned char)(3*8+t+1);
         glBindTexture(GL_TEXTURE_2D,ts[t]);
         glDrawArrays(GL_TRIANGLE_STRIP,0,4);
         glFinish();
         unsigned char px[4]={0,0,0,0};
         glReadPixels(3,3,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
         if (px[0]!=want){ printf("stress tex %d: got %d want %d  MISMATCH\n",t,px[0],want); fail=1; }
      }
      if (!fail) printf("stress: 8 textures x 4 passes ok\n");
   }

   printf(fail? "RESULT: FAIL\n" : "RESULT: PASS (%d rounds + stress)\n", ROUNDS);
   return fail;
}
