/* Full hardware rendering test: shader-compiled triangle + readback */
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <gbm.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

static const char *vs =
    "attribute vec4 pos; void main() { gl_Position = pos; }";
static const char *fs =
    "precision mediump float; void main() { gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0); }";

int main(void) {
    int fd = open("/dev/dri/renderD128", O_RDWR | O_CLOEXEC);
    struct gbm_device *gdev = gbm_create_device(fd);
    EGLDisplay dpy = eglGetPlatformDisplay(EGL_PLATFORM_GBM_KHR, gdev, NULL);
    if (!eglInitialize(dpy, NULL, NULL)) { printf("eglInitialize FAILED\n"); return 1; }
    eglBindAPI(EGL_OPENGL_ES_API);
    static const EGLint cfgattr[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RED_SIZE,8, EGL_GREEN_SIZE,8, EGL_BLUE_SIZE,8, EGL_ALPHA_SIZE,8, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE };
    EGLConfig cfg; EGLint n;
    if (!eglChooseConfig(dpy, cfgattr, &cfg, 1, &n) || !n) { printf("chooseconfig failed\n"); return 1; }
    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});
    struct gbm_surface *gs = gbm_surface_create(gdev, 64, 64, GBM_FORMAT_ARGB8888, GBM_BO_USE_RENDERING);
    EGLSurface surf = eglCreateWindowSurface(dpy, cfg, (EGLNativeWindowType)gs, NULL);
    eglMakeCurrent(dpy, surf, surf, ctx);

    /* compile a real shader: exercises the NIR backend end-to-end */
    GLint v = glCreateShader(GL_VERTEX_SHADER); glShaderSource(v, 1, &vs, NULL); glCompileShader(v);
    GLint ok = 0; glGetShaderiv(v, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[512]; glGetShaderInfoLog(v, 512, NULL, log); printf("VS compile FAILED: %s\n", log); return 1; }
    GLint f = glCreateShader(GL_FRAGMENT_SHADER); glShaderSource(f, 1, &fs, NULL); glCompileShader(f);
    glGetShaderiv(f, GL_COMPILE_STATUS, &ok);
    if (!ok) { char log[512]; glGetShaderInfoLog(f, 512, NULL, log); printf("FS compile FAILED: %s\n", log); return 1; }
    GLint p = glCreateProgram(); glAttachShader(p, v); glAttachShader(p, f); glLinkProgram(p);
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) { char log[512]; glGetProgramInfoLog(p, 512, NULL, log); printf("link FAILED: %s\n", log); return 1; }
    glUseProgram(p);

    GLfloat verts[] = { 0.0f, 0.9f, 0.0f, -0.9f, -0.9f, 0.0f, 0.9f, -0.9f, 0.0f };
    glVertexAttribPointer(0, 3, GL_FLOAT, 0, 0, verts);
    glEnableVertexAttribArray(0);

    glViewport(0, 0, 64, 64);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glFinish();

    /* read back center pixel: must be red (triangle interior) */
    unsigned char px[4] = {0};
    glReadPixels(32, 20, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    printf("center pixel: R=%d G=%d B=%d A=%d\n", px[0], px[1], px[2], px[3]);
    if (px[0] > 200 && px[1] < 50 && px[2] < 50)
        printf("HARDWARE TRIANGLE RENDER: PASS (shader compile + draw + readback on MWV207)\n");
    else
        printf("TRIANGLE: UNEXPECTED COLOR\n");

    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface(dpy, surf); gbm_surface_destroy(gs);
    eglDestroyContext(dpy, ctx);
    gbm_device_destroy(gdev); eglTerminate(dpy); close(fd);
    return (px[0] > 200) ? 0 : 1;
}
