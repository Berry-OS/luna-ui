/* tooltip.c — CSS tooltip demo for Luna UI (hover + keyboard focus) */
#define LUNA_UI_IMPLEMENTATION
#include "luna-ui.h"

static const char* TOOLTIP_HTML =
    "<!doctype html><html><body>"
    "<main class=\"panel\">"
    "<h1>Tooltip</h1>"
    "<p class=\"intro\">Point at a control, or press Tab to focus it.</p>"
    "<div class=\"actions\">"
    "<button class=\"tooltip\" tabindex=\"0\">Save"
    "<span class=\"tooltip-text\">Save the current document</span></button>"
    "<button class=\"tooltip secondary\" tabindex=\"0\">Share"
    "<span class=\"tooltip-text below\">Copy a shareable link</span></button>"
    "<button class=\"tooltip danger\" tabindex=\"0\">Delete"
    "<span class=\"tooltip-text\">Move this item to Trash</span></button>"
    "</div>"
    "<p class=\"note\">The bubble is ordinary nested HTML. CSS controls visibility.</p>"
    "</main></body></html>";

static const char* TOOLTIP_CSS =
    "/* Hallmark · component: tooltip · genre: modern-minimal · theme: existing Luna UI\n"
    " * states: default · hover · focus · active · disabled · loading · error · success\n"
    " * contrast: pass\n"
    " * pre-emit critique: P5 H4 E4 S5 R5 V4\n"
    " */\n"
    ":root {"
    "--paper:#f5f7fb;--surface:#ffffff;--ink:#111827;--muted:#526071;"
    "--rule:#dce2ea;--rule-strong:#aab4c2;"
    "--accent:#2457d6;--accent-ink:#ffffff;--danger:#b42318;"
    "--tip:#172033;--tip-ink:#ffffff;--focus:#0047bb;"
    "--font-body:Manrope;"
    "--space-sm:8px;--space-md:16px;--space-lg:32px;"
    "--radius-sm:6px;--radius-md:12px;--dur-fast:0.16s;}"
    "html,body{margin:0;width:100%;height:100%;overflow-x:clip;}"
    "body{display:flex;align-items:center;justify-content:center;background:var(--paper);color:var(--ink);font-family:var(--font-body);}"
    ".panel{width:94%;max-width:620px;height:340px;padding:16px;box-sizing:border-box;background:var(--surface);border:1px solid var(--rule);border-radius:12px;}"
    "h1{margin:0;height:40px;font-size:28px;font-weight:bold;}"
    ".intro,.note{height:28px;color:var(--muted);font-size:14px;}"
    ".actions{display:flex;align-items:center;gap:8px;height:96px;}"
    ".tooltip{position:relative;width:80px;height:44px;white-space:nowrap;border:1px solid var(--accent);border-radius:6px;background:var(--accent);color:var(--accent-ink);font-size:14px;font-weight:bold;cursor:pointer;outline-width:2px;outline-color:var(--focus);outline-offset:2px;}"
    ".tooltip.secondary{background:var(--surface);color:var(--ink);border-color:var(--rule-strong);}"
    ".tooltip.danger{background:var(--danger);border-color:var(--danger);color:var(--accent-ink);}"
    ".tooltip-text{position:absolute;left:50%;bottom:56px;width:210px;height:34px;box-sizing:border-box;padding:8px;border-radius:6px;background:var(--tip);color:var(--tip-ink);font-size:12px;font-weight:normal;text-align:center;white-space:nowrap;opacity:0;visibility:hidden;transform:translateX(-50%);transition:opacity 0.16s;pointer-events:none;z-index:50;}"
    ".tooltip-text.below{top:56px;bottom:auto;}"
    ".tooltip-text::before{content:'';position:absolute;left:50%;bottom:-6px;width:12px;height:12px;background:var(--tip);transform:translateX(-50%) rotate(45deg);z-index:-1;}"
    ".tooltip-text.below::before{top:-6px;bottom:auto;}"
    "@keyframes tooltip-in{from{opacity:0}to{opacity:1}}"
    ".tooltip:hover .tooltip-text{visibility:visible;animation:tooltip-in 0.16s forwards;animation-delay:0.8s;}"
    ".tooltip:focus .tooltip-text,.tooltip:focus-visible .tooltip-text{opacity:1;visibility:visible;animation:tooltip-in 0.16s forwards;animation-delay:0s;}"
    ".tooltip:hover,.tooltip.is-hover{filter:brightness(1.08);}"
    ".tooltip:active,.tooltip.is-active{transform:translateY(1px);}"
    ".tooltip:disabled,.tooltip.is-disabled{opacity:.5;cursor:not-allowed;}"
    ".tooltip.is-loading{opacity:.72;}"
    ".tooltip.is-error{border-color:var(--danger);}"
    ".tooltip.is-success{border-color:var(--accent);}"
    ".note{margin-top:12px;}"
    "@media(min-width:500px){.panel{height:300px;padding:32px}.actions{gap:16px}.tooltip{width:132px}}"
    "@media(prefers-reduced-motion:reduce){.tooltip-text{transition:opacity 0.01s;}}";

int main(void) {
    LunaAppConfig app = {0};
    app.title = "Luna UI Tooltip";
    app.width = 760;
    app.height = 460;
    app.resizable = 1;
    app.vsync = 1;
    app.html = TOOLTIP_HTML;
    app.css = TOOLTIP_CSS;
    return luna_app_run(&app);
}
