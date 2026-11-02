/*
 * map-monitor — JM9100 X11 首帧死区守护 v2
 *
 * 根因: 窗口 map 后 0-4ms 内的第一次 X 层绘制(XFillRectangle/XCopyArea/
 * EGL present/DRI2 swap)被永久丢弃(Xorg 合成器 backing pixmap 分配竞态)。
 * 受影响: Firefox 冷启动首页/恢复页黑屏等"map 后立即绘制"的应用。
 *
 * 策略 v2(温和化): 监听 root SubstructureNotify 的 MapNotify(仅顶层窗),
 * 50ms 后发 1x1 XClearArea(exposures=True)。1x1 足以触发客户端 Expose
 * → WebRender/标准 X11 客户端全量重绘 damage 区域 → 死区内容补上。
 * 不清全窗(避免闪烁), 吞掉 BadMatch(窗口死亡竞态), 跳过 InputOnly。
 *
 * 已验证: x11-expose-recovery + x11-guard-test 探针均恢复 (254,0,0)。
 */
#define _GNU_SOURCE
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <time.h>

#define DELAY_MS 50

static Display *g_dpy;
static volatile int g_run = 1;

typedef struct {
    Window win;
    struct timespec due;
} pend_t;

static pend_t g_pend[4096];
static int g_np = 0;
static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;

static void msleep(int ms){ struct timespec t={ms/1000,(long)(ms%1000)*1000000}; nanosleep(&t,NULL); }

/* 吞掉异步错误(BadMatch 等 — 窗口死亡竞态) */
static int err_handler(Display *d, XErrorEvent *e){ (void)d;(void)e; return 0; }
static int io_err_handler(Display *d){ (void)d; g_run=0; return 0; }

/* 线程: 到点发 1x1 XClearArea */
static void *worker(void *arg){
    (void)arg;
    while (g_run) {
        pend_t batch[64]; int n=0;
        pthread_mutex_lock(&g_mu);
        struct timespec now; clock_gettime(CLOCK_MONOTONIC, &now);
        for (int i=0;i<g_np;) {
            if (now.tv_sec > g_pend[i].due.tv_sec ||
                (now.tv_sec==g_pend[i].due.tv_sec && now.tv_nsec>=g_pend[i].due.tv_nsec)) {
                if (n<64) batch[n++]=g_pend[i];
                memmove(&g_pend[i], &g_pend[i+1], (size_t)(g_np-i-1)*sizeof(pend_t));
                g_np--; continue;
            }
            i++;
        }
        pthread_mutex_unlock(&g_mu);
        for (int i=0;i<n;i++) {
            XClearArea(g_dpy, batch[i].win, 0, 0, 1, 1, True);
            XFlush(g_dpy);
            if (getenv("MAPMON_DEBUG"))
                fprintf(stderr, "[mapmon] clear 0x%lx (1x1)\n", batch[i].win);
        }
        msleep(5);
    }
    return NULL;
}

int main(int argc, char **argv){
    (void)argc; (void)argv;
    g_dpy = XOpenDisplay(NULL);
    if (!g_dpy) { fprintf(stderr,"[mapmon] cannot open display\n"); return 1; }
    XSetErrorHandler(err_handler);
    XSetIOErrorHandler(io_err_handler);
    Window root = DefaultRootWindow(g_dpy);
    XSelectInput(g_dpy, root, SubstructureNotifyMask);
    XFlush(g_dpy);
    if (getenv("MAPMON_DEBUG"))
        fprintf(stderr, "[mapmon] watching root 0x%lx\n", root);
    pthread_t th; pthread_create(&th, NULL, worker, NULL);

    while (g_run) {
        XEvent ev;
        XNextEvent(g_dpy, &ev);
        if (ev.type == MapNotify) {
            /* 只处理 root 直接子窗(顶层窗), InputOnly 跳过 */
            XWindowAttributes wa;
            Window dummy_root, parent; Window *kids=NULL; unsigned nk=0;
            if (!XQueryTree(g_dpy, ev.xmap.window, &dummy_root, &parent, &kids, &nk))
                continue;
            if (kids) XFree(kids);
            if (parent != root) continue;               /* 非顶层 */
            if (!XGetWindowAttributes(g_dpy, ev.xmap.window, &wa))
                continue;
            if (wa.class == InputOnly) continue;
            if (wa.override_redirect) continue;         /* OR 窗不毒化 */
            pthread_mutex_lock(&g_mu);
            if (g_np < 4096) {
                g_pend[g_np].win = ev.xmap.window;
                struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
                t.tv_nsec += (long)DELAY_MS*1000000; if (t.tv_nsec>=1000000000){t.tv_sec++;t.tv_nsec-=1000000000;}
                g_pend[g_np].due = t; g_np++;
            }
            pthread_mutex_unlock(&g_mu);
        }
    }
    return 0;
}
