/* dmabuf.c - EGL_MESA_image_dma_buf_export / import 往返探针
 * 目的: 验证 MWV207 驱动能否把一个 GL 纹理导出成 dmabuf, 再导入回来采样,
 *       并打印 fourcc / modifier / planes, 供 Firefox DMABUF_SURFACE_EXPORT 评估。
 */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>

static PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC p_query;
static PFNEGLEXPORTDMABUFIMAGEMESAPROC p_export;
static PFNEGLCREATEIMAGEKHRPROC p_create_image;
static PFNEGLDESTROYIMAGEKHRPROC p_destroy_image;
typedef void (*PFN_TARGETTEX)(GLenum, void *);
static PFN_TARGETTEX p_target_tex;

/* glvnd 的 eglGetProcAddress 只有在厂商 libEGL 被 dlopen 之后才返回扩展函数;
 * 先按 /usr/share/glvnd/egl_vendor.d/*.json 把第一个可用的厂商库预打开。 */
static void preload_egl_vendor(void)
{
   static const char *paths[] = {
      "/usr/local/lib/mwv207-oss/libEGL_jm.so.0.0.0",
      "/usr/lib/aarch64-linux-gnu/libEGL_mwv207.so.1.0.0",
      "/usr/lib/aarch64-linux-gnu/libEGL_mesa.so.0",
      NULL
   };
   for (int i = 0; paths[i]; i++)
      if (dlopen(paths[i], RTLD_NOW | RTLD_GLOBAL))
         return;
}

/* 注意: 参数是 (变量, 类型, 函数名) —— 之前写成 #name 会把变量名当函数名查,
 * eglGetProcAddress("p_query") 当然返回 NULL。 */
#define LOAD(var, type, fn) do { var = (type)eglGetProcAddress(fn); \
   if (!var) { preload_egl_vendor(); var = (type)eglGetProcAddress(fn); } \
   if (!var) { printf("FAIL: 缺少入口 %s (eglGetProcAddress 为空)\n", fn); return 1; } } while (0)

int main(void)
{
   EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   if (!eglInitialize(dpy, NULL, NULL)) { printf("FAIL: eglInitialize\n"); return 1; }

   printf("RENDERER: %s\n", glGetString(GL_RENDERER) ? (const char *)glGetString(GL_RENDERER) : "(未初始化)");

   EGLint cfg_attr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                         EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                         EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
                         EGL_NONE };
   EGLConfig cfg; EGLint n = 0;
   if (!eglChooseConfig(dpy, cfg_attr, &cfg, 1, &n) || n < 1) { printf("FAIL: eglChooseConfig\n"); return 1; }
   EGLint pb[] = { EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE };
   EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, pb);
   EGLint ca[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
   EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ca);
   if (!ctx || !eglMakeCurrent(dpy, surf, surf, ctx)) { printf("FAIL: makeCurrent\n"); return 1; }
   printf("RENDERER(中间): %s\n", (const char *)glGetString(GL_RENDERER));

   LOAD(p_query, PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC, "eglExportDMABUFImageQueryMESA");
   LOAD(p_export, PFNEGLEXPORTDMABUFIMAGEMESAPROC, "eglExportDMABUFImageMESA");
   LOAD(p_create_image, PFNEGLCREATEIMAGEKHRPROC, "eglCreateImageKHR");
   LOAD(p_destroy_image, PFNEGLDESTROYIMAGEKHRPROC, "eglDestroyImageKHR");
   LOAD(p_target_tex, PFN_TARGETTEX, "glEGLImageTargetTexture2DOES");

   /* 渲染一张 32x32 的图案到纹理 */
   const int W = 32, H = 32;
   GLuint tex;
   glGenTextures(1, &tex);
   glBindTexture(GL_TEXTURE_2D, tex);
   glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

   GLuint fbo, rb;
   glGenFramebuffers(1, &fbo);
   glBindFramebuffer(GL_FRAMEBUFFER, fbo);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
   if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) { printf("FAIL: FBO 不完整\n"); return 1; }
   glViewport(0, 0, W, H);
   glClearColor(0.25f, 0.5f, 0.75f, 1.0f);
   glClear(GL_COLOR_BUFFER_BIT);
   glFinish();

   glBindTexture(GL_TEXTURE_2D, tex);
   EGLImageKHR img = p_create_image(dpy, ctx, EGL_GL_TEXTURE_2D_KHR,
                                    (EGLClientBuffer)(uintptr_t)tex, NULL);
   if (img == EGL_NO_IMAGE_KHR) { printf("FAIL: eglCreateImageKHR(GL_TEXTURE_2D) err=0x%x\n", eglGetError()); return 1; }

   EGLint fourcc = 0, planes = 0;
   EGLuint64KHR mods[4] = { 0 };
   if (!p_query(dpy, img, &fourcc, &planes, mods)) {
      printf("FAIL: eglExportDMABUFImageQueryMESA err=0x%x\n", eglGetError());
      return 1;
   }
   printf("EXPORT: fourcc=0x%x (%.4s) planes=%d modifier=0x%llx\n",
          fourcc, (const char *)&fourcc, planes, (unsigned long long)mods[0]);
   if (fourcc != 0x34325241 /* AR24 */ && fourcc != 0x34324241 /* AB24 */ &&
       fourcc != 0x32315559 /* YU12 */ && fourcc != 0x3231564e /* NV12 */)
      printf("WARN: fourcc 不认识, Firefox 可能拒绝\n");

   int fds[4] = { -1, -1, -1, -1 };
   EGLint strides[4] = { 0 }, offsets[4] = { 0 };
   if (!p_export(dpy, img, fds, strides, offsets)) {
      printf("FAIL: eglExportDMABUFImageMESA err=0x%x\n", eglGetError());
      return 1;
   }
   printf("FDS: %d stride=%d offset=%d\n", fds[0], strides[0], offsets[0]);
   if (fds[0] < 0 || strides[0] <= 0) { printf("FAIL: 导出结果非法\n"); return 1; }

   /* 把 dmabuf 重新导入成纹理, 采样回来比对颜色 */
   EGLint ia[] = {
      EGL_WIDTH, W, EGL_HEIGHT, H,
      EGL_LINUX_DRM_FOURCC_EXT, fourcc,
      EGL_DMA_BUF_PLANE0_FD_EXT, fds[0],
      EGL_DMA_BUF_PLANE0_OFFSET_EXT, offsets[0],
      EGL_DMA_BUF_PLANE0_PITCH_EXT, strides[0],
      EGL_DMA_BUF_PLANE0_MODIFIER_LO_EXT, (EGLint)(mods[0] & 0xffffffffu),
      EGL_DMA_BUF_PLANE0_MODIFIER_HI_EXT, (EGLint)(mods[0] >> 32),
      EGL_NONE
   };
   EGLImageKHR dimg = p_create_image(dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, ia);
   if (dimg == EGL_NO_IMAGE_KHR) {
      printf("FAIL: 导入 dmabuf 失败 err=0x%x (Firefox 的 DMABUF_SURFACE_EXPORT 会用同一条路径)\n", eglGetError());
      return 1;
   }
   GLuint itex;
   glGenTextures(1, &itex);
   glBindTexture(GL_TEXTURE_2D, itex);
   p_target_tex(GL_TEXTURE_2D, (void *)dimg);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

   /* 把导入的纹理直接挂到 FBO 上再读回, 这是最直接的"文字/图像经 dmabuf 往返"验证 */
   glBindFramebuffer(GL_FRAMEBUFFER, fbo);
   glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, itex, 0);
   if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
      printf("FAIL: 导入纹理不能作为 FBO 附件\n");
      return 1;
   }
   glFinish();
   unsigned char px[4] = { 0 };
   glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
   printf("IMPORT: px=%d,%d,%d,%d 期望 64,128,191,255\n", px[0], px[1], px[2], px[3]);

   int ok = (abs(px[0] - 64) <= 1 && abs(px[1] - 128) <= 1 && abs(px[2] - 191) <= 1);
   printf("RESULT: %s\n", ok ? "PASS" : "FAIL");
   return ok ? 0 : 1;
}
