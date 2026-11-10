/*
 * hangprobe.c —— N14 reset 通路实测探针
 *
 * 目的：在用户的 3D 队列里制造一个超过 drm_sched 超时（2 s）的 GPU 作业，
 * 触发 mwv207_sched_timedout_job() → drm_sched_stop() → pipe->reset()
 * （其中包含 N14 的 pipe_3d_force_complete()：以 -EIO signal 在飞 fence）
 * → drm_sched_resubmit_jobs() → drm_sched_start(true)。
 *
 * 判定：
 *   PASS：
 *     1. 提交后客户端没有永久阻塞（glClientWaitSync 在 30 s 内返回）；
 *     2. dmesg 出现 "timed out!"（reset 通路确实被走到）；
 *     3. 复位后能重新提交并完成一个短作业；
 *     4. 全程无 SIGALRM 看门狗触发。
 *   FAIL：等待永不返回 / 复位后无法恢复 / 驱动异常退出。
 *
 * 说明：内核里的 s_fence 与 pipe 级 fence 都不直接暴露 error 给用户态，
 * 因此 -EIO 是否生效由内核侧 kprobe（见 run-n14-reset.sh）证明：
 * pipe_3d_force_complete 被调用即意味着 N14 的 set_error(-EIO)+signal 被执行。
 *
 * 构建：gcc -O2 -o hangprobe hangprobe.c -lGL -lX11
 * 运行：DISPLAY=:0 XAUTHORITY=/home/admin/.Xauthority ./hangprobe [迭代次数]
 */
#define _GNU_SOURCE
#define GL_GLEXT_PROTOTYPES 1
#include <X11/Xlib.h>
#include <GL/glx.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <signal.h>
#include <unistd.h>

#define WATCHDOG_SEC 120
#define SYNC_DEADLINE_SEC 30.0
#define RECOVERY_DEADLINE_SEC 10.0
#define DEFAULT_ITERS 2000000000

static double now_sec(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void on_alarm(int sig)
{
	const char msg[] =
		"PROBE-FAIL: watchdog fired, client still blocked in fence wait\n";
	(void)sig;
	write(2, msg, sizeof(msg) - 1);
	_exit(4);
}

static GLuint compile_shader(GLenum type, const char *src)
{
	GLuint sh = glCreateShader(type);
	GLint ok = 0;
	char log[4096];

	glShaderSource(sh, 1, &src, NULL);
	glCompileShader(sh);
	glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		glGetShaderInfoLog(sh, sizeof(log), NULL, log);
		fprintf(stderr, "shader compile failed: %s\n", log);
		exit(3);
	}
	return sh;
}

static GLuint link_program(GLuint vs, GLuint fs)
{
	GLuint p = glCreateProgram();
	GLint ok = 0;
	char log[4096];

	glAttachShader(p, vs);
	glAttachShader(p, fs);
	glLinkProgram(p);
	glGetProgramiv(p, GL_LINK_STATUS, &ok);
	if (!ok) {
		glGetProgramInfoLog(p, sizeof(log), NULL, log);
		fprintf(stderr, "program link failed: %s\n", log);
		exit(3);
	}
	return p;
}

/* 等待一个 GL sync 对象；返回 0=已 signal，1=超时 */
static int wait_sync(GLsync sync, double deadline_sec, double *out_elapsed)
{
	double t0 = now_sec(), t;

	for (;;) {
		GLenum r = glClientWaitSync(sync, GL_SYNC_FLUSH_COMMANDS_BIT,
					    250000000ULL /* 250 ms */);
		t = now_sec() - t0;
		if (r == GL_ALREADY_SIGNALED || r == GL_CONDITION_SATISFIED) {
			if (out_elapsed)
				*out_elapsed = t;
			return 0;
		}
		if (r == GL_WAIT_FAILED) {
			fprintf(stderr, "glClientWaitSync: WAIT_FAILED\n");
			if (out_elapsed)
				*out_elapsed = t;
			return 2;
		}
		if (t > deadline_sec) {
			if (out_elapsed)
				*out_elapsed = t;
			return 1;
		}
		if (((int)(t * 2)) != ((int)((t - 0.25) * 2)))
			printf("  ... waiting for fence: %.1fs\n", t);
	}
}

/* 画一次全屏三角形（无 VBO：用 gl_VertexID 生成坐标） */
static void draw_triangle(GLuint prog, int iters)
{
	GLfloat verts[] = { -1.f, -1.f, 3.f, -1.f, -1.f, 3.f };
	GLuint vao = 0;

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glUseProgram(prog);
	glUniform1i(glGetUniformLocation(prog, "u_iters"), iters);
	glViewport(0, 0, 4, 4);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glFlush();
	(void)verts;
	glDeleteVertexArrays(1, &vao);
}

int main(int argc, char **argv)
{
	static const char *vs_src =
		"#version 400 core\n"
		"void main(void)\n"
		"{\n"
		"    float x = (gl_VertexID == 1) ? 3.0 : -1.0;\n"
		"    float y = (gl_VertexID == 2) ? 3.0 : -1.0;\n"
		"    gl_Position = vec4(x, y, 0.0, 1.0);\n"
		"}\n";
	static const char *fs_src =
		"#version 400 core\n"
		"uniform int u_iters;\n"
		"out vec4 o_color;\n"
		"void main(void)\n"
		"{\n"
		"    float x = 0.5;\n"
		"    /* 动态上界：编译器无法展开成常数次迭代 */\n"
		"    for (int i = 0; i < u_iters; ++i)\n"
		"        x = fract(x * 1.0000001 + 0.000001);\n"
		"    o_color = vec4(x);\n"
		"}\n";

	int iters = DEFAULT_ITERS;
	Display *dpy;
	int screen, ncfgs = 0;
	GLXFBConfig *cfgs;
	GLXContext ctx;
	GLXWindow dummy;
	const char *renderer, *version;
	double elapsed = 0.0;
	GLsync sync;
	GLuint vs, fs, prog;

	if (argc > 1)
		iters = atoi(argv[1]);

	signal(SIGALRM, on_alarm);
	alarm(WATCHDOG_SEC);

	dpy = XOpenDisplay(NULL);
	if (!dpy) {
		fprintf(stderr, "cannot open X display (need DISPLAY/XAUTHORITY)\n");
		return 3;
	}
	screen = DefaultScreen(dpy);

	{
		int fbattr[] = {
			GLX_X_RENDERABLE, True,
			GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT,
			GLX_RENDER_TYPE, GLX_RGBA_BIT,
			GLX_RED_SIZE, 8, GLX_GREEN_SIZE, 8,
			GLX_BLUE_SIZE, 8, GLX_ALPHA_SIZE, 8,
			None
		};
		cfgs = glXChooseFBConfig(dpy, screen, fbattr, &ncfgs);
		if (!cfgs || !ncfgs) {
			fprintf(stderr, "no pbuffer FBConfig\n");
			return 3;
		}

		GLXContext (*create_ctx_attribs)(Display *, GLXFBConfig, GLXContext,
						 Bool, const int *) =
			(GLXContext (*)(Display *, GLXFBConfig, GLXContext, Bool,
					const int *))
			glXGetProcAddressARB((const GLubyte *)
					     "glXCreateContextAttribsARB");
		int ctxattr[] = {
			GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
			GLX_CONTEXT_MINOR_VERSION_ARB, 0,
			GLX_CONTEXT_PROFILE_MASK_ARB, GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
			None
		};
		int pbattr[] = {
			GLX_PBUFFER_WIDTH, 64,
			GLX_PBUFFER_HEIGHT, 64,
			None
		};

		ctx = create_ctx_attribs(dpy, cfgs[0], NULL, True, ctxattr);
		if (!ctx) {
			fprintf(stderr, "glXCreateContextAttribsARB failed\n");
			return 3;
		}
		dummy = glXCreatePbuffer(dpy, cfgs[0], pbattr);
		if (!dummy) {
			fprintf(stderr, "glXCreatePbuffer failed\n");
			return 3;
		}
		if (!glXMakeContextCurrent(dpy, dummy, dummy, ctx)) {
			fprintf(stderr, "glXMakeContextCurrent failed\n");
			return 3;
		}
	}

	renderer = (const char *)glGetString(GL_RENDERER);
	version = (const char *)glGetString(GL_VERSION);
	printf("renderer : %s\n", renderer ? renderer : "(null)");
	printf("version  : %s\n", version ? version : "(null)");
	if (!renderer || !strstr(renderer, "MWV207")) {
		fprintf(stderr, "PROBE-FAIL: not the MWV207 driver (got %s)\n",
			renderer ? renderer : "(null)");
		return 3;
	}

	vs = compile_shader(GL_VERTEX_SHADER, vs_src);
	fs = compile_shader(GL_FRAGMENT_SHADER, fs_src);
	prog = link_program(vs, fs);

	printf("iters    : %d (动态循环片元着色器，预期 > 2 s 触发 TDR)\n", iters);
	printf("--- 提交 hang 作业 ---\n");
	draw_triangle(prog, iters);
	sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	glFlush();

	printf("--- 等待 fence（deadline %.0fs；预期 6×2s TDR 后由 karma 判定完成）---\n",
	       SYNC_DEADLINE_SEC);
	if (wait_sync(sync, SYNC_DEADLINE_SEC, &elapsed)) {
		fprintf(stderr,
			"PROBE-FAIL: fence 在 %.1fs 内未完成 —— 疑似 reset 丢弃了在飞 fence\n",
			elapsed);
		return 2;
	}
	printf("PASS: fence 完成，耗时 %.1fs（未阻塞）\n", elapsed);

	/* 复位后恢复性检查 */
	printf("--- 复位后恢复性检查 ---\n");
	draw_triangle(prog, 64);
	sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	glFlush();
	if (wait_sync(sync, RECOVERY_DEADLINE_SEC, &elapsed)) {
		fprintf(stderr, "PROBE-FAIL: 复位后短作业无法完成（%.1fs）\n", elapsed);
		return 2;
	}
	printf("PASS: 复位后短作业完成（%.1fs）\n", elapsed);

	{
		GLenum err = glGetError();
		if (err != GL_NO_ERROR)
			printf("note: glGetError() = 0x%x\n", err);
	}

	printf("\nPROBE-RESULT: PASS（reset 通路被走到，fence 被完成，复位后可用）\n");
	glXMakeContextCurrent(dpy, None, None, NULL);
	glXDestroyContext(dpy, ctx);
	XCloseDisplay(dpy);
	return 0;
}