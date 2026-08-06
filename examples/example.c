// gcc -o example example.c -lGL -lm -lglfw
#include <stdio.h>

#define LUNA_UI_IMPLEMENTATION
#include "../luna-ui.h"

static const char* app_html =
    "<body>"
    "  <main class=\"card\">"
    "    <h1>Luna UI</h1>"
    "    <p id=\"status\">Header-only native host is running.</p>"
    "    <button onclick=\"hello\">Click me</button>"
    "  </main>"
    "</body>";

static const char* app_css =
    "body{display:flex;align-items:center;justify-content:center;"
    "background:linear-gradient(135deg,#0f172a,#312e81);color:white;}"
    ".card{width:420px;padding:32px;border-radius:24px;"
    "background:rgba(255,255,255,.12);backdrop-filter:blur(18px);"
    "box-shadow:0 24px 70px rgba(0,0,0,.35);}"
    "h1{font-size:34px;margin-bottom:12px;}"
    "p{color:rgba(255,255,255,.78);margin-bottom:24px;}"
    "button{padding:12px 20px;border-radius:12px;border:0;"
    "background:#fff;color:#312e81;font-weight:bold;cursor:pointer;}"
    "button:hover{transform:scale(1.04);}";

static void hello(LunaElement* element) {
    int status = luna_get_element_by_id("status");
    (void)element;
    if (status >= 0) luna_set_text(status, "The shared Luna UI core handled this click.");
}

static void app_init(void* userdata) {
    (void)userdata;
    luna_register_js_handler("hello", hello);
}

int main(void) {
    LunaAppConfig cfg = {0};
    cfg.title = "Luna UI header-only example";
    cfg.width = 960;
    cfg.height = 640;
    cfg.resizable = 1;
    cfg.vsync = 1;
    cfg.html = app_html;
    cfg.css = app_css;
    cfg.on_init = app_init;
    return luna_app_run(&cfg);
}
