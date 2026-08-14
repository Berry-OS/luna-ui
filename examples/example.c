// Build with `make example`; Linux selects native Wayland or X11 at runtime.
#include <stdio.h>

#define LUNA_UI_IMPLEMENTATION
#include "../luna-ui.h"

static const char* app_html =
    "<body>"
    "  <div class=\"nebula nebula-a\"></div>"
    "  <div class=\"nebula nebula-b\"></div>"
    "  <div class=\"stars stars-a\"></div>"
    "  <div class=\"stars stars-b\"></div>"
    "  <div class=\"stars stars-c\"></div>"
    "  <main class=\"card\">"
    "    <div class=\"eyebrow\">NATIVE UI · GLASS</div>"
    "    <h1>Luna UI</h1>"
    "    <p id=\"status\">Header-only native host is running.</p>"
    "    <button onclick=\"hello\">Click me <span>→</span></button>"
    "  </main>"
    "</body>";

static const char* app_css =
    "body{display:flex;align-items:center;justify-content:center;position:relative;overflow:hidden;"
    "background:linear-gradient(145deg,#050816 0%,#0a1024 42%,#111a3d 72%,#15113a 100%);"
    "color:#f7f9ff;}"

    ".nebula{position:absolute;border-radius:999px;pointer-events:none;}"
    ".nebula-a{width:560px;height:560px;left:-180px;top:-210px;"
    "background:radial-gradient(circle,rgba(86,119,255,.24) 0%,rgba(78,89,210,.10) 43%,rgba(0,0,0,0) 72%);"
    "animation:floatA 11s ease-in-out infinite alternate;}"
    ".nebula-b{width:620px;height:620px;right:-230px;bottom:-300px;"
    "background:radial-gradient(circle,rgba(171,96,255,.20) 0%,rgba(83,59,180,.09) 44%,rgba(0,0,0,0) 72%);"
    "animation:floatB 14s ease-in-out infinite alternate;}"

    ".stars{position:absolute;left:0;top:0;width:100%;height:100%;pointer-events:none;}"
    ".stars-a{opacity:.95;"
    "background:radial-gradient(circle at 8% 18%,#fff 0 1.2px,transparent 1.7px),"
    "radial-gradient(circle at 18% 72%,#dce8ff 0 1px,transparent 1.5px),"
    "radial-gradient(circle at 29% 28%,#fff 0 1.4px,transparent 1.9px),"
    "radial-gradient(circle at 42% 12%,#cfe0ff 0 1px,transparent 1.5px),"
    "radial-gradient(circle at 57% 22%,#fff 0 1.2px,transparent 1.7px),"
    "radial-gradient(circle at 68% 68%,#d9e4ff 0 1.4px,transparent 1.9px),"
    "radial-gradient(circle at 81% 16%,#fff 0 1px,transparent 1.5px),"
    "radial-gradient(circle at 91% 54%,#d7e2ff 0 1.3px,transparent 1.8px),"
    "radial-gradient(circle at 76% 86%,#fff 0 1px,transparent 1.5px),"
    "radial-gradient(circle at 35% 87%,#fff 0 1.2px,transparent 1.7px);"
    "animation:twinkleA 3.2s ease-in-out infinite;}"

    ".stars-b{opacity:.55;"
    "background:radial-gradient(circle at 13% 42%,#9fc3ff 0 1.7px,transparent 2.3px),"
    "radial-gradient(circle at 24% 11%,#fff 0 1.5px,transparent 2.1px),"
    "radial-gradient(circle at 48% 76%,#c4d8ff 0 1.7px,transparent 2.3px),"
    "radial-gradient(circle at 61% 45%,#fff 0 1.5px,transparent 2.1px),"
    "radial-gradient(circle at 73% 31%,#cdb8ff 0 1.8px,transparent 2.4px),"
    "radial-gradient(circle at 86% 78%,#fff 0 1.5px,transparent 2.1px);"
    "animation:twinkleB 4.8s ease-in-out infinite;}"

    ".stars-c{opacity:.32;"
    "background:radial-gradient(circle at 6% 84%,#fff 0 2.1px,transparent 2.8px),"
    "radial-gradient(circle at 39% 52%,#d7c9ff 0 2px,transparent 2.7px),"
    "radial-gradient(circle at 66% 9%,#bcd7ff 0 2.2px,transparent 2.9px),"
    "radial-gradient(circle at 95% 24%,#fff 0 2px,transparent 2.7px);"
    "animation:twinkleC 6.2s ease-in-out infinite;}"

    ".card{position:relative;width:420px;padding:34px;border-radius:26px;"
    "background:linear-gradient(145deg,rgba(255,255,255,.14),rgba(255,255,255,.075));"
    "border:1px solid rgba(255,255,255,.18);backdrop-filter:blur(22px);"
    "box-shadow:0 30px 90px rgba(0,0,0,.46),inset 0 1px 0 rgba(255,255,255,.14);"
    "animation:cardIn .65s ease-out;}"
    ".card:before{content:\"\";position:absolute;left:24px;right:24px;top:0;height:1px;"
    "background:linear-gradient(90deg,transparent,rgba(255,255,255,.72),transparent);}"

    ".eyebrow{font-size:11px;letter-spacing:2.2px;font-weight:bold;color:#9db8ff;margin-bottom:12px;}"
    "h1{font-size:38px;letter-spacing:-1px;margin-bottom:10px;text-shadow:0 2px 20px rgba(120,150,255,.22);}"
    "p{color:rgba(236,241,255,.74);margin-bottom:26px;line-height:1.65;}"
    "button{padding:13px 20px;border-radius:13px;border:1px solid rgba(255,255,255,.45);"
    "background:linear-gradient(180deg,#fff,#e8edff);color:#20265e;font-weight:bold;cursor:pointer;"
    "box-shadow:0 9px 28px rgba(85,105,255,.26);transition:transform .18s ease,box-shadow .18s ease;}"
    "button span{margin-left:7px;}"
    "button:hover{transform:translateY(-2px) scale(1.02);box-shadow:0 13px 34px rgba(100,119,255,.36);}"

    "@keyframes twinkleA{0%,100%{opacity:.42;}45%{opacity:1;}70%{opacity:.68;}}"
    "@keyframes twinkleB{0%,100%{opacity:.72;}35%{opacity:.28;}65%{opacity:.82;}}"
    "@keyframes twinkleC{0%,100%{opacity:.20;}50%{opacity:.58;}}"
    "@keyframes floatA{from{transform:translate(-12px,-8px) scale(1);}to{transform:translate(32px,22px) scale(1.08);}}"
    "@keyframes floatB{from{transform:translate(12px,8px) scale(1.02);}to{transform:translate(-28px,-18px) scale(1.10);}}"
    "@keyframes cardIn{from{opacity:0;transform:translateY(12px) scale(.985);}to{opacity:1;transform:translateY(0) scale(1);}}";

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
