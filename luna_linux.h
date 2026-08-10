/* luna_linux.h - header-only GLFW/OpenGL host for Luna UI
 *
 * Linux applications only need GLFW and OpenGL at build time. This header does
 * not include Xlib, Xutil or GLX headers and does not call X11 directly.
 */

#if defined(LUNA_UI_PLATFORM_PRELUDE)
#ifndef LUNA_LINUX_PRELUDE_INCLUDED
#define LUNA_LINUX_PRELUDE_INCLUDED

#include <GLFW/glfw3.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>

#define LUNA_UI_PLATFORM_GL_INCLUDED 1
#endif
#endif /* LUNA_UI_PLATFORM_PRELUDE */

#if defined(LUNA_UI_PLATFORM_BODY)
#ifndef LUNA_LINUX_BODY_INCLUDED
#define LUNA_LINUX_BODY_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LunaLinuxOptions {
    /* Retained for source compatibility. GLFW selects the display backend. */
    const char* display_name;
    int screen;
} LunaLinuxOptions;

void luna_linux_set_options(const LunaLinuxOptions* options);
GLFWwindow* luna_linux_window(void);
/* Apps that replace GLFW cursor/button callbacks must forward motion/release
 * through these so client-side titlebar move/resize still runs (X11 and
 * luna_wm set_position fallbacks). */
void luna_linux_window_drag_motion(GLFWwindow* window, double x, double y);
void luna_linux_window_drag_end(void);
/* Ask the Luna shell for a window menu at surface-local (x, y). */
int luna_linux_show_window_menu(int x, int y);

#ifdef __cplusplus
}
#endif
#endif /* LUNA_LINUX_BODY_INCLUDED */

#if defined(LUNA_UI_PLATFORM_BODY) && defined(LUNA_UI_IMPLEMENTATION) && !defined(LUNA_LINUX_IMPLEMENTATION_INCLUDED)
#define LUNA_LINUX_IMPLEMENTATION_INCLUDED

typedef struct LunaLinuxState {
    GLFWwindow* window;
    GLFWcursor* cursors[6];
    int window_width;
    int window_height;
    int framebuffer_width;
    int framebuffer_height;
    int glfw_initialized;
    int visible_at_create;
    int wayland;
    int window_pos_x;
    int window_pos_y;
    int redraw;
    int window_drag_mode;
    int window_resize_edges;
    double drag_anchor_x, drag_anchor_y;
    double drag_last_x, drag_last_y;
    LunaLinuxOptions options;
    LunaAppConfig config;
} LunaLinuxState;

static LunaLinuxState luna_linux_state;
static LunaLinuxOptions luna_linux_options;

#if !defined(LUNA_LINUX_NO_WAYLAND)

/* Wayland text-input-v3 IME. Stock GLFW 3.4 builds often omit this protocol,
 * so Luna talks to the compositor directly using the same wl_display/surface. */
#if !defined(LUNA_LINUX_WAYLAND_IME)
#  define LUNA_LINUX_WAYLAND_IME 1
#endif

#if LUNA_LINUX_WAYLAND_IME
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3native.h>
#include <wayland-client.h>
#include "text-input-unstable-v3-client-protocol.h"
#include "text-input-unstable-v3-protocol.c"

typedef struct LunaLinuxIme {
    struct wl_display* display;
    struct wl_registry* registry;
    struct wl_seat* seat;
    struct zwp_text_input_manager_v3* manager;
    struct zwp_text_input_v3* text_input;
    struct wl_surface* surface;
    int enabled;
    int entered;
    int pending_enable;
    int rect_x, rect_y, rect_w, rect_h;
    char pending_preedit[1024];
    int pending_preedit_begin;
    int pending_preedit_end;
    char pending_commit[1024];
    unsigned pending_delete_before;
    unsigned pending_delete_after;
    int have_preedit;
    int have_commit;
    int have_delete;
} LunaLinuxIme;

static LunaLinuxIme luna_linux_ime;

static void luna_linux_ime_emit_utf8(const char* text) {
    const unsigned char* p = (const unsigned char*)(text ? text : "");
    while (*p) {
        unsigned cp;
        if (*p < 0x80) {
            cp = *p++;
        } else if ((*p & 0xE0) == 0xC0 && p[1]) {
            cp = ((unsigned)(p[0] & 0x1F) << 6) | (unsigned)(p[1] & 0x3F);
            p += 2;
        } else if ((*p & 0xF0) == 0xE0 && p[1] && p[2]) {
            cp = ((unsigned)(p[0] & 0x0F) << 12) |
                 ((unsigned)(p[1] & 0x3F) << 6) |
                 (unsigned)(p[2] & 0x3F);
            p += 3;
        } else if ((*p & 0xF8) == 0xF0 && p[1] && p[2] && p[3]) {
            cp = ((unsigned)(p[0] & 0x07) << 18) |
                 ((unsigned)(p[1] & 0x3F) << 12) |
                 ((unsigned)(p[2] & 0x3F) << 6) |
                 (unsigned)(p[3] & 0x3F);
            p += 4;
        } else {
            p++;
            continue;
        }
        luna_char(cp);
    }
}

static void luna_linux_ime_text_enter(void* data, struct zwp_text_input_v3* text_input,
                                      struct wl_surface* surface) {
    (void)data; (void)text_input;
    luna_linux_ime.entered = (surface == luna_linux_ime.surface);
    if (luna_linux_ime.entered && luna_linux_ime.pending_enable) {
        zwp_text_input_v3_enable(luna_linux_ime.text_input);
        zwp_text_input_v3_set_cursor_rectangle(luna_linux_ime.text_input,
            luna_linux_ime.rect_x, luna_linux_ime.rect_y,
            luna_linux_ime.rect_w, luna_linux_ime.rect_h);
        zwp_text_input_v3_commit(luna_linux_ime.text_input);
        luna_linux_ime.enabled = 1;
    }
}

static void luna_linux_ime_text_leave(void* data, struct zwp_text_input_v3* text_input,
                                      struct wl_surface* surface) {
    (void)data; (void)text_input; (void)surface;
    luna_linux_ime.entered = 0;
    luna_linux_ime.enabled = 0;
    luna_ime_clear();
}

static void luna_linux_ime_preedit(void* data, struct zwp_text_input_v3* text_input,
                                   const char* text, int32_t cursor_begin, int32_t cursor_end) {
    (void)data; (void)text_input;
    size_t n = text ? strlen(text) : 0;
    if (n >= sizeof(luna_linux_ime.pending_preedit))
        n = sizeof(luna_linux_ime.pending_preedit) - 1;
    if (n) memcpy(luna_linux_ime.pending_preedit, text, n);
    luna_linux_ime.pending_preedit[n] = '\0';
    luna_linux_ime.pending_preedit_begin = cursor_begin;
    luna_linux_ime.pending_preedit_end = cursor_end;
    luna_linux_ime.have_preedit = 1;
}

static void luna_linux_ime_commit(void* data, struct zwp_text_input_v3* text_input,
                                  const char* text) {
    (void)data; (void)text_input;
    size_t n = text ? strlen(text) : 0;
    if (n >= sizeof(luna_linux_ime.pending_commit))
        n = sizeof(luna_linux_ime.pending_commit) - 1;
    if (n) memcpy(luna_linux_ime.pending_commit, text, n);
    luna_linux_ime.pending_commit[n] = '\0';
    luna_linux_ime.have_commit = 1;
}

static void luna_linux_ime_delete(void* data, struct zwp_text_input_v3* text_input,
                                  uint32_t before_length, uint32_t after_length) {
    (void)data; (void)text_input;
    luna_linux_ime.pending_delete_before = before_length;
    luna_linux_ime.pending_delete_after = after_length;
    luna_linux_ime.have_delete = 1;
}

static void luna_linux_ime_done(void* data, struct zwp_text_input_v3* text_input,
                                uint32_t serial) {
    (void)data; (void)text_input; (void)serial;
    if (luna_linux_ime.have_delete) {
        luna_ime_delete_surrounding(luna_linux_ime.pending_delete_before,
                                    luna_linux_ime.pending_delete_after);
        luna_linux_ime.have_delete = 0;
        luna_linux_ime.pending_delete_before = 0;
        luna_linux_ime.pending_delete_after = 0;
    }
    if (luna_linux_ime.have_commit) {
        luna_ime_clear();
        luna_linux_ime_emit_utf8(luna_linux_ime.pending_commit);
        luna_linux_ime.pending_commit[0] = '\0';
        luna_linux_ime.have_commit = 0;
        luna_linux_ime.have_preedit = 0;
        luna_linux_ime.pending_preedit[0] = '\0';
        luna_linux_state.redraw = 1;
    } else if (luna_linux_ime.have_preedit) {
        luna_ime_set_preedit(luna_linux_ime.pending_preedit,
                             luna_linux_ime.pending_preedit_begin,
                             luna_linux_ime.pending_preedit_end);
        luna_linux_ime.have_preedit = 0;
        luna_linux_state.redraw = 1;
    } else {
        luna_ime_clear();
    }
}

static const struct zwp_text_input_v3_listener luna_linux_ime_text_listener = {
    luna_linux_ime_text_enter,
    luna_linux_ime_text_leave,
    luna_linux_ime_preedit,
    luna_linux_ime_commit,
    luna_linux_ime_delete,
    luna_linux_ime_done
};

static void luna_linux_ime_registry_global(void* data, struct wl_registry* registry,
                                           uint32_t name, const char* interface,
                                           uint32_t version) {
    (void)data;
    if (!interface) return;
    if (strcmp(interface, "wl_seat") == 0) {
        uint32_t ver = version < 7 ? version : 7;
        if (!luna_linux_ime.seat)
            luna_linux_ime.seat = wl_registry_bind(registry, name, &wl_seat_interface, ver);
    } else if (strcmp(interface, zwp_text_input_manager_v3_interface.name) == 0) {
        if (!luna_linux_ime.manager)
            luna_linux_ime.manager = wl_registry_bind(registry, name,
                &zwp_text_input_manager_v3_interface, 1);
    }
}

static void luna_linux_ime_registry_global_remove(void* data, struct wl_registry* registry,
                                                  uint32_t name) {
    (void)data; (void)registry; (void)name;
}

static const struct wl_registry_listener luna_linux_ime_registry_listener = {
    luna_linux_ime_registry_global,
    luna_linux_ime_registry_global_remove
};

static void luna_linux_ime_init(GLFWwindow* window) {
    memset(&luna_linux_ime, 0, sizeof(luna_linux_ime));
    if (!luna_linux_state.wayland || !window) return;
    luna_linux_ime.display = glfwGetWaylandDisplay();
    luna_linux_ime.surface = glfwGetWaylandWindow(window);
    if (!luna_linux_ime.display || !luna_linux_ime.surface) return;

    luna_linux_ime.registry = wl_display_get_registry(luna_linux_ime.display);
    if (!luna_linux_ime.registry) return;
    wl_registry_add_listener(luna_linux_ime.registry, &luna_linux_ime_registry_listener, NULL);
    wl_display_roundtrip(luna_linux_ime.display);
    if (!luna_linux_ime.manager || !luna_linux_ime.seat) return;

    luna_linux_ime.text_input = zwp_text_input_manager_v3_get_text_input(
        luna_linux_ime.manager, luna_linux_ime.seat);
    if (!luna_linux_ime.text_input) return;
    zwp_text_input_v3_add_listener(luna_linux_ime.text_input,
                                   &luna_linux_ime_text_listener, NULL);
    wl_display_flush(luna_linux_ime.display);
}

static void luna_linux_ime_shutdown(void) {
    if (luna_linux_ime.text_input) {
        zwp_text_input_v3_destroy(luna_linux_ime.text_input);
        luna_linux_ime.text_input = NULL;
    }
    if (luna_linux_ime.manager) {
        zwp_text_input_manager_v3_destroy(luna_linux_ime.manager);
        luna_linux_ime.manager = NULL;
    }
    if (luna_linux_ime.seat) {
        wl_seat_destroy(luna_linux_ime.seat);
        luna_linux_ime.seat = NULL;
    }
    if (luna_linux_ime.registry) {
        wl_registry_destroy(luna_linux_ime.registry);
        luna_linux_ime.registry = NULL;
    }
    memset(&luna_linux_ime, 0, sizeof(luna_linux_ime));
}

static void luna_linux_text_input_impl(int enabled, float x, float y, float w, float h) {
    int rx = (int)floorf(x + 0.5f);
    int ry = (int)floorf(y + 0.5f);
    int rw = (int)ceilf(w);
    int rh = (int)ceilf(h);
    if (rw < 1) rw = 1;
    if (rh < 1) rh = 1;

    luna_linux_ime.pending_enable = enabled ? 1 : 0;
    luna_linux_ime.rect_x = rx;
    luna_linux_ime.rect_y = ry;
    luna_linux_ime.rect_w = rw;
    luna_linux_ime.rect_h = rh;

    if (!luna_linux_ime.text_input) return;

    if (enabled) {
        zwp_text_input_v3_enable(luna_linux_ime.text_input);
        zwp_text_input_v3_set_content_type(luna_linux_ime.text_input, 0, 0);
        zwp_text_input_v3_set_cursor_rectangle(luna_linux_ime.text_input, rx, ry, rw, rh);
        zwp_text_input_v3_commit(luna_linux_ime.text_input);
        luna_linux_ime.enabled = 1;
    } else {
        zwp_text_input_v3_disable(luna_linux_ime.text_input);
        zwp_text_input_v3_commit(luna_linux_ime.text_input);
        luna_linux_ime.enabled = 0;
        luna_ime_clear();
    }
    if (luna_linux_ime.display) wl_display_flush(luna_linux_ime.display);
}
#else
static void luna_linux_ime_init(GLFWwindow* window) { (void)window; }
static void luna_linux_ime_shutdown(void) {}
static void luna_linux_text_input_impl(int enabled, float x, float y, float w, float h) {
    (void)enabled; (void)x; (void)y; (void)w; (void)h;
}
#endif

/* Luna compositor private absolute placement (luna_wm_v1). */
#if !defined(GLFW_EXPOSE_NATIVE_WAYLAND)
#define GLFW_EXPOSE_NATIVE_WAYLAND
#include <GLFW/glfw3native.h>
#endif
#define LUNA_WM_CLIENT_IMPLEMENTATION
#include "luna-wm-client.h"

#else /* LUNA_LINUX_NO_WAYLAND — GLFW-only builds (no libwayland-client) */

static void luna_linux_ime_init(GLFWwindow* window) { (void)window; }
static void luna_linux_ime_shutdown(void) {}
static void luna_linux_text_input_impl(int enabled, float x, float y, float w, float h) {
    (void)enabled; (void)x; (void)y; (void)w; (void)h;
}
#define LUNA_WM_CLIENT_IMPLEMENTATION
#include "luna-wm-client.h"

#endif /* LUNA_LINUX_NO_WAYLAND */

static void luna_linux_error_callback(int code, const char* description) {
    if (code == GLFW_FEATURE_UNAVAILABLE && description &&
        strstr(description, "window position"))
        return;
    fprintf(stderr, "[luna-ui] GLFW error %d: %s\n", code,
            description ? description : "unknown error");
}

static void luna_linux_sleep_millis(long milliseconds) {
    struct timeval timeout;
    if (milliseconds <= 0) return;
    timeout.tv_sec = milliseconds / 1000;
    timeout.tv_usec = (milliseconds % 1000) * 1000;
    (void)select(0, NULL, NULL, NULL, &timeout);
}

static double luna_linux_time_impl(void) {
    return glfwGetTime();
}

static void* luna_linux_get_proc(const char* name) {
    GLFWglproc proc;
    void* symbol;
    if (!name) return NULL;
    proc = glfwGetProcAddress(name);
    if (!proc) return NULL;
    /* ISO C does not guarantee conversion between function and data pointers.
     * GLFW exposes GL entry points as GLFWglproc, while Luna's host ABI uses
     * void*. Copy the representation without an explicit cast warning. */
    symbol = NULL;
    memcpy(&symbol, &proc,
           sizeof(symbol) < sizeof(proc) ? sizeof(symbol) : sizeof(proc));
    return symbol;
}

static void luna_linux_set_cursor_impl(int type) {
    int shape = GLFW_ARROW_CURSOR;
    GLFWcursor* cursor;
    if (!luna_linux_state.window) return;
    if (type < 0 || type >= 6) type = 0;

    switch (type) {
        case 1: shape = GLFW_HAND_CURSOR; break;
        case 2: shape = GLFW_IBEAM_CURSOR; break;
        case 3: shape = GLFW_CROSSHAIR_CURSOR; break;
        case 4: shape = GLFW_HRESIZE_CURSOR; break;
        case 5: shape = GLFW_VRESIZE_CURSOR; break;
        default: shape = GLFW_ARROW_CURSOR; break;
    }

    cursor = luna_linux_state.cursors[type];
    if (!cursor) {
        cursor = glfwCreateStandardCursor(shape);
        luna_linux_state.cursors[type] = cursor;
    }
    glfwSetCursor(luna_linux_state.window, cursor);
}

static void luna_linux_close_impl(void) {
    if (luna_linux_state.window)
        glfwSetWindowShouldClose(luna_linux_state.window, GLFW_TRUE);
}

static void luna_linux_redraw_impl(void) {
    int was_redraw = luna_linux_state.redraw;
    luna_linux_state.redraw = 1;
    /* Wake an event-driven host when redraw is requested outside a GLFW
     * callback.  Avoid posting duplicate empty events while already dirty. */
    if (!was_redraw && luna_linux_state.glfw_initialized) glfwPostEmptyEvent();
}

static void luna_linux_iconify_impl(void) {
    if (luna_linux_state.window) glfwIconifyWindow(luna_linux_state.window);
}

static void luna_linux_maximize_impl(void) {
    if (!luna_linux_state.window) return;
#if defined(GLFW_MAXIMIZED)
    if (glfwGetWindowAttrib(luna_linux_state.window, GLFW_MAXIMIZED))
        glfwRestoreWindow(luna_linux_state.window);
    else
        glfwMaximizeWindow(luna_linux_state.window);
#else
    glfwMaximizeWindow(luna_linux_state.window);
#endif
}

static void luna_linux_begin_move_impl(void) {
    if (!luna_linux_state.window) return;
    luna_linux_state.window_resize_edges = LUNA_RESIZE_EDGE_NONE;
    /* Wayland: one compositor grab via luna_wm.start_move.  Streaming
     * set_position every motion fought the grab, rewrote state.json, and
     * made titlebar buttons / edge snap unreliable. */
    if (luna_linux_state.wayland) {
        luna_linux_state.window_drag_mode = 0;
        if (luna_wm_client_start_move(luna_linux_state.window))
            return;
        /* No luna_wm: keep the button held; compositor CSD promotion may grab. */
        return;
    }
    glfwGetCursorPos(luna_linux_state.window, &luna_linux_state.drag_anchor_x,
                     &luna_linux_state.drag_anchor_y);
    luna_linux_state.drag_last_x = luna_linux_state.drag_anchor_x;
    luna_linux_state.drag_last_y = luna_linux_state.drag_anchor_y;
    luna_linux_state.window_drag_mode = 1;
}

static void luna_linux_begin_resize_impl(int edge) {
    if (!luna_linux_state.window || edge == LUNA_RESIZE_EDGE_NONE) return;
    glfwGetCursorPos(luna_linux_state.window, &luna_linux_state.drag_anchor_x,
                     &luna_linux_state.drag_anchor_y);
    luna_linux_state.drag_last_x = luna_linux_state.drag_anchor_x;
    luna_linux_state.drag_last_y = luna_linux_state.drag_anchor_y;
    luna_linux_state.window_drag_mode = 2;
    luna_linux_state.window_resize_edges = edge;
}

static void luna_linux_set_title_impl(const char* title) {
    if (luna_linux_state.window)
        glfwSetWindowTitle(luna_linux_state.window, title ? title : "");
}

static int luna_linux_system_notify_impl(const char* app_name, int kind,
                                         const char* title, const char* message) {
    pid_t pid;
    const char* urgency = kind == LUNA_NOTIFY_ERROR ? "critical" :
                          kind == LUNA_NOTIFY_WARNING ? "normal" : "low";
    pid = fork();
    if (pid < 0) return 0;
    if (pid == 0) {
        execlp("notify-send", "notify-send", "-a", app_name ? app_name : "Luna",
               "-u", urgency, title ? title : "", message ? message : "",
               (char*)NULL);
        _exit(127);
    }
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {}
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static void luna_linux_set_clipboard_impl(const char* utf8) {
    if (luna_linux_state.window)
        glfwSetClipboardString(luna_linux_state.window, utf8 ? utf8 : "");
}

static char* luna_linux_get_clipboard_impl(void) {
    const char* text;
    if (!luna_linux_state.window) return NULL;
    text = glfwGetClipboardString(luna_linux_state.window);
    return text ? luna_strdup_local(text) : NULL;
}

static float luna_linux_scale_impl(void) {
#if GLFW_VERSION_MAJOR > 3 || \
    (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 3)
    float xscale = 1.0f;
    float yscale = 1.0f;
    if (!luna_linux_state.window) return 1.0f;
    glfwGetWindowContentScale(luna_linux_state.window, &xscale, &yscale);
    if (xscale <= 0.0f || xscale > 8.0f) xscale = 1.0f;
    if (yscale <= 0.0f || yscale > 8.0f) yscale = 1.0f;
    return xscale > yscale ? xscale : yscale;
#else
    return 1.0f;
#endif
}

void luna_linux_window_drag_motion(GLFWwindow* window, double x, double y) {
    if (!window || !luna_linux_state.window_drag_mode) return;
    if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) != GLFW_PRESS) {
        luna_linux_state.window_drag_mode = 0;
        luna_linux_state.window_resize_edges = LUNA_RESIZE_EDGE_NONE;
        return;
    }
    int wx, wy, ww, wh;
    int min_w = 160, min_h = 120;
    if (luna_linux_state.wayland) {
        wx = luna_linux_state.window_pos_x;
        wy = luna_linux_state.window_pos_y;
    } else {
        glfwGetWindowPos(window, &wx, &wy);
        luna_linux_state.window_pos_x = wx;
        luna_linux_state.window_pos_y = wy;
    }
    glfwGetWindowSize(window, &ww, &wh);
    if (luna_linux_state.window_drag_mode == 1) {
        int dx = (int)llround(x - luna_linux_state.drag_anchor_x);
        int dy = (int)llround(y - luna_linux_state.drag_anchor_y);
        if (dx || dy) {
            wx += dx;
            wy += dy;
            luna_linux_state.window_pos_x = wx;
            luna_linux_state.window_pos_y = wy;
            /* Wayland: prefer luna_wm_v1 absolute place; otherwise only
             * Luna compositor CSD move promotion can relocate the window. */
            if (!luna_wm_client_set_pos(window, wx, wy) &&
                !luna_linux_state.wayland)
                glfwSetWindowPos(window, wx, wy);
        }
    } else {
        int edge = luna_linux_state.window_resize_edges;
        int dx_left = (int)llround(x - luna_linux_state.drag_anchor_x);
        int dy_top = (int)llround(y - luna_linux_state.drag_anchor_y);
        int dx_right = (int)llround(x - luna_linux_state.drag_last_x);
        int dy_bottom = (int)llround(y - luna_linux_state.drag_last_y);
        int nx = wx, ny = wy, nw = ww, nh = wh;
        if (edge & LUNA_RESIZE_EDGE_LEFT) { nx += dx_left; nw -= dx_left; }
        if (edge & LUNA_RESIZE_EDGE_TOP) { ny += dy_top; nh -= dy_top; }
        if (edge & LUNA_RESIZE_EDGE_RIGHT) nw += dx_right;
        if (edge & LUNA_RESIZE_EDGE_BOTTOM) nh += dy_bottom;
        if (nw < min_w) { if (edge & LUNA_RESIZE_EDGE_LEFT) nx -= min_w - nw; nw = min_w; }
        if (nh < min_h) { if (edge & LUNA_RESIZE_EDGE_TOP) ny -= min_h - nh; nh = min_h; }
        if (nx != wx || ny != wy) {
            luna_linux_state.window_pos_x = nx;
            luna_linux_state.window_pos_y = ny;
            if (!luna_wm_client_set_pos(window, nx, ny) &&
                !luna_linux_state.wayland)
                glfwSetWindowPos(window, nx, ny);
        }
        if (nw != ww || nh != wh) glfwSetWindowSize(window, nw, nh);
        if (edge & LUNA_RESIZE_EDGE_RIGHT) luna_linux_state.drag_last_x = x;
        if (edge & LUNA_RESIZE_EDGE_BOTTOM) luna_linux_state.drag_last_y = y;
    }
}

void luna_linux_window_drag_end(void) {
    luna_linux_state.window_drag_mode = 0;
    luna_linux_state.window_resize_edges = LUNA_RESIZE_EDGE_NONE;
}

int luna_linux_show_window_menu(int x, int y) {
    if (!luna_linux_state.window) return 0;
    return luna_wm_client_show_window_menu(luna_linux_state.window, x, y);
}

static void luna_linux_cursor_position_callback(GLFWwindow* window,
                                                 double x, double y) {
    luna_linux_window_drag_motion(window, x, y);
    if (luna_mouse_move_changed(x, y)) luna_linux_state.redraw = 1;
}

static void luna_linux_mouse_button_callback(GLFWwindow* window,
                                             int button, int action, int mods) {
    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    luna_mouse_button(button, action, mods, x, y);
    luna_linux_state.redraw = 1;
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE)
        luna_linux_window_drag_end();
}

static void luna_linux_scroll_callback(GLFWwindow* window,
                                       double xoffset, double yoffset) {
    (void)window;
    luna_scroll(xoffset, yoffset);
    luna_linux_state.redraw = 1;
}

static void luna_linux_key_callback(GLFWwindow* window, int key, int scancode,
                                    int action, int mods) {
    (void)window;
    luna_key(key, scancode, action, mods);
    luna_linux_state.redraw = 1;
}

static void luna_linux_char_callback(GLFWwindow* window, unsigned int codepoint) {
    (void)window;
    luna_char(codepoint);
    luna_linux_state.redraw = 1;
}

static void luna_linux_window_size_callback(GLFWwindow* window,
                                            int width, int height) {
    (void)window;
    luna_linux_state.window_width = width;
    luna_linux_state.window_height = height;
    if (width > 0 && height > 0)
        luna_resize((float)width, (float)height);
    luna_linux_state.redraw = 1;
}

static void luna_linux_framebuffer_size_callback(GLFWwindow* window,
                                                 int width, int height) {
    (void)window;
    luna_linux_state.framebuffer_width = width;
    luna_linux_state.framebuffer_height = height;
    luna_framebuffer_resized();
    luna_linux_state.redraw = 1;
}

#if GLFW_VERSION_MAJOR > 3 || \
    (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 3)
static void luna_linux_content_scale_callback(GLFWwindow* window,
                                              float xscale, float yscale) {
    (void)window;
    (void)xscale;
    (void)yscale;
    luna_framebuffer_resized();
    luna_linux_redraw_impl();
}
#endif

static void luna_linux_window_refresh_callback(GLFWwindow* window) {
    (void)window;
    luna_linux_state.redraw = 1;
}

static void luna_linux_install_callbacks(GLFWwindow* window) {
    glfwSetCursorPosCallback(window, luna_linux_cursor_position_callback);
    glfwSetMouseButtonCallback(window, luna_linux_mouse_button_callback);
    glfwSetScrollCallback(window, luna_linux_scroll_callback);
    glfwSetKeyCallback(window, luna_linux_key_callback);
    glfwSetCharCallback(window, luna_linux_char_callback);
    glfwSetWindowSizeCallback(window, luna_linux_window_size_callback);
    glfwSetFramebufferSizeCallback(window, luna_linux_framebuffer_size_callback);
    glfwSetWindowRefreshCallback(window, luna_linux_window_refresh_callback);
#if GLFW_VERSION_MAJOR > 3 || \
    (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 3)
    glfwSetWindowContentScaleCallback(window, luna_linux_content_scale_callback);
#endif
}

static void luna_linux_destroy_host(void) {
    int i;
    luna_linux_ime_shutdown();
    luna_wm_client_shutdown();
    for (i = 0; i < 6; ++i) {
        if (luna_linux_state.cursors[i]) {
            glfwDestroyCursor(luna_linux_state.cursors[i]);
            luna_linux_state.cursors[i] = NULL;
        }
    }
    if (luna_linux_state.window) {
        glfwDestroyWindow(luna_linux_state.window);
        luna_linux_state.window = NULL;
    }
    if (luna_linux_state.glfw_initialized) {
        glfwTerminate();
        luna_linux_state.glfw_initialized = 0;
    }
}

void luna_linux_set_options(const LunaLinuxOptions* options) {
    if (options) luna_linux_options = *options;
    else memset(&luna_linux_options, 0, sizeof(luna_linux_options));
}

GLFWwindow* luna_linux_window(void) {
    return luna_linux_state.window;
}

void* luna_app_native_handle(void) {
    /* Deliberately returns GLFWwindow*, not an X11 Window. Applications that
     * require a native X11/Wayland handle may use GLFW's native-access API in
     * their own platform-specific translation unit. */
    return (void*)luna_linux_state.window;
}

void luna_app_quit(void) {
    luna_linux_close_impl();
}

void luna_app_request_redraw(void) {
    luna_linux_redraw_impl();
}

/* Paint and present one complete framebuffer.  Keep this in one place so
 * startup and the event loop cannot drift into subtly different render paths. */
static int luna_linux_present_frame(void) {
    GLFWwindow* window = luna_linux_state.window;
    int framebuffer_width, framebuffer_height;
    if (!window) return 0;

    glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
    luna_linux_state.framebuffer_width = framebuffer_width;
    luna_linux_state.framebuffer_height = framebuffer_height;
    if (framebuffer_width <= 0 || framebuffer_height <= 0) return 0;

    luna_render(framebuffer_width, framebuffer_height);
    if (luna_linux_state.config.on_render)
        luna_linux_state.config.on_render(framebuffer_width, framebuffer_height,
                                          luna_linux_state.config.userdata);
    glfwSwapBuffers(window);
    return 1;
}

int luna_app_run(const LunaAppConfig* user_config) {
    LunaAppConfig config;
    LunaPlatform platform;
    LunaInitConfig init;
    GLFWwindow* window;
    double previous;
    int luna_initialized = 0;
    int result = 1;

    memset(&config, 0, sizeof(config));
    if (user_config) config = *user_config;
    if (!config.title) config.title = "Luna UI";
    if (config.width <= 0) config.width = 1024;
    if (config.height <= 0) config.height = 768;
    if (!user_config) {
        config.resizable = 1;
        config.vsync = 1;
    }

    memset(&luna_linux_state, 0, sizeof(luna_linux_state));
    luna_linux_state.options = luna_linux_options;
    luna_linux_state.config = config;

    glfwSetErrorCallback(luna_linux_error_callback);
#ifdef GLFW_WAYLAND_LIBDECOR
    /* luna-session selects the dependency-free libdecor-cairo plugin.  Keep
     * GLFW's normal libdecor path by default: on some GLFW/Mesa builds the
     * forced native-decoration path configures the xdg_toplevel but never
     * publishes its first EGL buffer.  An explicit false value remains useful
     * for diagnostics on systems with a known-good native path. */
    {
        const char* choice = getenv("LUNA_GLFW_LIBDECOR");
        int disable_libdecor = choice &&
            (!strcmp(choice, "0") || !strcmp(choice, "no") ||
             !strcmp(choice, "false") || !strcmp(choice, "off"));
        if (disable_libdecor)
            glfwInitHint(GLFW_WAYLAND_LIBDECOR, GLFW_WAYLAND_DISABLE_LIBDECOR);
    }
#endif
    if (!glfwInit()) {
        fprintf(stderr, "[luna-ui] glfwInit failed\n");
        return 1;
    }
    luna_linux_state.glfw_initialized = 1;
#if defined(GLFW_PLATFORM_WAYLAND) && \
    (GLFW_VERSION_MAJOR > 3 || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 4))
    luna_linux_state.wayland = glfwGetPlatform() == GLFW_PLATFORM_WAYLAND;
#else
    {
        const char* session_type = getenv("XDG_SESSION_TYPE");
        const char* wayland_display = getenv("WAYLAND_DISPLAY");
        luna_linux_state.wayland =
            (session_type && strcmp(session_type, "wayland") == 0) ||
            (wayland_display && wayland_display[0] && !getenv("DISPLAY"));
    }
#endif

    glfwDefaultWindowHints();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    /* A hidden GLFW Wayland window creates its xdg_toplevel only when
     * glfwShowWindow is called.  With Mesa's wl_shm EGL path, creating the EGL
     * window/context before that role exists can leave swapBuffers returning
     * forever without attaching a wl_buffer.  Wayland does not expose an
     * unpainted native window anyway: make it logically visible at creation,
     * then publish the first pixels with the first swap below.  Keep the
     * hidden warm-up on X11 where mapping and buffer attachment are separate. */
    {
        const char* session_type = getenv("XDG_SESSION_TYPE");
        const char* wayland_display = getenv("WAYLAND_DISPLAY");
        int wayland_session =
            (session_type && strcmp(session_type, "wayland") == 0) ||
            (wayland_display && wayland_display[0] && !getenv("DISPLAY"));
        luna_linux_state.visible_at_create = wayland_session;
        glfwWindowHint(GLFW_VISIBLE, wayland_session ? GLFW_TRUE : GLFW_FALSE);
    }
    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_DECORATED, config.frameless ? GLFW_FALSE : GLFW_TRUE);
#ifdef GLFW_TRANSPARENT_FRAMEBUFFER
    glfwWindowHint(GLFW_TRANSPARENT_FRAMEBUFFER,
                   config.transparent ? GLFW_TRUE : GLFW_FALSE);
#endif
#ifdef GLFW_SCALE_TO_MONITOR
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
#endif

    window = glfwCreateWindow(config.width, config.height, config.title, NULL, NULL);
    if (!window) {
        fprintf(stderr, "[luna-ui] glfwCreateWindow failed (OpenGL 3.3 required)\n");
        goto cleanup;
    }
    luna_linux_state.window = window;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(config.vsync ? 1 : 0);
    luna_linux_install_callbacks(window);

    glfwGetWindowSize(window, &luna_linux_state.window_width,
                      &luna_linux_state.window_height);
    glfwGetFramebufferSize(window, &luna_linux_state.framebuffer_width,
                          &luna_linux_state.framebuffer_height);

    memset(&platform, 0, sizeof(platform));
    platform.struct_size = sizeof(platform);
    platform.api_version = LUNA_UI_API_VERSION;
    platform.get_time = luna_linux_time_impl;
    platform.get_proc = luna_linux_get_proc;
    platform.set_cursor = luna_linux_set_cursor_impl;
    platform.request_close = luna_linux_close_impl;
    platform.iconify = luna_linux_iconify_impl;
    platform.maximize_toggle = luna_linux_maximize_impl;
    platform.request_redraw = luna_linux_redraw_impl;
    platform.set_clipboard = luna_linux_set_clipboard_impl;
    platform.get_clipboard = luna_linux_get_clipboard_impl;
    platform.get_scale = luna_linux_scale_impl;
    platform.begin_move = luna_linux_begin_move_impl;
    platform.begin_resize = luna_linux_begin_resize_impl;
    platform.set_title = luna_linux_set_title_impl;
    platform.system_notify = luna_linux_system_notify_impl;
    platform.text_input = luna_linux_text_input_impl;
    luna_set_platform(&platform);

    luna_linux_ime_init(window);
    if (luna_linux_state.wayland) {
        luna_wm_client_init(window);
        if (luna_wm_client_available())
            fprintf(stderr, "[luna-ui] luna_wm_v1 placement enabled\n");
    }

    memset(&init, 0, sizeof(init));
    init.width = (float)luna_linux_state.window_width;
    init.height = (float)luna_linux_state.window_height;
    init.get_proc = luna_linux_get_proc;
    init.frameless = config.frameless;
    if (!luna_init(&init)) {
        fprintf(stderr, "[luna-ui] luna_init failed\n");
        goto cleanup;
    }
    luna_initialized = 1;

    if (config.html) luna_parse_html(config.html);
    else if (config.html_path && !luna_load_html_file(config.html_path))
        fprintf(stderr, "[luna-ui] could not load HTML: %s\n", config.html_path);

    if (config.css) luna_parse_css(config.css);
    else if (config.css_path && !luna_load_css_file(config.css_path))
        fprintf(stderr, "[luna-ui] could not load CSS: %s\n", config.css_path);

    luna_inject_body_background();
    if (config.on_init) config.on_init(config.userdata);
    luna_wire_onclick_handlers();

    /* Prime layout everywhere, but prime GPU draw state only while the X11
     * window is hidden.  On Wayland even drawing (without swapping) before the
     * initial xdg_surface.configure can strand Mesa's wl_shm EGL surface with
     * no first buffer attachment. */
    previous = glfwGetTime();
    luna_update(previous, 0.0);
    luna_linux_state.redraw = 0;
    if (!luna_linux_state.visible_at_create) {
        int framebuffer_width, framebuffer_height;
        glfwGetFramebufferSize(window, &framebuffer_width, &framebuffer_height);
        if (framebuffer_width > 0 && framebuffer_height > 0) {
            luna_render(framebuffer_width, framebuffer_height);
            if (config.on_render)
                config.on_render(framebuffer_width, framebuffer_height,
                                 config.userdata);
        }
    }

    /* Map first so Wayland can issue the initial configure, then present. */
    glfwShowWindow(window);
    glfwPollEvents();

    double mapped_now = glfwGetTime();
    double mapped_dt = mapped_now - previous;
    if (mapped_dt <= 0.0 || mapped_dt > 0.25) mapped_dt = 1.0 / 60.0;
    previous = mapped_now;
    int settling = luna_update_settling(mapped_now, mapped_dt);
    luna_linux_state.redraw = 0;
    (void)luna_linux_present_frame();

    /* One final mapped follow-up fills the other back buffer / absorbs a late
     * map-time resize. This replaces the old editor's several startup frames. */
    int startup_frames = 1;

    while (!glfwWindowShouldClose(window)) {
        double now;
        double dt;
        int continuous = config.on_frame != NULL && config.frame_interval <= 0.0;
        int periodic = config.on_frame != NULL && config.frame_interval > 0.0;
        int css_animating = luna_css_anim_running_under(-1);

        /* Static applications block indefinitely. Continuous applications poll
         * every frame. Periodic applications (for example an editor caret) sleep
         * between idle ticks but still wake immediately for native events. */
        if (!continuous && !luna_linux_state.redraw && !settling && !css_animating &&
            startup_frames <= 0) {
            if (periodic) glfwWaitEventsTimeout(config.frame_interval);
            else glfwWaitEvents();
        } else {
            glfwPollEvents();
        }

        now = glfwGetTime();
        dt = now - previous;
        previous = now;
        if (dt < 0.0 || dt > 0.25) dt = 1.0 / 60.0;

        int requested = luna_linux_state.redraw;
        luna_linux_state.redraw = 0;
        if (config.on_frame) config.on_frame(dt, config.userdata);
        settling = luna_update_settling(now, dt);
        css_animating = luna_css_anim_running_under(-1);

        if (continuous || requested || luna_linux_state.redraw || settling || css_animating ||
            startup_frames > 0) {
            /* Consume redraw requests that caused this paint before rendering.
             * A new request raised from luna_render()/on_render then survives
             * for the next iteration instead of being cleared after the swap. */
            luna_linux_state.redraw = 0;
            (void)luna_linux_present_frame();
            if (startup_frames > 0) startup_frames--;
        }

        if (continuous && !config.vsync) luna_linux_sleep_millis(1);
    }

    result = 0;

cleanup:
    if (luna_initialized) {
        if (config.on_shutdown) config.on_shutdown(config.userdata);
        luna_shutdown();
    }
    luna_linux_destroy_host();
    return result;
}

#endif /* LUNA_UI_IMPLEMENTATION */
#endif /* LUNA_UI_PLATFORM_BODY */
