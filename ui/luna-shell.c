/*
 * luna-shell — Luna Desktop shell (macOS-style desktop environment)
 *
 * Runs fullscreen on top of luna-compositor (Wayland) right after kernel
 * boot — no Xorg, no Weston. Renders the whole desktop (menu bar, dock,
 * launchpad, widgets) with the Luna UI HTML/CSS engine and launches GTK
 * apps that connect to the same compositor.
 *
 * Copyright © 2026 Yuichiro Nakada / Project Luna (Vespera) — MPL 2.0
 */

#define LUNA_UI_IMPLEMENTATION
#define LUNA_UI_GLFW
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/utsname.h>
#include <dirent.h>
#include <math.h>
#include <GLFW/glfw3.h>
#include "luna-ui.h"

#define LUNA_SHELL_VERSION "1.1"

static GLFWwindow* g_window = NULL;
static int g_desktop_mode = 0;
static int g_fullscreen = 0;

static const char* g_layout_path = NULL;
static const char* g_css_path = NULL;
static const char* default_css =
#include "luna-shell.css.h"
;
static const char* default_html =
#include "luna-shell.html.h"
;

/* ── Applications (dock & launchpad) ──
 * default_cmd is the fallback; cmd[] is the mutable runtime command
 * (overridden by LUNA_APP_<NAME> env vars and by the settings dialog). */
typedef struct {
    const char* key;
    const char* name;
    const char* env;
    const char* default_cmd;
    char        cmd[256];
    pid_t       pid;
} LunaApp;

static LunaApp g_apps[] = {
    { .key = "files",    .name = "Files",     .env = "LUNA_APP_FILES",    .default_cmd = "nautilus"             },
    { .key = "terminal", .name = "Terminal",  .env = "LUNA_APP_TERMINAL", .default_cmd = "sakura"               },
    { .key = "browser",  .name = "Browser",   .env = "LUNA_APP_BROWSER",  .default_cmd = "firefox"              },
    { .key = "editor",   .name = "Editor",    .env = "LUNA_APP_EDITOR",   .default_cmd = "gedit"                },
    { .key = "music",    .name = "Music",     .env = "LUNA_APP_MUSIC",    .default_cmd = "gnome-music"          },
    { .key = "settings", .name = "Settings",  .env = "LUNA_APP_SETTINGS", .default_cmd = "gnome-control-center" },
    { .key = "demo",     .name = "GTK Demo",  .env = "LUNA_APP_DEMO",     .default_cmd = "gtk4-demo"            },
    { .key = "hello",    .name = "Hello GTK", .env = "LUNA_APP_HELLO",    .default_cmd = "hello-gtk"            },
};
#define APP_COUNT ((int)(sizeof(g_apps) / sizeof(g_apps[0])))

/* ── Persistent settings ── */

typedef struct {
    char wallpaper[32]; /* "night" | "ocean" | "forest" | "sunset" */
    char hostname[64];
} LunaSettings;

static LunaSettings g_settings;

static void settings_path(char* buf, size_t n) {
    const char* home = getenv("HOME");
    if (!home || !*home) home = "/root";
    snprintf(buf, n, "%s/.config/luna-shell/settings.conf", home);
}

static void init_app_cmds(void) {
    for (int i = 0; i < APP_COUNT; i++) {
        const char* env = g_apps[i].env ? getenv(g_apps[i].env) : NULL;
        if (env && *env)
            snprintf(g_apps[i].cmd, sizeof(g_apps[i].cmd), "%s", env);
        else
            snprintf(g_apps[i].cmd, sizeof(g_apps[i].cmd), "%s", g_apps[i].default_cmd);
    }
}

static void settings_defaults(void) {
    snprintf(g_settings.wallpaper, sizeof(g_settings.wallpaper), "night");
    snprintf(g_settings.hostname, sizeof(g_settings.hostname), "Luna Desktop");
}

static void settings_load(void) {
    settings_defaults();
    init_app_cmds();
    char path[512];
    settings_path(path, sizeof(path));
    FILE* f = fopen(path, "r");
    if (!f) return;
    char line[512], section[64] = "";
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = 0;
        if (!line[0] || line[0] == '#') continue;
        if (line[0] == '[') {
            snprintf(section, sizeof(section), "%.*s", (int)(sizeof(section)-1), line + 1);
            char* e = strchr(section, ']'); if (e) *e = 0;
            continue;
        }
        char* eq = strchr(line, '=');
        if (!eq) continue;
        *eq = 0;
        const char* key = line, *val = eq + 1;
        if (!strcmp(section, "apps")) {
            for (int i = 0; i < APP_COUNT; i++)
                if (!strcmp(g_apps[i].key, key))
                    snprintf(g_apps[i].cmd, sizeof(g_apps[i].cmd), "%s", val);
        } else if (!strcmp(section, "shell")) {
            if (!strcmp(key, "wallpaper"))
                snprintf(g_settings.wallpaper, sizeof(g_settings.wallpaper), "%s", val);
            else if (!strcmp(key, "hostname"))
                snprintf(g_settings.hostname, sizeof(g_settings.hostname), "%s", val);
        }
    }
    fclose(f);
}

static void ensure_config_dir(void) {
    const char* home = getenv("HOME");
    if (!home || !*home) home = "/root";
    char dir[512];
    snprintf(dir, sizeof(dir), "%s/.config", home);
    mkdir(dir, 0755);
    snprintf(dir, sizeof(dir), "%s/.config/luna-shell", home);
    mkdir(dir, 0755);
}

static void settings_save(void) {
    ensure_config_dir();
    char path[512];
    settings_path(path, sizeof(path));
    FILE* f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "[luna-shell] cannot write settings: %s\n", path);
        return;
    }
    fprintf(f, "# Luna Shell settings — auto-generated\n");
    fprintf(f, "[apps]\n");
    for (int i = 0; i < APP_COUNT; i++)
        fprintf(f, "%s=%s\n", g_apps[i].key, g_apps[i].cmd);
    fprintf(f, "\n[shell]\n");
    fprintf(f, "wallpaper=%s\n", g_settings.wallpaper);
    fprintf(f, "hostname=%s\n", g_settings.hostname);
    fclose(f);
}

/* ── Element indices resolved after layout load ── */
static int g_luna_menu_idx = -1;
static int g_cc_idx        = -1;
static int g_launchpad_idx = -1;
static int g_about_idx     = -1;
static int g_confirm_idx   = -1;
static int g_toast_idx     = -1;
static int g_lp_search_idx = -1;
static int g_settings_idx          = -1;
static int g_settings_panel_apps   = -1;
static int g_settings_panel_disp   = -1;
static int g_stab_apps_idx         = -1;
static int g_stab_disp_idx         = -1;
/* Cached menubar hit-test indices (avoid repeated ID lookups in mouse hook) */
static int g_mb_logo_idx  = -1;
static int g_mb_cc_idx    = -1;
static int g_mb_wifi_idx  = -1;

static double g_toast_deadline = 0.0;
static double g_last_clock = -10.0;
static double g_last_stats = -10.0;
static double g_now = 0.0;
static char   g_lp_query[160] = "";

/* Pending confirmation action */
enum { ACT_NONE = 0, ACT_SHUTDOWN, ACT_RESTART, ACT_LOGOUT };
static int g_pending_action = ACT_NONE;

/* /proc/stat sampling for CPU% */
static unsigned long long g_cpu_prev_idle = 0, g_cpu_prev_total = 0;

/* ── Small helpers ── */

static int elem_idx_of(LunaElement* e) {
    for (int i = 0; i < luna_element_count(); i++)
        if (luna_element_at(i) == e) return i;
    return -1;
}

static int ci_contains(const char* hay, const char* needle) {
    if (!needle[0]) return 1;
    size_t nl = strlen(needle);
    for (const char* p = hay; *p; p++) {
        size_t i = 0;
        while (i < nl && p[i] && tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) i++;
        if (i == nl) return 1;
    }
    return 0;
}

/* Toggle visibility via the "hidden" class so display_mode is recomputed. */
static void set_hidden(int idx, int hidden) {
    if (idx < 0) return;
    if (hidden) luna_add_class(idx, "hidden");
    else luna_remove_class(idx, "hidden");
    luna_update_element_style(idx);
    luna_mark_layout_dirty();
}

static int is_shown(int idx) {
    if (idx < 0) return 0;
    LunaElement* e = luna_element_at(idx);
    return e && strstr(e->class_name, "hidden") == NULL;
}

/* Wire handler onto element and every descendant (no event bubbling in engine). */
static void wire_subtree(int root, LunaEventHandler fn) {
    if (root < 0) return;
    luna_set_on_click(root, fn);
    for (int i = 0; i < luna_element_count(); i++) {
        for (int p = luna_element_at(i)->parent_idx; p != -1; p = luna_element_at(p)->parent_idx) {
            if (p == root) { luna_set_on_click(i, fn); break; }
        }
    }
}

static void center_element(int idx) {
    if (idx < 0) return;
    LunaElement* e = luna_element_at(idx);
    float w = e->css_width > 0 ? e->css_width : e->w;
    float h = e->css_height > 0 ? e->css_height : e->h;
    e->rel_x = floorf((luna_window_width  - w) * 0.5f);
    e->rel_y = floorf((luna_window_height - h) * 0.42f);
    e->pos_overridden_x = 1;
    e->pos_overridden_y = 1;
    luna_mark_layout_dirty();
}

/* Center the dock horizontally — replaces unsupported CSS transform: translateX(-50%). */
static void center_dock(void) {
    int idx = luna_get_element_by_id("dock");
    if (idx < 0) return;
    LunaElement* e = luna_element_at(idx);
    float dw = (e->css_width > 0) ? e->css_width : (e->w > 10 ? e->w : 542.0f);
    float target = floorf((luna_window_width - dw) * 0.5f);
    if (!e->pos_overridden_x || fabsf(e->rel_x - target) > 0.5f) {
        e->rel_x = target;
        e->pos_overridden_x = 1;
        luna_mark_layout_dirty();
    }
}

/* ── Toast notifications ── */

static void toast_show(const char* title, const char* msg, double secs) {
    int t = luna_get_element_by_id("toast_title");
    int m = luna_get_element_by_id("toast_msg");
    if (t != -1) luna_set_text(t, title);
    if (m != -1) luna_set_text(m, msg);
    set_hidden(g_toast_idx, 0);
    g_toast_deadline = g_now + secs;
}

static void on_toast_close(LunaElement* e) {
    (void)e;
    set_hidden(g_toast_idx, 1);
    g_toast_deadline = 0.0;
}

/* ── App launching ── */

static LunaApp* resolve_app(LunaElement* e) {
    for (int idx = elem_idx_of(e); idx != -1; idx = luna_element_at(idx)->parent_idx) {
        const char* id = luna_element_at(idx)->id;
        const char* key = NULL;
        if (!strncmp(id, "dock_", 5)) key = id + 5;
        else if (!strncmp(id, "lp_", 3)) key = id + 3;
        if (key) {
            for (int i = 0; i < APP_COUNT; i++)
                if (!strcmp(g_apps[i].key, key)) return &g_apps[i];
        }
    }
    return NULL;
}

static void app_set_dot(LunaApp* app, int running) {
    char dot_id[64];
    snprintf(dot_id, sizeof(dot_id), "dot_%s", app->key);
    int idx = luna_get_element_by_id(dot_id);
    if (idx != -1) luna_element_at(idx)->opacity = running ? 1.0f : 0.0f;
}

static void app_launch(LunaApp* app) {
    const char* cmd = app->cmd[0] ? app->cmd : app->default_cmd;
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
        _exit(127);
    }
    if (pid > 0) {
        app->pid = pid;
        app_set_dot(app, 1);
        char msg[320];
        snprintf(msg, sizeof(msg), "Starting \"%s\"", cmd);
        toast_show(app->name, msg, 4.0);
        fprintf(stderr, "[luna-shell] launch %s: %s (pid %d)\n", app->name, cmd, (int)pid);
    }
}

static void reap_children(void) {
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < APP_COUNT; i++) {
            if (g_apps[i].pid == pid) {
                g_apps[i].pid = 0;
                app_set_dot(&g_apps[i], 0);
                if (WIFEXITED(status) && WEXITSTATUS(status) == 127)
                    toast_show(g_apps[i].name, "App not installed (LUNA_APP_*)", 5.0);
            }
        }
    }
}

static void spawn_command(const char* cmd) {
    pid_t pid = fork();
    if (pid == 0) {
        setsid();
        execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
        _exit(127);
    }
}

/* ── Overlays: Luna menu / Control Center / Launchpad ── */

static void dismiss_luna_menu(int trap_idx) {
    (void)trap_idx;
    if (!is_shown(g_luna_menu_idx)) return;
    set_hidden(g_luna_menu_idx, 1);
}

static void dismiss_cc(int trap_idx) {
    (void)trap_idx;
    if (!is_shown(g_cc_idx)) return;
    set_hidden(g_cc_idx, 1);
}

static void on_luna_menu(LunaElement* e) {
    (void)e;
    dismiss_cc(g_cc_idx);
    if (is_shown(g_luna_menu_idx)) { dismiss_luna_menu(g_luna_menu_idx); return; }
    set_hidden(g_luna_menu_idx, 0);
}

static void on_control_center(LunaElement* e) {
    (void)e;
    dismiss_luna_menu(g_luna_menu_idx);
    if (is_shown(g_cc_idx)) { dismiss_cc(g_cc_idx); return; }
    set_hidden(g_cc_idx, 0);
}

static void launchpad_close(void) {
    if (!is_shown(g_launchpad_idx)) return;
    set_hidden(g_launchpad_idx, 1);
}

static void on_launchpad_open(LunaElement* e) {
    (void)e;
    dismiss_luna_menu(g_luna_menu_idx);
    dismiss_cc(g_cc_idx);
    set_hidden(g_launchpad_idx, 0);
}

static void on_launchpad_close(LunaElement* e) {
    (void)e;
    launchpad_close();
}

static void on_launch_app(LunaElement* e) {
    LunaApp* app = resolve_app(e);
    launchpad_close();
    if (app) app_launch(app);
}

static void on_dock_click(LunaElement* e) {
    LunaApp* app = resolve_app(e);
    if (app) app_launch(app);
}

static void on_trash(LunaElement* e) {
    (void)e;
    toast_show("Trash", "Trash is empty.", 3.0);
}

/* Dismiss popovers when clicking outside them. */
static int hit_inside(int hit, int root) {
    for (int p = hit; p != -1; p = luna_element_at(p)->parent_idx)
        if (p == root) return 1;
    return 0;
}

static void on_mouse_release_hook(int hit, int drag_moved) {
    if (drag_moved || hit < 0) return;
    if (is_shown(g_luna_menu_idx) &&
        !hit_inside(hit, g_luna_menu_idx) &&
        !hit_inside(hit, g_mb_logo_idx))
        dismiss_luna_menu(g_luna_menu_idx);
    if (is_shown(g_cc_idx) &&
        !hit_inside(hit, g_cc_idx) &&
        !hit_inside(hit, g_mb_cc_idx) &&
        !hit_inside(hit, g_mb_wifi_idx))
        dismiss_cc(g_cc_idx);
}

/* ── About window ── */

static void on_about(LunaElement* e) {
    (void)e;
    dismiss_luna_menu(g_luna_menu_idx);
    center_element(g_about_idx);
    set_hidden(g_about_idx, 0);
}

static void on_about_close(LunaElement* e) {
    (void)e;
    set_hidden(g_about_idx, 1);
}

/* ── Settings dialog ── */

static void apply_wallpaper(const char* theme) {
    int idx = luna_get_element_by_id("wallpaper");
    if (idx < 0) return;
    /* Remove all theme classes; night is the default gradient (no extra class). */
    luna_remove_class(idx, "ocean");
    luna_remove_class(idx, "forest");
    luna_remove_class(idx, "sunset");
    if (strcmp(theme, "night") != 0)
        luna_add_class(idx, theme);
    luna_update_element_style(idx);
    luna_mark_layout_dirty();
}

static void settings_mark_wallpaper(const char* theme) {
    const char* ids[] = { "wp_night", "wp_ocean", "wp_forest", "wp_sunset" };
    for (int i = 0; i < 4; i++) {
        int ti = luna_get_element_by_id(ids[i]);
        if (ti < 0) continue;
        if (!strcmp(ids[i] + 3, theme)) /* "wp_night"+3 = "night" */
            luna_add_class(ti, "selected");
        else
            luna_remove_class(ti, "selected");
        luna_update_element_style(ti);
    }
    luna_mark_layout_dirty();
}

static void settings_populate_ui(void) {
    /* Fill app command inputs */
    for (int i = 0; i < APP_COUNT; i++) {
        char input_id[64];
        snprintf(input_id, sizeof(input_id), "pref_%s", g_apps[i].key);
        int idx = luna_get_element_by_id(input_id);
        if (idx >= 0) luna_set_value(idx, g_apps[i].cmd);
    }
    /* Hostname */
    int h = luna_get_element_by_id("pref_hostname");
    if (h >= 0) luna_set_value(h, g_settings.hostname);
    /* Wallpaper selection markers */
    settings_mark_wallpaper(g_settings.wallpaper);
    /* Show apps tab by default */
    set_hidden(g_settings_panel_apps, 0);
    set_hidden(g_settings_panel_disp, 1);
    if (g_stab_apps_idx >= 0) { luna_add_class(g_stab_apps_idx, "active"); luna_update_element_style(g_stab_apps_idx); }
    if (g_stab_disp_idx  >= 0) { luna_remove_class(g_stab_disp_idx, "active"); luna_update_element_style(g_stab_disp_idx); }
}

static void on_settings_open(LunaElement* e) {
    (void)e;
    dismiss_luna_menu(g_luna_menu_idx);
    settings_populate_ui();
    center_element(g_settings_idx);
    set_hidden(g_settings_idx, 0);
}

static void on_settings_close(LunaElement* e) {
    (void)e;
    set_hidden(g_settings_idx, 1);
}

static void on_settings_save(LunaElement* e) {
    (void)e;
    /* Read app commands back from inputs */
    for (int i = 0; i < APP_COUNT; i++) {
        char input_id[64];
        snprintf(input_id, sizeof(input_id), "pref_%s", g_apps[i].key);
        int idx = luna_get_element_by_id(input_id);
        if (idx >= 0) {
            const char* v = luna_get_value(idx);
            if (v && *v) snprintf(g_apps[i].cmd, sizeof(g_apps[i].cmd), "%s", v);
        }
    }
    /* Hostname */
    int h = luna_get_element_by_id("pref_hostname");
    if (h >= 0) {
        const char* v = luna_get_value(h);
        if (v && *v) snprintf(g_settings.hostname, sizeof(g_settings.hostname), "%s", v);
    }
    settings_save();
    apply_wallpaper(g_settings.wallpaper);
    set_hidden(g_settings_idx, 1);
    toast_show("Settings", "Settings saved successfully.", 3.0);
}

static void on_settings_tab(LunaElement* e) {
    int idx = elem_idx_of(e);
    /* Walk up to find a .stab element */
    int tab_idx = -1;
    for (int i = idx; i >= 0; i = luna_element_at(i)->parent_idx) {
        if (strstr(luna_element_at(i)->class_name, "stab") &&
            luna_element_at(i)->id[0]) { tab_idx = i; break; }
    }
    if (tab_idx < 0) return;
    const char* id = luna_element_at(tab_idx)->id;
    int is_apps = !strcmp(id, "stab_apps");
    int is_disp = !strcmp(id, "stab_disp");

    if (g_stab_apps_idx >= 0) {
        if (is_apps) luna_add_class(g_stab_apps_idx, "active");
        else luna_remove_class(g_stab_apps_idx, "active");
        luna_update_element_style(g_stab_apps_idx);
    }
    if (g_stab_disp_idx >= 0) {
        if (is_disp) luna_add_class(g_stab_disp_idx, "active");
        else luna_remove_class(g_stab_disp_idx, "active");
        luna_update_element_style(g_stab_disp_idx);
    }
    set_hidden(g_settings_panel_apps, !is_apps);
    set_hidden(g_settings_panel_disp, !is_disp);
}

static void on_wallpaper_select(LunaElement* e) {
    int idx = elem_idx_of(e);
    const char* theme = NULL;
    for (int i = idx; i >= 0; i = luna_element_at(i)->parent_idx) {
        const char* id = luna_element_at(i)->id;
        if (id[0] == 'w' && id[1] == 'p' && id[2] == '_' && id[3]) { theme = id + 3; break; }
    }
    if (!theme) return;
    snprintf(g_settings.wallpaper, sizeof(g_settings.wallpaper), "%s", theme);
    settings_mark_wallpaper(theme);
    apply_wallpaper(theme);
}

/* ── Power / session actions with confirmation ── */

static void on_restart(LunaElement* e);
static void on_shutdown(LunaElement* e);
static void on_logout(LunaElement* e);

static void confirm_open(int action) {
    dismiss_luna_menu(g_luna_menu_idx);
    g_pending_action = action;
    int t  = luna_get_element_by_id("confirm_title");
    int m  = luna_get_element_by_id("confirm_msg");
    int ok = luna_get_element_by_id("confirm_ok");
    switch (action) {
    case ACT_SHUTDOWN:
        if (t)  luna_set_text(t,  "Shut Down");
        if (m)  luna_set_text(m,  "Are you sure you want to shut down your computer now?");
        if (ok) luna_set_text(ok, "Shut Down");
        break;
    case ACT_RESTART:
        if (t)  luna_set_text(t,  "Restart");
        if (m)  luna_set_text(m,  "Are you sure you want to restart your computer now?");
        if (ok) luna_set_text(ok, "Restart");
        break;
    case ACT_LOGOUT:
        if (t)  luna_set_text(t,  "Log Out");
        if (m)  luna_set_text(m,  "Quit the Luna session and return to the console?");
        if (ok) luna_set_text(ok, "Log Out");
        break;
    }
    center_element(luna_get_element_by_id("confirm_box"));
    set_hidden(g_confirm_idx, 0);
}

static void on_shutdown(LunaElement* e) { (void)e; confirm_open(ACT_SHUTDOWN); }
static void on_restart(LunaElement* e)  { (void)e; confirm_open(ACT_RESTART); }
static void on_logout(LunaElement* e)   { (void)e; confirm_open(ACT_LOGOUT); }

static void on_confirm_cancel(LunaElement* e) {
    (void)e;
    g_pending_action = ACT_NONE;
    set_hidden(g_confirm_idx, 1);
}

static void on_confirm_ok(LunaElement* e) {
    (void)e;
    int action = g_pending_action;
    g_pending_action = ACT_NONE;
    set_hidden(g_confirm_idx, 1);
    switch (action) {
    case ACT_SHUTDOWN: spawn_command("systemctl poweroff"); break;
    case ACT_RESTART:  spawn_command("systemctl reboot");  break;
    case ACT_LOGOUT:   glfwSetWindowShouldClose(g_window, GLFW_TRUE); break;
    }
}

/* ── Control Center toggles & sliders ── */

static void on_cc_toggle(LunaElement* e) {
    int idx = -1;
    for (int i = elem_idx_of(e); i != -1; i = luna_element_at(i)->parent_idx) {
        if (strstr(luna_element_at(i)->class_name, "cc_toggle")) { idx = i; break; }
    }
    if (idx == -1) return;
    LunaElement* t = luna_element_at(idx);
    int now_on = strstr(t->class_name, "on") == NULL;
    if (now_on) luna_add_class(idx, "on");
    else         luna_remove_class(idx, "on");
    char knob_id[80];
    snprintf(knob_id, sizeof(knob_id), "%s_knob", t->id);
    int k = luna_get_element_by_id(knob_id);
    if (k != -1) {
        luna_element_at(k)->rel_x = now_on ? 21.0f : 3.0f;
        luna_element_at(k)->pos_overridden_x = 1;
    }
    luna_update_element_style(idx);
    luna_mark_layout_dirty();
}

static void slider_tick(const char* thumb_id, const char* fill_id, const char* track_id) {
    int ti = luna_get_element_by_id(thumb_id);
    int fi = luna_get_element_by_id(fill_id);
    int ki = luna_get_element_by_id(track_id);
    if (ti == -1 || fi == -1 || ki == -1) return;
    LunaElement* th = luna_element_at(ti);
    LunaElement* tr = luna_element_at(ki);
    float track_w = tr->w > 0 ? tr->w : 268.0f;
    float max_x = track_w - 19.0f;
    if (th->rel_x < 1.0f)    th->rel_x = 1.0f;
    if (th->rel_x > max_x)   th->rel_x = max_x;
    th->rel_y = 1.0f;
    th->pos_overridden_x = 1;
    th->pos_overridden_y = 1;
    LunaElement* fill = luna_element_at(fi);
    fill->css_width   = th->rel_x + 9.0f;
    fill->has_css_width = 1;
}

/* ── System status: clock, CPU, memory, disk, battery, network ── */

static float read_cpu_percent(void) {
    FILE* f = fopen("/proc/stat", "r");
    if (!f) return 0.0f;
    unsigned long long v[8] = {0};
    int n = fscanf(f, "cpu %llu %llu %llu %llu %llu %llu %llu %llu",
                   &v[0],&v[1],&v[2],&v[3],&v[4],&v[5],&v[6],&v[7]);
    fclose(f);
    if (n < 4) return 0.0f;
    unsigned long long idle  = v[3] + v[4];
    unsigned long long total = 0;
    for (int i = 0; i < 8; i++) total += v[i];
    unsigned long long didle  = idle  - g_cpu_prev_idle;
    unsigned long long dtotal = total - g_cpu_prev_total;
    g_cpu_prev_idle  = idle;
    g_cpu_prev_total = total;
    if (dtotal == 0) return 0.0f;
    return 100.0f * (float)(dtotal - didle) / (float)dtotal;
}

static int read_mem_percent(unsigned long* total_kb_out) {
    FILE* f = fopen("/proc/meminfo", "r");
    if (!f) return 0;
    unsigned long total = 0, avail = 0;
    char line[160];
    while (fgets(line, sizeof(line), f)) {
        sscanf(line, "MemTotal: %lu kB",     &total);
        sscanf(line, "MemAvailable: %lu kB", &avail);
    }
    fclose(f);
    if (total_kb_out) *total_kb_out = total;
    if (!total) return 0;
    return (int)(100.0 * (double)(total - avail) / (double)total);
}

static int read_battery_percent(void) {
    static const char* paths[] = {
        "/sys/class/power_supply/BAT0/capacity",
        "/sys/class/power_supply/BAT1/capacity",
        "/sys/class/power_supply/BAT2/capacity",
    };
    for (size_t i = 0; i < sizeof(paths)/sizeof(paths[0]); i++) {
        FILE* f = fopen(paths[i], "r");
        if (!f) continue;
        int pct = -1;
        if (fscanf(f, "%d", &pct) != 1) pct = -1;
        fclose(f);
        if (pct >= 0) return pct;
    }
    return -1;
}

static const char* read_net_status(void) {
    DIR* d = opendir("/sys/class/net");
    if (!d) return "Offline";
    struct dirent* de;
    int wired_up = 0, wifi_up = 0;
    while ((de = readdir(d))) {
        if (de->d_name[0] == '.' || !strcmp(de->d_name, "lo")) continue;
        char path[300];
        snprintf(path, sizeof(path), "/sys/class/net/%s/operstate", de->d_name);
        FILE* f = fopen(path, "r");
        if (!f) continue;
        char state[24] = "";
        if (!fgets(state, sizeof(state), f)) state[0] = 0;
        fclose(f);
        if (strncmp(state, "up", 2)) continue;
        snprintf(path, sizeof(path), "/sys/class/net/%s/wireless", de->d_name);
        struct stat st;
        if (stat(path, &st) == 0) wifi_up = 1;
        else wired_up = 1;
    }
    closedir(d);
    if (wifi_up) return "Wi-Fi";
    if (wired_up) return "Ethernet";
    return "Offline";
}

static void set_bar_fill(const char* fill_id, float pct) {
    int fi = luna_get_element_by_id(fill_id);
    if (fi == -1) return;
    LunaElement* fill = luna_element_at(fi);
    float bar_w = 176.0f;
    int p = fill->parent_idx;
    if (p != -1 && luna_element_at(p)->w > 0) bar_w = luna_element_at(p)->w;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 100.0f) pct = 100.0f;
    fill->css_width = 2.0f + (bar_w - 2.0f) * pct / 100.0f;
    fill->has_css_width = 1;
    luna_mark_layout_dirty();
}

static void update_clock(void) {
    if (g_now - g_last_clock < 1.0) return;
    g_last_clock = g_now;
    time_t t = time(NULL);
    struct tm* tm_info = localtime(&t);
    if (!tm_info) return;
    char buf[64];
    int idx = luna_get_element_by_id("mb_clock");
    if (idx != -1 && strftime(buf, sizeof(buf), "%a %b %e  %H:%M", tm_info) > 0)
        luna_set_text(idx, buf);
    idx = luna_get_element_by_id("wg_time");
    if (idx != -1 && strftime(buf, sizeof(buf), "%H:%M", tm_info) > 0)
        luna_set_text(idx, buf);
    idx = luna_get_element_by_id("wg_date");
    if (idx != -1 && strftime(buf, sizeof(buf), "%A, %B %e", tm_info) > 0)
        luna_set_text(idx, buf);
}

static void update_stats(void) {
    if (g_now - g_last_stats < 2.0) return;
    g_last_stats = g_now;
    char buf[64];

    float cpu = read_cpu_percent();
    int idx = luna_get_element_by_id("st_cpu_val");
    if (idx != -1) { snprintf(buf, sizeof(buf), "%d%%", (int)cpu); luna_set_text(idx, buf); }
    set_bar_fill("st_cpu_fill", cpu);
    set_bar_fill("cc_cpu_fill", cpu);

    int mem = read_mem_percent(NULL);
    idx = luna_get_element_by_id("st_mem_val");
    if (idx != -1) { snprintf(buf, sizeof(buf), "%d%%", mem); luna_set_text(idx, buf); }
    set_bar_fill("st_mem_fill", (float)mem);
    set_bar_fill("cc_mem_fill", (float)mem);

    struct statvfs vfs;
    if (statvfs("/", &vfs) == 0 && vfs.f_blocks > 0) {
        double total = (double)vfs.f_blocks * vfs.f_frsize;
        double avail = (double)vfs.f_bavail * vfs.f_frsize;
        double used_pct = 100.0 * (total - avail) / total;
        idx = luna_get_element_by_id("st_disk_val");
        if (idx != -1) {
            snprintf(buf, sizeof(buf), "%.0fG free", avail / (1024.0 * 1024.0 * 1024.0));
            luna_set_text(idx, buf);
        }
        set_bar_fill("st_disk_fill", (float)used_pct);
    }

    idx = luna_get_element_by_id("mb_bat");
    if (idx != -1) {
        int bat = read_battery_percent();
        if (bat >= 0) snprintf(buf, sizeof(buf), "%d%%", bat);
        else snprintf(buf, sizeof(buf), "AC");
        luna_set_text(idx, buf);
    }
    idx = luna_get_element_by_id("mb_wifi");
    if (idx != -1) luna_set_text(idx, read_net_status());
}

static void update_launchpad_filter(void) {
    if (g_lp_search_idx == -1 || !is_shown(g_launchpad_idx)) return;
    const char* q = luna_get_value(g_lp_search_idx);
    if (!q) q = "";
    if (!strcmp(q, g_lp_query)) return;
    snprintf(g_lp_query, sizeof(g_lp_query), "%s", q);
    for (int i = 0; i < APP_COUNT; i++) {
        char tile_id[64];
        snprintf(tile_id, sizeof(tile_id), "lp_%s", g_apps[i].key);
        int idx = luna_get_element_by_id(tile_id);
        if (idx != -1)
            luna_element_at(idx)->display_none = !ci_contains(g_apps[i].name, q);
    }
    luna_mark_layout_dirty();
}

static void fill_about_info(void) {
    char buf[192];
    struct utsname un;
    int idx = luna_get_element_by_id("about_kernel");
    if (idx != -1 && uname(&un) == 0) {
        snprintf(buf, sizeof(buf), "Kernel   %s %s", un.sysname, un.release);
        luna_set_text(idx, buf);
    }
    idx = luna_get_element_by_id("about_mem");
    unsigned long total_kb = 0;
    read_mem_percent(&total_kb);
    if (idx != -1 && total_kb > 0) {
        snprintf(buf, sizeof(buf), "Memory   %.1f GB", (double)total_kb / (1024.0 * 1024.0));
        luna_set_text(idx, buf);
    }
}

/* ── Wiring ── */

static void register_handlers(void) {
    luna_register_js_handler("onLunaMenu",      on_luna_menu);
    luna_register_js_handler("onControlCenter", on_control_center);
    luna_register_js_handler("onAbout",         on_about);
    luna_register_js_handler("onAboutClose",    on_about_close);
    luna_register_js_handler("onSettingsOpen",  on_settings_open);
    luna_register_js_handler("onSettingsClose", on_settings_close);
    luna_register_js_handler("onSettingsSave",  on_settings_save);
    luna_register_js_handler("onSettingsTab",   on_settings_tab);
    luna_register_js_handler("onWpSelect",      on_wallpaper_select);
    luna_register_js_handler("onRestart",       on_restart);
    luna_register_js_handler("onShutdown",      on_shutdown);
    luna_register_js_handler("onLogout",        on_logout);
    luna_register_js_handler("onConfirmCancel", on_confirm_cancel);
    luna_register_js_handler("onConfirmOk",     on_confirm_ok);
    luna_register_js_handler("onLaunchpadOpen", on_launchpad_open);
    luna_register_js_handler("onLaunchpadClose",on_launchpad_close);
    luna_register_js_handler("onLaunchApp",     on_launch_app);
    luna_register_js_handler("onDockClick",     on_dock_click);
    luna_register_js_handler("onTrash",         on_trash);
    luna_register_js_handler("onToastClose",    on_toast_close);
    luna_register_js_handler("onCcToggle",      on_cc_toggle);
}

static void bind_indices(void) {
    g_luna_menu_idx     = luna_get_element_by_id("luna_menu");
    g_cc_idx            = luna_get_element_by_id("control_center");
    g_launchpad_idx     = luna_get_element_by_id("launchpad");
    g_about_idx         = luna_get_element_by_id("about_win");
    g_confirm_idx       = luna_get_element_by_id("confirm_overlay");
    g_toast_idx         = luna_get_element_by_id("toast");
    g_lp_search_idx     = luna_get_element_by_id("lp_search");
    g_settings_idx      = luna_get_element_by_id("settings_win");
    g_settings_panel_apps = luna_get_element_by_id("settings_panel_apps");
    g_settings_panel_disp = luna_get_element_by_id("settings_panel_disp");
    g_stab_apps_idx     = luna_get_element_by_id("stab_apps");
    g_stab_disp_idx     = luna_get_element_by_id("stab_disp");
    g_mb_logo_idx       = luna_get_element_by_id("mb_logo");
    g_mb_cc_idx         = luna_get_element_by_id("mb_cc");
    g_mb_wifi_idx       = luna_get_element_by_id("mb_wifi");

    /* Wire dock items */
    for (int i = 0; i < APP_COUNT; i++) {
        char id[64];
        snprintf(id, sizeof(id), "dock_%s", g_apps[i].key);
        wire_subtree(luna_get_element_by_id(id), on_dock_click);
        snprintf(id, sizeof(id), "lp_%s", g_apps[i].key);
        wire_subtree(luna_get_element_by_id(id), on_launch_app);
        app_set_dot(&g_apps[i], 0);
    }
    wire_subtree(luna_get_element_by_id("mb_logo"),       on_luna_menu);
    wire_subtree(luna_get_element_by_id("dock_launchpad"),on_launchpad_open);
    wire_subtree(luna_get_element_by_id("dock_trash"),    on_trash);
    wire_subtree(luna_get_element_by_id("toast_close"),   on_toast_close);
    wire_subtree(luna_get_element_by_id("cc_wifi"),       on_cc_toggle);
    wire_subtree(luna_get_element_by_id("cc_bt"),         on_cc_toggle);
    wire_subtree(luna_get_element_by_id("cc_night"),      on_cc_toggle);

    /* Wire wallpaper selection thumbs */
    const char* wp_ids[] = {"wp_night","wp_ocean","wp_forest","wp_sunset"};
    for (int i = 0; i < 4; i++)
        wire_subtree(luna_get_element_by_id(wp_ids[i]), on_wallpaper_select);

    /* Wire settings tab buttons */
    wire_subtree(g_stab_apps_idx, on_settings_tab);
    wire_subtree(g_stab_disp_idx, on_settings_tab);

    /* Initial Control Center knob positions (wifi & bt start "on") */
    int k = luna_get_element_by_id("cc_wifi_knob");
    if (k != -1) { luna_element_at(k)->rel_x = 21.0f; luna_element_at(k)->pos_overridden_x = 1; }
    k = luna_get_element_by_id("cc_bt_knob");
    if (k != -1) { luna_element_at(k)->rel_x = 21.0f; luna_element_at(k)->pos_overridden_x = 1; }
}

/* ── GLFW platform glue ── */

static void glfw_error_cb(int err, const char* desc) {
    fprintf(stderr, "GLFW Error %d: %s\n", err, desc);
}

static double plat_time(void)     { return glfwGetTime(); }
static void*  plat_proc(const char* n) { return (void*)glfwGetProcAddress(n); }
static void   plat_close(void)    { glfwSetWindowShouldClose(g_window, GLFW_TRUE); }
static void   plat_iconify(void)  { glfwIconifyWindow(g_window); }
static void   plat_maximize(void) {
    if (glfwGetWindowAttrib(g_window, GLFW_MAXIMIZED)) glfwRestoreWindow(g_window);
    else glfwMaximizeWindow(g_window);
}

static GLFWcursor* g_cursors[6];
static void plat_cursor(int t) {
    GLFWcursor* c = (t >= 0 && t < 6) ? g_cursors[t] : NULL;
    glfwSetCursor(g_window, c);
}

static void on_cursor(GLFWwindow* w, double x, double y) { (void)w; luna_mouse_move(x, y); }
static void on_mouse(GLFWwindow* w, int btn, int act, int mods) {
    (void)w;
    double x, y; glfwGetCursorPos(g_window, &x, &y);
    luna_mouse_button(btn, act, mods, x, y);
}
static void on_scroll(GLFWwindow* w, double xo, double yo) { (void)w; luna_scroll(xo, yo); }
static void on_char(GLFWwindow* w, unsigned int cp) { (void)w; luna_char(cp); }

static void take_timestamped_screenshot(void) {
    char path[512];
    time_t t = time(NULL);
    struct tm* tm_info = localtime(&t);
    if (tm_info && strftime(path, sizeof(path), "luna_%Y%m%d_%H%M%S.png", tm_info) > 0)
        luna_take_screenshot(path);
    else
        luna_take_screenshot("luna.png");
}

static void on_key(GLFWwindow* w, int key, int sc, int act, int mods) {
    (void)w;
    if (act == GLFW_PRESS) {
        if (key == GLFW_KEY_ESCAPE) {
            if (is_shown(g_settings_idx))    { on_settings_close(NULL); return; }
            if (is_shown(g_launchpad_idx))   { launchpad_close();       return; }
            if (is_shown(g_confirm_idx))     { on_confirm_cancel(NULL); return; }
            if (is_shown(g_luna_menu_idx))   { dismiss_luna_menu(g_luna_menu_idx); return; }
            if (is_shown(g_cc_idx))          { dismiss_cc(g_cc_idx);    return; }
        }
        if (key == GLFW_KEY_LEFT_SUPER || key == GLFW_KEY_RIGHT_SUPER || key == GLFW_KEY_F4) {
            if (is_shown(g_launchpad_idx)) launchpad_close();
            else on_launchpad_open(NULL);
            return;
        }
        if (key == GLFW_KEY_COMMA && (mods & GLFW_MOD_SUPER)) {
            on_settings_open(NULL);
            return;
        }
        if (key == GLFW_KEY_F12) { take_timestamped_screenshot(); return; }
    }
    luna_key(key, sc, act, mods);
}

static void parse_args(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--desktop") || !strcmp(argv[i], "-d"))
            { g_desktop_mode = 1; g_fullscreen = 1; }
        else if (!strcmp(argv[i], "--fullscreen") || !strcmp(argv[i], "-f"))
            g_fullscreen = 1;
        else if (!strcmp(argv[i], "--size") && i + 1 < argc)
            sscanf(argv[++i], "%fx%f", &luna_window_width, &luna_window_height);
        else if (!strcmp(argv[i], "--layout") && i + 1 < argc)
            g_layout_path = argv[++i];
        else if (!strcmp(argv[i], "--css") && i + 1 < argc)
            g_css_path = argv[++i];
        else if (!strcmp(argv[i], "--screenshot") && i + 1 < argc)
            luna_request_screenshot(argv[++i]);
        else if (!strcmp(argv[i], "--help") || !strcmp(argv[i], "-h")) {
            fprintf(stderr,
                "luna-shell " LUNA_SHELL_VERSION " — Luna Desktop shell\n"
                "usage: luna-shell [--desktop] [--fullscreen] [--size WxH]\n"
                "                  [--layout PATH] [--css PATH] [--screenshot PATH]\n"
                "  env: LUNA_DESKTOP_LAYOUT / LUNA_DESKTOP_CSS — external layout override\n"
                "       LUNA_APP_<NAME>=<cmd> — override dock/launchpad app commands\n"
                "  keys: Super/F4 — Launchpad, Esc — close overlay\n"
                "        Cmd+, — Settings, F12 — screenshot\n"
                "  settings: ~/.config/luna-shell/settings.conf\n");
            exit(0);
        }
    }
    if (!g_layout_path) g_layout_path = getenv("LUNA_DESKTOP_LAYOUT");
    if (!g_css_path)    g_css_path    = getenv("LUNA_DESKTOP_CSS");
}

int main(int argc, char** argv) {
    luna_window_width  = 1440.0f;
    luna_window_height = 900.0f;
    parse_args(argc, argv);
    settings_load();

#if defined(GLFW_PLATFORM_WAYLAND)
    if (getenv("WAYLAND_DISPLAY"))
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
#endif
    glfwSetErrorCallback(glfw_error_cb);
    if (!glfwInit()) { fprintf(stderr, "[luna-shell] glfwInit failed\n"); return 1; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    /* Borderless maximised window — never exclusive fullscreen which fights compositors. */
    if (g_fullscreen) {
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    }

    /* Always windowed (NULL monitor): avoids mode-change conflicts with
     * luna-compositor and lets GTK apps on the same session share the display. */
    g_window = glfwCreateWindow((int)luna_window_width, (int)luna_window_height,
                                "Luna Desktop", NULL, NULL);
    if (!g_window) { fprintf(stderr, "[luna-shell] failed to create window\n"); glfwTerminate(); return 1; }
    g_luna_glfw_window = g_window;
    glfwMakeContextCurrent(g_window);
    glfwSwapInterval(1);

    g_cursors[0] = NULL;
    g_cursors[1] = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
    g_cursors[2] = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    g_cursors[3] = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);
    g_cursors[4] = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    g_cursors[5] = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);

    LunaPlatform plat = {
        .get_time       = plat_time,
        .get_proc       = plat_proc,
        .set_cursor     = plat_cursor,
        .request_close  = plat_close,
        .iconify        = plat_iconify,
        .maximize_toggle= plat_maximize,
    };
    luna_set_platform(&plat);

    LunaInitConfig cfg = { luna_window_width, luna_window_height, plat_proc };
    if (!luna_init(&cfg)) { fprintf(stderr, "[luna-shell] luna_init failed\n"); glfwTerminate(); return 1; }

    int css_loaded = g_css_path && luna_load_css_file(g_css_path);
    int loaded = 0;
    if (g_layout_path) {
        luna_set_html_base_dir(g_layout_path);
        loaded = luna_load_html_file(g_layout_path);
    }
    if (!loaded) {
        luna_set_html_base_dir("ui");
        if (!css_loaded) luna_parse_css(default_css);
        luna_parse_html(default_html);
    }
    luna_inject_body_background();
    register_handlers();
    luna_wire_onclick_handlers();
    bind_indices();
    luna_set_mouse_release_hook(on_mouse_release_hook);
    fill_about_info();
    apply_wallpaper(g_settings.wallpaper);
    read_cpu_percent(); /* prime /proc/stat delta */

    glfwSetCursorPosCallback(g_window, on_cursor);
    glfwSetMouseButtonCallback(g_window, on_mouse);
    glfwSetScrollCallback(g_window, on_scroll);
    glfwSetKeyCallback(g_window, on_key);
    glfwSetCharCallback(g_window, on_char);
    /* No framebuffer-size callback — we query size every frame to handle
     * HiDPI and compositor-driven resizes without double-calling luna_resize. */

    g_now = glfwGetTime();
    toast_show("Welcome to Luna", "Your desktop is ready.", 8.0);

    double last   = glfwGetTime();
    int    prev_ww = 0, prev_wh = 0;

    while (!glfwWindowShouldClose(g_window)) {
        g_now = glfwGetTime();
        double dt = g_now - last;
        last = g_now;

        /* Window / framebuffer sizes */
        int ww, wh, fbw, fbh;
        glfwGetWindowSize(g_window, &ww, &wh);
        glfwGetFramebufferSize(g_window, &fbw, &fbh);
        /* Only call luna_resize when dimensions actually change. */
        if (ww != prev_ww || wh != prev_wh) {
            luna_resize((float)ww, (float)wh);
            prev_ww = ww; prev_wh = wh;
        }
        glViewport(0, 0, fbw, fbh);
        glClearColor(0.04f, 0.05f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        reap_children();
        update_clock();
        update_stats();
        update_launchpad_filter();
        center_dock();
        slider_tick("bright_thumb", "bright_fill", "bright_track");
        slider_tick("vol_thumb", "vol_fill", "vol_track");
        if (g_toast_deadline > 0.0 && g_now > g_toast_deadline) {
            set_hidden(g_toast_idx, 1);
            g_toast_deadline = 0.0;
        }

        luna_update(g_now, dt);
        luna_render(fbw, fbh);
        glfwSwapBuffers(g_window);
        luna_flush_pending_screenshot();
        glfwPollEvents();
    }
    luna_shutdown();
    glfwTerminate();
    return 0;
}
