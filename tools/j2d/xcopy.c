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

	a = XCreatePixmap(d, win, (unsigned)size, (unsigned)size, DefaultDepth(d, DefaultScreen(d)));
	b = XCreatePixmap(d, win, (unsigned)size, (unsigned)size, DefaultDepth(d, DefaultScreen(d)));
	gc = XCreateGC(d, win, 0, NULL);

	XSetForeground(d, gc, 0x3366cc);
	XFillRectangle(d, a, gc, 0, 0, (unsigned)size, (unsigned)size);
	XSetForeground(d, gc, 0xcc3366);
	XFillRectangle(d, b, gc, 0, 0, (unsigned)size, (unsigned)size);
	XSync(d, False);

	if (count <= 0) {
		/* 单次 pixmap→pixmap 拷贝：给命令流做"最干净"的对照样本 */
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
	printf("xcopy: %d 次 XCopyArea(%dx%d) 完成\n", count, size, size);
	return 0;
}
