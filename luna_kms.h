/*
 * luna_kms.h — bare-console host for Luna UI: DRM/KMS + GBM + EGL + libinput.
 *
 * No display server is involved.  The host takes the DRM master's primary
 * plane, page-flips scanout buffers handed straight from EGL/GBM (no CPU
 * copy), draws the pointer on the hardware cursor plane, and reads the seat
 * through libinput/xkbcommon.  This is the production path for a shell that
 * boots straight into the framebuffer.
 *
 * Two entry points, one implementation: luna_app_run() for applications
 * (selected with -DLUNA_UI_BACKEND_KMS), and luna_host_kms() for an embedder
 * that owns its main loop (see LunaHostOps in luna-ui.h).
 *
 * Link with: -lEGL -lgbm -ldrm -linput -ludev -lxkbcommon
 *
 * Copyright © 2026 Yuichiro Nakada / Project Vespera — MPL 2.0
 */
#if defined(LUNA_UI_PLATFORM_PRELUDE) || defined(LUNA_UI_HOST_PRELUDE)
#ifndef LUNA_KMS_PRELUDE_INCLUDED
#define LUNA_KMS_PRELUDE_INCLUDED
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
#include <dlfcn.h>
#include <time.h>
#include <sys/mman.h>

#include <gbm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <libinput.h>
#include <libudev.h>
#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon.h>

/* The renderer needs GL types and enum constants.  Every gl* call in
 * luna-ui.h goes through function pointers resolved by get_proc(), so this is
 * only a type/constant header — the libGL symbols themselves are never used,
 * and their separate GLX current-context slot is never touched. */
#include <GL/gl.h>
#include <GL/glext.h>
#define LUNA_UI_PLATFORM_GL_INCLUDED 1
#endif
#endif

#if defined(LUNA_UI_PLATFORM_BODY) || defined(LUNA_UI_HOST_BODY)
#ifndef LUNA_KMS_BODY_INCLUDED
#define LUNA_KMS_BODY_INCLUDED

#if defined(LUNA_UI_IMPLEMENTATION)

/* libinput reports raw evdev codes, which luna-keys.h maps exactly. */
#define LUNA_KEYS_EVDEV_TABLE 1
#include "luna-keys.h"

/* 64×64 is the size essentially every KMS driver accepts on the cursor plane. */
#define LUNA_KMS_CURSOR_SIZE 64

typedef struct LunaKmsHost {
    int fd;
    uint32_t conn_id, crtc_id;
    drmModeModeInfo mode;
    drmModeCrtc* saved_crtc;
    int width, height;

    struct gbm_device*  gbm_dev;
    struct gbm_surface* gbm_surf;
    struct gbm_bo*      prev_bo;

    EGLDisplay dpy;
    EGLContext ctx;
    EGLSurface surf;
    int flip_pending;

    struct udev*        udev;
    struct libinput*    li;
    struct xkb_context* xkb_ctx;
    struct xkb_keymap*  xkb_keymap;
    struct xkb_state*   xkb_state;

    double mouse_x, mouse_y;
    int    buttons[8];          /* LUNA_PRESS / LUNA_RELEASE, by Luna index */

    /* Hardware cursor: ARGB dumb buffer on the cursor plane. */
    uint32_t  cursor_handle;
    uint32_t  cursor_pitch;
    uint64_t  cursor_size;
    uint32_t* cursor_map;
    int       cursor_w, cursor_h;
    int       cursor_hot_x, cursor_hot_y;
    int       cursor_type;
    int       cursor_ok;        /* dumb buffer created and mapped */
    int       cursor_shown;     /* drmModeSetCursor2 accepted */

    LunaHostConfig config;
    int  should_close;
    unsigned event_seq;         /* bumped by every dispatched input event */
    void* gl_lib;               /* dlopen handle for the get_proc fallback */
} LunaKmsHost;

static LunaKmsHost luna_kms;

static void luna_kms_cursor_move(void);

/* ── Platform entry points ─────────────────────────────────────────────── */

static double luna_kms_time(void) {
    static struct timespec t0;
    static int started = 0;
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    if (!started) { t0 = ts; started = 1; }
    return (double)(ts.tv_sec - t0.tv_sec) + (double)(ts.tv_nsec - t0.tv_nsec) / 1e9;
}

/*
 * eglGetProcAddress() is only spec-guaranteed to resolve *extension*
 * functions — core entry points like glGenTextures/glCreateShader may come
 * back NULL depending on the driver.  luna-ui.h loads core and extension calls
 * alike through this callback, so an unresolved core function is a NULL call
 * (and a crash) the first time it is used, typically well after startup.  Fall
 * back to dlsym on a vendor-neutral library, opened RTLD_LOCAL so it cannot
 * re-route direct gl* calls through a second, GLX-flavoured dispatch table.
 */
static void* luna_kms_get_proc(const char* name) {
    void* p = (void*)eglGetProcAddress(name);
    if (p) return p;
    if (!luna_kms.gl_lib) {
        /* libOpenGL.so.0 is the EGL-compatible, GLX-free glvnd library
         * (Mesa 17.3+).  libGL.so.1 is the fallback when it is absent. */
        luna_kms.gl_lib = dlopen("libOpenGL.so.0", RTLD_NOW | RTLD_LOCAL);
        if (!luna_kms.gl_lib) luna_kms.gl_lib = dlopen("libGL.so.1", RTLD_NOW | RTLD_LOCAL);
        if (!luna_kms.gl_lib) luna_kms.gl_lib = dlopen("libGL.so",   RTLD_NOW | RTLD_LOCAL);
    }
    if (luna_kms.gl_lib) p = dlsym(luna_kms.gl_lib, name);
    if (!p) fprintf(stderr, "[luna-ui/kms] warning: unresolved GL symbol %s\n", name);
    return p;
}

static void luna_kms_request_close(void) { luna_kms.should_close = 1; }
static void luna_kms_request_redraw(void) { luna_kms.event_seq++; }
static void luna_kms_noop(void) { /* no window manager to iconify/maximize to */ }
static void luna_kms_begin_resize(int edge) { (void)edge; }
static void luna_kms_set_title(const char* title) { (void)title; }
static float luna_kms_scale(void) { return 1.0f; }
static int  luna_kms_is_fullscreen(void) { return 1; }
static void luna_kms_set_fullscreen(int enable) { (void)enable; }
static int  luna_kms_decoration_mode(void) { return LUNA_DECORATION_CLIENT; }

static void luna_kms_get_cursor_pos(double* x, double* y) {
    if (x) *x = luna_kms.mouse_x;
    if (y) *y = luna_kms.mouse_y;
}

static int luna_kms_get_mouse_button(int button) {
    if (button < 0 || button >= (int)(sizeof(luna_kms.buttons)/sizeof(luna_kms.buttons[0])))
        return LUNA_RELEASE;
    return luna_kms.buttons[button];
}

static int luna_kms_get_key(int key) {
    /* xkb tracks the physical keyboard, but Luna keys are not keycodes; a
     * console host has no portable reverse mapping.  Report "not pressed"
     * rather than guessing. */
    (void)key;
    return LUNA_RELEASE;
}

static void luna_kms_get_window_rect(int* x, int* y, int* w, int* h) {
    if (x) *x = 0;
    if (y) *y = 0;
    if (w) *w = luna_kms.width;
    if (h) *h = luna_kms.height;
}

/* ── Built-in cursor glyphs ────────────────────────────────────────────────
 * The host owns the cursor plane, so it must be able to draw a pointer on its
 * own.  An embedder with a cursor theme supplies config.cursor_paint instead
 * (luna-shell does), and these are then only the fallback.
 *   '.' transparent, 'X' outline (black), 'O' fill (white)
 */
#define LUNA_KMS_ART_W 12
#define LUNA_KMS_ART_H 20
static const char* const luna_kms_arrow_art[LUNA_KMS_ART_H] = {
    "X...........",
    "XX..........",
    "XOX.........",
    "XOOX........",
    "XOOOX.......",
    "XOOOOX......",
    "XOOOOOX.....",
    "XOOOOOOX....",
    "XOOOOOOOX...",
    "XOOOOOOOOX..",
    "XOOOOOOOOOX.",
    "XOOOOOOXXXXX",
    "XOOOXOOX....",
    "XOOXXOOX....",
    "XOX..XOOX...",
    "XX...XOOX...",
    "X.....XOOX..",
    "......XOOX..",
    ".......XX...",
    "............",
};
static const char* const luna_kms_hand_art[LUNA_KMS_ART_H] = {
    "....XX......",
    "...XOOX.....",
    "...XOOX.....",
    "...XOOX.....",
    "...XOOX.....",
    "...XOOXXX...",
    "...XOOXOOXX.",
    "...XOOXOOXOX",
    "XX.XOOXOOXOX",
    "XOXXOOOOOOOX",
    "XOOXOOOOOOOX",
    ".XOOOOOOOOOX",
    ".XOOOOOOOOX.",
    "..XOOOOOOOX.",
    "..XOOOOOOOX.",
    "...XOOOOOX..",
    "...XOOOOOX..",
    "...XOOOOOX..",
    "....XXXXX...",
    "............",
};

static void luna_kms_plot(uint32_t* dst, int stride_px, int w, int h,
                          int x, int y, uint32_t argb) {
    if (x < 0 || y < 0 || x >= w || y >= h) return;
    dst[y * stride_px + x] = argb;
}

static void luna_kms_draw_art(uint32_t* dst, int stride_px, int w, int h,
                              const char* const* art, int ox, int oy) {
    for (int y = 0; y < LUNA_KMS_ART_H; ++y)
        for (int x = 0; x < LUNA_KMS_ART_W; ++x) {
            char c = art[y][x];
            if (c == 'X')      luna_kms_plot(dst, stride_px, w, h, ox + x, oy + y, 0xff000000u);
            else if (c == 'O') luna_kms_plot(dst, stride_px, w, h, ox + x, oy + y, 0xffffffffu);
        }
}

/* A bar with an outline: the shared primitive behind the I-beam, the
 * crosshair and the two resize cursors. */
static void luna_kms_draw_bar(uint32_t* dst, int stride_px, int w, int h,
                              int x0, int y0, int x1, int y1) {
    for (int y = y0 - 1; y <= y1 + 1; ++y)
        for (int x = x0 - 1; x <= x1 + 1; ++x) {
            int inside = x >= x0 && x <= x1 && y >= y0 && y <= y1;
            luna_kms_plot(dst, stride_px, w, h, x, y, inside ? 0xffffffffu : 0xff000000u);
        }
    /* Repaint the interior so the outline pass cannot overwrite it. */
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
            luna_kms_plot(dst, stride_px, w, h, x, y, 0xffffffffu);
}

/* Types match luna-ui: 0 arrow, 1 pointer, 2 text, 3 crosshair, 4 ew, 5 ns. */
static void luna_kms_paint_builtin(uint32_t* dst, int w, int h, int stride_px,
                                   int cursor_type, int* hot_x, int* hot_y) {
    int cx = w / 4, cy = h / 4;   /* centre of the glyph area we actually use */
    *hot_x = 0; *hot_y = 0;

    switch (cursor_type) {
    case 1:
        luna_kms_draw_art(dst, stride_px, w, h, luna_kms_hand_art, 0, 0);
        *hot_x = 4; *hot_y = 0;
        break;
    case 2:
        luna_kms_draw_bar(dst, stride_px, w, h, cx - 1, cy - 9, cx, cy + 9);
        luna_kms_draw_bar(dst, stride_px, w, h, cx - 4, cy - 10, cx + 3, cy - 9);
        luna_kms_draw_bar(dst, stride_px, w, h, cx - 4, cy + 9, cx + 3, cy + 10);
        *hot_x = cx; *hot_y = cy;
        break;
    case 3:
        luna_kms_draw_bar(dst, stride_px, w, h, cx - 10, cy, cx + 10, cy);
        luna_kms_draw_bar(dst, stride_px, w, h, cx, cy - 10, cx, cy + 10);
        *hot_x = cx; *hot_y = cy;
        break;
    case 4:
        luna_kms_draw_bar(dst, stride_px, w, h, cx - 10, cy - 1, cx + 10, cy + 1);
        for (int i = 0; i < 6; ++i) {
            luna_kms_draw_bar(dst, stride_px, w, h, cx - 10 + i, cy - i, cx - 10 + i, cy + i);
            luna_kms_draw_bar(dst, stride_px, w, h, cx + 10 - i, cy - i, cx + 10 - i, cy + i);
        }
        *hot_x = cx; *hot_y = cy;
        break;
    case 5:
        luna_kms_draw_bar(dst, stride_px, w, h, cx - 1, cy - 10, cx + 1, cy + 10);
        for (int i = 0; i < 6; ++i) {
            luna_kms_draw_bar(dst, stride_px, w, h, cx - i, cy - 10 + i, cx + i, cy - 10 + i);
            luna_kms_draw_bar(dst, stride_px, w, h, cx - i, cy + 10 - i, cx + i, cy + 10 - i);
        }
        *hot_x = cx; *hot_y = cy;
        break;
    default:
        luna_kms_draw_art(dst, stride_px, w, h, luna_kms_arrow_art, 0, 0);
        *hot_x = 0; *hot_y = 0;
        break;
    }
}

static void luna_kms_cursor_paint(int cursor_type) {
    int stride, painted;
    uint32_t* px;
    if (!luna_kms.cursor_map) return;

    stride = (int)(luna_kms.cursor_pitch / 4);
    px = luna_kms.cursor_map;
    memset(px, 0, (size_t)luna_kms.cursor_size);
    luna_kms.cursor_type = cursor_type;
    luna_kms.cursor_hot_x = 0;
    luna_kms.cursor_hot_y = 0;

    painted = luna_kms.config.cursor_paint &&
              luna_kms.config.cursor_paint(px, LUNA_KMS_CURSOR_SIZE,
                                           LUNA_KMS_CURSOR_SIZE, stride,
                                           cursor_type, &luna_kms.cursor_hot_x,
                                           &luna_kms.cursor_hot_y,
                                           luna_kms.config.userdata);
    if (!painted) {
        memset(px, 0, (size_t)luna_kms.cursor_size);
        luna_kms_paint_builtin(px, LUNA_KMS_CURSOR_SIZE, LUNA_KMS_CURSOR_SIZE,
                               stride, cursor_type,
                               &luna_kms.cursor_hot_x, &luna_kms.cursor_hot_y);
    }

    if (luna_kms.cursor_shown) {
        if (drmModeSetCursor2(luna_kms.fd, luna_kms.crtc_id, luna_kms.cursor_handle,
                              luna_kms.cursor_w, luna_kms.cursor_h,
                              luna_kms.cursor_hot_x, luna_kms.cursor_hot_y) != 0)
            drmModeSetCursor(luna_kms.fd, luna_kms.crtc_id, luna_kms.cursor_handle,
                             luna_kms.cursor_w, luna_kms.cursor_h);
        luna_kms_cursor_move();
    }
}

static void luna_kms_cursor_move(void) {
    if (!luna_kms.cursor_shown) return;
    drmModeMoveCursor(luna_kms.fd, luna_kms.crtc_id,
                      (int)luna_kms.mouse_x - luna_kms.cursor_hot_x,
                      (int)luna_kms.mouse_y - luna_kms.cursor_hot_y);
}

static int luna_kms_cursor_init(void) {
    struct drm_mode_create_dumb creq;
    struct drm_mode_map_dumb mreq;
    struct drm_mode_destroy_dumb dreq;

    memset(&creq, 0, sizeof(creq));
    creq.width  = LUNA_KMS_CURSOR_SIZE;
    creq.height = LUNA_KMS_CURSOR_SIZE;
    creq.bpp    = 32;
    if (drmIoctl(luna_kms.fd, DRM_IOCTL_MODE_CREATE_DUMB, &creq) != 0) {
        fprintf(stderr, "[luna-ui/kms] CREATE_DUMB(cursor) failed: %s\n", strerror(errno));
        return 0;
    }
    luna_kms.cursor_handle = creq.handle;
    luna_kms.cursor_pitch  = creq.pitch;
    luna_kms.cursor_size   = creq.size;
    luna_kms.cursor_w      = LUNA_KMS_CURSOR_SIZE;
    luna_kms.cursor_h      = LUNA_KMS_CURSOR_SIZE;

    memset(&mreq, 0, sizeof(mreq));
    mreq.handle = creq.handle;
    memset(&dreq, 0, sizeof(dreq));
    dreq.handle = creq.handle;
    if (drmIoctl(luna_kms.fd, DRM_IOCTL_MODE_MAP_DUMB, &mreq) != 0) {
        fprintf(stderr, "[luna-ui/kms] MAP_DUMB(cursor) failed: %s\n", strerror(errno));
        drmIoctl(luna_kms.fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
        luna_kms.cursor_handle = 0;
        return 0;
    }
    luna_kms.cursor_map = mmap(NULL, (size_t)creq.size, PROT_READ | PROT_WRITE,
                               MAP_SHARED, luna_kms.fd, (off_t)mreq.offset);
    if (luna_kms.cursor_map == MAP_FAILED) {
        fprintf(stderr, "[luna-ui/kms] mmap(cursor) failed: %s\n", strerror(errno));
        luna_kms.cursor_map = NULL;
        drmIoctl(luna_kms.fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
        luna_kms.cursor_handle = 0;
        return 0;
    }
    luna_kms.cursor_ok = 1;
    luna_kms_cursor_paint(0);
    return 1;
}

static void luna_kms_cursor_show(void) {
    if (!luna_kms.cursor_ok || luna_kms.cursor_shown) return;
    if (drmModeSetCursor2(luna_kms.fd, luna_kms.crtc_id, luna_kms.cursor_handle,
                          luna_kms.cursor_w, luna_kms.cursor_h,
                          luna_kms.cursor_hot_x, luna_kms.cursor_hot_y) != 0) {
        if (drmModeSetCursor(luna_kms.fd, luna_kms.crtc_id, luna_kms.cursor_handle,
                             luna_kms.cursor_w, luna_kms.cursor_h) != 0) {
            fprintf(stderr, "[luna-ui/kms] drmModeSetCursor failed: %s"
                            " (no hardware cursor plane?)\n", strerror(errno));
            return;
        }
    }
    luna_kms.cursor_shown = 1;
    luna_kms_cursor_move();
}

static void luna_kms_cursor_fini(void) {
    if (luna_kms.cursor_shown) {
        drmModeSetCursor(luna_kms.fd, luna_kms.crtc_id, 0, 0, 0);
        luna_kms.cursor_shown = 0;
    }
    if (luna_kms.cursor_map) {
        munmap(luna_kms.cursor_map, (size_t)luna_kms.cursor_size);
        luna_kms.cursor_map = NULL;
    }
    if (luna_kms.cursor_handle) {
        struct drm_mode_destroy_dumb dreq;
        memset(&dreq, 0, sizeof(dreq));
        dreq.handle = luna_kms.cursor_handle;
        drmIoctl(luna_kms.fd, DRM_IOCTL_MODE_DESTROY_DUMB, &dreq);
        luna_kms.cursor_handle = 0;
    }
    luna_kms.cursor_ok = 0;
}

static void luna_kms_set_cursor(int cursor_type) {
    if (!luna_kms.cursor_ok) return;
    if (cursor_type == luna_kms.cursor_type && luna_kms.cursor_shown) return;
    luna_kms_cursor_paint(cursor_type);
}

/* An embedder with an animated theme re-pushes the same type when its frame
 * advances, which the identity check above would drop. */
void luna_host_kms_refresh_cursor(int cursor_type) {
    if (luna_kms.cursor_ok) luna_kms_cursor_paint(cursor_type);
}

/* ── DRM / GBM / EGL bring-up ──────────────────────────────────────────── */

static int luna_kms_open_device(void) {
    static const char* const nodes[] = {
        "/dev/dri/card0", "/dev/dri/card1", "/dev/dri/card2", NULL
    };
    const char* forced = getenv("LUNA_DRM_DEVICE");
    if (forced && *forced) {
        int fd = open(forced, O_RDWR | O_CLOEXEC);
        drmModeRes* res = fd >= 0 ? drmModeGetResources(fd) : NULL;
        if (res) { drmModeFreeResources(res); return fd; }
        if (fd >= 0) close(fd);
        fprintf(stderr, "[luna-ui/kms] LUNA_DRM_DEVICE=%s is not usable\n", forced);
    }
    for (int i = 0; nodes[i]; ++i) {
        int fd = open(nodes[i], O_RDWR | O_CLOEXEC);
        drmModeRes* res;
        if (fd < 0) continue;
        res = drmModeGetResources(fd);
        if (res) { drmModeFreeResources(res); return fd; }
        close(fd);
    }
    return -1;
}

static int luna_kms_find_display(void) {
    drmModeRes* res = drmModeGetResources(luna_kms.fd);
    drmModeConnector* conn = NULL;
    drmModeModeInfo* best;
    drmModeEncoder* enc;
    uint32_t crtc_id = 0;

    if (!res) { fprintf(stderr, "[luna-ui/kms] drmModeGetResources failed\n"); return 0; }
    for (int i = 0; i < res->count_connectors; ++i) {
        drmModeConnector* c = drmModeGetConnector(luna_kms.fd, res->connectors[i]);
        if (c && c->connection == DRM_MODE_CONNECTED && c->count_modes > 0) { conn = c; break; }
        if (c) drmModeFreeConnector(c);
    }
    if (!conn) {
        fprintf(stderr, "[luna-ui/kms] no connected display on this DRM device\n");
        drmModeFreeResources(res);
        return 0;
    }

    best = &conn->modes[0];
    for (int i = 0; i < conn->count_modes; ++i)
        if (conn->modes[i].type & DRM_MODE_TYPE_PREFERRED) { best = &conn->modes[i]; break; }
    luna_kms.mode    = *best;
    luna_kms.width   = best->hdisplay;
    luna_kms.height  = best->vdisplay;
    luna_kms.conn_id = conn->connector_id;

    enc = conn->encoder_id ? drmModeGetEncoder(luna_kms.fd, conn->encoder_id) : NULL;
    if (enc && enc->crtc_id) {
        crtc_id = enc->crtc_id;
    } else {
        for (int i = 0; i < conn->count_encoders && !crtc_id; ++i) {
            drmModeEncoder* e = drmModeGetEncoder(luna_kms.fd, conn->encoders[i]);
            if (!e) continue;
            for (int j = 0; j < res->count_crtcs; ++j)
                if (e->possible_crtcs & (1u << j)) { crtc_id = res->crtcs[j]; break; }
            drmModeFreeEncoder(e);
        }
    }
    if (enc) drmModeFreeEncoder(enc);
    if (!crtc_id) {
        fprintf(stderr, "[luna-ui/kms] no usable CRTC for the connected display\n");
        drmModeFreeConnector(conn);
        drmModeFreeResources(res);
        return 0;
    }
    luna_kms.crtc_id    = crtc_id;
    luna_kms.saved_crtc = drmModeGetCrtc(luna_kms.fd, luna_kms.crtc_id);

    drmModeFreeConnector(conn);
    drmModeFreeResources(res);
    return 1;
}

static int luna_kms_init_gbm_egl(void) {
    static const int versions[][2] = { {4,5}, {4,1}, {3,3} };
    static const EGLint cfg_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE
    };
    PFNEGLGETPLATFORMDISPLAYEXTPROC get_plat_dpy;
    EGLConfig cfg;
    EGLint n_cfg = 0, major, minor;

    luna_kms.gbm_dev = gbm_create_device(luna_kms.fd);
    if (!luna_kms.gbm_dev) {
        fprintf(stderr, "[luna-ui/kms] gbm_create_device failed\n"); return 0;
    }
    luna_kms.gbm_surf = gbm_surface_create(luna_kms.gbm_dev,
                                           luna_kms.width, luna_kms.height,
                                           GBM_FORMAT_XRGB8888,
                                           GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);
    if (!luna_kms.gbm_surf) {
        fprintf(stderr, "[luna-ui/kms] gbm_surface_create failed\n"); return 0;
    }

    get_plat_dpy = (PFNEGLGETPLATFORMDISPLAYEXTPROC)eglGetProcAddress("eglGetPlatformDisplayEXT");
    luna_kms.dpy = get_plat_dpy
        ? get_plat_dpy(EGL_PLATFORM_GBM_KHR, luna_kms.gbm_dev, NULL)
        : eglGetDisplay((EGLNativeDisplayType)luna_kms.gbm_dev);
    if (luna_kms.dpy == EGL_NO_DISPLAY) {
        fprintf(stderr, "[luna-ui/kms] eglGetDisplay failed\n"); return 0;
    }
    if (!eglInitialize(luna_kms.dpy, &major, &minor)) {
        fprintf(stderr, "[luna-ui/kms] eglInitialize failed\n"); return 0;
    }
    if (!eglBindAPI(EGL_OPENGL_API)) {
        fprintf(stderr, "[luna-ui/kms] eglBindAPI(EGL_OPENGL_API) failed\n"); return 0;
    }
    if (!eglChooseConfig(luna_kms.dpy, cfg_attribs, &cfg, 1, &n_cfg) || n_cfg < 1) {
        fprintf(stderr, "[luna-ui/kms] eglChooseConfig failed\n"); return 0;
    }

    /* Try GL 4.5 first (native glCreateVertexArrays), then 4.1 / 3.3 core. */
    luna_kms.ctx = EGL_NO_CONTEXT;
    for (int i = 0; i < (int)(sizeof(versions)/sizeof(versions[0])); ++i) {
        const EGLint ctx_attribs[] = {
            EGL_CONTEXT_MAJOR_VERSION, versions[i][0],
            EGL_CONTEXT_MINOR_VERSION, versions[i][1],
            EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
            EGL_NONE
        };
        luna_kms.ctx = eglCreateContext(luna_kms.dpy, cfg, EGL_NO_CONTEXT, ctx_attribs);
        if (luna_kms.ctx != EGL_NO_CONTEXT) {
            fprintf(stderr, "[luna-ui/kms] OpenGL %d.%d context\n",
                    versions[i][0], versions[i][1]);
            break;
        }
    }
    if (luna_kms.ctx == EGL_NO_CONTEXT) {
        fprintf(stderr, "[luna-ui/kms] eglCreateContext failed\n"); return 0;
    }

    luna_kms.surf = eglCreateWindowSurface(luna_kms.dpy, cfg,
                                           (EGLNativeWindowType)luna_kms.gbm_surf, NULL);
    if (luna_kms.surf == EGL_NO_SURFACE) {
        fprintf(stderr, "[luna-ui/kms] eglCreateWindowSurface failed\n"); return 0;
    }
    if (!eglMakeCurrent(luna_kms.dpy, luna_kms.surf, luna_kms.surf, luna_kms.ctx)) {
        fprintf(stderr, "[luna-ui/kms] eglMakeCurrent failed\n"); return 0;
    }
    /* There is no compositor to pace us: drmModePageFlip() below is the one
     * and only vblank throttle.  Asking EGL to wait as well serializes the
     * render at eglSwapBuffers and then waits for a second vblank at the page
     * flip, which shows up as a very regular 30 Hz hitch on the console. */
    eglSwapInterval(luna_kms.dpy, 0);
    return 1;
}

/* ── libinput / xkbcommon ──────────────────────────────────────────────── */

static int luna_kms_li_open(const char* path, int flags, void* user_data) {
    int fd;
    (void)user_data;
    fd = open(path, flags);
    return fd < 0 ? -errno : fd;
}
static void luna_kms_li_close(int fd, void* user_data) { (void)user_data; close(fd); }
static const struct libinput_interface luna_kms_li_iface = {
    luna_kms_li_open, luna_kms_li_close
};

static int luna_kms_init_input(void) {
    const char* seat = getenv("LUNA_SEAT");
    struct xkb_rule_names rules;

    luna_kms.udev = udev_new();
    if (!luna_kms.udev) return 0;
    luna_kms.li = libinput_udev_create_context(&luna_kms_li_iface, NULL, luna_kms.udev);
    if (!luna_kms.li) return 0;
    if (!seat) seat = "seat0";
    if (libinput_udev_assign_seat(luna_kms.li, seat) != 0) {
        fprintf(stderr, "[luna-ui/kms] libinput_udev_assign_seat(%s) failed"
                        " (needs a running seatd/logind session, or CAP_SYS_ADMIN)\n", seat);
        return 0;
    }

    luna_kms.xkb_ctx = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    memset(&rules, 0, sizeof(rules));
    rules.rules   = getenv("XKB_DEFAULT_RULES");
    rules.model   = getenv("XKB_DEFAULT_MODEL");
    rules.layout  = getenv("XKB_DEFAULT_LAYOUT");
    rules.variant = getenv("XKB_DEFAULT_VARIANT");
    rules.options = getenv("XKB_DEFAULT_OPTIONS");
    luna_kms.xkb_keymap = xkb_keymap_new_from_names(luna_kms.xkb_ctx, &rules,
                                                    XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!luna_kms.xkb_keymap) /* fall back to a bare default (US) layout */
        luna_kms.xkb_keymap = xkb_keymap_new_from_names(luna_kms.xkb_ctx, NULL,
                                                        XKB_KEYMAP_COMPILE_NO_FLAGS);
    if (!luna_kms.xkb_keymap) return 0;
    luna_kms.xkb_state = xkb_state_new(luna_kms.xkb_keymap);
    return luna_kms.xkb_state != NULL;
}

static int luna_kms_mod_bits(void) {
    struct xkb_state* st = luna_kms.xkb_state;
    int mods = 0;
    if (!st) return 0;
    if (xkb_state_mod_name_is_active(st, XKB_MOD_NAME_SHIFT, XKB_STATE_MODS_EFFECTIVE) > 0) mods |= LUNA_MOD_SHIFT;
    if (xkb_state_mod_name_is_active(st, XKB_MOD_NAME_CTRL,  XKB_STATE_MODS_EFFECTIVE) > 0) mods |= LUNA_MOD_CONTROL;
    if (xkb_state_mod_name_is_active(st, XKB_MOD_NAME_ALT,   XKB_STATE_MODS_EFFECTIVE) > 0) mods |= LUNA_MOD_ALT;
    if (xkb_state_mod_name_is_active(st, XKB_MOD_NAME_LOGO,  XKB_STATE_MODS_EFFECTIVE) > 0) mods |= LUNA_MOD_SUPER;
    return mods;
}

static void luna_kms_emit_motion(void) {
    luna_kms_cursor_move();
    if (!luna_kms.config.on_mouse_move ||
        !luna_kms.config.on_mouse_move(luna_kms.mouse_x, luna_kms.mouse_y,
                                       luna_kms.config.userdata))
        luna_mouse_move(luna_kms.mouse_x, luna_kms.mouse_y);
}

static void luna_kms_process_input(void) {
    struct libinput_event* ev;
    if (!luna_kms.li) return;
    if (libinput_dispatch(luna_kms.li) != 0) return;

    while ((ev = libinput_get_event(luna_kms.li))) {
        luna_kms.event_seq++;
        switch (libinput_event_get_type(ev)) {
        case LIBINPUT_EVENT_POINTER_MOTION: {
            struct libinput_event_pointer* p = libinput_event_get_pointer_event(ev);
            luna_kms.mouse_x += libinput_event_pointer_get_dx(p);
            luna_kms.mouse_y += libinput_event_pointer_get_dy(p);
            if (luna_kms.mouse_x < 0) luna_kms.mouse_x = 0;
            if (luna_kms.mouse_y < 0) luna_kms.mouse_y = 0;
            if (luna_kms.mouse_x > luna_kms.width)  luna_kms.mouse_x = luna_kms.width;
            if (luna_kms.mouse_y > luna_kms.height) luna_kms.mouse_y = luna_kms.height;
            luna_kms_emit_motion();
            break;
        }
        case LIBINPUT_EVENT_POINTER_MOTION_ABSOLUTE: {
            struct libinput_event_pointer* p = libinput_event_get_pointer_event(ev);
            luna_kms.mouse_x = libinput_event_pointer_get_absolute_x_transformed(p, luna_kms.width);
            luna_kms.mouse_y = libinput_event_pointer_get_absolute_y_transformed(p, luna_kms.height);
            luna_kms_emit_motion();
            break;
        }
        case LIBINPUT_EVENT_POINTER_BUTTON: {
            struct libinput_event_pointer* p = libinput_event_get_pointer_event(ev);
            int btn = luna_button_from_evdev(libinput_event_pointer_get_button(p));
            int action, mods;
            if (btn < 0) break;
            action = libinput_event_pointer_get_button_state(p) == LIBINPUT_BUTTON_STATE_PRESSED
                   ? LUNA_PRESS : LUNA_RELEASE;
            mods = luna_kms_mod_bits();
            if (btn >= 0 && btn < (int)(sizeof(luna_kms.buttons)/sizeof(luna_kms.buttons[0])))
                luna_kms.buttons[btn] = action;
            if (!luna_kms.config.on_mouse_button ||
                !luna_kms.config.on_mouse_button(btn, action, mods,
                                                 luna_kms.mouse_x, luna_kms.mouse_y,
                                                 luna_kms.config.userdata))
                luna_mouse_button(btn, action, mods, luna_kms.mouse_x, luna_kms.mouse_y);
            break;
        }
        case LIBINPUT_EVENT_POINTER_AXIS: {
            struct libinput_event_pointer* p = libinput_event_get_pointer_event(ev);
            double yv = 0.0, xv = 0.0;
            if (libinput_event_pointer_has_axis(p, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL))
                yv = libinput_event_pointer_get_axis_value(p, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);
            if (libinput_event_pointer_has_axis(p, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL))
                xv = libinput_event_pointer_get_axis_value(p, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL);
            xv = -xv / 10.0;
            yv = -yv / 10.0;
            if (!luna_kms.config.on_scroll ||
                !luna_kms.config.on_scroll(xv, yv, luna_kms.config.userdata))
                luna_scroll(xv, yv);
            break;
        }
        case LIBINPUT_EVENT_TOUCH_DOWN:
        case LIBINPUT_EVENT_TOUCH_MOTION:
        case LIBINPUT_EVENT_TOUCH_UP:
        case LIBINPUT_EVENT_TOUCH_CANCEL: {
            struct libinput_event_touch* t = libinput_event_get_touch_event(ev);
            struct libinput_device* device = libinput_event_get_device(ev);
            int type = libinput_event_get_type(ev);
            int slot = libinput_event_touch_get_seat_slot(t);
            LunaTouchEvent touch;
            memset(&touch, 0, sizeof(touch));
            touch.id = (int64_t)(((uint64_t)(uintptr_t)device << 8) ^
                                 (uint32_t)(slot < 0 ? 0 : slot));
            touch.phase = type == LIBINPUT_EVENT_TOUCH_DOWN   ? LUNA_TOUCH_DOWN :
                          type == LIBINPUT_EVENT_TOUCH_MOTION ? LUNA_TOUCH_MOVE :
                          type == LIBINPUT_EVENT_TOUCH_UP     ? LUNA_TOUCH_UP : LUNA_TOUCH_CANCEL;
            touch.tool = LUNA_TOUCH_TOOL_FINGER;
            touch.x = libinput_event_touch_get_x_transformed(t, luna_kms.width);
            touch.y = libinput_event_touch_get_y_transformed(t, luna_kms.height);
            touch.pressure = (touch.phase == LUNA_TOUCH_UP ||
                              touch.phase == LUNA_TOUCH_CANCEL) ? 0.0f : 1.0f;
            if (!luna_kms.config.on_touch ||
                !luna_kms.config.on_touch(&touch, luna_kms.config.userdata))
                luna_touch(&touch);
            break;
        }
        case LIBINPUT_EVENT_TABLET_TOOL_TIP:
        case LIBINPUT_EVENT_TABLET_TOOL_AXIS: {
            struct libinput_event_tablet_tool* t = libinput_event_get_tablet_tool_event(ev);
            struct libinput_tablet_tool* tool = libinput_event_tablet_tool_get_tool(t);
            enum libinput_tablet_tool_type tool_type = libinput_tablet_tool_get_type(tool);
            int type = libinput_event_get_type(ev);
            LunaTouchEvent touch;
            memset(&touch, 0, sizeof(touch));
            touch.id = (int64_t)(uintptr_t)tool;
            touch.phase = type == LIBINPUT_EVENT_TABLET_TOOL_AXIS ? LUNA_TOUCH_MOVE :
                (libinput_event_tablet_tool_get_tip_state(t) == LIBINPUT_TABLET_TOOL_TIP_DOWN
                     ? LUNA_TOUCH_DOWN : LUNA_TOUCH_UP);
            touch.tool = tool_type == LIBINPUT_TABLET_TOOL_TYPE_ERASER
                             ? LUNA_TOUCH_TOOL_ERASER : LUNA_TOUCH_TOOL_STYLUS;
            touch.x = libinput_event_tablet_tool_get_x_transformed(t, luna_kms.width);
            touch.y = libinput_event_tablet_tool_get_y_transformed(t, luna_kms.height);
            touch.pressure = touch.phase == LUNA_TOUCH_UP ? 0.0f :
                             (float)libinput_event_tablet_tool_get_pressure(t);
            touch.tilt_x = (float)libinput_event_tablet_tool_get_tilt_x(t);
            touch.tilt_y = (float)libinput_event_tablet_tool_get_tilt_y(t);
            if (!luna_kms.config.on_touch ||
                !luna_kms.config.on_touch(&touch, luna_kms.config.userdata))
                luna_touch(&touch);
            break;
        }
        case LIBINPUT_EVENT_KEYBOARD_KEY: {
            struct libinput_event_keyboard* k;
            uint32_t code, cp;
            int pressed, mods, key, action;
            if (!luna_kms.xkb_state) break;
            k = libinput_event_get_keyboard_event(ev);
            code = libinput_event_keyboard_get_key(k);
            pressed = libinput_event_keyboard_get_key_state(k) == LIBINPUT_KEY_STATE_PRESSED;
            /* xkb keycodes are evdev codes biased by 8; the Luna key code comes
             * from the raw evdev code so it stays layout-independent, while xkb
             * supplies the modifiers and the committed character below. */
            xkb_state_update_key(luna_kms.xkb_state, code + 8,
                                 pressed ? XKB_KEY_DOWN : XKB_KEY_UP);
            mods = luna_kms_mod_bits();
            key  = luna_key_from_evdev(code);
            action = pressed ? LUNA_PRESS : LUNA_RELEASE;
            if (key != LUNA_KEY_UNKNOWN) {
                if (!luna_kms.config.on_key ||
                    !luna_kms.config.on_key(key, (int)code, action, mods,
                                            luna_kms.config.userdata))
                    luna_key(key, (int)code, action, mods);
            }
            if (pressed) {
                cp = xkb_state_key_get_utf32(luna_kms.xkb_state, code + 8);
                if (cp >= 32 && cp != 127) {
                    if (!luna_kms.config.on_char ||
                        !luna_kms.config.on_char(cp, luna_kms.config.userdata))
                        luna_char(cp);
                }
            }
            break;
        }
        default: break;
        }
        libinput_event_destroy(ev);
    }
}

/* ── Scanout ───────────────────────────────────────────────────────────── */

static void luna_kms_fb_destroy_cb(struct gbm_bo* bo, void* data) {
    uint32_t* fb_id = data;
    (void)bo;
    drmModeRmFB(luna_kms.fd, *fb_id);
    free(fb_id);
}

static uint32_t luna_kms_fb_for_bo(struct gbm_bo* bo) {
    uint32_t* fb_id = gbm_bo_get_user_data(bo);
    uint32_t handle, stride;
    if (fb_id) return *fb_id;

    handle = gbm_bo_get_handle(bo).u32;
    stride = gbm_bo_get_stride(bo);
    fb_id = malloc(sizeof(uint32_t));
    if (!fb_id) return 0;
    if (drmModeAddFB(luna_kms.fd, luna_kms.width, luna_kms.height, 24, 32,
                     stride, handle, fb_id)) {
        fprintf(stderr, "[luna-ui/kms] drmModeAddFB failed: %s\n", strerror(errno));
        free(fb_id);
        return 0;
    }
    gbm_bo_set_user_data(bo, fb_id, luna_kms_fb_destroy_cb);
    return *fb_id;
}

static void luna_kms_page_flip_handler(int fd, unsigned int frame,
                                       unsigned int sec, unsigned int usec, void* data) {
    (void)fd; (void)frame; (void)sec; (void)usec; (void)data;
    luna_kms.flip_pending = 0;
}

static void luna_kms_swap_buffers(void) {
    struct gbm_bo* bo;
    uint32_t fb_id;

    if (!eglSwapBuffers(luna_kms.dpy, luna_kms.surf)) {
        fprintf(stderr, "[luna-ui/kms] eglSwapBuffers failed (EGL 0x%x)\n", eglGetError());
        return;
    }
    bo = gbm_surface_lock_front_buffer(luna_kms.gbm_surf);
    if (!bo) return;
    fb_id = luna_kms_fb_for_bo(bo);
    if (!fb_id) {
        gbm_surface_release_buffer(luna_kms.gbm_surf, bo);
        return;
    }

    if (!luna_kms.prev_bo) {
        /* First frame: set the mode directly instead of page-flipping. */
        if (drmModeSetCrtc(luna_kms.fd, luna_kms.crtc_id, fb_id, 0, 0,
                           &luna_kms.conn_id, 1, &luna_kms.mode) != 0) {
            fprintf(stderr, "[luna-ui/kms] initial modeset failed: %s\n", strerror(errno));
            gbm_surface_release_buffer(luna_kms.gbm_surf, bo);
            return;
        }
        /* The cursor plane needs an active CRTC — enable it after the modeset. */
        luna_kms_cursor_show();
    } else {
        int flip_queued;
        luna_kms.flip_pending = 1;
        flip_queued = drmModePageFlip(luna_kms.fd, luna_kms.crtc_id, fb_id,
                                      DRM_MODE_PAGE_FLIP_EVENT, NULL) == 0;
        if (flip_queued) {
            while (luna_kms.flip_pending) {
                struct pollfd pfds[2];
                int nfd = 1, pr;
                pfds[0].fd = luna_kms.fd;
                pfds[0].events = POLLIN;
                pfds[0].revents = 0;
                /* Keep draining libinput while waiting for the flip so the
                 * pointer and keyboard never freeze between frames. */
                if (luna_kms.li) {
                    pfds[1].fd = libinput_get_fd(luna_kms.li);
                    pfds[1].events = POLLIN;
                    pfds[1].revents = 0;
                    nfd = 2;
                }
                pr = poll(pfds, (nfds_t)nfd, -1);
                if (pr < 0) { if (errno == EINTR) continue; break; }
                if (nfd > 1 && (pfds[1].revents & POLLIN)) luna_kms_process_input();
                if (pfds[0].revents & POLLIN) {
                    drmEventContext evctx;
                    memset(&evctx, 0, sizeof(evctx));
                    evctx.version = 2;
                    evctx.page_flip_handler = luna_kms_page_flip_handler;
                    if (drmHandleEvent(luna_kms.fd, &evctx) != 0) break;
                }
            }
        }
        /* The old BO is still scanned out until the flip event arrives.
         * Releasing it after a failed or incomplete page flip lets GBM recycle
         * the visible buffer, which shows up as intermittent console flicker. */
        if (!flip_queued) {
            fprintf(stderr, "[luna-ui/kms] page flip failed: %s\n", strerror(errno));
            luna_kms.flip_pending = 0;
            gbm_surface_release_buffer(luna_kms.gbm_surf, bo);
            return;
        }
        if (luna_kms.flip_pending) {
            /* The kernel accepted the flip, so either BO may be active now.
             * Keep both locked and leave cleanly rather than recycling a
             * possibly scanned-out buffer after an event-channel failure. */
            fprintf(stderr, "[luna-ui/kms] page flip completion failed\n");
            luna_kms.should_close = 1;
            return;
        }
        /* The scanout BO goes straight from EGL/GBM to KMS: no CPU copy.
         * Release the previous BO only once the flip event says KMS is done
         * with it.  The cursor plane is independent of primary-plane flips, so
         * it is deliberately not re-programmed on every frame. */
        gbm_surface_release_buffer(luna_kms.gbm_surf, luna_kms.prev_bo);
    }
    luna_kms.prev_bo = bo;
}

/* ── LunaHostOps implementation ────────────────────────────────────────── */

static int luna_kms_host_start(const LunaHostConfig* user_cfg) {
    LunaPlatform platform;

    memset(&luna_kms, 0, sizeof(luna_kms));
    if (user_cfg) luna_kms.config = *user_cfg;
    luna_kms.fd = -1;
    luna_kms.dpy = EGL_NO_DISPLAY;
    luna_kms.ctx = EGL_NO_CONTEXT;
    luna_kms.surf = EGL_NO_SURFACE;
    for (int i = 0; i < (int)(sizeof(luna_kms.buttons)/sizeof(luna_kms.buttons[0])); ++i)
        luna_kms.buttons[i] = LUNA_RELEASE;

    luna_kms.fd = luna_kms_open_device();
    if (luna_kms.fd < 0) {
        fprintf(stderr, "[luna-ui/kms] no usable /dev/dri/cardN"
                        " (permissions? try the video group, or root)\n");
        return 0;
    }
    if (!luna_kms_find_display()) { close(luna_kms.fd); luna_kms.fd = -1; return 0; }
    if (!luna_kms_init_gbm_egl()) return 0;
    if (!luna_kms_init_input())
        fprintf(stderr, "[luna-ui/kms] libinput setup failed —"
                        " continuing without keyboard/mouse input\n");

    luna_kms.mouse_x = luna_kms.width  / 2.0;
    luna_kms.mouse_y = luna_kms.height / 2.0;
    if (!luna_kms_cursor_init())
        fprintf(stderr, "[luna-ui/kms] hardware cursor unavailable —"
                        " the pointer will be invisible\n");

    memset(&platform, 0, sizeof(platform));
    platform.get_time            = luna_kms_time;
    platform.get_proc            = luna_kms_get_proc;
    platform.set_cursor          = luna_kms_set_cursor;
    platform.request_close       = luna_kms_request_close;
    platform.iconify             = luna_kms_noop;
    platform.maximize_toggle     = luna_kms_noop;
    platform.request_redraw      = luna_kms_request_redraw;
    platform.get_scale           = luna_kms_scale;
    platform.begin_move          = luna_kms_noop;
    platform.begin_resize        = luna_kms_begin_resize;
    platform.set_title           = luna_kms_set_title;
    platform.get_mouse_button    = luna_kms_get_mouse_button;
    platform.get_key             = luna_kms_get_key;
    platform.get_cursor_pos      = luna_kms_get_cursor_pos;
    platform.set_fullscreen      = luna_kms_set_fullscreen;
    platform.is_fullscreen       = luna_kms_is_fullscreen;
    platform.get_window_rect     = luna_kms_get_window_rect;
    platform.get_decoration_mode = luna_kms_decoration_mode;
    platform.struct_size         = sizeof(platform);
    platform.api_version         = LUNA_UI_API_VERSION;
    if (luna_kms.config.configure_platform)
        luna_kms.config.configure_platform(&platform, luna_kms.config.userdata);
    luna_set_platform(&platform);

    luna_window_width  = (float)luna_kms.width;
    luna_window_height = (float)luna_kms.height;
    fprintf(stderr, "[luna-ui/kms] %dx%d @ %s\n",
            luna_kms.width, luna_kms.height, luna_kms.mode.name);
    return 1;
}

static void luna_kms_host_get_fb_size(int* w, int* h) {
    if (w) *w = luna_kms.width;
    if (h) *h = luna_kms.height;
}

static int luna_kms_host_should_close(void) { return luna_kms.should_close; }

static unsigned luna_kms_host_wait_events(int timeout_ms, const int* fds, int nfds) {
    struct pollfd pfds[1 + LUNA_HOST_MAX_WAIT_FDS];
    unsigned before = luna_kms.event_seq;
    unsigned mask = 0;
    int n = 0, pr, i;

    if (nfds > LUNA_HOST_MAX_WAIT_FDS) nfds = LUNA_HOST_MAX_WAIT_FDS;
    pfds[n].fd = luna_kms.li ? libinput_get_fd(luna_kms.li) : -1;
    pfds[n].events = POLLIN;
    pfds[n].revents = 0;
    n++;
    for (i = 0; i < nfds; ++i) {
        pfds[n].fd = fds[i];
        pfds[n].events = POLLIN;
        pfds[n].revents = 0;
        n++;
    }

    do { pr = poll(pfds, (nfds_t)n, timeout_ms); }
    while (pr < 0 && errno == EINTR);

    if (pr > 0) {
        for (i = 0; i < nfds; ++i)
            if (pfds[1 + i].fd >= 0 && (pfds[1 + i].revents & POLLIN))
                mask |= 1u << i;
    }
    /* Dispatch unconditionally: libinput may already hold queued events that
     * never make the descriptor readable again. */
    luna_kms_process_input();
    if (luna_kms.event_seq != before) mask |= LUNA_HOST_WAIT_INPUT;
    return mask;
}

static void luna_kms_host_terminate(void) {
    luna_kms_cursor_fini();
    if (luna_kms.li)         libinput_unref(luna_kms.li);
    if (luna_kms.udev)       udev_unref(luna_kms.udev);
    if (luna_kms.xkb_state)  xkb_state_unref(luna_kms.xkb_state);
    if (luna_kms.xkb_keymap) xkb_keymap_unref(luna_kms.xkb_keymap);
    if (luna_kms.xkb_ctx)    xkb_context_unref(luna_kms.xkb_ctx);
    if (luna_kms.prev_bo)    gbm_surface_release_buffer(luna_kms.gbm_surf, luna_kms.prev_bo);
    if (luna_kms.saved_crtc) {
        drmModeSetCrtc(luna_kms.fd, luna_kms.saved_crtc->crtc_id,
                       luna_kms.saved_crtc->buffer_id,
                       luna_kms.saved_crtc->x, luna_kms.saved_crtc->y,
                       &luna_kms.conn_id, 1, &luna_kms.saved_crtc->mode);
        drmModeFreeCrtc(luna_kms.saved_crtc);
        luna_kms.saved_crtc = NULL;
    }
    if (luna_kms.surf != EGL_NO_SURFACE) eglDestroySurface(luna_kms.dpy, luna_kms.surf);
    if (luna_kms.ctx  != EGL_NO_CONTEXT) eglDestroyContext(luna_kms.dpy, luna_kms.ctx);
    if (luna_kms.dpy  != EGL_NO_DISPLAY) eglTerminate(luna_kms.dpy);
    if (luna_kms.gbm_surf) gbm_surface_destroy(luna_kms.gbm_surf);
    if (luna_kms.gbm_dev)  gbm_device_destroy(luna_kms.gbm_dev);
    if (luna_kms.fd >= 0)  close(luna_kms.fd);
    luna_kms.fd = -1;
}

static const LunaHostOps g_luna_kms_ops = {
    "kms",
    luna_kms_host_start,
    luna_kms_host_get_fb_size,
    luna_kms_swap_buffers,
    luna_kms_host_wait_events,
    luna_kms_set_cursor,
    luna_kms_host_should_close,
    luna_kms_host_terminate,
};

const LunaHostOps* luna_host_kms(void) { return &g_luna_kms_ops; }

#if defined(LUNA_UI_PLATFORM_BODY)
/* ── Application loop, written on the ops above ─────────────────────────── */

void* luna_app_native_handle(void) { return (void*)(intptr_t)luna_kms.fd; }
void  luna_app_quit(void) { luna_kms.should_close = 1; }
void  luna_app_request_redraw(void) { luna_kms.event_seq++; }

int luna_app_run(const LunaAppConfig* user_cfg) {
    LunaAppConfig cfg;
    LunaHostConfig host;
    LunaInitConfig init;
    double previous;
    int redraw;

    memset(&cfg, 0, sizeof(cfg));
    if (user_cfg) cfg = *user_cfg;

    memset(&host, 0, sizeof(host));
    host.title           = cfg.title;
    host.vsync           = 1;             /* the page flip is the only throttle */
    host.fullscreen      = 1;
    host.on_mouse_button = cfg.on_mouse_button;
    host.on_mouse_move   = cfg.on_mouse_move;
    host.on_scroll       = cfg.on_scroll;
    host.on_key          = cfg.on_key;
    host.on_char         = cfg.on_char;
    host.on_touch        = cfg.on_touch;
    host.userdata        = cfg.userdata;
    if (!luna_kms_host_start(&host)) return 1;

    memset(&init, 0, sizeof(init));
    init.width    = (float)luna_kms.width;
    init.height   = (float)luna_kms.height;
    init.get_proc = luna_kms_get_proc;
    init.frameless = 1;                   /* nothing here draws a title bar */
    if (!luna_init(&init)) {
        luna_kms_host_terminate();
        return 1;
    }
    if (cfg.html) luna_parse_html(cfg.html);
    else if (cfg.html_path && !luna_load_html_file(cfg.html_path))
        fprintf(stderr, "luna-ui: could not load HTML: %s\n", cfg.html_path);
    if (cfg.css) luna_parse_css(cfg.css);
    else if (cfg.css_path && !luna_load_css_file(cfg.css_path))
        fprintf(stderr, "luna-ui: could not load CSS: %s\n", cfg.css_path);
    luna_inject_body_background();
    if (cfg.on_init) cfg.on_init(cfg.userdata);
    luna_wire_onclick_handlers();

    redraw = 1;
    previous = luna_kms_time();
    luna_update(previous, 0.0);
    while (!luna_kms.should_close) {
        double now, dt;
        unsigned before = luna_kms.event_seq;
        int timeout_ms;

        if (cfg.on_frame && cfg.frame_interval <= 0.0) timeout_ms = 0;
        else if (cfg.on_frame) timeout_ms = (int)(cfg.frame_interval * 1000.0);
        else timeout_ms = redraw ? 0 : -1;
        luna_kms_host_wait_events(timeout_ms, NULL, 0);
        if (luna_kms.event_seq != before) redraw = 1;
        if (luna_kms.should_close) break;

        now = luna_kms_time();
        dt = now - previous;
        previous = now;
        if (dt < 0.0 || dt > 0.25) dt = 1.0 / 60.0;
        if (cfg.on_frame) cfg.on_frame(dt, cfg.userdata);
        luna_update(now, dt);
        if (luna_needs_redraw(now, dt)) redraw = 1;
        if (!redraw) continue;

        if (cfg.custom_render) {
            if (cfg.on_render) cfg.on_render(luna_kms.width, luna_kms.height, cfg.userdata);
        } else {
            luna_render(luna_kms.width, luna_kms.height);
            if (cfg.on_render) cfg.on_render(luna_kms.width, luna_kms.height, cfg.userdata);
        }
        luna_flush_pending_screenshot();
        luna_kms_swap_buffers();
        redraw = 0;
    }

    if (cfg.on_shutdown) cfg.on_shutdown(cfg.userdata);
    luna_shutdown();
    luna_kms_host_terminate();
    return 0;
}
#endif /* LUNA_UI_PLATFORM_BODY */
#endif /* LUNA_UI_IMPLEMENTATION */
#endif /* LUNA_KMS_BODY_INCLUDED */
#endif /* LUNA_UI_PLATFORM_BODY || LUNA_UI_HOST_BODY */
