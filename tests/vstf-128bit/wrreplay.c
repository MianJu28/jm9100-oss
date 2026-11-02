/* wrreplay.c —— WebRender 真实着色器双驱动对照探针
 *
 * 用我们的 mwv207 驱动 与 参考 llvmpipe，在**同一二进制、同一输入**下编译链接
 * Mozilla WebRender 的 brush_solid(ALPHA_PASS) VS/FS 并进行一次
 * glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, 1)，
 * 然后把 64x64 RGBA8 FBO 的内容逐字节比较。
 *
 * 用法:
 *   ./wrreplay [vsfile] [fsfile] [tag]
 * 环境变量:
 *   WRR_MODE=formula  按任务规定的公式化确定性图案填充
 *   WRR_MODE=tuned    用一组精心设计的确定性输入，保证几何覆盖全屏（默认 tuned）
 *   WRR_DUMP=1        逐行打印 64x64
 *
 * 编译: cc -O1 -o /tmp/wrreplay /tmp/wrreplay.c -lEGL -lGLESv2
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#ifndef GL_SAMPLER_EXTERNAL_OES
#define GL_SAMPLER_EXTERNAL_OES 0x8D65
#endif

#define FBW 64
#define FBH 64
#define TEXW 1024
#define TEXH 64

static int g_tuned = 1;

/* ------------------------------------------------------------------ */
static char *slurp(const char *p) {
    FILE *f = fopen(p, "rb");
    if (!f) { fprintf(stderr, "cannot open %s\n", p); exit(1); }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *b = (char *)malloc(n + 1);
    if (!b || fread(b, 1, n, f) != (size_t)n) { fprintf(stderr, "read fail %s\n", p); exit(1); }
    b[n] = 0; fclose(f); return b;
}

static const char *tname(GLenum t) {
    switch (t) {
    case GL_FLOAT: return "GL_FLOAT";
    case GL_FLOAT_VEC2: return "GL_FLOAT_VEC2";
    case GL_FLOAT_VEC3: return "GL_FLOAT_VEC3";
    case GL_FLOAT_VEC4: return "GL_FLOAT_VEC4";
    case GL_INT: return "GL_INT";
    case GL_INT_VEC2: return "GL_INT_VEC2";
    case GL_INT_VEC3: return "GL_INT_VEC3";
    case GL_INT_VEC4: return "GL_INT_VEC4";
    case GL_UNSIGNED_INT: return "GL_UNSIGNED_INT";
    case GL_UNSIGNED_INT_VEC2: return "GL_UNSIGNED_INT_VEC2";
    case GL_UNSIGNED_INT_VEC3: return "GL_UNSIGNED_INT_VEC3";
    case GL_UNSIGNED_INT_VEC4: return "GL_UNSIGNED_INT_VEC4";
    case GL_BOOL: return "GL_BOOL";
    case GL_BOOL_VEC2: return "GL_BOOL_VEC2";
    case GL_BOOL_VEC3: return "GL_BOOL_VEC3";
    case GL_BOOL_VEC4: return "GL_BOOL_VEC4";
    case GL_FLOAT_MAT2: return "GL_FLOAT_MAT2";
    case GL_FLOAT_MAT3: return "GL_FLOAT_MAT3";
    case GL_FLOAT_MAT4: return "GL_FLOAT_MAT4";
    case GL_SAMPLER_2D: return "GL_SAMPLER_2D";
    case GL_SAMPLER_3D: return "GL_SAMPLER_3D";
    case GL_SAMPLER_CUBE: return "GL_SAMPLER_CUBE";
    case GL_SAMPLER_2D_SHADOW: return "GL_SAMPLER_2D_SHADOW";
    case GL_SAMPLER_2D_ARRAY: return "GL_SAMPLER_2D_ARRAY";
    case GL_INT_SAMPLER_2D: return "GL_INT_SAMPLER_2D";
    case GL_INT_SAMPLER_3D: return "GL_INT_SAMPLER_3D";
    case GL_UNSIGNED_INT_SAMPLER_2D: return "GL_UNSIGNED_INT_SAMPLER_2D";
    case GL_UNSIGNED_INT_SAMPLER_3D: return "GL_UNSIGNED_INT_SAMPLER_3D";
    case GL_SAMPLER_EXTERNAL_OES: return "GL_SAMPLER_EXTERNAL_OES";
    default: return "OTHER";
    }
}
static int is_sampler(GLenum t) {
    return t == GL_SAMPLER_2D || t == GL_SAMPLER_3D || t == GL_SAMPLER_CUBE ||
           t == GL_SAMPLER_2D_SHADOW || t == GL_SAMPLER_2D_ARRAY ||
           t == GL_INT_SAMPLER_2D || t == GL_INT_SAMPLER_3D ||
           t == GL_UNSIGNED_INT_SAMPLER_2D || t == GL_UNSIGNED_INT_SAMPLER_3D ||
           t == GL_SAMPLER_EXTERNAL_OES;
}

/* ------------------------------------------------------------------ */
/* 纹理数据填充：formula = 任务规定的公式化图案；tuned = 设计值（覆盖 formula 后打补丁） */
static void fill_float_tex(float *d) {
    for (int y = 0; y < TEXH; y++)
        for (int x = 0; x < TEXW; x++) {
            int i = y * TEXW + x, o = i * 4;
            d[o + 0] = (float)((i % 64)) * 0.01f + 0.1f;
            d[o + 1] = (float)(((i + 21) % 64)) * 0.01f + 0.1f;
            d[o + 2] = (float)(((i + 43) % 64)) * 0.01f + 0.1f;
            d[o + 3] = 1.0f;
        }
}
static void fill_int_tex(int *d) {
    for (int y = 0; y < TEXH; y++)
        for (int x = 0; x < TEXW; x++) {
            int i = y * TEXW + x, o = i * 4;
            d[o + 0] = i % 7 - 3;
            d[o + 1] = (i + 2) % 7 - 3;
            d[o + 2] = (i + 4) % 7 - 3;
            d[o + 3] = (i + 6) % 7 - 3;
        }
}
static void put_f(float *d, int x, int y, float r, float g, float b, float a) {
    int o = (y * TEXW + x) * 4;
    d[o] = r; d[o + 1] = g; d[o + 2] = b; d[o + 3] = a;
}
static void put_i(int *d, int x, int y, int r, int g, int b, int a) {
    int o = (y * TEXW + x) * 4;
    d[o] = r; d[o + 1] = g; d[o + 2] = b; d[o + 3] = a;
}

/* tuned 补丁: 让真实 VS 的间接寻址链落到一组良定义的输入上 */
static void patch_tuned_float(const char *name, float *d, int is_r8) {
    if (is_r8) return;
    if (!strcmp(name, "sGpuCache")) {
        put_f(d, 0, 0, 0.3f, 0.6f, 0.9f, 1.0f);              /* solid brush color */
    } else if (!strcmp(name, "sTransformPalette")) {
        /* transform_id=0 -> index 0, VECS_PER_TRANSFORM=8 -> uv=(0,0) */
        put_f(d, 0, 0, 1, 0, 0, 0); put_f(d, 1, 0, 0, 1, 0, 0);
        put_f(d, 2, 0, 0, 0, 1, 0); put_f(d, 3, 0, 0, 0, 0, 1);
        put_f(d, 4, 0, 1, 0, 0, 0); put_f(d, 5, 0, 0, 1, 0, 0);
        put_f(d, 6, 0, 0, 0, 1, 0); put_f(d, 7, 0, 0, 0, 0, 1);
    } else if (!strcmp(name, "sPrimitiveHeadersF")) {
        /* prim_header_address=1, VECS_PER_PRIM_HEADER_F=2 -> uv=(2,0) */
        put_f(d, 2, 0, -1.0f, -1.0f, 1.0f, 1.0f);            /* local_rect */
        put_f(d, 3, 0, -1.5f, -1.5f, 1.5f, 1.5f);            /* local_clip_rect */
    } else if (!strcmp(name, "sRenderTasks")) {
        /* pic_task_address=0 -> uv=(0,0); clip_address=2 -> uv=(4,0) */
        put_f(d, 0, 0, 0.0f, 0.0f, 64.0f, 64.0f);            /* pic task_rect: p0=(0,0) p1=(64,64) */
        put_f(d, 1, 0, 1.0f, 0.0f, 0.0f, 0.0f);              /* device_pixel_scale=1 content_origin=(0,0) */
        put_f(d, 4, 0, -2.0f, -2.0f, 64.0f, 64.0f);          /* clip task_rect (放宽 clip 边界) */
        put_f(d, 5, 0, 1.0f, 0.0f, 0.0f, 0.0f);              /* clip device_pixel_scale=1 screen_origin=(0,0) */
    }
}
static void patch_tuned_int(const char *name, int *d) {
    if (!strcmp(name, "sPrimitiveHeadersI")) {
        /* prim_header_address=1, VECS_PER_PRIM_HEADER_I=2 -> uv=(2,0) */
        put_i(d, 2, 0, 0, 0, 0, 0);        /* z=0 specific=0 transform_id=0 pic_task=0 */
        put_i(d, 3, 0, 65535, 0, 0, 0);    /* user_data.x=65535 -> opacity=1 */
    }
}

/* ------------------------------------------------------------------ */
static GLuint mktex_float(const char *name) {
    GLuint t; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    float *d = (float *)malloc((size_t)TEXW * TEXH * 4 * sizeof(float));
    fill_float_tex(d);
    if (g_tuned) patch_tuned_float(name, d, 0);
    printf("TEXF %-24s texel2=(%g,%g,%g,%g) texel3=(%g,%g,%g,%g)\n", name,
           d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15]);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32F, TEXW, TEXH);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, TEXW, TEXH, GL_RGBA, GL_FLOAT, d);
    free(d);
    return t;
}
static GLuint mktex_int(const char *name) {
    GLuint t; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    int *d = (int *)malloc((size_t)TEXW * TEXH * 4 * sizeof(int));
    fill_int_tex(d);
    if (g_tuned) patch_tuned_int(name, d);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA32I, TEXW, TEXH);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, TEXW, TEXH, GL_RGBA_INTEGER, GL_INT, d);
    free(d);
    return t;
}
static GLuint mktex_r8(const char *name) {
    (void)name;
    GLuint t; glGenTextures(1, &t); glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    unsigned char *d = (unsigned char *)malloc((size_t)TEXW * TEXH);
    memset(d, 200, (size_t)TEXW * TEXH);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, TEXW, TEXH);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, TEXW, TEXH, GL_RED, GL_UNSIGNED_BYTE, d);
    free(d);
    return t;
}

/* ------------------------------------------------------------------ */
static void set_non_sampler_uniform(GLint loc, GLenum type, GLint size) {
    int i, n = size > 16 ? 16 : size;
    /* 确定性非零值（任务规定） */
    switch (type) {
    case GL_FLOAT:            for (i = 0; i < n; i++) glUniform1f(loc + i, 0.5f); break;
    case GL_FLOAT_VEC2:       for (i = 0; i < n; i++) glUniform2f(loc + i, 0.25f, 0.5f); break;
    case GL_FLOAT_VEC3:       for (i = 0; i < n; i++) glUniform3f(loc + i, 0.25f, 0.5f, 0.75f); break;
    case GL_FLOAT_VEC4:       for (i = 0; i < n; i++) glUniform4f(loc + i, 0.25f, 0.5f, 0.75f, 1.0f); break;
    case GL_INT: case GL_BOOL: for (i = 0; i < n; i++) glUniform1i(loc + i, 3); break;
    case GL_INT_VEC2: case GL_BOOL_VEC2: for (i = 0; i < n; i++) glUniform2i(loc + i, 3, 3); break;
    case GL_INT_VEC3: case GL_BOOL_VEC3: for (i = 0; i < n; i++) glUniform3i(loc + i, 3, 3, 3); break;
    case GL_INT_VEC4: case GL_BOOL_VEC4: for (i = 0; i < n; i++) glUniform4i(loc + i, 3, 3, 3, 3); break;
    case GL_UNSIGNED_INT:     for (i = 0; i < n; i++) glUniform1ui(loc + i, 5u); break;
    case GL_UNSIGNED_INT_VEC2:for (i = 0; i < n; i++) glUniform2ui(loc + i, 5u, 5u); break;
    case GL_UNSIGNED_INT_VEC3:for (i = 0; i < n; i++) glUniform3ui(loc + i, 5u, 5u, 5u); break;
    case GL_UNSIGNED_INT_VEC4:for (i = 0; i < n; i++) glUniform4ui(loc + i, 5u, 5u, 5u, 5u); break;
    case GL_FLOAT_MAT2: { float m[4] = {1,0,0,1}; for (i=0;i<n;i++) glUniformMatrix2fv(loc+i,1,GL_FALSE,m); } break;
    case GL_FLOAT_MAT3: { float m[9] = {1,0,0, 0,1,0, 0,0,1}; for (i=0;i<n;i++) glUniformMatrix3fv(loc+i,1,GL_FALSE,m); } break;
    case GL_FLOAT_MAT4: { float m[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1}; for (i=0;i<n;i++) glUniformMatrix4fv(loc+i,1,GL_FALSE,m); } break;
    default: printf("    (未处理的非 sampler 类型 %s，跳过)\n", tname(type)); break;
    }
}

/* ------------------------------------------------------------------ */
int main(int argc, char **argv) {
    const char *vsfile = argc > 1 ? argv[1] : "/tmp/wrshade/brush_solid.vs.glsl";
    const char *fsfile = argc > 2 ? argv[2] : "/tmp/wrshade/brush_solid.fs.glsl";
    const char *tag    = argc > 3 ? argv[3] : "brush_solid";
    const char *mode   = getenv("WRR_MODE");
    if (mode && !strcmp(mode, "formula")) g_tuned = 0;
    int dump = getenv("WRR_DUMP") && !strcmp(getenv("WRR_DUMP"), "1");

    printf("=== WRR tag=%s mode=%s vs=%s fs=%s ===\n", tag, g_tuned ? "tuned" : "formula", vsfile, fsfile);

    /* ---- EGL / ES3.0 pbuffer ---- */
    EGLDisplay dpy; EGLConfig cfg; EGLContext ctx; EGLSurface surf;
    EGLint ma[] = { EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
                    EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE };
    EGLint ca[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    EGLint pb[] = { EGL_WIDTH, FBW, EGL_HEIGHT, FBH, EGL_NONE };
    eglBindAPI(EGL_OPENGL_ES_API);
    dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (dpy == EGL_NO_DISPLAY || !eglInitialize(dpy, NULL, NULL)) { printf("EGL-INIT-FAIL 0x%x\n", eglGetError()); return 1; }
    EGLint ncfg = 0;
    if (!eglChooseConfig(dpy, ma, &cfg, 1, &ncfg) || ncfg < 1) {
        if (!eglChooseConfig(dpy, NULL, &cfg, 1, &ncfg) || ncfg < 1) { printf("EGL-CFG-FAIL 0x%x\n", eglGetError()); return 1; }
    }
    ctx = eglCreateContext(dpy, cfg, EGL_NO_CONTEXT, ca);
    if (ctx == EGL_NO_CONTEXT) { printf("EGL-CTX-FAIL 0x%x\n", eglGetError()); return 1; }
    surf = eglCreatePbufferSurface(dpy, cfg, pb);
    if (!eglMakeCurrent(dpy, surf, surf, ctx)) { printf("EGL-MAKE-FAIL 0x%x\n", eglGetError()); return 1; }

    printf("GL_VENDOR=%s\n", glGetString(GL_VENDOR));
    printf("GL_RENDERER=%s\n", glGetString(GL_RENDERER));
    printf("GL_VERSION=%s\n", glGetString(GL_VERSION));
    printf("GL_SHADING_LANGUAGE_VERSION=%s\n", glGetString(GL_SHADING_LANGUAGE_VERSION));

    /* ---- 编译 ---- */
    char *vsrc = slurp(vsfile), *fsrc = slurp(fsfile);
    GLuint v = glCreateShader(GL_VERTEX_SHADER), f = glCreateShader(GL_FRAGMENT_SHADER);
    const char *sp;
    GLint ok = 0; char *log;
    sp = vsrc; glShaderSource(v, 1, &sp, NULL); glCompileShader(v);
    glGetShaderiv(v, GL_COMPILE_STATUS, &ok);
    { GLint L = 0; glGetShaderiv(v, GL_INFO_LOG_LENGTH, &L); log = (char *)malloc(L + 2); if (L > 0) glGetShaderInfoLog(v, L, NULL, log); else log[0] = 0; log[L] = 0; }
    printf("VS_COMPILE_STATUS=%d\nVS_INFO_LOG<<<\n%s\n>>>\n", ok, log); free(log);
    if (!ok) { printf("VERDICT=VS-COMPILE-FAIL\n"); return 2; }

    sp = fsrc; glShaderSource(f, 1, &sp, NULL); glCompileShader(f);
    glGetShaderiv(f, GL_COMPILE_STATUS, &ok);
    { GLint L = 0; glGetShaderiv(f, GL_INFO_LOG_LENGTH, &L); log = (char *)malloc(L + 2); if (L > 0) glGetShaderInfoLog(f, L, NULL, log); else log[0] = 0; log[L] = 0; }
    printf("FS_COMPILE_STATUS=%d\nFS_INFO_LOG<<<\n%s\n>>>\n", ok, log); free(log);
    if (!ok) { printf("VERDICT=FS-COMPILE-FAIL\n"); return 2; }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, v); glAttachShader(prog, f);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    { GLint L = 0; glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &L); log = (char *)malloc(L + 2); if (L > 0) glGetProgramInfoLog(prog, L, NULL, log); else log[0] = 0; log[L] = 0; }
    printf("LINK_STATUS=%d\nLINK_INFO_LOG<<<\n%s\n>>>\n", ok, log); free(log);
    if (!ok) { printf("VERDICT=LINK-FAIL\n"); return 2; }
    glUseProgram(prog);   /* glUniform* 要求 program 为 current */

    /* ---- active uniforms ---- */
    GLint nu = 0; glGetProgramiv(prog, GL_ACTIVE_UNIFORMS, &nu);
    printf("ACTIVE_UNIFORMS=%d\n", nu);
    GLuint unit = 0;
    for (GLint i = 0; i < nu; i++) {
        char nm[256]; GLint sz = 0; GLenum ty = 0; GLsizei len = 0;
        glGetActiveUniform(prog, i, sizeof nm, &len, &sz, &ty, nm);
        GLint loc = glGetUniformLocation(prog, nm);
        printf("  U[%d] %-28s type=%-26s size=%d loc=%d\n", i, nm, tname(ty), sz, loc);
        if (is_sampler(ty)) {
            /* 关键：先切到目标单元，mktex_* 内部会用当前单元绑定新纹理；
               否则新纹理会覆盖上一个 sampler 的单元绑定，导致多个 sampler 读同一张纹理。 */
            glActiveTexture(GL_TEXTURE0 + unit);
            GLuint tex;
            int r8 = !strcmp(nm, "sClipMask");
            if (ty == GL_UNSIGNED_INT_SAMPLER_2D || ty == GL_INT_SAMPLER_2D) tex = mktex_int(nm);
            else if (r8) tex = mktex_r8(nm);
            else tex = mktex_float(nm);
            GLint n = sz > 1 ? sz : 1;
            for (GLint e = 0; e < n && e < 16; e++) {
                glActiveTexture(GL_TEXTURE0 + unit);
                glBindTexture(GL_TEXTURE_2D, tex);
                glUniform1i(loc + e, (GLint)unit);
                { GLint uu = -1; glGetUniformiv(prog, loc + e, &uu);
                  printf("    BIND %-24s loc=%d unit=%u tex=%u readback=%d\n", nm, loc + e, unit, tex, uu); }
                unit++;
            }
        } else if (loc >= 0) {
            set_non_sampler_uniform(loc, ty, sz);
        }
    }

    /* ---- active attributes (VS) ---- */
    GLint na = 0; glGetProgramiv(prog, GL_ACTIVE_ATTRIBUTES, &na);
    printf("ACTIVE_ATTRIBUTES=%d\n", na);
    GLuint vao; glGenVertexArrays(1, &vao); glBindVertexArray(vao);
    GLuint *vbos = (GLuint *)calloc(na ? na : 1, sizeof(GLuint));
    for (GLint i = 0; i < na; i++) {
        char nm[256]; GLint sz = 0; GLenum ty = 0; GLsizei len = 0;
        glGetActiveAttrib(prog, i, sizeof nm, &len, &sz, &ty, nm);
        GLint loc = glGetAttribLocation(prog, nm);
        int comps = 1;
        switch (ty) {
        case GL_FLOAT_VEC2: case GL_INT_VEC2: case GL_UNSIGNED_INT_VEC2: comps = 2; break;
        case GL_FLOAT_VEC3: case GL_INT_VEC3: case GL_UNSIGNED_INT_VEC3: comps = 3; break;
        case GL_FLOAT_VEC4: case GL_INT_VEC4: case GL_UNSIGNED_INT_VEC4: comps = 4; break;
        default: comps = 1; break;
        }
        int isint = (ty == GL_INT || ty == GL_INT_VEC2 || ty == GL_INT_VEC3 || ty == GL_INT_VEC4 ||
                     ty == GL_UNSIGNED_INT || ty == GL_UNSIGNED_INT_VEC2 ||
                     ty == GL_UNSIGNED_INT_VEC3 || ty == GL_UNSIGNED_INT_VEC4);
        printf("  A[%d] %-28s type=%-18s size=%d loc=%d comps=%d int=%d\n", i, nm, tname(ty), sz, loc, comps, isint);
        glGenBuffers(1, &vbos[i]);
        glBindBuffer(GL_ARRAY_BUFFER, vbos[i]);
        if (isint) {
            GLint data[4 * 4];
            for (int vtx = 0; vtx < 4; vtx++)
                for (int c = 0; c < 4; c++) {
                    int val;
                    if (g_tuned && !strcmp(nm, "aData")) {
                        /* prim_header_address=1, clip_address=2, z=flags<<16|segment_index=0xffff(INVALID), w=0 */
                        static const int tuned_aData[4][4] = {{1,2,65535,0},{1,2,65535,0},{1,2,65535,0},{1,2,65535,0}};
                        val = tuned_aData[vtx][c];
                    } else {
                        val = (vtx * 4 + c) % 7 - 3;   /* 公式化确定性图案 */
                    }
                    data[vtx * comps + c] = val;
                }
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(4 * comps * sizeof(GLint)), data, GL_STATIC_DRAW);
            glVertexAttribIPointer((GLuint)loc, comps, GL_INT, comps * 4, (void *)0);
        } else {
            float data[4 * 4];
            for (int vtx = 0; vtx < 4; vtx++)
                for (int c = 0; c < 4; c++) {
                    float val;
                    if (comps == 2) val = (float)((vtx >> c) & 1);         /* 单位方 (0,0)(1,0)(0,1)(1,1) */
                    else val = 0.125f * (float)(vtx + 1) + 0.0625f * (float)c; /* 公式化确定性图案 */
                    data[vtx * comps + c] = val;
                }
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(4 * comps * sizeof(float)), data, GL_STATIC_DRAW);
            glVertexAttribPointer((GLuint)loc, comps, GL_FLOAT, GL_FALSE, comps * 4, (void *)0);
        }
        glEnableVertexAttribArray((GLuint)loc);
        /* WebRender 里唯一 per-instance 的属性是 PER_INSTANCE in ivec4 aData（divisor 1）；
         * aPosition 是 attribute vec2，每顶点一份（divisor 0）。
         * 任务书要求"全部 divisor 1"，那会把这 4 个顶点全折叠到 element 0
         * → 退化四边形 → 零覆盖。用 WRR_DIV_ALL=1 可复现该字面设置。 */
        int divall = getenv("WRR_DIV_ALL") && !strcmp(getenv("WRR_DIV_ALL"), "1");
        int dv = (divall || !strcmp(nm, "aData")) ? 1 : 0;
        printf("      -> divisor=%d\n", dv);
        glVertexAttribDivisor((GLuint)loc, (GLuint)dv);
    }

    /* ---- FBO 64x64 RGBA8 ---- */
    /* 用最高纹理单元绑定 FBO 颜色纹理，避免覆盖上面已绑好的 sampler 单元 */
    GLint maxunits = 0; glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &maxunits);
    GLuint fbounit = (GLuint)(maxunits > 1 ? maxunits - 1 : 0);
    printf("MAX_COMBINED_TEX_UNITS=%d fbo_tex_unit=%u\n", maxunits, fbounit);
    GLuint ct; glGenTextures(1, &ct);
    glActiveTexture(GL_TEXTURE0 + fbounit);
    glBindTexture(GL_TEXTURE_2D, ct);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, FBW, FBH);
    glActiveTexture(GL_TEXTURE0);
    GLuint fbo; glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ct, 0);
    printf("FBO_STATUS=0x%x (complete=0x%x)\n", glCheckFramebufferStatus(GL_FRAMEBUFFER), GL_FRAMEBUFFER_COMPLETE);

    /* ---- 绘制 ---- */
    glViewport(0, 0, FBW, FBH);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_DITHER);           /* 固定功能抖动会掩盖着色器差异 */
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(prog);
    { GLint cp=0, vb=0, ab=0, en=0, sz=0, dv=0, ty=0, st=0, off=0;
      glGetIntegerv(GL_CURRENT_PROGRAM, &cp);
      glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vb);
      glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &ab);
      glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_ENABLED, &en);
      glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_SIZE, &sz);
      glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_DIVISOR, &dv);
      glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_TYPE, &ty);
      glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_STRIDE, &st);
      glGetVertexAttribiv(0, GL_VERTEX_ATTRIB_ARRAY_BUFFER_BINDING, &off);
      printf("DBG cur_prog=%d is_prog=%d vao=%u arraybuf=%u attr0(en=%d size=%d div=%d type=0x%x stride=%d buf=%d)\n",
             cp, glIsProgram(prog), vb, ab, en, sz, dv, ty, st, off);
      float vv[8]; float *mp = (float *)glMapBufferRange(GL_ARRAY_BUFFER, 0, sizeof vv, GL_MAP_READ_BIT);
      if (mp) { memcpy(vv, mp, sizeof vv); glUnmapBuffer(GL_ARRAY_BUFFER); }
      else memset(vv, 0, sizeof vv);
      printf("DBG attr0 data = (%g,%g) (%g,%g) (%g,%g) (%g,%g)\n", vv[0],vv[1],vv[2],vv[3],vv[4],vv[5],vv[6],vv[7]);
    }
    if (getenv("WRR_NONINST")) glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    else glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, 1);
    glFinish();
    GLenum err = glGetError();
    printf("GL_ERROR_AFTER_DRAW=0x%x\n", err);

    unsigned char buf[FBW * FBH * 4];
    glReadPixels(0, 0, FBW, FBH, GL_RGBA, GL_UNSIGNED_BYTE, buf);

    unsigned long long h = 1469598103934665603ULL;
    for (size_t i = 0; i < sizeof buf; i++) { h ^= buf[i]; h *= 1099511628211ULL; }
    printf("FB_HASH_FNV1A64=0x%016llx\n", h);

    int ncov = 0;
    for (int i = 0; i < FBW * FBH; i++)
        if (buf[i*4] || buf[i*4+1] || buf[i*4+2] || buf[i*4+3] != 255) ncov++;
    printf("COVERAGE_PIXELS=%d/%d\n", ncov, FBW * FBH);

    int pts[][2] = {{8,8},{32,8},{56,8},{8,32},{32,32},{56,32},{8,56},{32,56},{56,56}};
    for (size_t i = 0; i < sizeof(pts)/sizeof(pts[0]); i++) {
        int x = pts[i][0], y = pts[i][1];
        unsigned char *p = &buf[(y * FBW + x) * 4];
        printf("  PT(%2d,%2d)=(%3u,%3u,%3u,%3u)\n", x, y, p[0], p[1], p[2], p[3]);
    }
    if (dump) {
        for (int y = 0; y < FBH; y++) {
            printf("  ROW%02d ", y);
            for (int x = 0; x < FBW; x++) printf("%02x%02x%02x%02x", buf[(y*FBW+x)*4], buf[(y*FBW+x)*4+1], buf[(y*FBW+x)*4+2], buf[(y*FBW+x)*4+3]);
            printf("\n");
        }
    }
    printf("VERDICT=OK\n");
    eglMakeCurrent(dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    return 0;
}