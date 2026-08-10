/*
 * luna-window.h — standard Luna window chrome, dialogs, notifications and
 * reusable file dialog/file manager.
 *
 * Usage:
 *   #define LUNA_WINDOW_IMPLEMENTATION
 *   #include "luna-window.h"
 *
 * To compile the reusable desktop file dialog implementation in the same TU:
 *   #define LUNA_UI_IMPLEMENTATION
 *   #define LUNA_WINDOW_IMPLEMENTATION
 *   #define LUNA_WINDOW_FILE_DIALOG_IMPLEMENTATION
 *   #include "luna-window.h"
 *
 * luna-window.h will raise LUNA_UI_MAX_ELEMENTS to 2048 for this case.
 *
 * Copyright © 2026 Yuichiro Nakada / Project Luna (Vespera) — MPL 2.0
 */
#ifndef LUNA_WINDOW_H
#define LUNA_WINDOW_H

/*
 * The built-in Luna file dialog preallocates 384 item slots (four UI
 * elements per slot) plus its chrome, menus and modals.  The default Luna UI
 * capacity is therefore not large enough when the full dialog implementation
 * is compiled into this translation unit.
 *
 * Keep the larger capacity local to users of the file dialog rather than
 * increasing Luna UI's global default for every application.
 */
#if defined(LUNA_WINDOW_FILE_DIALOG_IMPLEMENTATION)
#  if defined(LUNA_UI_H)
#    if LUNA_UI_MAX_ELEMENTS < 2048
#      error "Luna file dialog requires LUNA_UI_MAX_ELEMENTS >= 2048; include luna-window.h before luna-ui.h or increase LUNA_UI_MAX_ELEMENTS"
#    endif
#  else
#    ifndef LUNA_UI_MAX_ELEMENTS
#      define LUNA_UI_MAX_ELEMENTS 2048
#    elif LUNA_UI_MAX_ELEMENTS < 2048
#      error "Luna file dialog requires LUNA_UI_MAX_ELEMENTS >= 2048"
#    endif
#  endif
#endif

#ifndef LUNA_UI_H
#include "luna-ui.h"
#endif

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LUNA_WINDOW_API_VERSION 0x00020000u
#define LUNA_FILE_DIALOG_MAX_RESULTS 32
#if defined(LUNA_WINDOW_FILE_DIALOG_IMPLEMENTATION) && !defined(LUNA_WINDOW_IMPLEMENTATION)
#error "LUNA_WINDOW_FILE_DIALOG_IMPLEMENTATION requires LUNA_WINDOW_IMPLEMENTATION in the same translation unit"
#endif
#if defined(LUNA_WINDOW_FILE_DIALOG_IMPLEMENTATION) && !defined(LUNA_UI_IMPLEMENTATION)
#error "LUNA_WINDOW_FILE_DIALOG_IMPLEMENTATION requires LUNA_UI_IMPLEMENTATION in the same translation unit"
#endif
#ifndef LUNA_WINDOW_PATH_MAX
#define LUNA_WINDOW_PATH_MAX 4096
#endif

typedef enum LunaWindowThemeMode {
    LUNA_WINDOW_THEME_SYSTEM = 0,
    LUNA_WINDOW_THEME_LIGHT,
    LUNA_WINDOW_THEME_DARK,
    LUNA_WINDOW_THEME_CUSTOM
} LunaWindowThemeMode;

typedef enum LunaWindowChromeMode {
    LUNA_WINDOW_CHROME_AUTO = 0,
    LUNA_WINDOW_CHROME_NATIVE,
    LUNA_WINDOW_CHROME_CLIENT,
    LUNA_WINDOW_CHROME_NONE
} LunaWindowChromeMode;

typedef enum LunaWindowDialogKind {
    LUNA_WINDOW_DIALOG_INFO = 0,
    LUNA_WINDOW_DIALOG_SUCCESS,
    LUNA_WINDOW_DIALOG_WARNING,
    LUNA_WINDOW_DIALOG_ERROR,
    LUNA_WINDOW_DIALOG_QUESTION
} LunaWindowDialogKind;

typedef enum LunaWindowDialogResult {
    LUNA_WINDOW_RESULT_NONE = 0,
    LUNA_WINDOW_RESULT_OK,
    LUNA_WINDOW_RESULT_CANCEL,
    LUNA_WINDOW_RESULT_YES,
    LUNA_WINDOW_RESULT_NO
} LunaWindowDialogResult;

typedef struct LunaWindowTheme {
    LunaWindowThemeMode mode;
    uint32_t window_background;
    uint32_t surface;
    uint32_t surface_secondary;
    uint32_t border;
    uint32_t text;
    uint32_t text_secondary;
    uint32_t accent;
    uint32_t accent_hover;
    uint32_t danger;
    uint32_t titlebar_active;
    uint32_t titlebar_inactive;
    uint32_t titlebar_frame;
    float titlebar_height;
    float corner_radius;
    float dialog_radius;
    float control_size;
    int titlebar_style; /* 0 modern, 1 classic traffic lights, 2 flat retro */
    int controls_on_left;
} LunaWindowTheme;

typedef struct LunaWindowConfig {
    const char* root_id;
    const char* titlebar_id;
    const char* title_id;
    const char* close_id;
    const char* minimize_id;
    const char* maximize_id;
    const char* resize_left_id;
    const char* resize_right_id;
    const char* resize_top_id;
    const char* resize_bottom_id;
    const char* resize_top_left_id;
    const char* resize_top_right_id;
    const char* resize_bottom_left_id;
    const char* resize_bottom_right_id;
    const char* title;
    const char* app_name;
    LunaWindowChromeMode chrome;
    LunaWindowThemeMode theme;
} LunaWindowConfig;

typedef void (*LunaWindowDialogCallback)(LunaWindowDialogResult result,
                                         const char* input,
                                         void* userdata);

const char* luna_window_standard_css(void);
const char* luna_window_standard_titlebar_html(void);
const char* luna_window_standard_overlay_html(void);
void luna_window_theme_default(LunaWindowTheme* theme, LunaWindowThemeMode mode);
int  luna_window_theme_load_file(LunaWindowTheme* theme, const char* path);
int  luna_window_theme_load_system(LunaWindowTheme* theme);
void luna_window_theme_apply(const LunaWindowTheme* theme);

void luna_window_bind(const LunaWindowConfig* config);
void luna_window_unbind(void);
void luna_window_tick(double dt);
/* Re-paint chrome that sits above host custom drawing (dialog, toast, window
 * menu).  Call after any on_render that paints over the Luna DOM — e.g. a
 * virtualized editor surface — otherwise those popups appear behind it. */
void luna_window_render_floating(int fbw, int fbh);
void luna_window_set_title(const char* title);
void luna_window_alert(LunaWindowDialogKind kind, const char* title,
                       const char* message);
void luna_window_confirm(LunaWindowDialogKind kind, const char* title,
                         const char* message, LunaWindowDialogCallback callback,
                         void* userdata);
void luna_window_prompt(const char* title, const char* message,
                        const char* initial_value,
                        LunaWindowDialogCallback callback, void* userdata);
void luna_window_dialog_close(void);
void luna_window_toast(LunaWindowDialogKind kind, const char* title,
                       const char* message, double seconds);
int  luna_window_notify(const char* app_name, LunaWindowDialogKind kind,
                        const char* title, const char* message, double fallback_seconds);

typedef enum LunaFileDialogMode {
    LUNA_FILE_DIALOG_MANAGER = 0,
    LUNA_FILE_DIALOG_OPEN_FILE,
    LUNA_FILE_DIALOG_OPEN_FILES,
    LUNA_FILE_DIALOG_SELECT_FOLDER,
    LUNA_FILE_DIALOG_SAVE_FILE
} LunaFileDialogMode;

typedef enum LunaFileDialogBackend {
    /* AUTO means the Luna standard dialog. Desktop helpers are explicit opt-ins. */
    LUNA_FILE_DIALOG_BACKEND_AUTO = 0,
    LUNA_FILE_DIALOG_BACKEND_LUNA,
    LUNA_FILE_DIALOG_BACKEND_ZENITY,
    LUNA_FILE_DIALOG_BACKEND_KDIALOG,
    LUNA_FILE_DIALOG_BACKEND_YAD
} LunaFileDialogBackend;

typedef struct LunaFileDialogConfig {
    LunaFileDialogMode mode;
    LunaFileDialogBackend backend;
    const char* title;
    const char* initial_path;
    const char* suggested_name;
    const char* accept_label;
    /* Optional desktop-helper filter, e.g. "Images" and "*.png *.jpg". */
    const char* filter_name;
    const char* filter_patterns;
    /* Deprecated compatibility field. The built-in Luna backend no longer
       launches luna-fm, so this value is ignored. */
    const char* luna_fm_path;
    int width;
    int height;
    int client_chrome;
} LunaFileDialogConfig;

typedef struct LunaFileDialogResult {
    int accepted;
    int count;
    char paths[LUNA_FILE_DIALOG_MAX_RESULTS][LUNA_WINDOW_PATH_MAX];
} LunaFileDialogResult;

/* Returns 0 when the dialog ran normally, including user cancellation.
 * Inspect result->accepted to distinguish Accept from Cancel. */
int luna_file_dialog_run(const LunaFileDialogConfig* config,
                         LunaFileDialogResult* result);

/* Opens a separate dialog process, so it is safe to call from an already
 * running Luna UI application. AUTO/LUNA use the file-dialog implementation
 * compiled into the current executable; no luna-fm executable is required.
 * zenity/kdialog/yad are explicit opt-ins. Returns 0 for Accept or Cancel and
 * a negative value only when the requested backend could not be launched. */
int luna_file_dialog_show(const LunaFileDialogConfig* config,
                          LunaFileDialogResult* result);
LunaFileDialogBackend luna_file_dialog_backend_from_string(const char* name);
const char* luna_file_dialog_backend_name(LunaFileDialogBackend backend);

#ifdef __cplusplus
}
#endif

#ifdef LUNA_WINDOW_IMPLEMENTATION
#ifndef LUNA_WINDOW_IMPLEMENTATION_INCLUDED
#define LUNA_WINDOW_IMPLEMENTATION_INCLUDED

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* luna_window_css_text =
/* Surface fill is chrome-only (:not(.app)).  Applications that share the
 * chrome root keep authorship of background/color in their own stylesheet. */
".luna-window-root{position:fixed;inset:0;display:flex;flex-direction:column;overflow:hidden;}\n"
".luna-window-root:not(.app){background:#f3f5f8;color:#202633;}\n"
".luna-window-root.luna-theme-dark:not(.app){background:#111722;color:#edf2f7;}\n"
".luna-titlebar{height:42px;min-height:42px;display:flex;align-items:center;position:relative;padding:0 12px;border-bottom:1px solid rgba(30,41,59,.14);background:rgba(250,251,253,.96);user-select:none;}\n"
".luna-theme-dark .luna-titlebar{background:rgba(24,31,43,.97);border-bottom-color:rgba(148,163,184,.16);}\n"
".luna-titlebar-title{flex:1;text-align:center;font-weight:700;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;pointer-events:none;}\n"
".luna-window-controls{position:absolute;left:13px;top:0;height:100%;display:flex;align-items:center;gap:8px;z-index:3;}\n"
".luna-window-control{width:13px;height:13px;border-radius:50%;cursor:pointer;}\n"
".luna-window-close{background:#ff5f57}.luna-window-minimize{background:#febc2e}.luna-window-maximize{background:#28c840}\n"
".luna-controls-right .luna-window-controls{left:auto;right:13px}.luna-controls-left .luna-window-controls{left:13px;right:auto}\n"
".luna-titlebar-classic .luna-window-control{box-shadow:inset 0 0 0 1px rgba(0,0,0,.22)}\n"
".luna-titlebar-retro .luna-window-controls{gap:4px}.luna-titlebar-retro .luna-window-control{width:22px;height:18px;border-radius:3px;background:#d9dee6;border:1px solid #748092;box-shadow:inset 1px 1px rgba(255,255,255,.75)}\n"
".luna-titlebar-retro .luna-window-close{background:#d96a70}.luna-titlebar-retro .luna-window-minimize{background:#d9dee6}.luna-titlebar-retro .luna-window-maximize{background:#d9dee6}\n"
".luna-window-content{flex:1;min-width:0;min-height:0;overflow:hidden;}\n"
".luna-resize-handle{position:fixed;z-index:1000;background:transparent;}\n"
".luna-resize-left{left:0;top:8px;bottom:8px;width:6px;cursor:ew-resize}.luna-resize-right{right:0;top:8px;bottom:8px;width:6px;cursor:ew-resize}\n"
".luna-resize-top{left:8px;right:8px;top:0;height:6px;cursor:ns-resize}.luna-resize-bottom{left:8px;right:8px;bottom:0;height:6px;cursor:ns-resize}\n"
".luna-resize-tl,.luna-resize-tr,.luna-resize-bl,.luna-resize-br{width:10px;height:10px}.luna-resize-tl{left:0;top:0;cursor:nwse-resize}.luna-resize-tr{right:0;top:0;cursor:nesw-resize}.luna-resize-bl{left:0;bottom:0;cursor:nesw-resize}.luna-resize-br{right:0;bottom:0;cursor:nwse-resize}\n"
".luna-common-overlay{position:fixed;inset:0;display:flex;align-items:center;justify-content:center;padding:18px;background:rgba(10,15,25,.36);z-index:5000;}\n"
".luna-common-overlay.hidden,.luna-common-toast.hidden{display:none;}\n"
".luna-common-dialog{width:92%;max-width:480px;display:flex;flex-direction:column;gap:12px;padding:18px;border-radius:15px;border:1px solid rgba(255,255,255,.54);background:rgba(249,250,252,.98);box-shadow:0 26px 80px rgba(15,23,42,.34);}\n"
".luna-theme-dark .luna-common-dialog{background:#192230;border-color:rgba(148,163,184,.20);box-shadow:0 28px 80px rgba(0,0,0,.54);}\n"
".luna-common-dialog-title{font-size:16px;font-weight:750}.luna-common-dialog-message{line-height:1.55;white-space:pre-wrap;color:#5d6b7d}.luna-theme-dark .luna-common-dialog-message{color:#b4c0ce}\n"
".luna-common-dialog-input{height:36px;padding:7px 10px;border:1px solid #9fb5cf;border-radius:9px;background:white;color:#172033}.luna-theme-dark .luna-common-dialog-input{background:#111a26;color:#eef3f8;border-color:#40536b}\n"
".luna-common-dialog-actions{display:flex;justify-content:flex-end;gap:8px}.luna-common-button{min-width:78px;height:32px;display:flex;align-items:center;justify-content:center;border-radius:9px;background:#e6ebf1;cursor:pointer}.luna-theme-dark .luna-common-button{background:#2b394b}.luna-common-button.primary{background:#237fe5;color:white}.luna-common-button.danger{background:#d94a55;color:white}\n"
".luna-common-toast{position:fixed;right:16px;top:54px;width:340px;max-width:calc(100% - 32px);display:flex;flex-direction:column;gap:3px;padding:13px 15px;border-radius:13px;background:rgba(24,32,45,.94);color:white;box-shadow:0 18px 48px rgba(0,0,0,.34);z-index:5200}.luna-common-toast-title{font-weight:700}.luna-common-toast-message{color:rgba(236,242,248,.82);white-space:pre-wrap;}\n"
".luna-window-menu{position:fixed;left:0;top:0;width:188px;display:flex;flex-direction:column;gap:1px;padding:6px;border:1px solid rgba(71,85,105,.18);border-radius:10px;background:rgba(250,252,255,.98);box-shadow:0 14px 36px rgba(15,23,42,.22);z-index:5400;color:#303641;font-size:12px;}\n"
".luna-window-menu.hidden{display:none;}\n"
".luna-theme-dark .luna-window-menu{background:#172130;border-color:rgba(148,163,184,.18);box-shadow:0 18px 48px rgba(0,0,0,.45);color:#d7e0ea;}\n"
".luna-window-menu-item{height:28px;min-height:28px;display:flex;align-items:center;padding:0 10px;border-radius:6px;cursor:pointer;white-space:nowrap;}\n"
".luna-window-menu-item:hover{background:#dbeafe;color:#145da7;}\n"
".luna-theme-dark .luna-window-menu-item:hover{background:rgba(59,130,246,.22);color:#dbeafe;}\n"
".luna-window-menu-sep{height:1px;margin:3px 4px;background:rgba(71,85,105,.13);}\n"
".luna-theme-dark .luna-window-menu-sep{background:rgba(148,163,184,.16);}\n";

static const char* luna_window_titlebar_html_text =
"<header id=\"luna-titlebar\" class=\"luna-titlebar\">"
"<div class=\"luna-window-controls\">"
"<div id=\"luna-window-close\" class=\"luna-window-control luna-window-close\" onclick=\"lunaWindowClose()\"></div>"
"<div id=\"luna-window-minimize\" class=\"luna-window-control luna-window-minimize\" onclick=\"lunaWindowMinimize()\"></div>"
"<div id=\"luna-window-maximize\" class=\"luna-window-control luna-window-maximize\" onclick=\"lunaWindowMaximize()\"></div>"
"</div><div id=\"luna-window-title\" class=\"luna-titlebar-title\"></div></header>"
"<div id=\"luna-resize-left\" class=\"luna-resize-handle luna-resize-left\"></div>"
"<div id=\"luna-resize-right\" class=\"luna-resize-handle luna-resize-right\"></div>"
"<div id=\"luna-resize-top\" class=\"luna-resize-handle luna-resize-top\"></div>"
"<div id=\"luna-resize-bottom\" class=\"luna-resize-handle luna-resize-bottom\"></div>"
"<div id=\"luna-resize-tl\" class=\"luna-resize-handle luna-resize-tl\"></div>"
"<div id=\"luna-resize-tr\" class=\"luna-resize-handle luna-resize-tr\"></div>"
"<div id=\"luna-resize-bl\" class=\"luna-resize-handle luna-resize-bl\"></div>"
"<div id=\"luna-resize-br\" class=\"luna-resize-handle luna-resize-br\"></div>";

static const char* luna_window_overlay_html_text =
"<div id=\"luna-common-dialog-overlay\" class=\"luna-common-overlay hidden\">"
"<div id=\"luna-common-dialog\" class=\"luna-common-dialog\" role=\"dialog\">"
"<div id=\"luna-common-dialog-title\" class=\"luna-common-dialog-title\"></div>"
"<div id=\"luna-common-dialog-message\" class=\"luna-common-dialog-message\"></div>"
"<input id=\"luna-common-dialog-input\" class=\"luna-common-dialog-input hidden\" type=\"text\">"
"<div class=\"luna-common-dialog-actions\">"
"<div id=\"luna-common-dialog-cancel\" class=\"luna-common-button hidden\" onclick=\"lunaWindowDialogCancel()\">Cancel</div>"
"<div id=\"luna-common-dialog-ok\" class=\"luna-common-button primary\" onclick=\"lunaWindowDialogOk()\">OK</div>"
"</div></div></div>"
"<div id=\"luna-common-toast\" class=\"luna-common-toast hidden\" onclick=\"lunaWindowToastClose()\">"
"<div id=\"luna-common-toast-title\" class=\"luna-common-toast-title\"></div>"
"<div id=\"luna-common-toast-message\" class=\"luna-common-toast-message\"></div>"
"</div>"
"<div id=\"luna-window-menu\" class=\"luna-window-menu hidden\">"
"<div class=\"luna-window-menu-item\" onclick=\"lunaWindowMenuMinimize()\">最小化</div>"
"<div id=\"luna-window-menu-maximize\" class=\"luna-window-menu-item\" onclick=\"lunaWindowMenuMaximize()\">最大化</div>"
"<div class=\"luna-window-menu-sep\"></div>"
"<div class=\"luna-window-menu-item\" onclick=\"lunaWindowMenuClose()\">閉じる</div>"
"</div>";

typedef struct LunaWindowState {
    int bound;
    LunaWindowConfig config;
    int root, titlebar, title, close_btn, min_btn, max_btn;
    int resize_ids[8];
    int dialog_overlay, dialog_title, dialog_message, dialog_input;
    int dialog_cancel, dialog_ok;
    int toast, toast_title, toast_message;
    int window_menu, window_menu_maximize;
    double toast_remaining;
    LunaWindowDialogCallback dialog_callback;
    void* dialog_userdata;
    int prompt_active;
    LunaWindowDialogKind dialog_kind;
    char system_theme_path[LUNA_WINDOW_PATH_MAX];
    double system_theme_poll_remaining;
    LunaWindowTheme applied_theme;
    int applied_theme_valid;
} LunaWindowState;

static LunaWindowState luna_window_state;

static void luna_window_state_reset(void) {
    memset(&luna_window_state,0,sizeof(luna_window_state));
    luna_window_state.root=-1;
    luna_window_state.titlebar=-1;
    luna_window_state.title=-1;
    luna_window_state.close_btn=-1;
    luna_window_state.min_btn=-1;
    luna_window_state.max_btn=-1;
    for(int i=0;i<8;i++)luna_window_state.resize_ids[i]=-1;
    luna_window_state.dialog_overlay=-1;
    luna_window_state.dialog_title=-1;
    luna_window_state.dialog_message=-1;
    luna_window_state.dialog_input=-1;
    luna_window_state.dialog_cancel=-1;
    luna_window_state.dialog_ok=-1;
    luna_window_state.toast=-1;
    luna_window_state.toast_title=-1;
    luna_window_state.toast_message=-1;
    luna_window_state.window_menu=-1;
    luna_window_state.window_menu_maximize=-1;
}

static int luna_window_id(const char* id) { return id && id[0] ? luna_get_element_by_id(id) : -1; }
static int luna_window_is_descendant(int idx, int ancestor) {
    while (idx >= 0) {
        if (idx == ancestor) return 1;
        idx = luna_element_parent(idx);
    }
    return 0;
}
static int luna_window_notify_kind(LunaWindowDialogKind kind) {
    return kind == LUNA_WINDOW_DIALOG_ERROR ? LUNA_NOTIFY_ERROR :
           kind == LUNA_WINDOW_DIALOG_WARNING ? LUNA_NOTIFY_WARNING :
           kind == LUNA_WINDOW_DIALOG_SUCCESS ? LUNA_NOTIFY_SUCCESS : LUNA_NOTIFY_INFO;
}
static void luna_window_control_close(LunaElement* e) { (void)e; luna_platform_request_close(); }
static void luna_window_control_minimize(LunaElement* e) { (void)e; luna_platform_iconify(); }
static void luna_window_control_maximize(LunaElement* e) { (void)e; luna_platform_maximize_toggle(); }

static void luna_window_menu_hide(void) {
    if (luna_window_state.window_menu < 0) return;
    luna_add_class(luna_window_state.window_menu, "hidden");
    luna_mark_layout_dirty();
    luna_platform_request_redraw();
}

static void luna_window_menu_show_at(double mx, double my) {
    LunaElement* e;
    float menu_w = 188.0f, menu_h = 104.0f;
    float x, y, ww, hh;
    if (luna_window_state.window_menu < 0) return;
    e = luna_element_at(luna_window_state.window_menu);
    if (!e) return;
    if (e->w > 1.0f) menu_w = e->w;
    if (e->h > 1.0f) menu_h = e->h;
    ww = luna_window_width > 1.0f ? luna_window_width : 800.0f;
    hh = luna_window_height > 1.0f ? luna_window_height : 600.0f;
    x = (float)mx;
    y = (float)my;
    if (x + menu_w > ww - 4.0f) x = ww - menu_w - 4.0f;
    if (y + menu_h > hh - 4.0f) y = hh - menu_h - 4.0f;
    if (x < 4.0f) x = 4.0f;
    if (y < 4.0f) y = 4.0f;
    snprintf(e->inline_style, sizeof(e->inline_style), "left:%.0fpx;top:%.0fpx;", x, y);
    e->has_inline_style = 1;
    luna_update_element_style(luna_window_state.window_menu);
    if (luna_window_state.window_menu_maximize >= 0)
        luna_set_text(luna_window_state.window_menu_maximize, "最大化");
    luna_remove_class(luna_window_state.window_menu, "hidden");
    luna_consume_pointer_event();
    luna_mark_layout_dirty();
    luna_platform_request_redraw();
}

static void luna_window_menu_minimize(LunaElement* e) {
    (void)e;
    luna_window_menu_hide();
    luna_platform_iconify();
}
static void luna_window_menu_maximize(LunaElement* e) {
    (void)e;
    luna_window_menu_hide();
    luna_platform_maximize_toggle();
}
static void luna_window_menu_close(LunaElement* e) {
    (void)e;
    luna_window_menu_hide();
    luna_platform_request_close();
}

static void luna_window_dialog_finish(LunaWindowDialogResult result) {
    char input_copy[512] = {0};
    if (luna_window_state.prompt_active && luna_window_state.dialog_input >= 0) {
        const char* v = luna_get_value(luna_window_state.dialog_input);
        if (v) snprintf(input_copy, sizeof(input_copy), "%s", v);
    }
    if (luna_window_state.dialog_overlay >= 0) {
        luna_add_class(luna_window_state.dialog_overlay, "hidden");
        luna_pop_focus_trap(luna_window_state.dialog_overlay);
    }
    LunaWindowDialogCallback callback = luna_window_state.dialog_callback;
    void* userdata = luna_window_state.dialog_userdata;
    luna_window_state.dialog_callback = NULL;
    luna_window_state.dialog_userdata = NULL;
    luna_window_state.prompt_active = 0;
    if (callback) callback(result, input_copy, userdata);
    luna_platform_request_redraw();
}
static void luna_window_dialog_ok_handler(LunaElement* e) { (void)e; luna_window_dialog_finish(LUNA_WINDOW_RESULT_OK); }
static void luna_window_dialog_cancel_handler(LunaElement* e) { (void)e; luna_window_dialog_finish(LUNA_WINDOW_RESULT_CANCEL); }
static void luna_window_toast_close_handler(LunaElement* e) { (void)e; if (luna_window_state.toast >= 0) luna_add_class(luna_window_state.toast, "hidden"); luna_window_state.toast_remaining = 0; luna_platform_request_redraw(); }

static int luna_window_hit_is_interactive(int hit) {
    LunaElement* e;
    if (hit < 0) return 0;
    e = luna_element_at(hit);
    if (!e) return 0;
    if (e->on_click || e->is_input) return 1;
    if (!strcmp(e->type, "button") || !strcmp(e->type, "a") ||
        !strcmp(e->type, "input") || !strcmp(e->type, "textarea") ||
        !strcmp(e->type, "select")) return 1;
    return 0;
}

static void luna_window_press_hook(int hit, int button, int mods) {
    (void)mods;
    /* Dismiss the local window menu on any outside press. */
    if (luna_window_state.window_menu >= 0) {
        LunaElement* menu = luna_element_at(luna_window_state.window_menu);
        if (menu && !element_has_class(menu, "hidden")) {
            if (!luna_window_is_descendant(hit, luna_window_state.window_menu)) {
                luna_window_menu_hide();
                if (!(button == LUNA_MOUSE_BUTTON_RIGHT &&
                      luna_window_is_descendant(hit, luna_window_state.titlebar))) {
                    return;
                }
                /* Else fall through and reopen at the new point. */
            } else if (button == LUNA_MOUSE_BUTTON_LEFT) {
                /* Let menu item on_click run on release. */
                return;
            }
        }
    }
    if (hit < 0) return;
    if (button == LUNA_MOUSE_BUTTON_RIGHT) {
        /* Titlebar right-click → Luna shell menu, else in-app fallback (LXDE/X11). */
        if (!luna_window_is_descendant(hit, luna_window_state.titlebar)) return;
        if (luna_window_is_descendant(hit, luna_window_state.close_btn) ||
            luna_window_is_descendant(hit, luna_window_state.min_btn) ||
            luna_window_is_descendant(hit, luna_window_state.max_btn)) return;
        {
            double mx = 0.0, my = 0.0;
            luna_get_pointer(&mx, &my);
            /* luna_linux_show_window_menu lives in luna_linux.h; custom hosts
             * that set LUNA_UI_NO_PLATFORM (e.g. luna-shell) skip it and use
             * the in-app fallback menu below. */
#if (defined(__linux__) || defined(__FreeBSD__)) && !defined(LUNA_UI_NO_PLATFORM)
            if (luna_linux_show_window_menu((int)mx, (int)my)) {
                luna_consume_pointer_event();
                return;
            }
#endif
            luna_window_menu_show_at(mx, my);
        }
        return;
    }
    if (button != LUNA_MOUSE_BUTTON_LEFT) return;
    if (luna_window_is_descendant(hit, luna_window_state.close_btn) ||
        luna_window_is_descendant(hit, luna_window_state.min_btn) ||
        luna_window_is_descendant(hit, luna_window_state.max_btn)) return;
    /* Toolbar titlebars host Open/Save buttons.  Starting a compositor move
     * grab on those hits steals the click and makes Open appear broken. */
    if (luna_window_hit_is_interactive(hit)) return;
    if (luna_window_is_descendant(hit, luna_window_state.titlebar)) {
        luna_platform_begin_move();
        return;
    }
    static const int edges[8] = {
        LUNA_RESIZE_EDGE_LEFT, LUNA_RESIZE_EDGE_RIGHT,
        LUNA_RESIZE_EDGE_TOP, LUNA_RESIZE_EDGE_BOTTOM,
        LUNA_RESIZE_EDGE_TOP_LEFT, LUNA_RESIZE_EDGE_TOP_RIGHT,
        LUNA_RESIZE_EDGE_BOTTOM_LEFT, LUNA_RESIZE_EDGE_BOTTOM_RIGHT
    };
    for (int i = 0; i < 8; ++i) {
        if (luna_window_is_descendant(hit, luna_window_state.resize_ids[i])) {
            luna_platform_begin_resize(edges[i]);
            return;
        }
    }
}

const char* luna_window_standard_css(void) { return luna_window_css_text; }
const char* luna_window_standard_titlebar_html(void) { return luna_window_titlebar_html_text; }
const char* luna_window_standard_overlay_html(void) { return luna_window_overlay_html_text; }

static void luna_window_theme_fill_default(LunaWindowTheme* theme, LunaWindowThemeMode mode) {
    if (!theme) return;
    memset(theme, 0, sizeof(*theme));
    if (mode != LUNA_WINDOW_THEME_DARK && mode != LUNA_WINDOW_THEME_LIGHT)
        mode = LUNA_WINDOW_THEME_LIGHT;
    theme->mode = mode;
    int dark = mode == LUNA_WINDOW_THEME_DARK;
    theme->window_background = dark ? 0xff111722u : 0xfff3f5f8u;
    theme->surface = dark ? 0xff192230u : 0xfffafbfdu;
    theme->surface_secondary = dark ? 0xff222e3du : 0xffedf1f5u;
    theme->border = dark ? 0xff42536au : 0xffc8d2deu;
    theme->text = dark ? 0xffedf2f7u : 0xff202633u;
    theme->text_secondary = dark ? 0xffb4c0ceu : 0xff5d6b7du;
    theme->accent = 0xff237fe5u;
    theme->accent_hover = 0xff176bc5u;
    theme->danger = 0xffd94a55u;
    theme->titlebar_active = theme->surface;
    theme->titlebar_inactive = theme->surface_secondary;
    theme->titlebar_frame = theme->border;
    theme->titlebar_height = 42.0f;
    theme->corner_radius = 12.0f;
    theme->dialog_radius = 15.0f;
    theme->control_size = 13.0f;
    theme->titlebar_style = 0;
    theme->controls_on_left = 1;
}

static uint32_t luna_window_parse_color(const char* value, uint32_t fallback) {
    if (!value || !*value) return fallback;
    while (*value == ' ' || *value == '\t') value++;
    if (*value == '#') value++;
    if (value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) value += 2;
    char* end = NULL;
    unsigned long v = strtoul(value, &end, 16);
    if (!end || end == value) return fallback;
    size_t digits = (size_t)(end - value);
    if (digits <= 6) return 0xff000000u | (uint32_t)(v & 0xffffffu);
    return (uint32_t)v;
}

static void luna_window_trim(char* text) {
    if (!text) return;
    char* p = text;
    while (*p == ' ' || *p == '\t') p++;
    if (p != text) memmove(text, p, strlen(p) + 1);
    size_t n = strlen(text);
    while (n && (text[n-1] == ' ' || text[n-1] == '\t' || text[n-1] == '\r' || text[n-1] == '\n'))
        text[--n] = 0;
}

static int luna_window_system_theme_path(char* out, size_t n) {
    if (!out || n == 0) return 0;
    const char* explicit_path = getenv("LUNA_WINDOW_THEME_FILE");
    if (explicit_path && *explicit_path) {
        int z = snprintf(out, n, "%s", explicit_path);
        return z >= 0 && (size_t)z < n;
    }
    const char* runtime = getenv("XDG_RUNTIME_DIR");
    if (runtime && *runtime) {
        int z = snprintf(out, n, "%s/luna-shell/window-theme.conf", runtime);
        if (z >= 0 && (size_t)z < n) return 1;
    }
    const char* config = getenv("XDG_CONFIG_HOME");
    const char* home = getenv("HOME");
    if (config && *config) {
        int z = snprintf(out, n, "%s/luna-shell/window-theme.conf", config);
        return z >= 0 && (size_t)z < n;
    }
    if (home && *home) {
        int z = snprintf(out, n, "%s/.config/luna-shell/window-theme.conf", home);
        return z >= 0 && (size_t)z < n;
    }
    out[0] = 0;
    return 0;
}

int luna_window_theme_load_file(LunaWindowTheme* theme, const char* path) {
    if (!theme || !path || !*path) return 0;
    FILE* f = fopen(path, "r");
    if (!f) return 0;
    LunaWindowThemeMode mode = LUNA_WINDOW_THEME_LIGHT;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        luna_window_trim(line);
        if (!line[0] || line[0] == '#') continue;
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq++ = 0; luna_window_trim(line); luna_window_trim(eq);
        if (!strcmp(line, "mode"))
            mode = !strcmp(eq, "dark") ? LUNA_WINDOW_THEME_DARK : LUNA_WINDOW_THEME_LIGHT;
    }
    luna_window_theme_fill_default(theme, mode);
    rewind(f);
    while (fgets(line, sizeof(line), f)) {
        luna_window_trim(line);
        if (!line[0] || line[0] == '#') continue;
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq++ = 0; luna_window_trim(line); luna_window_trim(eq);
        if (!strcmp(line,"window_background")) theme->window_background=luna_window_parse_color(eq,theme->window_background);
        else if (!strcmp(line,"surface")) theme->surface=luna_window_parse_color(eq,theme->surface);
        else if (!strcmp(line,"surface_secondary")) theme->surface_secondary=luna_window_parse_color(eq,theme->surface_secondary);
        else if (!strcmp(line,"border")) theme->border=luna_window_parse_color(eq,theme->border);
        else if (!strcmp(line,"text")) theme->text=luna_window_parse_color(eq,theme->text);
        else if (!strcmp(line,"text_secondary")) theme->text_secondary=luna_window_parse_color(eq,theme->text_secondary);
        else if (!strcmp(line,"accent")) theme->accent=luna_window_parse_color(eq,theme->accent);
        else if (!strcmp(line,"accent_hover")) theme->accent_hover=luna_window_parse_color(eq,theme->accent_hover);
        else if (!strcmp(line,"danger")) theme->danger=luna_window_parse_color(eq,theme->danger);
        else if (!strcmp(line,"titlebar_active")) theme->titlebar_active=luna_window_parse_color(eq,theme->titlebar_active);
        else if (!strcmp(line,"titlebar_inactive")) theme->titlebar_inactive=luna_window_parse_color(eq,theme->titlebar_inactive);
        else if (!strcmp(line,"titlebar_frame")) theme->titlebar_frame=luna_window_parse_color(eq,theme->titlebar_frame);
        else if (!strcmp(line,"titlebar_height")) theme->titlebar_height=strtof(eq,NULL);
        else if (!strcmp(line,"corner_radius")) theme->corner_radius=strtof(eq,NULL);
        else if (!strcmp(line,"dialog_radius")) theme->dialog_radius=strtof(eq,NULL);
        else if (!strcmp(line,"control_size")) theme->control_size=strtof(eq,NULL);
        else if (!strcmp(line,"titlebar_style")) theme->titlebar_style=atoi(eq);
        else if (!strcmp(line,"controls_on_left")) theme->controls_on_left=atoi(eq)!=0;
    }
    fclose(f);
    if (theme->titlebar_style < 0) theme->titlebar_style = 0;
    if (theme->titlebar_style > 2) theme->titlebar_style = 2;
    return 1;
}

int luna_window_theme_load_system(LunaWindowTheme* theme) {
    char path[LUNA_WINDOW_PATH_MAX];
    if (luna_window_system_theme_path(path, sizeof(path)) &&
        luna_window_theme_load_file(theme, path)) return 1;
    const char* env = getenv("LUNA_WINDOW_THEME");
    const char* gtk = getenv("GTK_THEME");
    int dark = (env && (!strcmp(env,"dark") || !strcmp(env,"Dark"))) ||
               (gtk && (strstr(gtk,"dark") || strstr(gtk,"Dark")));
    luna_window_theme_fill_default(theme, dark ? LUNA_WINDOW_THEME_DARK : LUNA_WINDOW_THEME_LIGHT);
    return 0;
}

void luna_window_theme_default(LunaWindowTheme* theme, LunaWindowThemeMode mode) {
    if (!theme) return;
    if (mode == LUNA_WINDOW_THEME_SYSTEM) {
        luna_window_theme_load_system(theme);
        return;
    }
    luna_window_theme_fill_default(theme, mode);
    if (mode == LUNA_WINDOW_THEME_CUSTOM) theme->mode = LUNA_WINDOW_THEME_CUSTOM;
}

static void luna_window_color(char* out, size_t n, uint32_t c) {
    unsigned a=(c>>24)&255u,r=(c>>16)&255u,g=(c>>8)&255u,b=c&255u;
    snprintf(out,n,"rgba(%u,%u,%u,%.3f)",r,g,b,(double)a/255.0);
}
void luna_window_theme_apply(const LunaWindowTheme* theme) {
    if (!theme) return;
    luna_window_state.applied_theme=*theme;
    luna_window_state.applied_theme_valid=1;
    if (luna_window_state.root >= 0) {
        const char* mode_class = theme->mode == LUNA_WINDOW_THEME_DARK ? "luna-theme-dark" :
                                 theme->mode == LUNA_WINDOW_THEME_CUSTOM ? "luna-theme-custom" : "luna-theme-light";
        const char* side_class = theme->controls_on_left ? "luna-controls-left" : "luna-controls-right";
        const char* style_class = theme->titlebar_style == 2 ? "luna-titlebar-retro" :
                                  theme->titlebar_style == 1 ? "luna-titlebar-classic" : "luna-titlebar-modern";
        char classes[128];
        snprintf(classes,sizeof(classes),"%s %s %s",mode_class,side_class,style_class);
        luna_update_classes(luna_window_state.root,
            "luna-theme-light luna-theme-dark luna-theme-custom luna-controls-left luna-controls-right luna-titlebar-modern luna-titlebar-classic luna-titlebar-retro",
            classes);
    }
    char bg[48],surface[48],border[48],text[48],secondary[48],accent[48],accent_hover[48],danger[48],titlebar[48],titlebar_inactive[48],frame[48];
    char css[3072];
    luna_window_color(bg,sizeof(bg),theme->window_background);
    luna_window_color(surface,sizeof(surface),theme->surface);
    luna_window_color(border,sizeof(border),theme->border);
    luna_window_color(text,sizeof(text),theme->text);
    luna_window_color(secondary,sizeof(secondary),theme->text_secondary);
    luna_window_color(accent,sizeof(accent),theme->accent);
    luna_window_color(accent_hover,sizeof(accent_hover),theme->accent_hover);
    luna_window_color(danger,sizeof(danger),theme->danger);
    luna_window_color(titlebar,sizeof(titlebar),theme->titlebar_active);
    luna_window_color(titlebar_inactive,sizeof(titlebar_inactive),theme->titlebar_inactive);
    luna_window_color(frame,sizeof(frame),theme->titlebar_frame);
    /* Root fill/text must not clobber application stylesheets.  Apps that share
     * the chrome root (class="app luna-window-root") author their own
     * background/color; theme_apply runs after those sheets and would otherwise
     * flatten gradients or wipe transparency.  Restrict the surface fill to
     * chrome-only roots via :not(.app).  Titlebar/dialog/control tokens still
     * apply under either root shape. */
    snprintf(css,sizeof(css),
        ".luna-window-root:not(.app){background:%s;color:%s;}"
        ".luna-window-root .luna-titlebar{height:%.1fpx;min-height:%.1fpx;background:%s;border-bottom-color:%s;}"
        ".luna-window-root.luna-window-inactive .luna-titlebar{background:%s;}"
        ".luna-window-root .luna-window-control{width:%.1fpx;height:%.1fpx;}"
        ".luna-window-root .luna-common-dialog{background:%s;border-color:%s;border-radius:%.1fpx;color:%s;}"
        ".luna-window-root .luna-common-dialog-message{color:%s;}"
        ".luna-window-root .luna-common-dialog-input{border-color:%s;}"
        ".luna-window-root .luna-common-button.primary{background:%s;}"
        ".luna-window-root .luna-common-button.primary:hover{background:%s;}"
        ".luna-window-root .luna-common-button.danger{background:%s;}"
        ".luna-window-root{border-radius:%.1fpx;}",
        bg,text,theme->titlebar_height,theme->titlebar_height,titlebar,frame,titlebar_inactive,
        theme->control_size,theme->control_size,surface,border,theme->dialog_radius,text,
        secondary,border,accent,accent_hover,danger,theme->corner_radius);
    luna_parse_css(css);
    luna_mark_layout_dirty();
    luna_platform_request_redraw();
}

void luna_window_bind(const LunaWindowConfig* config) {
    luna_set_mouse_press_hook(NULL);
    luna_window_state_reset();
    LunaWindowConfig c;
    memset(&c, 0, sizeof(c));
    if (config) c = *config;
    if (!c.root_id) c.root_id = "luna-window-root";
    if (!c.titlebar_id) c.titlebar_id = "luna-titlebar";
    if (!c.title_id) c.title_id = "luna-window-title";
    if (!c.close_id) c.close_id = "luna-window-close";
    if (!c.minimize_id) c.minimize_id = "luna-window-minimize";
    if (!c.maximize_id) c.maximize_id = "luna-window-maximize";
    if (!c.resize_left_id) c.resize_left_id = "luna-resize-left";
    if (!c.resize_right_id) c.resize_right_id = "luna-resize-right";
    if (!c.resize_top_id) c.resize_top_id = "luna-resize-top";
    if (!c.resize_bottom_id) c.resize_bottom_id = "luna-resize-bottom";
    if (!c.resize_top_left_id) c.resize_top_left_id = "luna-resize-tl";
    if (!c.resize_top_right_id) c.resize_top_right_id = "luna-resize-tr";
    if (!c.resize_bottom_left_id) c.resize_bottom_left_id = "luna-resize-bl";
    if (!c.resize_bottom_right_id) c.resize_bottom_right_id = "luna-resize-br";
    luna_window_state.config = c;
    luna_window_state.root = luna_window_id(c.root_id);
    luna_window_state.titlebar = luna_window_id(c.titlebar_id);
    luna_window_state.title = luna_window_id(c.title_id);
    luna_window_state.close_btn = luna_window_id(c.close_id);
    luna_window_state.min_btn = luna_window_id(c.minimize_id);
    luna_window_state.max_btn = luna_window_id(c.maximize_id);
    const char* resize_ids[8] = {c.resize_left_id,c.resize_right_id,c.resize_top_id,c.resize_bottom_id,c.resize_top_left_id,c.resize_top_right_id,c.resize_bottom_left_id,c.resize_bottom_right_id};
    for (int i=0;i<8;i++) luna_window_state.resize_ids[i]=luna_window_id(resize_ids[i]);
    luna_window_state.dialog_overlay=luna_window_id("luna-common-dialog-overlay");
    luna_window_state.dialog_title=luna_window_id("luna-common-dialog-title");
    luna_window_state.dialog_message=luna_window_id("luna-common-dialog-message");
    luna_window_state.dialog_input=luna_window_id("luna-common-dialog-input");
    luna_window_state.dialog_cancel=luna_window_id("luna-common-dialog-cancel");
    luna_window_state.dialog_ok=luna_window_id("luna-common-dialog-ok");
    luna_window_state.toast=luna_window_id("luna-common-toast");
    luna_window_state.toast_title=luna_window_id("luna-common-toast-title");
    luna_window_state.toast_message=luna_window_id("luna-common-toast-message");
    luna_window_state.window_menu=luna_window_id("luna-window-menu");
    luna_window_state.window_menu_maximize=luna_window_id("luna-window-menu-maximize");
    luna_register_js_handler("lunaWindowClose",luna_window_control_close);
    luna_register_js_handler("lunaWindowMinimize",luna_window_control_minimize);
    luna_register_js_handler("lunaWindowMaximize",luna_window_control_maximize);
    luna_register_js_handler("lunaWindowMenuMinimize",luna_window_menu_minimize);
    luna_register_js_handler("lunaWindowMenuMaximize",luna_window_menu_maximize);
    luna_register_js_handler("lunaWindowMenuClose",luna_window_menu_close);
    luna_register_js_handler("lunaWindowDialogOk",luna_window_dialog_ok_handler);
    luna_register_js_handler("lunaWindowDialogCancel",luna_window_dialog_cancel_handler);
    luna_register_js_handler("lunaWindowToastClose",luna_window_toast_close_handler);
    if (luna_window_state.close_btn >= 0) luna_set_on_click(luna_window_state.close_btn,luna_window_control_close);
    int client_chrome=c.chrome==LUNA_WINDOW_CHROME_CLIENT||
        (c.chrome==LUNA_WINDOW_CHROME_AUTO&&luna_window_state.titlebar>=0);
    if(client_chrome)luna_set_mouse_press_hook(luna_window_press_hook);
    else{
        if(luna_window_state.titlebar>=0)luna_add_class(luna_window_state.titlebar,"hidden");
        for(int i=0;i<8;i++)if(luna_window_state.resize_ids[i]>=0)luna_add_class(luna_window_state.resize_ids[i],"hidden");
    }
    luna_window_state.bound=1;
    if (c.title) luna_window_set_title(c.title);
    LunaWindowTheme theme; luna_window_theme_default(&theme,c.theme); luna_window_theme_apply(&theme);
    luna_window_state.system_theme_path[0]=0;
    luna_window_state.system_theme_poll_remaining=1.0;
    if(c.theme==LUNA_WINDOW_THEME_SYSTEM)
        (void)luna_window_system_theme_path(luna_window_state.system_theme_path,sizeof(luna_window_state.system_theme_path));
}
void luna_window_unbind(void) { luna_set_mouse_press_hook(NULL); luna_window_state_reset(); }
void luna_window_render_floating(int fbw, int fbh) {
    float ww, hh;
    if (!luna_window_state.bound || fbw <= 0 || fbh <= 0) return;
    ww = luna_window_width > 1.0f ? luna_window_width : (float)fbw;
    hh = luna_window_height > 1.0f ? luna_window_height : (float)fbh;
    /* Paint in CSS z-index order: dialog (5000) < toast (5200) < menu (5400). */
    if (luna_window_state.dialog_overlay >= 0 &&
        luna_element_visible(luna_window_state.dialog_overlay))
        luna_render_region(luna_window_state.dialog_overlay, fbw, fbh, 0, 0, ww, hh);
    if (luna_window_state.toast >= 0 &&
        luna_element_visible(luna_window_state.toast))
        luna_render_region(luna_window_state.toast, fbw, fbh, 0, 0, ww, hh);
    if (luna_window_state.window_menu >= 0 &&
        luna_element_visible(luna_window_state.window_menu))
        luna_render_region(luna_window_state.window_menu, fbw, fbh, 0, 0, ww, hh);
}
void luna_window_tick(double dt) {
    if(!luna_window_state.bound)return;
    if (dt < 0) dt = 0;
    if(luna_window_state.config.theme==LUNA_WINDOW_THEME_SYSTEM&&luna_window_state.system_theme_path[0]){
        luna_window_state.system_theme_poll_remaining-=dt;
        if(luna_window_state.system_theme_poll_remaining<=0){
            luna_window_state.system_theme_poll_remaining=1.0;
            LunaWindowTheme theme;
            if(luna_window_theme_load_file(&theme,luna_window_state.system_theme_path)&&
               (!luna_window_state.applied_theme_valid||memcmp(&theme,&luna_window_state.applied_theme,sizeof(theme))!=0))
                luna_window_theme_apply(&theme);
        }
    }
    if (luna_window_state.toast_remaining > 0) {
        luna_window_state.toast_remaining -= dt;
        if (luna_window_state.toast_remaining <= 0 && luna_window_state.toast >= 0) {
            luna_add_class(luna_window_state.toast,"hidden");
            luna_window_state.toast_remaining=0;
            luna_platform_request_redraw();
        }
    }
}
void luna_window_set_title(const char* title) {
    if (luna_window_state.title >= 0) luna_set_text(luna_window_state.title,title?title:"");
    luna_platform_set_title(title?title:"");
    luna_platform_request_redraw();
}
static void luna_window_show_dialog(LunaWindowDialogKind kind,const char* title,const char* message,const char* initial,LunaWindowDialogCallback cb,void*ud,int prompt,int cancel) {
    if (!luna_window_state.bound || luna_window_state.dialog_overlay < 0) return;
    luna_window_state.dialog_kind=kind;luna_window_state.dialog_callback=cb;luna_window_state.dialog_userdata=ud;luna_window_state.prompt_active=prompt;
    if(luna_window_state.dialog_title>=0)luna_set_text(luna_window_state.dialog_title,title?title:"");
    if(luna_window_state.dialog_message>=0)luna_set_text(luna_window_state.dialog_message,message?message:"");
    if(luna_window_state.dialog_input>=0){if(prompt){luna_remove_class(luna_window_state.dialog_input,"hidden");luna_set_value(luna_window_state.dialog_input,initial?initial:"");}else luna_add_class(luna_window_state.dialog_input,"hidden");}
    if(luna_window_state.dialog_cancel>=0){if(cancel)luna_remove_class(luna_window_state.dialog_cancel,"hidden");else luna_add_class(luna_window_state.dialog_cancel,"hidden");}
    if(luna_window_state.dialog_ok>=0){luna_set_text(luna_window_state.dialog_ok,"OK");luna_update_classes(luna_window_state.dialog_ok,"danger",kind==LUNA_WINDOW_DIALOG_ERROR?"danger":"");}
    luna_remove_class(luna_window_state.dialog_overlay,"hidden");luna_push_focus_trap(luna_window_state.dialog_overlay,NULL,0);if(prompt&&luna_window_state.dialog_input>=0)luna_focus_element(luna_window_state.dialog_input);luna_mark_layout_dirty();luna_platform_request_redraw();
}
void luna_window_alert(LunaWindowDialogKind kind,const char*title,const char*message){luna_window_show_dialog(kind,title,message,NULL,NULL,NULL,0,0);}
void luna_window_confirm(LunaWindowDialogKind kind,const char*title,const char*message,LunaWindowDialogCallback cb,void*ud){luna_window_show_dialog(kind,title,message,NULL,cb,ud,0,1);}
void luna_window_prompt(const char*title,const char*message,const char*initial,LunaWindowDialogCallback cb,void*ud){luna_window_show_dialog(LUNA_WINDOW_DIALOG_QUESTION,title,message,initial,cb,ud,1,1);}
void luna_window_dialog_close(void){luna_window_dialog_finish(LUNA_WINDOW_RESULT_CANCEL);}
void luna_window_toast(LunaWindowDialogKind kind,const char*title,const char*message,double seconds){(void)kind;if(!luna_window_state.bound||luna_window_state.toast<0)return;if(luna_window_state.toast_title>=0)luna_set_text(luna_window_state.toast_title,title?title:"");if(luna_window_state.toast_message>=0)luna_set_text(luna_window_state.toast_message,message?message:"");luna_remove_class(luna_window_state.toast,"hidden");luna_window_state.toast_remaining=seconds>0?seconds:3.0;luna_platform_request_redraw();}
int luna_window_notify(const char*app_name,LunaWindowDialogKind kind,const char*title,const char*message,double fallback_seconds){if(luna_platform_system_notify(app_name,luna_window_notify_kind(kind),title,message))return 1;luna_window_toast(kind,title,message,fallback_seconds);return 0;}

/* ------------------------------------------------------------------------- */
/* Standard file-dialog launcher                                             */
/* ------------------------------------------------------------------------- */

#if defined(_WIN32)
#define LUNA_WINDOW_POPEN  _popen
#define LUNA_WINDOW_PCLOSE _pclose
#else
#include <unistd.h>
#define LUNA_WINDOW_POPEN  popen
#define LUNA_WINDOW_PCLOSE pclose
#endif

LunaFileDialogBackend luna_file_dialog_backend_from_string(const char* name) {
    if (!name || !*name || !strcmp(name,"auto") || !strcmp(name,"system"))
        return LUNA_FILE_DIALOG_BACKEND_AUTO;
    if (!strcmp(name,"luna") || !strcmp(name,"luna-fm"))
        return LUNA_FILE_DIALOG_BACKEND_LUNA;
    if (!strcmp(name,"zenity")) return LUNA_FILE_DIALOG_BACKEND_ZENITY;
    if (!strcmp(name,"kdialog")) return LUNA_FILE_DIALOG_BACKEND_KDIALOG;
    if (!strcmp(name,"yad")) return LUNA_FILE_DIALOG_BACKEND_YAD;
    return LUNA_FILE_DIALOG_BACKEND_AUTO;
}

const char* luna_file_dialog_backend_name(LunaFileDialogBackend backend) {
    switch (backend) {
        case LUNA_FILE_DIALOG_BACKEND_LUNA: return "luna";
        case LUNA_FILE_DIALOG_BACKEND_ZENITY: return "zenity";
        case LUNA_FILE_DIALOG_BACKEND_KDIALOG: return "kdialog";
        case LUNA_FILE_DIALOG_BACKEND_YAD: return "yad";
        default: return "auto";
    }
}

static int luna_window_command_exists(const char* name) {
    if (!name || !*name) return 0;
#if defined(_WIN32)
    char found[MAX_PATH];
    if (strchr(name,'\\') || strchr(name,'/'))
        return GetFileAttributesA(name) != INVALID_FILE_ATTRIBUTES;
    return SearchPathA(NULL,name,".exe",MAX_PATH,found,NULL) > 0;
#else
    if (strchr(name,'/')) return access(name,X_OK)==0;
    const char* path=getenv("PATH");
    if (!path) return 0;
    size_t nl=strlen(name);
    const char* p=path;
    while (*p) {
        const char* end=strchr(p,':');
        size_t dl=end?(size_t)(end-p):strlen(p);
        char full[LUNA_WINDOW_PATH_MAX];
        if (dl==0) {
            if (nl+3<sizeof(full)) snprintf(full,sizeof(full),"./%s",name);
            else full[0]=0;
        } else if (dl+1+nl+1<sizeof(full)) {
            memcpy(full,p,dl); full[dl]='/'; memcpy(full+dl+1,name,nl+1);
        } else full[0]=0;
        if (full[0] && access(full,X_OK)==0) return 1;
        if (!end) break;
        p=end+1;
    }
    return 0;
#endif
}

static int luna_window_cmd_raw(char* out,size_t cap,size_t* len,const char* text) {
    size_t n=text?strlen(text):0;
    if (!out || !len || *len+n+1>cap) return 0;
    if (n) memcpy(out+*len,text,n);
    *len+=n; out[*len]=0; return 1;
}

static int luna_window_cmd_quote(char* out,size_t cap,size_t* len,const char* text) {
    if (!luna_window_cmd_raw(out,cap,len,"'")) return 0;
    for (const char* p=text?text:""; *p; ++p) {
        if (*p=='\'') {
            if (!luna_window_cmd_raw(out,cap,len,"'\\''")) return 0;
        } else {
            char ch[2]={*p,0};
            if (!luna_window_cmd_raw(out,cap,len,ch)) return 0;
        }
    }
    return luna_window_cmd_raw(out,cap,len,"'");
}

static int luna_window_cmd_arg(char* out,size_t cap,size_t* len,const char* text) {
    return luna_window_cmd_raw(out,cap,len," ") &&
           luna_window_cmd_quote(out,cap,len,text?text:"");
}

static void luna_window_join_initial(char* out,size_t cap,
                                     const LunaFileDialogConfig* c) {
    const char* initial=(c&&c->initial_path&&*c->initial_path)?c->initial_path:".";
    if (c && c->mode==LUNA_FILE_DIALOG_SAVE_FILE &&
        c->suggested_name && *c->suggested_name) {
        size_t n=strlen(initial);
        snprintf(out,cap,"%s%s%s",initial,
                 (n && (initial[n-1]=='/' || initial[n-1]=='\\'))?"":"/",
                 c->suggested_name);
    } else snprintf(out,cap,"%s",initial);
}

static LunaFileDialogBackend luna_window_choose_file_backend(
    const LunaFileDialogConfig* c) {
    LunaFileDialogBackend backend=c?c->backend:LUNA_FILE_DIALOG_BACKEND_AUTO;
    if (backend==LUNA_FILE_DIALOG_BACKEND_AUTO) {
        const char* env=getenv("LUNA_FILE_DIALOG_BACKEND");
        if (env && *env) backend=luna_file_dialog_backend_from_string(env);
    }
    if (backend==LUNA_FILE_DIALOG_BACKEND_AUTO ||
        backend==LUNA_FILE_DIALOG_BACKEND_LUNA) {
#if defined(LUNA_WINDOW_FILE_DIALOG_IMPLEMENTATION) && defined(__linux__)
        return LUNA_FILE_DIALOG_BACKEND_LUNA;
#else
        return LUNA_FILE_DIALOG_BACKEND_AUTO;
#endif
    }
    if (backend==LUNA_FILE_DIALOG_BACKEND_ZENITY)
        return luna_window_command_exists("zenity")?backend:LUNA_FILE_DIALOG_BACKEND_AUTO;
    if (backend==LUNA_FILE_DIALOG_BACKEND_KDIALOG)
        return luna_window_command_exists("kdialog")?backend:LUNA_FILE_DIALOG_BACKEND_AUTO;
    if (backend==LUNA_FILE_DIALOG_BACKEND_YAD)
        return luna_window_command_exists("yad")?backend:LUNA_FILE_DIALOG_BACKEND_AUTO;
    return LUNA_FILE_DIALOG_BACKEND_AUTO;
}

#if defined(LUNA_WINDOW_FILE_DIALOG_IMPLEMENTATION) && defined(__linux__)
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

static void luna_window_internal_env(const char* name,const char* value) {
    if (value && *value) setenv(name,value,1);
    else unsetenv(name);
}

static void luna_window_internal_env_int(const char* name,int value) {
    char buf[32];
    snprintf(buf,sizeof(buf),"%d",value);
    setenv(name,buf,1);
}

static int luna_window_show_internal_luna(const LunaFileDialogConfig* c,
                                          LunaFileDialogResult* result) {
    int fd[2];
    if (pipe(fd)!=0) return -2;
    pid_t pid=fork();
    if (pid<0) { close(fd[0]); close(fd[1]); return -2; }
    if (pid==0) {
        close(fd[0]);
        if (dup2(fd[1],STDOUT_FILENO)<0) _exit(126);
        close(fd[1]);
        luna_window_internal_env("LUNA_WINDOW_INTERNAL_FILE_DIALOG_CHILD","1");
        luna_window_internal_env_int("LUNA_WINDOW_FD_MODE",c?c->mode:LUNA_FILE_DIALOG_OPEN_FILE);
        luna_window_internal_env("LUNA_WINDOW_FD_TITLE",c?c->title:NULL);
        luna_window_internal_env("LUNA_WINDOW_FD_INITIAL",c?c->initial_path:NULL);
        luna_window_internal_env("LUNA_WINDOW_FD_NAME",c?c->suggested_name:NULL);
        luna_window_internal_env("LUNA_WINDOW_FD_ACCEPT",c?c->accept_label:NULL);
        luna_window_internal_env("LUNA_WINDOW_FD_FILTER_NAME",c?c->filter_name:NULL);
        luna_window_internal_env("LUNA_WINDOW_FD_FILTER",c?c->filter_patterns:NULL);
        luna_window_internal_env_int("LUNA_WINDOW_FD_WIDTH",c?c->width:0);
        luna_window_internal_env_int("LUNA_WINDOW_FD_HEIGHT",c?c->height:0);
        luna_window_internal_env_int("LUNA_WINDOW_FD_CLIENT_CHROME",c?c->client_chrome:0);
        char exe[LUNA_WINDOW_PATH_MAX];
        ssize_t n=readlink("/proc/self/exe",exe,sizeof(exe)-1);
        if (n<=0 || (size_t)n>=sizeof(exe)-1) _exit(127);
        exe[n]=0;
        execl(exe,exe,(char*)NULL);
        _exit(127);
    }
    close(fd[1]);
    FILE* pipe=fdopen(fd[0],"r");
    if (!pipe) { close(fd[0]); while(waitpid(pid,NULL,0)<0&&errno==EINTR){} return -2; }
    char line[LUNA_WINDOW_PATH_MAX];
    int count=0;
    while (fgets(line,sizeof(line),pipe)) {
        size_t n=strlen(line);
        while(n && (line[n-1]=='\n'||line[n-1]=='\r')) line[--n]=0;
        if (!line[0]) continue;
        if (result && count<LUNA_FILE_DIALOG_MAX_RESULTS)
            snprintf(result->paths[count],sizeof(result->paths[count]),"%s",line);
        count++;
        if (!c || c->mode!=LUNA_FILE_DIALOG_OPEN_FILES) break;
    }
    fclose(pipe);
    int status=0;
    while(waitpid(pid,&status,0)<0&&errno==EINTR){}
    if (!WIFEXITED(status) || WEXITSTATUS(status)!=0) return -2;
    if (result && count>0) {
        result->accepted=1;
        result->count=count>LUNA_FILE_DIALOG_MAX_RESULTS?LUNA_FILE_DIALOG_MAX_RESULTS:count;
    }
    return 0;
}
#endif

static int luna_window_build_file_command(char* cmd,size_t cap,
    const LunaFileDialogConfig* c,LunaFileDialogBackend backend) {
    size_t len=0; cmd[0]=0;
    const char* title=(c&&c->title&&*c->title)?c->title:"ファイルを選択";
    const char* patterns=(c&&c->filter_patterns&&*c->filter_patterns)?c->filter_patterns:"*";
    const char* filter_name=(c&&c->filter_name&&*c->filter_name)?c->filter_name:"Files";
    char initial[LUNA_WINDOW_PATH_MAX];
    luna_window_join_initial(initial,sizeof(initial),c);

    if (backend==LUNA_FILE_DIALOG_BACKEND_ZENITY ||
               backend==LUNA_FILE_DIALOG_BACKEND_YAD) {
        if (!luna_window_cmd_raw(cmd,cap,&len,
                backend==LUNA_FILE_DIALOG_BACKEND_ZENITY?"zenity --file-selection":"yad --file-selection")) return 0;
        if(!luna_window_cmd_raw(cmd,cap,&len," --title=") || !luna_window_cmd_quote(cmd,cap,&len,title))return 0;
        if(!luna_window_cmd_raw(cmd,cap,&len," --filename=") || !luna_window_cmd_quote(cmd,cap,&len,initial))return 0;
        if(c && c->mode==LUNA_FILE_DIALOG_OPEN_FILES &&
           !luna_window_cmd_raw(cmd,cap,&len," --multiple --separator='|'"))return 0;
        if(c && c->mode==LUNA_FILE_DIALOG_SELECT_FOLDER &&
           !luna_window_cmd_raw(cmd,cap,&len," --directory"))return 0;
        if(c && c->mode==LUNA_FILE_DIALOG_SAVE_FILE &&
           !luna_window_cmd_raw(cmd,cap,&len," --save --confirm-overwrite"))return 0;
        if(c && c->filter_patterns && *c->filter_patterns) {
            char filter[LUNA_WINDOW_PATH_MAX];
            snprintf(filter,sizeof(filter),"%s | %s",filter_name,patterns);
            if(!luna_window_cmd_raw(cmd,cap,&len," --file-filter=") ||
               !luna_window_cmd_quote(cmd,cap,&len,filter))return 0;
        }
    } else if (backend==LUNA_FILE_DIALOG_BACKEND_KDIALOG) {
        if (c && c->mode==LUNA_FILE_DIALOG_SELECT_FOLDER)
            { if(!luna_window_cmd_raw(cmd,cap,&len,"kdialog --getexistingdirectory"))return 0; }
        else if (c && c->mode==LUNA_FILE_DIALOG_SAVE_FILE)
            { if(!luna_window_cmd_raw(cmd,cap,&len,"kdialog --getsavefilename"))return 0; }
        else { if(!luna_window_cmd_raw(cmd,cap,&len,"kdialog --getopenfilename"))return 0; }
        if(!luna_window_cmd_arg(cmd,cap,&len,initial))return 0;
        if(c && c->mode!=LUNA_FILE_DIALOG_SELECT_FOLDER &&
           c->filter_patterns && *c->filter_patterns) {
            char filter[LUNA_WINDOW_PATH_MAX];
            snprintf(filter,sizeof(filter),"%s|%s",patterns,filter_name);
            if(!luna_window_cmd_arg(cmd,cap,&len,filter))return 0;
        }
        if(c && c->mode==LUNA_FILE_DIALOG_OPEN_FILES &&
           !luna_window_cmd_raw(cmd,cap,&len," --multiple --separate-output"))return 0;
        if(!luna_window_cmd_raw(cmd,cap,&len," --title") ||
           !luna_window_cmd_arg(cmd,cap,&len,title))return 0;
    } else return 0;
    return luna_window_cmd_raw(cmd,cap,&len," 2>/dev/null");
}

int luna_file_dialog_show(const LunaFileDialogConfig* config,
                          LunaFileDialogResult* result) {
    LunaFileDialogConfig c;
    memset(&c,0,sizeof(c));
    if (config) c=*config;
    if (c.mode==LUNA_FILE_DIALOG_MANAGER) c.mode=LUNA_FILE_DIALOG_OPEN_FILE;
    if (result) memset(result,0,sizeof(*result));
    LunaFileDialogBackend backend=luna_window_choose_file_backend(&c);
    if (backend==LUNA_FILE_DIALOG_BACKEND_AUTO) return -1;
#if defined(LUNA_WINDOW_FILE_DIALOG_IMPLEMENTATION) && defined(__linux__)
    if (backend==LUNA_FILE_DIALOG_BACKEND_LUNA)
        return luna_window_show_internal_luna(&c,result);
#endif
    char command[LUNA_WINDOW_PATH_MAX*3];
    if (!luna_window_build_file_command(command,sizeof(command),&c,backend)) return -2;
    FILE* pipe=LUNA_WINDOW_POPEN(command,"r");
    if (!pipe) return -2;
    char line[LUNA_WINDOW_PATH_MAX];
    int count=0;
    while (fgets(line,sizeof(line),pipe)) {
        size_t n=strlen(line);
        while(n && (line[n-1]=='\n'||line[n-1]=='\r')) line[--n]=0;
        if (!line[0]) continue;
        char* cursor=line;
        while (*cursor) {
            char* separator=(c.mode==LUNA_FILE_DIALOG_OPEN_FILES)?strchr(cursor,'|'):NULL;
            if (separator) *separator=0;
            if (*cursor) {
                if (result && count<LUNA_FILE_DIALOG_MAX_RESULTS)
                    snprintf(result->paths[count],sizeof(result->paths[count]),"%s",cursor);
                count++;
            }
            if (!separator || c.mode!=LUNA_FILE_DIALOG_OPEN_FILES) break;
            cursor=separator+1;
        }
        if (c.mode!=LUNA_FILE_DIALOG_OPEN_FILES) break;
    }
    int status=LUNA_WINDOW_PCLOSE(pipe);
    if (status==0 && count>0 && result) {
        result->accepted=1;
        result->count=count>LUNA_FILE_DIALOG_MAX_RESULTS?LUNA_FILE_DIALOG_MAX_RESULTS:count;
    }
    return 0;
}

#undef LUNA_WINDOW_POPEN
#undef LUNA_WINDOW_PCLOSE

#endif /* LUNA_WINDOW_IMPLEMENTATION_INCLUDED */
#endif /* LUNA_WINDOW_IMPLEMENTATION */

#if defined(LUNA_WINDOW_FILE_DIALOG_IMPLEMENTATION)
#ifndef LUNA_WINDOW_FILE_DIALOG_IMPLEMENTATION_INCLUDED
#define LUNA_WINDOW_FILE_DIALOG_IMPLEMENTATION_INCLUDED
#if !defined(__linux__) && !defined(__FreeBSD__)
#error "The full Luna file-dialog implementation currently requires a POSIX desktop host."
#endif
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <dirent.h>
#include <fnmatch.h>
#include <unistd.h>

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define MAX_ITEMS       384
#define MAX_CLIP_ITEMS  256
#define MAX_HISTORY      64
#define MAX_SEARCH      256
#define MAX_OPEN_APPS     24
#define MAX_VOLUMES       16
#define TOAST_SECONDS   2.8
#define MIN_WINDOW_W     400
#define MIN_WINDOW_H     400
#define DEFAULT_WINDOW_W 700
#define DEFAULT_WINDOW_H 460
#define MODAL_INFO_PARTS 28
#define VOLUME_REFRESH_SEC 2.5

typedef enum {
    SORT_NAME = 0,
    SORT_DATE,
    SORT_SIZE
} SortMode;

typedef struct {
    char name[NAME_MAX + 1];
    char path[PATH_MAX];
    off_t size;
    time_t mtime;
    mode_t mode;
    int is_dir;
    int is_link;
    int selected;
} FileEntry;

typedef struct {
    char name[256];
    char device[PATH_MAX];
    char mount_path[PATH_MAX];
    char uuid[128];
    int can_mount;
    int can_unmount;
    int can_eject;
    int is_mounted;
    int is_removable;
} VolumeEntry;

typedef struct {
    GLFWwindow *window;
    FileEntry entries[MAX_ITEMS];
    int entry_count;
    int truncated;
    int slot_entry[MAX_ITEMS];
    int visible_count;
    int anchor_entry;
    int context_entry;
    int last_click_entry;
    double last_click_time;

    char cwd[PATH_MAX];
    char history[MAX_HISTORY][PATH_MAX];
    int history_count;
    int history_pos;

    char clip_paths[MAX_CLIP_ITEMS][PATH_MAX];
    int clip_count;
    int clip_cut;

    int grid_view;
    int show_hidden;
    SortMode sort_mode;
    int sort_desc;
    int show_sidebar;
    int show_search;
    int settings_open;
    int redraw;
    int window_width;
    int window_height;
    int font_size;       /* 0 small, 1 standard, 2 large */
    int font_family;     /* 0 auto, 1 sans, 2 serif, 3 mono, 4 custom */
    int display_density; /* 0 compact, 1 standard, 2 roomy, 3 minimal */
    int theme;           /* 0 light, 1 dark */
    int accent;          /* 0 blue, 1 purple, 2 green, 3 orange */
    int show_status;
    int show_grid_details;
    int animations;
    int startup_mode;    /* 0 home, 1 last folder, 2 process cwd */
    int resize_pending;
    int framebuffer_dirty;
    int pending_window_width;
    int pending_window_height;
    int resize_redraw_frames;

    /* Saved only when the file grid actually scrolls.  Every other path only
       reads these values when laying out or drawing the grid. */
    float grid_scroll_top;
    float grid_scroll_left;
    float grid_scroll_dest_top;
    float grid_scroll_dest_left;
    double suppress_blank_until;

    char search[MAX_SEARCH];
    char side_paths[9][PATH_MAX];
    char terminal_command[384]; /* empty: auto-detect */
    char custom_font[PATH_MAX];  /* family name or font file path */
    char last_directory[PATH_MAX];
    char modal_mime[128];

    int open_with_open;
    int open_with_target;
    int open_app_count;
    char open_app_ids[MAX_OPEN_APPS][256];
    char open_app_names[MAX_OPEN_APPS][256];
    char open_app_files[MAX_OPEN_APPS][PATH_MAX];

    VolumeEntry volumes[MAX_VOLUMES];
    int volume_count;
    int volume_context;
    double volumes_next_refresh;

    /* XDG .desktop entry editor (create or edit). */
    int desktop_edit_open;
    int desktop_edit_create; /* 1 = new launcher in cwd, 0 = edit existing */
    char desktop_edit_path[PATH_MAX];
    char desktop_edit_type[32]; /* Application | Link | Directory */
    int desktop_edit_terminal;
    char desktop_edit_extra[4096]; /* preserved unknown Desktop Entry keys / other sections */

    int modal_kind; /* 0 none, 1 new folder, 2 rename, 3 properties, 4 XDG app, 5 terminal, 6 font, 7 permanent delete */
    int modal_return_settings;
    int modal_target;
    int properties_layout_warmed;
    double toast_until;

    int id_app;
    int id_toolbar;
    int id_sidebar;
    int id_path;
    int id_search;
    int id_grid;
    int id_status;
    int id_title;
    int id_context;
    int id_modal;
    int id_modal_title;
    int id_modal_input;
    int id_modal_cancel;
    int id_modal_ok;
    int id_modal_info;
    int id_modal_info_parts[MODAL_INFO_PARTS];
    int id_open_with;
    int id_open_with_info;
    int id_open_app_list;
    int id_side_devices_label;
    int id_volume_context;
    int id_desktop_edit;
    int id_desktop_edit_title;
    int id_desktop_edit_hint;
    int id_desktop_edit_type;
    int id_desktop_edit_name;
    int id_desktop_edit_comment;
    int id_desktop_edit_exec_label;
    int id_desktop_edit_exec;
    int id_desktop_edit_icon;
    int id_desktop_edit_workdir_row;
    int id_desktop_edit_workdir;
    int id_desktop_edit_terminal_row;
    int id_desktop_edit_terminal;
    int id_desktop_edit_filename_row;
    int id_desktop_edit_filename;
    int id_toast;
    int id_btn_back;
    int id_btn_forward;
    int id_btn_paste;
    int id_btn_hidden;
    int id_btn_sort;
    int id_btn_settings;
    int id_settings;
    int id_settings_scroll;
    int id_set_sidebar;
    int id_set_search;
    int id_set_hidden;
    int id_set_view;
    int id_set_sort;
    int id_set_desc;
    int id_set_font_size;
    int id_set_font_family;
    int id_set_custom_font;
    int id_set_density;
    int id_set_theme;
    int id_set_accent;
    int id_set_status;
    int id_set_grid_details;
    int id_set_animations;
    int id_set_startup;
    int id_set_terminal;

    /* Location-bar Tab completion state. When several entries share no
       longer common prefix, repeated Tab presses cycle through the sorted
       candidates instead of only reporting their count. */
    char completion_display_dir[PATH_MAX];
    char completion_names[MAX_ITEMS][NAME_MAX + 1];
    unsigned char completion_is_dir[MAX_ITEMS];
    char completion_last_value[PATH_MAX];
    int completion_count;
    int completion_index;
} App;

static App g;
static LunaFileDialogConfig g_file_dialog_config;
static LunaFileDialogResult* g_file_dialog_result;
static int g_file_dialog_cancelled;
static char g_file_dialog_pending_save[PATH_MAX];
static GLFWcursor *g_hand_cursor, *g_cursor_ibeam, *g_cursor_crosshair;
static GLFWcursor *g_cursor_hresize, *g_cursor_vresize;
static int g_window_drag_mode;
static int g_window_resize_edges;
static double g_window_drag_anchor_x, g_window_drag_anchor_y;
static double g_window_drag_last_x, g_window_drag_last_y;
static void file_dialog_accept(void);

static void request_redraw(void) { g.redraw = 1; }
static void platform_redraw(void) { request_redraw(); }
static void refresh_sidebar_visibility(void);
static void save_settings(void);
static void show_toast(const char *fmt, ...);
static void open_settings_dialog(void);
static void hide_context(void);
static void close_open_with_dialog(void);
static void close_modal(void);
static int load_directory(const char *path);
static void request_overlay_layout(void);
static void refresh_volumes(int force);
static void render_volumes(void);
static void navigate(const char *path, int add_history);
static void hide_volume_context(void);
static int program_exists(const char *name);
static int pipe_read_program(const char *prog, char *const argv[], char *out, size_t cap);
static void trim_text(char *s);
static int run_program_wait(const char *prog, char *const argv[]);

static const char *UI_CSS =
"* { box-sizing: border-box; }\n"
"body { width:100%; height:100%; margin:0; background:#edf1f6; color:#1f2937; font-size:12px; overflow:hidden; }\n"
".hidden { display:none; }\n"
"input { font-size:12px; }\n"
".app { position:fixed; left:0; top:0; width:100%; height:100%; min-width:0; min-height:0; display:flex; flex-direction:column; background:linear-gradient(180deg,#f8fafc 0%,#edf2f7 100%); overflow:hidden; }\n"
".toolbar { width:100%; height:48px; min-height:48px; max-height:48px; flex:0 0 48px; display:flex; align-items:center; gap:5px; padding:6px 8px; background:rgba(249,250,252,.93); border-bottom:1px solid rgba(15,23,42,.12); box-shadow:0 2px 10px rgba(30,41,59,.06); overflow:hidden; }\n"
".navgroup,.viewgroup { flex-shrink:0; display:flex; align-items:center; gap:3px; padding:2px; border-radius:9px; background:rgba(100,116,139,.10); }\n"
".tool { width:29px; min-width:29px; height:29px; flex-shrink:0; display:flex; align-items:center; justify-content:center; border-radius:7px; color:#334155; cursor:pointer; font-size:14px; transition:.12s; }\n"
".tool:hover { background:rgba(255,255,255,.9); box-shadow:0 1px 5px rgba(15,23,42,.10); }\n"
".tool:active { transform:scale(.95); }\n"
".tool.disabled { opacity:.34; pointer-events:none; }\n"
".tool.on { background:#dbeafe; color:#1261c9; }\n"
".location { flex:1 1 auto; min-width:90px; height:32px; padding:6px 10px; border:1px solid rgba(100,116,139,.18); border-radius:9px; background:rgba(255,255,255,.86); color:#243244; box-shadow:inset 0 1px 2px rgba(15,23,42,.04); caret-color:#1684ff; }\n"
".location:focus { border-color:#5aa7ff; outline:2px solid rgba(58,141,255,.18); }\n"
".search { width:160px; min-width:110px; height:32px; flex:0 1 160px; padding:6px 9px; border:1px solid rgba(100,116,139,.18); border-radius:9px; background:rgba(255,255,255,.86); color:#243244; caret-color:#1684ff; }\n"
".search:focus { border-color:#5aa7ff; outline:2px solid rgba(58,141,255,.18); }\n"
".workbench { width:100%; flex:1 1 0; min-width:0; min-height:0; display:flex; overflow:hidden; }\n"
".sidebar { width:174px; min-width:174px; max-width:174px; flex:0 0 174px; display:flex; flex-direction:column; gap:2px; padding:9px 7px; background:rgba(239,244,249,.86); border-right:1px solid rgba(71,85,105,.11); overflow-y:auto; scrollbar-width:thin; }\n"
".side-label { height:20px; padding:3px 8px; color:#7b8798; font-size:10px; font-weight:700; letter-spacing:.3px; text-transform:uppercase; }\n"
".side-item { height:29px; display:flex; align-items:center; padding:5px 8px; border-radius:7px; cursor:pointer; color:#344256; white-space:nowrap; text-overflow:ellipsis; overflow:hidden; }\n"
".side-item:hover { background:rgba(255,255,255,.74); }\n"
".side-item.missing { display:none; }\n"
".side-item.hidden { display:none; }\n"
".sidebar.hidden,.search.hidden { display:none; }\n"
".side-item.active { background:#d9eaff; color:#135eaf; font-weight:700; }\n"
".side-item.volume-unmounted { opacity:.78; font-style:italic; }\n"
".side-label.hidden { display:none; }\n"
".volume-context { position:fixed; right:12px; top:120px; width:240px; display:flex; flex-direction:column; gap:1px; padding:6px; border:1px solid rgba(71,85,105,.16); border-radius:11px; background:rgba(250,252,255,.96); box-shadow:0 16px 44px rgba(15,23,42,.23); z-index:85; }\n"
".volume-context.hidden { display:none; }\n"
".main { flex:1 1 0; min-width:0; min-height:0; display:flex; flex-direction:column; background:rgba(255,255,255,.58); overflow:hidden; }\n"
".subbar { height:32px; min-height:32px; flex:0 0 32px; display:flex; align-items:center; gap:6px; padding:3px 10px; color:#64748b; border-bottom:1px solid rgba(71,85,105,.08); }\n"
".folder-title { flex:1; font-size:12px; font-weight:700; color:#334155; white-space:nowrap; text-overflow:ellipsis; overflow:hidden; }\n"
".hint { font-size:10px; color:#8a97a8; }\n"
".file-grid { width:100%; flex:1 1 0; min-width:0; min-height:0; overflow:auto; scrollbar-width:thin; padding:10px; align-content:flex-start; }\n"
".file-grid.grid { display:flex; flex-direction:row; flex-wrap:wrap; align-items:flex-start; align-content:flex-start; justify-content:flex-start; gap:6px; }\n"
".file-grid.list { display:flex; flex-direction:column; align-items:stretch; justify-content:flex-start; gap:1px; padding:6px 8px; }\n"
".file-item { cursor:pointer; transition:.10s; user-select:none; }\n"
".file-item.hidden { display:none; }\n"
".file-item.grid-item { width:108px; min-width:108px; max-width:108px; height:94px; min-height:94px; max-height:94px; flex-grow:0; flex-shrink:0; flex-basis:108px; display:flex; flex-direction:column; align-items:center; justify-content:center; gap:4px; padding:6px 5px; border-radius:10px; text-align:center; }\n"
".file-item.list-item { width:auto; min-width:0; height:32px; min-height:32px; max-height:32px; flex-grow:0; flex-shrink:0; flex-basis:32px; align-self:stretch; display:grid; grid-template-columns:31px minmax(0,1fr) 130px; align-items:center; gap:6px; padding:2px 7px; border-radius:7px; }\n"
".file-item:hover { background:rgba(219,234,254,.58); }\n"
".file-item.selected { background:#cfe5ff; color:#0b4f97; box-shadow:inset 0 0 0 1px rgba(40,126,220,.25); }\n"
".file-item.cut { opacity:.48; }\n"
".file-icon,.file-name,.file-meta { pointer-events:none; }\n"
".grid-item .file-icon { width:40px; height:38px; display:flex; align-items:center; justify-content:center; border-radius:10px; font-size:25px; color:#4b88d8; background:linear-gradient(145deg,#eaf4ff,#d6e9ff); box-shadow:0 3px 8px rgba(31,91,154,.11); }\n"
".grid-item.type-folder .file-icon { color:#d89b26; background:linear-gradient(145deg,#fff3bf,#ffe18a); }\n"
".grid-item.type-image .file-icon { color:#b54dcc; background:linear-gradient(145deg,#f8e9ff,#edd3f6); }\n"
".grid-item.type-audio .file-icon { color:#df4d76; background:linear-gradient(145deg,#ffe9f0,#ffd4e0); }\n"
".grid-item.type-video .file-icon { color:#8b5fd7; background:linear-gradient(145deg,#efe7ff,#ded0ff); }\n"
".grid-item.type-code .file-icon { color:#3b8f77; background:linear-gradient(145deg,#e2f8f0,#ccefe3); }\n"
".grid-item.type-archive .file-icon { color:#9a7040; background:linear-gradient(145deg,#f5ecdf,#ead9c2); }\n"
".grid-item.type-desktop .file-icon { color:#2f7d56; background:linear-gradient(145deg,#e5f7ee,#cfeedd); }\n"
".list-item .file-icon { width:26px; height:26px; display:flex; align-items:center; justify-content:center; border-radius:7px; background:#e8f2ff; color:#4a82c5; font-size:15px; }\n"
".list-item.type-folder .file-icon { background:#fff0b3; color:#bf8420; }\n"
".file-name { width:100%; white-space:nowrap; text-overflow:ellipsis; overflow:hidden; }\n"
".grid-item .file-name { font-size:11px; }\n"
".file-meta { color:#8a97a8; font-size:10px; white-space:nowrap; text-overflow:ellipsis; overflow:hidden; }\n"
".grid-item .file-meta { display:none; }\n"
".empty { width:100%; min-height:90px; display:flex; align-items:center; justify-content:center; color:#8a97a8; font-size:12px; pointer-events:none; }\n"
".empty.hidden { display:none; }\n"
".status { height:24px; min-height:24px; flex:0 0 24px; display:flex; align-items:center; padding:3px 10px; border-top:1px solid rgba(71,85,105,.09); background:rgba(248,250,252,.88); color:#6b788a; font-size:10px; white-space:nowrap; text-overflow:ellipsis; overflow:hidden; }\n"
".context { position:fixed; right:12px; top:82px; width:286px; display:flex; flex-direction:column; gap:1px; padding:6px; border:1px solid rgba(71,85,105,.16); border-radius:11px; background:rgba(250,252,255,.96); box-shadow:0 16px 44px rgba(15,23,42,.23); z-index:80; }\n"
".context.hidden { display:none; }\n"
".menu-item { height:28px; min-height:28px; max-height:28px; flex:0 0 28px; display:flex; align-items:center; padding:6px 8px; border-radius:6px; cursor:pointer; }\n"
".menu-item:hover { background:#dbeafe; color:#145da7; }\n"
".menu-item.danger:hover { background:#ffe2e5; color:#b4232f; }\n"
".menu-sep { height:1px; min-height:1px; max-height:1px; flex:0 0 1px; margin:3px 4px; background:rgba(71,85,105,.13); }\n"
".modal-wrap { position:fixed; left:0; top:0; width:100%; height:100%; display:flex; align-items:center; justify-content:center; padding:14px; background:rgba(15,23,42,.24); z-index:100; }\n"
".modal-wrap.hidden { display:none; }\n"
".modal-wrap.overlay-idle { visibility:hidden; }\n"
".modal { width:90%; max-width:520px; max-height:88%; overflow-y:auto; display:flex; flex-direction:column; gap:10px; padding:16px; border:1px solid rgba(255,255,255,.65); border-radius:14px; background:rgba(248,250,253,.97); box-shadow:0 22px 60px rgba(15,23,42,.30); }\n"
".modal-title { font-size:15px; font-weight:700; color:#263548; }\n"
".modal-info { color:#617085; line-height:1.45; white-space:pre-line; overflow-wrap:anywhere; }\n"
".modal-info.hidden { display:none; }\n"
".modal-info-stack { width:100%; min-height:1px; display:flex; flex-direction:column; gap:0; }\n"
".modal-wrap.properties-dialog .modal { width:94%; max-width:720px; max-height:82%; padding:0; gap:0; overflow-y:auto; scrollbar-width:thin; }\n"
".modal-wrap.properties-dialog .modal-title { padding:16px 18px 13px; border-bottom:1px solid rgba(71,85,105,.12); background:rgba(248,250,253,.98); font-size:16px; }\n"
".modal-wrap.properties-dialog .modal-info-stack { min-height:0; display:block; overflow:visible; padding:16px 20px 20px; background:rgba(255,255,255,.72); }\n"
".modal-wrap.properties-dialog .modal-info { width:100%; min-height:0; display:block; color:#455468; font-size:12px; line-height:1.72; white-space:pre-wrap; overflow-wrap:anywhere; }\n"
".modal-wrap.properties-dialog .modal-actions { padding:11px 14px; border-top:1px solid rgba(71,85,105,.12); background:rgba(248,250,253,.98); }\n"
".open-with-modal { width:92%; max-width:470px; max-height:88%; }\n"
".open-app-list { min-height:0; max-height:380px; display:flex; flex-direction:column; gap:4px; overflow-y:auto; padding:2px; scrollbar-width:thin; }\n"
".app-choice { min-height:43px; flex:0 0 auto; display:flex; align-items:center; padding:7px 10px; border:1px solid rgba(100,116,139,.13); border-radius:9px; background:rgba(255,255,255,.72); color:#334155; cursor:pointer; line-height:1.35; white-space:pre-line; overflow-wrap:anywhere; }\n"
".app-choice:hover { background:#dbeafe; color:#145da7; border-color:rgba(54,152,255,.25); }\n"
".app-choice.hidden { display:none; }\n"
".modal-input { height:34px; padding:6px 9px; border:1px solid #9eb9d7; border-radius:8px; background:white; caret-color:#1684ff; }\n"
".desktop-edit-modal { width:94%; max-width:540px; max-height:92%; gap:8px; }\n"
".desktop-edit-hint { color:#617085; font-size:11px; line-height:1.4; white-space:pre-line; }\n"
".desktop-edit-form { display:flex; flex-direction:column; gap:7px; min-height:0; overflow-y:auto; padding:2px 1px 4px; scrollbar-width:thin; }\n"
".desktop-edit-row { display:flex; flex-direction:column; gap:3px; }\n"
".desktop-edit-row.hidden { display:none; }\n"
".desktop-edit-label { color:#4b5b70; font-size:11px; font-weight:700; }\n"
".desktop-edit-input { height:32px; padding:5px 8px; border:1px solid #9eb9d7; border-radius:8px; background:white; caret-color:#1684ff; }\n"
".desktop-edit-toggle { height:32px; min-height:32px; display:flex; align-items:center; justify-content:space-between; padding:5px 10px; border:1px solid rgba(100,116,139,.18); border-radius:8px; background:rgba(255,255,255,.78); cursor:pointer; color:#334155; font-size:12px; }\n"
".desktop-edit-toggle:hover { background:#dbeafe; color:#145da7; }\n"
".desktop-edit-toggle.on { border-color:rgba(54,152,255,.35); background:#eef6ff; }\n"
".modal-actions { display:flex; justify-content:flex-end; gap:6px; }\n"
".button { min-width:72px; height:30px; display:flex; align-items:center; justify-content:center; padding:5px 10px; border-radius:8px; background:#e6ebf1; cursor:pointer; }\n"
".button:hover { filter:brightness(1.04); }\n"
".button.primary { color:white; background:linear-gradient(180deg,#3698ff,#1477e6); box-shadow:0 4px 10px rgba(20,119,230,.24); }\n"
".toast { position:fixed; left:10%; bottom:36px; width:80%; max-width:360px; min-height:34px; display:flex; align-items:center; justify-content:center; padding:7px 11px; border-radius:10px; background:rgba(26,35,49,.91); color:white; box-shadow:0 10px 30px rgba(15,23,42,.28); z-index:130; }\n"
".toast.hidden { display:none; }\n"
".settings-panel { width:90%; max-width:430px; max-height:90%; overflow-y:auto; display:flex; flex-direction:column; gap:4px; padding:12px; border:1px solid rgba(71,85,105,.16); border-radius:14px; background:#f8fafc; box-shadow:0 22px 60px rgba(15,23,42,.30); }\n"
".settings-row { height:34px; min-height:34px; display:flex; align-items:center; gap:9px; padding:5px 8px; border-radius:8px; cursor:pointer; }\n"
".settings-row:hover { background:#e7f0fb; }\n"
".settings-name { flex:1; color:#334155; font-size:12px; pointer-events:none; }\n"
".settings-state { min-width:66px; height:24px; display:flex; align-items:center; justify-content:center; border-radius:12px; background:#e3e8ee; color:#5b6878; font-size:11px; font-weight:700; pointer-events:none; }\n"
".settings-state.on { background:#d8eaff; color:#145da7; }\n"
".settings-note { min-height:34px; color:#738094; font-size:10px; line-height:1.4; }\n"
".app.font-small,.app.font-small input { font-size:11px; }\n"
".app.font-small .folder-title,.app.font-small .settings-name { font-size:11px; }\n"
".app.font-small .grid-item .file-name,.app.font-small .file-meta,.app.font-small .hint,.app.font-small .status,.app.font-small .side-label,.app.font-small .settings-note { font-size:9px; }\n"
".app.font-standard,.app.font-standard input { font-size:13px; }\n"
".app.font-standard .folder-title,.app.font-standard .settings-name { font-size:13px; }\n"
".app.font-standard .grid-item .file-name { font-size:12px; }\n"
".app.font-standard .file-meta,.app.font-standard .hint,.app.font-standard .status,.app.font-standard .side-label,.app.font-standard .settings-note { font-size:11px; }\n"
".app.font-large,.app.font-large input { font-size:15px; }\n"
".app.font-large .folder-title,.app.font-large .settings-name { font-size:15px; }\n"
".app.font-large .grid-item .file-name { font-size:14px; }\n"
".app.font-large .file-meta,.app.font-large .hint,.app.font-large .status,.app.font-large .side-label,.app.font-large .settings-note { font-size:12px; }\n"
".app.density-minimal .toolbar { height:42px; min-height:42px; max-height:42px; flex-basis:42px; padding:4px 6px; gap:4px; }\n"
".app.density-minimal .tool { width:25px; min-width:25px; height:25px; font-size:12px; border-radius:6px; }\n"
".app.density-minimal .navgroup,.app.density-minimal .viewgroup { gap:1px; padding:1px; border-radius:7px; }\n"
".app.density-minimal .location,.app.density-minimal .search { height:28px; padding:4px 7px; }\n"
".app.density-minimal .search { width:130px; min-width:86px; flex-basis:130px; }\n"
".app.density-minimal .sidebar { width:148px; min-width:148px; max-width:148px; flex-basis:148px; padding:6px 5px; }\n"
".app.density-minimal .side-label { height:17px; padding:2px 7px; }\n"
".app.density-minimal .side-item { height:25px; min-height:25px; padding:3px 7px; border-radius:6px; }\n"
".app.density-minimal .subbar { height:27px; min-height:27px; flex-basis:27px; padding:2px 8px; }\n"
".app.density-minimal .file-grid { padding:6px; }\n"
".app.density-minimal .file-grid.grid { gap:4px; }\n"
".app.density-minimal .file-item.grid-item { width:88px; min-width:88px; max-width:88px; flex-basis:88px; height:76px; min-height:76px; max-height:76px; gap:3px; padding:4px; border-radius:8px; }\n"
".app.density-minimal .grid-item .file-icon { width:30px; height:29px; font-size:19px; border-radius:8px; }\n"
".app.density-minimal .file-item.list-item { grid-template-columns:25px minmax(0,1fr) 112px; height:27px; min-height:27px; max-height:27px; flex-basis:27px; gap:5px; padding:1px 6px; border-radius:6px; }\n"
".app.density-minimal .list-item .file-icon { width:23px; height:23px; font-size:13px; border-radius:6px; }\n"
".app.density-minimal .status { height:21px; min-height:21px; flex-basis:21px; padding:2px 8px; }\n"
".app.density-standard .toolbar { height:52px; min-height:52px; max-height:52px; flex-basis:52px; padding:7px 10px; gap:6px; }\n"
".app.density-standard .tool { width:32px; min-width:32px; height:31px; }\n"
".app.density-standard .location,.app.density-standard .search { height:34px; }\n"
".app.density-standard .sidebar { width:190px; min-width:190px; max-width:190px; flex-basis:190px; padding:11px 8px; }\n"
".app.density-standard .side-item { height:32px; padding:6px 9px; }\n"
".app.density-standard .subbar { height:35px; min-height:35px; flex-basis:35px; }\n"
".app.density-standard .file-grid { padding:12px; }\n"
".app.density-standard .file-item.grid-item { width:116px; min-width:116px; max-width:116px; flex-basis:116px; height:102px; min-height:102px; max-height:102px; }\n"
".app.density-standard .file-item.list-item { height:35px; min-height:35px; max-height:35px; flex-basis:35px; }\n"
".app.density-roomy .toolbar { height:57px; min-height:57px; max-height:57px; flex-basis:57px; padding:8px 11px; gap:7px; }\n"
".app.density-roomy .tool { width:35px; min-width:35px; height:33px; }\n"
".app.density-roomy .location,.app.density-roomy .search { height:36px; }\n"
".app.density-roomy .sidebar { width:206px; min-width:206px; max-width:206px; flex-basis:206px; padding:13px 10px; }\n"
".app.density-roomy .side-item { height:34px; padding:7px 10px; }\n"
".app.density-roomy .subbar { height:38px; min-height:38px; flex-basis:38px; }\n"
".app.density-roomy .file-grid { padding:14px; }\n"
".app.density-roomy .file-item.grid-item { width:124px; min-width:124px; max-width:124px; flex-basis:124px; height:110px; min-height:110px; max-height:110px; }\n"
".app.density-roomy .file-item.list-item { height:38px; min-height:38px; max-height:38px; flex-basis:38px; }\n"
".app.window-narrow .toolbar { height:44px; min-height:44px; max-height:44px; flex-basis:44px; gap:3px; padding:5px 6px; }\n"
".app.window-narrow .tool { width:27px; min-width:27px; height:27px; font-size:13px; }\n"
".app.window-narrow .navgroup,.app.window-narrow .viewgroup { gap:1px; padding:1px; }\n"
".app.window-narrow .location { min-width:72px; height:29px; padding:5px 7px; }\n"
".app.window-narrow .search { width:120px; min-width:82px; height:29px; flex-basis:120px; padding:5px 7px; }\n"
".app.window-narrow .sidebar { width:150px; min-width:150px; max-width:150px; flex-basis:150px; padding:7px 5px; }\n"
".app.window-narrow .subbar { height:29px; min-height:29px; flex-basis:29px; padding:2px 8px; }\n"
".app.window-narrow .file-grid { padding:7px; }\n"
".app.window-narrow .file-item.grid-item { width:96px; min-width:96px; max-width:96px; flex-basis:96px; height:84px; min-height:84px; max-height:84px; padding:4px; }\n"
".app.window-narrow .grid-item .file-icon { width:36px; height:34px; font-size:22px; }\n"
".app.window-narrow .file-item.list-item { grid-template-columns:28px minmax(0,1fr) 105px; height:29px; min-height:29px; max-height:29px; flex-basis:29px; }\n"
".app.window-narrow .responsive-hide-narrow { display:none; }\n"
".app.window-tiny .search,.app.window-tiny .sidebar,.app.window-tiny .responsive-hide-tiny { display:none; }\n"
".app.window-tiny .toolbar { gap:2px; padding-left:4px; padding-right:4px; }\n"
".app.window-tiny .tool { width:26px; min-width:26px; height:26px; }\n"
".app.window-tiny .location { min-width:56px; }\n"
".app.window-tiny .hint { display:none; }\n"
".app.window-tiny .file-item.list-item { grid-template-columns:27px minmax(0,1fr); }\n"
".app.window-tiny .file-meta { display:none; }\n"
".app.window-short .sidebar { padding-top:5px; padding-bottom:5px; }\n"
".app.window-short .side-item { height:27px; min-height:27px; }\n"
".app.window-short .settings-panel { max-height:94%; }\n"
".app.density-minimal.window-narrow .toolbar { height:40px; min-height:40px; max-height:40px; flex-basis:40px; padding:3px 5px; gap:2px; }\n"
".app.density-minimal.window-narrow .tool { width:24px; min-width:24px; height:24px; font-size:12px; }\n"
".app.density-minimal.window-narrow .location { min-width:64px; height:27px; padding:4px 6px; }\n"
".app.density-minimal.window-narrow .search { width:108px; min-width:72px; height:27px; flex-basis:108px; padding:4px 6px; }\n"
".app.density-minimal.window-narrow .sidebar { width:138px; min-width:138px; max-width:138px; flex-basis:138px; padding:5px 4px; }\n"
".app.density-minimal.window-narrow .side-item { height:24px; min-height:24px; padding:3px 6px; }\n"
".app.density-minimal.window-narrow .subbar { height:26px; min-height:26px; flex-basis:26px; padding:2px 7px; }\n"
".app.density-minimal.window-narrow .file-grid { padding:5px; }\n"
".app.density-minimal.window-narrow .file-item.grid-item { width:82px; min-width:82px; max-width:82px; flex-basis:82px; height:72px; min-height:72px; max-height:72px; padding:3px; }\n"
".app.density-minimal.window-narrow .grid-item .file-icon { width:28px; height:27px; font-size:18px; }\n"
".app.density-minimal.window-narrow .file-item.list-item { grid-template-columns:24px minmax(0,1fr) 96px; height:26px; min-height:26px; max-height:26px; flex-basis:26px; }\n"
".app.density-minimal.window-tiny .file-item.list-item { grid-template-columns:24px minmax(0,1fr); }\n"
".app.density-minimal.window-narrow .status { height:20px; min-height:20px; flex-basis:20px; }\n"
".settings-panel { overflow:hidden; gap:0; padding:0; }\n"
".settings-header { height:48px; min-height:48px; flex:0 0 48px; display:flex; align-items:center; gap:8px; padding:10px 14px; border-bottom:1px solid rgba(71,85,105,.12); background:rgba(248,250,252,.98); }\n"
".settings-header .modal-title { flex:1; }\n"
".settings-shortcut { color:#8793a4; font-size:10px; }\n"
".settings-scroll { flex:1 1 auto; min-height:0; overflow-y:auto; padding:10px 12px 12px; scrollbar-width:thin; }\n"
".settings-section { height:25px; display:flex; align-items:flex-end; padding:5px 8px 3px; color:#7b8798; font-size:10px; font-weight:700; letter-spacing:.35px; text-transform:uppercase; }\n"
".settings-footer { height:52px; min-height:52px; flex:0 0 52px; display:flex; align-items:center; justify-content:space-between; gap:8px; padding:10px 12px; border-top:1px solid rgba(71,85,105,.12); background:rgba(248,250,252,.98); }\n"
".settings-footer-right { display:flex; gap:6px; }\n"
".button.subtle { min-width:84px; background:transparent; color:#64748b; }\n"
".app.hide-status .status { display:none; }\n"
".app.grid-details .grid-item .file-meta { display:block; width:100%; font-size:9px; line-height:1.15; text-align:center; }\n"
".app.grid-details .file-item.grid-item { height:112px; min-height:112px; max-height:112px; }\n"
".app.grid-details.density-minimal .file-item.grid-item { height:91px; min-height:91px; max-height:91px; }\n"
".app.grid-details.density-standard .file-item.grid-item { height:122px; min-height:122px; max-height:122px; }\n"
".app.grid-details.density-roomy .file-item.grid-item { height:130px; min-height:130px; max-height:130px; }\n"
".app.grid-details.window-narrow .file-item.grid-item { height:102px; min-height:102px; max-height:102px; }\n"
".app.grid-details.density-minimal.window-narrow .file-item.grid-item { height:86px; min-height:86px; max-height:86px; }\n"
".app.no-motion .tool,.app.no-motion .file-item,.app.no-motion .button,.app.no-motion .settings-row { transition:0s; }\n"
".app.theme-dark { background:linear-gradient(180deg,#121923 0%,#0d131c 100%); color:#e5eaf0; }\n"
".app.theme-dark .toolbar { background:rgba(22,30,42,.96); border-bottom-color:rgba(148,163,184,.15); box-shadow:0 2px 12px rgba(0,0,0,.22); }\n"
".app.theme-dark .navgroup,.app.theme-dark .viewgroup { background:rgba(148,163,184,.10); }\n"
".app.theme-dark .tool { color:#c8d2df; }\n"
".app.theme-dark .tool:hover { background:rgba(51,65,85,.92); box-shadow:0 1px 5px rgba(0,0,0,.22); }\n"
".app.theme-dark .location,.app.theme-dark .search,.app.theme-dark .modal-input,.app.theme-dark .desktop-edit-input { background:#182231; color:#eef3f8; border-color:rgba(148,163,184,.22); }\n"
".app.theme-dark .desktop-edit-hint,.app.theme-dark .desktop-edit-label { color:#8997aa; }\n"
".app.theme-dark .desktop-edit-toggle { background:#1d2a3a; border-color:rgba(148,163,184,.14); color:#d7e0ea; }\n"
".app.theme-dark .desktop-edit-toggle:hover { background:#29415d; color:#c9e3ff; }\n"
".app.theme-dark .desktop-edit-toggle.on { background:#24364c; border-color:rgba(125,178,255,.35); }\n"
".app.theme-dark .sidebar { background:rgba(18,27,39,.94); border-right-color:rgba(148,163,184,.13); }\n"
".app.theme-dark .side-label,.app.theme-dark .hint,.app.theme-dark .file-meta,.app.theme-dark .settings-note,.app.theme-dark .settings-shortcut { color:#8997aa; }\n"
".app.theme-dark .side-item { color:#cbd5e1; }\n"
".app.theme-dark .side-item:hover { background:rgba(51,65,85,.72); }\n"
".app.theme-dark .main { background:rgba(13,19,28,.72); }\n"
".app.theme-dark .subbar { color:#8fa0b5; border-bottom-color:rgba(148,163,184,.11); }\n"
".app.theme-dark .folder-title,.app.theme-dark .settings-name,.app.theme-dark .modal-title { color:#e7edf4; }\n"
".app.theme-dark .file-item:hover { background:rgba(51,65,85,.62); }\n"
".app.theme-dark .grid-item .file-icon { background:linear-gradient(145deg,#24354a,#1d2d40); box-shadow:0 3px 8px rgba(0,0,0,.18); }\n"
".app.theme-dark .grid-item.type-folder .file-icon { background:linear-gradient(145deg,#493d23,#372f20); }\n"
".app.theme-dark .list-item .file-icon { background:#22344a; }\n"
".app.theme-dark .list-item.type-folder .file-icon { background:#463a20; }\n"
".app.theme-dark .status { background:rgba(20,28,39,.96); color:#8fa0b5; border-top-color:rgba(148,163,184,.11); }\n"
".app.theme-dark .context,.app.theme-dark .volume-context,.app.theme-dark .modal,.app.theme-dark .settings-panel { background:#172130; border-color:rgba(148,163,184,.18); box-shadow:0 22px 60px rgba(0,0,0,.45); }\n"
".app.theme-dark .menu-item { color:#d7e0ea; }\n"
".app.theme-dark .menu-item:hover,.app.theme-dark .settings-row:hover { background:#24364c; }\n"
".app.theme-dark .app-choice { background:#1d2a3a; color:#d7e0ea; border-color:rgba(148,163,184,.14); }\n"
".app.theme-dark .app-choice:hover { background:#29415d; color:#c9e3ff; }\n"
".app.theme-dark .menu-sep { background:rgba(148,163,184,.15); }\n"
".app.theme-dark .button { background:#2a3748; color:#e5ebf2; }\n"
".app.theme-dark .button.subtle { background:transparent; color:#9ba9ba; }\n"
".app.theme-dark .settings-state { background:#2a3746; color:#a9b5c4; }\n"
".app.theme-dark .settings-header,.app.theme-dark .settings-footer { background:#172130; border-color:rgba(148,163,184,.15); }\n"
".app.theme-dark .modal-wrap.properties-dialog .modal-title,.app.theme-dark .modal-wrap.properties-dialog .modal-actions { background:#172130; border-color:rgba(148,163,184,.15); }\n"
".app.theme-dark .modal-wrap.properties-dialog .modal-info-stack { background:#131c29; }\n"
".app.theme-dark .modal-wrap.properties-dialog .modal-info { color:#c8d3df; }\n"
".app.theme-dark .settings-section { color:#8d9bad; }\n"
".app.accent-blue .tool.on,.app.accent-blue .settings-state.on { background:#d8eaff; color:#145da7; }\n"
".app.accent-blue .side-item.active,.app.accent-blue .file-item.selected { background:#cfe5ff; color:#0b4f97; }\n"
".app.accent-blue .button.primary { background:linear-gradient(180deg,#3698ff,#1477e6); }\n"
".app.accent-purple .tool.on,.app.accent-purple .settings-state.on { background:#eadcff; color:#7040b5; }\n"
".app.accent-purple .side-item.active,.app.accent-purple .file-item.selected { background:#e7d6ff; color:#6430a5; }\n"
".app.accent-purple .button.primary { background:linear-gradient(180deg,#9a69e8,#7445c5); }\n"
".app.accent-green .tool.on,.app.accent-green .settings-state.on { background:#d8f3e7; color:#19704f; }\n"
".app.accent-green .side-item.active,.app.accent-green .file-item.selected { background:#d2f0e1; color:#146443; }\n"
".app.accent-green .button.primary { background:linear-gradient(180deg,#39b985,#188a5f); }\n"
".app.accent-orange .tool.on,.app.accent-orange .settings-state.on { background:#ffead2; color:#a65b13; }\n"
".app.accent-orange .side-item.active,.app.accent-orange .file-item.selected { background:#ffe2c2; color:#934c0c; }\n"
".app.accent-orange .button.primary { background:linear-gradient(180deg,#f3a64d,#d97716); }\n"
".app.theme-dark.accent-blue .tool.on,.app.theme-dark.accent-blue .settings-state.on { background:#203f63; color:#8dc5ff; }\n"
".app.theme-dark.accent-blue .side-item.active,.app.theme-dark.accent-blue .file-item.selected { background:#22466f; color:#b9dcff; }\n"
".app.theme-dark.accent-purple .tool.on,.app.theme-dark.accent-purple .settings-state.on { background:#402d5d; color:#d4b9ff; }\n"
".app.theme-dark.accent-purple .side-item.active,.app.theme-dark.accent-purple .file-item.selected { background:#49336b; color:#e0ccff; }\n"
".app.theme-dark.accent-green .tool.on,.app.theme-dark.accent-green .settings-state.on { background:#1d493a; color:#9ce2c5; }\n"
".app.theme-dark.accent-green .side-item.active,.app.theme-dark.accent-green .file-item.selected { background:#22533f; color:#b4ead2; }\n"
".app.theme-dark.accent-orange .tool.on,.app.theme-dark.accent-orange .settings-state.on { background:#53371f; color:#ffc487; }\n"
".app.theme-dark.accent-orange .side-item.active,.app.theme-dark.accent-orange .file-item.selected { background:#614024; color:#ffd2a1; }\n"
".file-dialog-actions { flex:0 0 54px; min-height:54px; display:flex; align-items:center; justify-content:flex-end; gap:8px; padding:9px 12px; border-top:1px solid rgba(71,85,105,.13); background:rgba(249,250,252,.97); }\n"
".file-dialog-name { flex:1 1 auto; min-width:120px; max-width:420px; height:34px; padding:6px 9px; border:1px solid #9eb9d7; border-radius:8px; background:white; }\n"
".app.theme-dark .file-dialog-actions { background:#172130; border-top-color:rgba(148,163,184,.15); }\n"
".app.theme-dark .file-dialog-name { background:#111a26; color:#eef3f8; border-color:#40536b; }\n"
;

static void log_error(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "[luna-fm] ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
}

static const char *home_dir(void) {
    const char *h = getenv("HOME");
    if (h && *h) return h;
    struct passwd *pw = getpwuid(getuid());
    return (pw && pw->pw_dir) ? pw->pw_dir : "/";
}

static void safe_copy(char *dst, size_t cap, const char *src) {
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = 0; return; }
    size_t n = strlen(src);
    if (n >= cap) n = cap - 1;
    if (n) memmove(dst, src, n);
    dst[n] = 0;
}

static float clamp_grid_scroll(float value, float content, float viewport) {
    float max_scroll = content - viewport;
    if (max_scroll < 0.0f) max_scroll = 0.0f;
    if (value < 0.0f) return 0.0f;
    if (value > max_scroll) return max_scroll;
    return value;
}

static void apply_grid_scroll(void) {
    LunaElement *grid = luna_element_at(g.id_grid);
    if (!grid) return;
    float inner_h = grid->h - grid->padding * 2.0f;
    float inner_w = grid->w - grid->padding * 2.0f;
    grid->scroll_top = clamp_grid_scroll(g.grid_scroll_top,grid->scroll_content_h,inner_h);
    grid->scroll_left = clamp_grid_scroll(g.grid_scroll_left,grid->scroll_content_w,inner_w);
    grid->scroll_dest_top = clamp_grid_scroll(g.grid_scroll_dest_top,grid->scroll_content_h,inner_h);
    grid->scroll_dest_left = clamp_grid_scroll(g.grid_scroll_dest_left,grid->scroll_content_w,inner_w);
}

static void store_grid_scroll_if_changed(void) {
    LunaElement *grid = luna_element_at(g.id_grid);
    if (!grid) return;
    if (grid->scroll_top == g.grid_scroll_top &&
        grid->scroll_left == g.grid_scroll_left &&
        grid->scroll_dest_top == g.grid_scroll_dest_top &&
        grid->scroll_dest_left == g.grid_scroll_dest_left) return;
    g.grid_scroll_top = grid->scroll_top;
    g.grid_scroll_left = grid->scroll_left;
    g.grid_scroll_dest_top = grid->scroll_dest_top;
    g.grid_scroll_dest_left = grid->scroll_dest_left;
}

static void reset_grid_scroll(void) {
    g.grid_scroll_top = 0.0f;
    g.grid_scroll_left = 0.0f;
    g.grid_scroll_dest_top = 0.0f;
    g.grid_scroll_dest_left = 0.0f;
    apply_grid_scroll();
}

static void request_overlay_layout(void) {
    luna_mark_layout_dirty();
    request_redraw();
}

static size_t modal_text_chunk_size(const char *text, size_t max_bytes) {
    size_t remaining = strlen(text);
    if (remaining <= max_bytes) return remaining;

    size_t cut = max_bytes;
    while (cut > 0 && (((unsigned char)text[cut] & 0xc0u) == 0x80u)) cut--;
    if (cut == 0) cut = max_bytes;

    /* Prefer a recent line boundary so headings and values stay together. */
    size_t line = cut;
    while (line > 0 && text[line - 1] != '\n') line--;
    if (line >= (max_bytes * 7) / 8) cut = line;
    return cut;
}

static void set_modal_info_text(const char *text) {
    const char *p = text ? text : "";
    for (int part = 0; part < MODAL_INFO_PARTS; part++) {
        int id = g.id_modal_info_parts[part];
        if (id < 0) continue;
        if (*p) {
            /* Keep each text node below Luna UI's compact text-buffer range.
               Empty blocks have zero auto-height, so all chunks can stay in
               flow.  Avoiding hidden-class churn removes dozens of subtree
               restyles from the first Properties open. */
            /* 511 payload bytes fit LunaElement::text exactly.  With the
               line-boundary heuristic, 28 nodes still cover the full 12 KiB
               Properties buffer while reducing DOM/layout work. */
            char chunk[512];
            size_t n = modal_text_chunk_size(p, sizeof(chunk) - 1);
            memcpy(chunk, p, n);
            chunk[n] = 0;
            luna_set_text(id, chunk);
            p += n;
        } else {
            luna_set_text(id, "");
        }
    }
}

static int path_join(char *out, size_t cap, const char *a, const char *b) {
    if (!a || !b) return 0;
    int n = snprintf(out, cap, "%s%s%s", a, (!*a || a[strlen(a)-1] == '/') ? "" : "/", b);
    return n >= 0 && (size_t)n < cap;
}

static const char *base_name(const char *path) {
    if (!path || !*path) return "/";
    const char *p = strrchr(path, '/');
    if (!p) return path;
    if (!p[1]) return "/";
    return p + 1;
}

static void parent_path(const char *path, char *out, size_t cap) {
    safe_copy(out, cap, path && *path ? path : "/");
    size_t n = strlen(out);
    while (n > 1 && out[n-1] == '/') out[--n] = 0;
    char *slash = strrchr(out, '/');
    if (!slash || slash == out) safe_copy(out, cap, "/");
    else *slash = 0;
}

static int starts_with_ci(const char *s, const char *q) {
    if (!q || !*q) return 1;
    if (!s) return 0;
    return strcasestr(s, q) != NULL;
}

static void human_size(off_t value, char *out, size_t cap) {
    static const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double n = (double)value;
    int u = 0;
    while (n >= 1024.0 && u < 4) { n /= 1024.0; u++; }
    if (u == 0) snprintf(out, cap, "%lld B", (long long)value);
    else if (n >= 10.0) snprintf(out, cap, "%.0f %s", n, units[u]);
    else snprintf(out, cap, "%.1f %s", n, units[u]);
}

static void format_meta(const FileEntry *f, char *out, size_t cap) {
    char when[32] = "";
    struct tm tmv;
    localtime_r(&f->mtime, &tmv);
    strftime(when, sizeof(when), "%Y/%m/%d %H:%M", &tmv);
    if (f->is_dir) snprintf(out, cap, "フォルダー   %s", when);
    else {
        char sz[32]; human_size(f->size, sz, sizeof(sz));
        snprintf(out, cap, "%s   %s", sz, when);
    }
}

static const char *type_class(const FileEntry *f) {
    if (f->is_dir) return "type-folder";
    const char *ext = strrchr(f->name, '.');
    if (!ext) return "type-file";
    ext++;
    if (!strcasecmp(ext,"png") || !strcasecmp(ext,"jpg") || !strcasecmp(ext,"jpeg") ||
        !strcasecmp(ext,"gif") || !strcasecmp(ext,"webp") || !strcasecmp(ext,"svg") ||
        !strcasecmp(ext,"bmp")) return "type-image";
    if (!strcasecmp(ext,"mp3") || !strcasecmp(ext,"flac") || !strcasecmp(ext,"wav") ||
        !strcasecmp(ext,"ogg") || !strcasecmp(ext,"m4a") || !strcasecmp(ext,"aac")) return "type-audio";
    if (!strcasecmp(ext,"mp4") || !strcasecmp(ext,"mkv") || !strcasecmp(ext,"webm") ||
        !strcasecmp(ext,"mov") || !strcasecmp(ext,"avi")) return "type-video";
    if (!strcasecmp(ext,"zip") || !strcasecmp(ext,"tar") || !strcasecmp(ext,"gz") ||
        !strcasecmp(ext,"bz2") || !strcasecmp(ext,"xz") || !strcasecmp(ext,"7z") ||
        !strcasecmp(ext,"rar")) return "type-archive";
    if (!strcasecmp(ext,"c") || !strcasecmp(ext,"h") || !strcasecmp(ext,"cpp") ||
        !strcasecmp(ext,"py") || !strcasecmp(ext,"js") || !strcasecmp(ext,"ts") ||
        !strcasecmp(ext,"html") || !strcasecmp(ext,"css") || !strcasecmp(ext,"json") ||
        !strcasecmp(ext,"md") || !strcasecmp(ext,"sh")) return "type-code";
    if (!strcasecmp(ext,"desktop")) return "type-desktop";
    return "type-file";
}

static const char *type_icon(const FileEntry *f) {
    const char *t = type_class(f);
    if (!strcmp(t,"type-folder")) return "◆";
    if (!strcmp(t,"type-image")) return "▧";
    if (!strcmp(t,"type-audio")) return "♫";
    if (!strcmp(t,"type-video")) return "▶";
    if (!strcmp(t,"type-archive")) return "▤";
    if (!strcmp(t,"type-code")) return "⌘";
    if (!strcmp(t,"type-desktop")) return "☆";
    return "◇";
}

static int compare_entries(const void *pa, const void *pb) {
    const FileEntry *a = pa, *b = pb;
    if (a->is_dir != b->is_dir) return a->is_dir ? -1 : 1;
    int r = 0;
    if (g.sort_mode == SORT_DATE) r = (a->mtime > b->mtime) - (a->mtime < b->mtime);
    else if (g.sort_mode == SORT_SIZE) r = (a->size > b->size) - (a->size < b->size);
    else r = strcasecmp(a->name, b->name);
    if (g.sort_desc) r = -r;
    if (r == 0) r = strcasecmp(a->name, b->name);
    return r;
}

static int path_is_directory(const char *path) {
    struct stat st;
    return path && *path && stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static void settings_defaults(void) {
    g.grid_view = 1;
    g.show_hidden = 0;
    g.sort_mode = SORT_NAME;
    g.sort_desc = 0;
    g.show_sidebar = 1;
    g.show_search = 1;
    g.window_width = DEFAULT_WINDOW_W;
    g.window_height = DEFAULT_WINDOW_H;
    g.font_size = 0;
    g.font_family = 0;
    g.display_density = 0;
    g.theme = 0;
    g.accent = 0;
    g.show_status = 1;
    g.show_grid_details = 0;
    g.animations = 1;
    g.startup_mode = 0;
    g.terminal_command[0] = 0;
    g.custom_font[0] = 0;
    g.last_directory[0] = 0;
}

static void settings_path(char *out, size_t cap) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && *xdg) snprintf(out, cap, "%s/luna-fm.conf", xdg);
    else snprintf(out, cap, "%s/.config/luna-fm.conf", home_dir());
}

static int setting_bool(const char *s, int fallback) {
    if (!s) return fallback;
    if (!strcasecmp(s,"1") || !strcasecmp(s,"true") || !strcasecmp(s,"yes") || !strcasecmp(s,"on")) return 1;
    if (!strcasecmp(s,"0") || !strcasecmp(s,"false") || !strcasecmp(s,"no") || !strcasecmp(s,"off")) return 0;
    return fallback;
}

static void load_settings(void) {
    settings_defaults();
    char path[PATH_MAX]; settings_path(path,sizeof(path));
    FILE *fp=fopen(path,"r"); if(!fp)return;
    char line[1024];
    while(fgets(line,sizeof(line),fp)){
        char *nl=strpbrk(line,"\r\n"); if(nl)*nl=0;
        char *eq=strchr(line,'='); if(!eq)continue; *eq++=0;
        if(!strcmp(line,"show_sidebar"))g.show_sidebar=setting_bool(eq,g.show_sidebar);
        else if(!strcmp(line,"show_search"))g.show_search=setting_bool(eq,g.show_search);
        else if(!strcmp(line,"show_hidden"))g.show_hidden=setting_bool(eq,g.show_hidden);
        else if(!strcmp(line,"grid_view"))g.grid_view=setting_bool(eq,g.grid_view);
        else if(!strcmp(line,"sort_mode")){int v=atoi(eq);if(v>=0&&v<=2)g.sort_mode=(SortMode)v;}
        else if(!strcmp(line,"sort_desc"))g.sort_desc=setting_bool(eq,g.sort_desc);
        else if(!strcmp(line,"window_width"))g.window_width=atoi(eq);
        else if(!strcmp(line,"window_height"))g.window_height=atoi(eq);
        else if(!strcmp(line,"font_size")){int v=atoi(eq);if(v>=0&&v<=2)g.font_size=v;}
        else if(!strcmp(line,"font_family")){int v=atoi(eq);if(v>=0&&v<=4)g.font_family=v;}
        else if(!strcmp(line,"display_density")){int v=atoi(eq);if(v>=0&&v<=3)g.display_density=v;}
        else if(!strcmp(line,"theme")){int v=atoi(eq);if(v>=0&&v<=1)g.theme=v;}
        else if(!strcmp(line,"accent")){int v=atoi(eq);if(v>=0&&v<=3)g.accent=v;}
        else if(!strcmp(line,"show_status"))g.show_status=setting_bool(eq,g.show_status);
        else if(!strcmp(line,"show_grid_details"))g.show_grid_details=setting_bool(eq,g.show_grid_details);
        else if(!strcmp(line,"animations"))g.animations=setting_bool(eq,g.animations);
        else if(!strcmp(line,"startup_mode")){int v=atoi(eq);if(v>=0&&v<=2)g.startup_mode=v;}
        else if(!strcmp(line,"terminal_command"))safe_copy(g.terminal_command,sizeof(g.terminal_command),eq);
        else if(!strcmp(line,"custom_font"))safe_copy(g.custom_font,sizeof(g.custom_font),eq);
        else if(!strcmp(line,"last_directory"))safe_copy(g.last_directory,sizeof(g.last_directory),eq);
    }
    fclose(fp);
    if(g.window_width<MIN_WINDOW_W||g.window_width>4096)g.window_width=DEFAULT_WINDOW_W;
    if(g.window_height<MIN_WINDOW_H||g.window_height>2160)g.window_height=DEFAULT_WINDOW_H;
}

static void save_settings(void) {
    char path[PATH_MAX]; settings_path(path,sizeof(path));
    if(g.window)glfwGetWindowSize(g.window,&g.window_width,&g.window_height);
    if(g.cwd[0])safe_copy(g.last_directory,sizeof(g.last_directory),g.cwd);
    FILE *fp=fopen(path,"w"); if(!fp)return;
    fprintf(fp,"show_sidebar=%d\n",g.show_sidebar);
    fprintf(fp,"show_search=%d\n",g.show_search);
    fprintf(fp,"show_hidden=%d\n",g.show_hidden);
    fprintf(fp,"grid_view=%d\n",g.grid_view);
    fprintf(fp,"sort_mode=%d\n",(int)g.sort_mode);
    fprintf(fp,"sort_desc=%d\n",g.sort_desc);
    fprintf(fp,"window_width=%d\nwindow_height=%d\n",g.window_width,g.window_height);
    fprintf(fp,"font_size=%d\nfont_family=%d\n",g.font_size,g.font_family);
    fprintf(fp,"display_density=%d\ntheme=%d\naccent=%d\n",g.display_density,g.theme,g.accent);
    fprintf(fp,"show_status=%d\nshow_grid_details=%d\nanimations=%d\n",g.show_status,g.show_grid_details,g.animations);
    fprintf(fp,"startup_mode=%d\n",g.startup_mode);
    fprintf(fp,"terminal_command=%s\n",g.terminal_command);
    fprintf(fp,"custom_font=%s\n",g.custom_font);
    fprintf(fp,"last_directory=%s\n",g.last_directory);
    fclose(fp);
}

static void expand_home_dir_value(const char *src, char *out, size_t cap) {
    if(!src){out[0]=0;return;}
    if(!strncmp(src,"$HOME",5))snprintf(out,cap,"%s%s",home_dir(),src+5);
    else if(!strncmp(src,"${HOME}",7))snprintf(out,cap,"%s%s",home_dir(),src+7);
    else safe_copy(out,cap,src);
}

static void xdg_user_dir(const char *key, const char *fallback, char *out, size_t cap) {
    char cfg[PATH_MAX];
    const char *xdg=getenv("XDG_CONFIG_HOME");
    if(xdg&&*xdg)snprintf(cfg,sizeof(cfg),"%s/user-dirs.dirs",xdg);
    else snprintf(cfg,sizeof(cfg),"%s/.config/user-dirs.dirs",home_dir());
    FILE *fp=fopen(cfg,"r");
    if(fp){
        char line[PATH_MAX+128]; size_t klen=strlen(key);
        while(fgets(line,sizeof(line),fp)){
            char *p=line; while(*p==' '||*p=='\t')p++;
            if(strncmp(p,key,klen)||p[klen]!='=')continue;
            p+=klen+1; while(*p==' '||*p=='\t')p++;
            char *end=p+strlen(p); while(end>p&&(end[-1]=='\r'||end[-1]=='\n'||end[-1]==' '||end[-1]=='\t'))*--end=0;
            if(*p=='"'&&end>p+1&&end[-1]=='"'){p++;end[-1]=0;}
            expand_home_dir_value(p,out,cap); fclose(fp); return;
        }
        fclose(fp);
    }
    snprintf(out,cap,"%s/%s",home_dir(),fallback);
}

static void build_sidebar_paths(void) {
    safe_copy(g.side_paths[0],sizeof(g.side_paths[0]),home_dir());
    xdg_user_dir("XDG_DESKTOP_DIR","Desktop",g.side_paths[1],sizeof(g.side_paths[1]));
    xdg_user_dir("XDG_DOCUMENTS_DIR","Documents",g.side_paths[2],sizeof(g.side_paths[2]));
    xdg_user_dir("XDG_DOWNLOAD_DIR","Downloads",g.side_paths[3],sizeof(g.side_paths[3]));
    xdg_user_dir("XDG_PICTURES_DIR","Pictures",g.side_paths[4],sizeof(g.side_paths[4]));
    xdg_user_dir("XDG_MUSIC_DIR","Music",g.side_paths[5],sizeof(g.side_paths[5]));
    xdg_user_dir("XDG_VIDEOS_DIR","Videos",g.side_paths[6],sizeof(g.side_paths[6]));
    snprintf(g.side_paths[7],sizeof(g.side_paths[7]),"%s/.local/share/Trash/files",home_dir());
    safe_copy(g.side_paths[8],sizeof(g.side_paths[8]),"/");
}

static void refresh_sidebar_visibility(void) {
    static const char *ids[9]={"side-home","side-desktop","side-documents","side-downloads","side-pictures","side-music","side-videos","side-trash","side-root"};
    for(int i=0;i<9;i++){
        int id=luna_get_element_by_id(ids[i]);
        if(path_is_directory(g.side_paths[i]))luna_remove_class(id,"missing");
        else luna_add_class(id,"missing");
    }
    if(g.show_sidebar)luna_remove_class(g.id_sidebar,"hidden"); else luna_add_class(g.id_sidebar,"hidden");
    if(g.show_search)luna_remove_class(g.id_search,"hidden"); else luna_add_class(g.id_search,"hidden");
    refresh_volumes(0);
    request_redraw();
}

static int hex_nibble(char c) {
    if(c>='0'&&c<='9')return c-'0';
    if(c>='a'&&c<='f')return c-'a'+10;
    if(c>='A'&&c<='F')return c-'A'+10;
    return -1;
}

static void file_uri_to_path(const char *uri, char *out, size_t cap) {
    const char *p=uri?uri:"";
    size_t w=0;
    if(!strncmp(p,"file://",7))p+=7;
    while(*p&&w+1<cap){
        if(*p=='%'&&hex_nibble(p[1])>=0&&hex_nibble(p[2])>=0){
            out[w++]=(char)((hex_nibble(p[1])<<4)|hex_nibble(p[2]));
            p+=3;
        }else out[w++]=*p++;
    }
    out[w]=0;
}

static int volume_path_is_under(const char *cwd, const char *mount) {
    size_t n;
    if(!cwd||!mount||!*mount)return 0;
    n=strlen(mount);
    if(strcmp(cwd,mount)==0)return 1;
    return strncmp(cwd,mount,n)==0 && cwd[n]=='/';
}

static int add_volume_entry(const char *name, const char *device, const char *mount,
                            const char *uuid, int can_mount, int can_unmount, int can_eject,
                            int is_removable) {
    VolumeEntry *v;
    if(g.volume_count>=MAX_VOLUMES)return 0;
    if((!name||!*name)&&(!device||!*device)&&(!mount||!*mount))return 0;
    /* Deduplicate by device or mount path. */
    for(int i=0;i<g.volume_count;i++){
        if(device&&*device&&g.volumes[i].device[0]&&!strcmp(g.volumes[i].device,device)){
            v=&g.volumes[i];
            if(name&&*name)safe_copy(v->name,sizeof(v->name),name);
            if(mount&&*mount){safe_copy(v->mount_path,sizeof(v->mount_path),mount);v->is_mounted=1;v->can_unmount=can_unmount||v->can_unmount;v->can_mount=0;}
            if(uuid&&*uuid)safe_copy(v->uuid,sizeof(v->uuid),uuid);
            v->can_eject=can_eject||v->can_eject;
            v->is_removable=is_removable||v->is_removable;
            return 1;
        }
        if(mount&&*mount&&g.volumes[i].mount_path[0]&&!strcmp(g.volumes[i].mount_path,mount)){
            v=&g.volumes[i];
            if(name&&*name)safe_copy(v->name,sizeof(v->name),name);
            if(device&&*device)safe_copy(v->device,sizeof(v->device),device);
            v->is_mounted=1;v->can_unmount=1;v->can_mount=0;
            v->can_eject=can_eject||v->can_eject;
            return 1;
        }
    }
    v=&g.volumes[g.volume_count++];
    memset(v,0,sizeof(*v));
    if(name&&*name)safe_copy(v->name,sizeof(v->name),name);
    else if(device&&*device)safe_copy(v->name,sizeof(v->name),base_name(device));
    else safe_copy(v->name,sizeof(v->name),"ボリューム");
    if(device)safe_copy(v->device,sizeof(v->device),device);
    if(mount&&*mount){safe_copy(v->mount_path,sizeof(v->mount_path),mount);v->is_mounted=1;}
    if(uuid)safe_copy(v->uuid,sizeof(v->uuid),uuid);
    v->can_mount=can_mount && !v->is_mounted;
    v->can_unmount=can_unmount || v->is_mounted;
    v->can_eject=can_eject;
    v->is_removable=is_removable;
    return 1;
}

static void collect_volumes_from_gio(void) {
    char output[131072];
    char *argv[]={"gio","mount","-l","-i",NULL};
    char cur_name[256]="",cur_device[PATH_MAX]="",cur_uuid[128]="",cur_mount[PATH_MAX]="";
    int in_volume=0,can_mount=0,can_eject=0,can_unmount=0,drive_removable=0;
    char *line,*save=NULL;
    if(!program_exists("gio"))return;
    if(!pipe_read_program("gio",argv,output,sizeof(output)))return;
    for(line=strtok_r(output,"\n",&save);line;line=strtok_r(NULL,"\n",&save)){
        char *p=line;
        while(*p==' '||*p=='\t')p++;
        if(!strncmp(p,"Drive(",6)){
            if(in_volume)
                add_volume_entry(cur_name,cur_device,cur_mount,cur_uuid,can_mount,can_unmount,can_eject,drive_removable);
            in_volume=0;cur_name[0]=cur_device[0]=cur_uuid[0]=cur_mount[0]=0;
            can_mount=can_eject=can_unmount=0;drive_removable=0;
            continue;
        }
        if(!strncmp(p,"is_removable=",13)){drive_removable=atoi(p+13)!=0;continue;}
        if(!strncmp(p,"Volume(",7)){
            if(in_volume)
                add_volume_entry(cur_name,cur_device,cur_mount,cur_uuid,can_mount,can_unmount,can_eject,drive_removable);
            in_volume=1;cur_name[0]=cur_device[0]=cur_uuid[0]=cur_mount[0]=0;
            can_mount=can_eject=can_unmount=0;
            {
                const char *colon=strchr(p,':');
                if(colon){safe_copy(cur_name,sizeof(cur_name),colon+1);trim_text(cur_name);}
            }
            continue;
        }
        if(!in_volume)continue;
        if(!strncmp(p,"unix-device: '",13)){
            const char *s=p+13;const char *e=strchr(s,'\'');
            size_t n=e?(size_t)(e-s):strlen(s);
            if(n>=sizeof(cur_device))n=sizeof(cur_device)-1;
            memcpy(cur_device,s,n);cur_device[n]=0;
            continue;
        }
        if(!strncmp(p,"uuid=",5)&&!cur_uuid[0]){safe_copy(cur_uuid,sizeof(cur_uuid),p+5);trim_text(cur_uuid);continue;}
        if(!strncmp(p,"can_mount=",10)){can_mount=atoi(p+10)!=0;continue;}
        if(!strncmp(p,"can_eject=",10)){can_eject=atoi(p+10)!=0;continue;}
        if(!strncmp(p,"can_unmount=",12)){can_unmount=atoi(p+12)!=0;continue;}
        if(!strncmp(p,"Mount(",6)){
            const char *arrow=strstr(p,"->");
            if(arrow){
                char uri[PATH_MAX];
                safe_copy(uri,sizeof(uri),arrow+2);trim_text(uri);
                file_uri_to_path(uri,cur_mount,sizeof(cur_mount));
                can_unmount=1;can_mount=0;
            }
            continue;
        }
    }
    if(in_volume)
        add_volume_entry(cur_name,cur_device,cur_mount,cur_uuid,can_mount,can_unmount,can_eject,drive_removable);
}

static void collect_volumes_from_lsblk_pairs(void) {
    char output[65536];
    char *argv[]={"lsblk","-P","-o","NAME,LABEL,UUID,FSTYPE,MOUNTPOINT,RM,HOTPLUG,TYPE",NULL};
    char *line,*save=NULL;
    if(!program_exists("lsblk"))return;
    if(!pipe_read_program("lsblk",argv,output,sizeof(output)))return;
    for(line=strtok_r(output,"\n",&save);line;line=strtok_r(NULL,"\n",&save)){
        char name[128]="",label[256]="",uuid[128]="",fstype[64]="",mount[PATH_MAX]="",type[32]="";
        int rm=0,hotplug=0;
        char *p=line;
        while(*p){
            while(*p==' ')p++;
            if(!*p)break;
            char key[32]="",val[PATH_MAX]="";
            char *eq=strchr(p,'=');
            if(!eq)break;
            size_t klen=(size_t)(eq-p);if(klen>=sizeof(key))klen=sizeof(key)-1;
            memcpy(key,p,klen);key[klen]=0;p=eq+1;
            if(*p=='"'){
                p++;char *end=strchr(p,'"');if(!end)break;
                size_t n=(size_t)(end-p);if(n>=sizeof(val))n=sizeof(val)-1;
                memcpy(val,p,n);val[n]=0;p=end+1;
            }else{
                char *sp=strchr(p,' ');size_t n=sp?(size_t)(sp-p):strlen(p);
                if(n>=sizeof(val))n=sizeof(val)-1;
                memcpy(val,p,n);val[n]=0;p+=n;
            }
            if(!strcmp(key,"NAME"))safe_copy(name,sizeof(name),val);
            else if(!strcmp(key,"LABEL"))safe_copy(label,sizeof(label),val);
            else if(!strcmp(key,"UUID"))safe_copy(uuid,sizeof(uuid),val);
            else if(!strcmp(key,"FSTYPE"))safe_copy(fstype,sizeof(fstype),val);
            else if(!strcmp(key,"MOUNTPOINT"))safe_copy(mount,sizeof(mount),val);
            else if(!strcmp(key,"TYPE"))safe_copy(type,sizeof(type),val);
            else if(!strcmp(key,"RM"))rm=atoi(val)!=0;
            else if(!strcmp(key,"HOTPLUG"))hotplug=atoi(val)!=0;
        }
        if(strcmp(type,"part")&&strcmp(type,"crypt")&&strcmp(type,"lvm"))continue;
        if(!fstype[0])continue;
        /* Keep removable/hotplug volumes and anything already mounted under media paths. */
        int media_mount = mount[0] && (strstr(mount,"/media/")||strstr(mount,"/run/media/")||
                                       (!strncmp(mount,"/mnt/",5)&&strcmp(mount,"/mnt")));
        if(!(rm||hotplug||media_mount))continue;
        if(!strcmp(mount,"/")||!strcmp(mount,"/boot")||!strncmp(mount,"/boot/",6)||
           !strcmp(mount,"/home")||!strncmp(mount,"/home/",6))continue;
        {
            char device[PATH_MAX];
            if(name[0]=='/')safe_copy(device,sizeof(device),name);
            else snprintf(device,sizeof(device),"/dev/%s",name);
            add_volume_entry(label[0]?label:name,device,mount,uuid,
                             mount[0]?0:1, mount[0]?1:0, rm||hotplug, rm||hotplug);
        }
    }
}

static void render_volumes(void) {
    if(g.id_side_devices_label>=0){
        if(g.volume_count>0)luna_remove_class(g.id_side_devices_label,"hidden");
        else luna_add_class(g.id_side_devices_label,"hidden");
    }
    for(int i=0;i<MAX_VOLUMES;i++){
        char id[32],label[320];
        int eid;
        snprintf(id,sizeof(id),"vol%02d",i);
        eid=luna_get_element_by_id(id);
        if(eid<0)continue;
        if(i>=g.volume_count){
            luna_add_class(eid,"hidden");
            luna_remove_class(eid,"active");
            luna_remove_class(eid,"volume-unmounted");
            continue;
        }
        VolumeEntry *v=&g.volumes[i];
        snprintf(label,sizeof(label),"%s　%s",
                 v->is_mounted?"▣":"◇",
                 v->name[0]?v->name:(v->device[0]?v->device:"ボリューム"));
        luna_set_text(eid,label);
        luna_remove_class(eid,"hidden");
        if(v->is_mounted && volume_path_is_under(g.cwd,v->mount_path))luna_add_class(eid,"active");
        else luna_remove_class(eid,"active");
        if(v->is_mounted)luna_remove_class(eid,"volume-unmounted");
        else luna_add_class(eid,"volume-unmounted");
    }
    request_redraw();
}

static void refresh_volumes(int force) {
    double now=glfwGetTime();
    if(!force && g.volumes_next_refresh>0 && now<g.volumes_next_refresh)return;
    g.volume_count=0;
    /* Prefer util-linux lsblk (no gvfs). Optional gio fills network mounts. */
    collect_volumes_from_lsblk_pairs();
    if(program_exists("gio"))collect_volumes_from_gio();
    g.volumes_next_refresh=now+VOLUME_REFRESH_SEC;
    /* If the current folder vanished because a volume was unmounted, go home. */
    if(g.cwd[0] && !path_is_directory(g.cwd)){
        show_toast("マウントが解除されたためホームへ戻ります");
        navigate(home_dir(),1);
        return;
    }
    render_volumes();
}

static int mount_volume_index(int index) {
    VolumeEntry *v;
    char device_arg[PATH_MAX];
    if(index<0||index>=g.volume_count)return 0;
    v=&g.volumes[index];
    if(v->is_mounted && v->mount_path[0] && path_is_directory(v->mount_path))return 1;
    if(v->device[0])safe_copy(device_arg,sizeof(device_arg),v->device);
    else return 0;
    /* udisks2 alone is enough; gio/gvfs is optional. */
    if(program_exists("udisksctl")){
        char *argv[]={"udisksctl","mount","-b",device_arg,NULL};
        if(run_program_wait("udisksctl",argv)){refresh_volumes(1);return 1;}
    }
    if(program_exists("gio")){
        char *argv[]={"gio","mount","-d",device_arg,NULL};
        if(run_program_wait("gio",argv)){refresh_volumes(1);return 1;}
    }
    return 0;
}

static int unmount_volume_index(int index, int eject) {
    VolumeEntry *v;
    if(index<0||index>=g.volume_count)return 0;
    v=&g.volumes[index];
    if(program_exists("udisksctl") && v->device[0]){
        char *argv[]={"udisksctl","unmount","-b",v->device,NULL};
        if(run_program_wait("udisksctl",argv)){
            if(eject){
                char *pargv[]={"udisksctl","power-off","-b",v->device,NULL};
                run_program_wait("udisksctl",pargv);
            }
            refresh_volumes(1);return 1;
        }
    }
    if(program_exists("gio")){
        if(eject && v->mount_path[0]){
            char *argv[]={"gio","mount","-e",v->mount_path,NULL};
            if(run_program_wait("gio",argv)){refresh_volumes(1);return 1;}
        }
        if(v->mount_path[0]){
            char *argv[]={"gio","mount","-u",v->mount_path,NULL};
            if(run_program_wait("gio",argv)){
                if(eject && v->device[0] && program_exists("udisksctl")){
                    char *pargv[]={"udisksctl","power-off","-b",v->device,NULL};
                    run_program_wait("udisksctl",pargv);
                }
                refresh_volumes(1);return 1;
            }
        }
    }
    return 0;
}

static void hide_volume_context(void) {
    if(g.id_volume_context>=0)luna_add_class(g.id_volume_context,"hidden");
    g.volume_context=-1;
}

static void show_volume_context(int index) {
    if(index<0||index>=g.volume_count)return;
    hide_context();
    g.volume_context=index;
    if(g.id_volume_context>=0){
        luna_remove_class(g.id_volume_context,"hidden");
        request_overlay_layout();
    }
}

static void activate_volume_index(int index) {
    char name[256], device[PATH_MAX], uuid[128], mount_path[PATH_MAX];
    int was_mounted;
    if(index<0||index>=g.volume_count)return;
    safe_copy(name,sizeof(name),g.volumes[index].name);
    safe_copy(device,sizeof(device),g.volumes[index].device);
    safe_copy(uuid,sizeof(uuid),g.volumes[index].uuid);
    safe_copy(mount_path,sizeof(mount_path),g.volumes[index].mount_path);
    was_mounted=g.volumes[index].is_mounted;
    if(!was_mounted){
        show_toast("%s をマウントしています…",name);
        if(!mount_volume_index(index)){show_toast("マウントに失敗しました");return;}
        index=-1;
        for(int i=0;i<g.volume_count;i++){
            if((device[0]&&!strcmp(g.volumes[i].device,device))||
               (uuid[0]&&!strcmp(g.volumes[i].uuid,uuid))||
               (name[0]&&!strcmp(g.volumes[i].name,name))){index=i;break;}
        }
        if(index>=0)safe_copy(mount_path,sizeof(mount_path),g.volumes[index].mount_path);
    }
    if(mount_path[0]&&path_is_directory(mount_path)){
        navigate(mount_path,1);
        show_toast("%s を開きました",name);
    }else show_toast("マウントポイントが見つかりません");
}

static int ensure_dir(const char *path, mode_t mode) {
    if (mkdir(path, mode) == 0) return 1;
    return errno == EEXIST;
}

static int copy_file_bytes(const char *src, const char *dst, mode_t mode) {
    int in = open(src, O_RDONLY | O_CLOEXEC);
    if (in < 0) return 0;
    int out = open(dst, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, mode & 0777);
    if (out < 0) { close(in); return 0; }
    char buf[131072];
    int ok = 1;
    for (;;) {
        ssize_t nr = read(in, buf, sizeof(buf));
        if (nr == 0) break;
        if (nr < 0) { if (errno == EINTR) continue; ok = 0; break; }
        char *p = buf;
        ssize_t left = nr;
        while (left > 0) {
            ssize_t nw = write(out, p, (size_t)left);
            if (nw < 0) { if (errno == EINTR) continue; ok = 0; break; }
            p += nw; left -= nw;
        }
        if (!ok) break;
    }
    close(in);
    if (close(out) != 0) ok = 0;
    if (!ok) unlink(dst);
    return ok;
}

static int copy_tree(const char *src, const char *dst) {
    struct stat st;
    if (lstat(src, &st) != 0) return 0;
    if (S_ISLNK(st.st_mode)) {
        char target[PATH_MAX];
        ssize_t n = readlink(src, target, sizeof(target)-1);
        if (n < 0) return 0;
        target[n] = 0;
        return symlink(target, dst) == 0;
    }
    if (S_ISREG(st.st_mode)) return copy_file_bytes(src, dst, st.st_mode);
    if (!S_ISDIR(st.st_mode)) return 0;
    if (mkdir(dst, st.st_mode & 0777) != 0) return 0;
    DIR *d = opendir(src);
    if (!d) { rmdir(dst); return 0; }
    int ok = 1;
    struct dirent *de;
    while ((de = readdir(d))) {
        if (!strcmp(de->d_name,".") || !strcmp(de->d_name,"..")) continue;
        char a[PATH_MAX], b[PATH_MAX];
        if (!path_join(a,sizeof(a),src,de->d_name) || !path_join(b,sizeof(b),dst,de->d_name) || !copy_tree(a,b)) {
            ok = 0; break;
        }
    }
    closedir(d);
    if (!ok) return 0;
    chmod(dst, st.st_mode & 0777);
    return 1;
}

static int remove_tree(const char *path) {
    struct stat st;
    if (lstat(path, &st) != 0) return 0;
    if (!S_ISDIR(st.st_mode) || S_ISLNK(st.st_mode)) return unlink(path) == 0;
    DIR *d = opendir(path);
    if (!d) return 0;
    int ok = 1;
    struct dirent *de;
    while ((de = readdir(d))) {
        if (!strcmp(de->d_name,".") || !strcmp(de->d_name,"..")) continue;
        char child[PATH_MAX];
        if (!path_join(child,sizeof(child),path,de->d_name) || !remove_tree(child)) { ok = 0; break; }
    }
    closedir(d);
    return ok && rmdir(path) == 0;
}

static int unique_destination(const char *dir, const char *name, char *out, size_t cap) {
    if (!path_join(out,cap,dir,name)) return 0;
    if (access(out,F_OK) != 0) return 1;
    char stem[NAME_MAX+1], ext[NAME_MAX+1];
    safe_copy(stem,sizeof(stem),name);
    ext[0]=0;
    char *dot = strrchr(stem,'.');
    if (dot && dot != stem) { safe_copy(ext,sizeof(ext),dot); *dot=0; }
    for (int i=2;i<10000;i++) {
        char candidate[NAME_MAX+1];
        char suffix[32];
        int suffix_n = snprintf(suffix,sizeof(suffix),"（コピー %d）",i);
        if (suffix_n < 0 || (size_t)suffix_n >= sizeof(suffix)) return 0;

        size_t suffix_len = (size_t)suffix_n;
        size_t ext_len = strlen(ext);
        if (suffix_len + ext_len >= sizeof(candidate)) return 0;

        size_t stem_len = strlen(stem);
        size_t max_stem = sizeof(candidate) - 1 - suffix_len - ext_len;
        if (stem_len > max_stem) {
            stem_len = max_stem;
            /* Do not cut a UTF-8 sequence in the middle. */
            while (stem_len && (((unsigned char)stem[stem_len] & 0xc0u) == 0x80u))
                stem_len--;
        }

        char *q = candidate;
        if (stem_len) { memcpy(q,stem,stem_len); q += stem_len; }
        memcpy(q,suffix,suffix_len); q += suffix_len;
        if (ext_len) { memcpy(q,ext,ext_len); q += ext_len; }
        *q = 0;

        if (!path_join(out,cap,dir,candidate)) return 0;
        if (access(out,F_OK)!=0) return 1;
    }
    return 0;
}

static void url_escape_path(const char *src, char *dst, size_t cap) {
    static const char hex[]="0123456789ABCDEF";
    size_t w=0;
    for (const unsigned char *p=(const unsigned char*)src; *p && w+4<cap; p++) {
        unsigned char c=*p;
        if ((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='/'||c=='-'||c=='_'||c=='.'||c=='~') dst[w++]=(char)c;
        else { dst[w++]='%'; dst[w++]=hex[c>>4]; dst[w++]=hex[c&15]; }
    }
    dst[w]=0;
}

static int move_to_trash(const char *path) {
    char base[PATH_MAX], files[PATH_MAX], info[PATH_MAX];
    snprintf(base,sizeof(base),"%s/.local/share/Trash",home_dir());
    path_join(files,sizeof(files),base,"files");
    path_join(info,sizeof(info),base,"info");
    ensure_dir(base,0700); ensure_dir(files,0700); ensure_dir(info,0700);
    char dst[PATH_MAX];
    if (!unique_destination(files,base_name(path),dst,sizeof(dst))) return 0;
    if (rename(path,dst)!=0) {
        if (!copy_tree(path,dst) || !remove_tree(path)) return 0;
    }
    char infopath[PATH_MAX];
    char info_name[NAME_MAX + sizeof(".trashinfo")];
    const char *dst_name = base_name(dst);
    size_t dst_name_len = strlen(dst_name);
    static const char trashinfo_suffix[] = ".trashinfo";
    size_t trashinfo_suffix_len = sizeof(trashinfo_suffix) - 1;
    if (dst_name_len + trashinfo_suffix_len >= sizeof(info_name)) return 1;
    memcpy(info_name,dst_name,dst_name_len);
    memcpy(info_name+dst_name_len,trashinfo_suffix,trashinfo_suffix_len+1);
    if (!path_join(infopath,sizeof(infopath),info,info_name)) return 1;
    FILE *fp=fopen(infopath,"w");
    if (fp) {
        char escaped[PATH_MAX*3]; url_escape_path(path,escaped,sizeof(escaped));
        time_t now=time(NULL); struct tm tmv; localtime_r(&now,&tmv); char t[40];
        strftime(t,sizeof(t),"%Y-%m-%dT%H:%M:%S",&tmv);
        fprintf(fp,"[Trash Info]\nPath=%s\nDeletionDate=%s\n",escaped,t);
        fclose(fp);
    }
    return 1;
}

static int program_exists(const char *name) {
    const char *path=getenv("PATH");
    if (!path) return 0;
    char *dup=strdup(path), *save=NULL;
    if (!dup) return 0;
    int found=0;
    for (char *d=strtok_r(dup,":",&save); d; d=strtok_r(NULL,":",&save)) {
        char p[PATH_MAX]; snprintf(p,sizeof(p),"%s/%s",*d?d:".",name);
        if (access(p,X_OK)==0) { found=1; break; }
    }
    free(dup); return found;
}

static int pipe_write_program(const char *prog, char *const argv[], const char *data) {
    int fds[2]; if (pipe(fds)!=0) return 0;
    pid_t pid=fork();
    if (pid==0) {
        dup2(fds[0],STDIN_FILENO); close(fds[0]); close(fds[1]);
        execvp(prog,argv); _exit(127);
    }
    close(fds[0]);
    size_t left=strlen(data); const char *p=data;
    while (left) { ssize_t n=write(fds[1],p,left); if (n<0){if(errno==EINTR)continue;break;} p+=n; left-=n; }
    close(fds[1]);
    int st=0; waitpid(pid,&st,0);
    return WIFEXITED(st)&&WEXITSTATUS(st)==0;
}

static int pipe_read_program(const char *prog, char *const argv[], char *out, size_t cap) {
    int fds[2]; if (pipe(fds)!=0) return 0;
    pid_t pid=fork();
    if (pid==0) {
        dup2(fds[1],STDOUT_FILENO); close(fds[0]); close(fds[1]);
        int dev=open("/dev/null",O_WRONLY); if(dev>=0){dup2(dev,STDERR_FILENO);close(dev);} 
        execvp(prog,argv); _exit(127);
    }
    close(fds[1]); size_t w=0;
    while (w+1<cap) { ssize_t n=read(fds[0],out+w,cap-w-1); if(n==0)break; if(n<0){if(errno==EINTR)continue;break;} w+=(size_t)n; }
    out[w]=0; close(fds[0]); int st=0; waitpid(pid,&st,0);
    return w>0 && WIFEXITED(st)&&WEXITSTATUS(st)==0;
}


static void trim_text(char *s) {
    if(!s)return;
    char *start=s;
    while(*start==' '||*start=='\t'||*start=='\r'||*start=='\n')start++;
    if(start!=s)memmove(s,start,strlen(start)+1);
    size_t n=strlen(s);
    while(n>0&&(s[n-1]==' '||s[n-1]=='\t'||s[n-1]=='\r'||s[n-1]=='\n'))s[--n]=0;
}

static int font_file_extension_supported(const char *path) {
    const char *dot=path?strrchr(path,'.'):NULL;
    return dot&&(!strcasecmp(dot,".ttf")||!strcasecmp(dot,".otf")||
                 !strcasecmp(dot,".ttc")||!strcasecmp(dot,".otc"));
}

static int resolve_font_spec(const char *spec, char *out, size_t cap) {
    if(!spec||!*spec||!out||cap==0)return 0;
    char value[PATH_MAX];safe_copy(value,sizeof(value),spec);trim_text(value);
    if(!value[0])return 0;
    if(value[0]=='~'&&(value[1]=='/'||value[1]==0)){
        char expanded[PATH_MAX];snprintf(expanded,sizeof(expanded),"%s%s",home_dir(),value+1);
        safe_copy(value,sizeof(value),expanded);
    }
    if(strchr(value,'/')||value[0]=='.'){
        char resolved[PATH_MAX];struct stat st;
        if(!realpath(value,resolved)||stat(resolved,&st)!=0||!S_ISREG(st.st_mode)||
           !font_file_extension_supported(resolved))return 0;
        safe_copy(out,cap,resolved);return 1;
    }
    if(!program_exists("fc-match"))return 0;
    char matched[PATH_MAX];
    char *argv[]={"fc-match","-f","%{file}\n",value,NULL};
    if(!pipe_read_program("fc-match",argv,matched,sizeof(matched)))return 0;
    char *nl=strpbrk(matched,"\r\n");if(nl)*nl=0;trim_text(matched);
    struct stat st;
    if(!matched[0]||stat(matched,&st)!=0||!S_ISREG(st.st_mode)||
       !font_file_extension_supported(matched))return 0;
    safe_copy(out,cap,matched);return 1;
}

static const char *font_family_spec(void) {
    if(g.font_family==1)return "sans-serif:lang=ja";
    if(g.font_family==2)return "serif:lang=ja";
    if(g.font_family==3)return "monospace:lang=ja";
    if(g.font_family==4)return g.custom_font;
    return NULL;
}

static void apply_font_environment(void) {
    const char *spec=font_family_spec();
    if(!spec||!*spec)return;
    char path[PATH_MAX];
    if(!resolve_font_spec(spec,path,sizeof(path))){
        log_error("configured font could not be resolved: %s",spec);
        return;
    }
    setenv("LUNA_FONT_REGULAR",path,1);
    setenv("LUNA_FONT_CJK",path,1);
    if(g.font_family==3)setenv("LUNA_FONT_MONO",path,1);
}

static int run_program_wait(const char *prog, char *const argv[]) {
    pid_t pid=fork();
    if(pid<0)return 0;
    if(pid==0){
        int dev=open("/dev/null",O_WRONLY);
        if(dev>=0){dup2(dev,STDOUT_FILENO);dup2(dev,STDERR_FILENO);close(dev);}
        execvp(prog,argv);_exit(127);
    }
    int st=0;
    while(waitpid(pid,&st,0)<0){if(errno!=EINTR)return 0;}
    return WIFEXITED(st)&&WEXITSTATUS(st)==0;
}

static int spawn_detached(char *const argv[], const char *cwd) {
    pid_t pid=fork();
    if(pid<0)return 0;
    if(pid==0){
        pid_t child=fork();
        if(child<0)_exit(127);
        if(child>0)_exit(0);
        setsid();
        if(cwd&&*cwd&&chdir(cwd)!=0)_exit(126);
        int dev=open("/dev/null",O_RDWR);
        if(dev>=0){dup2(dev,STDIN_FILENO);dup2(dev,STDOUT_FILENO);dup2(dev,STDERR_FILENO);if(dev>STDERR_FILENO)close(dev);}
        execvp(argv[0],argv);_exit(127);
    }
    int st=0;
    while(waitpid(pid,&st,0)<0){if(errno!=EINTR)return 0;}
    return WIFEXITED(st)&&WEXITSTATUS(st)==0;
}

static int query_xdg_mime_type(const char *path, char *out, size_t cap) {
    if(!program_exists("xdg-mime"))return 0;
    char *argv[]={"xdg-mime","query","filetype",(char*)path,NULL};
    if(!pipe_read_program("xdg-mime",argv,out,cap))return 0;
    trim_text(out);
    return *out&&strchr(out,'/');
}

static int query_xdg_default_app(const char *mime, char *out, size_t cap) {
    if(!program_exists("xdg-mime"))return 0;
    char *argv[]={"xdg-mime","query","default",(char*)mime,NULL};
    if(!pipe_read_program("xdg-mime",argv,out,cap)){if(cap)out[0]=0;return 0;}
    trim_text(out);
    return *out!=0;
}

static int valid_desktop_id(const char *id) {
    if(!id||!*id||strchr(id,'/'))return 0;
    size_t n=strlen(id);
    if(n<9||strcmp(id+n-8,".desktop"))return 0;
    for(const unsigned char *p=(const unsigned char*)id;*p;p++)
        if(!(isalnum(*p)||*p=='.'||*p=='_'||*p=='-'||*p=='+'))return 0;
    return 1;
}

static int set_xdg_default_app(const char *desktop_id, const char *mime) {
    if(!program_exists("xdg-mime"))return 0;
    char *argv[]={"xdg-mime","default",(char*)desktop_id,(char*)mime,NULL};
    return run_program_wait("xdg-mime",argv);
}

static void shell_quote(const char *src, char *out, size_t cap) {
    size_t w=0;
    if(cap==0)return;
    if(w+1<cap)out[w++]='\'';
    for(const char *p=src?src:"";*p&&w+5<cap;p++){
        if(*p=='\''){out[w++]='\'';out[w++]='\\';out[w++]='\'';out[w++]='\'';}
        else out[w++]=*p;
    }
    if(w+1<cap)out[w++]='\'';
    out[w<cap?w:cap-1]=0;
}



static int append_text(char *out, size_t cap, const char *fmt, ...) {
    if(!out||cap==0)return 0;
    size_t used=strlen(out);
    if(used>=cap-1)return 0;
    va_list ap;va_start(ap,fmt);
    int n=vsnprintf(out+used,cap-used,fmt,ap);
    va_end(ap);
    return n>=0&&(size_t)n<cap-used;
}

static int add_open_app_id(const char *value) {
    if(!value||!*value||g.open_app_count>=MAX_OPEN_APPS)return 0;
    char id[256];safe_copy(id,sizeof(id),value);trim_text(id);
    while(id[0]&&strchr("'\"([{<",id[0]))memmove(id,id+1,strlen(id));
    size_t n=strlen(id);
    while(n&&strchr("'\".,;:)]}>",id[n-1]))id[--n]=0;
    if(!valid_desktop_id(id))return 0;
    for(int i=0;i<g.open_app_count;i++)if(!strcmp(g.open_app_ids[i],id))return 0;
    safe_copy(g.open_app_ids[g.open_app_count],sizeof(g.open_app_ids[0]),id);
    g.open_app_names[g.open_app_count][0]=0;
    g.open_app_files[g.open_app_count][0]=0;
    g.open_app_count++;
    return 1;
}

static void collect_ids_from_value(const char *value) {
    if(!value||!*value)return;
    char copy[8192];safe_copy(copy,sizeof(copy),value);
    char *save=NULL;
    for(char *p=strtok_r(copy,";",&save);p;p=strtok_r(NULL,";",&save))add_open_app_id(p);
}

static void collect_ids_from_association_file(const char *path, const char *mime) {
    FILE *fp=fopen(path,"r");if(!fp)return;
    char line[16384];int removed_section=0;
    while(fgets(line,sizeof(line),fp)){
        char *nl=strpbrk(line,"\r\n");if(nl)*nl=0;
        trim_text(line);if(!line[0]||line[0]=='#')continue;
        if(line[0]=='['){removed_section=!strcasecmp(line,"[Removed Associations]");continue;}
        if(removed_section)continue;
        char *eq=strchr(line,'=');if(!eq)continue;*eq++=0;trim_text(line);
        if(!strcmp(line,mime))collect_ids_from_value(eq);
    }
    fclose(fp);
}

static void collect_ids_from_gio(const char *mime) {
    if(!program_exists("gio"))return;
    char output[32768];char *argv[]={"gio","mime",(char*)mime,NULL};
    if(!pipe_read_program("gio",argv,output,sizeof(output)))return;
    char *save=NULL;
    for(char *p=strtok_r(output," \t\r\n",&save);p;p=strtok_r(NULL," \t\r\n",&save))add_open_app_id(p);
}

static int resolve_desktop_file(const char *desktop_id, char *out, size_t cap) {
    if(!desktop_id||!*desktop_id)return 0;
    const char *data_home=getenv("XDG_DATA_HOME");
    char path[PATH_MAX];
    if(data_home&&*data_home)snprintf(path,sizeof(path),"%s/applications/%s",data_home,desktop_id);
    else snprintf(path,sizeof(path),"%s/.local/share/applications/%s",home_dir(),desktop_id);
    if(access(path,R_OK)==0){safe_copy(out,cap,path);return 1;}
    snprintf(path,sizeof(path),"%s/.local/share/flatpak/exports/share/applications/%s",home_dir(),desktop_id);
    if(access(path,R_OK)==0){safe_copy(out,cap,path);return 1;}
    const char *dirs=getenv("XDG_DATA_DIRS");if(!dirs||!*dirs)dirs="/usr/local/share:/usr/share";
    char *copy=strdup(dirs),*save=NULL;
    if(copy){
        for(char *d=strtok_r(copy,":",&save);d;d=strtok_r(NULL,":",&save)){
            snprintf(path,sizeof(path),"%s/applications/%s",d,desktop_id);
            if(access(path,R_OK)==0){safe_copy(out,cap,path);free(copy);return 1;}
        }
        free(copy);
    }
    snprintf(path,sizeof(path),"/var/lib/flatpak/exports/share/applications/%s",desktop_id);
    if(access(path,R_OK)==0){safe_copy(out,cap,path);return 1;}
    if(cap)out[0]=0;
    return 0;
}

static void desktop_display_name(const char *desktop_file, const char *desktop_id, char *out, size_t cap) {
    char fallback[256];safe_copy(fallback,sizeof(fallback),desktop_id?desktop_id:"");
    char *suffix=strstr(fallback,".desktop");if(suffix&&suffix[8]==0)*suffix=0;
    safe_copy(out,cap,fallback);
    if(!desktop_file||!*desktop_file)return;
    FILE *fp=fopen(desktop_file,"r");if(!fp)return;
    char line[2048],generic[256]="",ja[256]="";int in_entry=0;
    while(fgets(line,sizeof(line),fp)){
        char *nl=strpbrk(line,"\r\n");if(nl)*nl=0;
        if(line[0]=='['){in_entry=!strcmp(line,"[Desktop Entry]");continue;}
        if(!in_entry)continue;
        if(!strncmp(line,"Name[ja_JP]=",12))safe_copy(ja,sizeof(ja),line+12);
        else if(!strncmp(line,"Name[ja]=",9)&&!ja[0])safe_copy(ja,sizeof(ja),line+9);
        else if(!strncmp(line,"Name=",5)&&!generic[0])safe_copy(generic,sizeof(generic),line+5);
    }
    fclose(fp);
    if(ja[0])safe_copy(out,cap,ja);else if(generic[0])safe_copy(out,cap,generic);
}

static int desktop_exec_line(const char *desktop_file, char *exec_line, size_t exec_cap) {
    if(!desktop_file||!*desktop_file)return 0;
    FILE *fp=fopen(desktop_file,"r");if(!fp)return 0;
    char line[4096];int in_entry=0,ok=0;
    while(fgets(line,sizeof(line),fp)){
        char *nl=strpbrk(line,"\r\n");if(nl)*nl=0;
        if(line[0]=='['){in_entry=!strcmp(line,"[Desktop Entry]");continue;}
        if(in_entry&&!strncmp(line,"Exec=",5)){safe_copy(exec_line,exec_cap,line+5);ok=exec_line[0]!=0;break;}
    }
    fclose(fp);return ok;
}


static int desktop_file_supports_mime(const char *desktop_file, const char *mime) {
    FILE *fp=fopen(desktop_file,"r");if(!fp)return 0;
    char line[8192];int in_entry=0,matched=0;
    while(fgets(line,sizeof(line),fp)){
        char *nl=strpbrk(line,"\r\n");if(nl)*nl=0;
        if(line[0]=='['){in_entry=!strcmp(line,"[Desktop Entry]");continue;}
        if(!in_entry||strncmp(line,"MimeType=",9))continue;
        char *save=NULL;
        for(char *v=strtok_r(line+9,";",&save);v;v=strtok_r(NULL,";",&save)){
            if(!strcmp(v,mime)){matched=1;break;}
        }
        break;
    }
    fclose(fp);return matched;
}

static void collect_ids_from_desktop_directory(const char *dir, const char *mime) {
    DIR *d=opendir(dir);if(!d)return;struct dirent *de;
    while(g.open_app_count<MAX_OPEN_APPS&&(de=readdir(d))){
        size_t n=strlen(de->d_name);if(n<9||strcmp(de->d_name+n-8,".desktop"))continue;
        char path[PATH_MAX];if(!path_join(path,sizeof(path),dir,de->d_name))continue;
        if(desktop_file_supports_mime(path,mime))add_open_app_id(de->d_name);
    }
    closedir(d);
}

static void collect_open_apps(const char *path, char *mime, size_t mime_cap) {
    g.open_app_count=0;if(mime_cap)mime[0]=0;
    if(!query_xdg_mime_type(path,mime,mime_cap))return;
    char default_id[256]="";query_xdg_default_app(mime,default_id,sizeof(default_id));
    if(default_id[0])add_open_app_id(default_id);
    collect_ids_from_gio(mime);

    char file[PATH_MAX];const char *cfg=getenv("XDG_CONFIG_HOME");
    if(cfg&&*cfg)snprintf(file,sizeof(file),"%s/mimeapps.list",cfg);
    else snprintf(file,sizeof(file),"%s/.config/mimeapps.list",home_dir());
    collect_ids_from_association_file(file,mime);
    snprintf(file,sizeof(file),"%s/.local/share/applications/mimeapps.list",home_dir());collect_ids_from_association_file(file,mime);
    snprintf(file,sizeof(file),"%s/.local/share/applications/mimeinfo.cache",home_dir());collect_ids_from_association_file(file,mime);

    const char *dirs=getenv("XDG_DATA_DIRS");if(!dirs||!*dirs)dirs="/usr/local/share:/usr/share";
    char *copy=strdup(dirs),*save=NULL;
    if(copy){
        for(char *d=strtok_r(copy,":",&save);d;d=strtok_r(NULL,":",&save)){
            snprintf(file,sizeof(file),"%s/applications/mimeapps.list",d);collect_ids_from_association_file(file,mime);
            snprintf(file,sizeof(file),"%s/applications/mimeinfo.cache",d);collect_ids_from_association_file(file,mime);
        }
        free(copy);
    }
    snprintf(file,sizeof(file),"%s/.local/share/flatpak/exports/share/applications/mimeinfo.cache",home_dir());collect_ids_from_association_file(file,mime);
    collect_ids_from_association_file("/var/lib/flatpak/exports/share/applications/mimeinfo.cache",mime);

    /* Some lightweight systems do not generate mimeinfo.cache. Scan desktop
       entries as a fallback so the chooser can still show every compatible app. */
    snprintf(file,sizeof(file),"%s/.local/share/applications",home_dir());collect_ids_from_desktop_directory(file,mime);
    snprintf(file,sizeof(file),"%s/.local/share/flatpak/exports/share/applications",home_dir());collect_ids_from_desktop_directory(file,mime);
    copy=strdup(dirs);save=NULL;
    if(copy){for(char *d=strtok_r(copy,":",&save);d&&g.open_app_count<MAX_OPEN_APPS;d=strtok_r(NULL,":",&save)){snprintf(file,sizeof(file),"%s/applications",d);collect_ids_from_desktop_directory(file,mime);}free(copy);}
    collect_ids_from_desktop_directory("/var/lib/flatpak/exports/share/applications",mime);

    for(int i=0;i<g.open_app_count;i++){
        resolve_desktop_file(g.open_app_ids[i],g.open_app_files[i],sizeof(g.open_app_files[i]));
        desktop_display_name(g.open_app_files[i],g.open_app_ids[i],g.open_app_names[i],sizeof(g.open_app_names[i]));
    }
}

static int build_desktop_exec_command(const char *exec_line, const char *path, const char *desktop_file, const char *app_name, char *out, size_t cap) {
    if(!exec_line||!*exec_line||!out||cap==0)return 0;
    char qpath[PATH_MAX*2],qdesktop[PATH_MAX*2],qname[1024];
    shell_quote(path,qpath,sizeof(qpath));shell_quote(desktop_file,qdesktop,sizeof(qdesktop));shell_quote(app_name,qname,sizeof(qname));
    size_t w=0;int used_path=0;
    for(size_t i=0;exec_line[i];i++){
        const char *add=NULL;char one[2]={0};
        if(exec_line[i]=='%'&&exec_line[i+1]){
            char code=exec_line[++i];
            if(code=='f'||code=='F'||code=='u'||code=='U'){add=qpath;used_path=1;}
            else if(code=='c')add=qname;
            else if(code=='k')add=qdesktop;
            else if(code=='%'){one[0]='%';add=one;}
            else continue;
        }else{one[0]=exec_line[i];add=one;}
        size_t n=strlen(add);if(w+n+1>cap)return 0;memcpy(out+w,add,n);w+=n;
    }
    if(!used_path){size_t n=strlen(qpath);if(w+n+2>cap)return 0;out[w++]=' ';memcpy(out+w,qpath,n);w+=n;}
    out[w]=0;return 1;
}

static int launch_desktop_application(int slot, const char *path) {
    if(slot<0||slot>=g.open_app_count||!path||!*path)return 0;
    const char *id=g.open_app_ids[slot],*desktop=g.open_app_files[slot];
    if(desktop[0]&&program_exists("gio")){
        char *argv[]={"gio","launch",(char*)desktop,(char*)path,NULL};
        if(spawn_detached(argv,NULL))return 1;
    }
    if(program_exists("gtk-launch")){
        char *argv[]={"gtk-launch",(char*)id,(char*)path,NULL};
        if(spawn_detached(argv,NULL))return 1;
    }
    char exec_line[4096],command[8192];
    if(desktop_exec_line(desktop,exec_line,sizeof(exec_line))&&
       build_desktop_exec_command(exec_line,path,desktop,g.open_app_names[slot],command,sizeof(command))){
        char *argv[]={"sh","-c",command,NULL};
        if(spawn_detached(argv,NULL))return 1;
    }
    return 0;
}

static int is_desktop_filename(const char *name) {
    size_t n;
    if (!name || !*name) return 0;
    n = strlen(name);
    return n > 8 && !strcasecmp(name + n - 8, ".desktop");
}

static int desktop_key_managed(const char *key) {
    return !strcmp(key, "Type") || !strcmp(key, "Version") || !strcmp(key, "Name") ||
           !strcmp(key, "Comment") || !strcmp(key, "Exec") || !strcmp(key, "URL") ||
           !strcmp(key, "Icon") || !strcmp(key, "Path") || !strcmp(key, "Terminal");
}

static void desktop_entry_defaults(void) {
    safe_copy(g.desktop_edit_type, sizeof(g.desktop_edit_type), "Application");
    g.desktop_edit_terminal = 0;
    g.desktop_edit_extra[0] = 0;
    g.desktop_edit_path[0] = 0;
}

static int load_desktop_entry_file(const char *path,
                                   char *name, size_t name_cap,
                                   char *comment, size_t comment_cap,
                                   char *exec_or_url, size_t exec_cap,
                                   char *icon, size_t icon_cap,
                                   char *workdir, size_t workdir_cap) {
    FILE *fp;
    char line[2048];
    int in_entry = 0, saw_entry = 0;
    if (name_cap) name[0] = 0;
    if (comment_cap) comment[0] = 0;
    if (exec_cap) exec_or_url[0] = 0;
    if (icon_cap) icon[0] = 0;
    if (workdir_cap) workdir[0] = 0;
    desktop_entry_defaults();
    safe_copy(g.desktop_edit_path, sizeof(g.desktop_edit_path), path ? path : "");
    fp = fopen(path, "r");
    if (!fp) return 0;
    while (fgets(line, sizeof(line), fp)) {
        char *nl = strpbrk(line, "\r\n");
        char *eq;
        char key[128];
        const char *val;
        size_t key_len;
        if (nl) *nl = 0;
        if (line[0] == '[') {
            if (!strcmp(line, "[Desktop Entry]")) {
                in_entry = 1;
                saw_entry = 1;
            } else {
                in_entry = 0;
                append_text(g.desktop_edit_extra, sizeof(g.desktop_edit_extra), "%s\n", line);
            }
            continue;
        }
        if (!in_entry) {
            append_text(g.desktop_edit_extra, sizeof(g.desktop_edit_extra), "%s\n", line);
            continue;
        }
        if (!line[0] || line[0] == '#') {
            append_text(g.desktop_edit_extra, sizeof(g.desktop_edit_extra), "%s\n", line);
            continue;
        }
        eq = strchr(line, '=');
        if (!eq) {
            append_text(g.desktop_edit_extra, sizeof(g.desktop_edit_extra), "%s\n", line);
            continue;
        }
        key_len = (size_t)(eq - line);
        if (key_len >= sizeof(key)) key_len = sizeof(key) - 1;
        memcpy(key, line, key_len);
        key[key_len] = 0;
        val = eq + 1;
        /* Localized keys (Name[ja]=...) and unmanaged keys stay in extra. */
        if (!desktop_key_managed(key) || strchr(key, '[')) {
            append_text(g.desktop_edit_extra, sizeof(g.desktop_edit_extra), "%s\n", line);
            continue;
        }
        if (!strcmp(key, "Version")) continue;
        if (!strcmp(key, "Type")) safe_copy(g.desktop_edit_type, sizeof(g.desktop_edit_type), val);
        else if (!strcmp(key, "Name")) safe_copy(name, name_cap, val);
        else if (!strcmp(key, "Comment")) safe_copy(comment, comment_cap, val);
        else if (!strcmp(key, "Exec") || !strcmp(key, "URL")) safe_copy(exec_or_url, exec_cap, val);
        else if (!strcmp(key, "Icon")) safe_copy(icon, icon_cap, val);
        else if (!strcmp(key, "Path")) safe_copy(workdir, workdir_cap, val);
        else if (!strcmp(key, "Terminal"))
            g.desktop_edit_terminal = (!strcasecmp(val, "true") || !strcmp(val, "1"));
    }
    fclose(fp);
    if (!g.desktop_edit_type[0]) safe_copy(g.desktop_edit_type, sizeof(g.desktop_edit_type), "Application");
    return saw_entry || (name && name[0]) || (exec_or_url && exec_or_url[0]);
}

static void suggest_desktop_filename(const char *name, char *out, size_t cap) {
    size_t w = 0;
    const unsigned char *p = (const unsigned char *)(name ? name : "");
    if (!out || !cap) return;
    for (; *p && w + 9 < cap; p++) {
        unsigned char c = *p;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
            out[w++] = (char)c;
        else if (c == '-' || c == '_' || c == '.')
            out[w++] = (char)c;
        else if ((c == ' ' || c == '\t') && w > 0 && out[w - 1] != '-')
            out[w++] = '-';
    }
    while (w && (out[w - 1] == '-' || out[w - 1] == '.')) w--;
    if (!w) {
        safe_copy(out, cap, "launcher.desktop");
        return;
    }
    out[w] = 0;
    if (w + 8 < cap) {
        memcpy(out + w, ".desktop", 8);
        out[w + 8] = 0;
    } else
        safe_copy(out, cap, "launcher.desktop");
}

static int write_desktop_entry_file(const char *path,
                                    const char *type,
                                    const char *name,
                                    const char *comment,
                                    const char *exec_or_url,
                                    const char *icon,
                                    const char *workdir,
                                    int terminal,
                                    const char *extra) {
    FILE *fp;
    int is_link, is_dir;
    if (!path || !*path || !type || !*type || !name || !*name) return 0;
    is_link = !strcmp(type, "Link");
    is_dir = !strcmp(type, "Directory");
    if (!is_dir && (!exec_or_url || !*exec_or_url)) return 0;
    fp = fopen(path, "w");
    if (!fp) return 0;
    fprintf(fp, "[Desktop Entry]\n");
    fprintf(fp, "Version=1.0\n");
    fprintf(fp, "Type=%s\n", type);
    fprintf(fp, "Name=%s\n", name);
    if (comment && *comment) fprintf(fp, "Comment=%s\n", comment);
    if (is_link) fprintf(fp, "URL=%s\n", exec_or_url);
    else if (!is_dir) fprintf(fp, "Exec=%s\n", exec_or_url);
    if (icon && *icon) fprintf(fp, "Icon=%s\n", icon);
    if (!is_link && !is_dir && workdir && *workdir) fprintf(fp, "Path=%s\n", workdir);
    if (!is_link && !is_dir) fprintf(fp, "Terminal=%s\n", terminal ? "true" : "false");
    if (extra && *extra) {
        const char *p = extra;
        while (*p) {
            const char *eol = strchr(p, '\n');
            size_t n = eol ? (size_t)(eol - p) : strlen(p);
            if (n) fwrite(p, 1, n, fp);
            fputc('\n', fp);
            if (!eol) break;
            p = eol + 1;
        }
    }
    if (fclose(fp) != 0) return 0;
    chmod(path, 0755);
    return 1;
}

static void sync_desktop_edit_type_controls(void) {
    int is_link = !strcmp(g.desktop_edit_type, "Link");
    int is_dir = !strcmp(g.desktop_edit_type, "Directory");
    const char *type_label = is_link ? "リンク" : is_dir ? "ディレクトリ" : "アプリケーション";
    char type_row[128], term[128];
    snprintf(type_row, sizeof(type_row), "種類　　%s", type_label);
    luna_set_text(g.id_desktop_edit_type, type_row);
    luna_set_text(g.id_desktop_edit_exec_label, is_link ? "URL" : "コマンド (Exec)");
    luna_update_classes(g.id_desktop_edit_workdir_row, "hidden", (is_link || is_dir) ? "hidden" : "");
    luna_update_classes(g.id_desktop_edit_terminal_row, "hidden", (is_link || is_dir) ? "hidden" : "");
    snprintf(term, sizeof(term), "ターミナルで実行　　%s", g.desktop_edit_terminal ? "はい" : "いいえ");
    luna_set_text(g.id_desktop_edit_terminal, term);
    luna_update_classes(g.id_desktop_edit_type, "on", "on");
    luna_update_classes(g.id_desktop_edit_terminal, "on", g.desktop_edit_terminal ? "on" : "");
}

static void close_desktop_edit_dialog(void) {
    if (!g.desktop_edit_open) return;
    luna_pop_focus_trap(g.id_desktop_edit);
    luna_add_class(g.id_desktop_edit, "hidden");
    g.desktop_edit_open = 0;
    g.desktop_edit_create = 0;
    g.desktop_edit_path[0] = 0;
    g.desktop_edit_extra[0] = 0;
    request_overlay_layout();
}

static void show_desktop_edit_dialog(int create, int target) {
    char name[256] = "", comment[512] = "", exec_or_url[1024] = "", icon[512] = "", workdir[PATH_MAX] = "";
    char filename[NAME_MAX + 1] = "", hint[768];
    hide_context();
    if (g.open_with_open) close_open_with_dialog();
    if (g.modal_kind) close_modal();
    if (g.settings_open) {
        luna_add_class(g.id_settings, "overlay-idle");
        luna_pop_focus_trap(g.id_settings);
        g.settings_open = 0;
    }
    desktop_entry_defaults();
    if (create) {
        g.desktop_edit_create = 1;
        g.desktop_edit_path[0] = 0;
        safe_copy(name, sizeof(name), "新しいランチャー");
        safe_copy(exec_or_url, sizeof(exec_or_url), "");
        safe_copy(icon, sizeof(icon), "application-x-executable");
        suggest_desktop_filename(name, filename, sizeof(filename));
        snprintf(hint, sizeof(hint),
                 "現在のフォルダーに XDG Desktop Entry（.desktop）を作成します。\n"
                 "Exec にはコマンドと %%f / %%u などのプレースホルダを書けます。");
        luna_set_text(g.id_desktop_edit_title, "ランチャーを作成");
    } else {
        if (target < 0 || target >= g.entry_count || g.entries[target].is_dir ||
            !is_desktop_filename(g.entries[target].name)) {
            show_toast(".desktop ファイルを選択してください");
            return;
        }
        if (!load_desktop_entry_file(g.entries[target].path,
                                     name, sizeof(name),
                                     comment, sizeof(comment),
                                     exec_or_url, sizeof(exec_or_url),
                                     icon, sizeof(icon),
                                     workdir, sizeof(workdir))) {
            show_toast("デスクトップエントリを読み込めませんでした");
            return;
        }
        g.desktop_edit_create = 0;
        safe_copy(filename, sizeof(filename), g.entries[target].name);
        snprintf(hint, sizeof(hint),
                 "ファイル: %s\n書き込みできない場合は ~/.local/share/applications にコピーして保存します。",
                 g.entries[target].path);
        luna_set_text(g.id_desktop_edit_title, "デスクトップエントリを編集");
    }
    luna_set_text(g.id_desktop_edit_hint, hint);
    luna_set_value(g.id_desktop_edit_name, name);
    luna_set_value(g.id_desktop_edit_comment, comment);
    luna_set_value(g.id_desktop_edit_exec, exec_or_url);
    luna_set_value(g.id_desktop_edit_icon, icon);
    luna_set_value(g.id_desktop_edit_workdir, workdir);
    luna_set_value(g.id_desktop_edit_filename, filename);
    luna_update_classes(g.id_desktop_edit_filename_row, "hidden", create ? "" : "hidden");
    sync_desktop_edit_type_controls();
    g.desktop_edit_open = 1;
    luna_remove_class(g.id_desktop_edit, "hidden");
    luna_push_focus_trap(g.id_desktop_edit, NULL, 0);
    luna_focus_element(g.id_desktop_edit_name);
    request_overlay_layout();
}

static int save_desktop_edit_dialog(void) {
    char name[256], comment[512], exec_or_url[1024], icon[512], workdir[PATH_MAX], filename[NAME_MAX + 1];
    char dest[PATH_MAX];
    const char *type = g.desktop_edit_type;
    int is_link = !strcmp(type, "Link");
    int is_dir = !strcmp(type, "Directory");
    safe_copy(name, sizeof(name), luna_get_value(g.id_desktop_edit_name));
    safe_copy(comment, sizeof(comment), luna_get_value(g.id_desktop_edit_comment));
    safe_copy(exec_or_url, sizeof(exec_or_url), luna_get_value(g.id_desktop_edit_exec));
    safe_copy(icon, sizeof(icon), luna_get_value(g.id_desktop_edit_icon));
    safe_copy(workdir, sizeof(workdir), luna_get_value(g.id_desktop_edit_workdir));
    safe_copy(filename, sizeof(filename), luna_get_value(g.id_desktop_edit_filename));
    trim_text(name); trim_text(comment); trim_text(exec_or_url); trim_text(icon); trim_text(workdir); trim_text(filename);
    if (!name[0]) { show_toast("名前を入力してください"); return 0; }
    if (!is_dir && !exec_or_url[0]) {
        show_toast(is_link ? "URL を入力してください" : "コマンドを入力してください");
        return 0;
    }
    if (g.desktop_edit_create) {
        if (!filename[0]) suggest_desktop_filename(name, filename, sizeof(filename));
        if (strchr(filename, '/') || filename[0] == '.') { show_toast("ファイル名が不正です"); return 0; }
        if (!is_desktop_filename(filename)) {
            size_t n = strlen(filename);
            if (n + 8 >= sizeof(filename)) { show_toast("ファイル名が長すぎます"); return 0; }
            memcpy(filename + n, ".desktop", 8);
            filename[n + 8] = 0;
        }
        if (!unique_destination(g.cwd, filename, dest, sizeof(dest))) {
            show_toast("保存先を決められませんでした");
            return 0;
        }
    } else {
        safe_copy(dest, sizeof(dest), g.desktop_edit_path);
        if (access(dest, W_OK) != 0) {
            char local_dir[PATH_MAX], local_name[NAME_MAX + 1];
            snprintf(local_dir, sizeof(local_dir), "%s/.local/share/applications", home_dir());
            ensure_dir(local_dir, 0755);
            safe_copy(local_name, sizeof(local_name), base_name(dest));
            if (!unique_destination(local_dir, local_name, dest, sizeof(dest))) {
                show_toast("ユーザー領域へコピー保存できませんでした");
                return 0;
            }
            show_toast("書き込み不可のためユーザー領域に保存します");
        }
    }
    if (!write_desktop_entry_file(dest, type, name, comment, exec_or_url, icon, workdir,
                                  g.desktop_edit_terminal, g.desktop_edit_extra)) {
        show_toast("保存に失敗しました: %s", strerror(errno));
        return 0;
    }
    {
        int created = g.desktop_edit_create;
        close_desktop_edit_dialog();
        load_directory(g.cwd);
        show_toast(created ? "ランチャーを作成しました" : "デスクトップエントリを保存しました");
    }
    return 1;
}

static void format_time_value(time_t value, char *out, size_t cap) {
    struct tm tmv;if(!localtime_r(&value,&tmv)){safe_copy(out,cap,"不明");return;}
    strftime(out,cap,"%Y/%m/%d %H:%M:%S",&tmv);
}

static void permission_string(mode_t mode, char out[11]) {
    out[0]=S_ISDIR(mode)?'d':S_ISLNK(mode)?'l':S_ISCHR(mode)?'c':S_ISBLK(mode)?'b':S_ISFIFO(mode)?'p':S_ISSOCK(mode)?'s':'-';
    const mode_t bits[9]={S_IRUSR,S_IWUSR,S_IXUSR,S_IRGRP,S_IWGRP,S_IXGRP,S_IROTH,S_IWOTH,S_IXOTH};
    const char chars[9]={'r','w','x','r','w','x','r','w','x'};
    for(int i=0;i<9;i++)out[i+1]=(mode&bits[i])?chars[i]:'-';
    out[10]=0;
}

static const char *mode_type_name(mode_t mode) {
    if(S_ISDIR(mode))return "フォルダー";
    if(S_ISLNK(mode))return "シンボリックリンク";
    if(S_ISREG(mode))return "通常ファイル";
    if(S_ISCHR(mode))return "キャラクターデバイス";
    if(S_ISBLK(mode))return "ブロックデバイス";
    if(S_ISFIFO(mode))return "FIFO";
    if(S_ISSOCK(mode))return "ソケット";
    return "その他";
}

static void directory_direct_summary(const char *path, int *files, int *dirs, off_t *bytes) {
    *files=*dirs=0;*bytes=0;DIR *d=opendir(path);if(!d)return;struct dirent *de;
    while((de=readdir(d))){
        if(!strcmp(de->d_name,".")||!strcmp(de->d_name,".."))continue;
        char child[PATH_MAX];struct stat st;if(!path_join(child,sizeof(child),path,de->d_name)||lstat(child,&st)!=0)continue;
        if(S_ISDIR(st.st_mode)&&!S_ISLNK(st.st_mode))(*dirs)++;else{(*files)++;*bytes+=st.st_size;}
    }
    closedir(d);
}

static void build_properties_info(const FileEntry *f, char *info, size_t cap) {
    info[0]=0;struct stat st;if(lstat(f->path,&st)!=0){append_text(info,cap,"情報を取得できません: %s",strerror(errno));return;}
    char parent[PATH_MAX],size[64],allocated[64],modified[64],accessed[64],changed[64],perms[11],mime[128]="";
    parent_path(f->path,parent,sizeof(parent));human_size(st.st_size,size,sizeof(size));human_size((off_t)st.st_blocks*512,allocated,sizeof(allocated));
    format_time_value(st.st_mtime,modified,sizeof(modified));format_time_value(st.st_atime,accessed,sizeof(accessed));format_time_value(st.st_ctime,changed,sizeof(changed));
    permission_string(st.st_mode,perms);query_xdg_mime_type(f->path,mime,sizeof(mime));
    struct passwd *pw=getpwuid(st.st_uid);char owner[256];safe_copy(owner,sizeof(owner),pw&&pw->pw_name?pw->pw_name:"不明");
    struct group *gr=getgrgid(st.st_gid);char group_name[256];safe_copy(group_name,sizeof(group_name),gr&&gr->gr_name?gr->gr_name:"不明");

    const char *ext=strrchr(f->name,'.');
    if(ext==f->name||!ext||!ext[1])ext=NULL;

    append_text(info,cap,"概要\n────────────────────────\n");
    append_text(info,cap,"名前: %s\n",f->name);
    append_text(info,cap,"種類: %s\n",mode_type_name(st.st_mode));
    append_text(info,cap,"MIMEタイプ: %s\n",mime[0]?mime:"不明");
    append_text(info,cap,"拡張子: %s\n",ext?ext+1:"なし");
    append_text(info,cap,"サイズ: %s (%lld バイト)\nディスク使用量: %s\n",size,(long long)st.st_size,allocated);
    if(S_ISDIR(st.st_mode)){
        int files=0,dirs=0;off_t direct=0;directory_direct_summary(f->path,&files,&dirs,&direct);char direct_size[64];human_size(direct,direct_size,sizeof(direct_size));
        append_text(info,cap,"直下の内容: %d ファイル、%d フォルダー\n直下ファイルの合計: %s\n",files,dirs,direct_size);
    }

    append_text(info,cap,"\n場所\n────────────────────────\nフルパス: %s\n親フォルダー: %s\n",f->path,parent);
    if(S_ISLNK(st.st_mode)){
        char target[PATH_MAX];ssize_t n=readlink(f->path,target,sizeof(target)-1);if(n>=0){target[n]=0;append_text(info,cap,"リンク先: %s\n",target);}
    }

    append_text(info,cap,"\n日時\n────────────────────────\n更新日時: %s\nアクセス日時: %s\nメタデータ変更日時: %s\n",modified,accessed,changed);
    append_text(info,cap,"\nアクセス権と所有者\n────────────────────────\nアクセス権: %s (%04o)\n所有者: %s (UID %lu)\nグループ: %s (GID %lu)\n",perms,(unsigned)(st.st_mode&07777),owner,(unsigned long)st.st_uid,group_name,(unsigned long)st.st_gid);
    append_text(info,cap,"読み取り: %s　書き込み: %s　実行: %s\n",access(f->path,R_OK)==0?"可":"不可",access(f->path,W_OK)==0?"可":"不可",access(f->path,X_OK)==0?"可":"不可");
    append_text(info,cap,"\n技術情報\n────────────────────────\n隠し項目: %s\nシンボリックリンク: %s\nハードリンク数: %lu\ninode: %llu\nデバイスID: %llu\n",f->name[0]=='.'?"はい":"いいえ",S_ISLNK(st.st_mode)?"はい":"いいえ",(unsigned long)st.st_nlink,(unsigned long long)st.st_ino,(unsigned long long)st.st_dev);
    struct statvfs fs;if(statvfs(f->path,&fs)==0){
        off_t total=(off_t)((unsigned long long)fs.f_blocks*fs.f_frsize),avail=(off_t)((unsigned long long)fs.f_bavail*fs.f_frsize);char total_s[64],avail_s[64];human_size(total,total_s,sizeof(total_s));human_size(avail,avail_s,sizeof(avail_s));
        append_text(info,cap,"ファイルシステム空き容量: %s / %s\n",avail_s,total_s);
    }
}

static int expand_directory_placeholder(const char *command, const char *dir, char *out, size_t cap) {
    char quoted[PATH_MAX*2];shell_quote(dir,quoted,sizeof(quoted));
    size_t w=0;int replaced=0;
    for(size_t i=0;command&&command[i];i++){
        const char *add=NULL;char one[2]={0};
        if(command[i]=='%'&&command[i+1]=='d'){add=quoted;i++;replaced=1;}
        else if(command[i]=='%'&&command[i+1]=='%'){one[0]='%';add=one;i++;}
        else{one[0]=command[i];add=one;}
        size_t n=strlen(add);
        if(w+n+1>cap)return 0;
        memcpy(out+w,add,n);w+=n;
    }
    if(cap)out[w]=0;
    (void)replaced;
    return 1;
}

static int launch_terminal_command(const char *command, const char *cwd) {
    char expanded[1024];
    if(!command||!*command||!expand_directory_placeholder(command,cwd,expanded,sizeof(expanded)))return 0;
    char *argv[]={"sh","-c",expanded,NULL};
    return spawn_detached(argv,cwd);
}

static void open_terminal_here(void) {
    if(g.terminal_command[0]){
        if(launch_terminal_command(g.terminal_command,g.cwd))show_toast("ターミナルを開きました");
        else show_toast("ターミナルの起動に失敗しました");
        return;
    }
    const char *env=getenv("TERMINAL");
    if(env&&*env&&launch_terminal_command(env,g.cwd)){show_toast("ターミナルを開きました");return;}
    static const char *candidates[]={"xdg-terminal-exec","foot","kitty","alacritty","wezterm","gnome-terminal","konsole","xfce4-terminal","mate-terminal","lxterminal","qterminal","xterm",NULL};
    for(int i=0;candidates[i];i++){
        if(!program_exists(candidates[i]))continue;
        char *argv[]={(char*)candidates[i],NULL};
        if(spawn_detached(argv,g.cwd)){show_toast("ターミナルを開きました");return;}
    }
    show_toast("ターミナルが見つかりません。設定でコマンドを指定してください");
}

static void show_toast(const char *fmt, ...) {
    char msg[512]; va_list ap; va_start(ap,fmt); vsnprintf(msg,sizeof(msg),fmt,ap); va_end(ap);
    luna_set_text(g.id_toast,msg);
    luna_remove_class(g.id_toast,"hidden");
    g.toast_until=glfwGetTime()+TOAST_SECONDS;
    request_redraw();
}

static void hide_context(void) {
    luna_add_class(g.id_context,"hidden");
    g.context_entry=-1;
    hide_volume_context();
    request_overlay_layout();
}

static void update_toolbar_state(void) {
    if (g.history_pos<=0) luna_add_class(g.id_btn_back,"disabled"); else luna_remove_class(g.id_btn_back,"disabled");
    if (g.history_pos<0 || g.history_pos>=g.history_count-1) luna_add_class(g.id_btn_forward,"disabled"); else luna_remove_class(g.id_btn_forward,"disabled");
    if (g.clip_count<=0) luna_add_class(g.id_btn_paste,"disabled"); else luna_remove_class(g.id_btn_paste,"disabled");
    if (g.show_hidden) luna_add_class(g.id_btn_hidden,"on"); else luna_remove_class(g.id_btn_hidden,"on");
    const char *sort = g.sort_mode==SORT_NAME ? "A↓" : g.sort_mode==SORT_DATE ? "日↓" : "量↓";
    if (g.sort_desc) sort = g.sort_mode==SORT_NAME ? "A↑" : g.sort_mode==SORT_DATE ? "日↑" : "量↑";
    luna_set_text(g.id_btn_sort,sort);
    request_redraw();
}

static void update_sidebar_active(void) {
    static const char *ids[9]={"side-home","side-desktop","side-documents","side-downloads","side-pictures","side-music","side-videos","side-trash","side-root"};
    for(int i=0;i<9;i++){
        int id=luna_get_element_by_id(ids[i]);
        if(!strcmp(g.cwd,g.side_paths[i]))luna_add_class(id,"active"); else luna_remove_class(id,"active");
    }
    render_volumes();
    request_redraw();
}

static void clear_selection(void) {
    for(int i=0;i<g.entry_count;i++) g.entries[i].selected=0;
    g.anchor_entry=-1;
}

static int selected_count(void) {
    int n=0; for(int i=0;i<g.entry_count;i++) if(g.entries[i].selected)n++; return n;
}

static off_t selected_size(void) {
    off_t n=0; for(int i=0;i<g.entry_count;i++) if(g.entries[i].selected&&!g.entries[i].is_dir)n+=g.entries[i].size; return n;
}

static int path_in_clipboard(const char *path) {
    for (int i = 0; i < g.clip_count; i++)
        if (!strcmp(g.clip_paths[i], path)) return 1;
    return 0;
}

static void apply_item_classes(int slot, int ei) {
    char id[32], cls_remove[256], cls_add[256];
    snprintf(id,sizeof(id),"item%03d",slot);
    int idx=luna_get_element_by_id(id);
    snprintf(cls_remove,sizeof(cls_remove),"hidden grid-item list-item type-folder type-file type-image type-audio type-video type-code type-archive selected cut");
    if(ei<0){luna_update_classes(idx,cls_remove,"file-item hidden"); return;}
    FileEntry *f=&g.entries[ei];
    snprintf(cls_add,sizeof(cls_add),"file-item %s %s%s%s",g.grid_view?"grid-item":"list-item",type_class(f),f->selected?" selected":"",(g.clip_cut&&path_in_clipboard(f->path))?" cut":"");
    luna_update_classes(idx,cls_remove,cls_add);
}

static void update_file_status(void) {
    char status[256], sz[32];
    int sel=selected_count();
    human_size(selected_size(),sz,sizeof(sz));
    if(sel) snprintf(status,sizeof(status),"%d 個を選択中（%s）  •  %d 個表示 / %d 個%s",sel,sz,g.visible_count,g.entry_count,g.truncated?"（上限384件）":"");
    else snprintf(status,sizeof(status),"%d 個表示 / %d 個%s  •  %s表示  •  Ctrl+C / Ctrl+X / Ctrl+V",g.visible_count,g.entry_count,g.truncated?"（上限384件）":"",g.grid_view?"アイコン":"リスト");
    luna_set_text(g.id_status,status);
    update_toolbar_state();
}

/* Selection and clipboard markers do not change item geometry.  Updating only
   their classes avoids a layout pass, which is the main source of scroll jumps. */
static void refresh_file_visuals(void) {
    for(int slot=0;slot<g.visible_count;slot++)
        apply_item_classes(slot,g.slot_entry[slot]);
    update_file_status();
    request_redraw();
}

static void render_files(void) {
    int old_visible=g.visible_count;
    int slot=0;
    for(int i=0;i<g.entry_count && slot<MAX_ITEMS;i++) {
        FileEntry *f=&g.entries[i];
        if(!g.show_hidden && f->name[0]=='.') continue;
        if(!starts_with_ci(f->name,g.search)) continue;
        g.slot_entry[slot]=i;
        char id[32], meta[96];
        snprintf(id,sizeof(id),"icon%03d",slot);luna_set_text(luna_get_element_by_id(id),type_icon(f));
        snprintf(id,sizeof(id),"name%03d",slot);luna_set_text(luna_get_element_by_id(id),f->name);
        format_meta(f,meta,sizeof(meta));snprintf(id,sizeof(id),"meta%03d",slot);luna_set_text(luna_get_element_by_id(id),meta);
        apply_item_classes(slot,i);
        slot++;
    }
    for(int i=slot;i<old_visible;i++){g.slot_entry[i]=-1;apply_item_classes(i,-1);}
    g.visible_count=slot;
    if(slot==0)luna_remove_class(luna_get_element_by_id("empty"),"hidden");else luna_add_class(luna_get_element_by_id("empty"),"hidden");
    if(g.grid_view)luna_update_classes(g.id_grid,"list","grid");else luna_update_classes(g.id_grid,"grid","list");
    update_file_status();
    luna_mark_layout_dirty();
    request_redraw();
}

static int file_dialog_filter_matches(const char *name) {
    if (!name || !*name || !g_file_dialog_config.filter_patterns ||
        !*g_file_dialog_config.filter_patterns) return 1;
    char patterns[1024];
    safe_copy(patterns, sizeof(patterns), g_file_dialog_config.filter_patterns);
    char *save = NULL;
    for (char *pattern = strtok_r(patterns, " \t;", &save);
         pattern; pattern = strtok_r(NULL, " \t;", &save)) {
        if (!strcmp(pattern, "*") || fnmatch(pattern, name, 0) == 0) return 1;
        /* Common image/source filters should behave case-insensitively even on
           a case-sensitive filesystem. */
        char lower_name[NAME_MAX + 1], lower_pattern[256];
        size_t ni = 0, pi = 0;
        for (; name[ni] && ni + 1 < sizeof(lower_name); ++ni)
            lower_name[ni] = (char)tolower((unsigned char)name[ni]);
        lower_name[ni] = 0;
        for (; pattern[pi] && pi + 1 < sizeof(lower_pattern); ++pi)
            lower_pattern[pi] = (char)tolower((unsigned char)pattern[pi]);
        lower_pattern[pi] = 0;
        if (fnmatch(lower_pattern, lower_name, 0) == 0) return 1;
    }
    return 0;
}

static int load_directory(const char *path) {
    char resolved[PATH_MAX];
    if(!realpath(path,resolved)) { show_toast("開けません: %s",strerror(errno)); return 0; }
    struct stat st; if(stat(resolved,&st)!=0 || !S_ISDIR(st.st_mode)){show_toast("フォルダーではありません");return 0;}
    DIR *d=opendir(resolved); if(!d){show_toast("開けません: %s",strerror(errno));return 0;}
    g.entry_count=0; g.truncated=0;
    struct dirent *de;
    while((de=readdir(d))){
        if(!strcmp(de->d_name,".")||!strcmp(de->d_name,".."))continue;
        if (g.entry_count >= MAX_ITEMS) { g.truncated = 1; continue; }
        FileEntry *f=&g.entries[g.entry_count]; memset(f,0,sizeof(*f));
        safe_copy(f->name,sizeof(f->name),de->d_name);
        if(!path_join(f->path,sizeof(f->path),resolved,de->d_name))continue;
        struct stat fs; if(lstat(f->path,&fs)!=0)continue;
        f->mode=fs.st_mode; f->is_link=S_ISLNK(fs.st_mode); f->is_dir=S_ISDIR(fs.st_mode);
        if(f->is_link){struct stat target;if(stat(f->path,&target)==0&&S_ISDIR(target.st_mode))f->is_dir=1;}
        if(!f->is_dir && g_file_dialog_config.mode!=LUNA_FILE_DIALOG_MANAGER &&
           !file_dialog_filter_matches(f->name)) continue;
        f->size=fs.st_size; f->mtime=fs.st_mtime; g.entry_count++;
    }
    closedir(d);
    qsort(g.entries,(size_t)g.entry_count,sizeof(g.entries[0]),compare_entries);
    safe_copy(g.cwd,sizeof(g.cwd),resolved);
    clear_selection(); g.context_entry=-1;
    luna_set_value(g.id_path,g.cwd);
    luna_set_text(g.id_title,base_name(g.cwd));
    reset_grid_scroll();
    update_sidebar_active(); render_files();
    return 1;
}

static void history_push(const char *path) {
    if(g.history_pos>=0 && !strcmp(g.history[g.history_pos],path)) return;
    if(g.history_pos<g.history_count-1) g.history_count=g.history_pos+1;
    if(g.history_count==MAX_HISTORY){
        memmove(g.history,g.history+1,sizeof(g.history[0])*(MAX_HISTORY-1));
        g.history_count--; g.history_pos--;
    }
    safe_copy(g.history[g.history_count],PATH_MAX,path);
    g.history_count++; g.history_pos=g.history_count-1;
}

static void resolve_navigation_path(const char *input, char *out, size_t cap) {
    if (!input || !*input) { safe_copy(out, cap, g.cwd[0] ? g.cwd : home_dir()); return; }
    if (input[0] == '~' && (input[1] == '/' || input[1] == '\0'))
        snprintf(out, cap, "%s%s", home_dir(), input + 1);
    else if (input[0] == '/') safe_copy(out, cap, input);
    else path_join(out, cap, g.cwd[0] ? g.cwd : home_dir(), input);
}

static size_t utf8_complete_prefix_boundary(const char *s, size_t n) {
    size_t len = strlen(s);
    if (n > len) n = len;
    /* If candidates diverge inside a multibyte character, remove the partial
       character rather than putting invalid UTF-8 into the location input. */
    while (n > 0 && n < len && (((unsigned char)s[n] & 0xc0u) == 0x80u)) n--;
    return n;
}

static size_t common_name_prefix(const char *a, const char *b) {
    size_t n = 0;
    while (a[n] && b[n] && a[n] == b[n]) n++;
    return utf8_complete_prefix_boundary(a, n);
}

static int compare_completion_names(const void *pa, const void *pb) {
    const char *a = (const char *)pa;
    const char *b = (const char *)pb;
    return strcasecmp(a, b);
}

static void reset_path_completion(void) {
    g.completion_display_dir[0] = 0;
    g.completion_last_value[0] = 0;
    g.completion_count = 0;
    g.completion_index = -1;
}

static void finish_path_completion(const char *value) {
    luna_set_value(g.id_path, value);
    LunaElement *input = luna_element_at(g.id_path);
    if (input) {
        input->caret = (int)strlen(input->text);
        input->input_scroll_x = 0.0f;
    }
    request_redraw();
}

static void finish_path_candidate(int index) {
    if (index < 0 || index >= g.completion_count) return;

    char completed[PATH_MAX];
    int n = snprintf(completed, sizeof(completed), "%s%s",
                     g.completion_display_dir, g.completion_names[index]);
    if (n < 0 || (size_t)n >= sizeof(completed)) {
        show_toast("パスが長すぎます");
        reset_path_completion();
        return;
    }

    if (g.completion_is_dir[index]) {
        size_t used = strlen(completed);
        if (used + 1 < sizeof(completed) &&
            (used == 0 || completed[used - 1] != '/')) {
            completed[used] = '/';
            completed[used + 1] = 0;
        }
    }

    g.completion_index = index;
    safe_copy(g.completion_last_value, sizeof(g.completion_last_value), completed);
    finish_path_completion(completed);
    if (g.completion_count > 1)
        show_toast("候補 %d / %d — Tabで次へ", index + 1, g.completion_count);
}

static void complete_path_input(void) {
    const char *raw = luna_get_value(g.id_path);
    char typed[PATH_MAX];
    safe_copy(typed, sizeof(typed), raw ? raw : "");

    /* A second and subsequent Tab cycles the candidates produced by the
       previous completion. Editing the field naturally invalidates this
       branch because its value no longer equals completion_last_value. */
    if (g.completion_count > 0 &&
        !strcmp(typed, g.completion_last_value)) {
        int next = (g.completion_index + 1) % g.completion_count;
        finish_path_candidate(next);
        return;
    }

    reset_path_completion();

    if (!strcmp(typed, "~")) {
        finish_path_completion("~/");
        return;
    }

    const char *slash = strrchr(typed, '/');
    char display_dir[PATH_MAX] = "";
    char dir_part[PATH_MAX] = "";
    const char *prefix = typed;

    if (slash) {
        size_t display_len = (size_t)(slash - typed) + 1;
        if (display_len >= sizeof(display_dir)) {
            show_toast("パスが長すぎます");
            return;
        }
        memcpy(display_dir, typed, display_len);
        display_dir[display_len] = 0;
        prefix = slash + 1;

        size_t dir_len = (size_t)(slash - typed);
        if (dir_len == 0) safe_copy(dir_part, sizeof(dir_part), "/");
        else {
            if (dir_len >= sizeof(dir_part)) {
                show_toast("パスが長すぎます");
                return;
            }
            memcpy(dir_part, typed, dir_len);
            dir_part[dir_len] = 0;
        }
    }

    char scan_dir[PATH_MAX];
    if (slash) resolve_navigation_path(dir_part, scan_dir, sizeof(scan_dir));
    else safe_copy(scan_dir, sizeof(scan_dir), g.cwd[0] ? g.cwd : home_dir());

    DIR *dir = opendir(scan_dir);
    if (!dir) {
        show_toast("補完できません: %s", strerror(errno));
        return;
    }

    char common[NAME_MAX + 1] = "";
    size_t prefix_len = strlen(prefix);
    int matches = 0;
    int stored = 0;
    struct dirent *de;
    while ((de = readdir(dir))) {
        const char *name = de->d_name;
        if (!strcmp(name, ".") || !strcmp(name, "..")) continue;
        if (name[0] == '.' && prefix[0] != '.') continue;
        if (strncmp(name, prefix, prefix_len) != 0) continue;
        if (matches == 0) {
            safe_copy(common, sizeof(common), name);
        } else {
            common[common_name_prefix(common, name)] = 0;
        }
        if (stored < MAX_ITEMS) {
            safe_copy(g.completion_names[stored],
                      sizeof(g.completion_names[stored]), name);
            stored++;
        }
        matches++;
    }
    closedir(dir);

    if (matches == 0) {
        show_toast("一致する項目がありません");
        return;
    }

    /* Complete the common prefix first whenever Tab can add characters. */
    if (strlen(common) > prefix_len) {
        char completed[PATH_MAX];
        int n = snprintf(completed, sizeof(completed), "%s%s", display_dir, common);
        if (n < 0 || (size_t)n >= sizeof(completed)) {
            show_toast("パスが長すぎます");
            return;
        }
        if (matches == 1) {
            char candidate[PATH_MAX];
            if (path_join(candidate, sizeof(candidate), scan_dir, common) &&
                path_is_directory(candidate)) {
                size_t used = strlen(completed);
                if (used + 1 < sizeof(completed) &&
                    (used == 0 || completed[used - 1] != '/')) {
                    completed[used] = '/';
                    completed[used + 1] = 0;
                }
            }
        }
        finish_path_completion(completed);
        return;
    }

    /* No longer common prefix exists. Keep a deterministic sorted list and
       put an actual candidate into the field; repeated Tab cycles the list. */
    qsort(g.completion_names, (size_t)stored,
          sizeof(g.completion_names[0]), compare_completion_names);
    safe_copy(g.completion_display_dir, sizeof(g.completion_display_dir), display_dir);
    g.completion_count = stored;
    g.completion_index = -1;

    for (int i = 0; i < stored; i++) {
        char candidate[PATH_MAX];
        g.completion_is_dir[i] = 0;
        if (path_join(candidate, sizeof(candidate), scan_dir,
                      g.completion_names[i]))
            g.completion_is_dir[i] = path_is_directory(candidate) ? 1 : 0;
    }

    int first = 0;
    /* Prefer a candidate that changes the text. This avoids selecting an
       exact file-name match while other longer candidates are available. */
    for (int i = 0; i < stored; i++) {
        if (strcmp(g.completion_names[i], prefix) != 0 || g.completion_is_dir[i]) {
            first = i;
            break;
        }
    }
    finish_path_candidate(first);
    if (matches > stored)
        show_toast("先頭 %d / %d 件をTabで選択できます", stored, matches);
}

static void navigate(const char *path, int add_history) {
    reset_path_completion();
    char resolved_input[PATH_MAX]; resolve_navigation_path(path, resolved_input, sizeof(resolved_input));
    if(load_directory(resolved_input) && add_history) history_push(g.cwd);
    update_toolbar_state(); hide_context();
}

static void open_external(const char *path) {
    if(program_exists("xdg-open")){
        char *argv[]={"xdg-open",(char*)path,NULL};
        if(spawn_detached(argv,NULL))return;
    }
    if(program_exists("gio")){
        char *argv[]={"gio","open",(char*)path,NULL};
        if(spawn_detached(argv,NULL))return;
    }
    show_toast("開くためのXDGランチャーが見つかりません");
}

static void open_entry(int ei) {
    if(ei<0||ei>=g.entry_count)return;
    if(g.entries[ei].is_dir) navigate(g.entries[ei].path,1);
    else if(g_file_dialog_config.mode!=LUNA_FILE_DIALOG_MANAGER){
        if(!g.entries[ei].selected){clear_selection();g.entries[ei].selected=1;refresh_file_visuals();}
        file_dialog_accept();
    } else open_external(g.entries[ei].path);
}



static void close_open_with_dialog(void) {
    if(!g.open_with_open)return;
    luna_pop_focus_trap(g.id_open_with);
    luna_add_class(g.id_open_with,"hidden");
    g.open_with_open=0;g.open_with_target=-1;g.open_app_count=0;
    request_overlay_layout();
}

static void show_open_with_dialog(int target) {
    if(target<0||target>=g.entry_count){show_toast("ファイルを選択してください");return;}
    if(g.entries[target].is_dir){open_entry(target);return;}
    char mime[128]="";collect_open_apps(g.entries[target].path,mime,sizeof(mime));
    if(g.open_app_count<=0){show_toast("このファイルに対応するアプリが見つかりません");open_external(g.entries[target].path);return;}
    g.open_with_target=target;g.open_with_open=1;
    char heading[768];snprintf(heading,sizeof(heading),"「%s」を開くアプリを選択してください。\n%s\n%d 個の候補",g.entries[target].name,mime[0]?mime:"MIMEタイプ不明",g.open_app_count);
    luna_set_text(g.id_open_with_info,heading);
    char default_id[256]="";if(mime[0])query_xdg_default_app(mime,default_id,sizeof(default_id));
    for(int i=0;i<MAX_OPEN_APPS;i++){
        char id[32];snprintf(id,sizeof(id),"openapp%02d",i);int eid=luna_get_element_by_id(id);
        if(i<g.open_app_count){
            char label[768];snprintf(label,sizeof(label),"%s%s\n%s",g.open_app_names[i],default_id[0]&&!strcmp(default_id,g.open_app_ids[i])?"　（既定）":"",g.open_app_ids[i]);
            luna_set_text(eid,label);luna_remove_class(eid,"hidden");
        }else luna_add_class(eid,"hidden");
    }
    luna_remove_class(g.id_open_with,"hidden");
    luna_push_focus_trap(g.id_open_with,NULL,0);
    request_overlay_layout();
}

static int parse_slot(const char *id) {
    if(!id) return -1;
    const char *p=id;
    while(*p && (*p<'0'||*p>'9'))p++;
    if(!*p)return -1;
    int s=atoi(p); return (s>=0&&s<MAX_ITEMS)?s:-1;
}

static void select_entry_click(int ei, int mods, int right_click) {
    if(ei<0||ei>=g.entry_count)return;
    if(right_click){if(!g.entries[ei].selected){clear_selection();g.entries[ei].selected=1;}g.anchor_entry=ei;refresh_file_visuals();return;}
    if(mods & GLFW_MOD_SHIFT && g.anchor_entry>=0){
        int a=g.anchor_entry<ei?g.anchor_entry:ei,b=g.anchor_entry>ei?g.anchor_entry:ei;
        if(!(mods&GLFW_MOD_CONTROL))clear_selection();
        for(int i=a;i<=b;i++)g.entries[i].selected=1;
    } else if(mods & GLFW_MOD_CONTROL){
        g.entries[ei].selected=!g.entries[ei].selected; g.anchor_entry=ei;
    } else { clear_selection();g.entries[ei].selected=1;g.anchor_entry=ei; }
    refresh_file_visuals();
}

static void clipboard_export(void) {
    if(g.clip_count<=0)return;
    size_t cap=(size_t)g.clip_count*(PATH_MAX*3+16)+32;
    char *mime=calloc(1,cap),*text=calloc(1,cap); if(!mime||!text){free(mime);free(text);return;}
    strcat(mime,g.clip_cut?"cut\n":"copy\n");
    for(int i=0;i<g.clip_count;i++){
        char escaped[PATH_MAX*3];url_escape_path(g.clip_paths[i],escaped,sizeof(escaped));
        strncat(mime,"file://",cap-strlen(mime)-1);strncat(mime,escaped,cap-strlen(mime)-1);strncat(mime,"\n",cap-strlen(mime)-1);
        strncat(text,g.clip_paths[i],cap-strlen(text)-1);strncat(text,"\n",cap-strlen(text)-1);
    }
    glfwSetClipboardString(g.window,text);
    if(getenv("WAYLAND_DISPLAY")&&program_exists("wl-copy")){
        char *argv[]={"wl-copy","--type","x-special/gnome-copied-files",NULL};
        pipe_write_program("wl-copy",argv,mime);
    } else if(program_exists("xclip")){
        char *argv[]={"xclip","-selection","clipboard","-t","x-special/gnome-copied-files","-i",NULL};
        pipe_write_program("xclip",argv,mime);
    }
    free(mime);free(text);
}

static void clipboard_take_selection(int cut) {
    g.clip_count=0;g.clip_cut=cut;
    for(int i=0;i<g.entry_count&&g.clip_count<MAX_CLIP_ITEMS;i++)if(g.entries[i].selected)safe_copy(g.clip_paths[g.clip_count++],PATH_MAX,g.entries[i].path);
    if(!g.clip_count){show_toast("項目を選択してください");return;}
    clipboard_export(); refresh_file_visuals();
    show_toast("%d 個を%sしました",g.clip_count,cut?"切り取り":"コピー");
}

static int hexval(int c){if(c>='0'&&c<='9')return c-'0';if(c>='A'&&c<='F')return c-'A'+10;if(c>='a'&&c<='f')return c-'a'+10;return -1;}
static void url_decode(char *s){char *r=s,*w=s;while(*r){if(*r=='%'&&hexval(r[1])>=0&&hexval(r[2])>=0){*w++=(char)((hexval(r[1])<<4)|hexval(r[2]));r+=3;}else *w++=*r++;}*w=0;}

static int clipboard_import(void) {
    char buf[PATH_MAX*MAX_CLIP_ITEMS/2]; buf[0]=0;
    int got=0;
    if(getenv("WAYLAND_DISPLAY")&&program_exists("wl-paste")){
        char *argv[]={"wl-paste","-n","--type","x-special/gnome-copied-files",NULL};got=pipe_read_program("wl-paste",argv,buf,sizeof(buf));
    } else if(program_exists("xclip")){
        char *argv[]={"xclip","-selection","clipboard","-t","x-special/gnome-copied-files","-o",NULL};got=pipe_read_program("xclip",argv,buf,sizeof(buf));
    }
    if(!got){const char *s=glfwGetClipboardString(g.window);if(s)safe_copy(buf,sizeof(buf),s);}
    if(!buf[0])return 0;
    g.clip_count=0;g.clip_cut=0;
    char *save=NULL,*line=strtok_r(buf,"\r\n",&save);
    if(line && (!strcmp(line,"copy")||!strcmp(line,"cut"))){g.clip_cut=!strcmp(line,"cut");line=strtok_r(NULL,"\r\n",&save);}
    for(;line&&g.clip_count<MAX_CLIP_ITEMS;line=strtok_r(NULL,"\r\n",&save)){
        while(*line==' '||*line=='\t')line++;
        if(!strncmp(line,"file://",7))line+=7;
        char path[PATH_MAX];safe_copy(path,sizeof(path),line);url_decode(path);
        if(path[0]=='/'&&access(path,F_OK)==0)safe_copy(g.clip_paths[g.clip_count++],PATH_MAX,path);
    }
    update_toolbar_state(); return g.clip_count>0;
}

static int path_is_inside(const char *child, const char *parent) {
    size_t n = strlen(parent);
    if (strncmp(child, parent, n) != 0) return 0;
    return child[n] == '/' || (n == 1 && parent[0] == '/');
}

static void paste_clipboard(void) {
    if(g.clip_count<=0&&!clipboard_import()){show_toast("クリップボードにファイルがありません");return;}
    int ok=0,fail=0;
    for(int i=0;i<g.clip_count;i++){
        const char *src=g.clip_paths[i];
        char src_parent[PATH_MAX]; parent_path(src, src_parent, sizeof(src_parent));
        if (g.clip_cut && !strcmp(src_parent, g.cwd)) { ok++; continue; }
        if (path_is_inside(g.cwd, src)) { fail++; continue; }
        char dst[PATH_MAX];
        if(!unique_destination(g.cwd,base_name(src),dst,sizeof(dst))){fail++;continue;}
        int done=0;
        if(g.clip_cut){
            if(rename(src,dst)==0)done=1;
            else if(copy_tree(src,dst)&&remove_tree(src))done=1;
        }else done=copy_tree(src,dst);
        if(done)ok++;else fail++;
    }
    if(g.clip_cut&&fail==0){g.clip_count=0;g.clip_cut=0;}
    load_directory(g.cwd); show_toast("貼り付け: %d 件成功%s",ok,fail?"（一部失敗）":"");
}

static void delete_selected(void) {
    int n=selected_count();if(!n){show_toast("項目を選択してください");return;}
    int ok=0,fail=0;
    for(int i=0;i<g.entry_count;i++)if(g.entries[i].selected){if(move_to_trash(g.entries[i].path))ok++;else fail++;}
    refresh_sidebar_visibility();
    load_directory(g.cwd);show_toast("%d 件をごみ箱へ移動%s",ok,fail?"（一部失敗）":"");
}

static void open_modal(int kind, int target) {
    char mime[128]="",current[256]="";
    if(kind==4){
        if(target<0||target>=g.entry_count||g.entries[target].is_dir){show_toast("ファイルを選択してください");return;}
        if(!query_xdg_mime_type(g.entries[target].path,mime,sizeof(mime))){show_toast("MIMEタイプを取得できません");return;}
        query_xdg_default_app(mime,current,sizeof(current));
    }
    g.modal_kind=kind;g.modal_target=target;
    /* Keep the overlay paint-hidden until its final contents and geometry are
       ready.  Do not toggle controls through an intermediate visible state: it
       creates needless display transitions and makes the first Properties
       frame depend on a later settling pass. */
    luna_set_text(g.id_modal_cancel,"キャンセル");
    luna_set_text(g.id_modal_ok,"OK");
    if(kind==1){luna_set_text(g.id_modal_title,"新しいフォルダー");luna_set_value(g.id_modal_input,"名称未設定フォルダー");set_modal_info_text("現在の場所にフォルダーを作成します。");}
    else if(kind==2&&target>=0){luna_set_text(g.id_modal_title,"名前を変更");luna_set_value(g.id_modal_input,g.entries[target].name);set_modal_info_text("新しい名前を入力してください。");}
    else if(kind==3&&target>=0){
        char info[12288],title[512];
        build_properties_info(&g.entries[target],info,sizeof(info));
        snprintf(title,sizeof(title),"プロパティ — %s",g.entries[target].name);
        luna_set_text(g.id_modal_title,title);
        set_modal_info_text(info);
        luna_set_text(g.id_modal_ok,"閉じる");
    } else if(kind==7){
        char info[3072]="";int n=selected_count();append_text(info,sizeof(info),"選択した %d 件を完全に削除します。\nこの操作は元に戻せません。\n\n",n);
        int shown=0;for(int i=0;i<g.entry_count&&shown<12;i++)if(g.entries[i].selected){append_text(info,sizeof(info),"• %s\n",g.entries[i].name);shown++;}
        if(n>shown)append_text(info,sizeof(info),"ほか %d 件\n",n-shown);
        luna_set_text(g.id_modal_title,"完全に削除しますか？");set_modal_info_text(info);
    } else if(kind==4){
        char info[768];safe_copy(g.modal_mime,sizeof(g.modal_mime),mime);
        snprintf(info,sizeof(info),"MIMEタイプ: %s\n現在の既定: %s\n\nXDGの .desktop ID を入力してください。例: org.gnome.TextEditor.desktop\nこの種類のファイルに対するデスクトップ全体の既定アプリが変更されます。",mime,*current?current:"未設定");
        luna_set_text(g.id_modal_title,"既定のアプリを設定");set_modal_info_text(info);luna_set_value(g.id_modal_input,current);
    } else if(kind==5){
        luna_set_text(g.id_modal_title,"F4 ターミナル");
        set_modal_info_text("F4で現在のフォルダーを開くターミナルコマンドを指定します。空欄では $TERMINAL、xdg-terminal-exec、主要な端末の順に自動検出します。%d は現在のフォルダーに置換されます。");
        luna_set_value(g.id_modal_input,g.terminal_command);
    } else if(kind==6){
        luna_set_text(g.id_modal_title,"カスタムフォント");
        set_modal_info_text("フォントファミリー名、または .ttf / .otf / .ttc ファイルのパスを入力してください。fontconfigで解決できる名前も使用できます。変更は次回起動から反映されます。");
        luna_set_value(g.id_modal_input,g.custom_font);
    }
    luna_update_classes(g.id_modal_input, "hidden", (kind==3||kind==7)?"hidden":"");
    luna_update_classes(g.id_modal_cancel, "hidden", kind==3?"hidden":"");

    if(kind==3&&!g.properties_layout_warmed){
        /* Properties is the only modal that changes both its geometry and its
           scroll root substantially.  Lay it out once while visibility:hidden
           so the first visible frame already has final clips/scroll metrics.
           This is CPU-only, happens once per process, and avoids a second
           rendered frame or any steady-state repaint cost. */
        luna_update_classes(g.id_modal, "properties-dialog", "properties-dialog");
        luna_update(luna_platform_time(),0.0);
        g.properties_layout_warmed=1;
        luna_remove_class(g.id_modal,"overlay-idle");
    }else{
        /* Normal opens keep the fast single-restyle path. */
        luna_update_classes(g.id_modal, "overlay-idle properties-dialog",
                            kind==3?"properties-dialog":"");
    }
    luna_push_focus_trap(g.id_modal,NULL,0);
    if(kind!=3&&kind!=7)luna_focus_element(g.id_modal_input);
    request_overlay_layout();
}

static void close_modal(void) {
    if(!g.modal_kind)return;
    int reopen=g.modal_return_settings;
    g.modal_return_settings=0;
    luna_pop_focus_trap(g.id_modal);
    luna_update_classes(g.id_modal,"properties-dialog","overlay-idle");
    g.modal_kind=0;g.modal_target=-1;
    request_redraw();
    if(reopen)open_settings_dialog();
}

static void confirm_modal(void) {
    if(!g.modal_kind){return;}
    if(g.modal_kind==3){close_modal();return;}
    if(g.modal_kind==7){
        int ok=0,fail=0;for(int i=0;i<g.entry_count;i++)if(g.entries[i].selected){if(remove_tree(g.entries[i].path))ok++;else fail++;}
        close_modal();refresh_sidebar_visibility();load_directory(g.cwd);show_toast("完全削除: %d 件成功%s",ok,fail?"（一部失敗）":"");return;
    }
    const char *value=luna_get_value(g.id_modal_input);
    if(g.modal_kind==4){
        char desktop_id[256];safe_copy(desktop_id,sizeof(desktop_id),value);trim_text(desktop_id);
        if(!valid_desktop_id(desktop_id)){show_toast(".desktop IDを入力してください");return;}
        if(!set_xdg_default_app(desktop_id,g.modal_mime)){show_toast("既定のアプリを設定できませんでした");return;}
        close_modal();show_toast("%s の既定アプリを設定しました",g.modal_mime);return;
    }
    if(g.modal_kind==5){
        char command[sizeof(g.terminal_command)];safe_copy(command,sizeof(command),value);trim_text(command);
        safe_copy(g.terminal_command,sizeof(g.terminal_command),command);save_settings();
        close_modal();show_toast(*command?"F4のターミナルを設定しました":"F4のターミナルを自動検出に戻しました");return;
    }
    if(g.modal_kind==6){
        char spec[PATH_MAX],resolved[PATH_MAX];safe_copy(spec,sizeof(spec),value);trim_text(spec);
        if(!spec[0]){g.custom_font[0]=0;g.font_family=0;save_settings();close_modal();show_toast("フォントを自動選択に戻しました");return;}
        if(!resolve_font_spec(spec,resolved,sizeof(resolved))){show_toast("フォントを見つけられません。名前またはファイルパスを確認してください");return;}
        safe_copy(g.custom_font,sizeof(g.custom_font),spec);g.font_family=4;save_settings();
        close_modal();show_toast("カスタムフォントを保存しました。次回起動から反映されます");return;
    }
    const char *name=value;
    if(!name||!*name||strchr(name,'/')){show_toast("使用できない名前です");return;}
    char dst[PATH_MAX];if(!path_join(dst,sizeof(dst),g.cwd,name)){show_toast("名前が長すぎます");return;}
    int ok=0;
    if(g.modal_kind==1)ok=mkdir(dst,0755)==0;
    else if(g.modal_kind==2&&g.modal_target>=0)ok=rename(g.entries[g.modal_target].path,dst)==0;
    if(!ok){show_toast("操作できません: %s",strerror(errno));return;}
    close_modal();load_directory(g.cwd);show_toast("完了しました");
}

static int first_selected(void){for(int i=0;i<g.entry_count;i++)if(g.entries[i].selected)return i;return -1;}

static void on_blank(LunaElement *e) {
    (void)e;
    /* Item clicks bubble to the file-grid handler in Luna UI.  Ignore that
       matching parent callback so it cannot clear selection or disturb scroll. */
    double now = glfwGetTime();
    if (now <= g.suppress_blank_until) {
        g.suppress_blank_until = 0.0;
        return;
    }
    g.suppress_blank_until = 0.0;
    clear_selection();
    refresh_file_visuals();
    if (luna_last_click_button() == LUNA_MOUSE_BUTTON_RIGHT) {
        hide_volume_context();
        g.context_entry = -1;
        luna_remove_class(g.id_context, "hidden");
        request_overlay_layout();
    } else hide_context();
}

static void on_item(LunaElement *e) {
    g.suppress_blank_until = glfwGetTime() + 0.15;
    int slot=parse_slot(e->id);if(slot<0)return;int ei=g.slot_entry[slot];if(ei<0)return;
    int button=luna_last_click_button(),mods=luna_last_click_mods();
    if(button==LUNA_MOUSE_BUTTON_RIGHT){
        hide_volume_context();
        select_entry_click(ei,mods,1);
        g.context_entry=ei;
        luna_remove_class(g.id_context,"hidden");
        request_overlay_layout();
        return;
    }
    hide_context();select_entry_click(ei,mods,0);
    double now=glfwGetTime();
    if(ei==g.last_click_entry&&now-g.last_click_time<0.38&&!(mods&(GLFW_MOD_CONTROL|GLFW_MOD_SHIFT)))open_entry(ei);
    g.last_click_entry=ei;g.last_click_time=now;
}

static void on_back(LunaElement *e){(void)e;if(g.history_pos>0){g.history_pos--;load_directory(g.history[g.history_pos]);update_toolbar_state();}}
static void on_forward(LunaElement *e){(void)e;if(g.history_pos+1<g.history_count){g.history_pos++;load_directory(g.history[g.history_pos]);update_toolbar_state();}}
static void on_up(LunaElement *e){(void)e;char p[PATH_MAX];parent_path(g.cwd,p,sizeof(p));navigate(p,1);}
static void on_reload(LunaElement *e){(void)e;refresh_volumes(1);load_directory(g.cwd);show_toast("更新しました");}
static void on_home(LunaElement *e){(void)e;navigate(home_dir(),1);}
static void on_grid(LunaElement *e){(void)e;g.grid_view=1;render_files();}
static void on_list(LunaElement *e){(void)e;g.grid_view=0;render_files();}
static void on_hidden(LunaElement *e){(void)e;g.show_hidden=!g.show_hidden;render_files();}
static void on_sort(LunaElement *e){(void)e;if(g.sort_mode==SORT_NAME)g.sort_mode=SORT_DATE;else if(g.sort_mode==SORT_DATE)g.sort_mode=SORT_SIZE;else{g.sort_mode=SORT_NAME;g.sort_desc=!g.sort_desc;}qsort(g.entries,g.entry_count,sizeof(g.entries[0]),compare_entries);clear_selection();render_files();}
static void on_new_folder(LunaElement *e){(void)e;hide_context();open_modal(1,-1);}
static void on_copy(LunaElement *e){(void)e;hide_context();clipboard_take_selection(0);}
static void on_cut(LunaElement *e){(void)e;hide_context();clipboard_take_selection(1);}
static void on_paste(LunaElement *e){(void)e;hide_context();paste_clipboard();}
static void on_rename(LunaElement *e){(void)e;hide_context();int i=first_selected();if(i>=0)open_modal(2,i);else show_toast("項目を選択してください");}
static void on_delete(LunaElement *e){(void)e;hide_context();delete_selected();}
static void on_permanent_delete(LunaElement *e){(void)e;hide_context();int i=first_selected();if(i>=0)open_modal(7,i);else show_toast("項目を選択してください");}
static void on_properties(LunaElement *e){(void)e;hide_context();int i=first_selected();if(i>=0)open_modal(3,i);else show_toast("項目を選択してください");}
static void on_open(LunaElement *e){(void)e;hide_context();int i=first_selected();if(i>=0)open_entry(i);}
static void on_open_with(LunaElement *e){(void)e;hide_context();int i=first_selected();if(i>=0)show_open_with_dialog(i);else show_toast("ファイルを選択してください");}
static void on_open_app(LunaElement *e){
    int slot=parse_slot(e?e->id:NULL);if(slot<0||slot>=g.open_app_count||g.open_with_target<0||g.open_with_target>=g.entry_count)return;
    char path[PATH_MAX],name[256];safe_copy(path,sizeof(path),g.entries[g.open_with_target].path);safe_copy(name,sizeof(name),g.open_app_names[slot]);
    int ok=launch_desktop_application(slot,path);close_open_with_dialog();if(!ok)show_toast("%s で開けませんでした",name);
}
static void on_open_with_default(LunaElement *e){(void)e;if(g.open_with_target<0||g.open_with_target>=g.entry_count){close_open_with_dialog();return;}char path[PATH_MAX];safe_copy(path,sizeof(path),g.entries[g.open_with_target].path);close_open_with_dialog();open_external(path);}
static void on_open_with_cancel(LunaElement *e){(void)e;close_open_with_dialog();}
static void on_set_default_app(LunaElement *e){(void)e;hide_context();int i=first_selected();if(i>=0)open_modal(4,i);else show_toast("ファイルを選択してください");}
static void on_terminal(LunaElement *e){(void)e;hide_context();open_terminal_here();}
static void on_create_launcher(LunaElement *e){(void)e;show_desktop_edit_dialog(1,-1);}
static void on_edit_desktop(LunaElement *e){
    (void)e;hide_context();
    int i=first_selected();
    if(i<0){show_toast(".desktop ファイルを選択してください");return;}
    show_desktop_edit_dialog(0,i);
}
static void on_desktop_edit_cancel(LunaElement *e){(void)e;close_desktop_edit_dialog();}
static void on_desktop_edit_save(LunaElement *e){(void)e;save_desktop_edit_dialog();}
static void on_desktop_edit_type(LunaElement *e){
    (void)e;
    if(!strcmp(g.desktop_edit_type,"Application")) safe_copy(g.desktop_edit_type,sizeof(g.desktop_edit_type),"Link");
    else if(!strcmp(g.desktop_edit_type,"Link")) safe_copy(g.desktop_edit_type,sizeof(g.desktop_edit_type),"Directory");
    else safe_copy(g.desktop_edit_type,sizeof(g.desktop_edit_type),"Application");
    sync_desktop_edit_type_controls();
    request_redraw();
}
static void on_desktop_edit_terminal(LunaElement *e){
    (void)e;g.desktop_edit_terminal=!g.desktop_edit_terminal;sync_desktop_edit_type_controls();request_redraw();
}
static void on_modal_cancel(LunaElement *e){(void)e;close_modal();}
static void on_modal_ok(LunaElement *e){(void)e;confirm_modal();}
static void file_dialog_finish(int accepted) {
    if (g_file_dialog_result) g_file_dialog_result->accepted = accepted;
    g_file_dialog_cancelled = accepted ? 0 : 1;
    if (g.window) glfwSetWindowShouldClose(g.window, 1);
}
static void file_dialog_overwrite_result(LunaWindowDialogResult result,const char*input,void*userdata){
    (void)input;(void)userdata;
    if(result!=LUNA_WINDOW_RESULT_OK){g_file_dialog_pending_save[0]=0;return;}
    if(!g_file_dialog_result||!g_file_dialog_pending_save[0])return;
    safe_copy(g_file_dialog_result->paths[0],sizeof(g_file_dialog_result->paths[0]),g_file_dialog_pending_save);
    g_file_dialog_result->count=1;
    g_file_dialog_pending_save[0]=0;
    file_dialog_finish(1);
}
static void file_dialog_accept(void) {
    if (g_file_dialog_config.mode == LUNA_FILE_DIALOG_MANAGER) return;
    if (!g_file_dialog_result) { file_dialog_finish(0); return; }
    g_file_dialog_result->count = 0;
    if (g_file_dialog_config.mode == LUNA_FILE_DIALOG_SELECT_FOLDER) {
        int first = first_selected();
        const char* path = g.cwd;
        if (first >= 0 && g.entries[first].is_dir) path = g.entries[first].path;
        safe_copy(g_file_dialog_result->paths[0], sizeof(g_file_dialog_result->paths[0]), path);
        g_file_dialog_result->count = 1;
    } else if (g_file_dialog_config.mode == LUNA_FILE_DIALOG_SAVE_FILE) {
        int name_id = luna_get_element_by_id("file-dialog-name");
        const char* name = name_id >= 0 ? luna_get_value(name_id) : NULL;
        char path[PATH_MAX];
        if (!name || !name[0]) { show_toast("ファイル名を入力してください"); return; }
        if (!strcmp(name,".")||!strcmp(name,"..")||strchr(name,'/')) { show_toast("ファイル名だけを入力してください"); return; }
        if (!path_join(path,sizeof(path),g.cwd,name)) { show_toast("パスが長すぎます"); return; }
        struct stat existing;
        if(stat(path,&existing)==0){
            if(S_ISDIR(existing.st_mode)){show_toast("同じ名前のフォルダーがあります");return;}
            safe_copy(g_file_dialog_pending_save,sizeof(g_file_dialog_pending_save),path);
            luna_window_confirm(LUNA_WINDOW_DIALOG_WARNING,"上書きしますか？","同じ名前のファイルが既にあります。上書きすると元に戻せません。",file_dialog_overwrite_result,NULL);
            int overwrite_ok=luna_get_element_by_id("luna-common-dialog-ok");
            int overwrite_cancel=luna_get_element_by_id("luna-common-dialog-cancel");
            if(overwrite_ok>=0)luna_set_text(overwrite_ok,"上書き");
            if(overwrite_cancel>=0)luna_set_text(overwrite_cancel,"キャンセル");
            return;
        }
        safe_copy(g_file_dialog_result->paths[0],sizeof(g_file_dialog_result->paths[0]),path);
        g_file_dialog_result->count=1;
    } else {
        int max_results = g_file_dialog_config.mode == LUNA_FILE_DIALOG_OPEN_FILES ? LUNA_FILE_DIALOG_MAX_RESULTS : 1;
        for (int i=0;i<g.entry_count && g_file_dialog_result->count<max_results;i++) {
            if (!g.entries[i].selected || g.entries[i].is_dir) continue;
            safe_copy(g_file_dialog_result->paths[g_file_dialog_result->count],
                      sizeof(g_file_dialog_result->paths[0]),g.entries[i].path);
            g_file_dialog_result->count++;
        }
        if (g_file_dialog_result->count==0) { show_toast("ファイルを選択してください"); return; }
    }
    file_dialog_finish(1);
}
static void on_file_dialog_accept(LunaElement*e){(void)e;file_dialog_accept();}
static void on_file_dialog_cancel(LunaElement*e){(void)e;file_dialog_finish(0);}
static void on_close(LunaElement *e){(void)e;if(g_file_dialog_config.mode!=LUNA_FILE_DIALOG_MANAGER)file_dialog_finish(0);else glfwSetWindowShouldClose(g.window,1);}

static void side_navigate_index(int index) {
    if(index<0||index>=9||!path_is_directory(g.side_paths[index])){
        show_toast("このフォルダーは存在しません");
        refresh_sidebar_visibility();
        return;
    }
    navigate(g.side_paths[index],1);
}
static void on_side_home(LunaElement *e){(void)e;side_navigate_index(0);}
static void on_side_desktop(LunaElement *e){(void)e;side_navigate_index(1);}
static void on_side_documents(LunaElement *e){(void)e;side_navigate_index(2);}
static void on_side_downloads(LunaElement *e){(void)e;side_navigate_index(3);}
static void on_side_pictures(LunaElement *e){(void)e;side_navigate_index(4);}
static void on_side_music(LunaElement *e){(void)e;side_navigate_index(5);}
static void on_side_videos(LunaElement *e){(void)e;side_navigate_index(6);}
static void on_side_trash(LunaElement *e){(void)e;side_navigate_index(7);}
static void on_side_root(LunaElement *e){(void)e;side_navigate_index(8);}
static int parse_vol_slot(const char *id) {
    int s;
    if(!id||strncmp(id,"vol",3)!=0||strlen(id)!=5)return -1;
    if(id[3]<'0'||id[3]>'9'||id[4]<'0'||id[4]>'9')return -1;
    s=(id[3]-'0')*10+(id[4]-'0');
    return (s>=0&&s<MAX_VOLUMES)?s:-1;
}
static void on_volume(LunaElement *e){
    int slot=parse_vol_slot(e?e->id:NULL);
    int button=luna_last_click_button();
    if(slot<0||slot>=g.volume_count)return;
    if(button==LUNA_MOUSE_BUTTON_RIGHT){show_volume_context(slot);return;}
    hide_volume_context();hide_context();
    activate_volume_index(slot);
}
static void on_volume_open(LunaElement *e){(void)e;int i=g.volume_context;hide_volume_context();if(i>=0)activate_volume_index(i);}
static void on_volume_unmount(LunaElement *e){
    (void)e;int i=g.volume_context;VolumeEntry v;
    if(i<0||i>=g.volume_count){hide_volume_context();return;}
    v=g.volumes[i];hide_volume_context();
    if(!v.is_mounted){show_toast("マウントされていません");return;}
    if(unmount_volume_index(i,0))show_toast("%s をアンマウントしました",v.name);
    else show_toast("アンマウントに失敗しました");
}
static void on_volume_eject(LunaElement *e){
    (void)e;int i=g.volume_context;VolumeEntry v;
    if(i<0||i>=g.volume_count){hide_volume_context();return;}
    v=g.volumes[i];hide_volume_context();
    if(unmount_volume_index(i,1))show_toast("%s を取り外しました",v.name);
    else show_toast("取り外しに失敗しました");
}
static void on_volume_context_cancel(LunaElement *e){(void)e;hide_volume_context();}

static void apply_display_classes(void) {
    if(g.id_app<0)return;
    const char *font=g.font_size==0?"font-small":g.font_size==1?"font-standard":"font-large";
    const char *density=g.display_density==0?"density-compact":
                        g.display_density==1?"density-standard":
                        g.display_density==2?"density-roomy":"density-minimal";
    const char *theme=g.theme?"theme-dark":"theme-light";
    const char *accent=g.accent==0?"accent-blue":g.accent==1?"accent-purple":g.accent==2?"accent-green":"accent-orange";
    char classes[320];
    snprintf(classes,sizeof(classes),"%s %s %s %s%s%s%s%s%s%s",font,density,theme,accent,
             g.show_status?"":" hide-status",
             g.show_grid_details?" grid-details":"",
             g.animations?"":" no-motion",
             g.window_width<760?" window-narrow":"",
             g.window_width<560?" window-tiny":"",
             g.window_height<500?" window-short":"");
    luna_update_classes(g.id_app,
        "font-small font-standard font-large density-compact density-standard density-roomy density-minimal theme-light theme-dark accent-blue accent-purple accent-green accent-orange hide-status grid-details no-motion window-narrow window-tiny window-short",
        classes);
    luna_mark_layout_dirty();
    request_redraw();
}

static void set_setting_state(int id, const char *text, int on) {
    luna_set_text(id,text);
    if(on)luna_add_class(id,"on"); else luna_remove_class(id,"on");
}

static const char *font_family_label(void) {
    return g.font_family==0?"自動":g.font_family==1?"ゴシック":g.font_family==2?"明朝":g.font_family==3?"等幅":"カスタム";
}

static void sync_settings_dialog(void) {
    set_setting_state(g.id_set_theme,g.theme?"ダーク":"ライト",g.theme);
    set_setting_state(g.id_set_accent,g.accent==0?"ブルー":g.accent==1?"パープル":g.accent==2?"グリーン":"オレンジ",1);
    set_setting_state(g.id_set_font_family,font_family_label(),g.font_family!=0);
    set_setting_state(g.id_set_custom_font,g.custom_font[0]?"指定済み":"設定…",g.custom_font[0]!=0);
    set_setting_state(g.id_set_font_size,g.font_size==0?"小":g.font_size==1?"標準":"大",1);
    set_setting_state(g.id_set_density,g.display_density==0?"コンパクト":g.display_density==1?"標準":g.display_density==2?"ゆったり":"最小",g.display_density!=0);
    set_setting_state(g.id_set_animations,g.animations?"有効":"無効",g.animations);
    set_setting_state(g.id_set_sidebar,g.show_sidebar?"表示":"非表示",g.show_sidebar);
    set_setting_state(g.id_set_search,g.show_search?"表示":"非表示",g.show_search);
    set_setting_state(g.id_set_status,g.show_status?"表示":"非表示",g.show_status);
    set_setting_state(g.id_set_grid_details,g.show_grid_details?"詳細あり":"名前のみ",g.show_grid_details);
    set_setting_state(g.id_set_hidden,g.show_hidden?"表示":"非表示",g.show_hidden);
    set_setting_state(g.id_set_view,g.grid_view?"アイコン":"リスト",g.grid_view);
    set_setting_state(g.id_set_sort,g.sort_mode==SORT_NAME?"名前":g.sort_mode==SORT_DATE?"更新日":"サイズ",1);
    set_setting_state(g.id_set_desc,g.sort_desc?"降順":"昇順",g.sort_desc);
    set_setting_state(g.id_set_startup,g.startup_mode==0?"ホーム":g.startup_mode==1?"前回の場所":"起動場所",g.startup_mode!=0);
    set_setting_state(g.id_set_terminal,g.terminal_command[0]?"指定済み":"自動",g.terminal_command[0]!=0);
    request_redraw();
}

static void apply_basic_preferences(void) {
    refresh_sidebar_visibility();
    qsort(g.entries,(size_t)g.entry_count,sizeof(g.entries[0]),compare_entries);
    render_files();
}

static void open_settings_dialog(void) {
    if(g.settings_open)return;
    hide_context();
    g.settings_open=1;
    sync_settings_dialog();
    luna_remove_class(g.id_settings,"overlay-idle");
    luna_push_focus_trap(g.id_settings,NULL,0);
    request_overlay_layout();
}

static void close_settings_dialog(void) {
    if(!g.settings_open)return;
    luna_pop_focus_trap(g.id_settings);
    luna_add_class(g.id_settings,"overlay-idle");
    g.settings_open=0;
    save_settings();
    request_redraw();
}

static void open_modal_from_settings(int kind) {
    g.modal_return_settings=1;
    close_settings_dialog();
    open_modal(kind,-1);
}

static void on_settings_open(LunaElement *e){(void)e;open_settings_dialog();}
static void on_settings_close(LunaElement *e){(void)e;close_settings_dialog();}
static void on_setting_theme(LunaElement *e){(void)e;g.theme=!g.theme;apply_display_classes();sync_settings_dialog();}
static void on_setting_accent(LunaElement *e){(void)e;g.accent=(g.accent+1)%4;apply_display_classes();sync_settings_dialog();}
static void on_setting_sidebar(LunaElement *e){(void)e;g.show_sidebar=!g.show_sidebar;apply_basic_preferences();sync_settings_dialog();}
static void on_setting_search(LunaElement *e){(void)e;g.show_search=!g.show_search;apply_basic_preferences();sync_settings_dialog();}
static void on_setting_status(LunaElement *e){(void)e;g.show_status=!g.show_status;apply_display_classes();sync_settings_dialog();}
static void on_setting_grid_details(LunaElement *e){(void)e;g.show_grid_details=!g.show_grid_details;apply_display_classes();render_files();sync_settings_dialog();}
static void on_setting_animations(LunaElement *e){(void)e;g.animations=!g.animations;apply_display_classes();sync_settings_dialog();}
static void on_setting_hidden(LunaElement *e){(void)e;g.show_hidden=!g.show_hidden;apply_basic_preferences();sync_settings_dialog();}
static void on_setting_view(LunaElement *e){(void)e;g.grid_view=!g.grid_view;apply_basic_preferences();sync_settings_dialog();}
static void on_setting_sort(LunaElement *e){(void)e;g.sort_mode=(SortMode)(((int)g.sort_mode+1)%3);apply_basic_preferences();sync_settings_dialog();}
static void on_setting_desc(LunaElement *e){(void)e;g.sort_desc=!g.sort_desc;apply_basic_preferences();sync_settings_dialog();}
static void on_setting_font_size(LunaElement *e){(void)e;g.font_size=(g.font_size+1)%3;apply_display_classes();sync_settings_dialog();}
static void on_setting_font_family(LunaElement *e){
    (void)e;int next=(g.font_family+1)%5;
    if(next==4&&!g.custom_font[0]){open_modal_from_settings(6);return;}
    g.font_family=next;save_settings();sync_settings_dialog();
    show_toast("フォントの種類は次回起動から反映されます");
}
static void on_setting_custom_font(LunaElement *e){(void)e;open_modal_from_settings(6);}
static void on_setting_terminal(LunaElement *e){(void)e;open_modal_from_settings(5);}
static void on_setting_startup(LunaElement *e){(void)e;g.startup_mode=(g.startup_mode+1)%3;sync_settings_dialog();}
static void on_setting_density(LunaElement *e){
    (void)e;
    if(g.display_density==0)g.display_density=3;
    else if(g.display_density==3)g.display_density=1;
    else if(g.display_density==1)g.display_density=2;
    else g.display_density=0;
    apply_display_classes();
    sync_settings_dialog();
}
static void on_settings_reset(LunaElement *e){
    (void)e;int ww=g.window_width,wh=g.window_height;char current[PATH_MAX];safe_copy(current,sizeof(current),g.cwd);
    settings_defaults();g.window_width=ww;g.window_height=wh;safe_copy(g.last_directory,sizeof(g.last_directory),current);
    apply_display_classes();apply_basic_preferences();sync_settings_dialog();save_settings();
    show_toast("設定を初期値に戻しました。フォントは次回起動から反映されます");
}

static void register_handlers(void) {
    luna_register_js_handler("onItem",on_item); luna_register_js_handler("onBlank",on_blank);
    luna_register_js_handler("onBack",on_back); luna_register_js_handler("onForward",on_forward);
    luna_register_js_handler("onUp",on_up); luna_register_js_handler("onReload",on_reload);
    luna_register_js_handler("onHome",on_home); luna_register_js_handler("onGrid",on_grid);
    luna_register_js_handler("onList",on_list); luna_register_js_handler("onHidden",on_hidden);
    luna_register_js_handler("onSort",on_sort); luna_register_js_handler("onNewFolder",on_new_folder);
    luna_register_js_handler("onCopy",on_copy); luna_register_js_handler("onCut",on_cut);
    luna_register_js_handler("onPaste",on_paste); luna_register_js_handler("onRename",on_rename);
    luna_register_js_handler("onDelete",on_delete); luna_register_js_handler("onPermanentDelete",on_permanent_delete); luna_register_js_handler("onProperties",on_properties);
    luna_register_js_handler("onOpen",on_open); luna_register_js_handler("onOpenWith",on_open_with); luna_register_js_handler("onOpenApp",on_open_app);
    luna_register_js_handler("onOpenWithDefault",on_open_with_default); luna_register_js_handler("onOpenWithCancel",on_open_with_cancel);
    luna_register_js_handler("onSetDefaultApp",on_set_default_app); luna_register_js_handler("onTerminal",on_terminal);
    luna_register_js_handler("onCreateLauncher",on_create_launcher); luna_register_js_handler("onEditDesktop",on_edit_desktop);
    luna_register_js_handler("onDesktopEditCancel",on_desktop_edit_cancel); luna_register_js_handler("onDesktopEditSave",on_desktop_edit_save);
    luna_register_js_handler("onDesktopEditType",on_desktop_edit_type); luna_register_js_handler("onDesktopEditTerminal",on_desktop_edit_terminal);
    luna_register_js_handler("onModalCancel",on_modal_cancel);
    luna_register_js_handler("onModalOk",on_modal_ok); luna_register_js_handler("onClose",on_close);
    luna_register_js_handler("onSideHome",on_side_home); luna_register_js_handler("onSideDesktop",on_side_desktop);
    luna_register_js_handler("onSideDocuments",on_side_documents); luna_register_js_handler("onSideDownloads",on_side_downloads);
    luna_register_js_handler("onSidePictures",on_side_pictures); luna_register_js_handler("onSideMusic",on_side_music);
    luna_register_js_handler("onSideVideos",on_side_videos); luna_register_js_handler("onSideTrash",on_side_trash);
    luna_register_js_handler("onSideRoot",on_side_root);
    luna_register_js_handler("onVolume",on_volume);
    luna_register_js_handler("onVolumeOpen",on_volume_open);
    luna_register_js_handler("onVolumeUnmount",on_volume_unmount);
    luna_register_js_handler("onVolumeEject",on_volume_eject);
    luna_register_js_handler("onVolumeContextCancel",on_volume_context_cancel);
    luna_register_js_handler("onSettingsOpen",on_settings_open); luna_register_js_handler("onSettingsClose",on_settings_close); luna_register_js_handler("onSettingsReset",on_settings_reset);
    luna_register_js_handler("onSettingTheme",on_setting_theme); luna_register_js_handler("onSettingAccent",on_setting_accent);
    luna_register_js_handler("onSettingFontFamily",on_setting_font_family); luna_register_js_handler("onSettingCustomFont",on_setting_custom_font);
    luna_register_js_handler("onSettingFontSize",on_setting_font_size); luna_register_js_handler("onSettingDensity",on_setting_density); luna_register_js_handler("onSettingAnimations",on_setting_animations);
    luna_register_js_handler("onSettingSidebar",on_setting_sidebar); luna_register_js_handler("onSettingSearch",on_setting_search); luna_register_js_handler("onSettingStatus",on_setting_status);
    luna_register_js_handler("onSettingGridDetails",on_setting_grid_details); luna_register_js_handler("onSettingHidden",on_setting_hidden); luna_register_js_handler("onSettingView",on_setting_view);
    luna_register_js_handler("onSettingSort",on_setting_sort); luna_register_js_handler("onSettingDesc",on_setting_desc);
    luna_register_js_handler("onSettingStartup",on_setting_startup); luna_register_js_handler("onSettingTerminal",on_setting_terminal);
    luna_register_js_handler("onFileDialogAccept",on_file_dialog_accept);
    luna_register_js_handler("onFileDialogCancel",on_file_dialog_cancel);
}

static char *build_html(void) {
    size_t cap=700000;char *h=calloc(1,cap);if(!h)return NULL;size_t n=0;
#define APPEND(...) do{int z=snprintf(h+n,cap-n,__VA_ARGS__);if(z<0||(size_t)z>=cap-n){free(h);return NULL;}n+=(size_t)z;}while(0)
    APPEND("<html><head><title>Luna Files</title></head><body><div id=\"app\" class=\"app luna-window-root\">");
    if(g_file_dialog_config.client_chrome) APPEND("%s",luna_window_standard_titlebar_html());
    APPEND("<div id=\"toolbar\" class=\"toolbar\">"
      "<div class=\"navgroup\"><div id=\"btn-back\" class=\"tool\" onclick=\"onBack()\">‹</div><div id=\"btn-forward\" class=\"tool\" onclick=\"onForward()\">›</div><div class=\"tool\" onclick=\"onUp()\">⌃</div></div>"
      "<div class=\"tool responsive-hide-tiny\" onclick=\"onHome()\">⌂</div><div class=\"tool responsive-hide-narrow\" onclick=\"onReload()\">↻</div>"
      "<input id=\"path\" class=\"location\" type=\"text\" placeholder=\"場所を入力\">"
      "<input id=\"search\" class=\"search\" type=\"text\" placeholder=\"このフォルダーを検索\">"
      "<div class=\"viewgroup\"><div class=\"tool\" onclick=\"onGrid()\">▦</div><div class=\"tool\" onclick=\"onList()\">☰</div></div>"
      "<div id=\"btn-hidden\" class=\"tool responsive-hide-tiny\" onclick=\"onHidden()\">◌</div><div id=\"btn-sort\" class=\"tool responsive-hide-tiny\" onclick=\"onSort()\">A↓</div>"
      "<div class=\"tool\" onclick=\"onNewFolder()\">＋</div><div id=\"btn-paste\" class=\"tool\" onclick=\"onPaste()\">▣</div>"
      "<div id=\"btn-settings\" class=\"tool\" onclick=\"onSettingsOpen()\">⚙</div>"
      "</div><div class=\"workbench\"><div id=\"sidebar\" class=\"sidebar\">"
      "<div class=\"side-label\">お気に入り</div>"
      "<div id=\"side-home\" class=\"side-item\" onclick=\"onSideHome()\">⌂　ホーム</div>"
      "<div id=\"side-desktop\" class=\"side-item\" onclick=\"onSideDesktop()\">▧　デスクトップ</div>"
      "<div id=\"side-documents\" class=\"side-item\" onclick=\"onSideDocuments()\">◇　書類</div>"
      "<div id=\"side-downloads\" class=\"side-item\" onclick=\"onSideDownloads()\">⇩　ダウンロード</div>"
      "<div id=\"side-pictures\" class=\"side-item\" onclick=\"onSidePictures()\">▧　ピクチャ</div>"
      "<div id=\"side-music\" class=\"side-item\" onclick=\"onSideMusic()\">♫　ミュージック</div>"
      "<div id=\"side-videos\" class=\"side-item\" onclick=\"onSideVideos()\">▶　ビデオ</div>"
      "<div class=\"side-label\">場所</div>"
      "<div id=\"side-trash\" class=\"side-item\" onclick=\"onSideTrash()\">♲　ごみ箱</div>"
      "<div id=\"side-root\" class=\"side-item\" onclick=\"onSideRoot()\">◈　ファイルシステム</div>"
      "<div id=\"side-devices-label\" class=\"side-label hidden\">デバイス</div>");
    for(int i=0;i<MAX_VOLUMES;i++)
        APPEND("<div id=\"vol%02d\" class=\"side-item volume-item hidden\" onclick=\"onVolume()\"></div>",i);
    APPEND("</div><div class=\"main\"><div class=\"subbar\"><div id=\"folder-title\" class=\"folder-title\">ホーム</div><div class=\"hint\">右クリックで操作メニュー　F4: ターミナル</div></div>"
      "<div id=\"file-grid\" class=\"file-grid grid\" onclick=\"onBlank()\">");
    for(int i=0;i<MAX_ITEMS;i++) APPEND("<div id=\"item%03d\" class=\"file-item grid-item hidden\" onclick=\"onItem()\"><div id=\"icon%03d\" class=\"file-icon\">◇</div><div id=\"name%03d\" class=\"file-name\"></div><div id=\"meta%03d\" class=\"file-meta\"></div></div>",i,i,i,i);
    APPEND("<div id=\"empty\" class=\"empty hidden\">このフォルダーは空です</div></div><div id=\"status\" class=\"status\"></div></div></div>");
    if(g_file_dialog_config.mode!=LUNA_FILE_DIALOG_MANAGER){
        APPEND("<div class=\"file-dialog-actions\">");
        if(g_file_dialog_config.mode==LUNA_FILE_DIALOG_SAVE_FILE)
            APPEND("<input id=\"file-dialog-name\" class=\"file-dialog-name\" type=\"text\" placeholder=\"ファイル名\">");
        APPEND("<div class=\"button\" onclick=\"onFileDialogCancel()\">キャンセル</div><div id=\"file-dialog-accept\" class=\"button primary\" onclick=\"onFileDialogAccept()\">開く</div></div>");
    }
    APPEND("<div id=\"context\" class=\"context hidden\"><div class=\"menu-item\" onclick=\"onOpen()\">開く</div><div class=\"menu-item\" onclick=\"onOpenWith()\">アプリケーションで開く…</div><div class=\"menu-item\" onclick=\"onSetDefaultApp()\">この種類の既定アプリを設定…</div><div class=\"menu-item\" onclick=\"onCreateLauncher()\">ランチャーを作成…</div><div class=\"menu-item\" onclick=\"onEditDesktop()\">デスクトップエントリを編集…</div><div class=\"menu-item\" onclick=\"onTerminal()\">ここでターミナルを開く　F4</div><div class=\"menu-sep\"></div><div class=\"menu-item\" onclick=\"onCopy()\">コピー　Ctrl+C</div><div class=\"menu-item\" onclick=\"onCut()\">切り取り　Ctrl+X</div><div class=\"menu-item\" onclick=\"onPaste()\">貼り付け　Ctrl+V</div><div class=\"menu-sep\"></div><div class=\"menu-item\" onclick=\"onRename()\">名前を変更　F2</div><div class=\"menu-item danger\" onclick=\"onDelete()\">ごみ箱へ移動　Delete</div><div class=\"menu-item danger\" onclick=\"onPermanentDelete()\">完全に削除…　Shift+Delete</div><div class=\"menu-sep\"></div><div class=\"menu-item\" onclick=\"onProperties()\">プロパティ</div></div>"
      "<div id=\"volume-context\" class=\"volume-context hidden\">"
      "<div class=\"menu-item\" onclick=\"onVolumeOpen()\">開く / マウント</div>"
      "<div class=\"menu-item\" onclick=\"onVolumeUnmount()\">アンマウント</div>"
      "<div class=\"menu-item\" onclick=\"onVolumeEject()\">取り出す</div>"
      "<div class=\"menu-sep\"></div>"
      "<div class=\"menu-item\" onclick=\"onVolumeContextCancel()\">キャンセル</div>"
      "</div>"
      "<div id=\"modal-wrap\" class=\"modal-wrap overlay-idle\"><div class=\"modal\"><div id=\"modal-title\" class=\"modal-title\"></div><div class=\"modal-info-stack\">");
    for(int i=0;i<MODAL_INFO_PARTS;i++)
        APPEND("<div id=\"modal-info%d\" class=\"modal-info\"></div>",i);
    APPEND("</div><input id=\"modal-input\" class=\"modal-input\" type=\"text\"><div class=\"modal-actions\"><div id=\"modal-cancel\" class=\"button\" onclick=\"onModalCancel()\">キャンセル</div><div id=\"modal-ok\" class=\"button primary\" onclick=\"onModalOk()\">OK</div></div></div></div>"
      "<div id=\"open-with-wrap\" class=\"modal-wrap hidden\"><div class=\"modal open-with-modal\"><div class=\"modal-title\">アプリケーションで開く</div><div id=\"open-with-info\" class=\"modal-info\"></div><div id=\"open-app-list\" class=\"open-app-list\">");
    for(int i=0;i<MAX_OPEN_APPS;i++) APPEND("<div id=\"openapp%02d\" class=\"app-choice hidden\" onclick=\"onOpenApp()\"></div>",i);
    APPEND("</div><div class=\"modal-actions\"><div class=\"button\" onclick=\"onOpenWithCancel()\">キャンセル</div><div class=\"button primary\" onclick=\"onOpenWithDefault()\">既定で開く</div></div></div></div>"
      "<div id=\"desktop-edit-wrap\" class=\"modal-wrap hidden\"><div class=\"modal desktop-edit-modal\">"
      "<div id=\"desktop-edit-title\" class=\"modal-title\">デスクトップエントリ</div>"
      "<div id=\"desktop-edit-hint\" class=\"desktop-edit-hint\"></div>"
      "<div class=\"desktop-edit-form\">"
      "<div id=\"desktop-edit-type\" class=\"desktop-edit-toggle\" onclick=\"onDesktopEditType()\">種類</div>"
      "<div class=\"desktop-edit-row\"><div class=\"desktop-edit-label\">名前</div><input id=\"desktop-edit-name\" class=\"desktop-edit-input\" type=\"text\" placeholder=\"表示名\"></div>"
      "<div class=\"desktop-edit-row\"><div class=\"desktop-edit-label\">コメント</div><input id=\"desktop-edit-comment\" class=\"desktop-edit-input\" type=\"text\" placeholder=\"説明（任意）\"></div>"
      "<div class=\"desktop-edit-row\"><div id=\"desktop-edit-exec-label\" class=\"desktop-edit-label\">コマンド (Exec)</div><input id=\"desktop-edit-exec\" class=\"desktop-edit-input\" type=\"text\" placeholder=\"command %%f\"></div>"
      "<div class=\"desktop-edit-row\"><div class=\"desktop-edit-label\">アイコン</div><input id=\"desktop-edit-icon\" class=\"desktop-edit-input\" type=\"text\" placeholder=\"theme-icon または ファイルパス\"></div>"
      "<div id=\"desktop-edit-workdir-row\" class=\"desktop-edit-row\"><div class=\"desktop-edit-label\">作業ディレクトリ (Path)</div><input id=\"desktop-edit-workdir\" class=\"desktop-edit-input\" type=\"text\" placeholder=\"任意\"></div>"
      "<div id=\"desktop-edit-terminal-row\" class=\"desktop-edit-row\"><div id=\"desktop-edit-terminal\" class=\"desktop-edit-toggle\" onclick=\"onDesktopEditTerminal()\">ターミナルで実行</div></div>"
      "<div id=\"desktop-edit-filename-row\" class=\"desktop-edit-row\"><div class=\"desktop-edit-label\">ファイル名</div><input id=\"desktop-edit-filename\" class=\"desktop-edit-input\" type=\"text\" placeholder=\"launcher.desktop\"></div>"
      "</div>"
      "<div class=\"modal-actions\"><div class=\"button\" onclick=\"onDesktopEditCancel()\">キャンセル</div><div class=\"button primary\" onclick=\"onDesktopEditSave()\">保存</div></div>"
      "</div></div>"
      "<div id=\"settings-wrap\" class=\"modal-wrap overlay-idle\"><div class=\"settings-panel\">"
      "<div class=\"settings-header\"><div class=\"modal-title\">設定</div><div class=\"settings-shortcut\">Ctrl+,</div></div>"
      "<div id=\"settings-scroll\" class=\"settings-scroll\"><div class=\"settings-note\">外観と動作をここで変更できます。フォントの種類とカスタムフォントは、次回起動時に読み込まれます。</div>"
      "<div class=\"settings-section\">外観</div>"
      "<div class=\"settings-row\" onclick=\"onSettingTheme()\"><div class=\"settings-name\">テーマ</div><div id=\"set-theme\" class=\"settings-state\"></div></div>"
      "<div class=\"settings-row\" onclick=\"onSettingAccent()\"><div class=\"settings-name\">アクセントカラー</div><div id=\"set-accent\" class=\"settings-state\"></div></div>"
      "<div class=\"settings-row\" onclick=\"onSettingFontFamily()\"><div class=\"settings-name\">フォントの種類</div><div id=\"set-font-family\" class=\"settings-state\"></div></div>"
      "<div class=\"settings-row\" onclick=\"onSettingCustomFont()\"><div class=\"settings-name\">カスタムフォント</div><div id=\"set-custom-font\" class=\"settings-state\"></div></div>"
      "<div class=\"settings-row\" onclick=\"onSettingFontSize()\"><div class=\"settings-name\">文字サイズ</div><div id=\"set-font-size\" class=\"settings-state\"></div></div>"
      "<div class=\"settings-row\" onclick=\"onSettingDensity()\"><div class=\"settings-name\">表示密度</div><div id=\"set-density\" class=\"settings-state\"></div></div>"
      "<div class=\"settings-row\" onclick=\"onSettingAnimations()\"><div class=\"settings-name\">アニメーション</div><div id=\"set-animations\" class=\"settings-state\"></div></div>"
      "<div class=\"settings-section\">表示</div>"
      "<div class=\"settings-row\" onclick=\"onSettingSidebar()\"><div class=\"settings-name\">サイドバー</div><div id=\"set-sidebar\" class=\"settings-state\"></div></div>"
      "<div class=\"settings-row\" onclick=\"onSettingSearch()\"><div class=\"settings-name\">検索欄</div><div id=\"set-search\" class=\"settings-state\"></div></div>"
      "<div class=\"settings-row\" onclick=\"onSettingStatus()\"><div class=\"settings-name\">ステータスバー</div><div id=\"set-status\" class=\"settings-state\"></div></div>"
      "<div class=\"settings-row\" onclick=\"onSettingGridDetails()\"><div class=\"settings-name\">アイコン表示の詳細</div><div id=\"set-grid-details\" class=\"settings-state\"></div></div>"
      "<div class=\"settings-row\" onclick=\"onSettingHidden()\"><div class=\"settings-name\">隠しファイル</div><div id=\"set-hidden\" class=\"settings-state\"></div></div>"
      "<div class=\"settings-row\" onclick=\"onSettingView()\"><div class=\"settings-name\">表示形式</div><div id=\"set-view\" class=\"settings-state\"></div></div>"
      "<div class=\"settings-row\" onclick=\"onSettingSort()\"><div class=\"settings-name\">並び順</div><div id=\"set-sort\" class=\"settings-state\"></div></div>"
      "<div class=\"settings-row\" onclick=\"onSettingDesc()\"><div class=\"settings-name\">並び方向</div><div id=\"set-desc\" class=\"settings-state\"></div></div>"
      "<div class=\"settings-section\">動作</div>"
      "<div class=\"settings-row\" onclick=\"onSettingStartup()\"><div class=\"settings-name\">起動時のフォルダー</div><div id=\"set-startup\" class=\"settings-state\"></div></div>"
      "<div class=\"settings-row\" onclick=\"onSettingTerminal()\"><div class=\"settings-name\">F4 ターミナル</div><div id=\"set-terminal\" class=\"settings-state\"></div></div>"
      "<div class=\"settings-note\">ファイルはXDGの既定アプリで開きます。右クリックからMIMEごとの関連付けを変更できます。</div></div>"
      "<div class=\"settings-footer\"><div class=\"button subtle\" onclick=\"onSettingsReset()\">初期設定に戻す</div><div class=\"settings-footer-right\"><div class=\"button primary\" onclick=\"onSettingsClose()\">閉じる</div></div></div></div></div>"
      "<div id=\"toast\" class=\"toast hidden\"></div>");
    APPEND("%s",luna_window_standard_overlay_html());
    APPEND("</div></body></html>");
#undef APPEND
    return h;
}

static void cache_ids(void) {
    g.id_app=luna_get_element_by_id("app");g.id_toolbar=luna_get_element_by_id("toolbar");g.id_sidebar=luna_get_element_by_id("sidebar");
    g.id_path=luna_get_element_by_id("path");g.id_search=luna_get_element_by_id("search");g.id_grid=luna_get_element_by_id("file-grid");
    g.id_status=luna_get_element_by_id("status");g.id_title=luna_get_element_by_id("folder-title");g.id_context=luna_get_element_by_id("context");
    g.id_side_devices_label=luna_get_element_by_id("side-devices-label");
    g.id_volume_context=luna_get_element_by_id("volume-context");
    g.id_modal=luna_get_element_by_id("modal-wrap");g.id_modal_title=luna_get_element_by_id("modal-title");g.id_modal_input=luna_get_element_by_id("modal-input");
    g.id_modal_cancel=luna_get_element_by_id("modal-cancel");g.id_modal_ok=luna_get_element_by_id("modal-ok");
    for(int i=0;i<MODAL_INFO_PARTS;i++){
        char id[32];snprintf(id,sizeof(id),"modal-info%d",i);
        g.id_modal_info_parts[i]=luna_get_element_by_id(id);
    }
    g.id_modal_info=g.id_modal_info_parts[0];g.id_open_with=luna_get_element_by_id("open-with-wrap");g.id_open_with_info=luna_get_element_by_id("open-with-info");g.id_open_app_list=luna_get_element_by_id("open-app-list");
    g.id_desktop_edit=luna_get_element_by_id("desktop-edit-wrap");
    g.id_desktop_edit_title=luna_get_element_by_id("desktop-edit-title");
    g.id_desktop_edit_hint=luna_get_element_by_id("desktop-edit-hint");
    g.id_desktop_edit_type=luna_get_element_by_id("desktop-edit-type");
    g.id_desktop_edit_name=luna_get_element_by_id("desktop-edit-name");
    g.id_desktop_edit_comment=luna_get_element_by_id("desktop-edit-comment");
    g.id_desktop_edit_exec_label=luna_get_element_by_id("desktop-edit-exec-label");
    g.id_desktop_edit_exec=luna_get_element_by_id("desktop-edit-exec");
    g.id_desktop_edit_icon=luna_get_element_by_id("desktop-edit-icon");
    g.id_desktop_edit_workdir_row=luna_get_element_by_id("desktop-edit-workdir-row");
    g.id_desktop_edit_workdir=luna_get_element_by_id("desktop-edit-workdir");
    g.id_desktop_edit_terminal_row=luna_get_element_by_id("desktop-edit-terminal-row");
    g.id_desktop_edit_terminal=luna_get_element_by_id("desktop-edit-terminal");
    g.id_desktop_edit_filename_row=luna_get_element_by_id("desktop-edit-filename-row");
    g.id_desktop_edit_filename=luna_get_element_by_id("desktop-edit-filename");
    g.id_toast=luna_get_element_by_id("toast");g.id_btn_back=luna_get_element_by_id("btn-back");
    g.id_btn_forward=luna_get_element_by_id("btn-forward");g.id_btn_paste=luna_get_element_by_id("btn-paste");g.id_btn_hidden=luna_get_element_by_id("btn-hidden");g.id_btn_sort=luna_get_element_by_id("btn-sort");
    g.id_btn_settings=luna_get_element_by_id("btn-settings");g.id_settings=luna_get_element_by_id("settings-wrap");g.id_settings_scroll=luna_get_element_by_id("settings-scroll");
    g.id_set_sidebar=luna_get_element_by_id("set-sidebar");g.id_set_search=luna_get_element_by_id("set-search");g.id_set_hidden=luna_get_element_by_id("set-hidden");
    g.id_set_view=luna_get_element_by_id("set-view");g.id_set_sort=luna_get_element_by_id("set-sort");g.id_set_desc=luna_get_element_by_id("set-desc");
    g.id_set_font_size=luna_get_element_by_id("set-font-size");g.id_set_font_family=luna_get_element_by_id("set-font-family");g.id_set_custom_font=luna_get_element_by_id("set-custom-font");
    g.id_set_density=luna_get_element_by_id("set-density");g.id_set_theme=luna_get_element_by_id("set-theme");g.id_set_accent=luna_get_element_by_id("set-accent");
    g.id_set_status=luna_get_element_by_id("set-status");g.id_set_grid_details=luna_get_element_by_id("set-grid-details");g.id_set_animations=luna_get_element_by_id("set-animations");
    g.id_set_startup=luna_get_element_by_id("set-startup");g.id_set_terminal=luna_get_element_by_id("set-terminal");
}

static int validate_ui_ids(void) {
    int ids[]={g.id_app,g.id_toolbar,g.id_sidebar,g.id_path,g.id_search,g.id_grid,g.id_status,g.id_title,g.id_context,g.id_side_devices_label,g.id_volume_context,g.id_modal,g.id_modal_cancel,g.id_modal_ok,g.id_open_with,g.id_open_with_info,g.id_open_app_list,g.id_desktop_edit,g.id_desktop_edit_title,g.id_desktop_edit_hint,g.id_desktop_edit_type,g.id_desktop_edit_name,g.id_desktop_edit_comment,g.id_desktop_edit_exec_label,g.id_desktop_edit_exec,g.id_desktop_edit_icon,g.id_desktop_edit_workdir_row,g.id_desktop_edit_workdir,g.id_desktop_edit_terminal_row,g.id_desktop_edit_terminal,g.id_desktop_edit_filename_row,g.id_desktop_edit_filename,g.id_toast,g.id_settings,g.id_settings_scroll,
               g.id_set_font_size,g.id_set_font_family,g.id_set_custom_font,g.id_set_density,g.id_set_theme,g.id_set_accent,g.id_set_status,g.id_set_grid_details,g.id_set_animations,g.id_set_startup,g.id_set_terminal};
    for(size_t i=0;i<sizeof(ids)/sizeof(ids[0]);i++){
        if(ids[i]<0){log_error("UI element lookup failed at index %zu",i);return 0;}
    }
    for(int i=0;i<MODAL_INFO_PARTS;i++){
        if(g.id_modal_info_parts[i]<0){log_error("modal info element lookup failed at part %d",i);return 0;}
    }
    return 1;
}

static void cursor_cb(GLFWwindow *w,double x,double y){
    if(g_window_drag_mode&&glfwGetMouseButton(w,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS){
        int wx,wy,ww,wh;
        glfwGetWindowPos(w,&wx,&wy);
        glfwGetWindowSize(w,&ww,&wh);
        if(g_window_drag_mode==1){
            int dx=(int)(x-g_window_drag_anchor_x);
            int dy=(int)(y-g_window_drag_anchor_y);
            if(dx||dy)glfwSetWindowPos(w,wx+dx,wy+dy);
        }else{
            int edge=g_window_resize_edges;
            int dx_left=(int)(x-g_window_drag_anchor_x);
            int dy_top=(int)(y-g_window_drag_anchor_y);
            int dx_right=(int)(x-g_window_drag_last_x);
            int dy_bottom=(int)(y-g_window_drag_last_y);
            int nx=wx,ny=wy,nw=ww,nh=wh;
            if(edge&LUNA_RESIZE_EDGE_LEFT){nx+=dx_left;nw-=dx_left;}
            if(edge&LUNA_RESIZE_EDGE_TOP){ny+=dy_top;nh-=dy_top;}
            if(edge&LUNA_RESIZE_EDGE_RIGHT)nw+=dx_right;
            if(edge&LUNA_RESIZE_EDGE_BOTTOM)nh+=dy_bottom;
            if(nw<MIN_WINDOW_W){if(edge&LUNA_RESIZE_EDGE_LEFT)nx-=MIN_WINDOW_W-nw;nw=MIN_WINDOW_W;}
            if(nh<MIN_WINDOW_H){if(edge&LUNA_RESIZE_EDGE_TOP)ny-=MIN_WINDOW_H-nh;nh=MIN_WINDOW_H;}
            if(nx!=wx||ny!=wy)glfwSetWindowPos(w,nx,ny);
            if(nw!=ww||nh!=wh)glfwSetWindowSize(w,nw,nh);
            if(edge&LUNA_RESIZE_EDGE_RIGHT)g_window_drag_last_x=x;
            if(edge&LUNA_RESIZE_EDGE_BOTTOM)g_window_drag_last_y=y;
        }
    }
    apply_grid_scroll();
    luna_mouse_move(x,y);

    /* Mouse movement changes the file-grid position only while its scrollbar
       thumb is being dragged.  Ordinary hover movement never writes it. */
    if(glfwGetMouseButton(g.window,GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS)
        store_grid_scroll_if_changed();
    request_redraw();
}
static void mouse_cb(GLFWwindow *w,int button,int action,int mods){
    double x,y;
    (void)w;
    glfwGetCursorPos(g.window,&x,&y);
    apply_grid_scroll();
    luna_mouse_move(x,y);
    luna_mouse_button(button,action,mods,x,y);
    if(button==GLFW_MOUSE_BUTTON_LEFT&&action==GLFW_RELEASE){
        g_window_drag_mode=0;
        g_window_resize_edges=LUNA_RESIZE_EDGE_NONE;
    }

    /* A left press may page the scrollbar track, and a release completes a
       thumb drag.  File clicks leave the values unchanged and are not saved. */
    if(button==GLFW_MOUSE_BUTTON_LEFT)store_grid_scroll_if_changed();
    request_redraw();
}
static void scroll_cb(GLFWwindow *w,double x,double y){
    (void)w;
    apply_grid_scroll();
    luna_scroll(x,y);
    store_grid_scroll_if_changed();
    request_redraw();
}
static void char_cb(GLFWwindow *w,unsigned int cp){(void)w;luna_char(cp);request_redraw();}
static void window_size_cb(GLFWwindow *w,int width,int height){
    (void)w;
    if(width<1||height<1)return;
    g.pending_window_width=width;
    g.pending_window_height=height;
    g.resize_pending=1;
    request_redraw();
}
static void framebuffer_cb(GLFWwindow *w,int width,int height){
    (void)w;(void)width;(void)height;
    g.framebuffer_dirty=1;
    request_redraw();
}

static void apply_pending_resize(void) {
    if(!g.resize_pending&&!g.framebuffer_dirty)return;

    /* Read the final GLFW sizes instead of trusting callback ordering.  On
       Wayland and HiDPI displays the logical window and framebuffer callbacks
       may arrive separately while the user is dragging the window edge. */
    int width=0,height=0;
    glfwGetWindowSize(g.window,&width,&height);
    if(width<1||height<1){
        width=g.pending_window_width;
        height=g.pending_window_height;
    }
    if(width>0&&height>0){
        g.window_width=width;
        g.window_height=height;
        luna_resize((float)width,(float)height);
        apply_display_classes();
    }

    /* luna_render() binds the current framebuffer-sized viewport itself.
       Keep resize handling GL-free here; the next redraw applies it once. */

    luna_framebuffer_resized();
    luna_mark_layout_dirty();
    g.resize_pending=0;
    g.framebuffer_dirty=0;
    g.resize_redraw_frames=4;
    request_redraw();
}

static void move_selection(int delta) {
    if(g.visible_count<=0)return;

    int current_slot=-1;
    for(int slot=0;slot<g.visible_count;slot++){
        int entry=g.slot_entry[slot];
        if(entry>=0&&entry<g.entry_count&&g.entries[entry].selected){current_slot=slot;break;}
    }

    int next_slot;
    if(current_slot<0)next_slot=delta<0?g.visible_count-1:0;
    else next_slot=current_slot+delta;
    if(next_slot<0)next_slot=0;
    if(next_slot>=g.visible_count)next_slot=g.visible_count-1;

    int entry=g.slot_entry[next_slot];
    if(entry<0||entry>=g.entry_count)return;
    clear_selection();
    g.entries[entry].selected=1;
    g.anchor_entry=entry;
    refresh_file_visuals();

    char id[32];snprintf(id,sizeof(id),"item%03d",next_slot);
    int item=luna_get_element_by_id(id);
    scroll_into_view(item);
    request_redraw();
}

static void key_cb(GLFWwindow *w,int key,int scancode,int action,int mods) {
    apply_grid_scroll();
    int consumed = 0;
    if(action==GLFW_PRESS||action==GLFW_REPEAT){
        int focused=g_focused_element_idx;
        if(action==GLFW_PRESS && key==GLFW_KEY_TAB && focused==g.id_path &&
           !(mods&(GLFW_MOD_SHIFT|GLFW_MOD_CONTROL|GLFW_MOD_ALT|GLFW_MOD_SUPER))){
            complete_path_input();
            consumed = 1;
        }
        else if(key==GLFW_KEY_ESCAPE){if(g.settings_open)close_settings_dialog();else if(g.desktop_edit_open)close_desktop_edit_dialog();else if(g.open_with_open)close_open_with_dialog();else if(g.modal_kind)close_modal();else if(g.volume_context>=0)hide_volume_context();else hide_context();}
        else if(key==GLFW_KEY_ENTER||key==GLFW_KEY_KP_ENTER){
            if(g.settings_open)close_settings_dialog();
            else if(g.desktop_edit_open)save_desktop_edit_dialog();
            else if(g.open_with_open){}
            else if(g.modal_kind)confirm_modal();
            else if(focused==g.id_path){const char *p=luna_get_value(g.id_path);navigate(p,1);}
            else if(focused!=g.id_search){int i=first_selected();if(i>=0)open_entry(i);}
        }
        else if((mods&GLFW_MOD_CONTROL)&&key==GLFW_KEY_COMMA)open_settings_dialog();
        else if(g.settings_open||g.open_with_open||g.desktop_edit_open){}
        else if((mods&GLFW_MOD_CONTROL)&&key==GLFW_KEY_C&&focused!=g.id_path&&focused!=g.id_search&&focused!=g.id_modal_input)clipboard_take_selection(0);
        else if((mods&GLFW_MOD_CONTROL)&&key==GLFW_KEY_X&&focused!=g.id_path&&focused!=g.id_search&&focused!=g.id_modal_input)clipboard_take_selection(1);
        else if((mods&GLFW_MOD_CONTROL)&&key==GLFW_KEY_V&&focused!=g.id_path&&focused!=g.id_search&&focused!=g.id_modal_input)paste_clipboard();
        else if((mods&GLFW_MOD_CONTROL)&&key==GLFW_KEY_A&&focused!=g.id_path&&focused!=g.id_search&&focused!=g.id_modal_input){for(int i=0;i<g.entry_count;i++)g.entries[i].selected=1;refresh_file_visuals();}
        else if((mods&GLFW_MOD_CONTROL)&&key==GLFW_KEY_L){reset_path_completion();luna_set_value(g.id_path,g.cwd);luna_focus_element(g.id_path);show_toast("場所バーを選択しました");}
        else if(key==GLFW_KEY_F2){int i=first_selected();if(i>=0)open_modal(2,i);}
        else if(action==GLFW_PRESS&&key==GLFW_KEY_F4)open_terminal_here();
        else if(key==GLFW_KEY_DELETE&&focused!=g.id_path&&focused!=g.id_search&&focused!=g.id_modal_input){if(mods&GLFW_MOD_SHIFT){int i=first_selected();if(i>=0)open_modal(7,i);}else delete_selected();}
        else if(key==GLFW_KEY_BACKSPACE&&focused!=g.id_path&&focused!=g.id_search&&focused!=g.id_modal_input)on_back(NULL);
        else if(key==GLFW_KEY_UP&&focused!=g.id_path&&focused!=g.id_search&&focused!=g.id_modal_input)move_selection(-1);
        else if(key==GLFW_KEY_DOWN&&focused!=g.id_path&&focused!=g.id_search&&focused!=g.id_modal_input)move_selection(1);
    }
    if(!consumed) luna_key(key,scancode,action,mods);

    /* Only keys which can move a scroll container are allowed to update the
       saved file-grid position.  Escape/Enter/dialog actions never write it. */
    if((action==GLFW_PRESS||action==GLFW_REPEAT) &&
       (key==GLFW_KEY_UP||key==GLFW_KEY_DOWN||
        key==GLFW_KEY_PAGE_UP||key==GLFW_KEY_PAGE_DOWN||
        key==GLFW_KEY_HOME||key==GLFW_KEY_END))
        store_grid_scroll_if_changed();
    request_redraw();
    (void)w;
}

static void platform_cursor(int type){GLFWcursor*c=NULL;if(type==1)c=g_hand_cursor;else if(type==2)c=g_cursor_ibeam;else if(type==3)c=g_cursor_crosshair;else if(type==4)c=g_cursor_hresize;else if(type==5)c=g_cursor_vresize;if(g.window)glfwSetCursor(g.window,c);}
static void platform_close(void){glfwSetWindowShouldClose(g.window,1);}
static void platform_iconify(void){glfwIconifyWindow(g.window);}
static void platform_maximize(void){if(glfwGetWindowAttrib(g.window,GLFW_MAXIMIZED))glfwRestoreWindow(g.window);else glfwMaximizeWindow(g.window);}
static void platform_begin_move(void){
    if(!g.window)return;
#ifdef LUNA_WM_CLIENT_H
    /* Prefer compositor grab on Luna/Wayland; glfwSetWindowPos is unavailable. */
    if(luna_wm_client_available() && luna_wm_client_start_move(g.window))
        return;
#endif
    glfwGetCursorPos(g.window,&g_window_drag_anchor_x,&g_window_drag_anchor_y);
    g_window_drag_last_x=g_window_drag_anchor_x;
    g_window_drag_last_y=g_window_drag_anchor_y;
    g_window_drag_mode=1;
    g_window_resize_edges=LUNA_RESIZE_EDGE_NONE;
}
static void platform_begin_resize(int edge){
    if(!g.window||edge==LUNA_RESIZE_EDGE_NONE)return;
    glfwGetCursorPos(g.window,&g_window_drag_anchor_x,&g_window_drag_anchor_y);
    g_window_drag_last_x=g_window_drag_anchor_x;
    g_window_drag_last_y=g_window_drag_anchor_y;
    g_window_drag_mode=2;
    g_window_resize_edges=edge;
}
static void platform_set_title(const char*title){if(g.window)glfwSetWindowTitle(g.window,title?title:"");}
static int platform_notify(const char*app,int kind,const char*title,const char*message){
    const char*urgency=kind==LUNA_NOTIFY_ERROR?"critical":kind==LUNA_NOTIFY_WARNING?"normal":"low";
    pid_t p=fork();if(p<0)return 0;
    if(p==0){execlp("notify-send","notify-send","-a",app?app:"Luna Files","-u",urgency,title?title:"",message?message:"",(char*)NULL);_exit(127);}
    int status=0;while(waitpid(p,&status,0)<0&&errno==EINTR){}
    return WIFEXITED(status)&&WEXITSTATUS(status)==0;
}

int luna_file_dialog_run(const LunaFileDialogConfig* config, LunaFileDialogResult* result) {
    memset(&g_file_dialog_config,0,sizeof(g_file_dialog_config));
    if(config)g_file_dialog_config=*config;
    if(!g_file_dialog_config.title)g_file_dialog_config.title=g_file_dialog_config.mode==LUNA_FILE_DIALOG_MANAGER?"Luna Files":"ファイルを選択";
    if(g_file_dialog_config.width<=0)g_file_dialog_config.width=700;
    if(g_file_dialog_config.height<=0)g_file_dialog_config.height=460;
    g_file_dialog_result=result;
    if(result)memset(result,0,sizeof(*result));
    g_file_dialog_cancelled=0;
    g_file_dialog_pending_save[0]=0;
    memset(&g,0,sizeof(g));g.anchor_entry=-1;g.context_entry=-1;g.modal_target=-1;g.open_with_target=-1;g.volume_context=-1;g.history_pos=-1;
    load_settings();
    if(g_file_dialog_config.mode!=LUNA_FILE_DIALOG_MANAGER){LunaWindowTheme system_theme;luna_window_theme_load_system(&system_theme);g.theme=system_theme.mode==LUNA_WINDOW_THEME_DARK?1:0;}
    g.window_width=g_file_dialog_config.width;g.window_height=g_file_dialog_config.height;
    apply_font_environment();
    if(!glfwInit()){log_error("GLFW initialization failed");return 1;}
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR,3);glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR,3);glfwWindowHint(GLFW_OPENGL_PROFILE,GLFW_OPENGL_CORE_PROFILE);
    /* X11: keep hidden until the first Luna frame is ready.  Wayland: a hidden
       GLFW window may never get a usable EGL/wl_shm buffer, so create visible
       (same rule as luna_linux.h) and still wait for the first swap before
       treating the dialog as shown. */
    {
        const char* session_type=getenv("XDG_SESSION_TYPE");
        const char* wayland_display=getenv("WAYLAND_DISPLAY");
        int wayland_session=
            (session_type && strcmp(session_type,"wayland")==0) ||
            (wayland_display && wayland_display[0] && !getenv("DISPLAY"));
        glfwWindowHint(GLFW_VISIBLE, wayland_session ? GLFW_TRUE : GLFW_FALSE);
    }
    glfwWindowHint(GLFW_DECORATED,g_file_dialog_config.client_chrome?GLFW_FALSE:GLFW_TRUE);
    g.window=glfwCreateWindow(g.window_width,g.window_height,g_file_dialog_config.title,NULL,NULL);
    if(!g.window){log_error("window creation failed");glfwTerminate();return 1;}
    glfwSetWindowSizeLimits(g.window,MIN_WINDOW_W,MIN_WINDOW_H,GLFW_DONT_CARE,GLFW_DONT_CARE);
    glfwMakeContextCurrent(g.window);glfwSwapInterval(1);
#ifdef LUNA_WM_CLIENT_H
    luna_wm_client_init(g.window);
#endif
    g_hand_cursor=glfwCreateStandardCursor(GLFW_HAND_CURSOR);g_cursor_ibeam=glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    g_cursor_crosshair=glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);g_cursor_hresize=glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);g_cursor_vresize=glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);
    LunaPlatform p={0};p.get_time=glfwGetTime;p.get_proc=(LunaGetProcFn)glfwGetProcAddress;p.set_cursor=platform_cursor;p.request_close=platform_close;p.iconify=platform_iconify;p.maximize_toggle=platform_maximize;p.request_redraw=platform_redraw;p.begin_move=platform_begin_move;p.begin_resize=platform_begin_resize;p.set_title=platform_set_title;p.system_notify=platform_notify;p.struct_size=sizeof(p);p.api_version=LUNA_UI_API_VERSION;luna_set_platform(&p);
    LunaInitConfig cfg={(float)g.window_width,(float)g.window_height,(LunaGetProcFn)glfwGetProcAddress,g_file_dialog_config.client_chrome};
    if(!luna_init(&cfg)){log_error("Luna UI initialization failed");glfwDestroyWindow(g.window);glfwTerminate();return 1;}
    register_handlers();luna_parse_css(luna_window_standard_css());luna_parse_css(UI_CSS);char *html=build_html();if(!html){log_error("UI allocation failed");luna_shutdown();glfwDestroyWindow(g.window);glfwTerminate();return 1;}luna_parse_html(html);free(html);{LunaWindowConfig wc={0};wc.root_id="app";wc.title=g_file_dialog_config.title;wc.chrome=g_file_dialog_config.client_chrome?LUNA_WINDOW_CHROME_CLIENT:LUNA_WINDOW_CHROME_NATIVE;wc.theme=g_file_dialog_config.mode==LUNA_FILE_DIALOG_MANAGER?(g.theme?LUNA_WINDOW_THEME_DARK:LUNA_WINDOW_THEME_LIGHT):LUNA_WINDOW_THEME_SYSTEM;luna_window_bind(&wc);}luna_wire_onclick_handlers();cache_ids();
    if(g_file_dialog_config.mode==LUNA_FILE_DIALOG_SAVE_FILE&&g_file_dialog_config.suggested_name){int name_id=luna_get_element_by_id("file-dialog-name");if(name_id>=0)luna_set_value(name_id,g_file_dialog_config.suggested_name);}
    if(g_file_dialog_config.mode!=LUNA_FILE_DIALOG_MANAGER){int accept_id=luna_get_element_by_id("file-dialog-accept");const char*label=g_file_dialog_config.accept_label?g_file_dialog_config.accept_label:(g_file_dialog_config.mode==LUNA_FILE_DIALOG_SAVE_FILE?"保存":g_file_dialog_config.mode==LUNA_FILE_DIALOG_SELECT_FOLDER?"選択":"開く");if(accept_id>=0)luna_set_text(accept_id,label);}
    if(!validate_ui_ids()){luna_window_unbind();luna_shutdown();glfwDestroyWindow(g.window);glfwTerminate();return 1;}
    apply_display_classes();
    glfwSetCursorPosCallback(g.window,cursor_cb);glfwSetMouseButtonCallback(g.window,mouse_cb);glfwSetScrollCallback(g.window,scroll_cb);glfwSetCharCallback(g.window,char_cb);glfwSetKeyCallback(g.window,key_cb);glfwSetWindowSizeCallback(g.window,window_size_cb);glfwSetFramebufferSizeCallback(g.window,framebuffer_cb);
    build_sidebar_paths();refresh_sidebar_visibility();
    const char *start=g_file_dialog_config.initial_path&&g_file_dialog_config.initial_path[0]?g_file_dialog_config.initial_path:home_dir();
    navigate(start,1);clipboard_import();update_toolbar_state();
    g.redraw=1;
    int window_shown=0;
    double prev=glfwGetTime();char old_search[MAX_SEARCH]="";
    while(!glfwWindowShouldClose(g.window)){
        double before=glfwGetTime();
        int settling=luna_visuals_settling();
        if(!g.redraw&&!settling){
            double wait=1.0;
            if(g.toast_until>0){double remain=g.toast_until-before;if(remain<wait)wait=remain;if(wait<0.01)wait=0.01;}
            if(g.volumes_next_refresh>0){double remain=g.volumes_next_refresh-before;if(remain<wait)wait=remain;if(wait<0.05)wait=0.05;}
            glfwWaitEventsTimeout(wait);
        }else glfwPollEvents();
        apply_pending_resize();
        double now=glfwGetTime(),dt=now-prev;prev=now;if(dt>0.1)dt=0.1;
        luna_window_tick(dt);
        if(g.volumes_next_refresh<=0.0||now>=g.volumes_next_refresh)refresh_volumes(0);
        const char *sv=luna_get_value(g.id_search);if(sv&&strcmp(sv,old_search)){safe_copy(old_search,sizeof(old_search),sv);safe_copy(g.search,sizeof(g.search),sv);render_files();}
        if(g.toast_until>0&&now>=g.toast_until){luna_add_class(g.id_toast,"hidden");g.toast_until=0;request_redraw();}
        apply_grid_scroll();
        settling=luna_update_settling(now,dt);
        apply_grid_scroll();
        if(g.redraw||settling||g.resize_redraw_frames>0){
            int fbw,fbh;glfwGetFramebufferSize(g.window,&fbw,&fbh);
            int presented=0;
            if(fbw>0&&fbh>0){
                apply_grid_scroll();luna_render(fbw,fbh);glfwSwapBuffers(g.window);
                presented=1;
                if(!window_shown){glfwShowWindow(g.window);window_shown=1;}
            } else if(!window_shown) {
                /* Some Wayland compositors do not allocate a framebuffer for
                   an unmapped window.  Map it once, but keep redraw pending so
                   the first configure immediately produces the Luna frame. */
                glfwShowWindow(g.window);
                window_shown=1;
            }
            /* Never consume the only redraw while the native framebuffer is
               temporarily 0x0 (common during initial Wayland/HiDPI mapping). */
            g.redraw=presented?0:1;
            if(g.resize_redraw_frames>0&&presented){g.resize_redraw_frames--;if(g.resize_redraw_frames>0)g.redraw=1;}
        }
    }
    save_settings();
    luna_window_unbind();
    luna_shutdown();
#ifdef LUNA_WM_CLIENT_H
    luna_wm_client_shutdown();
#endif
    if (g_hand_cursor) glfwDestroyCursor(g_hand_cursor);
    if (g_cursor_ibeam) glfwDestroyCursor(g_cursor_ibeam);
    if (g_cursor_crosshair) glfwDestroyCursor(g_cursor_crosshair);
    if (g_cursor_hresize) glfwDestroyCursor(g_cursor_hresize);
    if (g_cursor_vresize) glfwDestroyCursor(g_cursor_vresize);
    glfwDestroyWindow(g.window);glfwTerminate();
    return 0;
}

#if defined(__linux__) && (defined(__GNUC__) || defined(__clang__))
static const char* luna_window_file_dialog_env_copy(const char* name,
                                                     char* out,size_t cap) {
    const char* value=getenv(name);
    if (!value || !*value || !out || cap==0) return NULL;
    snprintf(out,cap,"%s",value);
    return out;
}

__attribute__((constructor))
static void luna_window_file_dialog_child_entry(void) {
    const char* marker=getenv("LUNA_WINDOW_INTERNAL_FILE_DIALOG_CHILD");
    if (!marker || strcmp(marker,"1")!=0) return;
    unsetenv("LUNA_WINDOW_INTERNAL_FILE_DIALOG_CHILD");

    LunaFileDialogConfig c;
    LunaFileDialogResult r;
    char title[512],initial[LUNA_WINDOW_PATH_MAX],suggested[LUNA_WINDOW_PATH_MAX];
    char accept[128],filter_name[256],filter_patterns[LUNA_WINDOW_PATH_MAX];
    memset(&c,0,sizeof(c));
    memset(&r,0,sizeof(r));
    const char* v;
    v=getenv("LUNA_WINDOW_FD_MODE"); c.mode=v?(LunaFileDialogMode)atoi(v):LUNA_FILE_DIALOG_OPEN_FILE;
    c.backend=LUNA_FILE_DIALOG_BACKEND_LUNA;
    c.title=luna_window_file_dialog_env_copy("LUNA_WINDOW_FD_TITLE",title,sizeof(title));
    c.initial_path=luna_window_file_dialog_env_copy("LUNA_WINDOW_FD_INITIAL",initial,sizeof(initial));
    c.suggested_name=luna_window_file_dialog_env_copy("LUNA_WINDOW_FD_NAME",suggested,sizeof(suggested));
    c.accept_label=luna_window_file_dialog_env_copy("LUNA_WINDOW_FD_ACCEPT",accept,sizeof(accept));
    c.filter_name=luna_window_file_dialog_env_copy("LUNA_WINDOW_FD_FILTER_NAME",filter_name,sizeof(filter_name));
    c.filter_patterns=luna_window_file_dialog_env_copy("LUNA_WINDOW_FD_FILTER",filter_patterns,sizeof(filter_patterns));
    v=getenv("LUNA_WINDOW_FD_WIDTH"); c.width=v?atoi(v):0;
    v=getenv("LUNA_WINDOW_FD_HEIGHT"); c.height=v?atoi(v):0;
    v=getenv("LUNA_WINDOW_FD_CLIENT_CHROME"); c.client_chrome=v?atoi(v):0;

    int rc=luna_file_dialog_run(&c,&r);
    if (rc==0 && r.accepted) {
        for (int i=0;i<r.count;i++) {
            if (!r.paths[i][0]) continue;
            fputs(r.paths[i],stdout);
            fputc('\n',stdout);
        }
        fflush(stdout);
    }
    _exit(rc==0?0:2);
}
#endif

#undef MAX_ITEMS
#undef MAX_CLIP_ITEMS
#undef MAX_HISTORY
#undef MAX_SEARCH
#undef MAX_OPEN_APPS
#undef TOAST_SECONDS
#undef MIN_WINDOW_W
#undef MIN_WINDOW_H
#undef DEFAULT_WINDOW_W
#undef DEFAULT_WINDOW_H
#undef MODAL_INFO_PARTS
#endif /* LUNA_WINDOW_FILE_DIALOG_IMPLEMENTATION_INCLUDED */
#endif /* LUNA_WINDOW_FILE_DIALOG_IMPLEMENTATION */

#endif /* LUNA_WINDOW_H */
