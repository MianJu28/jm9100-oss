/* FBO-based render test: isolates render capability from EGL window surface */
#include <EGL/egl.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

int main(void) {
    /* surfaceless platform - no window system at all */
    EGLDisplay dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (!eglInitialize(dpy, NULL, NULL)) { printf("eglInitialize failed err=0x%x\n", eglGetError()); return 1; }
    eglBindAPI(EGL_OPENGL_ES_API);

    static const EGLint cfgattr[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RED_SIZE,8, EGL_GREEN_SIZE,8, EGL_BLUE_SIZE,8, EGL_ALPHA_SIZE,8, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE };
    EGLConfig cfg; EGLint n = 0;
    eglChooseConfig(dpy, cfgattr, &cfg, 1, &n);
    printf("surfaceless pbuffer configs: %d\n", n);
    if (!n) { printf("no pbuffer config on surfaceless\n"); return 1; }

    EGLContext ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, (EGLint[]){EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE});
    EGLSurface surf = eglCreatePbufferSurface(dpy, cfg, (EGLint[]){EGL_WIDTH, 64, EGL_HEIGHT, 64, EGL_NONE});
    if (!eglMakeCurrent(dpy, surf, surf, ctx)) { printf("makecurrent failed 0x%x\n", eglGetError()); return 1; }
    printf("renderer: %s\n", glGetString(GL_RENDERER));

    /* user FBO with our own texture render target */
    GLuint fbo, tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
    GLenum st = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    printf("FBO status: 0x%x %s\n", st, st == GL_FRAMEBUFFER_COMPLETE ? "COMPLETE" : "INCOMPLETE");
    if (st != GL_FRAMEBUFFER_COMPLETE) return 1;

    glViewport(0, 0, 64, 64);
    glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glFinish();
    unsigned char px[4];
    glReadPixels(32, 32, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
    printf("pixel after clear: R=%d G=%d B=%d A=%d\n", px[0], px[1], px[2], px[3]);
    printf("%s\n", (px[1] > 200) ? "FBO HARDWARE RENDER: PASS" : "FBO RENDER: FAIL");
    return !(px[1] > 200);
}
