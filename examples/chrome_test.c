/* chrome_test.c — verifies that client chrome follows the compositor's
 * xdg-decoration answer, i.e. that an SSD window does not also draw its own
 * titlebar.  Run under a compositor and flip prefer_ssd underneath it.
 */
#define LUNA_UI_IMPLEMENTATION
#define LUNA_WINDOW_IMPLEMENTATION
#include "luna-ui.h"
#include "luna-window.h"

static char g_html[8192];
static char g_css[16384];

static void report(void) {
    int tb = luna_get_element_by_id("luna-titlebar");
    LunaElement* e = tb >= 0 ? luna_element_at(tb) : NULL;
    fprintf(stderr, "[chrome] decoration=%s titlebar=%s rect=%.0fx%.0f\n",
            luna_platform_decoration_mode() == LUNA_DECORATION_SERVER ? "SERVER"
          : luna_platform_decoration_mode() == LUNA_DECORATION_CLIENT ? "CLIENT"
                                                                     : "unknown",
            (tb >= 0 && luna_element_visible(tb)) ? "SHOWN" : "hidden",
            e ? e->w : 0.0f, e ? e->h : 0.0f);
}

static void on_frame(double dt, void* ud) {
    static int n = 0;
    (void)dt; (void)ud;
    luna_window_tick(dt);
    /* Sample twice a second so a live renegotiation is visible in the log. */
    if (++n % 30 == 0) report();
}

static void on_init(void* ud) {
    LunaWindowConfig wc;
    (void)ud;
    memset(&wc, 0, sizeof(wc));
    wc.title = "chrome test";
    wc.app_name = "chrome-test";
    wc.chrome = LUNA_WINDOW_CHROME_AUTO;
    wc.theme = LUNA_WINDOW_THEME_SYSTEM;
    luna_window_bind(&wc);
    report();
}

int main(void) {
    LunaAppConfig cfg;
    snprintf(g_html, sizeof(g_html),
             "<div id=\"luna-window-root\" class=\"luna-window-root\">%s"
             "<div class=\"content\"><p>content</p></div>%s</div>",
             luna_window_standard_titlebar_html(),
             luna_window_standard_overlay_html());
    snprintf(g_css, sizeof(g_css),
             "%s body{margin:0}.luna-window-root{width:100%%;height:100%%;"
             "display:flex;flex-direction:column}.content{flex:1;padding:16px}",
             luna_window_standard_css());
    memset(&cfg, 0, sizeof(cfg));
    cfg.title = "chrome test";
    cfg.width = 640; cfg.height = 400;
    cfg.resizable = 1; cfg.vsync = 1; cfg.frameless = 1;
    cfg.html = g_html; cfg.css = g_css;
    cfg.on_init = on_init;
    cfg.on_frame = on_frame;
    return luna_app_run(&cfg);
}
