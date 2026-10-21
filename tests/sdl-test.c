/* SDL2 GLES2 render test on mwv207 via EGL (headless: use dummy video driver + EGL override) */
#include <SDL.h>
#include <SDL_syswm.h>
#include <GLES2/gl2.h>
#include <stdio.h>
int main(int argc, char **argv) {
    SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
    SDL_Init(SDL_INIT_VIDEO);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_Window *w = SDL_CreateWindow("t", 0, 0, 256, 256,
        SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!w) { printf("window failed: %s\n", SDL_GetError()); return 1; }
    SDL_GLContext ctx = SDL_GL_CreateContext(w);
    if (!ctx) { printf("GL ctx failed: %s\n", SDL_GetError()); return 1; }
    glClearColor(0.4f, 0.6f, 0.8f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glFinish();
    printf("SDL2+GLES2 renderer: %s\n", glGetString(GL_RENDERER));
    printf("SDL2+GLES2 version : %s\n", glGetString(GL_VERSION));
    SDL_GL_DeleteContext(ctx);
    SDL_DestroyWindow(w);
    SDL_Quit();
    return 0;
}
