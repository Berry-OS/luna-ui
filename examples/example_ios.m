#define LUNA_UI_IMPLEMENTATION
#include "../luna-ui.h"

static const char* app_html =
    "<body><main class=\"card\"><h1>Luna UI</h1>"
    "<p>Header-only UIKit + OpenGL ES host.</p></main></body>";
static const char* app_css =
    "body{display:flex;align-items:center;justify-content:center;background:#111827;color:white;}"
    ".card{width:82%;padding:28px;border-radius:24px;background:#1f2937;}"
    "h1{font-size:34px;}";

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    LunaAppConfig cfg = {0};
    cfg.title = "Luna UI";
    cfg.html = app_html;
    cfg.css = app_css;
    cfg.vsync = 1;
    return luna_app_run(&cfg);
}
