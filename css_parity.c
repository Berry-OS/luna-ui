#include <math.h>
#include <stdio.h>
#include <string.h>

/* Reuse the shipped example verbatim so its CSS remains the regression
   fixture.  No window or GL context is needed for parsing/layout checks. */
#define LUNA_UI_NO_PLATFORM
#define main luna_example_program_main
#include "examples/example.c"
#undef main

int luna_app_run(const LunaAppConfig *cfg) {
    (void)cfg;
    return 0;
}

void luna_app_request_redraw(void) {
}

static int failures;
static int touch_clicks;

static void count_touch_click(LunaElement *element) {
    (void)element;
    touch_clicks++;
}

static void check_true(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        failures++;
    }
}

static void check_near(float actual, float expected, float tolerance,
                       const char *message) {
    if (fabsf(actual - expected) > tolerance) {
        fprintf(stderr, "FAIL: %s (got %.4f, expected %.4f ± %.4f)\n",
                message, actual, expected, tolerance);
        failures++;
    }
}

static LunaElement *find_class(const char *class_name) {
    for (int i = 0; i < elem_count; i++) {
        const char *p = elements[i].class_name;
        size_t wanted = strlen(class_name);
        while (*p) {
            while (*p == ' ') p++;
            const char *end = p;
            while (*end && *end != ' ') end++;
            if ((size_t)(end - p) == wanted && strncmp(p, class_name, wanted) == 0)
                return &elements[i];
            p = end;
        }
    }
    return NULL;
}

static void test_example_layout(void) {
    luna_window_width = 960.0f;
    luna_window_height = 640.0f;
    luna_reset_css();
    luna_parse_html(app_html);
    luna_parse_css(app_css);
    update_layout_pass();

    LunaElement *body = &elements[0];
    LunaElement *card = find_class("card");
    LunaElement *eyebrow = find_class("eyebrow");
    LunaElement *stars_a = find_class("stars-a");
    LunaElement *stars_b = find_class("stars-b");
    LunaElement *stars_c = find_class("stars-c");
    LunaElement *button = NULL;
    LunaElement *arrow = NULL;
    LunaElement *heading = NULL;
    LunaElement *paragraph = NULL;
    LunaElement *before = NULL;

    for (int i = 0; i < elem_count; i++) {
        if (strcmp(elements[i].type, "button") == 0) button = &elements[i];
        if (strcmp(elements[i].type, "span") == 0) arrow = &elements[i];
        if (strcmp(elements[i].type, "h1") == 0) heading = &elements[i];
        if (strcmp(elements[i].type, "p") == 0) paragraph = &elements[i];
        if (elements[i].generated_pseudo == 1) before = &elements[i];
    }

    check_true(card && eyebrow && heading && paragraph && button && arrow && before,
               "all example content and ::before are materialized");
    check_true(stars_a && stars_b && stars_c, "all star fields are present");
    if (!card || !eyebrow || !heading || !paragraph || !button || !arrow || !before ||
        !stars_a || !stars_b || !stars_c) return;

    check_near(body->w, 944.0f, 0.02f, "browser default body margins reduce width");
    check_near(card->w, 490.0f, 0.02f, "card content, padding, and border width");
    check_near(card->h, 266.66f, 0.05f, "card auto height matches collapsed block flow");

    check_near(heading->y - (eyebrow->y + eyebrow->h), 25.46f, 0.05f,
               "adjacent eyebrow/h1 margins collapse to the larger margin");
    check_near(paragraph->y - (heading->y + heading->h), 16.0f, 0.05f,
               "adjacent h1/p margins collapse");

    check_near(button->w, 115.0f, 0.05f, "button shrink-wraps direct text and child span");
    check_near(button->h, 44.0f, 0.05f, "button uses browser small-control line metrics");
    check_true(button->direct_text_before_children, "button preserves direct-text source order");
    check_true(button->has_inline_text_flow && button->inline_text_w > 58.0f,
               "button direct text participates in its inline row");
    check_true(arrow->x >= button->x + button->inline_text_x + button->inline_text_w + 6.9f,
               "span margin follows button text without overlap");

    check_near(before->w, 440.0f, 0.05f, "card ::before resolves left/right insets");
    check_near(before->h, 1.0f, 0.01f, "card ::before keeps its one-pixel height");

    check_true(stars_a->bg_layer_count == 10, "stars-a keeps all 10 backgrounds");
    check_true(stars_b->bg_layer_count == 6, "stars-b keeps all 6 backgrounds");
    check_true(stars_c->bg_layer_count == 4, "stars-c keeps all 4 backgrounds");
    check_true(stars_a->bg_layers[0].grad_rad_r < 0.0f,
               "unsized radial circle resolves as farthest-corner");
    check_true((stars_a->bg_layers[0].grad_stop_px_mask & (1u << 1)) != 0,
               "radial hard-stop pixel length is preserved");
}

static void test_gradient_geometry_and_color(void) {
    const float pi = 3.14159265358979323846f;
    check_near(gradient_stop_basis(GRAD_LINEAR, 0.0f, 0.0f, 1.0f,
                                   0.0f, 0.0f, 0.0f, 960.0f, 640.0f),
               640.0f, 0.01f, "0deg gradient line uses physical height");
    check_near(gradient_stop_basis(GRAD_LINEAR, pi * 0.5f, 1.0f, 0.0f,
                                   0.0f, 0.0f, 0.0f, 960.0f, 640.0f),
               960.0f, 0.01f, "90deg gradient line uses physical width");

    float radial = gradient_stop_basis(GRAD_RADIAL, 0.0f, 0.08f, 0.18f,
                                       -1.0f, 0.0f, 0.0f, 944.0f, 266.66f);
    float far_x = 944.0f * 0.92f;
    float far_y = 266.66f * 0.82f;
    check_near(radial, hypotf(far_x, far_y), 0.02f,
               "off-centre radial gradient reaches its farthest corner");

    float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f;
    parse_color("#1238", &r, &g, &b, &a);
    check_near(r, 17.0f / 255.0f, 0.0001f, "#RGBA red nibble expands");
    check_near(g, 34.0f / 255.0f, 0.0001f, "#RGBA green nibble expands");
    check_near(b, 51.0f / 255.0f, 0.0001f, "#RGBA blue nibble expands");
    check_near(a, 136.0f / 255.0f, 0.0001f, "#RGBA alpha nibble expands");
}

static void test_declaration_level_important(void) {
    luna_reset_css();
    luna_parse_html("<body><div id=\"cascade\" class=\"mixed\">x</div></body>");
    luna_parse_css(".mixed{color:#f00!important;width:10px}"
                   ".mixed{color:#00f;width:20px}");

    int idx = luna_get_element_by_id("cascade");
    check_true(idx >= 0, "cascade fixture is addressable");
    if (idx < 0) return;
    LunaElement *probe = &elements[idx];
    check_near(probe->css_width, 20.0f, 0.01f,
               "!important color does not incorrectly promote sibling width");
    check_near(probe->t_r, 1.0f, 0.001f, "important color wins later normal color");
    check_near(probe->t_b, 0.0f, 0.001f, "important red remains red");
}

static void test_live_custom_properties(void) {
    luna_reset_css();
    luna_parse_html("<body><div id=\"variable-box\">x</div></body>");
    luna_parse_css(":root{--box-width:10px;--box-color:#123456}"
                   "#variable-box{width:var(--box-width);background:var(--box-color)}");
    int idx = luna_get_element_by_id("variable-box");
    int rules_before = rule_count;
    check_true(idx >= 0, "custom-property fixture is addressable");
    if (idx < 0) return;
    check_near(elements[idx].css_width, 10.0f, 0.01f,
               "initial custom-property length resolves");
    LunaCssVariable vars[] = {
        {"--box-width", "37px"}, {"--box-color", "#abcdef"}
    };
    luna_css_set_variables(vars, 2);
    check_true(rule_count == rules_before,
               "custom-property update does not append or reparse rules");
    check_near(elements[idx].css_width, 37.0f, 0.01f,
               "custom-property length updates in place");
    check_near(elements[idx].r, 171.0f / 255.0f, 0.001f,
               "custom-property color updates in place");
}

static void test_dynamic_ancestor_selector(void) {
    luna_window_width = 320.0f;
    luna_window_height = 200.0f;
    luna_reset_css();
    luna_parse_html("<body><button id=\"tip-host\" class=\"tooltip\">Save"
                    "<span id=\"tip-text\" class=\"tooltip-text\">Help</span>"
                    "</button></body>");
    luna_parse_css("body{margin:0}.tooltip{position:absolute;left:20px;top:20px;"
                   "width:100px;height:44px}.tooltip-text{opacity:0;visibility:hidden}"
                   ".tooltip:hover .tooltip-text,.tooltip:focus .tooltip-text{"
                   "opacity:1;visibility:visible}");
    update_layout_pass();
    int host = luna_get_element_by_id("tip-host");
    int text = luna_get_element_by_id("tip-text");
    check_true(host >= 0 && text >= 0, "tooltip fixtures are addressable");
    if (host < 0 || text < 0) return;
    check_true(elements[text].visibility_hidden && elements[text].opacity == 0.0f,
               "tooltip starts hidden");

    recompute_hover(NULL, elements[host].x + 8.0, elements[host].y + 8.0);
    check_true(!elements[text].visibility_hidden && elements[text].opacity == 1.0f,
               "hovered ancestor reveals tooltip descendant");
    recompute_hover(NULL, 300.0, 180.0);
    check_true(elements[text].visibility_hidden && elements[text].opacity == 0.0f,
               "leaving ancestor hides tooltip descendant");

    luna_focus_element(host);
    check_true(!elements[text].visibility_hidden && elements[text].opacity == 1.0f,
               "focused ancestor reveals tooltip descendant");
    luna_focus_element(-1);
}

static void test_transform_none_reset(void) {
    luna_window_width = 320.0f;
    luna_window_height = 200.0f;
    luna_reset_css();
    luna_parse_html("<body><div id=\"dock\"></div></body>");
    luna_parse_css("body{margin:0}#dock{width:120px;height:40px;"
                   "transform:translateX(-50%) scale(1.25) rotate(12deg)}"
                   "#dock.layer{transform:none}");
    update_layout_pass();
    int dock = luna_get_element_by_id("dock");
    check_true(dock >= 0, "transform:none fixture is addressable");
    if (dock < 0) return;
    check_true(elements[dock].transform_tx < -59.0f,
               "base percentage translation is initially applied");
    luna_update_classes(dock, NULL, "layer");
    check_near(elements[dock].transform_tx, 0.0f, 0.001f,
               "transform:none resets translation");
    check_near(elements[dock].transform_ty, 0.0f, 0.001f,
               "transform:none resets vertical translation");
    check_near(elements[dock].transform_scale, 1.0f, 0.001f,
               "transform:none resets scale");
    check_near(elements[dock].transform_rotate, 0.0f, 0.001f,
               "transform:none resets rotation");
}

static void test_touch_input(void) {
    LunaTouchEvent event;
    LunaTouchEvent active;
    luna_window_width = 320.0f;
    luna_window_height = 240.0f;
    luna_reset_css();
    luna_parse_html("<body><button id=\"tap\">Tap</button>"
                    "<div id=\"scroller\"><div id=\"content\">content</div></div></body>");
    luna_parse_css("body{margin:0}#tap{width:100px;height:40px}"
                   "#scroller{width:120px;height:80px;overflow:auto}"
                   "#content{width:300px;height:240px}");
    update_layout_pass();
    int tap = luna_get_element_by_id("tap");
    int scroller = luna_get_element_by_id("scroller");
    check_true(tap >= 0 && scroller >= 0, "touch fixtures are addressable");
    if (tap < 0 || scroller < 0) return;
    luna_set_on_click(tap, count_touch_click);
    touch_clicks = 0;

    memset(&event, 0, sizeof(event));
    event.id = 41; event.phase = LUNA_TOUCH_DOWN;
    event.tool = LUNA_TOUCH_TOOL_FINGER;
    event.x = elements[tap].x + 20.0; event.y = elements[tap].y + 20.0;
    event.pressure = 0.75f;
    luna_touch(&event);
    check_true(luna_touch_count() == 1, "touch down tracks one active contact");
    check_true(luna_touch_get(0, &active) && active.id == 41 &&
               fabsf(active.pressure - 0.75f) < 0.001f,
               "active touch preserves id and pressure");
    event.phase = LUNA_TOUCH_UP; event.pressure = 0.0f;
    luna_touch(&event);
    check_true(touch_clicks == 1, "touch tap activates the hit control once");
    check_true(luna_touch_count() == 0, "touch up removes the active contact");

    event.id = 42; event.phase = LUNA_TOUCH_DOWN; event.pressure = 1.0f;
    event.x = elements[scroller].x + 30.0;
    event.y = elements[scroller].y + 60.0;
    luna_touch(&event);
    event.phase = LUNA_TOUCH_MOVE; event.y -= 35.0;
    luna_touch(&event);
    check_true(elements[scroller].scroll_top > 25.0f,
               "one-finger pan scrolls the touched overflow container");
    event.phase = LUNA_TOUCH_UP; event.pressure = 0.0f;
    luna_touch(&event);
    check_true(touch_clicks == 1, "a touch pan does not synthesize a click");
}

static void test_hover_preserves_inherited_icon_face(void) {
    luna_window_width = 640.0f;
    luna_window_height = 480.0f;
    luna_reset_css();
    luna_parse_html("<body><div class=\"stab\" id=\"tab\"><span class=\"stab_icon luna_icon\" id=\"icon\">&#xf013;</span> Applications</div></body>");
    luna_parse_css(".luna_icon{font-family:'Luna Symbols'}"
                   ".stab{display:flex;color:#aaa}.stab:hover{color:#fff}"
                   ".stab_icon{margin-right:8px;font-size:12px}");
    update_layout_pass();

    int tab = luna_get_element_by_id("tab");
    int icon = luna_get_element_by_id("icon");
    check_true(tab >= 0 && icon >= 0, "hover icon fixture is addressable");
    if (tab < 0 || icon < 0) return;
    check_true(elements[icon].font_face == 1,
               "icon family is selected before hover");
    check_true(elements[tab].has_inline_text_flow,
               "icon followed by direct label has anonymous flex placement");
    float label_x = elements[tab].inline_text_x;
    luna_mouse_move(elements[icon].x + 1.0, elements[icon].y + 1.0);
    check_true(elements[icon].font_face == 1,
               "hover descendant restyle preserves icon family");
    check_true(elements[tab].has_inline_text_flow,
               "paint-only hover preserves anonymous flex placement");
    check_near(elements[tab].inline_text_x, label_x, 0.001f,
               "paint-only hover keeps label after its icon");
}

static void test_wrapping_flex_overflow_scroll(void) {
    luna_window_width = 640.0f;
    luna_window_height = 480.0f;
    luna_reset_css();
    luna_parse_html("<body><div id=\"launchpad\"><input id=\"search\"><div id=\"grid\">"
                    "<div class=\"app\"></div><div class=\"app\"></div>"
                    "<div class=\"app\"></div><div class=\"app\"></div>"
                    "<div class=\"app\"></div><div class=\"app\"></div>"
                    "<div class=\"app\"></div><div class=\"app\"></div>"
                    "</div><div id=\"hint\">hint</div></div></body>");
    luna_parse_css("body{margin:0}#launchpad{position:absolute;inset:0;display:flex;"
                   "flex-direction:column;align-items:center;padding:30px 0 10px;"
                   "min-height:0;overflow:hidden}#search{width:180px;height:30px;flex:0 0 30px}"
                   "#grid{width:220px;flex:1 1 0;min-height:0;padding:10px;"
                   "display:flex;flex-direction:row;flex-wrap:wrap;align-content:flex-start;"
                   "gap:10px;overflow-y:auto}.app{width:100px;height:100px;flex:none}"
                   "#hint{height:20px;flex:0 0 20px}");
    update_layout_pass();

    int grid = luna_get_element_by_id("grid");
    check_true(grid >= 0, "wrapping overflow fixture is addressable");
    if (grid < 0) return;
    check_true(elements[grid].scroll_content_h >
               elements[grid].h - elements[grid].pad_t - elements[grid].pad_b,
               "wrapped flex lines contribute to vertical scroll extent");

    luna_mouse_move(elements[grid].x + 20.0, elements[grid].y + 20.0);
    luna_scroll(0.0, -1.0);
    check_true(elements[grid].scroll_dest_top > 0.0f,
               "mouse wheel scrolls a wrapping flex overflow container");
}

static void test_calc_viewport_length(void) {
    float value = 0.0f, offset = 0.0f;
    int percent = 0;
    check_true(parse_length_calc("calc(100vh - 36px)",
                                 &value, &percent, &offset),
               "calc viewport length is parsed");
    check_true(percent == 1, "calc vh length keeps a viewport ratio");
    check_near(value, 1.0f, 0.001f, "calc vh ratio is normalized");
    check_near(offset, -36.0f, 0.001f, "calc vh pixel offset is retained");
}

int main(void) {
    test_example_layout();
    test_gradient_geometry_and_color();
    test_declaration_level_important();
    test_live_custom_properties();
    test_dynamic_ancestor_selector();
    test_transform_none_reset();
    test_touch_input();
    test_hover_preserves_inherited_icon_face();
    test_wrapping_flex_overflow_scroll();
    test_calc_viewport_length();
    if (failures) {
        fprintf(stderr, "%d CSS parity check(s) failed\n", failures);
        return 1;
    }
    puts("CSS parity checks passed");
    return 0;
}
