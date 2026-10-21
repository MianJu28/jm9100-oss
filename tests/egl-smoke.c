/* Full EGL/GBM smoke test against the NEW mwv207_dri.so, non-invasive (env-only) */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <gbm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

int main(void) {
    int fd = open("/dev/dri/renderD128", O_RDWR | O_CLOEXEC);
    if (fd < 0) { perror("open"); return 1; }
    struct gbm_device *gdev = gbm_create_device(fd);
    if (!gdev) { printf("gbm_create_device FAILED\n"); return 1; }
    printf("gbm backend: %s\n", gbm_device_get_backend_name(gdev));

    EGLDisplay dpy = eglGetPlatformDisplay(EGL_PLATFORM_GBM_MESA, gdev, NULL);
    if (dpy == EGL_NO_DISPLAY) { printf("eglGetPlatformDisplay FAILED\n"); return 1; }
    if (!eglInitialize(dpy, NULL, NULL)) { printf("eglInitialize FAILED err=0x%x\n", eglGetError()); return 1; }
    printf("EGL vendor: %s  version: %s\n", eglQueryString(dpy, EGL_VENDOR), eglQueryString(dpy, EGL_VERSION));

    struct gbm_bo *bo = gbm_bo_create(gdev, 256, 256, GBM_FORMAT_ARGB8888, GBM_BO_USE_RENDERING);
    if (!bo) { printf("gbm_bo_create FAILED\n"); return 1; }
    EGLImageKHR eimg = NULL;
    PFNEGLCREATEIMAGEKHRPROC createImage = (void*)eglGetProcAddress("eglCreateImageKHR");
    if (createImage) {
        eimg = createImage(dpy, EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR, (EGLClientBuffer)bo, NULL);
        printf("EGLImage from gbm bo: %s\n", eimg ? "OK" : "FAILED");
    }

    eglBindAPI(EGL_OPENGL_ES_API);
    static const EGLint cfgattr[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RED_SIZE,8, EGL_GREEN_SIZE,8, EGL_BLUE_SIZE,8, EGL_ALPHA_SIZE,8, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE };
    EGLConfig cfg; EGLint n;
    if (!eglChooseConfig(dpy, cfgattr, &cfg, 1, &n) || !n) { printf("eglChooseConfig FAILED\n"); return 1; }
    static const EGLint ctxattr[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ctxattr);
    if (ctx == EGL_NO_CONTEXT) { printf("eglCreateContext FAILED err=0x%x\n", eglGetError()); return 1; }

    /* real glamor-style path: gbm surface */
    struct gbm_surface *gs = gbm_surface_create(gdev, 256, 256, GBM_FORMAT_ARGB8888, GBM_BO_USE_RENDERING);
    if (!gs) { printf("gbm_surface_create FAILED\n"); return 1; }
    EGLSurface surf = eglCreateWindowSurface(dpy, cfg, (EGLNativeWindowType)gs, NULL);
    if (surf == EGL_NO_SURFACE) { printf("eglCreateWindowSurface FAILED err=0x%x\n", eglGetError()); return 1; }
    if (!eglMakeCurrent(dpy, surf, surf, ctx)) { printf("eglMakeCurrent FAILED err=0x%x\n", eglGetError()); return 1; }

    glClearColor(0.2f, 0.4f, 0.9f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glFinish();
    printf("GL renderer: %s\n", glGetString(GL_RENDERER));
    printf("GL version : %s\n", glGetString(GL_VERSION));
    printf("EGL full pipeline: OK (clear+finish on mwv207 hardware)\n");

    eglSwapBuffers(dpy, surf);
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(dpy, surf);
    gbm_surface_destroy(gs);
    eglDestroyContext(dpy, ctx);
    if (eimg) { PFNEGLDESTROYIMAGEKHRPROC d = (void*)eglGetProcAddress("eglDestroyImageKHR"); d(dpy, eimg); }
    gbm_bo_destroy(bo);
    gbm_device_destroy(gdev);
    eglTerminate(dpy);
    close(fd);
    return 0;
}
