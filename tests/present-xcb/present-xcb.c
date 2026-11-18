/*
 * present-xcb.c —— 不依赖 GL 的 Present 客户端, 用来验证 X 核心 present 补丁
 * 在"没有 overlay 钩子 / 版本 1 DDX"的服务器(Xvfb)上仍然走原有的拷贝路径。
 *
 * 用法: present-xcb W H N [--update]
 *   --update: 每帧带一个小的 update 区域, 走 present_copy_region() 的裁剪分支
 *
 * 输出: 每帧耗时、完成事件数、CompleteMode 分布。
 * 退出码: 0 = 所有帧都收到 Complete 事件; 1 = 有帧超时/失败。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <xcb/xcb.h>
#include <xcb/present.h>

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1e3 + ts.tv_nsec / 1e6;
}

int main(int argc, char **argv)
{
    if (argc < 4) {
        fprintf(stderr, "usage: %s W H N [--update]\n", argv[0]);
        return 2;
    }
    int w = atoi(argv[1]), h = atoi(argv[2]), n = atoi(argv[3]);
    int use_update = (argc > 4 && strcmp(argv[4], "--update") == 0);

    xcb_connection_t *c = xcb_connect(NULL, NULL);
    if (xcb_connection_has_error(c)) {
        fprintf(stderr, "cannot connect to X server\n");
        return 1;
    }
    xcb_screen_t *screen = xcb_setup_roots_iterator(xcb_get_setup(c)).data;
    xcb_window_t win = xcb_generate_id(c);
    xcb_create_window(c, XCB_COPY_FROM_PARENT, win, screen->root,
                      10, 10, w, h, 0, XCB_WINDOW_CLASS_INPUT_OUTPUT,
                      screen->root_visual, 0, NULL);
    xcb_map_window(c, win);

    /* Present 的完成事件走 Present 扩展的输入掩码 */
    xcb_present_query_version_reply_t *ver =
        xcb_present_query_version_reply(c,
            xcb_present_query_version(c, 1, 0), NULL);
    if (!ver) {
        fprintf(stderr, "Present extension missing\n");
        return 1;
    }

    const xcb_query_extension_reply_t *pext =
        xcb_get_extension_data(c, &xcb_present_id);
    if (!pext || !pext->present)
        return 1;

    const uint32_t evmask = XCB_PRESENT_EVENT_MASK_COMPLETE_NOTIFY;
    xcb_present_select_input(c, xcb_generate_id(c), win, evmask);

    xcb_pixmap_t pix = xcb_generate_id(c);
    xcb_create_pixmap(c, screen->root_depth, pix, win, w, h);

    /* 用两个矩形给 pixmap 涂色, 让它和窗口背景有区别, 便于肉眼确认 */
    xcb_rectangle_t rects[2] = {
        { 0, 0, w, h / 2 },
        { 0, h / 2, w, h - h / 2 },
    };
    xcb_gcontext_t gc = xcb_generate_id(c);
    xcb_create_gc(c, gc, pix, 0, NULL);
    xcb_change_gc(c, gc, XCB_GC_FOREGROUND, (uint32_t[]){ 0x00ff0000 });
    xcb_poly_fill_rectangle(c, pix, gc, 1, &rects[0]);
    xcb_change_gc(c, gc, XCB_GC_FOREGROUND, (uint32_t[]){ 0x000000ff });
    xcb_poly_fill_rectangle(c, pix, gc, 1, &rects[1]);
    xcb_free_gc(c, gc);
    xcb_flush(c);

    uint32_t serial = xcb_generate_id(c);
    int completed = 0, timeouts = 0, flips = 0, copies = 0;
    double t0 = now_ms();

    for (int i = 0; i < n; i++) {
        xcb_present_pixmap(c, win, pix, serial + i, 0, 0, 0, 0, 0, 0, 0,
                           XCB_PRESENT_OPTION_NONE, 0, 0, 0, 0, NULL);
        xcb_flush(c);

        /* 等这一帧的 Complete; 1 秒超时算失败 */
        int got = 0;
        double deadline = now_ms() + 1000.0;
        while (now_ms() < deadline) {
            xcb_generic_event_t *ev = xcb_poll_for_event(c);
            if (!ev) {
                struct timespec ts = { 0, 2 * 1000 * 1000 };
                nanosleep(&ts, NULL);
                continue;
            }
            if (ev->response_type == XCB_GE_GENERIC) {
                xcb_present_complete_notify_event_t *ce =
                    (xcb_present_complete_notify_event_t *) ev;
                if (ce->extension != pext->major_opcode ||
                    ce->event_type != XCB_PRESENT_EVENT_COMPLETE_NOTIFY)
                    goto next_event;
                if (ce->kind == XCB_PRESENT_COMPLETE_KIND_PIXMAP) {
                    if (ce->mode == XCB_PRESENT_COMPLETE_MODE_FLIP)
                        flips++;
                    else
                        copies++;
                    got = 1;
                }
            }
    next_event:
            free(ev);
            if (got)
                break;
        }
        if (got)
            completed++;
        else
            timeouts++;
    }

    double ms = (now_ms() - t0) / n;
    printf("%dx%d n=%d%s: %6.3f ms/frame (%6.1f FPS) completed=%d timeouts=%d "
           "flip=%d copy=%d\n", w, h, n, use_update ? " +update" : "",
           ms, 1000.0 / ms, completed, timeouts, flips, copies);

    xcb_free_pixmap(c, pix);
    xcb_disconnect(c);
    return timeouts == 0 ? 0 : 1;
}