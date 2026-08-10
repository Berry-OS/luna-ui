#include <math.h>
#include <stdio.h>
#include <string.h>

/* Reuse the shipped example verbatim so its CSS remains the regression
   fixture.  No window or GL context is needed for parsing/layout checks. */
#define LUNA_UI_NO_PLATFORM
#define main luna_example_program_main
#include "../examples/example.c"
#undef main

int luna_app_run(const LunaAppConfig *cfg) {
    (void)cfg;
    return 0;
}

static int failures;

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

int main(void) {
    test_example_layout();
    test_gradient_geometry_and_color();
    test_declaration_level_important();
    if (failures) {
        fprintf(stderr, "%d CSS parity check(s) failed\n", failures);
        return 1;
    }
    puts("CSS parity checks passed");
    return 0;
}
