/*
 * Copyright © 2026 Yuichiro Nakada / Project Vespera
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <GLFW/glfw3.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

typedef void (*PFNGLCREATEVERTEXARRAYSPROC)(GLsizei n, GLuint* arrays);
typedef void (*PFNGLDELETEVERTEXARRAYSPROC)(GLsizei n, const GLuint* arrays);
typedef void (*PFNGLGENBUFFERSPROC)(GLsizei n, GLuint* buffers);
typedef void (*PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void (*PFNGLBUFFERDATAPROC)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
typedef void (*PFNGLBUFFERSUBDATAPROC)(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
typedef void (*PFNGLVERTEXATTRIBPOINTERPROC)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
typedef void (*PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef void (*PFNGLDELETEBUFFERSPROC)(GLsizei n, const GLuint* buffers);
typedef void (*PFNGLBINDVERTEXARRAYPROC)(GLuint array);
typedef GLuint (*PFNGLCREATESHADERPROC)(GLenum type);
typedef void (*PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length);
typedef void (*PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef GLuint (*PFNGLCREATEPROGRAMPROC)(void);
typedef void (*PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void (*PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void (*PFNGLUSEPROGRAMPROC)(GLuint program);
typedef GLint (*PFNGLGETUNIFORMLOCATIONPROC)(GLuint program, const GLchar* name);
typedef void (*PFNGLUNIFORM4FPROC)(GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3);
typedef void (*PFNGLUNIFORM2FPROC)(GLint location, GLfloat v0, GLfloat v1);
typedef void (*PFNGLUNIFORM1FPROC)(GLint location, GLfloat v0);
typedef void (*PFNGLUNIFORM1IPROC)(GLint location, GLint v0);
typedef void (*PFNGLACTIVETEXTUREPROC)(GLenum texture);

PFNGLCREATEVERTEXARRAYSPROC glCreateVertexArrays;
PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
PFNGLGENBUFFERSPROC glGenBuffers;
PFNGLBINDBUFFERPROC glBindBuffer;
PFNGLBUFFERDATAPROC glBufferData;
PFNGLBUFFERSUBDATAPROC glBufferSubData;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;
PFNGLDELETEBUFFERSPROC glDeleteBuffers;
PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
PFNGLCREATESHADERPROC glCreateShader;
PFNGLSHADERSOURCEPROC glShaderSource;
PFNGLCOMPILESHADERPROC glCompileShader;
PFNGLCREATEPROGRAMPROC glCreateProgram;
PFNGLATTACHSHADERPROC glAttachShader;
PFNGLLINKPROGRAMPROC glLinkProgram;
PFNGLUSEPROGRAMPROC glUseProgram;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
PFNGLUNIFORM4FPROC glUniform4f;
PFNGLUNIFORM2FPROC glUniform2f;
PFNGLUNIFORM1FPROC glUniform1f;
PFNGLUNIFORM1IPROC glUniform1i_;
PFNGLACTIVETEXTUREPROC glActiveTexture_;

// --- Shaders ---

const char* bg_vs =
    "#version 330 core\n"
    "layout (location = 0) in vec2 aPos;\n"
    "out vec2 FragPos;\n"
    "uniform vec2 uResolution;\n"
    "uniform vec2 uPos;\n"
    "uniform vec2 uSize;\n"
    "void main() {\n"
    "    FragPos = vec2(aPos.x, 1.0 - aPos.y) * uSize;\n"
    "    vec2 screenPos = uPos + vec2(aPos.x * uSize.x, aPos.y * uSize.y);\n"
    "    vec2 ndc = (screenPos / uResolution) * 2.0 - 1.0;\n"
    "    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);\n"
    "}\0";

// Supports solid color and linear-gradient (uGradient=1).
// CSS angle convention: 0deg=to top, 90deg=to right, 180deg=to bottom.
const char* bg_fs =
    "#version 330 core\n"
    "in vec2 FragPos;\n"
    "out vec4 FragColor;\n"
    "uniform vec4 uColor;\n"
    "uniform vec4 uBorderColor;\n"
    "uniform float uBorderWidth;\n"
    "uniform vec2 uSize;\n"
    "uniform float uRadius;\n"
    "uniform int uGradient;\n"
    "uniform vec4 uGradColor1;\n"
    "uniform vec4 uGradColor2;\n"
    "uniform float uGradAngle;\n"
    "void main() {\n"
    "    vec2 halfSize = uSize / 2.0;\n"
    "    float r = max(uRadius, 0.001);\n"
    "    vec2 d = abs(FragPos - halfSize) - halfSize + vec2(r);\n"
    "    float dist = length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);\n"
    "    float alpha = 1.0 - smoothstep(r - 1.0, r + 0.5, dist);\n"
    "    if(alpha <= 0.0) discard;\n"
    "    vec4 baseColor;\n"
    "    if(uGradient == 1) {\n"
    "        float ca = cos(uGradAngle); float sa = sin(uGradAngle);\n"
    "        vec2 uv = FragPos / uSize;\n"
    "        float t = clamp(dot(uv - 0.5, vec2(sa, -ca)) + 0.5, 0.0, 1.0);\n"
    "        baseColor = mix(uGradColor1, uGradColor2, t);\n"
    "    } else {\n"
    "        baseColor = uColor;\n"
    "    }\n"
    "    float bw = uBorderWidth;\n"
    "    float borderMix = (bw > 0.01) ? smoothstep(r - bw - 1.0, r - bw + 0.5, dist) : 0.0;\n"
    "    vec4 finalColor = mix(baseColor, uBorderColor, borderMix);\n"
    "    FragColor = vec4(finalColor.rgb, finalColor.a * alpha);\n"
    "}\0";

const char* text_vs =
    "#version 330 core\n"
    "layout (location = 0) in vec4 vertex;\n"
    "out vec2 TexCoords;\n"
    "uniform vec2 uResolution;\n"
    "void main() {\n"
    "    vec2 ndc = (vertex.xy / uResolution) * 2.0 - 1.0;\n"
    "    gl_Position = vec4(ndc.x, -ndc.y, 0.0, 1.0);\n"
    "    TexCoords = vertex.zw;\n"
    "}\0";

const char* text_fs =
    "#version 330 core\n"
    "in vec2 TexCoords;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D text;\n"
    "uniform vec4 textColor;\n"
    "void main() {\n"
    "    float sampled = texture(text, TexCoords).r;\n"
    "    FragColor = vec4(textColor.rgb, textColor.a * sampled);\n"
    "}\0";

// --- Element ---

struct Element;
typedef void (*EventHandler)(struct Element* e);

typedef struct Element {
    int id_idx;
    int parent_idx;
    float rel_x, rel_y;
    float x, y, w, h;
    char text[256], type[32], class_name[96], id[64];
    int is_hovered, is_active, is_draggable;

    float r, g, b, a;
    float t_r, t_g, t_b, t_a;
    float bd_r, bd_g, bd_b, bd_a;

    float cur_r, cur_g, cur_b, cur_a;
    float cur_bd_r, cur_bd_g, cur_bd_b, cur_bd_a;
    float cur_scale;

    float border_radius;
    float border_width;
    float padding;

    float opacity;
    int display_none;
    int cursor_pointer;
    int text_align;
    int font_size;
    int font_bold;

    int has_shadow;
    float sh_dx, sh_dy, sh_blur;
    float sh_r, sh_g, sh_b, sh_a;

    // Linear gradient
    int has_gradient;
    float grad_r1, grad_g1, grad_b1, grad_a1;
    float grad_r2, grad_g2, grad_b2, grad_a2;
    float grad_angle; // radians (CSS convention)

    int z_index;

    int has_custom_bg, has_custom_color, has_custom_text, has_custom_border;
    EventHandler on_click;
} Element;

// --- CSS Rule ---

#define MAX_SEL_CLASSES   4
#define MAX_SEL_ANCESTORS 3

typedef struct {
    char sel_type[32];
    char sel_id[64];
    char sel_classes[MAX_SEL_CLASSES][64];
    int  sel_class_count;
} SimpleSelector;

typedef struct {
    char selector[128];
    SimpleSelector target;
    SimpleSelector ancestors[MAX_SEL_ANCESTORS];
    int ancestor_count;
    int specificity, is_hover, is_active;

    int has_bg;     float bg_r, bg_g, bg_b, bg_a;
    int has_color;  float c_r,  c_g,  c_b,  c_a;
    int has_border; float bd_r, bd_g, bd_b, bd_a; float border_width;
    int has_radius; float border_radius;
    int has_width;  float width;
    int has_height; float height;
    int has_padding; float padding;
    int has_left;   float left;
    int has_top;    float top;

    int has_opacity; float opacity;
    int has_cursor;  int cursor_pointer;
    int has_display; int display_none;
    int has_text_align; int text_align;
    int has_font_size;  int font_size;
    int has_font_weight; int font_bold;
    int has_shadow; float sh_dx, sh_dy, sh_blur, sh_r, sh_g, sh_b, sh_a;

    // Linear gradient
    int has_gradient;
    float grad_r1, grad_g1, grad_b1, grad_a1;
    float grad_r2, grad_g2, grad_b2, grad_a2;
    float grad_angle;

    int has_z_index; int z_index;
} CSSRule;

#define MAX_ELEMENTS 500
#define MAX_RULES    600

Element  elements[MAX_ELEMENTS]; int elem_count = 0;
CSSRule  css_rules[MAX_RULES];   int rule_count = 0;

static int render_order[MAX_ELEMENTS];

GLuint bg_program, text_program;
float  window_width = 1024.0f, window_height = 768.0f;
GLFWwindow* g_window = NULL;
static int g_desktop_mode = 0;
static int g_fullscreen   = 0;
static const char* g_layout_path = "layout.html";
static const char* g_css_path    = "style.css";
int   drag_target_idx = -1;
float drag_offset_x   = 0, drag_offset_y = 0;

GLFWcursor* g_hand_cursor  = NULL;
int         g_cursor_is_hand = 0;

GLuint text_vao, text_vbo;
int font_loaded      = 0;
int bold_font_loaded = 0;

// Shared VAO/VBO for rectangle drawing — created once, reused every frame.
static GLuint g_rect_vao = 0, g_rect_vbo = 0;

#define NUM_FONT_SIZES 4
static const float font_sizes[NUM_FONT_SIZES] = { 12.0f, 16.0f, 22.0f, 32.0f };

typedef struct {
    stbtt_bakedchar cdata[96];
    GLuint tex;
    int    loaded;
} FontAtlas;

FontAtlas font_regular[NUM_FONT_SIZES];
FontAtlas font_bold_atlas[NUM_FONT_SIZES];

int g_progress_fill_idx = -1;
int g_toast_idx         = -1;
int g_modal_overlay_idx = -1;

// ============================================================
// Utilities
// ============================================================

char* read_file(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) return NULL;
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    char* buffer = (char*)malloc(length + 1);
    if (buffer) { fread(buffer, 1, length, file); buffer[length] = '\0'; }
    fclose(file);
    return buffer;
}

unsigned char* read_file_bytes(const char* filename, long* out_size) {
    FILE* file = fopen(filename, "rb");
    if (!file) return NULL;
    fseek(file, 0, SEEK_END);
    long length = ftell(file);
    fseek(file, 0, SEEK_SET);
    unsigned char* buffer = (unsigned char*)malloc(length);
    if (buffer) fread(buffer, 1, length, file);
    fclose(file);
    if (out_size) *out_size = length;
    return buffer;
}

void trim_whitespace(char* str) {
    if (!str) return;
    char* start = str;
    while (isspace((unsigned char)*start)) start++;
    memmove(str, start, strlen(start) + 1);
    if (!*str) return;
    char* end = str + strlen(str) - 1;
    while (end >= str && isspace((unsigned char)*end)) { *end = '\0'; end--; }
}

// ============================================================
// Color parsing
// ============================================================

typedef struct { const char* name; float r, g, b, a; } NamedColor;
static const NamedColor named_colors[] = {
    {"transparent",  0.00f, 0.00f, 0.00f, 0.00f},
    {"white",        1.00f, 1.00f, 1.00f, 1.00f},
    {"black",        0.00f, 0.00f, 0.00f, 1.00f},
    {"red",          1.00f, 0.00f, 0.00f, 1.00f},
    {"green",        0.00f, 0.50f, 0.00f, 1.00f},
    {"blue",         0.00f, 0.00f, 1.00f, 1.00f},
    {"gray",         0.50f, 0.50f, 0.50f, 1.00f},
    {"grey",         0.50f, 0.50f, 0.50f, 1.00f},
    {"lightgray",    0.83f, 0.83f, 0.83f, 1.00f},
    {"lightgrey",    0.83f, 0.83f, 0.83f, 1.00f},
    {"darkgray",     0.41f, 0.41f, 0.41f, 1.00f},
    {"darkgrey",     0.41f, 0.41f, 0.41f, 1.00f},
    {"orange",       1.00f, 0.65f, 0.00f, 1.00f},
    {"yellow",       1.00f, 1.00f, 0.00f, 1.00f},
    {"purple",       0.50f, 0.00f, 0.50f, 1.00f},
    {"violet",       0.56f, 0.00f, 1.00f, 1.00f},
    {"pink",         1.00f, 0.75f, 0.80f, 1.00f},
    {"cyan",         0.00f, 1.00f, 1.00f, 1.00f},
    {"indigo",       0.29f, 0.00f, 0.51f, 1.00f},
    {"teal",         0.00f, 0.50f, 0.50f, 1.00f},
    {"silver",       0.75f, 0.75f, 0.75f, 1.00f},
    {"coral",        1.00f, 0.50f, 0.31f, 1.00f},
    {"gold",         1.00f, 0.84f, 0.00f, 1.00f},
    {"crimson",      0.86f, 0.08f, 0.24f, 1.00f},
    {"navy",         0.00f, 0.00f, 0.50f, 1.00f},
    {"skyblue",      0.53f, 0.81f, 0.92f, 1.00f},
};

void parse_color(const char* val, float* r, float* g, float* b, float* a) {
    if (!val) return;
    while (isspace((unsigned char)*val)) val++;
    *a = 1.0f;

    if (val[0] == '#') {
        int len = (int)strlen(val);
        if (len == 4) {
            int rv, gv, bv;
            if (sscanf(val, "#%1x%1x%1x", &rv, &gv, &bv) == 3) {
                *r = (rv * 17) / 255.0f; *g = (gv * 17) / 255.0f; *b = (bv * 17) / 255.0f;
            }
        } else if (len == 7) {
            unsigned int rv, gv, bv;
            if (sscanf(val, "#%2x%2x%2x", &rv, &gv, &bv) == 3) {
                *r = rv / 255.0f; *g = gv / 255.0f; *b = bv / 255.0f;
            }
        } else if (len >= 9) {
            unsigned int rv, gv, bv, av;
            if (sscanf(val, "#%2x%2x%2x%2x", &rv, &gv, &bv, &av) == 4) {
                *r = rv / 255.0f; *g = gv / 255.0f; *b = bv / 255.0f; *a = av / 255.0f;
            }
        }
    } else if (strncmp(val, "rgba(", 5) == 0 || strncmp(val, "rgb(", 4) == 0) {
        const char* paren = strchr(val, '(');
        if (paren) {
            char buf[64] = {0};
            strncpy(buf, paren + 1, sizeof(buf) - 1);
            float vals[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            int idx = 0;
            char* tok = strtok(buf, ", )");
            while (tok && idx < 4) { vals[idx++] = (float)atof(tok); tok = strtok(NULL, ", )"); }
            if (idx >= 3) {
                *r = vals[0] / 255.0f; *g = vals[1] / 255.0f; *b = vals[2] / 255.0f;
                *a = (idx == 4) ? vals[3] : 1.0f;
            }
        }
    } else {
        size_t nc = sizeof(named_colors) / sizeof(named_colors[0]);
        for (size_t i = 0; i < nc; i++) {
            if (strcmp(val, named_colors[i].name) == 0) {
                *r = named_colors[i].r; *g = named_colors[i].g;
                *b = named_colors[i].b; *a = named_colors[i].a;
                break;
            }
        }
    }
}

// ============================================================
// CSS value helpers
// ============================================================

// Parse a CSS numeric value, ignoring px/em/rem/% suffixes.
static float parse_float_val(const char* v) {
    float f = 0.0f;
    sscanf(v, "%f", &f);
    return f;
}

// Parse box-shadow: dx dy blur color
// Uses strtof so trailing color string (including rgba with spaces) is handled.
static void parse_box_shadow(const char* val, CSSRule* rule) {
    const char* p = val;
    char* endp;
    float dx   = strtof(p, &endp); p = endp; while (isspace((unsigned char)*p)) p++;
    float dy   = strtof(p, &endp); p = endp; while (isspace((unsigned char)*p)) p++;
    float blur = strtof(p, &endp); p = endp; while (isspace((unsigned char)*p)) p++;
    if (*p) {
        float cr = 0, cg = 0, cb = 0, ca = 1;
        parse_color(p, &cr, &cg, &cb, &ca);
        rule->has_shadow = 1;
        rule->sh_dx = dx; rule->sh_dy = dy; rule->sh_blur = blur;
        rule->sh_r = cr; rule->sh_g = cg; rule->sh_b = cb; rule->sh_a = ca;
    }
}

// Parse border shorthand: "1px solid #color" or "none" or "0"
static void parse_border_shorthand(const char* val, CSSRule* rule) {
    rule->has_border = 1;
    if (strcmp(val, "none") == 0 || strcmp(val, "0") == 0) {
        rule->border_width = 0; rule->bd_a = 0; return;
    }
    char buf[256]; strncpy(buf, val, 255); buf[255] = 0;
    char* p = buf;
    // width
    char* endp;
    float bw = strtof(p, &endp);
    if (endp != p) { rule->border_width = bw; p = endp; }
    // skip "px" or other unit
    while (*p && !isspace((unsigned char)*p) && *p != '#' && *p != 'r') p++;
    while (isspace((unsigned char)*p)) p++;
    // style keyword (solid/dashed/dotted/none)
    if (strncmp(p, "none", 4) == 0) { rule->border_width = 0; return; }
    if (strncmp(p, "solid", 5) == 0 || strncmp(p, "dashed", 6) == 0 || strncmp(p, "dotted", 6) == 0) {
        while (*p && !isspace((unsigned char)*p)) p++;
        while (isspace((unsigned char)*p)) p++;
    }
    // color
    if (*p) parse_color(p, &rule->bd_r, &rule->bd_g, &rule->bd_b, &rule->bd_a);
}

// Parse linear-gradient(angle, color1, color2)
static void parse_linear_gradient(const char* val, CSSRule* rule) {
    const char* p = strchr(val, '(');
    if (!p) return;
    p++;
    while (isspace((unsigned char)*p)) p++;

    float angle = 180.0f; // default: to bottom
    if (strncmp(p, "to ", 3) == 0) {
        if (strstr(p, "top"))    angle = 0.0f;
        else if (strstr(p, "right"))  angle = 90.0f;
        else if (strstr(p, "bottom")) angle = 180.0f;
        else if (strstr(p, "left"))   angle = 270.0f;
        const char* comma = strchr(p, ',');
        if (!comma) return;
        p = comma + 1;
    } else if (isdigit((unsigned char)*p) || *p == '-' || *p == '.') {
        char* endp;
        angle = strtof(p, &endp); p = endp;
        if (strncmp(p, "deg", 3) == 0) p += 3;
        while (isspace((unsigned char)*p)) p++;
        if (*p == ',') p++;
    }
    while (isspace((unsigned char)*p)) p++;

    // color1: read until top-level comma
    const char* start1 = p;
    int depth = 0;
    while (*p && !(*p == ',' && depth == 0)) {
        if (*p == '(') depth++;
        else if (*p == ')') { if (depth == 0) break; depth--; }
        p++;
    }
    char col1[64] = {0};
    int len1 = (int)(p - start1); if (len1 > 63) len1 = 63;
    strncpy(col1, start1, len1); trim_whitespace(col1);

    float r1=0, g1=0, b1=0, a1=1;
    parse_color(col1, &r1, &g1, &b1, &a1);

    if (*p == ',') p++;
    while (isspace((unsigned char)*p)) p++;

    // color2: until top-level ')' or end
    const char* start2 = p;
    depth = 0;
    while (*p && !(*p == ')' && depth == 0)) {
        if (*p == '(') depth++;
        else if (*p == ')') depth--;
        p++;
    }
    char col2[64] = {0};
    int len2 = (int)(p - start2); if (len2 > 63) len2 = 63;
    strncpy(col2, start2, len2); trim_whitespace(col2);

    float r2=0, g2=0, b2=0, a2=1;
    parse_color(col2, &r2, &g2, &b2, &a2);

    rule->has_gradient = 1;
    rule->grad_r1 = r1; rule->grad_g1 = g1; rule->grad_b1 = b1; rule->grad_a1 = a1;
    rule->grad_r2 = r2; rule->grad_g2 = g2; rule->grad_b2 = b2; rule->grad_a2 = a2;
    rule->grad_angle = angle * (float)M_PI / 180.0f;
    // Fallback solid color = first gradient color
    rule->has_bg = 1;
    rule->bg_r = r1; rule->bg_g = g1; rule->bg_b = b1; rule->bg_a = a1;
}

// Parse background shorthand: color or linear-gradient(...)
static void parse_background_shorthand(const char* val, CSSRule* rule) {
    if (strncmp(val, "linear-gradient", 15) == 0) {
        parse_linear_gradient(val, rule);
    } else {
        rule->has_bg = 1;
        parse_color(val, &rule->bg_r, &rule->bg_g, &rule->bg_b, &rule->bg_a);
    }
}

// ============================================================
// Element helpers
// ============================================================

int get_element_by_id(const char* id) {
    for (int i = 0; i < elem_count; i++)
        if (strcmp(elements[i].id, id) == 0) return i;
    return -1;
}

void set_text(int idx, const char* new_text) {
    if (idx >= 0 && idx < elem_count) {
        strncpy(elements[idx].text, new_text, 255);
        elements[idx].has_custom_text = 1;
    }
}

void set_on_click(int idx, EventHandler cb) {
    if (idx >= 0 && idx < elem_count) elements[idx].on_click = cb;
}

int element_has_class(Element* e, const char* cls) {
    char buf[96]; strncpy(buf, e->class_name, sizeof(buf) - 1); buf[sizeof(buf)-1] = 0;
    char* tok = strtok(buf, " ");
    while (tok) { if (strcmp(tok, cls) == 0) return 1; tok = strtok(NULL, " "); }
    return 0;
}

void add_class(Element* e, const char* cls) {
    if (element_has_class(e, cls)) return;
    if (strlen(e->class_name) + strlen(cls) + 2 < sizeof(e->class_name)) {
        if (strlen(e->class_name) > 0) strcat(e->class_name, " ");
        strcat(e->class_name, cls);
    }
}

void remove_class(Element* e, const char* cls) {
    char buf[96] = {0}, result[96] = {0};
    strncpy(buf, e->class_name, sizeof(buf) - 1);
    char* tok = strtok(buf, " ");
    while (tok) {
        if (strcmp(tok, cls) != 0) {
            if (strlen(result) > 0) strcat(result, " ");
            strcat(result, tok);
        }
        tok = strtok(NULL, " ");
    }
    strncpy(e->class_name, result, sizeof(e->class_name) - 1);
}

void set_bg(int idx, float r, float g, float b, float a) {
    if (idx < 0 || idx >= elem_count) return;
    elements[idx].r = r; elements[idx].g = g;
    elements[idx].b = b; elements[idx].a = a;
    elements[idx].has_custom_bg = 1;
}

int is_visible(int idx) {
    while (idx != -1) {
        if (elements[idx].display_none) return 0;
        idx = elements[idx].parent_idx;
    }
    return 1;
}

// ============================================================
// CSS selector parsing
// ============================================================

void parse_simple_selector(const char* sel_in, SimpleSelector* out) {
    memset(out, 0, sizeof(SimpleSelector));
    const char* p = sel_in;
    while (*p) {
        if (*p == '.' || *p == '#') {
            char kind = *p; p++;
            char token[64] = {0}; int ti = 0;
            while (*p && *p != '.' && *p != '#' && ti < 63) token[ti++] = *p++;
            token[ti] = 0;
            if (kind == '.') {
                if (out->sel_class_count < MAX_SEL_CLASSES)
                    strncpy(out->sel_classes[out->sel_class_count++], token, 63);
            } else {
                strncpy(out->sel_id, token, 63);
            }
        } else {
            char token[32] = {0}; int ti = 0;
            while (*p && *p != '.' && *p != '#' && ti < 31) token[ti++] = *p++;
            token[ti] = 0;
            if (strcmp(token, "*") != 0 && strlen(token) > 0)
                strncpy(out->sel_type, token, 31);
        }
    }
}

void parse_selector(const char* sel_in, CSSRule* rule) {
    char sel[128]; strncpy(sel, sel_in, sizeof(sel) - 1); sel[sizeof(sel)-1] = 0;
    rule->ancestor_count = 0;

    char* compounds[MAX_SEL_ANCESTORS + 1];
    int compound_count = 0;
    char* tok = strtok(sel, " \t");
    while (tok && compound_count < MAX_SEL_ANCESTORS + 1) {
        compounds[compound_count++] = tok;
        tok = strtok(NULL, " \t");
    }
    if (compound_count == 0) { memset(&rule->target, 0, sizeof(SimpleSelector)); return; }

    parse_simple_selector(compounds[compound_count - 1], &rule->target);
    for (int i = 0; i < compound_count - 1; i++)
        parse_simple_selector(compounds[i], &rule->ancestors[rule->ancestor_count++]);
}

int simple_selector_matches(SimpleSelector* s, Element* e) {
    if (s->sel_type[0] && strcmp(s->sel_type, e->type) != 0) return 0;
    if (s->sel_id[0]   && strcmp(s->sel_id,   e->id)   != 0) return 0;
    for (int i = 0; i < s->sel_class_count; i++)
        if (!element_has_class(e, s->sel_classes[i])) return 0;
    if (!s->sel_type[0] && !s->sel_id[0] && s->sel_class_count == 0) return 0;
    return 1;
}

int selector_matches(CSSRule* r, Element* e) {
    if (!simple_selector_matches(&r->target, e)) return 0;
    int search_from = e->parent_idx;
    for (int a = 0; a < r->ancestor_count; a++) {
        int found = -1;
        for (int p = search_from; p != -1; p = elements[p].parent_idx) {
            if (simple_selector_matches(&r->ancestors[a], &elements[p])) { found = p; break; }
        }
        if (found == -1) return 0;
        search_from = elements[found].parent_idx;
    }
    return 1;
}

// ============================================================
// CSS declaration parsing
// ============================================================

void parse_declarations(char* declarations, CSSRule* rule) {
    char* prop = declarations;
    while (prop && *prop) {
        char* semi  = strchr(prop, ';');
        if (semi) *semi = '\0';
        char* colon = strchr(prop, ':');
        if (colon) {
            *colon = '\0';
            char* key = prop; char* val = colon + 1;
            trim_whitespace(key); trim_whitespace(val);

            if      (strcmp(key, "background-color") == 0) { rule->has_bg = 1; parse_color(val, &rule->bg_r, &rule->bg_g, &rule->bg_b, &rule->bg_a); }
            else if (strcmp(key, "background") == 0)       { parse_background_shorthand(val, rule); }
            else if (strcmp(key, "color") == 0)            { rule->has_color = 1; parse_color(val, &rule->c_r, &rule->c_g, &rule->c_b, &rule->c_a); }
            else if (strcmp(key, "border-radius") == 0)    { rule->has_radius = 1; rule->border_radius = parse_float_val(val); }
            else if (strcmp(key, "border-width") == 0)     { rule->has_border = 1; rule->border_width = parse_float_val(val); }
            else if (strcmp(key, "border-color") == 0)     { rule->has_border = 1; parse_color(val, &rule->bd_r, &rule->bd_g, &rule->bd_b, &rule->bd_a); }
            else if (strcmp(key, "border") == 0)           { parse_border_shorthand(val, rule); }
            else if (strcmp(key, "width") == 0)            { rule->has_width = 1; rule->width = parse_float_val(val); }
            else if (strcmp(key, "height") == 0)           { rule->has_height = 1; rule->height = parse_float_val(val); }
            else if (strcmp(key, "padding") == 0)          { rule->has_padding = 1; rule->padding = parse_float_val(val); }
            else if (strcmp(key, "left") == 0)             { rule->has_left = 1; rule->left = parse_float_val(val); }
            else if (strcmp(key, "top") == 0)              { rule->has_top = 1; rule->top = parse_float_val(val); }
            else if (strcmp(key, "opacity") == 0)          { rule->has_opacity = 1; rule->opacity = parse_float_val(val); }
            else if (strcmp(key, "cursor") == 0)           { rule->has_cursor = 1; rule->cursor_pointer = (strstr(val, "pointer") != NULL); }
            else if (strcmp(key, "display") == 0)          { rule->has_display = 1; rule->display_none = (strcmp(val, "none") == 0); }
            else if (strcmp(key, "z-index") == 0)          { rule->has_z_index = 1; rule->z_index = atoi(val); }
            else if (strcmp(key, "text-align") == 0) {
                rule->has_text_align = 1;
                if      (strcmp(val, "center") == 0) rule->text_align = 1;
                else if (strcmp(val, "right") == 0)  rule->text_align = 2;
                else                                 rule->text_align = 0;
            }
            else if (strcmp(key, "font-size") == 0)   { rule->has_font_size = 1; rule->font_size = (int)parse_float_val(val); }
            else if (strcmp(key, "font-weight") == 0) { rule->has_font_weight = 1; rule->font_bold = (strstr(val, "bold") != NULL || atoi(val) >= 600); }
            else if (strcmp(key, "box-shadow") == 0)  { parse_box_shadow(val, rule); }
        }
        if (!semi) break;
        prop = semi + 1;
    }
}

int cmp_rules_by_specificity(const void* a, const void* b) {
    return ((const CSSRule*)a)->specificity - ((const CSSRule*)b)->specificity;
}

void parse_css(const char* css_text) {
    char* copy = strdup(css_text);
    // Strip /* ... */ comments
    char* c;
    while ((c = strstr(copy, "/*")) != NULL) {
        char* end = strstr(c, "*/");
        if (end) memset(c, ' ', end - c + 2); else break;
    }

    char* rule_start = copy;
    while (rule_start && *rule_start && rule_count < MAX_RULES) {
        char* brace_open = strchr(rule_start, '{');
        if (!brace_open) break;
        *brace_open = '\0';

        char selector_line[256] = {0};
        strncpy(selector_line, rule_start, 255); trim_whitespace(selector_line);

        char* brace_close = strchr(brace_open + 1, '}');
        if (!brace_close) break;
        *brace_close = '\0';

        char declarations[1024] = {0};
        strncpy(declarations, brace_open + 1, 1023); trim_whitespace(declarations);

        CSSRule template_rule; memset(&template_rule, 0, sizeof(CSSRule));
        parse_declarations(declarations, &template_rule);

        char sel_copy[256]; strncpy(sel_copy, selector_line, 255); sel_copy[255] = 0;
        char* sel_tok = strtok(sel_copy, ",");
        while (sel_tok && rule_count < MAX_RULES) {
            char one[128] = {0}; strncpy(one, sel_tok, 127); trim_whitespace(one);

            CSSRule rule = template_rule;
            char* pseudo_hover  = strstr(one, ":hover");
            char* pseudo_active = strstr(one, ":active");
            if (pseudo_hover)  { rule.is_hover = 1; *pseudo_hover = '\0'; }
            if (pseudo_active) { rule.is_active = 1; *pseudo_active = '\0'; }
            trim_whitespace(one);

            strncpy(rule.selector, one, 127);
            parse_selector(one, &rule);

            int spec = (rule.target.sel_id[0] ? 100 : 0)
                     + rule.target.sel_class_count * 10
                     + (rule.target.sel_type[0] ? 1 : 0);
            for (int a = 0; a < rule.ancestor_count; a++)
                spec += (rule.ancestors[a].sel_id[0] ? 100 : 0)
                      + rule.ancestors[a].sel_class_count * 10
                      + (rule.ancestors[a].sel_type[0] ? 1 : 0);
            rule.specificity = spec;
            if (rule.is_hover)  rule.specificity += 1000;
            if (rule.is_active) rule.specificity += 2000;

            css_rules[rule_count++] = rule;
            sel_tok = strtok(NULL, ",");
        }
        rule_start = brace_close + 1;
    }
    free(copy);
    qsort(css_rules, rule_count, sizeof(CSSRule), cmp_rules_by_specificity);
}

// ============================================================
// Style application
// ============================================================

void update_element_style(Element* e) {
    // Reset to defaults (preserve programmatic flags)
    if (!e->has_custom_bg)     { e->r = 0.95f; e->g = 0.95f; e->b = 0.95f; e->a = 1.0f; }
    if (!e->has_custom_color)  { e->t_r = 0.1f; e->t_g = 0.1f; e->t_b = 0.1f; e->t_a = 1.0f; }
    if (!e->has_custom_border) { e->border_width = 0; e->bd_r = 0; e->bd_g = 0; e->bd_b = 0; e->bd_a = 0; }
    e->border_radius = 0; e->padding = 10;
    e->opacity = 1; e->display_none = 0; e->cursor_pointer = 0;
    e->text_align = 0; e->font_size = 16; e->font_bold = 0;
    e->has_shadow = 0; e->has_gradient = 0; e->z_index = 0;

    for (int i = 0; i < rule_count; i++) {
        CSSRule* r = &css_rules[i];
        if (!selector_matches(r, e)) continue;
        if (r->is_hover  && !e->is_hovered) continue;
        if (r->is_active && !e->is_active)  continue;

        if (r->has_bg)     { e->r = r->bg_r; e->g = r->bg_g; e->b = r->bg_b; e->a = r->bg_a; e->has_custom_bg = 1; }
        if (r->has_color)  { e->t_r = r->c_r; e->t_g = r->c_g; e->t_b = r->c_b; e->t_a = r->c_a; e->has_custom_color = 1; }
        if (r->has_border) { e->bd_r = r->bd_r; e->bd_g = r->bd_g; e->bd_b = r->bd_b; e->bd_a = r->bd_a; e->border_width = r->border_width; e->has_custom_border = 1; }
        if (r->has_radius)  e->border_radius = r->border_radius;
        if (r->has_width)   e->w = r->width;
        if (r->has_height)  e->h = r->height;
        if (r->has_padding) e->padding = r->padding;
        if (r->has_left)    e->rel_x = r->left;
        if (r->has_top)     e->rel_y = r->top;
        if (r->has_opacity) e->opacity = r->opacity;
        if (r->has_cursor)  e->cursor_pointer = r->cursor_pointer;
        if (r->has_display) e->display_none = r->display_none;
        if (r->has_text_align)  e->text_align = r->text_align;
        if (r->has_font_size)   e->font_size = r->font_size;
        if (r->has_font_weight) e->font_bold = r->font_bold;
        if (r->has_shadow) {
            e->has_shadow = 1;
            e->sh_dx = r->sh_dx; e->sh_dy = r->sh_dy; e->sh_blur = r->sh_blur;
            e->sh_r = r->sh_r; e->sh_g = r->sh_g; e->sh_b = r->sh_b; e->sh_a = r->sh_a;
        }
        if (r->has_gradient) {
            e->has_gradient = 1;
            e->grad_r1 = r->grad_r1; e->grad_g1 = r->grad_g1;
            e->grad_b1 = r->grad_b1; e->grad_a1 = r->grad_a1;
            e->grad_r2 = r->grad_r2; e->grad_g2 = r->grad_g2;
            e->grad_b2 = r->grad_b2; e->grad_a2 = r->grad_a2;
            e->grad_angle = r->grad_angle;
        } else if (r->has_bg) {
            // Explicit background-color overrides gradient
            e->has_gradient = 0;
        }
        if (r->has_z_index) e->z_index = r->z_index;
    }
}

// ============================================================
// HTML parsing
// ============================================================

static int is_void_element(const char* t) {
    static const char* v[] = {
        "area","base","br","col","embed","hr","img","input",
        "link","meta","param","source","track","wbr",NULL
    };
    for (int i = 0; v[i]; i++) if (strcasecmp(t, v[i]) == 0) return 1;
    return 0;
}

static int is_ignored_element(const char* t) {
    static const char* ig[] = {"html","head","body","style","script","title",NULL};
    for (int i = 0; ig[i]; i++) if (strcasecmp(t, ig[i]) == 0) return 1;
    return 0;
}

void parse_html(const char* html) {
    int parent_stack[64];
    int stack_ptr = 0;
    int current_parent = -1;
    const char* p = html;

    while (*p && elem_count < MAX_ELEMENTS) {
        const char* tag_start = strchr(p, '<');
        if (!tag_start) break;

        // Skip comments <!-- ... -->
        if (strncmp(tag_start + 1, "!--", 3) == 0) {
            const char* end_c = strstr(tag_start, "-->");
            p = end_c ? end_c + 3 : tag_start + strlen(tag_start);
            continue;
        }

        // Skip <!DOCTYPE ...> and other <!...> declarations
        if (tag_start[1] == '!') {
            const char* gt = strchr(tag_start, '>');
            p = gt ? gt + 1 : tag_start + strlen(tag_start);
            continue;
        }

        // Closing tag
        if (tag_start[1] == '/') {
            if (stack_ptr > 0) stack_ptr--;
            current_parent = (stack_ptr > 0) ? parent_stack[stack_ptr - 1] : -1;
            const char* gt = strchr(tag_start, '>');
            p = gt ? gt + 1 : tag_start + strlen(tag_start);
            continue;
        }

        const char* tag_end = strchr(tag_start, '>');
        if (!tag_end) break;

        char tag_buf[512] = {0};
        int tblen = (int)(tag_end - tag_start - 1);
        if (tblen > 511) tblen = 511;
        strncpy(tag_buf, tag_start + 1, tblen);

        // Detect self-closing />
        int is_self_closing = (tblen > 0 && tag_buf[tblen - 1] == '/');

        char type[32] = {0};
        sscanf(tag_buf, "%31s", type);
        // Strip trailing '/' from type
        int tl = (int)strlen(type);
        while (tl > 0 && (type[tl-1] == '/' || isspace((unsigned char)type[tl-1]))) type[--tl] = '\0';

        // Skip style/script content
        if (strcasecmp(type, "style") == 0 || strcasecmp(type, "script") == 0) {
            char close_tag[40]; snprintf(close_tag, sizeof(close_tag), "</%s", type);
            const char* close_pos = strcasestr(tag_end + 1, close_tag);
            const char* gt2 = close_pos ? strchr(close_pos, '>') : NULL;
            p = gt2 ? gt2 + 1 : tag_end + 1;
            continue;
        }

        if (is_ignored_element(type)) {
            if (!is_void_element(type) && !is_self_closing &&
                strcasecmp(type, "html") != 0 && strcasecmp(type, "head") != 0 &&
                strcasecmp(type, "body") != 0) {
                // Need to push so close tag pops correctly
                if (stack_ptr < 63) parent_stack[stack_ptr++] = -1;
            }
            p = tag_end + 1;
            continue;
        }

        char id[64] = {0}, class_name[96] = {0};
        int draggable = 0;
        char* attr_id    = strstr(tag_buf, "id=\"");
        if (attr_id) sscanf(attr_id + 4, "%63[^\"]", id);
        char* attr_class = strstr(tag_buf, "class=\"");
        if (attr_class) sscanf(attr_class + 7, "%95[^\"]", class_name);
        char* attr_drag  = strstr(tag_buf, "draggable=\"");
        if (attr_drag) sscanf(attr_drag + 11, "%d", &draggable);

        char text[256] = {0};
        const char* next_tag = strchr(tag_end, '<');
        if (next_tag) {
            int text_len = (int)(next_tag - tag_end - 1);
            if (text_len > 0 && text_len < 255) {
                strncpy(text, tag_end + 1, text_len);
                trim_whitespace(text);
            }
        }

        Element e = {0};
        e.id_idx = elem_count; e.parent_idx = current_parent;
        e.w = 100; e.h = 50; e.is_draggable = draggable;
        strncpy(e.type, type, 31);
        strncpy(e.class_name, class_name, 95);
        strncpy(e.id, id, 63);
        strncpy(e.text, text, 255);
        e.cur_scale = 1.0f;

        elements[elem_count] = e;
        update_element_style(&elements[elem_count]);

        Element* ne = &elements[elem_count];
        ne->cur_r = ne->r; ne->cur_g = ne->g; ne->cur_b = ne->b; ne->cur_a = ne->a;
        ne->cur_bd_r = ne->bd_r; ne->cur_bd_g = ne->bd_g;
        ne->cur_bd_b = ne->bd_b; ne->cur_bd_a = ne->bd_a;

        if (!is_void_element(type) && !is_self_closing && stack_ptr < 63) {
            parent_stack[stack_ptr++] = elem_count;
            current_parent = elem_count;
        }
        elem_count++;
        p = tag_end + 1;
    }
}

// ============================================================
// Layout & Animation
// ============================================================

void update_layout() {
    for (int i = 0; i < elem_count; i++) {
        if (elements[i].parent_idx == -1) {
            elements[i].x = elements[i].rel_x;
            elements[i].y = elements[i].rel_y;
        } else {
            int par = elements[i].parent_idx;
            elements[i].x = elements[par].x + elements[i].rel_x;
            elements[i].y = elements[par].y + elements[i].rel_y;
        }
    }
}

void update_animations(double dt) {
    float factor = 1.0f - expf(-(float)dt * 14.0f);
    if (factor > 1.0f) factor = 1.0f;
    if (factor < 0.0f) factor = 0.0f;

    for (int i = 0; i < elem_count; i++) {
        Element* e = &elements[i];
        e->cur_r   += (e->r   - e->cur_r)   * factor;
        e->cur_g   += (e->g   - e->cur_g)   * factor;
        e->cur_b   += (e->b   - e->cur_b)   * factor;
        e->cur_a   += (e->a   - e->cur_a)   * factor;
        e->cur_bd_r += (e->bd_r - e->cur_bd_r) * factor;
        e->cur_bd_g += (e->bd_g - e->cur_bd_g) * factor;
        e->cur_bd_b += (e->bd_b - e->cur_bd_b) * factor;
        e->cur_bd_a += (e->bd_a - e->cur_bd_a) * factor;

        float target_scale = (e->cursor_pointer && e->is_active) ? 0.96f : 1.0f;
        e->cur_scale += (target_scale - e->cur_scale) * factor;
    }
}

// ============================================================
// Render-order sorting by z-index
// ============================================================

// Effective z-index = max of self and all ancestors.
// This ensures children are always rendered after (on top of) their parent,
// even when the parent has a higher explicit z-index than the child.
static int effective_z_index(int idx) {
    int z = 0;
    while (idx != -1) {
        if (elements[idx].z_index > z) z = elements[idx].z_index;
        idx = elements[idx].parent_idx;
    }
    return z;
}

static int cmp_render_order(const void* a, const void* b) {
    int ia = *(const int*)a, ib = *(const int*)b;
    int za = effective_z_index(ia), zb = effective_z_index(ib);
    if (za != zb) return za - zb;
    return ia - ib; // preserve DOM order for same effective z-index
}

static void build_render_order() {
    for (int i = 0; i < elem_count; i++) render_order[i] = i;
    qsort(render_order, elem_count, sizeof(int), cmp_render_order);
}

// ============================================================
// Font loading
// ============================================================

int bake_font_set(const unsigned char* ttf_buffer, FontAtlas* atlases) {
    int offset = stbtt_GetFontOffsetForIndex(ttf_buffer, 0);
    if (offset < 0) offset = 0;
    for (int i = 0; i < NUM_FONT_SIZES; i++) {
        static unsigned char temp_bitmap[512 * 512];
        stbtt_BakeFontBitmap(ttf_buffer, offset, font_sizes[i],
                             temp_bitmap, 512, 512, 32, 96, atlases[i].cdata);
        glGenTextures(1, &atlases[i].tex);
        glBindTexture(GL_TEXTURE_2D, atlases[i].tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, 512, 512, 0, GL_RED, GL_UNSIGNED_BYTE, temp_bitmap);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        atlases[i].loaded = 1;
    }
    return 1;
}

void init_font() {
    const char* regular_paths[] = {
        "/usr/share/fonts/ja/TrueType/NotoSansCJKjp-Regular.otf",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",
        "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        NULL
    };
    const char* bold_paths[] = {
        "/usr/share/fonts/ja/TrueType/NotoSansCJKjp-Bold.otf",
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Bold.ttc",
        "/usr/share/fonts/truetype/noto/NotoSans-Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        NULL
    };

    unsigned char* reg_buf = NULL;
    for (int i = 0; regular_paths[i]; i++) {
        reg_buf = read_file_bytes(regular_paths[i], NULL);
        if (reg_buf) break;
    }
    if (!reg_buf) { fprintf(stderr, "[vespera] Warning: no font found\n"); return; }
    bake_font_set(reg_buf, font_regular);
    free(reg_buf);

    unsigned char* bold_buf = NULL;
    for (int i = 0; bold_paths[i]; i++) {
        bold_buf = read_file_bytes(bold_paths[i], NULL);
        if (bold_buf) break;
    }
    if (bold_buf) {
        bake_font_set(bold_buf, font_bold_atlas);
        bold_font_loaded = 1;
        free(bold_buf);
    }

    glCreateVertexArrays(1, &text_vao); glGenBuffers(1, &text_vbo);
    glBindVertexArray(text_vao); glBindBuffer(GL_ARRAY_BUFFER, text_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 4 * 4, NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glEnableVertexAttribArray(0);
    font_loaded = 1;
}

FontAtlas* get_atlas(int size, int bold, int* out_is_fake_bold) {
    int best = 0; float best_diff = 1e9f;
    for (int i = 0; i < NUM_FONT_SIZES; i++) {
        float d = fabsf(font_sizes[i] - (float)size);
        if (d < best_diff) { best_diff = d; best = i; }
    }
    if (bold && bold_font_loaded) {
        if (out_is_fake_bold) *out_is_fake_bold = 0;
        return &font_bold_atlas[best];
    }
    if (out_is_fake_bold) *out_is_fake_bold = bold ? 1 : 0;
    return &font_regular[best];
}

float measure_text_width(FontAtlas* atlas, const char* text) {
    float w = 0.0f;
    for (const char* t = text; *t; t++) {
        unsigned char c = (unsigned char)*t;
        if (c >= 32 && c < 128) w += atlas->cdata[c - 32].xadvance;
    }
    return w;
}

// ============================================================
// Drawing
// ============================================================

void init_rect_geometry() {
    float vertices[] = { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };
    glCreateVertexArrays(1, &g_rect_vao);
    glGenBuffers(1, &g_rect_vbo);
    glBindVertexArray(g_rect_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_rect_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
}

// Full-featured rect draw: solid color or gradient.
void draw_rect_full(float x, float y, float w, float h,
                    float r, float g, float b, float a,
                    float radius, float b_w,
                    float bd_r, float bd_g, float bd_b, float bd_a,
                    int has_grad,
                    float gr1, float gg1, float gb1, float ga1,
                    float gr2, float gg2, float gb2, float ga2,
                    float gang) {
    if (a <= 0.0f && bd_a <= 0.0f && !has_grad) return;

    glUseProgram(bg_program);
    glUniform2f(glGetUniformLocation(bg_program, "uResolution"), window_width, window_height);
    glUniform2f(glGetUniformLocation(bg_program, "uPos"),  x, y);
    glUniform2f(glGetUniformLocation(bg_program, "uSize"), w, h);
    glUniform4f(glGetUniformLocation(bg_program, "uColor"),       r,    g,    b,    a);
    glUniform4f(glGetUniformLocation(bg_program, "uBorderColor"), bd_r, bd_g, bd_b, bd_a);
    glUniform1f(glGetUniformLocation(bg_program, "uBorderWidth"), b_w);
    glUniform1f(glGetUniformLocation(bg_program, "uRadius"),      radius);
    glUniform1i_(glGetUniformLocation(bg_program, "uGradient"),    has_grad);
    if (has_grad) {
        glUniform4f(glGetUniformLocation(bg_program, "uGradColor1"), gr1, gg1, gb1, ga1);
        glUniform4f(glGetUniformLocation(bg_program, "uGradColor2"), gr2, gg2, gb2, ga2);
        glUniform1f(glGetUniformLocation(bg_program, "uGradAngle"),  gang);
    }

    glBindVertexArray(g_rect_vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

// Simplified solid-color rect (no gradient).
void draw_rect(float x, float y, float w, float h,
               float r, float g, float b, float a,
               float radius, float b_w,
               float bd_r, float bd_g, float bd_b, float bd_a) {
    draw_rect_full(x, y, w, h, r, g, b, a, radius, b_w, bd_r, bd_g, bd_b, bd_a,
                   0, 0,0,0,0, 0,0,0,0, 0.0f);
}

void render_text_pass(FontAtlas* atlas, const char* text,
                      float start_x, float baseline_y,
                      float r, float g, float b, float a) {
    glUseProgram(text_program);
    glUniform2f(glGetUniformLocation(text_program, "uResolution"), window_width, window_height);
    glUniform4f(glGetUniformLocation(text_program, "textColor"), r, g, b, a);
    if (glActiveTexture_) glActiveTexture_(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas->tex);
    glBindVertexArray(text_vao);

    float draw_x = start_x, draw_y = baseline_y;
    while (*text) {
        unsigned char tc = (unsigned char)*text;
        if (tc >= 32 && tc < 128) {
            stbtt_aligned_quad q;
            stbtt_GetBakedQuad(atlas->cdata, 512, 512, (int)(tc - 32), &draw_x, &draw_y, &q, 1);
            float verts[4][4] = {
                { q.x0, q.y0, q.s0, q.t0 }, { q.x1, q.y0, q.s1, q.t0 },
                { q.x1, q.y1, q.s1, q.t1 }, { q.x0, q.y1, q.s0, q.t1 }
            };
            glBindBuffer(GL_ARRAY_BUFFER, text_vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
            glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        }
        text++;
    }
    glBindTexture(GL_TEXTURE_2D, 0);
}

void render_text(const char* text, float x, float y, float box_w, float box_h, int align,
                 float r, float g, float b, float a, int fsize, int bold) {
    if (!font_loaded || !text || !*text || a <= 0.0f) return;

    int is_fake_bold = 0;
    FontAtlas* atlas = get_atlas(fsize, bold, &is_fake_bold);
    if (!atlas->loaded) return;

    float text_w = measure_text_width(atlas, text);
    float start_x = x;
    if      (align == 1) start_x = x + (box_w - text_w) / 2.0f;
    else if (align == 2) start_x = x + (box_w - text_w);
    if (start_x < x) start_x = x;

    float used_size = font_sizes[0];
    for (int i = 0; i < NUM_FONT_SIZES; i++) {
        if (&font_regular[i] == atlas || &font_bold_atlas[i] == atlas) {
            used_size = font_sizes[i]; break;
        }
    }
    float cap_h   = used_size * 0.72f;
    float baseline = y + (box_h - cap_h) / 2.0f + cap_h;
    if (baseline < y + cap_h) baseline = y + cap_h;

    render_text_pass(atlas, text, start_x, baseline, r, g, b, a);
    if (is_fake_bold) render_text_pass(atlas, text, start_x + 1.0f, baseline, r, g, b, a);
}

// ============================================================
// Event callbacks
// ============================================================

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    if (drag_target_idx != -1) {
        float new_x = (float)xpos - drag_offset_x;
        float new_y = (float)ypos - drag_offset_y;
        if (elements[drag_target_idx].parent_idx == -1) {
            elements[drag_target_idx].rel_x = new_x;
            elements[drag_target_idx].rel_y = new_y;
        } else {
            int par = elements[drag_target_idx].parent_idx;
            elements[drag_target_idx].rel_x = new_x - elements[par].x;
            elements[drag_target_idx].rel_y = new_y - elements[par].y;
        }
        return;
    }

    int hit = -1;
    for (int ri = elem_count - 1; ri >= 0; ri--) {
        int i = render_order[ri];
        Element* e = &elements[i];
        if (!is_visible(i)) continue;
        if (xpos >= e->x && xpos <= e->x + e->w &&
            ypos >= e->y && ypos <= e->y + e->h) { hit = i; break; }
    }

    int any_pointer = 0;
    for (int i = 0; i < elem_count; i++) {
        Element* e = &elements[i];
        int should_hover = 0;
        for (int a = hit; a != -1; a = elements[a].parent_idx) {
            if (a == i) { should_hover = 1; break; }
        }
        if (should_hover != e->is_hovered) {
            e->is_hovered = should_hover;
            update_element_style(e);
        }
        if (should_hover && e->cursor_pointer) any_pointer = 1;
    }

    if (any_pointer && !g_cursor_is_hand) {
        glfwSetCursor(window, g_hand_cursor); g_cursor_is_hand = 1;
    } else if (!any_pointer && g_cursor_is_hand) {
        glfwSetCursor(window, NULL); g_cursor_is_hand = 0;
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    (void)window; (void)mods;
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);

    if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_LEFT) {
        // Iterate in reverse render order (topmost first)
        for (int ri = elem_count - 1; ri >= 0; ri--) {
            int i = render_order[ri];
            Element* e = &elements[i];
            if (!is_visible(i)) continue;
            if (mx >= e->x && mx <= e->x + e->w && my >= e->y && my <= e->y + e->h) {
                e->is_active = 1;
                update_element_style(e);
                if (e->is_draggable) {
                    drag_target_idx = (e->parent_idx != -1) ? e->parent_idx : i;
                    drag_offset_x = (float)mx - elements[drag_target_idx].x;
                    drag_offset_y = (float)my - elements[drag_target_idx].y;
                }
                break;
            }
        }
    } else if (action == GLFW_RELEASE) {
        for (int i = 0; i < elem_count; i++) {
            if (elements[i].is_active) {
                elements[i].is_active = 0;
                if (elements[i].is_hovered && elements[i].on_click)
                    elements[i].on_click(&elements[i]);
                update_element_style(&elements[i]);
            }
        }
        drag_target_idx = -1;
    }
}

// ============================================================
// Demo button callbacks
// ============================================================

void handle_close(Element* e) {
    (void)e;
    int title = get_element_by_id("title_text");
    if (title != -1) set_text(title, "Closing...");
    if (g_window) glfwSetWindowShouldClose(g_window, GLFW_TRUE);
}

void btn_click(Element* e) {
    (void)e;
    if (g_modal_overlay_idx != -1) {
        remove_class(&elements[g_modal_overlay_idx], "hidden");
        update_element_style(&elements[g_modal_overlay_idx]);
    }
}

void modal_cancel_click(Element* e) {
    (void)e;
    if (g_modal_overlay_idx != -1) {
        add_class(&elements[g_modal_overlay_idx], "hidden");
        update_element_style(&elements[g_modal_overlay_idx]);
    }
}

void modal_confirm_click(Element* e) {
    (void)e;
    int desc = get_element_by_id("desc");
    if (desc != -1) set_text(desc, "Settings applied. Gradient CSS engine active!");
    if (g_modal_overlay_idx != -1) {
        add_class(&elements[g_modal_overlay_idx], "hidden");
        update_element_style(&elements[g_modal_overlay_idx]);
    }
}

void toggle_click(Element* e) {
    int knob = get_element_by_id("toggle_knob");
    if (element_has_class(e, "on")) {
        remove_class(e, "on");
        if (knob != -1) elements[knob].rel_x = 3;
    } else {
        add_class(e, "on");
        if (knob != -1) elements[knob].rel_x = 25;
    }
    update_element_style(e);
}

void checkbox_click(Element* e) {
    if (element_has_class(e, "checked")) {
        remove_class(e, "checked");
        set_text(get_element_by_id("chk_label"), "Enable Notifications");
    } else {
        add_class(e, "checked");
        set_text(get_element_by_id("chk_label"), "Notifications: On");
    }
    update_element_style(e);
}

void toast_dismiss(Element* e) {
    (void)e;
    if (g_toast_idx != -1) elements[g_toast_idx].display_none = 1;
}

// ============================================================
// GL setup
// ============================================================

GLuint compile_shader(const char* src, GLenum type) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, NULL);
    glCompileShader(shader);
    return shader;
}

void load_gl_functions() {
#define LOAD(T, name) name = (T)glfwGetProcAddress(#name)
    LOAD(PFNGLCREATEVERTEXARRAYSPROC, glCreateVertexArrays);
    LOAD(PFNGLDELETEVERTEXARRAYSPROC, glDeleteVertexArrays);
    LOAD(PFNGLGENBUFFERSPROC,         glGenBuffers);
    LOAD(PFNGLBINDBUFFERPROC,         glBindBuffer);
    LOAD(PFNGLBUFFERDATAPROC,         glBufferData);
    LOAD(PFNGLBUFFERSUBDATAPROC,      glBufferSubData);
    LOAD(PFNGLVERTEXATTRIBPOINTERPROC,    glVertexAttribPointer);
    LOAD(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray);
    LOAD(PFNGLDELETEBUFFERSPROC,      glDeleteBuffers);
    LOAD(PFNGLBINDVERTEXARRAYPROC,    glBindVertexArray);
    LOAD(PFNGLCREATESHADERPROC,       glCreateShader);
    LOAD(PFNGLSHADERSOURCEPROC,       glShaderSource);
    LOAD(PFNGLCOMPILESHADERPROC,      glCompileShader);
    LOAD(PFNGLCREATEPROGRAMPROC,      glCreateProgram);
    LOAD(PFNGLATTACHSHADERPROC,       glAttachShader);
    LOAD(PFNGLLINKPROGRAMPROC,        glLinkProgram);
    LOAD(PFNGLUSEPROGRAMPROC,         glUseProgram);
    LOAD(PFNGLGETUNIFORMLOCATIONPROC, glGetUniformLocation);
    LOAD(PFNGLUNIFORM4FPROC,          glUniform4f);
    LOAD(PFNGLUNIFORM2FPROC,          glUniform2f);
    LOAD(PFNGLUNIFORM1FPROC,          glUniform1f);
    glUniform1i_ = (PFNGLUNIFORM1IPROC)glfwGetProcAddress("glUniform1i");
    glActiveTexture_ = (PFNGLACTIVETEXTUREPROC)glfwGetProcAddress("glActiveTexture");
#undef LOAD
}

static void parse_args(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--desktop") == 0 || strcmp(argv[i], "-d") == 0) {
            g_desktop_mode = 1; g_fullscreen = 1;
        } else if (strcmp(argv[i], "--fullscreen") == 0 || strcmp(argv[i], "-f") == 0) {
            g_fullscreen = 1;
        } else if (strcmp(argv[i], "--size") == 0 && i + 1 < argc) {
            sscanf(argv[++i], "%fx%f", &window_width, &window_height);
        } else if (strcmp(argv[i], "--layout") == 0 && i + 1 < argc) {
            g_layout_path = argv[++i];
        } else if (strcmp(argv[i], "--css") == 0 && i + 1 < argc) {
            g_css_path = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            fprintf(stderr, "lu-shell [--desktop] [--fullscreen] [--size WxH] [--layout PATH] [--css PATH]\n");
            exit(0);
        }
    }
    if (g_desktop_mode && !getenv("LU_SHELL_LAYOUT")) {
        const char* dl = getenv("LU_DESKTOP_LAYOUT");
        const char* dc = getenv("LU_DESKTOP_CSS");
        if (dl) g_layout_path = dl;
        if (dc) g_css_path    = dc;
    }
}

// ============================================================
// Built-in demo CSS/HTML
// ============================================================

static const char* default_css =
    // ---- Window & cards ----
    ".window { background-color: rgba(252,252,255,1); border-radius: 14; border-width: 1;"
    "  border-color: #00000018; box-shadow: 0 14 44 rgba(0,0,0,0.18); }\n"
    ".card { border-width: 1; border-color: #00000012; }\n"
    ".hidden { display: none; }\n"

    // ---- Main window ----
    "#main_win { width: 680; height: 480; left: 170; top: 144; z-index: 10; }\n"

    // ---- Title bar ----
    "#title_bar { background-color: transparent; width: 680; height: 44; left: 0; top: 0; cursor: pointer; }\n"
    "#title_text { color: #444444ff; background-color: transparent; left: 0; top: 13; width: 680; height: 20;"
    "  text-align: center; font-weight: bold; font-size: 14; }\n"
    ".btn_os { width: 13; height: 13; border-radius: 7; top: 16; cursor: pointer; }\n"
    "#btn_close { background-color: #ff5f56ff; left: 18; }\n"
    "#btn_min   { background-color: #ffbd2eff; left: 37; }\n"
    "#btn_max   { background-color: #27c93fff; left: 56; }\n"
    "#btn_close:hover { background-color: #ff7972ff; border-width: 1; border-color: #e0443e80; }\n"
    "#btn_min:hover   { background-color: #ffcc54ff; border-width: 1; border-color: #dea12380; }\n"
    "#btn_max:hover   { background-color: #4cd860ff; border-width: 1; border-color: #1aab2980; }\n"

    // ---- Sidebar ----
    "#sidebar { background-color: #f2f3f7ff; border-radius: 14; width: 180; height: 436;"
    "  left: 0; top: 44; border-width: 1; border-color: #00000010; }\n"
    "#sidebar_avatar { background: linear-gradient(135deg, #007affff, #5e5ce6ff);"
    "  border-radius: 24; width: 48; height: 48; left: 66; top: 18; }\n"
    "#sidebar_username { color: #333333ff; background-color: transparent; left: 0; top: 76;"
    "  width: 180; height: 20; text-align: center; font-weight: bold; font-size: 14; }\n"
    ".nav_item { background-color: transparent; color: #555555ff; width: 156; height: 34;"
    "  border-radius: 8; left: 12; padding: 9; cursor: pointer; font-size: 14; }\n"
    ".nav_item:hover { background-color: #e0e2e8ff; color: #333333ff; }\n"
    ".nav_item.active { background-color: #007aff1a; color: #007affff; font-weight: bold; }\n"
    "#nav_1 { top: 106; } #nav_2 { top: 144; } #nav_3 { top: 182; } #nav_4 { top: 220; }\n"

    // ---- Content area ----
    "#content { background-color: transparent; width: 500; height: 436; left: 180; top: 44; }\n"
    "#section_title { color: #222222ff; background-color: transparent; left: 30; top: 16;"
    "  width: 440; height: 28; font-size: 20; font-weight: bold; }\n"
    "#desc { color: #666666ff; background-color: transparent; width: 440; height: 36;"
    "  left: 30; top: 50; padding: 0; font-size: 14; }\n"

    // Separator
    "#sep1 { background-color: #00000012; height: 1; width: 440; left: 30; top: 94; }\n"

    // Apply button — gradient
    "#apply_btn { background: linear-gradient(135deg, #007affff, #5e5ce6ff);"
    "  color: #ffffffff; width: 160; height: 38; border-radius: 10; left: 30; top: 106;"
    "  padding: 8; text-align: center; font-weight: bold; cursor: pointer; }\n"
    "#apply_btn:hover  { background: linear-gradient(135deg, #3395ffff, #7b79f7ff); }\n"
    "#apply_btn:active { background: linear-gradient(135deg, #0062ccff, #4b49c8ff); }\n"

    // Toggle
    "#toggle_row_label { color: #444444ff; background-color: transparent; left: 30; top: 158;"
    "  width: 200; height: 24; font-size: 14; }\n"
    ".toggle { width: 46; height: 26; border-radius: 13; background-color: #d4d4d8ff;"
    "  left: 414; top: 156; cursor: pointer; }\n"
    ".toggle.on { background-color: #34c759ff; }\n"
    ".toggle_knob { width: 20; height: 20; border-radius: 10; background-color: #ffffffff;"
    "  box-shadow: 0 1 3 rgba(0,0,0,0.30); left: 3; top: 3; }\n"

    // Checkbox
    ".checkbox { width: 22; height: 22; border-radius: 6; background-color: #ffffffff;"
    "  border-width: 2; border-color: #c0c0c0ff; left: 30; top: 196; cursor: pointer; }\n"
    ".checkbox.checked { background-color: #007affff; border-color: #0062ccff; }\n"
    "#chk_label { color: #444444ff; background-color: transparent; left: 62; top: 198;"
    "  width: 300; height: 24; font-size: 14; }\n"

    // Slider (visual-only)
    "#slider_label { color: #888888ff; background-color: transparent; left: 30; top: 234;"
    "  width: 300; height: 18; font-size: 13; }\n"
    "#slider_track { background-color: #e5e5eaff; border-radius: 4; width: 400; height: 6;"
    "  left: 30; top: 258; }\n"
    "#slider_fill { background: linear-gradient(90deg, #007affff, #5e5ce6ff);"
    "  border-radius: 4; width: 280; height: 6; left: 0; top: 0; }\n"
    "#slider_thumb { background-color: #ffffffff; border-radius: 10; width: 22; height: 22;"
    "  border-width: 2; border-color: #007affff; left: 270; top: -8;"
    "  box-shadow: 0 2 6 rgba(0,0,0,0.18); }\n"

    // Progress
    "#progress_label { color: #888888ff; background-color: transparent; left: 30; top: 278;"
    "  width: 300; height: 18; font-size: 13; }\n"
    "#progress_track { background-color: #e5e5eaff; border-radius: 4; width: 400; height: 6;"
    "  left: 30; top: 302; }\n"
    "#progress_fill  { background-color: #007affff; border-radius: 4; width: 0; height: 6;"
    "  left: 0; top: 0; }\n"

    // Badges
    ".badge { border-radius: 10; height: 22; padding: 4; font-size: 12;"
    "  text-align: center; color: #ffffffff; }\n"
    "#badge_active { background-color: #34c759ff; width: 60; left: 30; top: 322; }\n"
    "#badge_update { background-color: #ff9500ff; width: 60; left: 98; top: 322; }\n"
    "#badge_err    { background-color: #ff3b30ff; width: 60; left: 166; top: 322; }\n"

    // Separator
    "#sep2 { background-color: #00000012; height: 1; width: 440; left: 30; top: 358; }\n"
    "#status_text { color: #aaaaaa; background-color: transparent; left: 30; top: 366;"
    "  width: 440; height: 18; font-size: 12; text-align: right; }\n"

    // ---- Toast ----
    "#toast { width: 320; height: 80; left: 680; top: 40; z-index: 20; }\n"
    "#toast_bar { background-color: transparent; width: 320; height: 80; left: 0; top: 0; cursor: pointer; }\n"
    "#toast_icon { background: linear-gradient(135deg, #007affff, #5e5ce6ff);"
    "  border-radius: 16; width: 36; height: 36; left: 16; top: 22; }\n"
    "#toast_title { color: #222222ff; background-color: transparent; left: 64; top: 18;"
    "  width: 220; height: 20; font-weight: bold; font-size: 14; }\n"
    "#toast_msg   { color: #888888ff; background-color: transparent; left: 64; top: 38;"
    "  width: 220; height: 30; font-size: 12; }\n"
    "#toast_close { background-color: transparent; color: #aaaaaa; border-radius: 10;"
    "  width: 20; height: 20; left: 290; top: 8; text-align: center; cursor: pointer; font-size: 14; }\n"
    "#toast_close:hover { background-color: #00000014; color: #555555ff; }\n"

    // ---- Modal ----
    "#modal_overlay { background-color: rgba(0,0,0,0.42); width: 1024; height: 768;"
    "  left: 0; top: 0; border-radius: 0; z-index: 100; }\n"
    "#modal_dialog  { width: 380; height: 236; left: 322; top: 266; z-index: 110; }\n"
    "#modal_icon    { background: linear-gradient(135deg, #007affff, #5e5ce6ff);"
    "  border-radius: 24; width: 48; height: 48; left: 166; top: 22; }\n"
    "#modal_title   { color: #222222ff; background-color: transparent; width: 380; height: 24;"
    "  left: 0; top: 84; text-align: center; font-weight: bold; font-size: 18; }\n"
    "#modal_msg     { color: #666666ff; background-color: transparent; width: 320; height: 44;"
    "  left: 30; top: 114; text-align: center; font-size: 14; }\n"
    "#modal_cancel  { background-color: #f2f2f7ff; color: #444444ff; width: 140; height: 40;"
    "  border-radius: 10; left: 28; top: 176; text-align: center; cursor: pointer;"
    "  border-width: 1; border-color: #00000018; }\n"
    "#modal_cancel:hover  { background-color: #e5e5eaff; }\n"
    "#modal_cancel:active { background-color: #d1d1d6ff; }\n"
    "#modal_confirm { background: linear-gradient(135deg, #007affff, #5e5ce6ff);"
    "  color: #ffffffff; width: 140; height: 40; border-radius: 10; left: 212; top: 176;"
    "  text-align: center; font-weight: bold; cursor: pointer; }\n"
    "#modal_confirm:hover  { background: linear-gradient(135deg, #3395ffff, #7b79f7ff); }\n"
    "#modal_confirm:active { background: linear-gradient(135deg, #0062ccff, #4b49c8ff); }\n";

static const char* default_html =
    "<div id=\"main_win\" class=\"window\">\n"
    "  <div id=\"title_bar\" draggable=\"1\">\n"
    "    <div id=\"btn_close\" class=\"btn_os\"></div>\n"
    "    <div id=\"btn_min\" class=\"btn_os\"></div>\n"
    "    <div id=\"btn_max\" class=\"btn_os\"></div>\n"
    "    <p id=\"title_text\">System Preferences</p>\n"
    "  </div>\n"
    "  <div id=\"sidebar\">\n"
    "    <div id=\"sidebar_avatar\"></div>\n"
    "    <p id=\"sidebar_username\">Vespera User</p>\n"
    "    <div id=\"nav_1\" class=\"nav_item active\">General</div>\n"
    "    <div id=\"nav_2\" class=\"nav_item\">Appearance</div>\n"
    "    <div id=\"nav_3\" class=\"nav_item\">Network</div>\n"
    "    <div id=\"nav_4\" class=\"nav_item\">Security</div>\n"
    "  </div>\n"
    "  <div id=\"content\">\n"
    "    <p id=\"section_title\">General</p>\n"
    "    <p id=\"desc\">Customize system preferences and appearance settings.</p>\n"
    "    <div id=\"sep1\"></div>\n"
    "    <div id=\"apply_btn\">Apply Changes</div>\n"
    "    <p id=\"toggle_row_label\">Dark Mode</p>\n"
    "    <div id=\"toggle_knob_track\" class=\"toggle\">\n"
    "      <div id=\"toggle_knob\" class=\"toggle_knob\"></div>\n"
    "    </div>\n"
    "    <div id=\"checkbox_notify\" class=\"checkbox\"></div>\n"
    "    <p id=\"chk_label\">Enable Notifications</p>\n"
    "    <p id=\"slider_label\">Brightness: 70%</p>\n"
    "    <div id=\"slider_track\">\n"
    "      <div id=\"slider_fill\"></div>\n"
    "      <div id=\"slider_thumb\"></div>\n"
    "    </div>\n"
    "    <p id=\"progress_label\">Syncing...</p>\n"
    "    <div id=\"progress_track\">\n"
    "      <div id=\"progress_fill\"></div>\n"
    "    </div>\n"
    "    <div id=\"badge_active\" class=\"badge\">Active</div>\n"
    "    <div id=\"badge_update\" class=\"badge\">Update</div>\n"
    "    <div id=\"badge_err\"    class=\"badge\">Error</div>\n"
    "    <div id=\"sep2\"></div>\n"
    "    <p id=\"status_text\">v2.1.0 - Vespera GUI Engine</p>\n"
    "  </div>\n"
    "</div>\n"
    "<div id=\"toast\" class=\"window card\">\n"
    "  <div id=\"toast_bar\" draggable=\"1\">\n"
    "    <div id=\"toast_icon\"></div>\n"
    "    <p id=\"toast_title\">Update Available</p>\n"
    "    <p id=\"toast_msg\">Version 2.1 is ready to install.</p>\n"
    "    <div id=\"toast_close\">x</div>\n"
    "  </div>\n"
    "</div>\n"
    "<div id=\"modal_overlay\" class=\"overlay hidden\">\n"
    "  <div id=\"modal_dialog\" class=\"window\">\n"
    "    <div id=\"modal_icon\"></div>\n"
    "    <p id=\"modal_title\">Apply Changes?</p>\n"
    "    <p id=\"modal_msg\">Your preferences will be updated immediately.</p>\n"
    "    <div id=\"modal_cancel\">Cancel</div>\n"
    "    <div id=\"modal_confirm\">Apply</div>\n"
    "  </div>\n"
    "</div>\n";

// ============================================================
// Main
// ============================================================

int main(int argc, char** argv) {
    parse_args(argc, argv);

    if (getenv("WAYLAND_DISPLAY")) {
#if defined(GLFW_PLATFORM_WAYLAND)
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND);
#endif
    }

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    if (g_fullscreen) {
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    }

    const char* title = g_desktop_mode ? "Lu Shell" : "Vespera GUI Engine";
    GLFWmonitor* monitor = g_fullscreen ? glfwGetPrimaryMonitor() : NULL;
    GLFWwindow* window = glfwCreateWindow((int)window_width, (int)window_height, title, monitor, NULL);
    g_window = window;
    glfwMakeContextCurrent(window);
    load_gl_functions();
    g_hand_cursor = glfwCreateStandardCursor(GLFW_HAND_CURSOR);

    GLuint vs  = compile_shader(bg_vs,   GL_VERTEX_SHADER);
    GLuint fs  = compile_shader(bg_fs,   GL_FRAGMENT_SHADER);
    bg_program = glCreateProgram();
    glAttachShader(bg_program, vs); glAttachShader(bg_program, fs);
    glLinkProgram(bg_program);

    GLuint tvs   = compile_shader(text_vs, GL_VERTEX_SHADER);
    GLuint tfs   = compile_shader(text_fs, GL_FRAGMENT_SHADER);
    text_program = glCreateProgram();
    glAttachShader(text_program, tvs); glAttachShader(text_program, tfs);
    glLinkProgram(text_program);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    init_rect_geometry();
    init_font();

    // Load CSS then HTML (file or built-in demo)
    char* css_str = read_file(g_css_path);
    if (css_str) { parse_css(css_str); free(css_str); }
    else if (!g_desktop_mode) { parse_css(default_css); }

    char* html_str = read_file(g_layout_path);
    if (html_str) { parse_html(html_str); free(html_str); }
    else if (!g_desktop_mode) { parse_html(default_html); }

    // Wire up callbacks
    int btn_close = get_element_by_id("btn_close");
    if (btn_close != -1) set_on_click(btn_close, handle_close);

    int btn_apply = get_element_by_id("apply_btn");
    if (btn_apply != -1) set_on_click(btn_apply, btn_click);

    int toggle_track = get_element_by_id("toggle_knob_track");
    if (toggle_track != -1) set_on_click(toggle_track, toggle_click);

    int checkbox = get_element_by_id("checkbox_notify");
    if (checkbox != -1) set_on_click(checkbox, checkbox_click);

    int toast_close = get_element_by_id("toast_close");
    if (toast_close != -1) set_on_click(toast_close, toast_dismiss);

    int modal_cancel = get_element_by_id("modal_cancel");
    if (modal_cancel != -1) set_on_click(modal_cancel, modal_cancel_click);

    int modal_confirm = get_element_by_id("modal_confirm");
    if (modal_confirm != -1) set_on_click(modal_confirm, modal_confirm_click);

    g_progress_fill_idx = get_element_by_id("progress_fill");
    g_toast_idx         = get_element_by_id("toast");
    g_modal_overlay_idx = get_element_by_id("modal_overlay");

    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    // Build initial render order
    for (int i = 0; i < elem_count; i++) render_order[i] = i;

    double last_time = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        double dt  = now - last_time;
        last_time  = now;

        // Shader resolution = window (logical) coords → matches CSS/mouse coords.
        // Viewport = framebuffer (physical) pixels → correct on HiDPI too.
        int ww, wh, fbw, fbh;
        glfwGetWindowSize(window, &ww, &wh);
        glfwGetFramebufferSize(window, &fbw, &fbh);
        window_width  = (float)ww;
        window_height = (float)wh;
        glViewport(0, 0, fbw, fbh);

        glClearColor(0.84f, 0.87f, 0.92f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        update_layout();
        update_animations(dt);

        // Animate progress bar
        if (g_progress_fill_idx != -1) {
            float progress = fmodf((float)now * 0.09f, 1.0f);
            int track = get_element_by_id("progress_track");
            float track_w = (track != -1) ? elements[track].w : 400.0f;
            elements[g_progress_fill_idx].w = track_w * progress;
        }

        build_render_order();

        // --- Shadow pass ---
        for (int ri = 0; ri < elem_count; ri++) {
            int i = render_order[ri];
            Element* e = &elements[i];
            if (!is_visible(i) || !e->has_shadow || e->sh_a <= 0.0f) continue;
            float spreads[3] = { e->sh_blur * 0.33f, e->sh_blur * 0.66f, e->sh_blur };
            float alphas[3]  = { e->sh_a, e->sh_a * 0.55f, e->sh_a * 0.30f };
            for (int L = 0; L < 3; L++) {
                float sp = spreads[L];
                draw_rect(e->x + e->sh_dx - sp,
                          e->y + e->sh_dy - sp * 0.25f,
                          e->w + sp * 2.0f, e->h + sp * 2.0f,
                          e->sh_r, e->sh_g, e->sh_b, alphas[L] * e->opacity,
                          e->border_radius + sp, 0, 0, 0, 0, 0);
            }
        }

        // --- Element pass ---
        for (int ri = 0; ri < elem_count; ri++) {
            int i = render_order[ri];
            Element* e = &elements[i];
            if (!is_visible(i)) continue;

            float scale = e->cur_scale;
            float dw = e->w * scale, dh = e->h * scale;
            float dx = e->x + (e->w - dw) * 0.5f;
            float dy = e->y + (e->h - dh) * 0.5f;

            draw_rect_full(dx, dy, dw, dh,
                           e->cur_r, e->cur_g, e->cur_b, e->cur_a * e->opacity,
                           e->border_radius * scale, e->border_width,
                           e->cur_bd_r, e->cur_bd_g, e->cur_bd_b, e->cur_bd_a * e->opacity,
                           e->has_gradient,
                           e->grad_r1, e->grad_g1, e->grad_b1, e->grad_a1,
                           e->grad_r2, e->grad_g2, e->grad_b2, e->grad_a2,
                           e->grad_angle);
        }

        // --- Text pass ---
        for (int ri = 0; ri < elem_count; ri++) {
            int i = render_order[ri];
            Element* e = &elements[i];
            if (!is_visible(i) || !e->text[0]) continue;
            float box_w = e->w - e->padding * 2.0f;
            float box_h = e->h - e->padding * 2.0f;
            render_text(e->text,
                        e->x + e->padding, e->y + e->padding,
                        box_w, box_h, e->text_align,
                        e->t_r, e->t_g, e->t_b, e->t_a * e->opacity,
                        e->font_size, e->font_bold);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
