/*
 * xcopy —— 触发 X 的 2D 拷贝路径（EXA/glamor 都会把 XCopyArea 变成引擎命令）。
 *
 * 用途：配合 j2dcap 抓厂商 DDX 的 2D 命令流；同一条命令在我们栈上则走
 * glamor（3D 着色器/blit），可用来对照"厂商用哪条路"。
 *
 * 用法：xcopy [display] [size] [count]
 */
#include <X11/Xlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
	const char *disp = argc > 1 ? argv[1] : ":0";
	int size = argc > 2 ? atoi(argv[2]) : 256;
	int count = argc > 3 ? atoi(argv[3]) : 50;
	int depth = argc > 4 ? atoi(argv[4]) : 0;      /* 0 = 屏幕默认深度 */
	const char *dest = argc > 5 ? argv[5] : "pix"; /* pix = 拷到 pixmap, win = 拷到窗口 */
	Display *d;
	Window win;
	GC gc;
	Pixmap a, b;
	int i;

	d = XOpenDisplay(disp);
	if (!d) {
		fprintf(stderr, "xcopy: 打不开显示 %s\n", disp);
		return 1;
	}
	win = XCreateSimpleWindow(d, DefaultRootWindow(d), 0, 0,
				  (unsigned)size, (unsigned)size, 0, 0, 0xffffff);
	XMapWindow(d, win);
	XSync(d, False);

	if (depth <= 0)
		depth = DefaultDepth(d, DefaultScreen(d));
	a = XCreatePixmap(d, win, (unsigned)size, (unsigned)size, (unsigned)depth);
	b = XCreatePixmap(d, win, (unsigned)size, (unsigned)size, (unsigned)depth);
	/* GC 必须与 pixmap 同深度，否则 XFillRectangle 会 BadMatch */
	gc = XCreateGC(d, a, 0, NULL);

	XSetForeground(d, gc, 0x3366cc);
	XFillRectangle(d, a, gc, 0, 0, (unsigned)size, (unsigned)size);
	XSetForeground(d, gc, 0xcc3366);
	XFillRectangle(d, b, gc, 0, 0, (unsigned)size, (unsigned)size);
	XSync(d, False);

	if (count <= 0) {
		/* 单次拷贝：给命令流做"最干净"的对照样本（默认 pixmap→pixmap） */
		if (dest[0] == 'w')
			XCopyArea(d, a, win, gc, 0, 0, (unsigned)size, (unsigned)size, 0, 0);
		else
			XCopyArea(d, a, b, gc, 0, 0, (unsigned)size, (unsigned)size, 0, 0);
		XSync(d, False);
	} else {
		for (i = 0; i < count; i++) {
			XCopyArea(d, a, b, gc, 0, 0, (unsigned)size, (unsigned)size, 0, 0);
			XCopyArea(d, b, win, gc, 0, 0, (unsigned)size, (unsigned)size, 0, 0);
			XSync(d, False);
		}
	}

	XFreePixmap(d, a);
	XFreePixmap(d, b);
	XFreeGC(d, gc);
	XDestroyWindow(d, win);
	XCloseDisplay(d);
	printf("xcopy: %s 深度=%d -> %s, %dx%d (count=%d) 完成\n",
	       disp, depth, dest, size, size, count);
	return 0;
}
