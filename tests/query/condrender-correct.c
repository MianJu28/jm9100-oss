/* 条件渲染（GL_NV_conditional_render）正确性测试 —— 桌面 GL / GLX
 *
 * condrender.c 只验证“不再 SIGSEGV”，这里验证语义对不对：
 *   A. 谓词成立（遮挡查询 >0） → 受条件约束的绘制必须真的画上去
 *   B. 谓词不成立（查询 ==0）  → 绘制必须被丢弃（像素保持原值）
 *   C. 反相谓词（*_INVERTED）  → 查询 ==0 时反而要画
 *
 * 构建: cc -o /tmp/condrender-correct condrender-correct.c -lGL -lX11
 * 运行: DISPLAY=:0 /tmp/condrender-correct
 */
#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#define GL_GLEXT_PROTOTYPES
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/glx.h>

typedef void (*PFNBCR)(GLuint, GLenum);
typedef void (*PFNECR)(void);

static Display *dpy;
static PFNBCR pBegin;
static PFNECR pEnd;
static int fails;

#define W 64
#define H 64

static void
draw_quad(float x0, float y0, float x1, float y1, int green)
{
   /* green=0 用红色（遮挡物），green=1 用绿色（受条件约束的绘制）——
    * 两者颜色必须不同，否则“画了”和“没画”区分不出来。 */
   glColor4f(green ? 0.0f : 1.0f, green ? 1.0f : 0.0f, 0.0f, 1.0f);
   glBegin(GL_TRIANGLE_STRIP);
   glVertex2f(x0, y0);
   glVertex2f(x1, y0);
   glVertex2f(x0, y1);
   glVertex2f(x1, y1);
   glEnd();
}

/* 跑一次遮挡查询：inside=1 画在视口内（应 >0），inside=0 画到视口外（应为 0） */
static GLuint
make_query(int inside)
{
   GLuint q;
   glGenQueries(1, &q);
   glBeginQuery(GL_SAMPLES_PASSED, q);
   draw_quad(inside ? -1.f : 3.f, inside ? -1.f : 3.f, 1.f, 1.f, 0);  /* 遮挡物=红 */
   glEndQuery(GL_SAMPLES_PASSED);
   return q;
}

/* 读中心像素的**绿**通道：遮挡物画红(R=255,G=0)，受条件约束的绘制画绿，
 * 故绿通道非 0 就说明"受条件约束的那次绘制确实发生了"。 */
static unsigned char
read_green(void)
{
   unsigned char px[4];
   memset(px, 0, sizeof px);
   glReadPixels(W / 2, H / 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px);
   return px[1];
}

static void
run_case(const char *name, int query_inside, GLenum mode, int expect_drawn)
{
   GLuint q;
   unsigned char got;
   GLenum err;

   glClearColor(0, 0, 0, 1);
   glClear(GL_COLOR_BUFFER_BIT);

   q = make_query(query_inside);
   glFlush();

   got = read_green();                 /* 此时绿通道应为 0（只有遮挡物） */
   if (got != 0) {
      printf("  [FAIL] %s: 前置状态不对（绿应为 0，实测 %u）\n", name, got);
      fails++;
      return;
   }

   pBegin(q, mode);
   draw_quad(-1.f, -1.f, 1.f, 1.f, 1);   /* 受条件约束的绘制=绿 */
   pEnd();
   glFlush();

   got = read_green();      /* 画上去 = 255 */

   err = glGetError();
   if (err != GL_NO_ERROR) {
      printf("  [FAIL] %s: glGetError=0x%x\n", name, err);
      fails++;
   } else if ((got != 0) == expect_drawn) {
      printf("  [PASS] %-38s 画上去=%s (G=%u)\n",
             name, got ? "是" : "否", got);
   } else {
      printf("  [FAIL] %-38s 期望画上去=%s, 实际=%s (G=%u)\n",
             name, expect_drawn ? "是" : "否", got ? "是" : "否", got);
      fails++;
   }
   glDeleteQueries(1, &q);
}

int main(void)
{
   XVisualInfo *vi;
   GLXContext ctx;
   Window win;
   int attribs[] = { GLX_RGBA, GLX_DOUBLEBUFFER, None };

   setvbuf(stdout, NULL, _IONBF, 0);

   dpy = XOpenDisplay(NULL);
   if (!dpy) { puts("no display"); return 2; }
   vi = glXChooseVisual(dpy, DefaultScreen(dpy), attribs);
   if (!vi) { puts("no visual"); return 2; }
   ctx = glXCreateContext(dpy, vi, NULL, True);
   {
      XSetWindowAttributes swa;
      memset(&swa, 0, sizeof swa);
      swa.colormap = XCreateColormap(dpy, RootWindow(dpy, vi->screen), vi->visual, AllocNone);
      win = XCreateWindow(dpy, RootWindow(dpy, vi->screen), 0, 0, W, H, 0, vi->depth,
                          InputOutput, vi->visual, CWBorderPixel | CWColormap, &swa);
   }
   glXMakeCurrent(dpy, win, ctx);

   pBegin = (PFNBCR)glXGetProcAddressARB((const GLubyte *)"glBeginConditionalRender");
   pEnd   = (PFNECR)glXGetProcAddressARB((const GLubyte *)"glEndConditionalRender");
   if (!pBegin || !pEnd) { puts("no glBeginConditionalRender"); return 2; }

   printf("GL_VERSION  = %s\n", glGetString(GL_VERSION));
   printf("GL_RENDERER = %s\n", glGetString(GL_RENDERER));
   printf("== 条件渲染语义 ==\n");

   /* A: 谓词成立 → 必须画 */
   run_case("WAIT  谓词成立 → 应绘制",   1, GL_QUERY_WAIT,          1);
   /* B: 谓词不成立 → 必须丢弃 */
   run_case("WAIT  谓词不成立 → 应丢弃", 0, GL_QUERY_WAIT,          0);
   /* C: 反相 + 谓词不成立 → 必须画 */
   run_case("INVERTED 谓词不成立 → 应绘制", 0, GL_QUERY_WAIT_INVERTED, 1);
   /* D: 反相 + 谓词成立 → 必须丢弃 */
   run_case("INVERTED 谓词成立 → 应丢弃",   1, GL_QUERY_WAIT_INVERTED, 0);
   /* E: NO_WAIT 也要遵循同一语义 */
   run_case("NO_WAIT 谓词不成立 → 应丢弃",  0, GL_QUERY_NO_WAIT,      0);

   printf("\n%s\n", fails ? "条件渲染: FAIL" : "条件渲染: PASS");
   glXMakeCurrent(dpy, None, NULL);
   glXDestroyContext(dpy, ctx);
   XDestroyWindow(dpy, win);
   XCloseDisplay(dpy);
   return fails ? 1 : 0;
}