/*
 * luna_glfw.h — compact GLFW host for Luna UI desktop applications.
 *
 * Linux deliberately uses GLFW as its public portability boundary.  Native
 * shells and embedders can define LUNA_UI_NO_PLATFORM and provide LunaPlatform
 * directly, without bringing GLFW into their process.
 *
 * Two entry points, one implementation: luna_app_run() for applications, and
 * luna_host_glfw() for an embedder that owns its main loop (see LunaHostOps in
 * luna-ui.h).  The application loop is written on the same ops.
 */
#if defined(LUNA_UI_PLATFORM_PRELUDE) || defined(LUNA_UI_HOST_PRELUDE)
#ifndef LUNA_GLFW_PRELUDE_INCLUDED
#define LUNA_GLFW_PRELUDE_INCLUDED
#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <poll.h>
#define LUNA_UI_PLATFORM_GL_INCLUDED 1
#endif
#endif

#if defined(LUNA_UI_PLATFORM_BODY) || defined(LUNA_UI_HOST_BODY)
#ifndef LUNA_GLFW_BODY_INCLUDED
#define LUNA_GLFW_BODY_INCLUDED

#if defined(LUNA_UI_IMPLEMENTATION)
typedef struct LunaGlfwHost {
    GLFWwindow* window;
    GLFWcursor* cursors[6];
    LunaHostConfig config;
    /* Bumped by every input callback so wait_events() can report whether the
     * sleep ended in input rather than in a timeout. */
    unsigned event_seq;
    LunaDropCallbackFn drop_cb;
    void* drop_data;
    LunaFocusCallbackFn focus_cb;
    void* focus_data;
    LunaCloseCallbackFn close_cb;
    void* close_data;
    int redraw;
    int fullscreen;
    int restore_x, restore_y, restore_w, restore_h;
    /* Interactive move/resize.  X11/Wayland WMs expose a server-side grab, but
     * GLFW has no portable binding for it, so the host drives the drag itself
     * from pointer motion. */
    int drag_mode;              /* 0 none, 1 move, 2 resize */
    int drag_edge;
    double drag_grab_x, drag_grab_y;   /* screen coords at press */
    int drag_x, drag_y, drag_w, drag_h;/* window rect at press */
    int limit_min_w, limit_min_h, limit_max_w, limit_max_h;
} LunaGlfwHost;

static LunaGlfwHost luna_glfw;

static void* luna_glfw_get_proc(const char* name) {
    return (void*)glfwGetProcAddress(name);
}

static double luna_glfw_time(void) { return glfwGetTime(); }

static void luna_glfw_wake(void) {
    luna_glfw.redraw = 1;
    glfwPostEmptyEvent();
}

static int luna_glfw_allow_close(void) {
    return !luna_glfw.close_cb || luna_glfw.close_cb(luna_glfw.close_data);
}

static void luna_glfw_request_close(void) {
    if (luna_glfw.window && luna_glfw_allow_close())
        glfwSetWindowShouldClose(luna_glfw.window, GLFW_TRUE);
}

static void luna_glfw_iconify(void) {
    if (luna_glfw.window) glfwIconifyWindow(luna_glfw.window);
}

static void luna_glfw_maximize_toggle(void) {
    if (!luna_glfw.window) return;
    if (glfwGetWindowAttrib(luna_glfw.window, GLFW_MAXIMIZED))
        glfwRestoreWindow(luna_glfw.window);
    else
        glfwMaximizeWindow(luna_glfw.window);
}

/* Pointer position in screen coordinates.  GLFW reports the cursor relative to
 * the content area, which moves with the window during a drag, so the two are
 * always sampled together and re-derived on every motion event. */
static void luna_glfw_cursor_screen(double* sx, double* sy) {
    double cx = 0.0, cy = 0.0;
    int wx = 0, wy = 0;
    if (luna_glfw.window) {
        glfwGetCursorPos(luna_glfw.window, &cx, &cy);
        glfwGetWindowPos(luna_glfw.window, &wx, &wy);
    }
    *sx = (double)wx + cx;
    *sy = (double)wy + cy;
}

static void luna_glfw_drag_begin(int mode, int edge) {
    if (!luna_glfw.window || luna_glfw.fullscreen) return;
    if (glfwGetWindowAttrib(luna_glfw.window, GLFW_MAXIMIZED)) {
        if (mode != 1) return;              /* resizing a maximized window: no-op */
        glfwRestoreWindow(luna_glfw.window);
    }
    glfwGetWindowPos(luna_glfw.window, &luna_glfw.drag_x, &luna_glfw.drag_y);
    glfwGetWindowSize(luna_glfw.window, &luna_glfw.drag_w, &luna_glfw.drag_h);
    luna_glfw_cursor_screen(&luna_glfw.drag_grab_x, &luna_glfw.drag_grab_y);
    luna_glfw.drag_mode = mode;
    luna_glfw.drag_edge = edge;
}

static void luna_glfw_begin_move(void) { luna_glfw_drag_begin(1, 0); }

static void luna_glfw_begin_resize(int edge) { luna_glfw_drag_begin(2, edge); }

static void luna_glfw_drag_end(void) { luna_glfw.drag_mode = 0; }

static void luna_glfw_drag_update(void) {
    double sx, sy;
    int dx, dy, x, y, w, h, min_w, min_h, left, right, top, bottom;
    if (!luna_glfw.drag_mode || !luna_glfw.window) return;
    luna_glfw_cursor_screen(&sx, &sy);
    dx = (int)(sx - luna_glfw.drag_grab_x);
    dy = (int)(sy - luna_glfw.drag_grab_y);
    if (luna_glfw.drag_mode == 1) {
        glfwSetWindowPos(luna_glfw.window, luna_glfw.drag_x + dx, luna_glfw.drag_y + dy);
        return;
    }

    left   = (luna_glfw.drag_edge & LUNA_RESIZE_EDGE_LEFT)   != 0;
    right  = (luna_glfw.drag_edge & LUNA_RESIZE_EDGE_RIGHT)  != 0;
    top    = (luna_glfw.drag_edge & LUNA_RESIZE_EDGE_TOP)    != 0;
    bottom = (luna_glfw.drag_edge & LUNA_RESIZE_EDGE_BOTTOM) != 0;

    x = luna_glfw.drag_x; y = luna_glfw.drag_y;
    w = luna_glfw.drag_w; h = luna_glfw.drag_h;
    min_w = luna_glfw.limit_min_w > 0 ? luna_glfw.limit_min_w : 1;
    min_h = luna_glfw.limit_min_h > 0 ? luna_glfw.limit_min_h : 1;

    /* Clamp against the limits here as well: letting the window manager clamp
     * the size would leave the left/top edge anchored at the wrong position. */
    if (left) {
        if (luna_glfw.drag_w - dx < min_w) dx = luna_glfw.drag_w - min_w;
        if (luna_glfw.limit_max_w > 0 && luna_glfw.drag_w - dx > luna_glfw.limit_max_w)
            dx = luna_glfw.drag_w - luna_glfw.limit_max_w;
        x = luna_glfw.drag_x + dx;
        w = luna_glfw.drag_w - dx;
    } else if (right) {
        w = luna_glfw.drag_w + dx;
        if (w < min_w) w = min_w;
        if (luna_glfw.limit_max_w > 0 && w > luna_glfw.limit_max_w) w = luna_glfw.limit_max_w;
    }
    if (top) {
        if (luna_glfw.drag_h - dy < min_h) dy = luna_glfw.drag_h - min_h;
        if (luna_glfw.limit_max_h > 0 && luna_glfw.drag_h - dy > luna_glfw.limit_max_h)
            dy = luna_glfw.drag_h - luna_glfw.limit_max_h;
        y = luna_glfw.drag_y + dy;
        h = luna_glfw.drag_h - dy;
    } else if (bottom) {
        h = luna_glfw.drag_h + dy;
        if (h < min_h) h = min_h;
        if (luna_glfw.limit_max_h > 0 && h > luna_glfw.limit_max_h) h = luna_glfw.limit_max_h;
    }

    glfwSetWindowPos(luna_glfw.window, x, y);
    glfwSetWindowSize(luna_glfw.window, w, h);
}

static void luna_glfw_set_cursor(int type) {
    if (!luna_glfw.window) return;
    if (type < 0 || type >= 6) type = 0;
    glfwSetCursor(luna_glfw.window, luna_glfw.cursors[type]);
}

static void luna_glfw_set_clipboard(const char* text) {
    if (luna_glfw.window) glfwSetClipboardString(luna_glfw.window, text ? text : "");
}

static char* luna_glfw_get_clipboard(void) {
    const char* text = luna_glfw.window ? glfwGetClipboardString(luna_glfw.window) : NULL;
    size_t n;
    char* copy;
    if (!text) return NULL;
    n = strlen(text) + 1;
    copy = (char*)malloc(n);
    if (copy) memcpy(copy, text, n);
    return copy;
}

static void luna_glfw_text_input(int enabled, float x, float y, float w, float h) {
    (void)enabled; (void)x; (void)y; (void)w; (void)h;
    /* GLFW delivers committed Unicode through its character callback. */
}

static float luna_glfw_scale(void) {
    float x = 1.0f, y = 1.0f;
    if (luna_glfw.window) glfwGetWindowContentScale(luna_glfw.window, &x, &y);
    return x > 0.0f ? x : 1.0f;
}

static void luna_glfw_set_title(const char* title) {
    if (luna_glfw.window) glfwSetWindowTitle(luna_glfw.window, title ? title : "");
}

static void luna_glfw_set_drop_handler(LunaDropCallbackFn cb, void* data) {
    luna_glfw.drop_cb = cb; luna_glfw.drop_data = data;
}

static void luna_glfw_set_focus_handler(LunaFocusCallbackFn cb, void* data) {
    luna_glfw.focus_cb = cb; luna_glfw.focus_data = data;
}

static void luna_glfw_set_close_handler(LunaCloseCallbackFn cb, void* data) {
    luna_glfw.close_cb = cb; luna_glfw.close_data = data;
}

static void luna_glfw_set_size_limits(int min_w, int min_h, int max_w, int max_h) {
    luna_glfw.limit_min_w = min_w == LUNA_DONT_CARE ? 0 : min_w;
    luna_glfw.limit_min_h = min_h == LUNA_DONT_CARE ? 0 : min_h;
    luna_glfw.limit_max_w = max_w == LUNA_DONT_CARE ? 0 : max_w;
    luna_glfw.limit_max_h = max_h == LUNA_DONT_CARE ? 0 : max_h;
    if (luna_glfw.window)
        glfwSetWindowSizeLimits(luna_glfw.window, min_w, min_h, max_w, max_h);
}

static int luna_glfw_get_key(int key) {
    return luna_glfw.window ? glfwGetKey(luna_glfw.window, key) : LUNA_RELEASE;
}

static int luna_glfw_get_mouse_button(int button) {
    return luna_glfw.window ? glfwGetMouseButton(luna_glfw.window, button) : LUNA_RELEASE;
}

static void luna_glfw_get_cursor_pos(double* x, double* y) {
    double px = 0.0, py = 0.0;
    if (luna_glfw.window) glfwGetCursorPos(luna_glfw.window, &px, &py);
    if (x) *x = px;
    if (y) *y = py;
}

static void luna_glfw_set_fullscreen(int enable) {
    GLFWmonitor* monitor;
    const GLFWvidmode* mode;
    if (!luna_glfw.window || (!!enable == !!luna_glfw.fullscreen)) return;
    if (enable) {
        glfwGetWindowPos(luna_glfw.window, &luna_glfw.restore_x, &luna_glfw.restore_y);
        glfwGetWindowSize(luna_glfw.window, &luna_glfw.restore_w, &luna_glfw.restore_h);
        monitor = glfwGetWindowMonitor(luna_glfw.window);
        if (!monitor) monitor = glfwGetPrimaryMonitor();
        mode = monitor ? glfwGetVideoMode(monitor) : NULL;
        if (!monitor || !mode) return;
        glfwSetWindowMonitor(luna_glfw.window, monitor, 0, 0,
                             mode->width, mode->height, mode->refreshRate);
        luna_glfw.fullscreen = 1;
    } else {
        glfwSetWindowMonitor(luna_glfw.window, NULL,
                             luna_glfw.restore_x, luna_glfw.restore_y,
                             luna_glfw.restore_w, luna_glfw.restore_h,
                             GLFW_DONT_CARE);
        luna_glfw.fullscreen = 0;
    }
    luna_glfw_wake();
}

static int luna_glfw_is_fullscreen(void) { return luna_glfw.fullscreen; }

static void luna_glfw_get_window_rect(int* x, int* y, int* w, int* h) {
    int px = 0, py = 0, pw = 0, ph = 0;
    if (luna_glfw.window) {
        glfwGetWindowPos(luna_glfw.window, &px, &py);
        glfwGetWindowSize(luna_glfw.window, &pw, &ph);
    }
    if (x) *x = px;
    if (y) *y = py;
    if (w) *w = pw;
    if (h) *h = ph;
}

static void luna_glfw_set_window_rect(int x, int y, int w, int h) {
    int ox, oy, ow, oh;
    if (!luna_glfw.window) return;
    luna_glfw_get_window_rect(&ox, &oy, &ow, &oh);
    if (x != LUNA_DONT_CARE || y != LUNA_DONT_CARE)
        glfwSetWindowPos(luna_glfw.window,
                         x == LUNA_DONT_CARE ? ox : x,
                         y == LUNA_DONT_CARE ? oy : y);
    if (w != LUNA_DONT_CARE || h != LUNA_DONT_CARE)
        glfwSetWindowSize(luna_glfw.window,
                          w == LUNA_DONT_CARE ? ow : w,
                          h == LUNA_DONT_CARE ? oh : h);
}

static int luna_glfw_decoration_mode(void) {
    if (!luna_glfw.window) return LUNA_DECORATION_UNKNOWN;
    return glfwGetWindowAttrib(luna_glfw.window, GLFW_DECORATED)
               ? LUNA_DECORATION_SERVER : LUNA_DECORATION_CLIENT;
}

static void luna_glfw_set_decoration_pref(int mode) {
    if (!luna_glfw.window || mode == LUNA_DECORATION_UNKNOWN) return;
    glfwSetWindowAttrib(luna_glfw.window, GLFW_DECORATED,
                        mode == LUNA_DECORATION_SERVER ? GLFW_TRUE : GLFW_FALSE);
    luna_notify_decoration_changed(luna_glfw_decoration_mode());
    luna_glfw_wake();
}

static void luna_glfw_window_size(GLFWwindow* window, int width, int height) {
    (void)window;
    if (width > 0 && height > 0) luna_resize((float)width, (float)height);
    luna_glfw.redraw = 1;
}

static void luna_glfw_framebuffer_size(GLFWwindow* window, int width, int height) {
    (void)window; (void)width; (void)height; luna_glfw.redraw = 1;
}

static void luna_glfw_content_scale(GLFWwindow* window, float x, float y) {
    int width, height;
    (void)x; (void)y;
    glfwGetWindowSize(window, &width, &height);
    luna_resize((float)width, (float)height);
    luna_glfw.redraw = 1;
}

static void luna_glfw_cursor_pos(GLFWwindow* window, double x, double y) {
    (void)window;
    luna_glfw.event_seq++;
    if (luna_glfw.drag_mode) {
        /* The pointer belongs to the window drag until the button comes up. */
        luna_glfw_drag_update();
        luna_glfw.redraw = 1;
        return;
    }
    if (!luna_glfw.config.on_mouse_move ||
        !luna_glfw.config.on_mouse_move(x, y, luna_glfw.config.userdata))
        luna_mouse_move(x, y);
    luna_glfw.redraw = 1;
}

static void luna_glfw_mouse_button(GLFWwindow* window, int button, int action, int mods) {
    double x, y;
    luna_glfw.event_seq++;
    glfwGetCursorPos(window, &x, &y);
    if (luna_glfw.drag_mode && action == LUNA_RELEASE) {
        luna_glfw_drag_end();
        luna_glfw.redraw = 1;
        /* Fall through so Luna clears the active/pressed state of the element
         * the drag started on. */
    }
    if (!luna_glfw.config.on_mouse_button ||
        !luna_glfw.config.on_mouse_button(button, action, mods, x, y,
                                          luna_glfw.config.userdata))
        luna_mouse_button(button, action, mods, x, y);
    luna_glfw.redraw = 1;
}

static void luna_glfw_scroll(GLFWwindow* window, double x, double y) {
    (void)window;
    luna_glfw.event_seq++;
    if (!luna_glfw.config.on_scroll ||
        !luna_glfw.config.on_scroll(x, y, luna_glfw.config.userdata))
        luna_scroll(x, y);
    luna_glfw.redraw = 1;
}

static void luna_glfw_key(GLFWwindow* window, int key, int scan, int action, int mods) {
    (void)window;
    luna_glfw.event_seq++;
    if (!luna_glfw.config.on_key ||
        !luna_glfw.config.on_key(key, scan, action, mods, luna_glfw.config.userdata))
        luna_key(key, scan, action, mods);
    luna_glfw.redraw = 1;
}

static void luna_glfw_char(GLFWwindow* window, unsigned int cp) {
    (void)window;
    luna_glfw.event_seq++;
    if (!luna_glfw.config.on_char ||
        !luna_glfw.config.on_char(cp, luna_glfw.config.userdata))
        luna_char(cp);
    luna_glfw.redraw = 1;
}

static void luna_glfw_drop(GLFWwindow* window, int count, const char** paths) {
    (void)window;
    if (luna_glfw.drop_cb) luna_glfw.drop_cb(count, paths, luna_glfw.drop_data);
    luna_glfw.redraw = 1;
}

static void luna_glfw_focus(GLFWwindow* window, int focused) {
    (void)window;
    if (!focused) luna_glfw_drag_end();
    if (luna_glfw.focus_cb) luna_glfw.focus_cb(focused, luna_glfw.focus_data);
    luna_glfw.redraw = 1;
}

static void luna_glfw_close(GLFWwindow* window) {
    if (!luna_glfw_allow_close()) glfwSetWindowShouldClose(window, GLFW_FALSE);
}

/* ── LunaHostOps implementation ─────────────────────────────────────────── */

static int luna_glfw_host_start(const LunaHostConfig* user_cfg) {
    LunaHostConfig cfg;
    LunaPlatform platform;
    static const int shapes[6] = {
        GLFW_ARROW_CURSOR, GLFW_HAND_CURSOR, GLFW_IBEAM_CURSOR,
        GLFW_CROSSHAIR_CURSOR, GLFW_HRESIZE_CURSOR, GLFW_VRESIZE_CURSOR
    };

    memset(&cfg, 0, sizeof(cfg));
    if (user_cfg) cfg = *user_cfg;
    if (!cfg.title) cfg.title = "Luna UI";
    if (cfg.width <= 0) cfg.width = 1024;
    if (cfg.height <= 0) cfg.height = 768;

    memset(&luna_glfw, 0, sizeof(luna_glfw));
    luna_glfw.config = cfg;
    if (!glfwInit()) {
        fprintf(stderr, "luna-ui: glfwInit failed\n");
        return 0;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, cfg.resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, cfg.frameless ? GLFW_FALSE : GLFW_TRUE);
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER, cfg.transparent ? GLFW_TRUE : GLFW_FALSE);
    luna_glfw.window = glfwCreateWindow(cfg.width, cfg.height, cfg.title, NULL, NULL);
    if (!luna_glfw.window) {
        fprintf(stderr, "luna-ui: glfwCreateWindow failed\n");
        glfwTerminate();
        return 0;
    }
    glfwMakeContextCurrent(luna_glfw.window);
    glfwSwapInterval(cfg.vsync ? 1 : 0);

    for (int i = 0; i < 6; ++i) luna_glfw.cursors[i] = glfwCreateStandardCursor(shapes[i]);
    glfwSetWindowSizeCallback(luna_glfw.window, luna_glfw_window_size);
    glfwSetFramebufferSizeCallback(luna_glfw.window, luna_glfw_framebuffer_size);
    glfwSetWindowContentScaleCallback(luna_glfw.window, luna_glfw_content_scale);
    glfwSetCursorPosCallback(luna_glfw.window, luna_glfw_cursor_pos);
    glfwSetMouseButtonCallback(luna_glfw.window, luna_glfw_mouse_button);
    glfwSetScrollCallback(luna_glfw.window, luna_glfw_scroll);
    glfwSetKeyCallback(luna_glfw.window, luna_glfw_key);
    glfwSetCharCallback(luna_glfw.window, luna_glfw_char);
    glfwSetDropCallback(luna_glfw.window, luna_glfw_drop);
    glfwSetWindowFocusCallback(luna_glfw.window, luna_glfw_focus);
    glfwSetWindowCloseCallback(luna_glfw.window, luna_glfw_close);

    memset(&platform, 0, sizeof(platform));
    platform.get_time = luna_glfw_time;
    platform.get_proc = luna_glfw_get_proc;
    platform.set_cursor = luna_glfw_set_cursor;
    platform.request_close = luna_glfw_request_close;
    platform.iconify = luna_glfw_iconify;
    platform.maximize_toggle = luna_glfw_maximize_toggle;
    platform.request_redraw = luna_glfw_wake;
    platform.set_clipboard = luna_glfw_set_clipboard;
    platform.get_clipboard = luna_glfw_get_clipboard;
    platform.text_input = luna_glfw_text_input;
    platform.get_scale = luna_glfw_scale;
    platform.begin_move = luna_glfw_begin_move;
    platform.begin_resize = luna_glfw_begin_resize;
    platform.set_title = luna_glfw_set_title;
    platform.set_drop_handler = luna_glfw_set_drop_handler;
    platform.set_focus_handler = luna_glfw_set_focus_handler;
    platform.set_close_handler = luna_glfw_set_close_handler;
    platform.set_size_limits = luna_glfw_set_size_limits;
    platform.get_key = luna_glfw_get_key;
    platform.get_mouse_button = luna_glfw_get_mouse_button;
    platform.get_cursor_pos = luna_glfw_get_cursor_pos;
    platform.set_fullscreen = luna_glfw_set_fullscreen;
    platform.is_fullscreen = luna_glfw_is_fullscreen;
    platform.get_window_rect = luna_glfw_get_window_rect;
    platform.set_window_rect = luna_glfw_set_window_rect;
    platform.get_decoration_mode = luna_glfw_decoration_mode;
    platform.set_decoration_pref = luna_glfw_set_decoration_pref;
    platform.struct_size = sizeof(platform);
    platform.api_version = LUNA_UI_API_VERSION;
    if (cfg.configure_platform) cfg.configure_platform(&platform, cfg.userdata);
    luna_set_platform(&platform);

    if (cfg.fullscreen) luna_glfw_set_fullscreen(1);
    /* GLFW draws the pointer from the compositor's cursor theme, so a
     * caller-supplied painter has nothing to paint into here. */
    return 1;
}

static void luna_glfw_host_get_fb_size(int* w, int* h) {
    int fw = 0, fh = 0;
    if (luna_glfw.window) glfwGetFramebufferSize(luna_glfw.window, &fw, &fh);
    if (w) *w = fw;
    if (h) *h = fh;
}

static void luna_glfw_host_swap_buffers(void) {
    if (luna_glfw.window) glfwSwapBuffers(luna_glfw.window);
}

static int luna_glfw_host_should_close(void) {
    return luna_glfw.window ? glfwWindowShouldClose(luna_glfw.window) : 1;
}

static unsigned luna_glfw_host_wait_events(int timeout_ms, const int* fds, int nfds) {
    unsigned mask = 0;
    unsigned before = luna_glfw.event_seq;
    double budget;

    if (nfds > LUNA_HOST_MAX_WAIT_FDS) nfds = LUNA_HOST_MAX_WAIT_FDS;
    if (nfds > 0 && (timeout_ms < 0 || timeout_ms > 50)) {
        /* GLFW's event queue is not a descriptor, so the caller's fds cannot
         * join the same sleep.  Cap the wait and test them on each wake
         * instead.  This host is the desktop/test path; a worst-case 50 ms of
         * latency on a worker eventfd does not justify a second event thread. */
        budget = 0.05;
    } else {
        budget = timeout_ms < 0 ? -1.0 : (double)timeout_ms / 1000.0;
    }

    if (budget < 0.0)       glfwWaitEvents();
    else if (budget > 0.0)  glfwWaitEventsTimeout(budget);
    else                    glfwPollEvents();

    /* A release delivered while the window was being repositioned can be
     * dropped by the WM; re-check the button so the drag never sticks. */
    if (luna_glfw.drag_mode && luna_glfw.window &&
        glfwGetMouseButton(luna_glfw.window, LUNA_MOUSE_BUTTON_LEFT) == LUNA_RELEASE)
        luna_glfw_drag_end();

    if (nfds > 0) {
        struct pollfd pfds[LUNA_HOST_MAX_WAIT_FDS];
        int i;
        for (i = 0; i < nfds; ++i) {
            pfds[i].fd = fds[i];
            pfds[i].events = POLLIN;
            pfds[i].revents = 0;
        }
        if (poll(pfds, (nfds_t)nfds, 0) > 0)
            for (i = 0; i < nfds; ++i)
                if (pfds[i].fd >= 0 && (pfds[i].revents & POLLIN))
                    mask |= 1u << i;
    }
    if (luna_glfw.event_seq != before) mask |= LUNA_HOST_WAIT_INPUT;
    return mask;
}

static void luna_glfw_host_terminate(void) {
    int i;
    for (i = 0; i < 6; ++i)
        if (luna_glfw.cursors[i]) glfwDestroyCursor(luna_glfw.cursors[i]);
    if (luna_glfw.window) glfwDestroyWindow(luna_glfw.window);
    luna_glfw.window = NULL;
    glfwTerminate();
}

static const LunaHostOps g_luna_glfw_ops = {
    "glfw",
    luna_glfw_host_start,
    luna_glfw_host_get_fb_size,
    luna_glfw_host_swap_buffers,
    luna_glfw_host_wait_events,
    luna_glfw_set_cursor,
    luna_glfw_host_should_close,
    luna_glfw_host_terminate,
};

const LunaHostOps* luna_host_glfw(void) { return &g_luna_glfw_ops; }

#if defined(LUNA_UI_PLATFORM_BODY)
/* ── Application loop, written on the ops above ─────────────────────────── */

void* luna_app_native_handle(void) { return (void*)luna_glfw.window; }
void luna_app_quit(void) {
    if (luna_glfw.window) glfwSetWindowShouldClose(luna_glfw.window, GLFW_TRUE);
}
void luna_app_request_redraw(void) { luna_glfw_wake(); }

int luna_app_run(const LunaAppConfig* user_cfg) {
    LunaAppConfig cfg;
    LunaHostConfig host;
    LunaInitConfig init;
    int width, height, fb_width, fb_height;
    double previous;

    memset(&cfg, 0, sizeof(cfg));
    if (user_cfg) cfg = *user_cfg;
    if (!user_cfg) { cfg.resizable = 1; cfg.vsync = 1; }

    memset(&host, 0, sizeof(host));
    host.title           = cfg.title;
    host.width           = cfg.width;
    host.height          = cfg.height;
    host.resizable       = cfg.resizable;
    host.frameless       = cfg.frameless;
    host.transparent     = cfg.transparent;
    host.vsync           = cfg.vsync;
    host.on_mouse_button = cfg.on_mouse_button;
    host.on_mouse_move   = cfg.on_mouse_move;
    host.on_scroll       = cfg.on_scroll;
    host.on_key          = cfg.on_key;
    host.on_char         = cfg.on_char;
    host.on_touch        = cfg.on_touch;
    host.userdata        = cfg.userdata;
    if (!luna_glfw_host_start(&host)) return 1;

    glfwGetWindowSize(luna_glfw.window, &width, &height);
    memset(&init, 0, sizeof(init));
    init.width = (float)width;
    init.height = (float)height;
    init.get_proc = luna_glfw_get_proc;
    init.frameless = cfg.frameless;
    if (!luna_init(&init)) {
        luna_glfw_host_terminate();
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
    luna_notify_decoration_changed(luna_glfw_decoration_mode());

    luna_glfw.redraw = 1;
    previous = glfwGetTime();
    luna_update(previous, 0.0);
    while (!glfwWindowShouldClose(luna_glfw.window)) {
        double now, dt;
        if (cfg.on_frame && cfg.frame_interval <= 0.0)
            glfwPollEvents();
        else if (cfg.on_frame)
            glfwWaitEventsTimeout(cfg.frame_interval);
        else if (!luna_glfw.redraw)
            glfwWaitEvents();
        else
            glfwPollEvents();
        if (glfwWindowShouldClose(luna_glfw.window)) break;
        /* A release delivered while the window was being repositioned can be
         * dropped by the WM; re-check the button so the drag never sticks. */
        if (luna_glfw.drag_mode &&
            glfwGetMouseButton(luna_glfw.window, LUNA_MOUSE_BUTTON_LEFT) == LUNA_RELEASE)
            luna_glfw_drag_end();
        now = glfwGetTime();
        dt = now - previous;
        previous = now;
        if (dt < 0.0 || dt > 0.25) dt = 1.0 / 60.0;
        if (cfg.on_frame) cfg.on_frame(dt, cfg.userdata);
        luna_update(now, dt);
        if (luna_needs_redraw(now, dt)) luna_glfw.redraw = 1;
        if (!luna_glfw.redraw) continue;
        glfwGetFramebufferSize(luna_glfw.window, &fb_width, &fb_height);
        if (fb_width > 0 && fb_height > 0) {
            if (cfg.custom_render) {
                if (cfg.on_render) cfg.on_render(fb_width, fb_height, cfg.userdata);
            } else {
                luna_render(fb_width, fb_height);
                if (cfg.on_render) cfg.on_render(fb_width, fb_height, cfg.userdata);
            }
            glfwSwapBuffers(luna_glfw.window);
        }
        luna_glfw.redraw = 0;
    }

    if (cfg.on_shutdown) cfg.on_shutdown(cfg.userdata);
    luna_shutdown();
    luna_glfw_host_terminate();
    return 0;
}
#endif /* LUNA_UI_PLATFORM_BODY */
#endif /* LUNA_UI_IMPLEMENTATION */
#endif /* LUNA_GLFW_BODY_INCLUDED */
#endif /* LUNA_UI_PLATFORM_BODY || LUNA_UI_HOST_BODY */
