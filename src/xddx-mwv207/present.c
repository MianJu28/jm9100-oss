/*
 * Copyright © 2014 Intel Corporation
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

#ifdef HAVE_DIX_CONFIG_H
#include "dix-config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <time.h>

#include <xf86.h>
#include <xf86Crtc.h>
#include <xf86drm.h>
#include <xf86str.h>
#include <drm_fourcc.h>
#include <present.h>

#include "driver.h"
#include "drmmode_display.h"

#if 0
#define DebugPresent(x) ErrorF x
#else
#define DebugPresent(x)
#endif

struct mwx_present_vblank_event {
    uint64_t        event_id;
    Bool            unflip;
};

static RRCrtcPtr
mwx_present_get_crtc(WindowPtr window)
{
    return mwx_randr_crtc_covering_drawable(&window->drawable);
}

static int
mwx_present_get_ust_msc(RRCrtcPtr crtc, CARD64 *ust, CARD64 *msc)
{
    xf86CrtcPtr xf86_crtc = crtc->devPrivate;

    return mwx_get_crtc_ust_msc(xf86_crtc, ust, msc);
}

/*
 * Changes the variable refresh state for every CRTC on the screen.
 */
void
mwx_present_set_screen_vrr(ScrnInfoPtr scrn, Bool vrr_enabled)
{
    xf86CrtcConfigPtr config = XF86_CRTC_CONFIG_PTR(scrn);
    xf86CrtcPtr crtc;
    int i;

    for (i = 0; i < config->num_crtc; i++) {
        crtc = config->crtc[i];
        drmmode_crtc_set_vrr(crtc, vrr_enabled);
    }
}

/*
 * Called when the queued vblank event has occurred
 */
static void
mwx_present_vblank_handler(uint64_t msc, uint64_t usec, void *data)
{
    struct mwx_present_vblank_event *event = data;

    DebugPresent(("\t\tmh %lld msc %llu\n",
                 (long long) event->event_id, (long long) msc));

    present_event_notify(event->event_id, usec, msc);
    free(event);
}

/*
 * Called when the queued vblank is aborted
 */
static void
mwx_present_vblank_abort(void *data)
{
    struct mwx_present_vblank_event *event = data;

    DebugPresent(("\t\tma %lld\n", (long long) event->event_id));

    free(event);
}

/*
 * Queue an event to report back to the Present extension when the specified
 * MSC has past
 */
static int
mwx_present_queue_vblank(RRCrtcPtr crtc,
                        uint64_t event_id,
                        uint64_t msc)
{
    xf86CrtcPtr xf86_crtc = crtc->devPrivate;
    struct mwx_present_vblank_event *event;
    uint32_t seq;

    event = calloc(sizeof(struct mwx_present_vblank_event), 1);
    if (!event)
        return BadAlloc;
    event->event_id = event_id;
    seq = mwx_drm_queue_alloc(xf86_crtc, event,
                             mwx_present_vblank_handler,
                             mwx_present_vblank_abort);
    if (!seq) {
        free(event);
        return BadAlloc;
    }

    if (!mwx_queue_vblank(xf86_crtc, MS_QUEUE_ABSOLUTE, msc, NULL, seq))
        return BadAlloc;

    DebugPresent(("\t\tmq %lld seq %u msc %llu\n",
                 (long long) event_id, seq, (long long) msc));
    return Success;
}

static Bool
mwx_present_event_match(void *data, void *match_data)
{
    struct mwx_present_vblank_event *event = data;
    uint64_t *match = match_data;

    return *match == event->event_id;
}

/*
 * Remove a pending vblank event from the DRM queue so that it is not reported
 * to the extension
 */
static void
mwx_present_abort_vblank(RRCrtcPtr crtc, uint64_t event_id, uint64_t msc)
{
    ScreenPtr screen = crtc->pScreen;
    ScrnInfoPtr scrn = xf86ScreenToScrn(screen);

    mwx_drm_abort(scrn, mwx_present_event_match, &event_id);
}

/*
 * Flush our batch buffer when requested by the Present extension.
 */
static void
mwx_present_flush(WindowPtr window)
{
#ifdef GLAMOR_HAS_GBM
    ScreenPtr screen = window->drawable.pScreen;
    ScrnInfoPtr scrn = xf86ScreenToScrn(screen);
    mwv207Ptr ms = mwv207PTR(scrn);

    if (ms->drmmode.glamor)
        ms->glamor.block_handler(screen);
#endif
}

#ifdef GLAMOR_HAS_GBM

/**
 * Callback for the DRM event queue when a flip has completed on all pipes
 *
 * Notify the extension code
 */
static void
mwx_present_flip_handler(mwv207Ptr ms, uint64_t msc,
                        uint64_t ust, void *data)
{
    struct mwx_present_vblank_event *event = data;

    DebugPresent(("\t\tms:fc %lld msc %llu ust %llu\n",
                  (long long) event->event_id,
                  (long long) msc, (long long) ust));

    if (event->unflip)
        ms->drmmode.present_flipping = FALSE;

    mwx_present_vblank_handler(msc, ust, event);
}

/*
 * Callback for the DRM queue abort code.  A flip has been aborted.
 */
static void
mwx_present_flip_abort(mwv207Ptr ms, void *data)
{
    struct mwx_present_vblank_event *event = data;

    DebugPresent(("\t\tms:fa %lld\n", (long long) event->event_id));

    free(event);
}

/*
 * Test to see if page flipping is possible on the target crtc
 *
 * We ignore sw-cursors when *disabling* flipping, we may very well be
 * returning to scanning out the normal framebuffer *because* we just
 * switched to sw-cursor mode and check_flip just failed because of that.
 */
static Bool
mwx_present_check_unflip(RRCrtcPtr crtc,
                        WindowPtr window,
                        PixmapPtr pixmap,
                        Bool sync_flip,
                        PresentFlipReason *reason)
{
    ScreenPtr screen = window->drawable.pScreen;
    ScrnInfoPtr scrn = xf86ScreenToScrn(screen);
    mwv207Ptr ms = mwv207PTR(scrn);
    xf86CrtcConfigPtr config = XF86_CRTC_CONFIG_PTR(scrn);
    int num_crtcs_on = 0;
    int i;
    struct gbm_bo *gbm;

    if (!ms->drmmode.pageflip)
        return FALSE;

    if (ms->drmmode.dri2_flipping)
        return FALSE;

    if (!scrn->vtSema)
        return FALSE;

    for (i = 0; i < config->num_crtc; i++) {
        drmmode_crtc_private_ptr drmmode_crtc = config->crtc[i]->driver_private;

        /* Don't do pageflipping if CRTCs are rotated. */
        if (drmmode_crtc->rotate_bo.gbm)
            return FALSE;

        if (xf86_crtc_on(config->crtc[i]))
            num_crtcs_on++;
    }

    /* We can't do pageflipping if all the CRTCs are off. */
    if (num_crtcs_on == 0)
        return FALSE;

    /*
     * Check stride, can't change that reliably on flip on some drivers, unless
     * the kms driver is atomic_modeset_capable.
     */
    if (!ms->atomic_modeset_capable &&
        pixmap->devKind != drmmode_bo_get_pitch(&ms->drmmode.front_bo))
        return FALSE;

    if (!ms->drmmode.glamor)
        return FALSE;

#ifdef GBM_BO_WITH_MODIFIERS
    /* Check if buffer format/modifier is supported by all active CRTCs */
    gbm = ms->glamor.gbm_bo_from_pixmap(screen, pixmap);
    if (gbm) {
        uint32_t format;
        uint64_t modifier;

        format = gbm_bo_get_format(gbm);
        modifier = gbm_bo_get_modifier(gbm);
        gbm_bo_destroy(gbm);

        if (!drmmode_is_format_supported(scrn, format, modifier)) {
            if (reason)
                *reason = PRESENT_FLIP_REASON_BUFFER_FORMAT;
            return FALSE;
        }
    }
#endif

    /* Make sure there's a bo we can get to */
    /* XXX: actually do this.  also...is it sufficient?
     * if (!glamor_get_pixmap_private(pixmap))
     *     return FALSE;
     */

    return TRUE;
}

static Bool
mwx_present_check_flip(RRCrtcPtr crtc,
                      WindowPtr window,
                      PixmapPtr pixmap,
                      Bool sync_flip,
                      PresentFlipReason *reason)
{
    ScreenPtr screen = window->drawable.pScreen;
    ScrnInfoPtr scrn = xf86ScreenToScrn(screen);
    mwv207Ptr ms = mwv207PTR(scrn);

    if (ms->drmmode.sprites_visible > 0)
        return FALSE;

    if(!mwx_present_check_unflip(crtc, window, pixmap, sync_flip, reason))
        return FALSE;

    ms->flip_window = window;

    return TRUE;
}

/*
 * Turn off the hardware overlay plane on every CRTC and forget the window we
 * were scanning out.
 */
static void
mwx_present_overlay_disable(ScrnInfoPtr scrn)
{
    mwv207Ptr ms = mwv207PTR(scrn);
    xf86CrtcConfigPtr config = XF86_CRTC_CONFIG_PTR(scrn);
    int i;

    for (i = 0; i < config->num_crtc; i++) {
        xf86CrtcPtr crtc = config->crtc[i];
        drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;

        if (!crtc->enabled || !drmmode_crtc->overlay_plane_id)
            continue;
        drmmode_overlay_plane_disable(crtc);
    }

    ms->overlay_active = FALSE;
    ms->overlay_window = NULL;
}

/*
 * Can 'pixmap' be scanned out by the hardware overlay plane at the window's
 * position?
 *
 * This is the zero-copy Present path.  Instead of copying the client's buffer
 * into the screen pixmap every frame (which costs about a third of a
 * millisecond at 800x600) WIN0 is pointed directly at the client buffer and
 * the display controller composites it on top of the primary plane.
 */
static Bool
mwx_present_check_overlay(RRCrtcPtr crtc, WindowPtr window, PixmapPtr pixmap,
                          int16_t x_off, int16_t y_off)
{
    ScreenPtr screen = window->drawable.pScreen;
    ScrnInfoPtr scrn = xf86ScreenToScrn(screen);
    mwv207Ptr ms = mwv207PTR(scrn);
    xf86CrtcPtr xf86_crtc;
    drmmode_crtc_private_ptr drmmode_crtc;
    RegionRec win_region;
    BoxRec box;
    Bool fully_visible;

    if (!crtc || !scrn->vtSema || !ms->drmmode.glamor)
        return FALSE;
    if (!ms->glamor.gbm_bo_from_pixmap)
        return FALSE;

    xf86_crtc = crtc->devPrivate;
    if (!xf86_crtc || !xf86_crtc_on(xf86_crtc))
        return FALSE;

    drmmode_crtc = xf86_crtc->driver_private;
    if (!drmmode_crtc->overlay_plane_id)
        return FALSE;

    /* The overlay plane sits above the primary plane, so it can only be used
     * when nothing else overlaps the window and the window is not redirected:
     * whatever the plane covers becomes invisible, including other windows.
     */
    if (window->parent != screen->root)
        return FALSE;
    if (screen->GetWindowPixmap(window) != screen->GetScreenPixmap(screen))
        return FALSE;

    /* Source pixmap must align with window exactly */
    if (x_off || y_off)
        return FALSE;
    if (window->drawable.width != pixmap->drawable.width ||
        window->drawable.height != pixmap->drawable.height)
        return FALSE;

    /* Negative window coordinates cannot be expressed in the WIN0 position
     * register, and the plane must stay inside the CRTC.
     */
    if (window->drawable.x < 0 || window->drawable.y < 0)
        return FALSE;
    if (window->drawable.x + window->drawable.width > xf86_crtc->mode.HDisplay ||
        window->drawable.y + window->drawable.height > xf86_crtc->mode.VDisplay)
        return FALSE;

    /* Nothing may cover the window */
    box.x1 = window->drawable.x;
    box.y1 = window->drawable.y;
    box.x2 = box.x1 + window->drawable.width;
    box.y2 = box.y1 + window->drawable.height;
    RegionInit(&win_region, &box, 1);
    fully_visible = RegionEqual(&window->clipList, &win_region);
    RegionUninit(&win_region);
    if (!fully_visible)
        return FALSE;

    switch (pixmap->drawable.depth) {
    case 24:
    case 32:
        break;
    default:
        return FALSE;
    }

    /* The KMD programs the overlay with a 64 byte aligned pitch and only
     * supports linear scanout, so only such buffers can be handed over.
     */
    {
        struct gbm_bo *gbm = ms->glamor.gbm_bo_from_pixmap(screen, pixmap);
        uint32_t stride, format;
        uint64_t modifier = DRM_FORMAT_MOD_LINEAR;

        if (!gbm)
            return FALSE;

        stride = gbm_bo_get_stride(gbm);
        format = gbm_bo_get_format(gbm);
#ifdef GBM_BO_WITH_MODIFIERS
        modifier = gbm_bo_get_modifier(gbm);
#endif
        gbm_bo_destroy(gbm);

        if (stride & 0x3f)
            return FALSE;
        if (modifier != DRM_FORMAT_MOD_LINEAR)
            return FALSE;
        if (format != DRM_FORMAT_ARGB8888 && format != DRM_FORMAT_XRGB8888 &&
            format != DRM_FORMAT_ABGR8888 && format != DRM_FORMAT_XBGR8888)
            return FALSE;
    }

    return TRUE;
}

/*
 * Scan 'pixmap' out through the overlay plane of 'crtc' at the window's
 * position.  This replaces the page flip of the primary plane: no buffer is
 * copied and no window pixmap is changed.
 */
static Bool
mwx_present_overlay_flip(RRCrtcPtr crtc, WindowPtr window, PixmapPtr pixmap,
                         int16_t x_off, int16_t y_off, uint64_t event_id)
{
    ScreenPtr screen = window->drawable.pScreen;
    ScrnInfoPtr scrn = xf86ScreenToScrn(screen);
    mwv207Ptr ms = mwv207PTR(scrn);
    xf86CrtcPtr xf86_crtc = crtc->devPrivate;
    struct mwx_present_vblank_event *event;
    uint32_t seq;

    if (!mwx_present_check_overlay(crtc, window, pixmap, x_off, y_off))
        return FALSE;

    if (drmmode_overlay_plane_set(xf86_crtc, pixmap,
                                  window->drawable.x,
                                  window->drawable.y) != 0) {
        xf86DrvMsg(scrn->scrnIndex, X_ERROR,
                   "overlay: failed to point the plane at the client buffer\n");
        return FALSE;
    }

    ms->overlay_active = TRUE;
    ms->overlay_window = window;

    /* Report completion at the next vblank, the same way a page flip would.  If
     * the vblank event cannot be queued we still have to tell Present that the
     * buffer reached the screen, otherwise the flip_pending slot would stay
     * occupied forever.
     */
    event = calloc(1, sizeof(struct mwx_present_vblank_event));
    if (!event) {
        present_event_notify(event_id, 0, 0);
        return TRUE;
    }

    event->event_id = event_id;
    seq = mwx_drm_queue_alloc(xf86_crtc, event, mwx_present_vblank_handler,
                             mwx_present_vblank_abort);
    if (!seq) {
        free(event);
        present_event_notify(event_id, 0, 0);
        return TRUE;
    }

    /* On failure mwx_queue_vblank drains the queue entry itself, so report
     * completion right away.
     */
    if (!mwx_queue_vblank(xf86_crtc, MS_QUEUE_RELATIVE, 0, NULL, seq))
        present_event_notify(event_id, 0, 0);

    return TRUE;
}

/*
 * Queue a flip on 'crtc' to 'pixmap' at 'target_msc'. If 'sync_flip' is true,
 * then wait for vblank. Otherwise, flip immediately
 */
static Bool
mwx_present_flip(RRCrtcPtr crtc,
                uint64_t event_id,
                uint64_t target_msc,
                PixmapPtr pixmap,
                Bool sync_flip)
{
    ScreenPtr screen = crtc->pScreen;
    ScrnInfoPtr scrn = xf86ScreenToScrn(screen);
    mwv207Ptr ms = mwv207PTR(scrn);
    xf86CrtcPtr xf86_crtc = crtc->devPrivate;
    drmmode_crtc_private_ptr drmmode_crtc = xf86_crtc->driver_private;
    Bool ret;
    struct mwx_present_vblank_event *event;

    if (!mwx_present_check_flip(crtc, ms->flip_window, pixmap, sync_flip, NULL))
        return FALSE;

    event = calloc(1, sizeof(struct mwx_present_vblank_event));
    if (!event)
        return FALSE;

    DebugPresent(("\t\tms:pf %lld msc %llu\n",
                  (long long) event_id, (long long) target_msc));

    event->event_id = event_id;
    event->unflip = FALSE;

    /* A window can only flip if it covers the entire X screen.
     * Only one window can flip at a time.
     *
     * If the window also has the variable refresh property then
     * variable refresh supported can be enabled on every CRTC.
     */
    if (ms->vrr_support && ms->is_connector_vrr_capable &&
          mwx_window_has_variable_refresh(ms, ms->flip_window)) {
        mwx_present_set_screen_vrr(scrn, TRUE);
    }

    ret = mwx_do_pageflip(screen, pixmap, event, drmmode_crtc->vblank_pipe, !sync_flip,
                         mwx_present_flip_handler, mwx_present_flip_abort,
                         "Present-flip");
    if (ret)
        ms->drmmode.present_flipping = TRUE;

    return ret;
}

/*
 * Queue a flip back to the normal frame buffer
 */
static void
mwx_present_unflip(ScreenPtr screen, uint64_t event_id)
{
    ScrnInfoPtr scrn = xf86ScreenToScrn(screen);
    mwv207Ptr ms = mwv207PTR(scrn);
    PixmapPtr pixmap = screen->GetScreenPixmap(screen);
    xf86CrtcConfigPtr config = XF86_CRTC_CONFIG_PTR(scrn);
    int i;

    mwx_present_set_screen_vrr(scrn, FALSE);

    /* Zero-copy overlay was in use: stop scanning the client's buffer out and
     * let the primary plane show the screen pixmap again.  There is nothing to
     * flip, only a plane update.
     */
    if (ms->overlay_active) {
        mwx_present_overlay_disable(scrn);
        present_event_notify(event_id, 0, 0);
        ms->drmmode.present_flipping = FALSE;
        return;
    }

    if (mwx_present_check_unflip(NULL, screen->root, pixmap, TRUE, NULL)) {
        struct mwx_present_vblank_event *event;

        event = calloc(1, sizeof(struct mwx_present_vblank_event));
        if (!event)
            return;

        event->event_id = event_id;
        event->unflip = TRUE;

        if (mwx_do_pageflip(screen, pixmap, event, -1, FALSE,
                           mwx_present_flip_handler, mwx_present_flip_abort,
                           "Present-unflip")) {
            return;
        }
    }

    for (i = 0; i < config->num_crtc; i++) {
        xf86CrtcPtr crtc = config->crtc[i];
	drmmode_crtc_private_ptr drmmode_crtc = crtc->driver_private;

	if (!crtc->enabled)
	    continue;

	/* info->drmmode.fb_id still points to the FB for the last flipped BO.
	 * Clear it, drmmode_set_mode_major will re-create it
	 */
	if (drmmode_crtc->drmmode->fb_id) {
		drmModeRmFB(drmmode_crtc->drmmode->fd,
			    drmmode_crtc->drmmode->fb_id);
		drmmode_crtc->drmmode->fb_id = 0;
	}

	if (drmmode_crtc->dpms_mode == DPMSModeOn)
	    crtc->funcs->set_mode_major(crtc, &crtc->mode, crtc->rotation,
					crtc->x, crtc->y);
	else
	    drmmode_crtc->need_modeset = TRUE;
    }

    present_event_notify(event_id, 0, 0);
    ms->drmmode.present_flipping = FALSE;
}
#endif

static present_screen_info_rec mwx_present_screen_info = {
    .version = PRESENT_SCREEN_INFO_VERSION,

    .get_crtc = mwx_present_get_crtc,
    .get_ust_msc = mwx_present_get_ust_msc,
    .queue_vblank = mwx_present_queue_vblank,
    .abort_vblank = mwx_present_abort_vblank,
    .flush = mwx_present_flush,

    .capabilities = PresentCapabilityNone,
#ifdef GLAMOR_HAS_GBM
    .check_flip = NULL,
    .check_flip2 = mwx_present_check_flip,
    .flip = mwx_present_flip,
    .unflip = mwx_present_unflip,
    .check_overlay = mwx_present_check_overlay,
    .overlay_flip = mwx_present_overlay_flip,
#endif
};

Bool
mwx_present_screen_init(ScreenPtr screen)
{
    ScrnInfoPtr scrn = xf86ScreenToScrn(screen);
    mwv207Ptr ms = mwv207PTR(scrn);
    uint64_t value;
    int ret;

    ret = drmGetCap(ms->fd, DRM_CAP_ASYNC_PAGE_FLIP, &value);
    if (ret == 0 && value == 1) {
        mwx_present_screen_info.capabilities |= PresentCapabilityAsync;
        ms->drmmode.can_async_flip = TRUE;
    }

    return present_screen_init(screen, &mwx_present_screen_info);
}
