/* 死区毒化 + Expose 重绘 — 有 map-monitor 时应自动恢复 */
#define _GNU_SOURCE
#include <X11/Xlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
int main(void){
    Display *dpy = XOpenDisplay(NULL);
    int scr = DefaultScreen(dpy);
    XSetWindowAttributes swa; swa.event_mask = ExposureMask|StructureNotifyMask; swa.background_pixel=0;
    Window win = XCreateWindow(dpy, RootWindow(dpy,scr), 300,300,400,300,0, DefaultDepth(dpy,scr), InputOutput, DefaultVisual(dpy,scr), CWEventMask|CWBackPixel, &swa);
    XStoreName(dpy,win,"guardTest");
    printf("WIN=0x%lx\n",win); fflush(stdout);
    XMapWindow(dpy,win);
    for(;;){XEvent e;XNextEvent(dpy,&e);if(e.type==MapNotify&&e.xmap.window==win)break;}
    XGCValues gcv; gcv.foreground=0xff0000;
    GC gc=XCreateGC(dpy,win,GCForeground,&gcv);
    XFillRectangle(dpy,win,gc,0,0,400,300);  /* 死区毒化 fill */
    XFlush(dpy);
    printf("POISONED\n"); fflush(stdout);
    time_t end=time(NULL)+10;
    while(time(NULL)<end){
        while(XPending(dpy)){
            XEvent e;XNextEvent(dpy,&e);
            if(e.type==Expose){
                printf("EXPOSE %dx%d\n",e.xexpose.width,e.xexpose.height); fflush(stdout);
                XFillRectangle(dpy,win,gc,e.xexpose.x,e.xexpose.y,e.xexpose.width,e.xexpose.height);
                XFlush(dpy);
            }
        }
        usleep(10000);
    }
    return 0;
}
