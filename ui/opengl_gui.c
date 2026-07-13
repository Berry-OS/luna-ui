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
#include <strings.h>
#include <ctype.h>
#include <math.h>
#include <time.h>
#include <dirent.h>
#include <sys/stat.h>
#include <GLFW/glfw3.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_FAILURE_STRINGS
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#define CSS_PARSER_IMPLEMENTATION
#include "cssparser.h"

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

// uGradient: 0=solid, 1=linear, 2=radial
// Up to 4 color stops via uGradStopCount, uGradColors[], uGradStops[]
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
    "uniform int uGradStopCount;\n"
    "uniform vec4 uGradColors[4];\n"
    "uniform float uGradStops[4];\n"
    "uniform float uGradAngle;\n"
    "uniform vec2 uGradCenter;\n"
    "uniform float uGradRadius;\n"
    "vec4 sampleGradient(float t) {\n"
    "    t = clamp(t, 0.0, 1.0);\n"
    "    if(uGradStopCount <= 1) return uGradColors[0];\n"
    "    if(t <= uGradStops[0]) return uGradColors[0];\n"
    "    for(int i = 0; i < 3; i++) {\n"
    "        if(i + 1 >= uGradStopCount) break;\n"
    "        float a = uGradStops[i];\n"
    "        float b = uGradStops[i + 1];\n"
    "        if(t >= a && t <= b) {\n"
    "            float u = (b > a) ? (t - a) / (b - a) : 0.0;\n"
    "            return mix(uGradColors[i], uGradColors[i + 1], u);\n"
    "        }\n"
    "    }\n"
    "    return uGradColors[uGradStopCount - 1];\n"
    "}\n"
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
    "        float t = dot(uv - 0.5, vec2(sa, -ca)) + 0.5;\n"
    "        baseColor = sampleGradient(t);\n"
    "    } else if(uGradient == 2) {\n"
    "        vec2 c = uGradCenter * uSize;\n"
    "        float radius = max(uGradRadius * max(uSize.x, uSize.y), 0.001);\n"
    "        float t = distance(FragPos, c) / radius;\n"
    "        baseColor = sampleGradient(t);\n"
    "    } else {\n"
    "        baseColor = uColor;\n"
    "    }\n"
    "    float bw = uBorderWidth;\n"
    "    float borderMix = (bw > 0.01) ? smoothstep(r - bw - 1.0, r - bw + 0.5, dist) : 0.0;\n"
    "    vec4 finalColor = mix(baseColor, uBorderColor, borderMix);\n"
    "    FragColor = vec4(finalColor.rgb, finalColor.a * alpha);\n"
    "}\0";

// CSS box-shadow: Gaussian soft shadow via SDF of the rounded rect.
// uShadowInset: top-left of actual element within the (padded) shadow rect (no offset).
// uElemSize:    actual element size.
// uOffset:      shadow offset (sh_dx, sh_dy) in FragPos space (x right, y down-flipped).
const char* shadow_fs =
    "#version 330 core\n"
    "in vec2 FragPos;\n"
    "out vec4 FragColor;\n"
    "uniform vec4 uShadowColor;\n"
    "uniform vec2 uShadowInset;\n"
    "uniform vec2 uElemSize;\n"
    "uniform vec2 uOffset;\n"
    "uniform vec2 uSize;\n"
    "uniform float uRadius;\n"
    "uniform float uBlur;\n"
    // Accurate erf approximation (A&S §7.1.26, max error < 1.5e-7)
    "float erf_approx(float x) {\n"
    "    float sign_x = x >= 0.0 ? 1.0 : -1.0;\n"
    "    float ax = abs(x);\n"
    "    float t = 1.0 / (1.0 + 0.3275911 * ax);\n"
    "    float y = 1.0 - t*(0.254829592 + t*(-0.284496736 + t*(1.421413741\n"
    "              + t*(-1.453152027 + t*1.061405429)))) * exp(-ax*ax);\n"
    "    return sign_x * y;\n"
    "}\n"
    "float rr_sdf(vec2 p, vec2 hs, float r) {\n"
    "    vec2 q = abs(p) - hs + vec2(r);\n"
    "    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;\n"
    "}\n"
    "void main() {\n"
    "    float r = clamp(uRadius, 0.0, min(uElemSize.x, uElemSize.y) * 0.5);\n"
    "    vec2 hs = uElemSize * 0.5;\n"
    "    // Shadow shape SDF (shifted by offset)\n"
    "    vec2 posShadow = FragPos - uShadowInset - uOffset;\n"
    "    float distShadow = rr_sdf(posShadow - hs, hs, r);\n"
    "    float sigma = max(uBlur * 0.5, 0.001);\n"
    "    float alpha = 0.5 - 0.5 * erf_approx(distShadow / (sigma * 1.41421356));\n"
    "    // Clip shadow inside the element's own footprint to prevent dark bleed\n"
    "    // at transparent rounded corners.  posElem anchors to the element's\n"
    "    // bottom-left corner in FragPos space (fragpos.y = 0 at rect bottom):\n"
    "    //   elem_fp_y = uSize.y - uShadowInset.y - uElemSize.y  (= bot_pad)\n"
    "    //   This compensates for sh_dy baked into the rect height.\n"
    "    vec2 posElem = vec2(FragPos.x - uShadowInset.x,\n"
    "                        FragPos.y - (uSize.y - uShadowInset.y - uElemSize.y));\n"
    "    float distElem = rr_sdf(posElem - hs, hs, r);\n"
    "    alpha *= smoothstep(-1.0, 0.0, distElem);\n"
    "    if (alpha < 0.004) discard;\n"
    "    FragColor = vec4(uShadowColor.rgb, uShadowColor.a * alpha);\n"
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

// Image shader: renders a texture cropped to a rounded rect.
// Uses same vertex shader as bg (bg_vs). FragPos.y is flipped (0=bottom, uSize.y=top).
// stb_image is loaded with flip_vertically so UV = FragPos/uSize maps correctly.
const char* img_fs =
    "#version 330 core\n"
    "in vec2 FragPos;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D uImage;\n"
    "uniform vec2 uSize;\n"
    "uniform float uRadius;\n"
    "uniform float uAlpha;\n"
    "void main() {\n"
    "    vec2 halfSize = uSize / 2.0;\n"
    "    float r = max(uRadius, 0.001);\n"
    "    vec2 d = abs(FragPos - halfSize) - halfSize + vec2(r);\n"
    "    float dist = length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);\n"
    "    float alpha = 1.0 - smoothstep(r - 1.0, r + 0.5, dist);\n"
    "    if(alpha <= 0.0) discard;\n"
    "    vec2 uv = FragPos / uSize;\n"
    "    vec4 tc = texture(uImage, uv);\n"
    "    FragColor = vec4(tc.rgb, tc.a * alpha * uAlpha);\n"
    "}\0";

#define MAX_GRAD_STOPS 4
#define GRAD_NONE   0
#define GRAD_LINEAR 1
#define GRAD_RADIAL 2

#define FLEX_DIR_ROW    0
#define FLEX_DIR_COLUMN 1
#define FLEX_JUSTIFY_START         0
#define FLEX_JUSTIFY_CENTER        1
#define FLEX_JUSTIFY_END           2
#define FLEX_JUSTIFY_SPACE_BETWEEN 3
#define FLEX_ALIGN_START   0
#define FLEX_ALIGN_CENTER  1
#define FLEX_ALIGN_END     2
#define FLEX_ALIGN_STRETCH 3
#define FLEX_ALIGN_SPACE_BETWEEN 4
#define FLEX_ALIGN_SPACE_AROUND  5

#define GRID_AUTO_FLOW_ROW     0
#define GRID_AUTO_FLOW_COLUMN  1
#define GRID_AUTO_FLOW_DENSE   2

#define DISPLAY_BLOCK 0
#define DISPLAY_NONE  1
#define DISPLAY_FLEX  2
#define DISPLAY_GRID  3

#define FLEX_WRAP_NOWRAP 0
#define FLEX_WRAP_WRAP   1

#define ALIGN_SELF_AUTO  -1

#define BOX_CONTENT 0
#define BOX_BORDER  1

#define MAX_GRID_TRACKS 8
#define MAX_GRID_AREA_ROWS 8
#define MAX_GRID_AREA_COLS 8
#define MAX_GRID_AREAS   16

#define GRID_TRACK_PX     0
#define GRID_TRACK_FR     1
#define GRID_TRACK_MINMAX 2

#define OVERFLOW_VISIBLE 0
#define OVERFLOW_HIDDEN  1
#define OVERFLOW_AUTO    2
#define OVERFLOW_SCROLL  3

typedef struct {
    char name[32];
    int col, row, col_span, row_span;
} GridAreaRect;

struct Element;
typedef void (*EventHandler)(struct Element* e);

typedef struct Element {
    int id_idx;
    int parent_idx;
    float rel_x, rel_y;
    float x, y, w, h;
    char text[256], type[32], class_name[96], id[64];
    int is_hovered, is_active, is_draggable;
    int drag_mode; // 0=none, 1=move parent window, 2=drag self (slider thumb)

    int pct_w, pct_h, pct_left, pct_top, pct_bottom, pct_right;
    float raw_w, raw_h, raw_left, raw_top, raw_bottom, raw_right;
    float raw_w_off, raw_h_off, raw_left_off, raw_top_off;
    int has_bottom, has_right;
    int has_top, has_left;
    float bottom_val, right_val;
    int pos_overridden_x, pos_overridden_y;
    int position_fixed;
    int position_sticky;
    int position_mode; /* 0 unset 1 static 2 relative 3 absolute */
    float sticky_top;
    int sticky_use_top;
    int sticky_use_bottom;
    float sticky_bottom;
    int sticky_use_left;
    float sticky_left;
    int sticky_use_right;
    float sticky_right;

    int inert;
    int tabindex; /* -2=unset -1=skip 0+=order */
    char aria_label[128];
    char role[32];
    int aria_live; /* 0=off/unset 1=polite 2=assertive */
    int aria_hidden;
    int aria_expanded; /* -1=unset 0=false 1=true */

    float scroll_margin_top, scroll_margin_right, scroll_margin_bottom, scroll_margin_left;
    float scroll_padding_top, scroll_padding_right, scroll_padding_bottom, scroll_padding_left;

    float r, g, b, a;
    float t_r, t_g, t_b, t_a;
    float bd_r, bd_g, bd_b, bd_a;

    float cur_r, cur_g, cur_b, cur_a;
    float cur_bd_r, cur_bd_g, cur_bd_b, cur_bd_a;
    float cur_scale;

    float border_radius;
    float border_width;
    float outline_width;
    float outline_offset;
    float ol_r, ol_g, ol_b, ol_a;
    int has_outline;
    float padding;                       /* legacy uniform value (== pad_t) */
    float pad_t, pad_r, pad_b, pad_l;    /* resolved per-side padding */
    float margin_top, margin_right, margin_bottom, margin_left;

    int margin_top_auto, margin_right_auto, margin_bottom_auto, margin_left_auto;

    float opacity;
    int display_none;
    int cursor_pointer;
    int cursor_type; // 0=default 1=pointer 2=text 3=crosshair 4=ew-resize 5=ns-resize
    int text_align;
    int font_size;
    int font_bold;
    float line_height;
    int white_space;    /* 0=normal 1=nowrap */
    int text_overflow;  /* 0=clip 1=ellipsis */
    int overflow_wrap;  /* 0=normal 1=break-word */

    int has_shadow;
    float sh_dx, sh_dy, sh_blur;
    float sh_r, sh_g, sh_b, sh_a;

    // Gradient: type 0=none 1=linear 2=radial
    int has_gradient;
    int grad_type;
    int grad_stop_count;
    float grad_stop_pos[MAX_GRAD_STOPS];
    float grad_stop_r[MAX_GRAD_STOPS], grad_stop_g[MAX_GRAD_STOPS];
    float grad_stop_b[MAX_GRAD_STOPS], grad_stop_a[MAX_GRAD_STOPS];
    float grad_angle;
    float grad_rad_cx, grad_rad_cy, grad_rad_r;

    int display_mode; // 0 block 1 none 2 flex 3 grid
    int flex_direction;
    int justify_content;
    int align_items;
    int justify_items;
    int align_content;
    int flex_wrap;
    int align_self;   // ALIGN_SELF_AUTO or FLEX_ALIGN_*
    int justify_self;
    float flex_gap;
    int flex_grow;
    int flex_shrink;
    float flex_basis;
    int has_flex_basis;
    int flex_basis_auto;
    int flex_child;

    int box_sizing;
    float css_width, css_height;
    float css_min_width, css_min_height;
    float css_max_width, css_max_height;
    int has_css_width, has_css_height;
    int has_min_width, has_min_height;
    int has_max_width, has_max_height;

    int grid_col_count, grid_row_count;
    float grid_col_track[MAX_GRID_TRACKS];
    int   grid_col_type[MAX_GRID_TRACKS];
    float grid_col_min[MAX_GRID_TRACKS];
    float grid_row_track[MAX_GRID_TRACKS];
    int   grid_row_type[MAX_GRID_TRACKS];
    float grid_row_min[MAX_GRID_TRACKS];
    float grid_col_gap, grid_row_gap;
    int grid_auto_flow;
    float grid_auto_row_track, grid_auto_col_track;
    int   grid_auto_row_type, grid_auto_col_type;
    float grid_auto_row_min, grid_auto_col_min;
    int has_grid_auto_rows, has_grid_auto_columns;

    int grid_area_rows, grid_area_cols;
    char grid_area_cell[MAX_GRID_AREA_ROWS][MAX_GRID_AREA_COLS][32];
    GridAreaRect grid_area_rects[MAX_GRID_AREAS];
    int grid_area_rect_count;

    int grid_col, grid_row;
    int grid_col_span, grid_row_span;
    int has_grid_col, has_grid_row;
    char grid_area_name[32];
    int has_grid_area;
    int grid_child;
    int flow_child;

    int overflow_x, overflow_y;
    float scroll_top, scroll_left;
    float scroll_dest_top, scroll_dest_left;
    float scroll_content_h, scroll_content_w;
    int scroll_smooth;
    int scroll_snap_type; /* 0=none 1=y mandatory 2=y proximity */
    int scroll_snap_align; /* 0=start 1=center 2=end */
    float scrollbar_width;
    int has_scrollbar_width;
    float sb_track_r, sb_track_g, sb_track_b, sb_track_a;
    float sb_thumb_r, sb_thumb_g, sb_thumb_b, sb_thumb_a;
    int has_scrollbar_color;

    int css_positioned; // 1=left/right set, 2=top/bottom set

    int z_index;
    int visibility_hidden;
    float transform_scale;
    float transform_tx, transform_ty;
    float raw_transform_tx, raw_transform_ty;
    int transform_tx_pct, transform_ty_pct;
    float cur_tx, cur_ty;
    float anim_speed;
    int pointer_events_none;

    int has_custom_bg, has_custom_color, has_custom_text, has_custom_border;
    EventHandler on_click;
    char onclick[64];
    char data_tab[32];
    char inline_style[256];
    int has_inline_style;

    /* CSS @keyframes animation state */
    char anim_name[64];
    float anim_duration;
    float anim_delay;
    int anim_infinite;
    int anim_alternate;
    int anim_easing; /* 0=linear 1=ease-in-out */
    int has_css_animation;
    double anim_start_time;
    int anim_override_layout;
    float anim_base_w;
    float anim_base_left;
    int anim_base_w_pct;
    int anim_base_left_pct;
    int anim_base_captured;

    int has_bg_image;
    char bg_image_path[256];
    GLuint bg_image_tex;
} Element;

// --- CSS Rule ---

#define MAX_SEL_CLASSES   4
#define MAX_SEL_ANCESTORS 3

typedef struct {
    char sel_type[32];
    char sel_id[64];
    char sel_classes[MAX_SEL_CLASSES][64];
    int  sel_class_count;
    int  is_universal;
} SimpleSelector;

typedef struct {
    char selector[128];
    SimpleSelector target;
    SimpleSelector ancestors[MAX_SEL_ANCESTORS];
    int ancestor_count;
    int specificity, is_hover, is_active, is_focus, is_focus_visible, is_focus_within;

    int has_bg;     float bg_r, bg_g, bg_b, bg_a;
    int has_color;  float c_r,  c_g,  c_b,  c_a;
    int has_border; float bd_r, bd_g, bd_b, bd_a; float border_width;
    int has_outline; float outline_width, outline_offset;
    float ol_r, ol_g, ol_b, ol_a;
    int has_radius; float border_radius;
    int has_width;  float width;
    int has_height; float height;
    int has_padding; float padding;
    float pad_t, pad_r, pad_b, pad_l;
    int has_margin; float margin_top, margin_right, margin_bottom, margin_left;
    int margin_top_auto, margin_right_auto, margin_bottom_auto, margin_left_auto;
    int has_left;   float left;
    int has_top;    float top;
    int has_bottom; float bottom;
    int has_right;  float right;
    int pct_bottom; float raw_bottom;
    int pct_right;  float raw_right;
    int has_position; int position_fixed; int position_sticky;
    int position_mode; /* 0 unset 1 static 2 relative 3 absolute */

    int has_opacity; float opacity;
    int has_cursor;  int cursor_pointer; int cursor_type;
    int has_display; int display_none;
    int display_mode; // 0 block 1 none 2 flex 3 grid
    int has_flex_direction; int flex_direction;
    int has_justify_content; int justify_content;
    int has_align_items; int align_items;
    int has_justify_items; int justify_items;
    int has_align_content; int align_content;
    int has_flex_wrap; int flex_wrap;
    int has_align_self; int align_self;
    int has_justify_self; int justify_self;
    int has_gap; float flex_gap;
    int has_flex_grow; int flex_grow;
    int has_flex_shrink; int flex_shrink;
    int has_flex_basis; float flex_basis; int flex_basis_auto;
    int has_min_width; float min_width;
    int has_min_height; float min_height;
    int has_max_width; float max_width;
    int has_max_height; float max_height;
    int has_box_sizing; int box_sizing;
    int has_grid_template_columns;
    float grid_col_track[MAX_GRID_TRACKS];
    int   grid_col_type[MAX_GRID_TRACKS];
    float grid_col_min[MAX_GRID_TRACKS];
    int   grid_col_count;
    int has_grid_template_rows;
    float grid_row_track[MAX_GRID_TRACKS];
    int   grid_row_type[MAX_GRID_TRACKS];
    float grid_row_min[MAX_GRID_TRACKS];
    int   grid_row_count;
    int has_grid_template_areas;
    int grid_area_rows, grid_area_cols;
    char grid_area_cell[MAX_GRID_AREA_ROWS][MAX_GRID_AREA_COLS][32];
    int has_column_gap; float grid_col_gap;
    int has_row_gap; float grid_row_gap;
    int has_grid_auto_flow; int grid_auto_flow;
    int has_grid_auto_rows;
    float grid_auto_row_track;
    int   grid_auto_row_type;
    float grid_auto_row_min;
    int has_grid_auto_columns;
    float grid_auto_col_track;
    int   grid_auto_col_type;
    float grid_auto_col_min;
    int has_grid_column; int grid_col;
    int has_grid_row; int grid_row;
    int has_grid_column_span; int grid_col_span;
    int has_grid_row_span; int grid_row_span;
    int has_grid_area; char grid_area_name[32];
    int has_overflow_x, overflow_x;
    int has_overflow_y, overflow_y;
    int has_scrollbar_width; float scrollbar_width;
    int has_scrollbar_color;
    float sb_thumb_r, sb_thumb_g, sb_thumb_b, sb_thumb_a;
    float sb_track_r, sb_track_g, sb_track_b, sb_track_a;
    int has_scroll_behavior; int scroll_smooth;
    int has_scroll_snap_type; int scroll_snap_type;
    int has_scroll_snap_align; int scroll_snap_align;
    int has_scroll_margin;
    float scroll_margin_top, scroll_margin_right, scroll_margin_bottom, scroll_margin_left;
    int has_scroll_padding;
    float scroll_padding_top, scroll_padding_right, scroll_padding_bottom, scroll_padding_left;
    int has_text_align; int text_align;
    int has_font_size;  int font_size;
    int has_font_weight; int font_bold;
    int has_line_height; float line_height;
    int has_white_space; int white_space;
    int has_text_overflow; int text_overflow;
    int has_overflow_wrap; int overflow_wrap;
    int has_shadow; float sh_dx, sh_dy, sh_blur, sh_r, sh_g, sh_b, sh_a;

    // Gradient
    int has_gradient;
    int grad_type;
    int grad_stop_count;
    float grad_stop_pos[MAX_GRAD_STOPS];
    float grad_stop_r[MAX_GRAD_STOPS], grad_stop_g[MAX_GRAD_STOPS];
    float grad_stop_b[MAX_GRAD_STOPS], grad_stop_a[MAX_GRAD_STOPS];
    float grad_angle;
    float grad_rad_cx, grad_rad_cy, grad_rad_r;

    int has_z_index; int z_index;

    int pct_w; float raw_w; float raw_w_off;
    int pct_h; float raw_h; float raw_h_off;
    int pct_left; float raw_left; float raw_left_off;
    int pct_top; float raw_top; float raw_top_off;

    int has_visibility; int visibility_hidden;
    int has_transform; float transform_scale;
    int has_transform_tx; float transform_tx;
    int has_transform_ty; float transform_ty;
    float raw_transform_tx, raw_transform_ty;
    int transform_tx_pct, transform_ty_pct;
    int has_transition; float transition_duration;
    int has_pointer_events; int pointer_events_none;

    int has_animation;
    char anim_name[64];
    float anim_duration;
    float anim_delay;
    int anim_infinite;
    int anim_alternate;
    int anim_easing;

    int has_bg_image;
    char bg_image_path[256];
} StyleRule;

#define MAX_ELEMENTS 500
#define MAX_RULES    600
#define MAX_KF_ANIMS 48
#define MAX_KF_STOPS 8
#define MAX_JS_HANDLERS 64

typedef struct {
    float position;
    int has_width; float width; int width_pct;
    int has_left; float left; int left_pct;
    int has_opacity; float opacity;
    int has_transform_scale; float transform_scale;
    int has_transform_tx; float transform_tx;
    int has_transform_ty; float transform_ty;
} KeyframeStop;

typedef struct {
    char name[64];
    KeyframeStop stops[MAX_KF_STOPS];
    int stop_count;
} CssKeyframe;

typedef struct {
    const char* name;
    EventHandler fn;
} JsHandlerEntry;

Element  elements[MAX_ELEMENTS]; int elem_count = 0;
StyleRule  css_rules[MAX_RULES];   int rule_count = 0;
CssKeyframe g_keyframes[MAX_KF_ANIMS];
int g_keyframe_count = 0;
JsHandlerEntry g_js_handlers[MAX_JS_HANDLERS];
int g_js_handler_count = 0;


static int render_order[MAX_ELEMENTS];

GLuint bg_program, text_program, shadow_program, img_program;
float  window_width = 1024.0f, window_height = 768.0f;

// Cached uniform locations — queried once after program link, then reused every frame.
static struct {
    GLint uResolution, uPos, uSize, uColor, uBorderColor, uBorderWidth, uRadius;
    GLint uGradient, uGradStopCount, uGradAngle, uGradCenter, uGradRadius;
    GLint uGradColors[MAX_GRAD_STOPS], uGradStops[MAX_GRAD_STOPS];
} bg_loc;
static struct {
    GLint uResolution, uPos, uSize;
    GLint uShadowColor, uElemSize, uRadius, uBlur, uShadowInset, uOffset;
} sh_loc;
static struct {
    GLint uResolution, textColor;
} tx_loc;
static struct {
    GLint uResolution, uPos, uSize, uRadius, uAlpha, uImage;
} img_loc;

// Texture cache — path → GL texture ID (loaded once, reused)
#define MAX_TEXTURES 64
typedef struct { char path[512]; GLuint tex; } TexEntry;
static TexEntry g_tex_cache[MAX_TEXTURES];
static int g_tex_count = 0;
GLFWwindow* g_window = NULL;
static int g_desktop_mode = 0;
static int g_fullscreen   = 0;
static const char* g_layout_path = "ui/demo_layout.html";
static const char* g_css_path    = "ui/demo_style.css";
static char g_html_base_dir[512] = "ui";
static char g_doc_title[128] = "Vespera GUI Engine";
static int  g_css_from_document = 0;
int   drag_target_idx = -1;
float drag_offset_x   = 0, drag_offset_y = 0;
static int    g_scroll_drag_idx = -1;
static int    g_scroll_drag_axis = 0; /* 0=vertical 1=horizontal */
static float  g_scroll_drag_off = 0.0f;
static int    g_scroll_hover_idx = -1;
static int    g_scroll_hover_axis = -1; /* -1=none 0=vertical 1=horizontal */
static int    g_drag_moved = 0;
static int    g_drag_mode  = 0;
static double g_press_x = 0, g_press_y = 0;
static int    g_focused_idx = -1;
static int    g_focused_element_idx = -1;
static int    g_focus_before_trap = -1;
static int    g_focus_via_keyboard = 0;
static char   g_a11y_live_msg[256];
static double g_a11y_live_until = 0.0;
static int    g_a11y_live_assertive = 0;
static int    g_top_z = 100;
#define DRAG_THRESHOLD 4.0

GLFWcursor* g_hand_cursor     = NULL;
GLFWcursor* g_cursor_ibeam      = NULL;
GLFWcursor* g_cursor_crosshair  = NULL;
GLFWcursor* g_cursor_hresize    = NULL;
GLFWcursor* g_cursor_vresize    = NULL;
int         g_current_cursor    = -1;

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

int g_toast_idx          = -1;
int g_modal_overlay_idx  = -1;
int g_info_win_idx       = -1;
int g_select_panel_idx   = -1;
int g_brightness_thumb_idx  = -1;
int g_brightness_track_idx  = -1;
int g_brightness_fill_idx   = -1;
int g_brightness_value_idx  = -1;
int g_clock_idx             = -1;
static double g_last_clock_update = 0.0;
static char g_screenshot_path[512] = {0};
static int g_screenshot_pending = 0;
static int g_layout_dirty = 1;
static float g_last_window_w = 0.0f, g_last_window_h = 0.0f;

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

    if (strcmp(val, "none") == 0) {
        *r = *g = *b = 0.0f; *a = 0.0f;
        return;
    }

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
    } else if (strncmp(val, "hsla(", 5) == 0 || strncmp(val, "hsl(", 4) == 0) {
        const char* paren = strchr(val, '(');
        if (paren) {
            char buf[64] = {0};
            strncpy(buf, paren + 1, sizeof(buf) - 1);
            float h = 0.0f, s = 0.0f, l = 0.0f, alpha = 1.0f;
            int idx = 0;
            char* tok = strtok(buf, ", )");
            while (tok && idx < 4) {
                float v = (float)atof(tok);
                if (idx == 0) h = v;
                else if (idx == 1) s = v;
                else if (idx == 2) l = v;
                else alpha = v;
                idx++;
                tok = strtok(NULL, ", )");
            }
            if (idx >= 3) {
                s /= 100.0f;
                l /= 100.0f;
                if (s < 0.0f) s = 0.0f;
                if (s > 1.0f) s = 1.0f;
                if (l < 0.0f) l = 0.0f;
                if (l > 1.0f) l = 1.0f;
                float c = (1.0f - fabsf(2.0f * l - 1.0f)) * s;
                float x = c * (1.0f - fabsf(fmodf(h / 60.0f, 2.0f) - 1.0f));
                float m = l - c * 0.5f;
                float rr = 0.0f, gg = 0.0f, bb = 0.0f;
                if      (h < 60.0f)  { rr = c; gg = x; }
                else if (h < 120.0f) { rr = x; gg = c; }
                else if (h < 180.0f) { gg = c; bb = x; }
                else if (h < 240.0f) { gg = x; bb = c; }
                else if (h < 300.0f) { rr = x; bb = c; }
                else                 { rr = c; bb = x; }
                *r = rr + m;
                *g = gg + m;
                *b = bb + m;
                *a = (idx == 4) ? alpha : 1.0f;
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

static const char* skip_css_unit(const char* p) {
    if (strncmp(p, "rem", 3) == 0) return p + 3;
    if (strncmp(p, "px", 2) == 0 || strncmp(p, "em", 2) == 0 ||
        strncmp(p, "vh", 2) == 0 || strncmp(p, "vw", 2) == 0 ||
        strncmp(p, "pt", 2) == 0 || strncmp(p, "vmin", 4) == 0) return p + 2;
    if (*p == '%') return p + 1;
    return p;
}

static float read_css_length(const char** pp) {
    char* endp;
    float v = strtof(*pp, &endp);
    *pp = skip_css_unit(endp);
    while (isspace((unsigned char)**pp)) (*pp)++;
    return v;
}

// Parse length with optional % (stored as 0.0-1.0 ratio when pct).
static void parse_length(const char* val, float* out_num, int* out_pct) {
    char buf[64] = {0};
    strncpy(buf, val, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    /* strip leading whitespace */
    char* st = buf; while (isspace((unsigned char)*st)) st++;
    if (st != buf) memmove(buf, st, strlen(st) + 1);
    int len = (int)strlen(buf);
    while (len > 0 && isspace((unsigned char)buf[len-1])) buf[--len] = '\0';
    *out_pct = 0;
    *out_num = 0.0f;
    if (len > 0 && buf[len-1] == '%') { *out_pct = 1; buf[--len] = '\0'; }
    if (*out_pct) { sscanf(buf, "%f", out_num); *out_num /= 100.0f; return; }
    sscanf(buf, "%f", out_num);
}

/* Parse length including calc(X% +/- Ypx). Returns 1 if calc was parsed. */
static int parse_length_calc(const char* val, float* out_num, int* out_pct, float* out_offset) {
    *out_offset = 0.0f;
    char buf[128] = {0};
    strncpy(buf, val, sizeof(buf) - 1);
    /* strip whitespace */
    char* s = buf; while (isspace((unsigned char)*s)) s++;
    if (s != buf) memmove(buf, s, strlen(s) + 1);
    int bl = (int)strlen(buf);
    while (bl > 0 && isspace((unsigned char)buf[bl-1])) buf[--bl] = '\0';
    if (strncmp(buf, "calc(", 5) == 0) {
        /* find the inner expression */
        const char* inner = buf + 5;
        /* trim trailing ) */
        char expr[96] = {0}; strncpy(expr, inner, sizeof(expr) - 1);
        int el = (int)strlen(expr);
        while (el > 0 && (expr[el-1] == ')' || isspace((unsigned char)expr[el-1]))) expr[--el] = '\0';
        /* parse: X% op Ypx */
        char* endp = NULL;
        float v1 = strtof(expr, &endp);
        if (endp && *endp == '%') {
            *out_pct = 1; *out_num = v1 / 100.0f;
            endp++;
            while (isspace((unsigned char)*endp)) endp++;
            char op = *endp; if (op == '+' || op == '-') endp++;
            while (isspace((unsigned char)*endp)) endp++;
            float v2 = strtof(endp, NULL);
            *out_offset = (op == '-') ? -v2 : v2;
            return 1;
        } else if (endp) {
            /* Npx op X% */
            float px_part = v1;
            while (isspace((unsigned char)*endp)) endp++;
            char op = *endp; if (op == '+' || op == '-') endp++;
            while (isspace((unsigned char)*endp)) endp++;
            float v2 = strtof(endp, &endp);
            if (endp && *endp == '%') {
                *out_pct = 1; *out_num = v2 / 100.0f;
                *out_offset = (op == '-') ? -px_part : px_part;
                return 1;
            }
            /* both px */
            *out_pct = 0; *out_num = (op == '-') ? px_part - v2 : px_part + v2;
            return 1;
        }
        *out_pct = 0; *out_num = 0.0f; return 1;
    }
    /* not calc - use regular parse */
    parse_length(val, out_num, out_pct);
    return 0;
}


// Parse box-shadow: multi-shadow, pick first non-inset shadow.
static void parse_box_shadow(const char* val, StyleRule* rule) {
    const char* p = val;
    int found_noninset = 0;
    while (*p && !found_noninset) {
        while (isspace((unsigned char)*p)) p++;
        if (!*p) break;
        /* Find end of this shadow entry (comma at depth 0) */
        const char* shadow_start = p;
        int depth = 0;
        while (*p) {
            if (*p == '(') depth++;
            else if (*p == ')') depth--;
            else if (*p == ',' && depth == 0) break;
            p++;
        }
        char shadow_buf[256] = {0};
        int slen = (int)(p - shadow_start);
        if (slen > 255) slen = 255;
        strncpy(shadow_buf, shadow_start, slen);
        /* skip trailing comma */
        if (*p == ',') p++;

        /* check if this shadow contains "inset" keyword */
        char lower[256] = {0};
        for (int i = 0; i < slen; i++) lower[i] = (char)tolower((unsigned char)shadow_buf[i]);
        int is_inset = (strstr(lower, "inset") != NULL);
        if (is_inset) continue;

        /* Parse this non-inset shadow */
        const char* sp = shadow_buf;
        while (isspace((unsigned char)*sp)) sp++;
        /* skip "inset" if somehow at start */
        if (strncmp(sp, "inset", 5) == 0 && (isspace((unsigned char)sp[5]) || !sp[5]))
            { sp += 5; while (isspace((unsigned char)*sp)) sp++; }

        float dx   = read_css_length(&sp);
        float dy   = read_css_length(&sp);
        float blur = read_css_length(&sp);
        /* optional spread - skip if next is a number */
        if (*sp && (*sp == '-' || *sp == '+' || isdigit((unsigned char)*sp) || *sp == '.'))
            read_css_length(&sp);
        /* color */
        if (*sp && *sp != ',' && *sp != '\0') {
            char colbuf[128] = {0};
            const char* end = sp;
            int d2 = 0;
            while (*end) {
                if (*end == '(') d2++;
                else if (*end == ')') d2--;
                else if (*end == ',' && d2 == 0) break;
                end++;
            }
            int clen = (int)(end - sp);
            if (clen >= 127) clen = 127;
            strncpy(colbuf, sp, clen);
            /* strip trailing "inset" if present */
            char* ins = strstr(colbuf, " inset");
            if (ins) *ins = '\0';
            /* trim */
            int cl = (int)strlen(colbuf);
            while (cl > 0 && isspace((unsigned char)colbuf[cl-1])) colbuf[--cl] = '\0';
            if (colbuf[0]) {
                float cr = 0, cg = 0, cb = 0, ca = 1;
                parse_color(colbuf, &cr, &cg, &cb, &ca);
                rule->has_shadow = 1;
                rule->sh_dx = dx; rule->sh_dy = dy; rule->sh_blur = blur;
                rule->sh_r = cr; rule->sh_g = cg; rule->sh_b = cb; rule->sh_a = ca;
                found_noninset = 1;
            }
        }
    }
}

// Parse border shorthand: "1px solid #color" or "none" or "0"
static void parse_border_shorthand(const char* val, StyleRule* rule) {
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

// Copy gradient stops into rule/element
static void apply_gradient_rule(StyleRule* rule, int type, float angle,
                                float rcx, float rcy, float rr) {
    rule->has_gradient = 1;
    rule->grad_type = type;
    rule->grad_angle = angle;
    rule->grad_rad_cx = rcx;
    rule->grad_rad_cy = rcy;
    rule->grad_rad_r = rr;
    rule->has_bg = 1;
    if (rule->grad_stop_count > 0) {
        rule->bg_r = rule->grad_stop_r[0];
        rule->bg_g = rule->grad_stop_g[0];
        rule->bg_b = rule->grad_stop_b[0];
        rule->bg_a = rule->grad_stop_a[0];
    }
}

// Parse comma-separated color stops after gradient header
static int parse_gradient_stops(const char** p_in, StyleRule* rule) {
    const char* p = *p_in;
    int count = 0;
    while (*p && *p != ')' && count < MAX_GRAD_STOPS) {
        while (isspace((unsigned char)*p) || *p == ',') p++;
        if (*p == ')' || !*p) break;

        const char* start = p;
        int depth = 0;
        while (*p && !(*p == ',' && depth == 0)) {
            if (*p == '(') depth++;
            else if (*p == ')') { if (depth == 0) break; depth--; }
            p++;
        }
        char token[96] = {0};
        int len = (int)(p - start);
        if (len > 95) len = 95;
        strncpy(token, start, len);
        trim_whitespace(token);

        char color_buf[64] = {0};
        float pos = -1.0f;
        char* sp = strrchr(token, ' ');
        if (sp) {
            char posbuf[24] = {0};
            strncpy(posbuf, sp + 1, 23);
            trim_whitespace(posbuf);
            int plen = (int)strlen(posbuf);
            if (plen > 0 && (posbuf[plen - 1] == '%' || isdigit((unsigned char)posbuf[0]))) {
                if (posbuf[plen - 1] == '%') posbuf[plen - 1] = '\0';
                pos = (float)atof(posbuf) / 100.0f;
                int clen = (int)(sp - token);
                if (clen > 63) clen = 63;
                strncpy(color_buf, token, clen);
                color_buf[clen] = '\0';
                trim_whitespace(color_buf);
            } else {
                strncpy(color_buf, token, 63);
            }
        } else {
            strncpy(color_buf, token, 63);
        }

        parse_color(color_buf,
                    &rule->grad_stop_r[count], &rule->grad_stop_g[count],
                    &rule->grad_stop_b[count], &rule->grad_stop_a[count]);
        rule->grad_stop_pos[count] = pos;
        count++;
        if (*p == ',') p++;
    }
    rule->grad_stop_count = count;
    if (count >= 2) {
        int all_auto = 1;
        for (int i = 0; i < count; i++) {
            if (rule->grad_stop_pos[i] >= 0.0f) all_auto = 0;
        }
        for (int i = 0; i < count; i++) {
            if (rule->grad_stop_pos[i] < 0.0f || all_auto)
                rule->grad_stop_pos[i] = (float)i / (float)(count - 1);
        }
    }
    *p_in = p;
    return count;
}

// Parse linear-gradient(angle, stop1, stop2, ...)
static void parse_linear_gradient(const char* val, StyleRule* rule) {
    const char* p = strchr(val, '(');
    if (!p) return;
    p++;
    while (isspace((unsigned char)*p)) p++;

    float angle = 180.0f;
    if (strncmp(p, "to ", 3) == 0) {
        if (strstr(p, "top"))         angle = 0.0f;
        else if (strstr(p, "right"))  angle = 90.0f;
        else if (strstr(p, "bottom")) angle = 180.0f;
        else if (strstr(p, "left"))   angle = 270.0f;
        const char* comma = strchr(p, ',');
        if (!comma) return;
        p = comma + 1;
    } else if (isdigit((unsigned char)*p) || *p == '-' || *p == '.') {
        char* endp;
        angle = strtof(p, &endp);
        p = endp;
        if (strncmp(p, "deg", 3) == 0) p += 3;
        while (isspace((unsigned char)*p)) p++;
        if (*p == ',') p++;
    }
    while (isspace((unsigned char)*p)) p++;

    memset(rule->grad_stop_pos, 0, sizeof(rule->grad_stop_pos));
    parse_gradient_stops(&p, rule);
    if (rule->grad_stop_count < 2) return;

    apply_gradient_rule(rule, GRAD_LINEAR, angle * (float)M_PI / 180.0f, 0.5f, 0.5f, 0.75f);
}

// Parse radial-gradient(shape at cx cy, stops...)
static void parse_radial_gradient(const char* val, StyleRule* rule) {
    const char* p = strchr(val, '(');
    if (!p) return;
    p++;
    while (isspace((unsigned char)*p)) p++;

    float cx = 0.5f, cy = 0.5f, radius = 0.75f;
    if (strncmp(p, "circle", 6) == 0 || strncmp(p, "ellipse", 7) == 0) {
        while (*p && *p != ',' && *p != 'a') p++;
    }
    while (isspace((unsigned char)*p)) p++;
    if (strncmp(p, "at ", 3) == 0) {
        p += 3;
        if (strncmp(p, "center", 6) == 0) {
            cx = 0.5f; cy = 0.5f;
            p = strchr(p, ',');
            if (p) p++;
        } else {
            char* endp;
            float vx = strtof(p, &endp);
            p = endp;
            if (*p == '%') p++;
            while (isspace((unsigned char)*p)) p++;
            float vy = strtof(p, &endp);
            p = endp;
            if (*p == '%') p++;
            cx = vx / 100.0f;
            cy = vy / 100.0f;
            while (isspace((unsigned char)*p)) p++;
            if (*p == ',') p++;
        }
    } else if (*p == ',') {
        p++;
    }
    while (isspace((unsigned char)*p)) p++;

    memset(rule->grad_stop_pos, 0, sizeof(rule->grad_stop_pos));
    parse_gradient_stops(&p, rule);
    if (rule->grad_stop_count < 2) return;

    apply_gradient_rule(rule, GRAD_RADIAL, 0.0f, cx, cy, radius);
}

// Parse url(...) helper: extracts the path into out_path (max len), returns 1 on success
static int parse_url(const char* val, char* out_path, int max_len) {
    const char* up = strstr(val, "url(");
    if (!up) return 0;
    up += 4;
    while (*up == ' ' || *up == '\'' || *up == '"') up++;
    const char* ue = strpbrk(up, "'\") ");
    if (!ue) ue = up + strlen(up);
    int ulen = (int)(ue - up);
    if (ulen <= 0 || ulen >= max_len) return 0;
    strncpy(out_path, up, (size_t)ulen);
    out_path[ulen] = '\0';
    return 1;
}

// Parse background shorthand: color, gradient, or url(...)
static void parse_background_shorthand(const char* val, StyleRule* rule) {
    char buf[512];
    strncpy(buf, val, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    trim_whitespace(buf);
    if (strcmp(buf, "none") == 0) {
        rule->has_bg = 0;
        return;
    }
    if (strncmp(buf, "url(", 4) == 0) {
        char path[256];
        if (parse_url(buf, path, sizeof(path))) {
            rule->has_bg_image = 1;
            strncpy(rule->bg_image_path, path, sizeof(rule->bg_image_path) - 1);
            rule->bg_image_path[sizeof(rule->bg_image_path) - 1] = '\0';
        }
        return;
    }
    if (strncmp(buf, "linear-gradient", 15) == 0) {
        parse_linear_gradient(buf, rule);
    } else if (strncmp(buf, "radial-gradient", 15) == 0) {
        parse_radial_gradient(buf, rule);
    } else {
        rule->has_bg = 1;
        parse_color(buf, &rule->bg_r, &rule->bg_g, &rule->bg_b, &rule->bg_a);
    }
}

static void parse_padding_shorthand(const char* val, StyleRule* rule) {
    float vals[4] = {0, 0, 0, 0};
    char buf[128];
    strncpy(buf, val, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    int count = 0;
    char* tok = strtok(buf, " \t");
    while (tok && count < 4) {
        vals[count++] = parse_float_val(tok);
        tok = strtok(NULL, " \t");
    }
    rule->has_padding = 1;
    /* CSS shorthand: 1=all, 2=(v h), 3=(t h b), 4=(t r b l) */
    if (count == 1) {
        rule->pad_t = rule->pad_r = rule->pad_b = rule->pad_l = vals[0];
    } else if (count == 2) {
        rule->pad_t = rule->pad_b = vals[0];
        rule->pad_r = rule->pad_l = vals[1];
    } else if (count == 3) {
        rule->pad_t = vals[0];
        rule->pad_r = rule->pad_l = vals[1];
        rule->pad_b = vals[2];
    } else {
        rule->pad_t = vals[0]; rule->pad_r = vals[1];
        rule->pad_b = vals[2]; rule->pad_l = vals[3];
    }
    rule->padding = rule->pad_t;
}

static void parse_margin_shorthand(const char* val, StyleRule* rule) {
    float vals[4] = {0, 0, 0, 0};
    char buf[128];
    strncpy(buf, val, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    int count = 0;
    char* tok = strtok(buf, " \t");
    while (tok && count < 4) {
        vals[count++] = parse_float_val(tok);
        tok = strtok(NULL, " \t");
    }
    rule->has_margin = 1;
    if (count == 1) {
        rule->margin_top = rule->margin_right = rule->margin_bottom = rule->margin_left = vals[0];
    } else if (count == 2) {
        rule->margin_top = rule->margin_bottom = vals[0];
        rule->margin_right = rule->margin_left = vals[1];
    } else if (count == 3) {
        rule->margin_top = vals[0];
        rule->margin_right = rule->margin_left = vals[1];
        rule->margin_bottom = vals[2];
    } else if (count >= 4) {
        rule->margin_top = vals[0];
        rule->margin_right = vals[1];
        rule->margin_bottom = vals[2];
        rule->margin_left = vals[3];
    }
}

static void parse_scroll_margin_shorthand(const char* val, StyleRule* rule) {
    float vals[4] = {0, 0, 0, 0};
    char buf[128];
    strncpy(buf, val, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    int count = 0;
    char* tok = strtok(buf, " \t");
    while (tok && count < 4) {
        vals[count++] = parse_float_val(tok);
        tok = strtok(NULL, " \t");
    }
    rule->has_scroll_margin = 1;
    if (count == 1) {
        rule->scroll_margin_top = rule->scroll_margin_right =
            rule->scroll_margin_bottom = rule->scroll_margin_left = vals[0];
    } else if (count == 2) {
        rule->scroll_margin_top = rule->scroll_margin_bottom = vals[0];
        rule->scroll_margin_right = rule->scroll_margin_left = vals[1];
    } else if (count == 3) {
        rule->scroll_margin_top = vals[0];
        rule->scroll_margin_right = rule->scroll_margin_left = vals[1];
        rule->scroll_margin_bottom = vals[2];
    } else if (count >= 4) {
        rule->scroll_margin_top = vals[0];
        rule->scroll_margin_right = vals[1];
        rule->scroll_margin_bottom = vals[2];
        rule->scroll_margin_left = vals[3];
    }
}

static void parse_scroll_padding_shorthand(const char* val, StyleRule* rule) {
    float vals[4] = {0, 0, 0, 0};
    char buf[128];
    strncpy(buf, val, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    int count = 0;
    char* tok = strtok(buf, " \t");
    while (tok && count < 4) {
        vals[count++] = parse_float_val(tok);
        tok = strtok(NULL, " \t");
    }
    rule->has_scroll_padding = 1;
    if (count == 1) {
        rule->scroll_padding_top = rule->scroll_padding_right =
            rule->scroll_padding_bottom = rule->scroll_padding_left = vals[0];
    } else if (count == 2) {
        rule->scroll_padding_top = rule->scroll_padding_bottom = vals[0];
        rule->scroll_padding_right = rule->scroll_padding_left = vals[1];
    } else if (count == 3) {
        rule->scroll_padding_top = vals[0];
        rule->scroll_padding_right = rule->scroll_padding_left = vals[1];
        rule->scroll_padding_bottom = vals[2];
    } else if (count >= 4) {
        rule->scroll_padding_top = vals[0];
        rule->scroll_padding_right = vals[1];
        rule->scroll_padding_bottom = vals[2];
        rule->scroll_padding_left = vals[3];
    }
}

// Parse transform: scale(), translate(x,y), translateX(), translateY() (may be combined)
static void parse_transform(const char* val, StyleRule* rule) {
    const char* p = val;
    while (p && *p) {
        while (isspace((unsigned char)*p)) p++;
        if (strncmp(p, "scale(", 6) == 0) {
            float s = 1.0f;
            if (sscanf(p + 6, "%f", &s) == 1) {
                rule->has_transform = 1;
                rule->transform_scale = s;
            }
        } else if (strncmp(p, "translate(", 10) == 0) {
            const char* arg = p + 10;
            char* endp;
            float tx = strtof(arg, &endp);
            int tx_pct = 0;
            if (*endp == '%') { tx_pct = 1; tx /= 100.0f; endp++; }
            /* find comma */
            while (*endp && *endp != ',' && *endp != ')') endp++;
            float ty = 0.0f;
            int ty_pct = 0;
            if (*endp == ',') {
                endp++;
                while (*endp == ' ') endp++;
                char* endp2;
                ty = strtof(endp, &endp2);
                if (*endp2 == '%') { ty_pct = 1; ty /= 100.0f; }
            }
            rule->has_transform_tx = 1;
            rule->has_transform_ty = 1;
            rule->transform_tx_pct = tx_pct;
            rule->transform_ty_pct = ty_pct;
            if (tx_pct) { rule->raw_transform_tx = tx; rule->transform_tx = 0.0f; }
            else rule->transform_tx = tx;
            if (ty_pct) { rule->raw_transform_ty = ty; rule->transform_ty = 0.0f; }
            else rule->transform_ty = ty;
        } else if (strncmp(p, "translateX(", 11) == 0) {
            char* endp;
            float tx = strtof(p + 11, &endp);
            int tx_pct = (*endp == '%');
            if (tx_pct) tx /= 100.0f;
            rule->has_transform_tx = 1;
            rule->transform_tx_pct = tx_pct;
            if (tx_pct) { rule->raw_transform_tx = tx; rule->transform_tx = 0.0f; }
            else rule->transform_tx = tx;
        } else if (strncmp(p, "translateY(", 11) == 0) {
            char* endp;
            float ty = strtof(p + 11, &endp);
            int ty_pct = (*endp == '%');
            if (ty_pct) ty /= 100.0f;
            rule->has_transform_ty = 1;
            rule->transform_ty_pct = ty_pct;
            if (ty_pct) { rule->raw_transform_ty = ty; rule->transform_ty = 0.0f; }
            else rule->transform_ty = ty;
        }
        const char* close = strchr(p, ')');
        if (!close) break;
        p = close + 1;
    }
}

static int parse_flex_direction(const char* val) {
    if (strstr(val, "column")) return FLEX_DIR_COLUMN;
    return FLEX_DIR_ROW;
}

static int parse_justify_content(const char* val) {
    if (strstr(val, "center"))        return FLEX_JUSTIFY_CENTER;
    if (strstr(val, "flex-end") || strstr(val, "end")) return FLEX_JUSTIFY_END;
    if (strstr(val, "space-between")) return FLEX_JUSTIFY_SPACE_BETWEEN;
    return FLEX_JUSTIFY_START;
}

static int parse_align_items(const char* val) {
    if (strstr(val, "center"))        return FLEX_ALIGN_CENTER;
    if (strstr(val, "flex-end") || strstr(val, "end")) return FLEX_ALIGN_END;
    if (strstr(val, "stretch"))       return FLEX_ALIGN_STRETCH;
    return FLEX_ALIGN_START;
}

static int parse_align_content(const char* val) {
    if (strstr(val, "space-between")) return FLEX_ALIGN_SPACE_BETWEEN;
    if (strstr(val, "space-around"))  return FLEX_ALIGN_SPACE_AROUND;
    return parse_align_items(val);
}

static int parse_grid_auto_flow(const char* val) {
    int flow = GRID_AUTO_FLOW_ROW;
    if (strstr(val, "column")) flow = GRID_AUTO_FLOW_COLUMN;
    if (strstr(val, "dense")) flow |= GRID_AUTO_FLOW_DENSE;
    return flow;
}

static int parse_overflow(const char* val) {
    if (strstr(val, "scroll")) return OVERFLOW_SCROLL;
    if (strstr(val, "auto")) return OVERFLOW_AUTO;
    if (strstr(val, "hidden")) return OVERFLOW_HIDDEN;
    return OVERFLOW_VISIBLE;
}

static int overflow_clips(int mode) {
    return mode == OVERFLOW_HIDDEN || mode == OVERFLOW_AUTO || mode == OVERFLOW_SCROLL;
}

static int overflow_scrollable(int mode) {
    return mode == OVERFLOW_AUTO || mode == OVERFLOW_SCROLL;
}

static float parse_scrollbar_width(const char* val) {
    if (strstr(val, "none")) return 0.0f;
    if (strstr(val, "thin")) return 3.0f;
    if (strstr(val, "auto")) return 5.0f;
    return parse_float_val(val);
}

static void parse_scrollbar_color(const char* val, StyleRule* rule) {
    char buf[128];
    strncpy(buf, val, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char* sp = strchr(buf, ' ');
    rule->has_scrollbar_color = 1;
    if (sp) {
        *sp = '\0';
        parse_color(buf, &rule->sb_thumb_r, &rule->sb_thumb_g, &rule->sb_thumb_b, &rule->sb_thumb_a);
        trim_whitespace(sp + 1);
        parse_color(sp + 1, &rule->sb_track_r, &rule->sb_track_g, &rule->sb_track_b, &rule->sb_track_a);
    } else {
        parse_color(buf, &rule->sb_thumb_r, &rule->sb_thumb_g, &rule->sb_thumb_b, &rule->sb_thumb_a);
        rule->sb_track_r = rule->sb_thumb_r * 0.85f + 0.08f;
        rule->sb_track_g = rule->sb_thumb_g * 0.85f + 0.08f;
        rule->sb_track_b = rule->sb_thumb_b * 0.85f + 0.08f;
        rule->sb_track_a = rule->sb_thumb_a * 0.35f;
    }
}

static float element_sb_width(const Element* c) {
    if (c->has_scrollbar_width) return c->scrollbar_width;
    return 5.0f;
}

static int parse_align_self(const char* val) {
    if (strstr(val, "auto")) return ALIGN_SELF_AUTO;
    return parse_align_items(val);
}

static int parse_flex_wrap(const char* val) {
    if (strstr(val, "wrap")) return FLEX_WRAP_WRAP;
    return FLEX_WRAP_NOWRAP;
}

static int parse_box_sizing(const char* val) {
    if (strstr(val, "border-box")) return BOX_BORDER;
    return BOX_CONTENT;
}

static void parse_one_grid_track(const char* tok, float* size, int* type, float* min_px) {
    *type = GRID_TRACK_PX;
    *min_px = 0.0f;
    if (strncmp(tok, "minmax(", 7) == 0) {
        *type = GRID_TRACK_MINMAX;
        const char* mp = tok + 7;
        *min_px = parse_float_val(mp);
        const char* comma = strchr(mp, ',');
        *size = comma ? parse_float_val(comma + 1) : 1.0f;
    } else if (strstr(tok, "fr")) {
        *type = GRID_TRACK_FR;
        *size = parse_float_val(tok);
    } else {
        *size = parse_float_val(tok);
    }
}

static void parse_grid_tracks(const char* val, float* sizes, int* types, float* mins, int* count) {
    const char* p = val;
    *count = 0;
    while (*p && *count < MAX_GRID_TRACKS) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        char tok[64] = {0};
        int ti = 0;
        if (strncmp(p, "minmax(", 7) == 0) {
            int depth = 0;
            while (*p && ti < 63) {
                tok[ti++] = *p;
                if (*p == '(') depth++;
                else if (*p == ')') { depth--; if (depth == 0) { p++; break; } }
                p++;
            }
        } else {
            while (*p && !isspace((unsigned char)*p) && ti < 63) tok[ti++] = *p++;
        }
        tok[ti] = '\0';
        trim_whitespace(tok);
        parse_one_grid_track(tok, &sizes[*count], &types[*count], &mins[*count]);
        (*count)++;
    }
}

static void parse_single_grid_track(const char* val, float* size, int* type, float* min_px) {
    char tok[64] = {0};
    strncpy(tok, val, sizeof(tok) - 1);
    trim_whitespace(tok);
    parse_one_grid_track(tok, size, type, min_px);
}

static void parse_grid_template_areas(const char* val, StyleRule* rule) {
    memset(rule->grid_area_cell, 0, sizeof(rule->grid_area_cell));
    rule->grid_area_rows = 0;
    rule->grid_area_cols = 0;
    const char* p = val;
    while (*p && rule->grid_area_rows < MAX_GRID_AREA_ROWS) {
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) break;
        if (*p != '"') { p++; continue; }
        p++;
        char rowbuf[256] = {0};
        int ri = 0;
        while (*p && *p != '"' && ri < 255) rowbuf[ri++] = *p++;
        if (*p == '"') p++;
        int col = 0;
        char* tok = strtok(rowbuf, " \t");
        while (tok && col < MAX_GRID_AREA_COLS) {
            strncpy(rule->grid_area_cell[rule->grid_area_rows][col], tok, 31);
            tok = strtok(NULL, " \t");
            col++;
        }
        if (col > rule->grid_area_cols) rule->grid_area_cols = col;
        rule->grid_area_rows++;
    }
    rule->has_grid_template_areas = (rule->grid_area_rows > 0);
}

static void compile_grid_area_rects(Element* cont) {
    cont->grid_area_rect_count = 0;
    int rows = cont->grid_area_rows, cols = cont->grid_area_cols;
    if (rows < 1 || cols < 1) return;
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            char* name = cont->grid_area_cell[r][c];
            if (!name[0] || strcmp(name, ".") == 0) continue;
            int found = 0;
            for (int i = 0; i < cont->grid_area_rect_count; i++) {
                if (strcmp(cont->grid_area_rects[i].name, name) == 0) { found = 1; break; }
            }
            if (found) continue;
            int minc = c, maxc = c, minr = r, maxr = r;
            for (int r2 = 0; r2 < rows; r2++) {
                for (int c2 = 0; c2 < cols; c2++) {
                    if (strcmp(cont->grid_area_cell[r2][c2], name) == 0) {
                        if (c2 < minc) minc = c2;
                        if (c2 > maxc) maxc = c2;
                        if (r2 < minr) minr = r2;
                        if (r2 > maxr) maxr = r2;
                    }
                }
            }
            if (cont->grid_area_rect_count >= MAX_GRID_AREAS) continue;
            GridAreaRect* ar = &cont->grid_area_rects[cont->grid_area_rect_count++];
            strncpy(ar->name, name, 31);
            ar->col = minc; ar->row = minr;
            ar->col_span = maxc - minc + 1;
            ar->row_span = maxr - minr + 1;
        }
    }
}

static int grid_area_lookup(Element* cont, const char* name, int* gc, int* gr, int* cs, int* rs) {
    for (int i = 0; i < cont->grid_area_rect_count; i++) {
        if (strcmp(cont->grid_area_rects[i].name, name) == 0) {
            *gc = cont->grid_area_rects[i].col;
            *gr = cont->grid_area_rects[i].row;
            *cs = cont->grid_area_rects[i].col_span;
            *rs = cont->grid_area_rects[i].row_span;
            return 1;
        }
    }
    return 0;
}

static int parse_grid_line_val(const char* val, int* span_out) {
    char buf[64];
    strncpy(buf, val, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    trim_whitespace(buf);
    if (strncmp(buf, "span ", 5) == 0) {
        *span_out = atoi(buf + 5);
        if (*span_out < 1) *span_out = 1;
        return -2;
    }
    int line = atoi(buf);
    if (line < 1) line = 1;
    return line - 1;
}

static int parse_cursor_type(const char* val) {
    if (strstr(val, "pointer"))   return 1;
    if (strstr(val, "text"))      return 2;
    if (strstr(val, "crosshair")) return 3;
    if (strstr(val, "ew-resize") || strstr(val, "e-resize")) return 4;
    if (strstr(val, "ns-resize") || strstr(val, "n-resize")) return 5;
    return 0;
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
        elements[idx].text[255] = '\0';
        elements[idx].has_custom_text = 1;
        if (elements[idx].aria_live == 1 || elements[idx].aria_live == 2) {
            snprintf(g_a11y_live_msg, sizeof(g_a11y_live_msg), "%s", new_text);
            g_a11y_live_assertive = (elements[idx].aria_live == 2);
            g_a11y_live_until = glfwGetTime() + 3.0;
        }
    }
}

void set_on_click(int idx, EventHandler cb) {
    if (idx >= 0 && idx < elem_count) elements[idx].on_click = cb;
}

static void register_js_handler(const char* name, EventHandler fn) {
    if (!name || !name[0] || !fn) return;
    for (int i = 0; i < g_js_handler_count; i++) {
        if (strcmp(g_js_handlers[i].name, name) == 0) {
            g_js_handlers[i].fn = fn;
            return;
        }
    }
    if (g_js_handler_count < MAX_JS_HANDLERS) {
        g_js_handlers[g_js_handler_count].name = name;
        g_js_handlers[g_js_handler_count].fn = fn;
        g_js_handler_count++;
    }
}

static EventHandler lookup_js_handler(const char* name) {
    if (!name || !name[0]) return NULL;
    for (int i = 0; i < g_js_handler_count; i++)
        if (strcmp(g_js_handlers[i].name, name) == 0)
            return g_js_handlers[i].fn;
    return NULL;
}

/* Parse onclick="onButton()" / "onButton(); return false;" → "onButton" */
static void parse_onclick_expr(const char* expr, char* out, int out_len) {
    if (!expr || !out || out_len <= 0) { if (out) out[0] = '\0'; return; }
    const char* p = expr;
    while (*p && isspace((unsigned char)*p)) p++;
    int i = 0;
    while (*p && (isalnum((unsigned char)*p) || *p == '_') && i < out_len - 1)
        out[i++] = *p++;
    out[i] = '\0';
}

static void wire_element_onclick_handlers(void) {
    for (int i = 0; i < elem_count; i++) {
        if (!elements[i].onclick[0]) continue;
        EventHandler fn = lookup_js_handler(elements[i].onclick);
        if (fn) elements[i].on_click = fn;
        else fprintf(stderr, "[vespera] Unknown onclick handler: %s (id=%s)\n",
                     elements[i].onclick, elements[i].id[0] ? elements[i].id : "(none)");
    }
}

static int extract_html_attr(const char* tag_buf, const char* attr, char* out, int out_len) {
    if (!tag_buf || !attr || !out || out_len <= 0) return 0;
    char needle[64];
    snprintf(needle, sizeof(needle), "%s=\"", attr);
    char* p = strstr(tag_buf, needle);
    if (!p) {
        snprintf(needle, sizeof(needle), "%s='", attr);
        p = strstr(tag_buf, needle);
        if (!p) return 0;
        p += strlen(attr) + 2;
        char* end = strchr(p, '\'');
        if (!end) return 0;
        int n = (int)(end - p);
        if (n >= out_len) n = out_len - 1;
        strncpy(out, p, (size_t)n);
        out[n] = '\0';
        return 1;
    }
    p += strlen(attr) + 2;
    char* end = strchr(p, '"');
    if (!end) return 0;
    int n = (int)(end - p);
    if (n >= out_len) n = out_len - 1;
    strncpy(out, p, (size_t)n);
    out[n] = '\0';
    return 1;
}

void parse_declarations(char* declarations, StyleRule* rule);

static void apply_element_inline_style(Element* e) {
    if (!e || !e->has_inline_style || !e->inline_style[0]) return;
    StyleRule rule;
    memset(&rule, 0, sizeof(rule));
    char buf[320];
    strncpy(buf, e->inline_style, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    parse_declarations(buf, &rule);

    if (rule.has_bg) {
        e->r = rule.bg_r; e->g = rule.bg_g; e->b = rule.bg_b; e->a = rule.bg_a;
        e->has_custom_bg = 1;
        if (rule.has_gradient) {
            e->has_gradient = 1;
            e->grad_type = rule.grad_type;
            e->grad_stop_count = rule.grad_stop_count;
            for (int s = 0; s < rule.grad_stop_count; s++) {
                e->grad_stop_pos[s] = rule.grad_stop_pos[s];
                e->grad_stop_r[s] = rule.grad_stop_r[s];
                e->grad_stop_g[s] = rule.grad_stop_g[s];
                e->grad_stop_b[s] = rule.grad_stop_b[s];
                e->grad_stop_a[s] = rule.grad_stop_a[s];
            }
            e->grad_angle = rule.grad_angle;
            e->grad_rad_cx = rule.grad_rad_cx;
            e->grad_rad_cy = rule.grad_rad_cy;
            e->grad_rad_r = rule.grad_rad_r;
        }
    }
    if (rule.has_color) {
        e->t_r = rule.c_r; e->t_g = rule.c_g; e->t_b = rule.c_b; e->t_a = rule.c_a;
        e->has_custom_color = 1;
    }
    if (rule.has_width) {
        e->has_css_width = 1;
        e->pct_w = rule.pct_w;
        e->raw_w = rule.raw_w;
        e->raw_w_off = rule.raw_w_off;
        e->css_width = rule.width;
    }
    if (rule.has_height) {
        e->has_css_height = 1;
        e->pct_h = rule.pct_h;
        e->raw_h = rule.raw_h;
        e->raw_h_off = rule.raw_h_off;
        e->css_height = rule.height;
    }
    if (rule.has_left) {
        e->has_left = 1;
        e->pct_left = rule.pct_left;
        e->raw_left = rule.left;
        e->raw_left_off = rule.raw_left_off;
        e->css_positioned |= 1;
    }
    if (rule.has_top) {
        e->has_top = 1;
        e->pct_top = rule.pct_top;
        e->raw_top = rule.top;
        e->raw_top_off = rule.raw_top_off;
        e->css_positioned |= 2;
    }
    if (rule.has_display) {
        e->display_none = rule.display_none;
        e->display_mode = rule.display_mode;
    }
    if (rule.has_margin) {
        e->margin_top = rule.margin_top;
        e->margin_right = rule.margin_right;
        e->margin_bottom = rule.margin_bottom;
        e->margin_left = rule.margin_left;
    }
    if (rule.has_shadow) {
        e->has_shadow = 1;
        e->sh_dx = rule.sh_dx; e->sh_dy = rule.sh_dy; e->sh_blur = rule.sh_blur;
        e->sh_r = rule.sh_r; e->sh_g = rule.sh_g; e->sh_b = rule.sh_b; e->sh_a = rule.sh_a;
    }
    if (rule.has_flex_direction || rule.has_display) {
        if (rule.display_mode == DISPLAY_FLEX) e->display_mode = DISPLAY_FLEX;
        if (rule.has_flex_direction) e->flex_direction = rule.flex_direction;
        if (rule.has_align_items) e->align_items = rule.align_items;
        if (rule.has_gap) e->flex_gap = rule.flex_gap;
    }
    if (rule.has_border) {
        e->border_width = rule.border_width;
        e->bd_r = rule.bd_r; e->bd_g = rule.bd_g; e->bd_b = rule.bd_b; e->bd_a = rule.bd_a;
        e->has_custom_border = 1;
    }
    if (rule.has_radius) e->border_radius = rule.border_radius;
    if (rule.has_text_align) e->text_align = rule.text_align;
    if (rule.has_font_size) e->font_size = rule.font_size;
    if (rule.has_font_weight) e->font_bold = rule.font_bold;
    if (rule.has_line_height) e->line_height = rule.line_height;
    if (rule.has_white_space) e->white_space = rule.white_space;
    if (rule.has_text_overflow) e->text_overflow = rule.text_overflow;
    if (rule.has_overflow_wrap) e->overflow_wrap = rule.overflow_wrap;
    g_layout_dirty = 1;
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

static int rects_intersect(float ax, float ay, float aw, float ah,
                           float bx, float by, float bw, float bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static int element_overflow_visible(int idx) {
    Element* e = &elements[idx];
    float ex = e->x, ey = e->y, ew = e->w, eh = e->h;
    int p = e->parent_idx;
    while (p != -1) {
        Element* par = &elements[p];
        if (overflow_clips(par->overflow_x) || overflow_clips(par->overflow_y)) {
            float cx = par->x + par->pad_l, cy = par->y + par->pad_t;
            float cw = par->w - par->pad_l - par->pad_r, ch = par->h - par->pad_t - par->pad_b;
            if (cw <= 0.0f || ch <= 0.0f) return 0;
            int clip_x = overflow_clips(par->overflow_x);
            int clip_y = overflow_clips(par->overflow_y);
            if (clip_x && (ex + ew <= cx || ex >= cx + cw)) return 0;
            if (clip_y && (ey + eh <= cy || ey >= cy + ch)) return 0;
            if (clip_x && clip_y && !rects_intersect(ex, ey, ew, eh, cx, cy, cw, ch)) return 0;
        }
        p = par->parent_idx;
    }
    return 1;
}

int is_rendered(int idx) {
    if (!is_visible(idx)) return 0;
    if (elements[idx].visibility_hidden) return 0;
    if (!element_overflow_visible(idx)) return 0;
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
            if (strcmp(token, "*") == 0) out->is_universal = 1;
            else if (strlen(token) > 0) strncpy(out->sel_type, token, 31);
        }
    }
}

void parse_selector(const char* sel_in, StyleRule* rule) {
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
    if (!s->sel_type[0] && !s->sel_id[0] && s->sel_class_count == 0)
        return s->is_universal ? 1 : 0;
    return 1;
}

int selector_matches(StyleRule* r, Element* e) {
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

#define POS_UNSET     0
#define POS_STATIC    1
#define POS_RELATIVE  2
#define POS_ABSOLUTE  3

static int offsets_should_apply(const Element* e) {
    if (e->position_fixed || e->position_sticky) return 1;
    if (e->position_mode == POS_STATIC) return 0;
    if (e->position_mode == POS_RELATIVE || e->position_mode == POS_ABSOLUTE) return 1;
    return 1;
}

/* Forward declaration — defined below */
void parse_declarations(char* declarations, StyleRule* rule);

/* Apply one pre-tokenised CSS key/value to a StyleRule.
   Wraps parse_declarations so all property logic stays in one place. */
static void apply_one_declaration(const char* key, const char* val, StyleRule* rule) {
    /* Skip CSS custom properties (--name: value) — resolved by cssparser.h */
    if (strncmp(key, "--", 2) == 0) return;
    char buf[CSS_MAX_VALUE + CSS_MAX_STR + 4];
    snprintf(buf, sizeof(buf), "%s: %s;", key, val);
    parse_declarations(buf, rule);
}

void parse_declarations(char* declarations, StyleRule* rule) {
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
            else if (strcmp(key, "background-image") == 0) {
                char path[256];
                if (parse_url(val, path, sizeof(path))) {
                    rule->has_bg_image = 1;
                    strncpy(rule->bg_image_path, path, sizeof(rule->bg_image_path) - 1);
                    rule->bg_image_path[sizeof(rule->bg_image_path) - 1] = '\0';
                }
            }
            else if (strcmp(key, "color") == 0)            { rule->has_color = 1; parse_color(val, &rule->c_r, &rule->c_g, &rule->c_b, &rule->c_a); }
            else if (strcmp(key, "border-radius") == 0)    { rule->has_radius = 1; rule->border_radius = parse_float_val(val); }
            else if (strcmp(key, "border-width") == 0)     { rule->has_border = 1; rule->border_width = parse_float_val(val); }
            else if (strcmp(key, "outline-width") == 0)  { rule->has_outline = 1; rule->outline_width = parse_float_val(val); }
            else if (strcmp(key, "outline-color") == 0)  {
                rule->has_outline = 1;
                parse_color(val, &rule->ol_r, &rule->ol_g, &rule->ol_b, &rule->ol_a);
            }
            else if (strcmp(key, "outline-offset") == 0) { rule->has_outline = 1; rule->outline_offset = parse_float_val(val); }
            else if (strcmp(key, "outline") == 0) {
                rule->has_outline = 1;
                char obuf[96];
                strncpy(obuf, val, sizeof(obuf) - 1);
                obuf[sizeof(obuf) - 1] = '\0';
                char* sp = strchr(obuf, ' ');
                if (sp) {
                    *sp = '\0';
                    rule->outline_width = parse_float_val(obuf);
                    parse_color(sp + 1, &rule->ol_r, &rule->ol_g, &rule->ol_b, &rule->ol_a);
                } else if (strstr(obuf, "#") || strstr(obuf, "rgb")) {
                    parse_color(obuf, &rule->ol_r, &rule->ol_g, &rule->ol_b, &rule->ol_a);
                    rule->outline_width = 2.0f;
                } else {
                    rule->outline_width = parse_float_val(obuf);
                }
            }
            else if (strcmp(key, "border-color") == 0)     { rule->has_border = 1; parse_color(val, &rule->bd_r, &rule->bd_g, &rule->bd_b, &rule->bd_a); }
            else if (strcmp(key, "border") == 0)           { parse_border_shorthand(val, rule); }
            else if (strcmp(key, "border-top") == 0 || strcmp(key, "border-right") == 0 ||
                     strcmp(key, "border-bottom") == 0 || strcmp(key, "border-left") == 0) {
                /* Directional border: parse color only; do NOT set border_width
                   (that would inflate element dimensions).  We only need the
                   color for the divider effect rendered via the uniform border. */
                float cr, cg, cb, ca = 1.0f;
                const char* sp = val;
                while (*sp && (*sp == ' ' || isdigit((unsigned char)*sp) || *sp == '.' || *sp == 'p' || *sp == 'x')) sp++;
                if (*sp == ' ') sp++;
                /* skip "solid"/"dashed"/"none" keyword */
                while (*sp && !(*sp == 'r' || *sp == '#' || *sp == 'h' || *sp == 't')) sp++;
                if (*sp) { parse_color(sp, &cr, &cg, &cb, &ca); rule->has_border = 1; rule->bd_r = cr; rule->bd_g = cg; rule->bd_b = cb; rule->bd_a = ca; }
            }
            else if (strcmp(key, "width") == 0) {
                if (strcmp(val,"fit-content")==0 || strcmp(val,"max-content")==0 || strcmp(val,"min-content")==0) {
                    /* leave has_css_width=0, let layout compute */
                } else {
                    rule->has_width = 1;
                    parse_length_calc(val, &rule->width, &rule->pct_w, &rule->raw_w_off);
                    if (rule->pct_w) rule->raw_w = rule->width;
                }
            }
            else if (strcmp(key, "height") == 0) {
                rule->has_height = 1;
                parse_length_calc(val, &rule->height, &rule->pct_h, &rule->raw_h_off);
                if (rule->pct_h) rule->raw_h = rule->height;
            }
            else if (strcmp(key, "padding") == 0)          { parse_padding_shorthand(val, rule); }
            else if (strcmp(key, "padding-top") == 0)      { rule->has_padding = 1; rule->pad_t = parse_float_val(val); }
            else if (strcmp(key, "padding-right") == 0)    { rule->has_padding = 1; rule->pad_r = parse_float_val(val); }
            else if (strcmp(key, "padding-bottom") == 0)   { rule->has_padding = 1; rule->pad_b = parse_float_val(val); }
            else if (strcmp(key, "padding-left") == 0)     { rule->has_padding = 1; rule->pad_l = parse_float_val(val); }
            else if (strcmp(key, "margin") == 0)           { parse_margin_shorthand(val, rule); }
            else if (strcmp(key, "margin-top") == 0)       { rule->has_margin = 1; if (strcmp(val,"auto")==0) rule->margin_top_auto=1; else rule->margin_top = parse_float_val(val); }
            else if (strcmp(key, "margin-right") == 0)     { rule->has_margin = 1; if (strcmp(val,"auto")==0) rule->margin_right_auto=1; else rule->margin_right = parse_float_val(val); }
            else if (strcmp(key, "margin-bottom") == 0)    { rule->has_margin = 1; if (strcmp(val,"auto")==0) rule->margin_bottom_auto=1; else rule->margin_bottom = parse_float_val(val); }
            else if (strcmp(key, "margin-left") == 0)      { rule->has_margin = 1; if (strcmp(val,"auto")==0) rule->margin_left_auto=1; else rule->margin_left = parse_float_val(val); }
            else if (strcmp(key, "left") == 0) { rule->has_left = 1; parse_length_calc(val, &rule->left, &rule->pct_left, &rule->raw_left_off); if (rule->pct_left) rule->raw_left = rule->left; }
            else if (strcmp(key, "top") == 0)  { rule->has_top = 1; parse_length_calc(val, &rule->top, &rule->pct_top, &rule->raw_top_off); if (rule->pct_top) rule->raw_top = rule->top; }
            else if (strcmp(key, "bottom") == 0)           { rule->has_bottom = 1; parse_length(val, &rule->bottom, &rule->pct_bottom); if (rule->pct_bottom) rule->raw_bottom = rule->bottom; }
            else if (strcmp(key, "right") == 0)            { rule->has_right = 1; parse_length(val, &rule->right, &rule->pct_right); if (rule->pct_right) rule->raw_right = rule->right; }
            else if (strcmp(key, "position") == 0) {
                rule->has_position = 1;
                rule->position_fixed = (strcmp(val, "fixed") == 0);
                rule->position_sticky = (strcmp(val, "sticky") == 0);
                if (strcmp(val, "absolute") == 0) rule->position_mode = POS_ABSOLUTE;
                else if (strcmp(val, "relative") == 0) rule->position_mode = POS_RELATIVE;
                else if (strcmp(val, "static") == 0) rule->position_mode = POS_STATIC;
            }
            else if (strcmp(key, "opacity") == 0)          { rule->has_opacity = 1; rule->opacity = parse_float_val(val); }
            else if (strcmp(key, "cursor") == 0)           { rule->has_cursor = 1; rule->cursor_type = parse_cursor_type(val); rule->cursor_pointer = (rule->cursor_type == 1); }
            else if (strcmp(key, "display") == 0) {
                rule->has_display = 1;
                if (strcmp(val, "none") == 0) {
                    rule->display_none = 1;
                    rule->display_mode = DISPLAY_NONE;
                } else if (strcmp(val, "flex") == 0) {
                    rule->display_none = 0;
                    rule->display_mode = DISPLAY_FLEX;
                } else if (strcmp(val, "grid") == 0) {
                    rule->display_none = 0;
                    rule->display_mode = DISPLAY_GRID;
                } else {
                    rule->display_none = 0;
                    rule->display_mode = DISPLAY_BLOCK;
                }
            }
            else if (strcmp(key, "flex-direction") == 0) {
                rule->has_flex_direction = 1;
                rule->flex_direction = parse_flex_direction(val);
            }
            else if (strcmp(key, "justify-content") == 0) {
                rule->has_justify_content = 1;
                rule->justify_content = parse_justify_content(val);
            }
            else if (strcmp(key, "align-items") == 0) {
                rule->has_align_items = 1;
                rule->align_items = parse_align_items(val);
            }
            else if (strcmp(key, "justify-items") == 0) {
                rule->has_justify_items = 1;
                rule->justify_items = parse_align_items(val);
            }
            else if (strcmp(key, "place-items") == 0) {
                int a = parse_align_items(val);
                rule->has_align_items = 1;
                rule->has_justify_items = 1;
                rule->align_items = a;
                rule->justify_items = a;
            }
            else if (strcmp(key, "align-content") == 0) {
                rule->has_align_content = 1;
                rule->align_content = parse_align_content(val);
            }
            else if (strcmp(key, "place-content") == 0) {
                char pcbuf[64];
                strncpy(pcbuf, val, sizeof(pcbuf) - 1);
                pcbuf[sizeof(pcbuf) - 1] = '\0';
                char* sp = strchr(pcbuf, ' ');
                if (sp) {
                    *sp = '\0';
                    rule->has_align_content = 1;
                    rule->has_justify_content = 1;
                    rule->align_content = parse_align_content(pcbuf);
                    rule->justify_content = parse_justify_content(sp + 1);
                } else {
                    rule->has_align_content = 1;
                    rule->has_justify_content = 1;
                    rule->align_content = parse_align_content(pcbuf);
                    rule->justify_content = parse_justify_content(pcbuf);
                }
            }
            else if (strcmp(key, "flex-wrap") == 0) {
                rule->has_flex_wrap = 1;
                rule->flex_wrap = parse_flex_wrap(val);
            }
            else if (strcmp(key, "justify-self") == 0) {
                rule->has_justify_self = 1;
                rule->justify_self = parse_align_self(val);
            }
            else if (strcmp(key, "place-self") == 0) {
                char psbuf[64];
                strncpy(psbuf, val, sizeof(psbuf) - 1);
                psbuf[sizeof(psbuf) - 1] = '\0';
                char* sp = strchr(psbuf, ' ');
                if (sp) {
                    *sp = '\0';
                    rule->has_align_self = 1;
                    rule->has_justify_self = 1;
                    rule->align_self = parse_align_self(sp + 1);
                    rule->justify_self = parse_align_self(psbuf);
                } else {
                    int v = parse_align_self(psbuf);
                    rule->has_align_self = 1;
                    rule->has_justify_self = 1;
                    rule->align_self = v;
                    rule->justify_self = v;
                }
            }
            else if (strcmp(key, "box-sizing") == 0) {
                rule->has_box_sizing = 1;
                rule->box_sizing = parse_box_sizing(val);
            }
            else if (strcmp(key, "grid-template-columns") == 0) {
                rule->has_grid_template_columns = 1;
                parse_grid_tracks(val, rule->grid_col_track, rule->grid_col_type,
                                  rule->grid_col_min, &rule->grid_col_count);
            }
            else if (strcmp(key, "grid-template-rows") == 0) {
                rule->has_grid_template_rows = 1;
                parse_grid_tracks(val, rule->grid_row_track, rule->grid_row_type,
                                  rule->grid_row_min, &rule->grid_row_count);
            }
            else if (strcmp(key, "grid-template-areas") == 0) {
                parse_grid_template_areas(val, rule);
            }
            else if (strcmp(key, "column-gap") == 0) {
                rule->has_column_gap = 1;
                rule->grid_col_gap = parse_float_val(val);
            }
            else if (strcmp(key, "row-gap") == 0) {
                rule->has_row_gap = 1;
                rule->grid_row_gap = parse_float_val(val);
            }
            else if (strcmp(key, "grid-auto-flow") == 0) {
                rule->has_grid_auto_flow = 1;
                rule->grid_auto_flow = parse_grid_auto_flow(val);
            }
            else if (strcmp(key, "grid-auto-rows") == 0) {
                rule->has_grid_auto_rows = 1;
                parse_single_grid_track(val, &rule->grid_auto_row_track,
                                        &rule->grid_auto_row_type, &rule->grid_auto_row_min);
            }
            else if (strcmp(key, "grid-auto-columns") == 0) {
                rule->has_grid_auto_columns = 1;
                parse_single_grid_track(val, &rule->grid_auto_col_track,
                                        &rule->grid_auto_col_type, &rule->grid_auto_col_min);
            }
            else if (strcmp(key, "grid-column") == 0) {
                int span = 1;
                int col = parse_grid_line_val(val, &span);
                if (col == -2) { rule->has_grid_column_span = 1; rule->grid_col_span = span; }
                else { rule->has_grid_column = 1; rule->grid_col = col; rule->grid_col_span = span; }
            }
            else if (strcmp(key, "grid-row") == 0) {
                int span = 1;
                int row = parse_grid_line_val(val, &span);
                if (row == -2) { rule->has_grid_row_span = 1; rule->grid_row_span = span; }
                else { rule->has_grid_row = 1; rule->grid_row = row; rule->grid_row_span = span; }
            }
            else if (strcmp(key, "grid-area") == 0) {
                rule->has_grid_area = 1;
                strncpy(rule->grid_area_name, val, 31);
                rule->grid_area_name[31] = '\0';
                trim_whitespace(rule->grid_area_name);
            }
            else if (strcmp(key, "overflow") == 0) {
                int m = parse_overflow(val);
                rule->has_overflow_x = 1;
                rule->has_overflow_y = 1;
                rule->overflow_x = m;
                rule->overflow_y = m;
            }
            else if (strcmp(key, "overflow-x") == 0) {
                rule->has_overflow_x = 1;
                rule->overflow_x = parse_overflow(val);
            }
            else if (strcmp(key, "overflow-y") == 0) {
                rule->has_overflow_y = 1;
                rule->overflow_y = parse_overflow(val);
            }
            else if (strcmp(key, "scrollbar-width") == 0) {
                rule->has_scrollbar_width = 1;
                rule->scrollbar_width = parse_scrollbar_width(val);
            }
            else if (strcmp(key, "scrollbar-color") == 0) {
                parse_scrollbar_color(val, rule);
            }
            else if (strcmp(key, "scroll-behavior") == 0) {
                rule->has_scroll_behavior = 1;
                rule->scroll_smooth = (strstr(val, "smooth") != NULL);
            }
            else if (strcmp(key, "scroll-snap-type") == 0) {
                rule->has_scroll_snap_type = 1;
                if (strstr(val, "none"))
                    rule->scroll_snap_type = 0;
                else if (strstr(val, "proximity"))
                    rule->scroll_snap_type = 2;
                else
                    rule->scroll_snap_type = 1;
            }
            else if (strcmp(key, "scroll-snap-align") == 0) {
                rule->has_scroll_snap_align = 1;
                if (strstr(val, "center"))
                    rule->scroll_snap_align = 1;
                else if (strstr(val, "end"))
                    rule->scroll_snap_align = 2;
                else
                    rule->scroll_snap_align = 0;
            }
            else if (strcmp(key, "scroll-margin") == 0) {
                parse_scroll_margin_shorthand(val, rule);
            }
            else if (strcmp(key, "scroll-margin-top") == 0) {
                rule->has_scroll_margin = 1;
                rule->scroll_margin_top = parse_float_val(val);
            }
            else if (strcmp(key, "scroll-margin-right") == 0) {
                rule->has_scroll_margin = 1;
                rule->scroll_margin_right = parse_float_val(val);
            }
            else if (strcmp(key, "scroll-margin-bottom") == 0) {
                rule->has_scroll_margin = 1;
                rule->scroll_margin_bottom = parse_float_val(val);
            }
            else if (strcmp(key, "scroll-margin-left") == 0) {
                rule->has_scroll_margin = 1;
                rule->scroll_margin_left = parse_float_val(val);
            }
            else if (strcmp(key, "scroll-padding") == 0) {
                parse_scroll_padding_shorthand(val, rule);
            }
            else if (strcmp(key, "scroll-padding-top") == 0) {
                rule->has_scroll_padding = 1;
                rule->scroll_padding_top = parse_float_val(val);
            }
            else if (strcmp(key, "scroll-padding-right") == 0) {
                rule->has_scroll_padding = 1;
                rule->scroll_padding_right = parse_float_val(val);
            }
            else if (strcmp(key, "scroll-padding-bottom") == 0) {
                rule->has_scroll_padding = 1;
                rule->scroll_padding_bottom = parse_float_val(val);
            }
            else if (strcmp(key, "scroll-padding-left") == 0) {
                rule->has_scroll_padding = 1;
                rule->scroll_padding_left = parse_float_val(val);
            }
            else if (strcmp(key, "min-width") == 0) {
                rule->has_min_width = 1;
                rule->min_width = parse_float_val(val);
            }
            else if (strcmp(key, "min-height") == 0) {
                rule->has_min_height = 1;
                rule->min_height = parse_float_val(val);
            }
            else if (strcmp(key, "max-width") == 0) {
                rule->has_max_width = 1;
                rule->max_width = parse_float_val(val);
            }
            else if (strcmp(key, "max-height") == 0) {
                rule->has_max_height = 1;
                rule->max_height = parse_float_val(val);
            }
            else if (strcmp(key, "gap") == 0) {
                rule->has_gap = 1;
                char gbuf[64];
                strncpy(gbuf, val, sizeof(gbuf) - 1);
                gbuf[sizeof(gbuf) - 1] = '\0';
                char* g2 = strchr(gbuf, ' ');
                if (g2) {
                    *g2 = '\0';
                    rule->flex_gap = parse_float_val(gbuf);
                    rule->has_column_gap = 1;
                    rule->has_row_gap = 1;
                    rule->grid_col_gap = rule->flex_gap;
                    rule->grid_row_gap = parse_float_val(g2 + 1);
                } else {
                    rule->flex_gap = parse_float_val(gbuf);
                }
            }
            else if (strcmp(key, "flex-grow") == 0) {
                rule->has_flex_grow = 1;
                rule->flex_grow = (int)parse_float_val(val);
            }
            else if (strcmp(key, "flex-shrink") == 0) {
                rule->has_flex_shrink = 1;
                rule->flex_shrink = (int)parse_float_val(val);
            }
            else if (strcmp(key, "flex-basis") == 0) {
                rule->has_flex_basis = 1;
                if (strstr(val, "auto")) {
                    rule->flex_basis_auto = 1;
                } else {
                    rule->flex_basis = parse_float_val(val);
                    rule->flex_basis_auto = 0;
                }
            }
            else if (strcmp(key, "flex") == 0) {
                char fbuf[64];
                strncpy(fbuf, val, sizeof(fbuf) - 1);
                fbuf[sizeof(fbuf) - 1] = '\0';
                char* tok = strtok(fbuf, " \t/");
                int part = 0;
                while (tok) {
                    trim_whitespace(tok);
                    if (part == 0) {
                        rule->has_flex_grow = 1;
                        rule->flex_grow = (int)parse_float_val(tok);
                    } else if (part == 1) {
                        rule->has_flex_shrink = 1;
                        rule->flex_shrink = (int)parse_float_val(tok);
                    } else if (part == 2) {
                        rule->has_flex_basis = 1;
                        if (strstr(tok, "auto")) rule->flex_basis_auto = 1;
                        else {
                            rule->flex_basis = parse_float_val(tok);
                            rule->flex_basis_auto = 0;
                        }
                    }
                    part++;
                    tok = strtok(NULL, " \t/");
                }
            }
            else if (strcmp(key, "visibility") == 0)       { rule->has_visibility = 1; rule->visibility_hidden = (strcmp(val, "hidden") == 0); }
            else if (strcmp(key, "pointer-events") == 0) { rule->has_pointer_events = 1; rule->pointer_events_none = (strcmp(val, "none") == 0); }
            else if (strcmp(key, "z-index") == 0)          { rule->has_z_index = 1; rule->z_index = atoi(val); }
            else if (strcmp(key, "transform") == 0)        { parse_transform(val, rule); }
            else if (strcmp(key, "transition") == 0 || strcmp(key, "transition-duration") == 0) {
                const char* p = val; float sec = 0.0f; char numbuf[32] = {0};
                while (*p) {
                    if (isdigit((unsigned char)*p) || *p == '.') {
                        int i = 0;
                        while ((isdigit((unsigned char)*p) || *p == '.') && i < 31) numbuf[i++] = *p++;
                        numbuf[i] = 0;
                        if (strncmp(p, "ms", 2) == 0) sec = (float)atof(numbuf) / 1000.0f;
                        else sec = (float)atof(numbuf);
                        break;
                    }
                    p++;
                }
                if (sec > 0.0f) { rule->has_transition = 1; rule->transition_duration = sec; }
            }
            else if (strcmp(key, "animation") == 0 || strcmp(key, "animation-name") == 0) {
                if (strcmp(key, "animation-name") == 0) {
                    rule->has_animation = 1;
                    strncpy(rule->anim_name, val, sizeof(rule->anim_name) - 1);
                } else {
                    rule->has_animation = 1;
                    char abuf[128];
                    strncpy(abuf, val, sizeof(abuf) - 1);
                    abuf[sizeof(abuf) - 1] = '\0';
                    char* tok = strtok(abuf, " \t,");
                    int part = 0;
                    while (tok) {
                        trim_whitespace(tok);
                        if (part == 0 && strcmp(tok, "none") != 0)
                            strncpy(rule->anim_name, tok, sizeof(rule->anim_name) - 1);
                        else if (strchr(tok, 's') || isdigit((unsigned char)tok[0])) {
                            float sec = 0.0f;
                            if (strstr(tok, "ms")) sec = (float)atof(tok) / 1000.0f;
                            else sec = (float)atof(tok);
                            if (part == 0 && rule->anim_duration <= 0.0f) rule->anim_duration = sec;
                            else if (rule->anim_duration <= 0.0f) rule->anim_duration = sec;
                            else rule->anim_delay = sec;
                        } else if (strstr(tok, "ease-in-out")) rule->anim_easing = 1;
                        else if (strstr(tok, "linear")) rule->anim_easing = 0;
                        else if (strcmp(tok, "infinite") == 0) rule->anim_infinite = 1;
                        else if (strcmp(tok, "alternate") == 0) rule->anim_alternate = 1;
                        part++;
                        tok = strtok(NULL, " \t,");
                    }
                }
            }
            else if (strcmp(key, "animation-duration") == 0) {
                rule->has_animation = 1;
                if (strstr(val, "ms")) rule->anim_duration = (float)atof(val) / 1000.0f;
                else rule->anim_duration = (float)atof(val);
            }
            else if (strcmp(key, "animation-delay") == 0) {
                rule->has_animation = 1;
                if (strstr(val, "ms")) rule->anim_delay = (float)atof(val) / 1000.0f;
                else rule->anim_delay = (float)atof(val);
            }
            else if (strcmp(key, "animation-iteration-count") == 0) {
                rule->has_animation = 1;
                if (strcmp(val, "infinite") == 0) rule->anim_infinite = 1;
            }
            else if (strcmp(key, "animation-direction") == 0) {
                rule->has_animation = 1;
                if (strcmp(val, "alternate") == 0) rule->anim_alternate = 1;
            }
            else if (strcmp(key, "animation-timing-function") == 0) {
                rule->has_animation = 1;
                rule->anim_easing = (strstr(val, "ease-in-out") != NULL) ? 1 : 0;
            }
            else if (strcmp(key, "text-align") == 0) {
                rule->has_text_align = 1;
                if      (strcmp(val, "center") == 0) rule->text_align = 1;
                else if (strcmp(val, "right") == 0)  rule->text_align = 2;
                else                                 rule->text_align = 0;
            }
            else if (strcmp(key, "font-size") == 0)   { rule->has_font_size = 1; rule->font_size = (int)parse_float_val(val); }
            else if (strcmp(key, "font-weight") == 0) { rule->has_font_weight = 1; rule->font_bold = (strstr(val, "bold") != NULL || atoi(val) >= 600); }
            else if (strcmp(key, "line-height") == 0) {
                rule->has_line_height = 1;
                if (strcmp(val, "normal") == 0) rule->line_height = 0.0f;
                else rule->line_height = parse_float_val(val);
            }
            else if (strcmp(key, "white-space") == 0) {
                rule->has_white_space = 1;
                rule->white_space = (strstr(val, "nowrap") != NULL) ? 1 : 0;
            }
            else if (strcmp(key, "text-overflow") == 0) {
                rule->has_text_overflow = 1;
                rule->text_overflow = (strstr(val, "ellipsis") != NULL) ? 1 : 0;
            }
            else if (strcmp(key, "overflow-wrap") == 0 || strcmp(key, "word-wrap") == 0 ||
                     strcmp(key, "word-break") == 0) {
                rule->has_overflow_wrap = 1;
                rule->overflow_wrap =
                    (strstr(val, "break") != NULL || strstr(val, "anywhere") != NULL) ? 1 : 0;
            }
            else if (strcmp(key, "box-shadow") == 0)  { parse_box_shadow(val, rule); }
            else if (strcmp(key, "inset") == 0) {
                float v = parse_float_val(val);
                rule->has_top = 1;    rule->top = v;
                rule->has_right = 1;  rule->right = v;
                rule->has_bottom = 1; rule->bottom = v;
                rule->has_left = 1;   rule->left = v;
            }
        }
        if (!semi) break;
        prop = semi + 1;
    }
}

int cmp_rules_by_specificity(const void* a, const void* b) {
    return ((const StyleRule*)a)->specificity - ((const StyleRule*)b)->specificity;
}

/* ── Build a StyleRule from one parsed CSSRule ──────────────────────── */

/* Convert cssparser.h CSSSelector into our SimpleSelector + pseudo flags. */
static void convert_selector(const CSSSelector *cs, SimpleSelector *ss,
                              int *is_hover, int *is_active, int *is_focus,
                              int *is_focus_visible, int *is_focus_within) {
    memset(ss, 0, sizeof(*ss));
    for (int pi = 0; pi < cs->compounds[cs->compound_count > 0 ? cs->compound_count - 1 : 0].part_count; pi++) {
        const CSSSelectorPart *p = &cs->compounds[cs->compound_count > 0 ? cs->compound_count - 1 : 0].parts[pi];
        switch (p->type) {
        case CSS_SEL_TYPE:
            strncpy(ss->sel_type, p->name, sizeof(ss->sel_type) - 1);
            break;
        case CSS_SEL_ID:
            /* strip leading '#' if present */
            strncpy(ss->sel_id, p->name[0] == '#' ? p->name + 1 : p->name, sizeof(ss->sel_id) - 1);
            break;
        case CSS_SEL_CLASS:
            if (ss->sel_class_count < MAX_SEL_CLASSES)
                strncpy(ss->sel_classes[ss->sel_class_count++], p->name, sizeof(ss->sel_classes[0]) - 1);
            break;
        case CSS_SEL_UNIVERSAL:
            ss->is_universal = 1;
            break;
        case CSS_SEL_PSEUDO_CLASS:
            if      (strcmp(p->name, "hover")        == 0) *is_hover = 1;
            else if (strcmp(p->name, "active")       == 0) *is_active = 1;
            else if (strcmp(p->name, "focus-visible") == 0) *is_focus_visible = 1;
            else if (strcmp(p->name, "focus-within") == 0) *is_focus_within = 1;
            else if (strcmp(p->name, "focus")        == 0) *is_focus = 1;
            break;
        default: break;
        }
    }
}

/* Compute specificity for our internal format (100·id + 10·cls + 1·type). */
static int simple_selector_spec(const SimpleSelector *ss) {
    return (ss->sel_id[0] ? 100 : 0)
         + ss->sel_class_count * 10
         + (ss->sel_type[0] ? 1 : 0);
}

/* Ingest one cssparser.h CSSRule into the global css_rules[] array.
   For each selector in the rule a separate StyleRule entry is created. */
static void ingest_parsed_rule(const CSSRule *pr) {
    /* Build declaration-derived template once */
    StyleRule tmpl; memset(&tmpl, 0, sizeof(tmpl));
    int has_important = 0;
    for (int di = 0; di < pr->decl_count; di++) {
        apply_one_declaration(pr->decls[di].property, pr->decls[di].value, &tmpl);
        if (pr->decls[di].important) has_important = 1;
    }

    for (int si = 0; si < pr->selector_count && rule_count < MAX_RULES; si++) {
        const CSSSelector *cs = &pr->selectors[si];
        if (cs->compound_count == 0) continue;

        /* Pseudo-element targets (::before, ::after, ::-webkit-scrollbar,
           ::selection, ...) do not match real DOM elements. This engine does
           not render generated content, so drop such rules entirely; otherwise
           their declarations (e.g. `width:4px`) would leak onto the element the
           compound is attached to (e.g. `.tab_panel::-webkit-scrollbar`). */
        {
            const CSSCompound *tgt = &cs->compounds[cs->compound_count - 1];
            int has_pseudo_elem = 0;
            for (int pi = 0; pi < tgt->part_count; pi++)
                if (tgt->parts[pi].type == CSS_SEL_PSEUDO_ELEM) { has_pseudo_elem = 1; break; }
            if (has_pseudo_elem) continue;
        }

        StyleRule rule = tmpl;
        int is_hover = 0, is_active = 0, is_focus = 0, is_fvis = 0, is_fwithin = 0;

        /* Target = last compound */
        convert_selector(cs, &rule.target,
                         &is_hover, &is_active, &is_focus, &is_fvis, &is_fwithin);

        /* Ancestor compounds (earlier) */
        rule.ancestor_count = 0;
        for (int ci = 0; ci < cs->compound_count - 1 && rule.ancestor_count < MAX_SEL_ANCESTORS; ci++) {
            SimpleSelector *anc = &rule.ancestors[rule.ancestor_count++];
            memset(anc, 0, sizeof(*anc));
            const CSSCompound *cmp = &cs->compounds[ci];
            for (int pi = 0; pi < cmp->part_count; pi++) {
                const CSSSelectorPart *p = &cmp->parts[pi];
                if      (p->type == CSS_SEL_TYPE)  strncpy(anc->sel_type, p->name, sizeof(anc->sel_type)-1);
                else if (p->type == CSS_SEL_ID)    strncpy(anc->sel_id,   p->name, sizeof(anc->sel_id)-1);
                else if (p->type == CSS_SEL_CLASS && anc->sel_class_count < MAX_SEL_CLASSES)
                    strncpy(anc->sel_classes[anc->sel_class_count++], p->name, sizeof(anc->sel_classes[0])-1);
            }
        }

        rule.is_hover        = is_hover;
        rule.is_active       = is_active;
        rule.is_focus        = is_focus && !is_fvis;
        rule.is_focus_visible = is_fvis;
        rule.is_focus_within  = is_fwithin;

        /* Specificity: use cssparser.h value scaled to our units */
        int spec = simple_selector_spec(&rule.target);
        for (int a = 0; a < rule.ancestor_count; a++)
            spec += simple_selector_spec(&rule.ancestors[a]);
        if (is_hover)    spec += 1000;
        if (is_active)   spec += 2000;
        if (is_fvis)     spec += 1600;
        if (is_fwithin)  spec += 1550;
        if (is_focus && !is_fvis) spec += 1500;
        if (has_important) spec += 10000; /* !important boosts over any selector specificity */
        rule.specificity = spec;

        /* Store selector string for debugging */
        snprintf(rule.selector, sizeof(rule.selector) - 1,
                 "[sel%d]", si);

        css_rules[rule_count++] = rule;
    }
}

static float parse_keyframe_stop_pos(const char* sel) {
    if (!sel) return 0.0f;
    if (strcasecmp(sel, "from") == 0 || strcasecmp(sel, "0%") == 0) return 0.0f;
    if (strcasecmp(sel, "to") == 0 || strcasecmp(sel, "100%") == 0) return 1.0f;
    if (strchr(sel, '%')) return (float)atof(sel) / 100.0f;
    return (float)atof(sel);
}

static void keyframe_stop_from_rule(const CSSRule* kr, KeyframeStop* stop) {
    memset(stop, 0, sizeof(*stop));
    for (int di = 0; di < kr->decl_count; di++) {
        StyleRule tmp;
        memset(&tmp, 0, sizeof(tmp));
        apply_one_declaration(kr->decls[di].property, kr->decls[di].value, &tmp);
        if (tmp.has_width) {
            stop->has_width = 1;
            stop->width = tmp.width;
            stop->width_pct = tmp.pct_w;
        }
        if (tmp.has_left) {
            stop->has_left = 1;
            stop->left = tmp.left;
            stop->left_pct = tmp.pct_left;
        }
        if (tmp.has_opacity) { stop->has_opacity = 1; stop->opacity = tmp.opacity; }
        if (tmp.has_transform) {
            if (tmp.transform_scale != 1.0f) {
                stop->has_transform_scale = 1;
                stop->transform_scale = tmp.transform_scale;
            }
            if (tmp.has_transform_tx) { stop->has_transform_tx = 1; stop->transform_tx = tmp.transform_tx; }
            if (tmp.has_transform_ty) { stop->has_transform_ty = 1; stop->transform_ty = tmp.transform_ty; }
        }
        if (tmp.has_shadow) {
            /* orb-pulse style animations use box-shadow blur */
            (void)tmp;
        }
    }
}

static void ingest_keyframes_from_sheet(const CSSStyleSheet* sheet) {
    if (!sheet) return;
    for (int ai = 0; ai < sheet->at_rule_count && g_keyframe_count < MAX_KF_ANIMS; ai++) {
        const CSSAtRule* at = &sheet->at_rules[ai];
        if (at->type != CSS_AT_KEYFRAMES) continue;
        CssKeyframe kf;
        memset(&kf, 0, sizeof(kf));
        char name[64] = {0};
        strncpy(name, at->prelude, sizeof(name) - 1);
        trim_whitespace(name);
        strncpy(kf.name, name, sizeof(kf.name) - 1);
        for (int ri = 0; ri < at->nested_rule_count && kf.stop_count < MAX_KF_STOPS; ri++) {
            const CSSRule* kr = &at->nested_rules[ri];
            if (kr->selector_count == 0 || kr->selectors[0].compound_count == 0) continue;
            const CSSCompound* cmp = &kr->selectors[0].compounds[0];
            if (cmp->part_count == 0) continue;
            KeyframeStop stop;
            keyframe_stop_from_rule(kr, &stop);
            stop.position = parse_keyframe_stop_pos(cmp->parts[0].name);
            kf.stops[kf.stop_count++] = stop;
        }
        if (kf.stop_count > 0) {
            /* Sort stops and insert implicit 0% keyframe when only `to` is defined */
            for (int si = 0; si < kf.stop_count - 1; si++) {
                for (int sj = si + 1; sj < kf.stop_count; sj++) {
                    if (kf.stops[sj].position < kf.stops[si].position) {
                        KeyframeStop tmp = kf.stops[si];
                        kf.stops[si] = kf.stops[sj];
                        kf.stops[sj] = tmp;
                    }
                }
            }
            if (kf.stops[0].position > 0.001f && kf.stop_count < MAX_KF_STOPS) {
                memmove(&kf.stops[1], &kf.stops[0],
                        (size_t)kf.stop_count * sizeof(KeyframeStop));
                memset(&kf.stops[0], 0, sizeof(KeyframeStop));
                kf.stops[0].position = 0.0f;
                kf.stop_count++;
            }
            g_keyframes[g_keyframe_count++] = kf;
        }
    }
}

void parse_css(const char* css_text) {
    rule_count = 0;
    g_keyframe_count = 0;
    CSSStyleSheet *sheet = css_parse(css_text, 0);
    if (!sheet) return;

    /* Normal rules */
    for (int i = 0; i < sheet->rule_count && rule_count < MAX_RULES; i++)
        ingest_parsed_rule(&sheet->rules[i]);

    /* @media / @supports nested rules */
    for (int i = 0; i < sheet->at_rule_count; i++) {
        const CSSAtRule *at = &sheet->at_rules[i];
        if (at->type != CSS_AT_MEDIA && at->type != CSS_AT_SUPPORTS) continue;
        for (int j = 0; j < at->nested_rule_count && rule_count < MAX_RULES; j++)
            ingest_parsed_rule(&at->nested_rules[j]);
    }

    ingest_keyframes_from_sheet(sheet);

    css_free(sheet);
    qsort(css_rules, rule_count, sizeof(StyleRule), cmp_rules_by_specificity);
}

// ============================================================
// Style application
// ============================================================

static int is_descendant_of(int idx, int ancestor);
void update_element_style(Element* e);

static void update_focus_within_styles(int idx);

static int element_contains_focus(int idx) {
    if (g_focused_element_idx == -1 || idx == -1) return 0;
    if (g_focused_element_idx == idx) return 1;
    return is_descendant_of(g_focused_element_idx, idx);
}

static void update_focus_within_styles(int idx) {
    while (idx != -1) {
        update_element_style(&elements[idx]);
        idx = elements[idx].parent_idx;
    }
}

void update_element_style(Element* e) {
    if (!e->has_custom_bg)     { e->r = 0.0f; e->g = 0.0f; e->b = 0.0f; e->a = 0.0f; e->has_gradient = 0; e->has_bg_image = 0; e->bg_image_path[0] = '\0'; e->bg_image_tex = 0; }
    if (!e->has_custom_color)  { e->t_r = 0.1f; e->t_g = 0.1f; e->t_b = 0.1f; e->t_a = 1.0f; }
    if (!e->has_custom_border) { e->border_width = 0; e->bd_r = 0; e->bd_g = 0; e->bd_b = 0; e->bd_a = 0; }
    e->outline_width = 0.0f;
    e->outline_offset = 0.0f;
    e->has_outline = 0;
    e->ol_r = 0.39f; e->ol_g = 0.40f; e->ol_b = 0.95f; e->ol_a = 0.5f;
    e->border_radius = 0; e->padding = 0;
    e->pad_t = e->pad_r = e->pad_b = e->pad_l = 0;
    e->margin_top = e->margin_right = e->margin_bottom = e->margin_left = 0.0f;
    e->margin_top_auto = e->margin_right_auto = e->margin_bottom_auto = e->margin_left_auto = 0;
    e->opacity = 1; e->display_none = 0; e->display_mode = DISPLAY_BLOCK;
    e->visibility_hidden = 0; e->cursor_pointer = 0;
    e->flex_direction = FLEX_DIR_ROW;
    e->justify_content = FLEX_JUSTIFY_START;
    e->align_items = FLEX_ALIGN_STRETCH;
    e->justify_items = FLEX_ALIGN_STRETCH;
    e->align_content = FLEX_ALIGN_START;
    e->flex_wrap = FLEX_WRAP_NOWRAP;
    e->align_self = ALIGN_SELF_AUTO;
    e->justify_self = ALIGN_SELF_AUTO;
    e->flex_gap = 0.0f;
    e->flex_grow = 0;
    e->flex_shrink = 1;
    e->flex_child = 0;
    e->box_sizing = BOX_CONTENT;
    e->css_width = e->css_height = 0.0f;
    e->has_css_width = e->has_css_height = 0;
    e->grid_col_count = e->grid_row_count = 0;
    e->grid_col_gap = e->grid_row_gap = 0.0f;
    e->grid_auto_flow = GRID_AUTO_FLOW_ROW;
    e->grid_auto_row_track = 1.0f; e->grid_auto_row_type = GRID_TRACK_FR;
    e->grid_auto_col_track = 1.0f; e->grid_auto_col_type = GRID_TRACK_FR;
    e->scroll_top = e->scroll_left = 0.0f;
    e->scroll_dest_top = e->scroll_dest_left = 0.0f;
    e->scroll_smooth = 0;
    e->scroll_snap_type = 0;
    e->scroll_snap_align = 0;
    e->scroll_content_h = e->scroll_content_w = 0.0f;
    e->scrollbar_width = 5.0f;
    e->has_scrollbar_width = 0;
    e->has_scrollbar_color = 0;
    e->position_sticky = 0;
    e->position_mode = POS_UNSET;
    e->sticky_top = 0.0f;
    e->sticky_use_top = 0;
    e->sticky_use_bottom = 0;
    e->sticky_bottom = 0.0f;
    e->sticky_use_left = 0;
    e->sticky_left = 0.0f;
    e->sticky_use_right = 0;
    e->sticky_right = 0.0f;
    e->inert = 0;
    e->tabindex = -2;
    e->aria_label[0] = '\0';
    e->role[0] = '\0';
    e->aria_live = 0;
    e->aria_hidden = 0;
    e->aria_expanded = -1;
    e->scroll_margin_top = e->scroll_margin_right = 0.0f;
    e->scroll_margin_bottom = e->scroll_margin_left = 0.0f;
    e->scroll_padding_top = e->scroll_padding_right = 0.0f;
    e->scroll_padding_bottom = e->scroll_padding_left = 0.0f;
    e->overflow_x = OVERFLOW_VISIBLE;
    e->overflow_y = OVERFLOW_VISIBLE;
    e->grid_col = e->grid_row = -1;
    e->grid_col_span = e->grid_row_span = 1;
    e->has_grid_col = e->has_grid_row = 0;
    e->grid_child = 0;
    e->css_positioned = 0;
    e->cursor_type = 0; e->position_fixed = 0;
    e->has_bottom = 0; e->has_right = 0;
    e->has_top = 0; e->has_left = 0;
    e->text_align = 0; e->font_size = 16; e->font_bold = 0;
    e->line_height = 0.0f; e->white_space = 0; e->text_overflow = 0; e->overflow_wrap = 1;
    e->has_shadow = 0; e->z_index = 0;
    e->transform_scale = 1.0f; e->transform_tx = 0.0f; e->transform_ty = 0.0f;
    e->transform_tx_pct = 0; e->transform_ty_pct = 0;
    e->raw_transform_tx = 0.0f; e->raw_transform_ty = 0.0f;
    e->anim_speed = 14.0f;
    e->pointer_events_none = 0;
    e->pct_w = 0; e->pct_h = 0; e->pct_left = 0; e->pct_top = 0;
    e->pct_bottom = 0; e->pct_right = 0;
    e->raw_w_off = 0.0f; e->raw_h_off = 0.0f;
    e->raw_left_off = 0.0f; e->raw_top_off = 0.0f;

    e->has_css_animation = 0;
    e->anim_name[0] = '\0';
    e->anim_duration = 0.0f;
    e->anim_delay = 0.0f;
    e->anim_infinite = 0;
    e->anim_alternate = 0;
    e->anim_easing = 0;

    for (int i = 0; i < rule_count; i++) {
        StyleRule* r = &css_rules[i];
        if (!selector_matches(r, e)) continue;
        if (r->is_hover  && !e->is_hovered) continue;
        if (r->is_active && !e->is_active)  continue;
        if (r->is_focus  && e->id_idx != g_focused_element_idx) continue;
        if (r->is_focus_visible &&
            (e->id_idx != g_focused_element_idx || !g_focus_via_keyboard)) continue;
        if (r->is_focus_within && !element_contains_focus(e->id_idx)) continue;

        if (r->has_bg_image && !e->has_custom_bg) {
            e->has_bg_image = 1;
            strncpy(e->bg_image_path, r->bg_image_path, sizeof(e->bg_image_path) - 1);
            e->bg_image_path[sizeof(e->bg_image_path) - 1] = '\0';
            e->bg_image_tex = 0;
        }
        if (r->has_bg && !e->has_custom_bg) {
            e->r = r->bg_r; e->g = r->bg_g; e->b = r->bg_b; e->a = r->bg_a;
            if (r->has_gradient) {
                e->has_gradient = 1;
                e->grad_type = r->grad_type;
                e->grad_stop_count = r->grad_stop_count;
                e->grad_angle = r->grad_angle;
                e->grad_rad_cx = r->grad_rad_cx;
                e->grad_rad_cy = r->grad_rad_cy;
                e->grad_rad_r = r->grad_rad_r;
                for (int s = 0; s < r->grad_stop_count && s < MAX_GRAD_STOPS; s++) {
                    e->grad_stop_pos[s] = r->grad_stop_pos[s];
                    e->grad_stop_r[s] = r->grad_stop_r[s];
                    e->grad_stop_g[s] = r->grad_stop_g[s];
                    e->grad_stop_b[s] = r->grad_stop_b[s];
                    e->grad_stop_a[s] = r->grad_stop_a[s];
                }
            } else {
                e->has_gradient = 0;
                e->grad_type = GRAD_NONE;
            }
        }
        if (r->has_color && !e->has_custom_color)  { e->t_r = r->c_r; e->t_g = r->c_g; e->t_b = r->c_b; e->t_a = r->c_a; }
        if (r->has_border && !e->has_custom_border) { e->bd_r = r->bd_r; e->bd_g = r->bd_g; e->bd_b = r->bd_b; e->bd_a = r->bd_a; e->border_width = r->border_width; }
        if (r->has_outline) {
            e->has_outline = 1;
            e->outline_width = r->outline_width;
            e->outline_offset = r->outline_offset;
            e->ol_r = r->ol_r; e->ol_g = r->ol_g; e->ol_b = r->ol_b; e->ol_a = r->ol_a;
        }
        if (r->has_radius)  e->border_radius = r->border_radius;
        if (r->has_width) {
            e->pct_w = r->pct_w;
            if (r->pct_w) { e->raw_w = r->raw_w; e->raw_w_off = r->raw_w_off; }
            else { e->css_width = r->width; e->has_css_width = 1; e->w = r->width; }
        }
        if (r->has_height) {
            e->pct_h = r->pct_h;
            if (r->pct_h) { e->raw_h = r->raw_h; e->raw_h_off = r->raw_h_off; }
            else { e->css_height = r->height; e->has_css_height = 1; e->h = r->height; }
        }
        if (r->has_padding) {
            e->padding = r->padding;
            e->pad_t = r->pad_t; e->pad_r = r->pad_r;
            e->pad_b = r->pad_b; e->pad_l = r->pad_l;
        }
        if (r->has_margin) {
            e->margin_top = r->margin_top;
            e->margin_right = r->margin_right;
            e->margin_bottom = r->margin_bottom;
            e->margin_left = r->margin_left;
            e->margin_top_auto = r->margin_top_auto;
            e->margin_right_auto = r->margin_right_auto;
            e->margin_bottom_auto = r->margin_bottom_auto;
            e->margin_left_auto = r->margin_left_auto;
        }
        if (r->has_position) {
            e->position_fixed = r->position_fixed;
            e->position_sticky = r->position_sticky;
            if (r->position_mode != POS_UNSET) e->position_mode = r->position_mode;
        }
        if (r->has_left && !e->pos_overridden_x && offsets_should_apply(e)) {
            e->pct_left = r->pct_left;
            if (r->pct_left) { e->raw_left = r->raw_left; e->raw_left_off = r->raw_left_off; }
            else e->rel_x = r->left;
            e->has_left = 1; e->has_right = 0; e->css_positioned |= 1;
        }
        if (r->has_top && !e->pos_overridden_y && offsets_should_apply(e)) {
            e->pct_top = r->pct_top;
            if (r->pct_top) { e->raw_top = r->raw_top; e->raw_top_off = r->raw_top_off; }
            else e->rel_y = r->top;
            e->has_top = 1; e->has_bottom = 0; e->css_positioned |= 2;
        }
        if (r->has_bottom && !e->pos_overridden_y && offsets_should_apply(e)) {
            e->has_bottom = 1; e->pct_bottom = r->pct_bottom;
            if (r->pct_bottom) e->raw_bottom = r->raw_bottom; else e->bottom_val = r->bottom;
            e->css_positioned |= 2;
        }
        if (r->has_right && !e->pos_overridden_x && offsets_should_apply(e)) {
            e->has_right = 1; e->pct_right = r->pct_right;
            if (r->pct_right) e->raw_right = r->raw_right; else e->right_val = r->right;
            e->css_positioned |= 1;
        }
        if (r->has_top && e->position_sticky && !e->sticky_use_bottom) {
            e->sticky_use_top = 1;
            e->sticky_top = r->pct_top ? e->rel_y : r->top;
        }
        if (r->has_bottom && e->position_sticky) {
            e->sticky_use_bottom = 1;
            e->sticky_bottom = r->pct_bottom ? e->bottom_val : r->bottom;
        }
        if (r->has_left && e->position_sticky) {
            e->sticky_use_left = 1;
            e->sticky_left = r->pct_left ? e->raw_left : r->left;
        }
        if (r->has_right && e->position_sticky) {
            e->sticky_use_right = 1;
            e->sticky_right = r->pct_right ? e->raw_right : r->right;
        }
        if (r->has_opacity) e->opacity = r->opacity;
        if (r->has_cursor)  { e->cursor_pointer = r->cursor_pointer; e->cursor_type = r->cursor_type; }
        if (r->has_display) {
            e->display_none = r->display_none;
            e->display_mode = r->display_mode;
        }
        if (r->has_flex_direction) e->flex_direction = r->flex_direction;
        if (r->has_justify_content) e->justify_content = r->justify_content;
        if (r->has_align_items) e->align_items = r->align_items;
        if (r->has_justify_items) e->justify_items = r->justify_items;
        if (r->has_align_content) e->align_content = r->align_content;
        if (r->has_flex_wrap) e->flex_wrap = r->flex_wrap;
        if (r->has_align_self) e->align_self = r->align_self;
        if (r->has_justify_self) e->justify_self = r->justify_self;
        if (r->has_gap) e->flex_gap = r->flex_gap;
        if (r->has_flex_grow) e->flex_grow = r->flex_grow;
        if (r->has_flex_shrink) e->flex_shrink = r->flex_shrink;
        if (r->has_flex_basis) {
            e->has_flex_basis = 1;
            e->flex_basis = r->flex_basis;
            e->flex_basis_auto = r->flex_basis_auto;
        }
        if (r->has_min_width) { e->has_min_width = 1; e->css_min_width = r->min_width; }
        if (r->has_min_height) { e->has_min_height = 1; e->css_min_height = r->min_height; }
        if (r->has_max_width) { e->has_max_width = 1; e->css_max_width = r->max_width; }
        if (r->has_max_height) { e->has_max_height = 1; e->css_max_height = r->max_height; }
        if (r->has_box_sizing) e->box_sizing = r->box_sizing;
        if (r->has_overflow_x) e->overflow_x = r->overflow_x;
        if (r->has_overflow_y) e->overflow_y = r->overflow_y;
        if (r->has_scrollbar_width) {
            e->has_scrollbar_width = 1;
            e->scrollbar_width = r->scrollbar_width;
        }
        if (r->has_scrollbar_color) {
            e->has_scrollbar_color = 1;
            e->sb_thumb_r = r->sb_thumb_r; e->sb_thumb_g = r->sb_thumb_g;
            e->sb_thumb_b = r->sb_thumb_b; e->sb_thumb_a = r->sb_thumb_a;
            e->sb_track_r = r->sb_track_r; e->sb_track_g = r->sb_track_g;
            e->sb_track_b = r->sb_track_b; e->sb_track_a = r->sb_track_a;
        }
        if (r->has_scroll_behavior) e->scroll_smooth = r->scroll_smooth;
        if (r->has_scroll_snap_type) e->scroll_snap_type = r->scroll_snap_type;
        if (r->has_scroll_snap_align) e->scroll_snap_align = r->scroll_snap_align;
        if (r->has_scroll_margin) {
            e->scroll_margin_top = r->scroll_margin_top;
            e->scroll_margin_right = r->scroll_margin_right;
            e->scroll_margin_bottom = r->scroll_margin_bottom;
            e->scroll_margin_left = r->scroll_margin_left;
        }
        if (r->has_scroll_padding) {
            e->scroll_padding_top = r->scroll_padding_top;
            e->scroll_padding_right = r->scroll_padding_right;
            e->scroll_padding_bottom = r->scroll_padding_bottom;
            e->scroll_padding_left = r->scroll_padding_left;
        }
        if (r->has_grid_template_columns) {
            e->grid_col_count = r->grid_col_count;
            for (int t = 0; t < r->grid_col_count; t++) {
                e->grid_col_track[t] = r->grid_col_track[t];
                e->grid_col_type[t] = r->grid_col_type[t];
                e->grid_col_min[t] = r->grid_col_min[t];
            }
        }
        if (r->has_grid_template_rows) {
            e->grid_row_count = r->grid_row_count;
            for (int t = 0; t < r->grid_row_count; t++) {
                e->grid_row_track[t] = r->grid_row_track[t];
                e->grid_row_type[t] = r->grid_row_type[t];
                e->grid_row_min[t] = r->grid_row_min[t];
            }
        }
        if (r->has_grid_template_areas) {
            e->grid_area_rows = r->grid_area_rows;
            e->grid_area_cols = r->grid_area_cols;
            memcpy(e->grid_area_cell, r->grid_area_cell, sizeof(e->grid_area_cell));
            if (!r->has_grid_template_columns && r->grid_area_cols > 0) {
                e->grid_col_count = r->grid_area_cols;
                for (int t = 0; t < e->grid_col_count; t++) {
                    e->grid_col_track[t] = 1.0f;
                    e->grid_col_type[t] = GRID_TRACK_FR;
                    e->grid_col_min[t] = 0.0f;
                }
            }
            if (!r->has_grid_template_rows && r->grid_area_rows > 0) {
                e->grid_row_count = r->grid_area_rows;
                for (int t = 0; t < e->grid_row_count; t++) {
                    e->grid_row_track[t] = 1.0f;
                    e->grid_row_type[t] = GRID_TRACK_FR;
                    e->grid_row_min[t] = 0.0f;
                }
            }
            compile_grid_area_rects(e);
        }
        if (r->has_column_gap) e->grid_col_gap = r->grid_col_gap;
        if (r->has_row_gap) e->grid_row_gap = r->grid_row_gap;
        if (r->has_grid_auto_flow) e->grid_auto_flow = r->grid_auto_flow;
        if (r->has_grid_auto_rows) {
            e->has_grid_auto_rows = 1;
            e->grid_auto_row_track = r->grid_auto_row_track;
            e->grid_auto_row_type = r->grid_auto_row_type;
            e->grid_auto_row_min = r->grid_auto_row_min;
        }
        if (r->has_grid_auto_columns) {
            e->has_grid_auto_columns = 1;
            e->grid_auto_col_track = r->grid_auto_col_track;
            e->grid_auto_col_type = r->grid_auto_col_type;
            e->grid_auto_col_min = r->grid_auto_col_min;
        }
        if (r->has_grid_column) { e->has_grid_col = 1; e->grid_col = r->grid_col; }
        if (r->has_grid_row) { e->has_grid_row = 1; e->grid_row = r->grid_row; }
        if (r->has_grid_column_span) e->grid_col_span = r->grid_col_span;
        if (r->has_grid_row_span) e->grid_row_span = r->grid_row_span;
        if (r->has_grid_area) {
            e->has_grid_area = 1;
            strncpy(e->grid_area_name, r->grid_area_name, 31);
            e->grid_area_name[31] = '\0';
        }
        if (r->has_visibility) e->visibility_hidden = r->visibility_hidden;
        if (r->has_pointer_events) e->pointer_events_none = r->pointer_events_none;
        if (r->has_text_align)  e->text_align = r->text_align;
        if (r->has_font_size)   e->font_size = r->font_size;
        if (r->has_font_weight) e->font_bold = r->font_bold;
        if (r->has_line_height) e->line_height = r->line_height;
        if (r->has_white_space) e->white_space = r->white_space;
        if (r->has_text_overflow) e->text_overflow = r->text_overflow;
        if (r->has_overflow_wrap) e->overflow_wrap = r->overflow_wrap;
        if (r->has_shadow) {
            e->has_shadow = 1;
            e->sh_dx = r->sh_dx; e->sh_dy = r->sh_dy; e->sh_blur = r->sh_blur;
            e->sh_r = r->sh_r; e->sh_g = r->sh_g; e->sh_b = r->sh_b; e->sh_a = r->sh_a;
        }
        if (r->has_z_index) e->z_index = r->z_index;
        if (r->has_transform) e->transform_scale = r->transform_scale;
        if (r->has_transform_tx) {
            e->transform_tx = r->transform_tx;
            e->transform_tx_pct = r->transform_tx_pct;
            e->raw_transform_tx = r->raw_transform_tx;
        }
        if (r->has_transform_ty) {
            e->transform_ty = r->transform_ty;
            e->transform_ty_pct = r->transform_ty_pct;
            e->raw_transform_ty = r->raw_transform_ty;
        }
        if (r->has_transition) e->anim_speed = 1.0f / r->transition_duration;
        if (r->has_animation && r->anim_name[0]) {
            e->has_css_animation = 1;
            strncpy(e->anim_name, r->anim_name, sizeof(e->anim_name) - 1);
            if (r->anim_duration > 0.0f) e->anim_duration = r->anim_duration;
            e->anim_delay = r->anim_delay;
            e->anim_infinite = r->anim_infinite;
            e->anim_alternate = r->anim_alternate;
            e->anim_easing = r->anim_easing;
            if (e->anim_start_time < 0.0) e->anim_start_time = glfwGetTime();
        }
    }

    apply_element_inline_style(e);

    if (e->has_css_animation && !e->anim_base_captured) {
        e->anim_base_w = e->has_css_width ? e->css_width : e->w;
        e->anim_base_w_pct = e->pct_w;
        e->anim_base_left = e->has_left ? e->raw_left : e->rel_x;
        e->anim_base_left_pct = e->pct_left;
        e->anim_base_captured = 1;
    }
}

// ============================================================
// HTML parsing
// ============================================================

static void set_html_base_dir(const char* layout_path) {
    if (!layout_path || !layout_path[0]) {
        snprintf(g_html_base_dir, sizeof(g_html_base_dir), "ui");
        return;
    }
    strncpy(g_html_base_dir, layout_path, sizeof(g_html_base_dir) - 1);
    g_html_base_dir[sizeof(g_html_base_dir) - 1] = '\0';
    char* slash = strrchr(g_html_base_dir, '/');
    if (slash) *slash = '\0';
    else g_html_base_dir[0] = '\0';
}

static void resolve_resource_path(const char* href, char* out, size_t outsz) {
    if (!href || !href[0]) { out[0] = '\0'; return; }
    if (href[0] == '/' || strchr(href, ':')) {
        snprintf(out, outsz, "%s", href);
        return;
    }
    if (g_html_base_dir[0])
        snprintf(out, outsz, "%s/%s", g_html_base_dir, href);
    else
        snprintf(out, outsz, "%s", href);
}

static int tag_attr_equals(const char* tag_buf, const char* key, const char* val) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "%s=\"", key);
    char* a = strstr(tag_buf, pattern);
    if (!a) return 0;
    char got[64] = {0};
    sscanf(a + (int)strlen(pattern), "%63[^\"]", got);
    return strcasecmp(got, val) == 0;
}

static int tag_is_stylesheet_link(const char* tag_buf) {
    if (!tag_attr_equals(tag_buf, "rel", "stylesheet")) {
        char* rel = strstr(tag_buf, "rel=\"");
        if (!rel) return 0;
        char rval[32] = {0};
        sscanf(rel + 5, "%31[^\"]", rval);
        return strcasestr(rval, "stylesheet") != NULL;
    }
    return 1;
}

static void load_stylesheet_href(const char* href) {
    if (!href || !href[0]) return;
    char path[512];
    resolve_resource_path(href, path, sizeof(path));
    char* css = read_file(path);
    if (css) {
        parse_css(css);
        free(css);
        g_css_from_document = 1;
    }
}

static void ingest_inline_style(const char* css, int len) {
    if (!css || len <= 0) return;
    char* buf = (char*)malloc((size_t)len + 1);
    if (!buf) return;
    memcpy(buf, css, (size_t)len);
    buf[len] = '\0';
    parse_css(buf);
    free(buf);
    g_css_from_document = 1;
}

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

static int is_semantic_shell_element(const char* t) {
    static const char* sh[] = {"html","head","body","meta","link","title",NULL};
    for (int i = 0; sh[i]; i++) if (strcasecmp(t, sh[i]) == 0) return 1;
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

        if (strcasecmp(type, "link") == 0) {
            if (tag_is_stylesheet_link(tag_buf)) {
                char href[256] = {0};
                char* attr_href = strstr(tag_buf, "href=\"");
                if (attr_href) sscanf(attr_href + 6, "%255[^\"]", href);
                load_stylesheet_href(href);
            }
            p = tag_end + 1;
            continue;
        }

        if (strcasecmp(type, "meta") == 0) {
            p = tag_end + 1;
            continue;
        }

        if (strcasecmp(type, "title") == 0) {
            const char* text_start = tag_end + 1;
            const char* close_title = strcasestr(text_start, "</title>");
            if (close_title && close_title > text_start) {
                int tlen = (int)(close_title - text_start);
                if (tlen > (int)sizeof(g_doc_title) - 1) tlen = (int)sizeof(g_doc_title) - 1;
                strncpy(g_doc_title, text_start, (size_t)tlen);
                g_doc_title[tlen] = '\0';
                trim_whitespace(g_doc_title);
            }
            p = close_title ? strchr(close_title, '>') + 1 : tag_end + 1;
            continue;
        }

        // Skip style/script content (style ingests CSS; script is ignored)
        if (strcasecmp(type, "style") == 0) {
            const char* css_start = tag_end + 1;
            const char* close_pos = strcasestr(css_start, "</style>");
            if (close_pos && close_pos > css_start)
                ingest_inline_style(css_start, (int)(close_pos - css_start));
            const char* gt2 = close_pos ? strchr(close_pos, '>') : NULL;
            p = gt2 ? gt2 + 1 : tag_end + 1;
            continue;
        }
        if (strcasecmp(type, "script") == 0) {
            char close_tag[40]; snprintf(close_tag, sizeof(close_tag), "</%s", type);
            const char* close_pos = strcasestr(tag_end + 1, close_tag);
            const char* gt2 = close_pos ? strchr(close_pos, '>') : NULL;
            p = gt2 ? gt2 + 1 : tag_end + 1;
            continue;
        }

        if (is_semantic_shell_element(type)) {
            p = tag_end + 1;
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
        char onclick_expr[96] = {0};
        char style_attr[256] = {0};
        char data_tab[32] = {0};
        char* attr_id    = strstr(tag_buf, "id=\"");
        if (attr_id) sscanf(attr_id + 4, "%63[^\"]", id);
        char* attr_class = strstr(tag_buf, "class=\"");
        if (attr_class) sscanf(attr_class + 7, "%95[^\"]", class_name);
        char* attr_drag  = strstr(tag_buf, "draggable=\"");
        if (attr_drag) sscanf(attr_drag + 11, "%d", &draggable);
        if (!extract_html_attr(tag_buf, "onclick", onclick_expr, sizeof(onclick_expr)))
            extract_html_attr(tag_buf, "onClick", onclick_expr, sizeof(onclick_expr));
        extract_html_attr(tag_buf, "style", style_attr, sizeof(style_attr));
        extract_html_attr(tag_buf, "data-tab", data_tab, sizeof(data_tab));
        int tabindex = -2;
        char* attr_tab = strstr(tag_buf, "tabindex=\"");
        if (attr_tab) sscanf(attr_tab + 10, "%d", &tabindex);
        int inert = (strstr(tag_buf, "inert") != NULL);
        char aria_label[128] = {0};
        char role[32] = {0};
        char* attr_aria = strstr(tag_buf, "aria-label=\"");
        if (attr_aria) sscanf(attr_aria + 12, "%127[^\"]", aria_label);
        char* attr_role = strstr(tag_buf, "role=\"");
        if (attr_role) sscanf(attr_role + 6, "%31[^\"]", role);
        int aria_live = 0;
        char* attr_live = strstr(tag_buf, "aria-live=\"");
        if (attr_live) {
            char live_val[16] = {0};
            sscanf(attr_live + 11, "%15[^\"]", live_val);
            if (strcasecmp(live_val, "polite") == 0) aria_live = 1;
            else if (strcasecmp(live_val, "assertive") == 0) aria_live = 2;
        }
        int aria_hidden = 0;
        char* attr_hidden = strstr(tag_buf, "aria-hidden=\"");
        if (attr_hidden) {
            char hidden_val[8] = {0};
            sscanf(attr_hidden + 13, "%7[^\"]", hidden_val);
            aria_hidden = (strcmp(hidden_val, "true") == 0 || strcmp(hidden_val, "1") == 0);
        } else if (strstr(tag_buf, "aria-hidden") != NULL) {
            aria_hidden = 1;
        }
        int aria_expanded = -1;
        char* attr_expanded = strstr(tag_buf, "aria-expanded=\"");
        if (attr_expanded) {
            char exp_val[8] = {0};
            sscanf(attr_expanded + 15, "%7[^\"]", exp_val);
            if (strcmp(exp_val, "true") == 0 || strcmp(exp_val, "1") == 0) aria_expanded = 1;
            else aria_expanded = 0;
        }

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
        e.w = 100; e.h = 50;
        e.is_draggable = (draggable != 0);
        e.drag_mode = draggable;
        e.tabindex = tabindex;
        e.inert = inert;
        e.aria_live = aria_live;
        e.aria_hidden = aria_hidden;
        e.aria_expanded = aria_expanded;
        strncpy(e.aria_label, aria_label, 127);
        e.aria_label[127] = '\0';
        strncpy(e.role, role, 31);
        e.role[31] = '\0';
        strncpy(e.type, type, 31);
        strncpy(e.class_name, class_name, 95);
        strncpy(e.id, id, 63);
        strncpy(e.text, text, 255);
        if (onclick_expr[0]) parse_onclick_expr(onclick_expr, e.onclick, (int)sizeof(e.onclick));
        if (data_tab[0]) strncpy(e.data_tab, data_tab, sizeof(e.data_tab) - 1);
        if (style_attr[0]) {
            strncpy(e.inline_style, style_attr, sizeof(e.inline_style) - 1);
            e.has_inline_style = 1;
        }
        e.cur_scale = 1.0f;
        e.anim_start_time = -1.0;

        // <img src="..."> — treat src as background image
        if (strcasecmp(type, "img") == 0) {
            char src[256] = {0};
            char* attr_src = strstr(tag_buf, "src=\"");
            if (attr_src) sscanf(attr_src + 5, "%255[^\"]", src);
            if (src[0]) {
                e.has_bg_image = 1;
                strncpy(e.bg_image_path, src, sizeof(e.bg_image_path) - 1);
                e.bg_image_path[sizeof(e.bg_image_path) - 1] = '\0';
            }
            // alt attribute as fallback text
            char alt[256] = {0};
            char* attr_alt = strstr(tag_buf, "alt=\"");
            if (attr_alt) sscanf(attr_alt + 5, "%255[^\"]", alt);
            if (alt[0]) strncpy(e.text, alt, 255);
        }

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

FontAtlas* get_atlas(int size, int bold, int* out_is_fake_bold);
float measure_text_width(FontAtlas* atlas, const char* text);

static float flow_content_height(Element* e) {
    int idx = (int)(e - elements);

    /* Does this element have in-flow children? If so its auto height is derived
       from them (sum for column/block, max for row); otherwise fall back to a
       single text line. Without this, an auto-height container collapses to one
       line-height and then flex-shrink crushes its children (e.g. #sidebar_nav
       squashing its 34px nav items to ~3px). */
    int n = 0;
    float total = 0.0f, maxh = 0.0f;
    for (int c = 0; c < elem_count; c++) {
        Element* ch = &elements[c];
        if (ch->parent_idx != idx) continue;
        if (!is_visible(c)) continue;
        if (ch->position_mode == POS_ABSOLUTE || ch->position_fixed) continue;
        float chh;
        if (ch->has_css_height && !ch->pct_h) {
            chh = (ch->box_sizing == BOX_CONTENT)
                ? ch->css_height + ch->pad_t + ch->pad_b + ch->border_width * 2.0f
                : ch->css_height;
        } else if (ch->pct_h) {
            chh = 0.0f;              /* percentage height is not a definite size here */
        } else {
            chh = flow_content_height(ch);   /* recurse for auto-height children */
        }
        chh += ch->margin_top + ch->margin_bottom;
        if (chh > maxh) maxh = chh;
        total += chh;
        n++;
    }

    if (n > 0) {
        int row = (e->display_mode == DISPLAY_FLEX && e->flex_direction == FLEX_DIR_ROW);
        float inner = row ? maxh : (total + (n > 1 ? e->flex_gap * (float)(n - 1) : 0.0f));
        return inner + e->pad_t + e->pad_b;
    }

    float lh = (float)e->font_size;
    if (lh < 10.0f) lh = 12.0f;
    lh *= 1.5f;
    return lh + e->pad_t + e->pad_b;
}

static float flex_content_width(Element* ch) {
    if (ch->has_css_width && !ch->pct_w) return ch->css_width;
    if (ch->text[0] && font_loaded) {
        FontAtlas* atlas = get_atlas(ch->font_size, ch->font_bold, NULL);
        return measure_text_width(atlas, ch->text) + ch->pad_l + ch->pad_r + 4.0f;
    }
    if (ch->w > 0.0f && ch->w < 200.0f) return ch->w;
    return 40.0f;
}

static int is_out_of_flow(int idx) {
    Element* e = &elements[idx];
    if (e->position_fixed || e->flex_child || e->grid_child) return 1;
    if (e->position_mode == POS_ABSOLUTE) return 1;
    if (e->position_sticky) return 0;
    if (e->position_mode == POS_RELATIVE) return 0;
    if (e->css_positioned & 3) return 1;
    return 0;
}

static void layout_block_container(int container_idx) {
    Element* cont = &elements[container_idx];
    if (cont->display_mode == DISPLAY_FLEX || cont->display_mode == DISPLAY_GRID) return;
    if (cont->display_mode == DISPLAY_NONE) return;

    float inner_w = cont->w - cont->pad_l - cont->pad_r;
    if (inner_w < 0.0f) inner_w = 0.0f;
    float y = cont->pad_t;

    for (int c = 0; c < elem_count; c++) {
        if (elements[c].parent_idx != container_idx) continue;
        if (!is_visible(c) || elements[c].position_fixed) continue;
        if (is_out_of_flow(c)) continue;

        Element* ch = &elements[c];
        ch->flow_child = 1;

        if (ch->pct_w) {
            ch->w = inner_w * ch->raw_w;
        } else if (!ch->has_css_width) {
            ch->w = inner_w;
        }

        if (!ch->has_css_height && !ch->pct_h) {
            ch->h = flow_content_height(ch);
            if (ch->h < 14.0f) ch->h = 14.0f;
        }

        ch->rel_x = cont->pad_l + ch->margin_left;
        ch->rel_y = y + ch->margin_top;
        y += ch->margin_top + ch->h + ch->margin_bottom;
    }
}

void update_layout() {
    for (int i = 0; i < elem_count; i++) {
        Element* e = &elements[i];
        int par = e->parent_idx;
        float parent_w, parent_h;

        // body/html root element: always cover full window regardless of CSS sizes
        if (par == -1 && e->z_index <= -9000 &&
            (strcmp(e->type, "body") == 0 || strcmp(e->type, "html") == 0)) {
            e->x = 0.0f; e->y = 0.0f;
            e->w = window_width; e->h = window_height;
            e->rel_x = 0.0f; e->rel_y = 0.0f;
            continue;
        }

        if (e->position_fixed || par == -1) {
            parent_w = window_width;
            parent_h = window_height;
        } else {
            parent_w = elements[par].w;
            parent_h = elements[par].h;
        }

        if (e->pct_w) e->w = parent_w * e->raw_w + e->raw_w_off;
        if (e->pct_h) e->h = parent_h * e->raw_h + e->raw_h_off;

        if (e->has_css_width && !e->pct_w) {
            if (e->box_sizing == BOX_CONTENT)
                e->w = e->css_width + e->pad_l + e->pad_r + e->border_width * 2.0f;
            else
                e->w = e->css_width;
        }
        if (e->has_css_height && !e->pct_h) {
            if (e->box_sizing == BOX_CONTENT)
                e->h = e->css_height + e->pad_t + e->pad_b + e->border_width * 2.0f;
            else
                e->h = e->css_height;
        }
        if (e->has_min_width && e->w < e->css_min_width) e->w = e->css_min_width;
        if (e->has_min_height && e->h < e->css_min_height) e->h = e->css_min_height;
        if (e->has_max_width && e->w > e->css_max_width) e->w = e->css_max_width;
        if (e->has_max_height && e->h > e->css_max_height) e->h = e->css_max_height;

        if (!e->pos_overridden_x) {
            /* Both left+right set (no explicit width): stretch to fill (CSS inset). */
            if (e->has_left && e->has_right && !e->has_css_width && !e->pct_w) {
                float lv = e->pct_left ? (parent_w * e->raw_left + e->raw_left_off) : e->rel_x;
                float rv = e->pct_right ? (parent_w * e->raw_right) : e->right_val;
                e->rel_x = lv;
                e->w = parent_w - lv - rv;
            } else if (e->has_right && !e->has_left) {
                /* Right-anchored only */
                float off = e->pct_right ? (parent_w * e->raw_right) : e->right_val;
                e->rel_x = parent_w - e->w - off;
            } else if (e->pct_left) {
                e->rel_x = parent_w * e->raw_left + e->raw_left_off;
            }
            /* non-percentage left: rel_x already holds the left value */
        }

        if (!e->pos_overridden_y) {
            /* Both top+bottom set (no explicit height): stretch to fill. */
            if (e->has_top && e->has_bottom && !e->has_css_height && !e->pct_h) {
                float tv = e->pct_top ? (parent_h * e->raw_top + e->raw_top_off) : e->rel_y;
                float bv = e->pct_bottom ? (parent_h * e->raw_bottom) : e->bottom_val;
                e->rel_y = tv;
                e->h = parent_h - tv - bv;
            } else if (e->has_bottom && !e->has_top) {
                /* Bottom-anchored only */
                float off = e->pct_bottom ? (parent_h * e->raw_bottom) : e->bottom_val;
                e->rel_y = parent_h - e->h - off;
            } else if (e->pct_top) {
                e->rel_y = parent_h * e->raw_top + e->raw_top_off;
            }
            /* non-percentage top: rel_y already holds the top value */
        }

        if (e->position_fixed || par == -1) {
            e->x = e->rel_x + e->margin_left;
            e->y = e->rel_y + e->margin_top;
        } else {
            e->x = elements[par].x + e->rel_x + e->margin_left;
            e->y = elements[par].y + e->rel_y + e->margin_top;
        }
    }
}

static int child_cross_align(Element* ch, Element* cont, int row_mode) {
    (void)row_mode;
    int align = (ch->align_self >= 0) ? ch->align_self : cont->align_items;
    if (align == FLEX_ALIGN_START) return 0;
    if (align == FLEX_ALIGN_CENTER) return 1;
    if (align == FLEX_ALIGN_END) return 2;
    return 3;
}

static void place_flex_cross(Element* ch, Element* cont, int row_mode,
                             float pad, float inner_cross, float cross_len, int align) {
    (void)cont;
    if (row_mode) {
        if (align == 3 && !(ch->css_positioned & 2)) { ch->h = inner_cross; ch->rel_y = pad; return; }
        if (!(ch->css_positioned & 2)) {
            if (align == 1) ch->rel_y = pad + (inner_cross - cross_len) * 0.5f;
            else if (align == 2) ch->rel_y = pad + inner_cross - cross_len;
            else ch->rel_y = pad;
        }
    } else {
        if (align == 3 && !(ch->css_positioned & 1)) { ch->w = inner_cross; ch->rel_x = pad; return; }
        if (!(ch->css_positioned & 1)) {
            if (align == 1) ch->rel_x = pad + (inner_cross - cross_len) * 0.5f;
            else if (align == 2) ch->rel_x = pad + inner_cross - cross_len;
            else ch->rel_x = pad;
        }
    }
}

static float flex_main_size(Element* ch, int row_mode) {
    if (ch->has_flex_basis && !ch->flex_basis_auto)
        return ch->flex_basis;
    if (row_mode) {
        if (ch->has_css_width && !ch->pct_w) return ch->css_width;
        return flex_content_width(ch);
    }
    if (ch->has_css_height && !ch->pct_h) return ch->css_height;
    return flow_content_height(ch);
}

static float flex_min_main(Element* ch, int row_mode) {
    if (row_mode && ch->has_min_width) return ch->css_min_width;
    if (!row_mode && ch->has_min_height) return ch->css_min_height;
    return 0.0f;
}

static void layout_flex_line(Element* cont, int* kids, int n, int row_mode,
                             float pad_main, float pad_cross,
                             float inner_main, float inner_cross, float gap) {
    float main_sz[MAX_ELEMENTS];
    float fixed_main = 0.0f;
    int grow_n = 0;
    float grow_sum = 0.0f;

    for (int k = 0; k < n; k++) {
        Element* ch = &elements[kids[k]];
        float ml = flex_main_size(ch, row_mode);
        main_sz[k] = ml;
        int positioned = row_mode ? (ch->css_positioned & 1) : (ch->css_positioned & 2);
        if (!positioned && ch->flex_grow > 0) {
            grow_n++;
            grow_sum += (float)ch->flex_grow;
            fixed_main += ml;
        } else {
            fixed_main += ml;
        }
    }
    fixed_main += gap * (float)(n > 0 ? n - 1 : 0);
    float free_main = inner_main - fixed_main;
    if (free_main > 0.0f) {
        for (int k = 0; k < n; k++) {
            Element* ch = &elements[kids[k]];
            int positioned = row_mode ? (ch->css_positioned & 1) : (ch->css_positioned & 2);
            if (!positioned && ch->flex_grow > 0 && grow_n > 0)
                main_sz[k] += free_main * ((float)ch->flex_grow / grow_sum);
        }
    } else if (free_main < 0.0f) {
        float overflow = -free_main;
        float shrink_sum = 0.0f;
        for (int k = 0; k < n; k++) {
            Element* ch = &elements[kids[k]];
            int positioned = row_mode ? (ch->css_positioned & 1) : (ch->css_positioned & 2);
            if (positioned || ch->flex_shrink <= 0) continue;
            shrink_sum += (float)ch->flex_shrink * main_sz[k];
        }
        if (shrink_sum > 0.0f) {
            for (int k = 0; k < n; k++) {
                Element* ch = &elements[kids[k]];
                int positioned = row_mode ? (ch->css_positioned & 1) : (ch->css_positioned & 2);
                if (positioned || ch->flex_shrink <= 0) continue;
                float factor = ((float)ch->flex_shrink * main_sz[k]) / shrink_sum;
                float min_sz = flex_min_main(ch, row_mode);
                main_sz[k] -= overflow * factor;
                if (main_sz[k] < min_sz) main_sz[k] = min_sz;
            }
        }
    }

    float total = 0.0f;
    for (int k = 0; k < n; k++) total += main_sz[k];
    total += gap * (float)(n > 1 ? n - 1 : 0);

    float start = pad_main;
    float use_gap = gap;
    if (cont->justify_content == FLEX_JUSTIFY_CENTER)
        start = pad_main + (inner_main - total) * 0.5f;
    else if (cont->justify_content == FLEX_JUSTIFY_END)
        start = pad_main + inner_main - total;
    else if (cont->justify_content == FLEX_JUSTIFY_SPACE_BETWEEN && n > 1) {
        float content = 0.0f;
        for (int k = 0; k < n; k++) content += main_sz[k];
        use_gap = (inner_main - content) / (float)(n - 1);
        if (use_gap < 0.0f) use_gap = 0.0f;
        start = pad_main;
    }

    /* Auto-margin: find first item with margin_left_auto (row) or margin_top_auto (col) */
    /* This item absorbs all free space as its margin, pushing it to the end */
    int auto_margin_k = -1;
    for (int k = 0; k < n; k++) {
        Element* ch = &elements[kids[k]];
        if (row_mode && ch->margin_left_auto) { auto_margin_k = k; break; }
        if (!row_mode && ch->margin_top_auto) { auto_margin_k = k; break; }
    }
    if (auto_margin_k >= 0 && free_main > 0.0f) {
        float cursor_a = pad_main;
        for (int k = 0; k < auto_margin_k; k++) {
            Element* ch = &elements[kids[k]];
            float cross_len = row_mode ? ch->h : ch->w;
            int align = child_cross_align(ch, cont, row_mode);
            if (row_mode) {
                if (!(ch->css_positioned & 1)) { ch->w = main_sz[k]; ch->rel_x = cursor_a; }
                place_flex_cross(ch, cont, 1, pad_cross, inner_cross, cross_len, align);
                if (!(ch->css_positioned & 1)) cursor_a += main_sz[k] + use_gap;
            } else {
                if (!(ch->css_positioned & 2)) { ch->h = main_sz[k]; ch->rel_y = cursor_a; }
                place_flex_cross(ch, cont, 0, pad_cross, inner_cross, cross_len, align);
                if (!(ch->css_positioned & 2)) cursor_a += main_sz[k] + use_gap;
            }
        }
        float after_total = 0.0f;
        for (int k = auto_margin_k; k < n; k++) after_total += main_sz[k];
        after_total += use_gap * (float)(n - auto_margin_k - 1);
        float cursor_b = pad_main + inner_main - after_total;
        if (cursor_b < cursor_a) cursor_b = cursor_a;
        for (int k = auto_margin_k; k < n; k++) {
            Element* ch = &elements[kids[k]];
            float cross_len = row_mode ? ch->h : ch->w;
            int align = child_cross_align(ch, cont, row_mode);
            if (row_mode) {
                if (!(ch->css_positioned & 1)) { ch->w = main_sz[k]; ch->rel_x = cursor_b; }
                place_flex_cross(ch, cont, 1, pad_cross, inner_cross, cross_len, align);
                if (!(ch->css_positioned & 1)) cursor_b += main_sz[k] + use_gap;
            } else {
                if (!(ch->css_positioned & 2)) { ch->h = main_sz[k]; ch->rel_y = cursor_b; }
                place_flex_cross(ch, cont, 0, pad_cross, inner_cross, cross_len, align);
                if (!(ch->css_positioned & 2)) cursor_b += main_sz[k] + use_gap;
            }
        }
        return; /* skip normal placement */
    }

    float cursor = start;
    for (int k = 0; k < n; k++) {
        Element* ch = &elements[kids[k]];
        float cross_len = row_mode ? ch->h : ch->w;
        int align = child_cross_align(ch, cont, row_mode);
        if (row_mode) {
            if (!(ch->css_positioned & 1)) { ch->w = main_sz[k]; ch->rel_x = cursor; }
            place_flex_cross(ch, cont, 1, pad_cross, inner_cross, cross_len, align);
            if (!(ch->css_positioned & 1)) cursor += main_sz[k] + use_gap;
        } else {
            if (!(ch->css_positioned & 2)) { ch->h = main_sz[k]; ch->rel_y = cursor; }
            place_flex_cross(ch, cont, 0, pad_cross, inner_cross, cross_len, align);
            if (!(ch->css_positioned & 2)) cursor += main_sz[k] + use_gap;
        }
    }
}

static void layout_flex_container(int container_idx) {
    Element* cont = &elements[container_idx];
    int kids[MAX_ELEMENTS];
    int n = 0;
    for (int c = 0; c < elem_count; c++) {
        if (elements[c].parent_idx != container_idx) continue;
        if (!is_visible(c) || elements[c].position_fixed) continue;
        // Absolutely positioned children are out of flow — don't include in flex layout.
        if (elements[c].position_mode == POS_ABSOLUTE) continue;
        elements[c].flex_child = 1;
        kids[n++] = c;
    }
    if (n == 0) return;

    int row_mode = (cont->flex_direction == FLEX_DIR_ROW);
    float gap = cont->flex_gap;
    float inner_w = cont->w - cont->pad_l - cont->pad_r;
    float inner_h = cont->h - cont->pad_t - cont->pad_b;
    if (inner_w < 0.0f) inner_w = 0.0f;
    if (inner_h < 0.0f) inner_h = 0.0f;
    float inner_main = row_mode ? inner_w : inner_h;
    float inner_cross = row_mode ? inner_h : inner_w;
    /* Main axis runs along flex-direction; cross axis is perpendicular. */
    float pad_main  = row_mode ? cont->pad_l : cont->pad_t;
    float pad_cross = row_mode ? cont->pad_t : cont->pad_l;
    float pad_cross_end = row_mode ? cont->pad_b : cont->pad_r;

    if (cont->flex_wrap == FLEX_WRAP_NOWRAP || !row_mode) {
        layout_flex_line(cont, kids, n, row_mode, pad_main, pad_cross, inner_main, inner_cross, gap);
        return;
    }

    typedef struct { int start, count; float cross_sz; } FlexLineInfo;
    FlexLineInfo lines[64];
    int num_lines = 0;

    int line_start = 0;
    float line_main = 0.0f;
    for (int k = 0; k <= n; k++) {
        int flush = 0;
        if (k < n) {
            Element* ch = &elements[kids[k]];
            float item_main = flex_main_size(ch, 1) + (line_main > 0.0f ? gap : 0.0f);
            if (line_main > 0.0f && line_main + item_main > inner_main + 0.5f)
                flush = 1;
            else
                line_main += item_main;
        } else {
            flush = 1;
        }
        if (!flush) continue;

        int line_n = k - line_start;
        if (line_n > 0 && num_lines < 64) {
            float line_cross = 0.0f;
            for (int li = 0; li < line_n; li++) {
                float ch_cross = elements[kids[line_start + li]].h;
                if (ch_cross > line_cross) line_cross = ch_cross;
            }
            lines[num_lines].start = line_start;
            lines[num_lines].count = line_n;
            lines[num_lines].cross_sz = line_cross;
            num_lines++;
        }
        line_start = k;
        line_main = (k < n) ? flex_main_size(&elements[kids[k]], 1) : 0.0f;
    }

    float total_cross = 0.0f;
    for (int i = 0; i < num_lines; i++) {
        total_cross += lines[i].cross_sz;
        if (i > 0) total_cross += gap;
    }
    float cross_free = inner_cross - total_cross;
    if (cross_free < 0.0f) cross_free = 0.0f;

    float cross_start = pad_cross;
    float cross_gap = gap;
    if (cont->align_content == FLEX_ALIGN_CENTER)
        cross_start = pad_cross + cross_free * 0.5f;
    else if (cont->align_content == FLEX_ALIGN_END)
        cross_start = pad_cross + cross_free;
    else if (cont->align_content == FLEX_ALIGN_SPACE_BETWEEN && num_lines > 1) {
        float content = 0.0f;
        for (int i = 0; i < num_lines; i++) content += lines[i].cross_sz;
        cross_gap = (inner_cross - content) / (float)(num_lines - 1);
        if (cross_gap < 0.0f) cross_gap = 0.0f;
        cross_start = pad_cross;
    } else if (cont->align_content == FLEX_ALIGN_SPACE_AROUND && num_lines > 0) {
        float content = 0.0f;
        for (int i = 0; i < num_lines; i++) content += lines[i].cross_sz;
        float slack = inner_cross - content;
        if (slack < 0.0f) slack = 0.0f;
        cross_start = pad_cross + slack / (float)(num_lines * 2);
        cross_gap = gap + slack / (float)num_lines;
    } else if (cont->align_content == FLEX_ALIGN_STRETCH && num_lines > 0) {
        float extra = cross_free / (float)num_lines;
        for (int i = 0; i < num_lines; i++)
            lines[i].cross_sz += extra;
    }

    float cross_cursor = cross_start;
    for (int i = 0; i < num_lines; i++) {
        int line_kids[MAX_ELEMENTS];
        for (int li = 0; li < lines[i].count; li++)
            line_kids[li] = kids[lines[i].start + li];
        layout_flex_line(cont, line_kids, lines[i].count, 1, pad_main, pad_cross, inner_main, inner_cross, gap);
        if (cont->align_content == FLEX_ALIGN_STRETCH) {
            for (int li = 0; li < lines[i].count; li++) {
                Element* ch = &elements[line_kids[li]];
                if (!(ch->css_positioned & 2)) ch->h = lines[i].cross_sz;
            }
        }
        for (int li = 0; li < lines[i].count; li++) {
            Element* ch = &elements[line_kids[li]];
            if (!(ch->css_positioned & 2))
                ch->rel_y += cross_cursor - pad_cross;
        }
        cross_cursor += lines[i].cross_sz;
        if (i < num_lines - 1) cross_cursor += cross_gap;
    }

    if (cont->flex_wrap == FLEX_WRAP_WRAP && row_mode && !cont->has_css_height && !cont->pct_h) {
        float needed_h = cross_cursor + pad_cross_end;
        if (cont->has_min_height && needed_h < cont->css_min_height)
            needed_h = cont->css_min_height;
        if (needed_h > cont->h) cont->h = needed_h;
    }
}

static void resolve_grid_tracks(float inner, int count, const float* track, const int* types,
                                const float* mins, float gap, float* out_sizes) {
    float fixed = 0.0f, fr_sum = 0.0f;
    for (int i = 0; i < count; i++) {
        if (types[i] == GRID_TRACK_FR || types[i] == GRID_TRACK_MINMAX)
            fr_sum += track[i];
        else
            fixed += track[i];
    }
    fixed += gap * (float)(count > 0 ? count - 1 : 0);
    float fr_unit = (fr_sum > 0.0f) ? (inner - fixed) / fr_sum : 0.0f;
    if (fr_unit < 0.0f) fr_unit = 0.0f;
    for (int i = 0; i < count; i++) {
        if (types[i] == GRID_TRACK_FR)
            out_sizes[i] = track[i] * fr_unit;
        else if (types[i] == GRID_TRACK_MINMAX) {
            float sz = track[i] * fr_unit;
            if (sz < mins[i]) sz = mins[i];
            out_sizes[i] = sz;
        } else
            out_sizes[i] = track[i];
    }
}

static void grid_axis_align(float inner, int count, float* sizes, float gap, int mode,
                            float* offset, float* out_gap) {
    float total = 0.0f;
    for (int i = 0; i < count; i++) total += sizes[i];
    if (count > 1) total += gap * (float)(count - 1);
    float slack = inner - total;
    if (slack < 0.0f) slack = 0.0f;
    *offset = 0.0f;
    *out_gap = gap;
    if (mode == FLEX_ALIGN_CENTER)
        *offset = slack * 0.5f;
    else if (mode == FLEX_ALIGN_END)
        *offset = slack;
    else if (mode == FLEX_ALIGN_STRETCH && count > 0 && slack > 0.0f) {
        float extra = slack / (float)count;
        for (int i = 0; i < count; i++) sizes[i] += extra;
    } else if (mode == FLEX_ALIGN_SPACE_BETWEEN && count > 1) {
        float content = 0.0f;
        for (int i = 0; i < count; i++) content += sizes[i];
        *out_gap = (inner - content) / (float)(count - 1);
        if (*out_gap < 0.0f) *out_gap = 0.0f;
    } else if (mode == FLEX_ALIGN_SPACE_AROUND && count > 0) {
        float content = 0.0f;
        for (int i = 0; i < count; i++) content += sizes[i];
        float s = inner - content;
        if (s < 0.0f) s = 0.0f;
        *offset = s / (float)(count * 2);
        *out_gap = gap + s / (float)count;
    }
}

static int grid_can_place(int occ[MAX_GRID_AREA_ROWS][MAX_GRID_AREA_COLS],
                          int rows, int cols, int r, int c, int rs, int cs) {
    if (r < 0 || c < 0 || r + rs > rows || c + cs > cols) return 0;
    for (int rr = r; rr < r + rs; rr++)
        for (int cc = c; cc < c + cs; cc++)
            if (occ[rr][cc]) return 0;
    return 1;
}

static void grid_mark_cells(int occ[MAX_GRID_AREA_ROWS][MAX_GRID_AREA_COLS],
                            int r, int c, int rs, int cs, int val) {
    for (int rr = r; rr < r + rs && rr < MAX_GRID_AREA_ROWS; rr++)
        for (int cc = c; cc < c + cs && cc < MAX_GRID_AREA_COLS; cc++)
            occ[rr][cc] = val;
}

static int grid_find_auto_slot(int occ[MAX_GRID_AREA_ROWS][MAX_GRID_AREA_COLS],
                               int rows, int cols, int rs, int cs,
                               int col_flow, int dense, int* out_r, int* out_c) {
    (void)dense;
    if (col_flow) {
        for (int cc = 0; cc < cols; cc++)
            for (int rr = 0; rr < rows; rr++) {
                if (grid_can_place(occ, rows, cols, rr, cc, rs, cs)) {
                    *out_r = rr; *out_c = cc; return 1;
                }
            }
    } else {
        for (int rr = 0; rr < rows; rr++)
            for (int cc = 0; cc < cols; cc++) {
                if (grid_can_place(occ, rows, cols, rr, cc, rs, cs)) {
                    *out_r = rr; *out_c = cc; return 1;
                }
            }
    }
    return 0;
}

static void grid_advance_cursor(int* ac, int* ar, int rs, int cs,
                                int rows, int cols, int col_flow,
                                int occ[MAX_GRID_AREA_ROWS][MAX_GRID_AREA_COLS]) {
    for (int attempt = 0; attempt < rows * cols; attempt++) {
        if (grid_can_place(occ, rows, cols, *ar, *ac, rs, cs)) return;
        if (col_flow) {
            *ar += rs;
            if (*ar + rs > rows) { *ar = 0; *ac += cs; }
        } else {
            *ac += cs;
            if (*ac + cs > cols) { *ac = 0; *ar += rs; }
        }
        if (*ac >= cols) *ac = 0;
        if (*ar >= rows) *ar = 0;
    }
}

static void place_grid_item(Element* ch, Element* cont,
                            float cx, float cy, float cw, float chh) {
    int jalign = (ch->justify_self >= 0) ? ch->justify_self : cont->justify_items;
    int aalign = (ch->align_self >= 0) ? ch->align_self : cont->align_items;
    float item_w = ch->w;
    float item_h = ch->h;
    if (jalign == FLEX_ALIGN_STRETCH && !(ch->css_positioned & 1)) item_w = cw;
    if (aalign == FLEX_ALIGN_STRETCH && !(ch->css_positioned & 2)) item_h = chh;
    float off_x = 0.0f, off_y = 0.0f;
    if (jalign == FLEX_ALIGN_CENTER) off_x = (cw - item_w) * 0.5f;
    else if (jalign == FLEX_ALIGN_END) off_x = cw - item_w;
    if (aalign == FLEX_ALIGN_CENTER) off_y = (chh - item_h) * 0.5f;
    else if (aalign == FLEX_ALIGN_END) off_y = chh - item_h;
    if (off_x < 0.0f) off_x = 0.0f;
    if (off_y < 0.0f) off_y = 0.0f;
    if (!(ch->css_positioned & 1)) { ch->rel_x = cx + off_x; ch->w = item_w; }
    if (!(ch->css_positioned & 2)) { ch->rel_y = cy + off_y; ch->h = item_h; }
}

typedef struct {
    int idx, gc, gr, cspan, rspan;
} GridPlace;

static void layout_grid_container(int container_idx) {
    Element* cont = &elements[container_idx];
    if (cont->grid_area_rect_count == 0 && cont->grid_area_rows > 0)
        compile_grid_area_rects(cont);

    int tmpl_cols = cont->grid_col_count;
    int tmpl_rows = cont->grid_row_count;
    if (tmpl_cols < 1) tmpl_cols = cont->has_grid_auto_columns ? 0 : 1;
    if (tmpl_rows < 1) tmpl_rows = cont->has_grid_auto_rows ? 0 : 1;

    float pad = cont->padding;
    float col_gap = cont->grid_col_gap > 0.0f ? cont->grid_col_gap : cont->flex_gap;
    float row_gap = cont->grid_row_gap > 0.0f ? cont->grid_row_gap : cont->flex_gap;

    int col_flow = (cont->grid_auto_flow & GRID_AUTO_FLOW_COLUMN) != 0;
    int dense = (cont->grid_auto_flow & GRID_AUTO_FLOW_DENSE) != 0;
    int occupied[MAX_GRID_AREA_ROWS][MAX_GRID_AREA_COLS];
    memset(occupied, 0, sizeof(occupied));

    GridPlace places[MAX_ELEMENTS];
    int place_n = 0;
    int auto_col = 0, auto_row = 0;
    int max_col_end = tmpl_cols > 0 ? tmpl_cols : 1;
    int max_row_end = tmpl_rows > 0 ? tmpl_rows : 1;

    for (int c = 0; c < elem_count; c++) {
        if (elements[c].parent_idx != container_idx) continue;
        if (!is_visible(c) || elements[c].position_fixed) continue;
        if (elements[c].position_mode == POS_ABSOLUTE) continue;
        Element* ch = &elements[c];
        ch->grid_child = 1;

        int cspan = ch->grid_col_span > 0 ? ch->grid_col_span : 1;
        int rspan = ch->grid_row_span > 0 ? ch->grid_row_span : 1;
        int gc = ch->has_grid_col ? ch->grid_col : -1;
        int gr = ch->has_grid_row ? ch->grid_row : -1;
        if (ch->has_grid_area && ch->grid_area_name[0])
            grid_area_lookup(cont, ch->grid_area_name, &gc, &gr, &cspan, &rspan);

        int auto_place = !ch->has_grid_col && !ch->has_grid_row && !ch->has_grid_area;
        int work_cols = max_col_end > 0 ? max_col_end : MAX_GRID_AREA_COLS;
        int work_rows = max_row_end > 0 ? max_row_end : MAX_GRID_AREA_ROWS;
        if (work_cols > MAX_GRID_AREA_COLS) work_cols = MAX_GRID_AREA_COLS;
        if (work_rows > MAX_GRID_AREA_ROWS) work_rows = MAX_GRID_AREA_ROWS;

        if (auto_place) {
            if (dense) {
                int found = 0;
                for (int attempt = 0; attempt < 32 && !found; attempt++) {
                    work_cols = max_col_end > 0 ? max_col_end : 1;
                    work_rows = max_row_end > 0 ? max_row_end : 1;
                    if (work_cols > MAX_GRID_AREA_COLS) work_cols = MAX_GRID_AREA_COLS;
                    if (work_rows > MAX_GRID_AREA_ROWS) work_rows = MAX_GRID_AREA_ROWS;
                    if (grid_find_auto_slot(occupied, work_rows, work_cols, rspan, cspan, col_flow, 1, &gr, &gc)) {
                        found = 1;
                        break;
                    }
                    if (col_flow) {
                        max_row_end++;
                        if (max_row_end > MAX_GRID_AREA_ROWS) break;
                    } else {
                        max_col_end++;
                        if (max_col_end > MAX_GRID_AREA_COLS) break;
                    }
                }
                if (!found) continue;
            } else {
                grid_advance_cursor(&auto_col, &auto_row, rspan, cspan, work_rows, work_cols, col_flow, occupied);
                gc = auto_col;
                gr = auto_row;
            }
        }

        if (gc < 0) gc = 0;
        if (gr < 0) gr = 0;

        if (auto_place && !dense) {
            if (col_flow) {
                auto_row = gr + rspan;
                if (auto_row + rspan > work_rows) { auto_row = 0; auto_col = gc + cspan; }
            } else {
                auto_col = gc + cspan;
                if (auto_col + cspan > work_cols) { auto_col = 0; auto_row = gr + rspan; }
            }
        }

        int col_end = gc + cspan;
        int row_end = gr + rspan;
        if (col_end > max_col_end) max_col_end = col_end;
        if (row_end > max_row_end) max_row_end = row_end;

        int pc = col_end > MAX_GRID_AREA_COLS ? MAX_GRID_AREA_COLS : col_end;
        int pr = row_end > MAX_GRID_AREA_ROWS ? MAX_GRID_AREA_ROWS : row_end;
        grid_mark_cells(occupied, gr, gc, rspan, cspan, 1);

        if (place_n < MAX_ELEMENTS) {
            places[place_n].idx = c;
            places[place_n].gc = gc;
            places[place_n].gr = gr;
            places[place_n].cspan = cspan;
            places[place_n].rspan = rspan;
            place_n++;
        }
        (void)pc; (void)pr;
    }

    int cols = max_col_end;
    int rows = max_row_end;
    if (cols < 1) cols = 1;
    if (rows < 1) rows = 1;
    if (cols > MAX_GRID_AREA_COLS) cols = MAX_GRID_AREA_COLS;
    if (rows > MAX_GRID_AREA_ROWS) rows = MAX_GRID_AREA_ROWS;

    float inner_w = cont->w - pad * 2.0f;
    float inner_h = cont->h - pad * 2.0f;
    if (inner_w < 0.0f) inner_w = 0.0f;
    if (inner_h < 0.0f) inner_h = 0.0f;

    float col_sz[MAX_GRID_TRACKS], row_sz[MAX_GRID_TRACKS];
    float col_tr[MAX_GRID_TRACKS], row_tr[MAX_GRID_TRACKS];
    int col_ty[MAX_GRID_TRACKS], row_ty[MAX_GRID_TRACKS];
    float col_mn[MAX_GRID_TRACKS], row_mn[MAX_GRID_TRACKS];

    for (int i = 0; i < cols; i++) {
        if (i < tmpl_cols && tmpl_cols > 0) {
            col_tr[i] = cont->grid_col_track[i];
            col_ty[i] = cont->grid_col_type[i];
            col_mn[i] = cont->grid_col_min[i];
        } else {
            col_tr[i] = cont->has_grid_auto_columns ? cont->grid_auto_col_track : 1.0f;
            col_ty[i] = cont->has_grid_auto_columns ? cont->grid_auto_col_type : GRID_TRACK_FR;
            col_mn[i] = cont->has_grid_auto_columns ? cont->grid_auto_col_min : 0.0f;
        }
        if (col_ty[i] == GRID_TRACK_PX && col_tr[i] <= 0.0f) col_tr[i] = inner_w / (float)cols;
    }
    for (int i = 0; i < rows; i++) {
        if (i < tmpl_rows && tmpl_rows > 0) {
            row_tr[i] = cont->grid_row_track[i];
            row_ty[i] = cont->grid_row_type[i];
            row_mn[i] = cont->grid_row_min[i];
        } else {
            row_tr[i] = cont->has_grid_auto_rows ? cont->grid_auto_row_track : 1.0f;
            row_ty[i] = cont->has_grid_auto_rows ? cont->grid_auto_row_type : GRID_TRACK_FR;
            row_mn[i] = cont->has_grid_auto_rows ? cont->grid_auto_row_min : 0.0f;
        }
        if (row_ty[i] == GRID_TRACK_PX && row_tr[i] <= 0.0f) row_tr[i] = inner_h / (float)rows;
    }

    resolve_grid_tracks(inner_w, cols, col_tr, col_ty, col_mn, col_gap, col_sz);
    resolve_grid_tracks(inner_h, rows, row_tr, row_ty, row_mn, row_gap, row_sz);

    float row_off = 0.0f, col_off = 0.0f;
    float use_row_gap = row_gap, use_col_gap = col_gap;
    grid_axis_align(inner_h, rows, row_sz, row_gap, cont->align_content, &row_off, &use_row_gap);
    grid_axis_align(inner_w, cols, col_sz, col_gap, cont->justify_content, &col_off, &use_col_gap);

    for (int pi = 0; pi < place_n; pi++) {
        GridPlace* pl = &places[pi];
        Element* ch = &elements[pl->idx];
        int gc = pl->gc, gr = pl->gr;
        int cspan = pl->cspan, rspan = pl->rspan;
        if (gc + cspan > cols) gc = cols - cspan;
        if (gr + rspan > rows) gr = rows - rspan;
        if (gc < 0) gc = 0;
        if (gr < 0) gr = 0;

        float cx = pad + col_off, cy = pad + row_off, cw = 0.0f, chh = 0.0f;
        for (int i = 0; i < gc; i++) cx += col_sz[i] + use_col_gap;
        for (int i = 0; i < gr; i++) cy += row_sz[i] + use_row_gap;
        for (int i = gc; i < gc + cspan && i < cols; i++) cw += col_sz[i];
        for (int i = gr; i < gr + rspan && i < rows; i++) chh += row_sz[i];
        cw += use_col_gap * (float)(cspan - 1);
        chh += use_row_gap * (float)(rspan - 1);

        place_grid_item(ch, cont, cx, cy, cw, chh);
    }
}

static void layout_flex_containers(void) {
    for (int i = 0; i < elem_count; i++) {
        elements[i].flex_child = 0;
        elements[i].grid_child = 0;
        elements[i].flow_child = 0;
    }
    /* Single top-down pass in document order (parent index < child index),
       so a container's own width/height is finalized by its parent before its
       children are sized. Dispatch each container by display mode; mixing flex
       and block containers in one ordered pass ensures width propagates
       parent -> child (e.g. content(block) -> tab_panel(flex) -> toolbar). */
    for (int i = 0; i < elem_count; i++) {
        if (!is_visible(i)) continue;
        if (elements[i].display_mode == DISPLAY_FLEX)
            layout_flex_container(i);
        else if (elements[i].display_mode == DISPLAY_GRID)
            layout_grid_container(i);
        else
            layout_block_container(i);
    }
    for (int i = 0; i < elem_count; i++) {
        Element* e = &elements[i];
        int par = e->parent_idx;
        if (par == -1 || e->position_fixed) continue;
        if (!e->flex_child && !e->grid_child && !e->flow_child) continue;
        e->x = elements[par].x + e->rel_x + e->margin_left;
        e->y = elements[par].y + e->rel_y + e->margin_top;
    }
}

static void apply_scroll_offsets(void);
static void apply_scroll_metrics(void);
static void apply_sticky_positions(void);

void update_layout_pass(void) {
    update_layout();
    layout_flex_containers();
    apply_scroll_metrics();
    apply_scroll_offsets();
    apply_sticky_positions();
    /* Resolve percentage-based transforms after element sizes are known */
    for (int i = 0; i < elem_count; i++) {
        Element* e = &elements[i];
        if (e->transform_tx_pct) e->transform_tx = e->raw_transform_tx * e->w;
        if (e->transform_ty_pct) e->transform_ty = e->raw_transform_ty * e->h;
    }
}

static void apply_sticky_positions(void) {
    for (int i = 0; i < elem_count; i++) {
        Element* e = &elements[i];
        if (!e->position_sticky || !is_visible(i)) continue;

        int scroll_y = -1, scroll_x = -1;
        for (int p = e->parent_idx; p != -1; p = elements[p].parent_idx) {
            if (scroll_y == -1 && overflow_scrollable(elements[p].overflow_y))
                scroll_y = p;
            if (scroll_x == -1 && overflow_scrollable(elements[p].overflow_x))
                scroll_x = p;
        }

        if (scroll_y != -1) {
            Element* par = &elements[scroll_y];
            if (e->sticky_use_bottom) {
                float stick_y = par->y + par->h - par->pad_b - e->h - e->sticky_bottom;
                if (e->y > stick_y) e->y = stick_y;
            } else if (e->sticky_use_top) {
                float min_y = par->y + par->pad_t + e->sticky_top;
                float max_y = par->y + par->h - par->pad_b - e->h;
                if (max_y < min_y) max_y = min_y;
                if (e->y < min_y) e->y = min_y;
                if (e->y > max_y) e->y = max_y;
            }
        }

        if (scroll_x != -1) {
            Element* par = &elements[scroll_x];
            if (e->sticky_use_left) {
                float min_x = par->x + par->pad_l + e->sticky_left;
                if (e->x < min_x) e->x = min_x;
            } else if (e->sticky_use_right) {
                float stick_x = par->x + par->w - par->pad_r - e->w - e->sticky_right;
                if (e->x > stick_x) e->x = stick_x;
            }
        }
    }
}

static float scroll_offset_x(int idx) {
    float s = 0.0f;
    int p = elements[idx].parent_idx;
    while (p != -1) {
        if (overflow_scrollable(elements[p].overflow_x))
            s += elements[p].scroll_left;
        p = elements[p].parent_idx;
    }
    return s;
}

static float scroll_offset_y(int idx) {
    float s = 0.0f;
    int p = elements[idx].parent_idx;
    while (p != -1) {
        if (overflow_scrollable(elements[p].overflow_y))
            s += elements[p].scroll_top;
        p = elements[p].parent_idx;
    }
    return s;
}

static void apply_scroll_offsets(void) {
    for (int i = 0; i < elem_count; i++) {
        if (elements[i].position_fixed || elements[i].parent_idx == -1) continue;
        elements[i].x -= scroll_offset_x(i);
        elements[i].y -= scroll_offset_y(i);
    }
}

static void apply_scroll_metrics(void) {
    for (int i = 0; i < elem_count; i++) {
        Element* c = &elements[i];
        if (!overflow_scrollable(c->overflow_y) && !overflow_scrollable(c->overflow_x)) continue;
        float pad = c->padding;
        float inner_h = c->h - pad * 2.0f;
        float inner_w = c->w - pad * 2.0f;
        if (inner_h < 0.0f) inner_h = 0.0f;
        if (inner_w < 0.0f) inner_w = 0.0f;
        float content_bottom = 0.0f, content_right = 0.0f;
        for (int ch = 0; ch < elem_count; ch++) {
            if (elements[ch].parent_idx != i) continue;
            if (!is_visible(ch)) continue;
            float bottom = elements[ch].rel_y + elements[ch].h;
            float right  = elements[ch].rel_x + elements[ch].w;
            if (bottom > content_bottom) content_bottom = bottom;
            if (right > content_right) content_right = right;
        }
        c->scroll_content_h = content_bottom;
        c->scroll_content_w = content_right;
        float max_scroll_y = content_bottom - inner_h;
        float max_scroll_x = content_right - inner_w;
        if (max_scroll_y < 0.0f) max_scroll_y = 0.0f;
        if (max_scroll_x < 0.0f) max_scroll_x = 0.0f;
        if (c->scroll_top > max_scroll_y) { c->scroll_top = max_scroll_y; c->scroll_dest_top = max_scroll_y; }
        if (c->scroll_left > max_scroll_x) { c->scroll_left = max_scroll_x; c->scroll_dest_left = max_scroll_x; }
        if (c->scroll_top < 0.0f) { c->scroll_top = 0.0f; c->scroll_dest_top = 0.0f; }
        if (c->scroll_left < 0.0f) { c->scroll_left = 0.0f; c->scroll_dest_left = 0.0f; }
    }
}

static int overflow_scrolls_y(int idx) {
    return overflow_scrollable(elements[idx].overflow_y);
}

static int overflow_scrolls_x(int idx) {
    return overflow_scrollable(elements[idx].overflow_x);
}

static int find_scroll_target_y(int idx) {
    while (idx != -1) {
        if (overflow_scrolls_y(idx)) return idx;
        idx = elements[idx].parent_idx;
    }
    return -1;
}

static int find_scroll_target_x(int idx) {
    while (idx != -1) {
        if (overflow_scrolls_x(idx)) return idx;
        idx = elements[idx].parent_idx;
    }
    return -1;
}

static void scrollbar_geom_y(Element* c, float* tx, float* ty, float* tw, float* th,
                             float* ux, float* uy, float* uw, float* uh, int* visible) {
    *visible = 0;
    float pad = c->padding;
    float inner_h = c->h - pad * 2.0f;
    if (!overflow_scrollable(c->overflow_y) || inner_h <= 0.0f) return;
    int needs_bar = (c->scroll_content_h > inner_h + 1.0f) || c->overflow_y == OVERFLOW_SCROLL;
    if (!needs_bar) return;
    float sbw = element_sb_width(c);
    if (sbw <= 0.0f) return;
    *visible = 1;
    *tw = sbw;
    *th = inner_h;
    *tx = c->x + c->w - pad - *tw - 2.0f;
    *ty = c->y + pad;
    float ratio = inner_h / c->scroll_content_h;
    *uh = *th * ratio;
    if (*uh < 14.0f) *uh = 14.0f;
    float max_scroll = c->scroll_content_h - inner_h;
    if (max_scroll < 0.0f) max_scroll = 0.0f;
    float scroll_range = *th - *uh;
    *uy = *ty + (max_scroll > 0.0f && scroll_range > 0.0f
                  ? (c->scroll_top / max_scroll) * scroll_range : 0.0f);
    *ux = *tx;
    *uw = *tw;
}

static void scrollbar_geom_x(Element* c, float* tx, float* ty, float* tw, float* th,
                             float* ux, float* uy, float* uw, float* uh, int* visible) {
    *visible = 0;
    float pad = c->padding;
    float inner_w = c->w - pad * 2.0f;
    if (!overflow_scrollable(c->overflow_x) || inner_w <= 0.0f) return;
    int needs_bar = (c->scroll_content_w > inner_w + 1.0f) || c->overflow_x == OVERFLOW_SCROLL;
    if (!needs_bar) return;
    float sbw = element_sb_width(c);
    if (sbw <= 0.0f) return;
    *visible = 1;
    *th = sbw;
    *tw = inner_w;
    *tx = c->x + pad;
    *ty = c->y + c->h - pad - *th - 2.0f;
    float ratio = inner_w / c->scroll_content_w;
    *uw = *tw * ratio;
    if (*uw < 14.0f) *uw = 14.0f;
    float max_scroll = c->scroll_content_w - inner_w;
    if (max_scroll < 0.0f) max_scroll = 0.0f;
    float scroll_range = *tw - *uw;
    *ux = *tx + (max_scroll > 0.0f && scroll_range > 0.0f
                  ? (c->scroll_left / max_scroll) * scroll_range : 0.0f);
    *uy = *ty;
    *uh = *th;
}

static int hit_scrollbar_thumb_y(int idx, double mx, double my) {
    Element* c = &elements[idx];
    float tx, ty, tw, th, ux, uy, uw, uh;
    int vis = 0;
    scrollbar_geom_y(c, &tx, &ty, &tw, &th, &ux, &uy, &uw, &uh, &vis);
    if (!vis) return 0;
    return (mx >= ux && mx <= ux + uw && my >= uy && my <= uy + uh);
}

static int hit_scrollbar_thumb_x(int idx, double mx, double my) {
    Element* c = &elements[idx];
    float tx, ty, tw, th, ux, uy, uw, uh;
    int vis = 0;
    scrollbar_geom_x(c, &tx, &ty, &tw, &th, &ux, &uy, &uw, &uh, &vis);
    if (!vis) return 0;
    return (mx >= ux && mx <= ux + uw && my >= uy && my <= uy + uh);
}

static int hit_scrollbar_track_y(int idx, double mx, double my) {
    Element* c = &elements[idx];
    float tx, ty, tw, th, ux, uy, uw, uh;
    int vis = 0;
    scrollbar_geom_y(c, &tx, &ty, &tw, &th, &ux, &uy, &uw, &uh, &vis);
    if (!vis) return 0;
    if (mx < tx || mx > tx + tw || my < ty || my > ty + th) return 0;
    return !(mx >= ux && mx <= ux + uw && my >= uy && my <= uy + uh);
}

static int hit_scrollbar_track_x(int idx, double mx, double my) {
    Element* c = &elements[idx];
    float tx, ty, tw, th, ux, uy, uw, uh;
    int vis = 0;
    scrollbar_geom_x(c, &tx, &ty, &tw, &th, &ux, &uy, &uw, &uh, &vis);
    if (!vis) return 0;
    if (mx < tx || mx > tx + tw || my < ty || my > ty + th) return 0;
    return !(mx >= ux && mx <= ux + uw && my >= uy && my <= uy + uh);
}

static void clamp_scroll_y(int idx) {
    Element* sc = &elements[idx];
    float pad = sc->padding;
    float inner_h = sc->h - pad * 2.0f;
    float max_scroll = sc->scroll_content_h - inner_h;
    if (max_scroll < 0.0f) max_scroll = 0.0f;
    if (sc->scroll_top < 0.0f) sc->scroll_top = 0.0f;
    if (sc->scroll_top > max_scroll) sc->scroll_top = max_scroll;
    if (sc->scroll_dest_top < 0.0f) sc->scroll_dest_top = 0.0f;
    if (sc->scroll_dest_top > max_scroll) sc->scroll_dest_top = max_scroll;
}

static void clamp_scroll_x(int idx) {
    Element* sc = &elements[idx];
    float pad = sc->padding;
    float inner_w = sc->w - pad * 2.0f;
    float max_scroll = sc->scroll_content_w - inner_w;
    if (max_scroll < 0.0f) max_scroll = 0.0f;
    if (sc->scroll_left < 0.0f) sc->scroll_left = 0.0f;
    if (sc->scroll_left > max_scroll) sc->scroll_left = max_scroll;
    if (sc->scroll_dest_left < 0.0f) sc->scroll_dest_left = 0.0f;
    if (sc->scroll_dest_left > max_scroll) sc->scroll_dest_left = max_scroll;
}

static void apply_scroll_snap_y(int idx);

static void set_scroll_top(int idx, float val, int instant) {
    Element* sc = &elements[idx];
    sc->scroll_dest_top = val;
    if (instant || !sc->scroll_smooth) sc->scroll_top = val;
    clamp_scroll_y(idx);
}

static void set_scroll_left(int idx, float val, int instant) {
    Element* sc = &elements[idx];
    sc->scroll_dest_left = val;
    if (instant || !sc->scroll_smooth) sc->scroll_left = val;
    clamp_scroll_x(idx);
}

static void add_scroll_top(int idx, float delta, int instant) {
    set_scroll_top(idx, elements[idx].scroll_dest_top + delta, instant);
    if (instant && elements[idx].scroll_snap_type)
        apply_scroll_snap_y(idx);
}

static void add_scroll_left(int idx, float delta, int instant) {
    set_scroll_left(idx, elements[idx].scroll_dest_left + delta, instant);
}

static void apply_scroll_snap_y(int idx) {
    Element* sc = &elements[idx];
    if (!sc->scroll_snap_type) return;
    float pad = sc->padding;
    float inner_h = sc->h - pad * 2.0f;
    float max_scroll = sc->scroll_content_h - inner_h;
    if (max_scroll < 0.0f) max_scroll = 0.0f;
    float cur = sc->scroll_dest_top;
    float best = cur;
    float best_dist = 1e9f;
    for (int i = 0; i < elem_count; i++) {
        if (elements[i].parent_idx != idx || elements[i].display_none) continue;
        if (!is_visible(i)) continue;
        Element* ch = &elements[i];
        float cy = ch->rel_y + ch->margin_top;
        float snap;
        if (ch->scroll_snap_align == 1)
            snap = cy + ch->h * 0.5f - inner_h * 0.5f;
        else if (ch->scroll_snap_align == 2)
            snap = cy + ch->h - inner_h;
        else
            snap = cy;
        if (snap < 0.0f) snap = 0.0f;
        if (snap > max_scroll) snap = max_scroll;
        float dist = fabsf(cur - snap);
        if (dist < best_dist) { best_dist = dist; best = snap; }
    }
    if (sc->scroll_snap_type == 2 && best_dist > 24.0f) return;
    if (fabsf(best - cur) > 0.5f)
        set_scroll_top(idx, best, 1);
}

void tick_smooth_scroll(double dt) {
    float k = 1.0f - expf(-(float)dt * 14.0f);
    if (k > 1.0f) k = 1.0f;
    for (int i = 0; i < elem_count; i++) {
        Element* c = &elements[i];
        if (!c->scroll_smooth && !c->scroll_snap_type) continue;
        float dy = c->scroll_dest_top - c->scroll_top;
        float dx = c->scroll_dest_left - c->scroll_left;
        if (c->scroll_smooth) {
            if (fabsf(dy) > 0.25f) c->scroll_top += dy * k;
            else c->scroll_top = c->scroll_dest_top;
            if (fabsf(dx) > 0.25f) c->scroll_left += dx * k;
            else c->scroll_left = c->scroll_dest_left;
        }
        if (c->scroll_snap_type && fabsf(c->scroll_dest_top - c->scroll_top) <= 0.25f)
            apply_scroll_snap_y(i);
    }
}

static void scroll_track_click_y(int idx, double mx, double my) {
    (void)mx;
    Element* sc = &elements[idx];
    float pad = sc->padding;
    float inner_h = sc->h - pad * 2.0f;
    float tx, ty, tw, th, ux, uy, uw, uh;
    int vis = 0;
    scrollbar_geom_y(sc, &tx, &ty, &tw, &th, &ux, &uy, &uw, &uh, &vis);
    if (!vis) return;
    float page = inner_h * 0.85f;
    float max_scroll = sc->scroll_content_h - inner_h;
    if (max_scroll < 0.0f) max_scroll = 0.0f;
    if ((float)my < uy)
        add_scroll_top(idx, -page, 0);
    else if ((float)my > uy + uh)
        add_scroll_top(idx, page, 0);
    else {
        float scroll_range = th - uh;
        if (scroll_range > 0.0f && max_scroll > 0.0f) {
            float ratio = ((float)my - uh * 0.5f - ty) / scroll_range;
            if (ratio < 0.0f) ratio = 0.0f;
            if (ratio > 1.0f) ratio = 1.0f;
            set_scroll_top(idx, ratio * max_scroll, 0);
        }
    }
}

static void scroll_track_click_x(int idx, double mx, double my) {
    (void)my;
    Element* sc = &elements[idx];
    float pad = sc->padding;
    float inner_w = sc->w - pad * 2.0f;
    float tx, ty, tw, th, ux, uy, uw, uh;
    int vis = 0;
    scrollbar_geom_x(sc, &tx, &ty, &tw, &th, &ux, &uy, &uw, &uh, &vis);
    if (!vis) return;
    float page = inner_w * 0.85f;
    float max_scroll = sc->scroll_content_w - inner_w;
    if (max_scroll < 0.0f) max_scroll = 0.0f;
    if ((float)mx < ux)
        add_scroll_left(idx, -page, 0);
    else if ((float)mx > ux + uw)
        add_scroll_left(idx, page, 0);
    else {
        float scroll_range = tw - uw;
        if (scroll_range > 0.0f && max_scroll > 0.0f) {
            float ratio = ((float)mx - uw * 0.5f - tx) / scroll_range;
            if (ratio < 0.0f) ratio = 0.0f;
            if (ratio > 1.0f) ratio = 1.0f;
            set_scroll_left(idx, ratio * max_scroll, 0);
        }
    }
}

static int hit_test_at(double xpos, double ypos);

void scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);
    int hit = hit_test_at(mx, my);
    int shift = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS;
    if (fabs(yoffset) > 0.001 && shift) {
        int scroll_idx = find_scroll_target_x(hit);
        if (scroll_idx != -1)
            add_scroll_left(scroll_idx, -(float)yoffset * 18.0f, 0);
    } else if (fabs(yoffset) > 0.001) {
        int scroll_idx = find_scroll_target_y(hit);
        if (scroll_idx != -1) {
            add_scroll_top(scroll_idx, -(float)yoffset * 18.0f, 0);
        }
    }
    if (fabs(xoffset) > 0.001) {
        int scroll_idx = find_scroll_target_x(hit);
        if (scroll_idx != -1) {
            add_scroll_left(scroll_idx, -(float)xoffset * 18.0f, 0);
        }
    }
}

static float element_effective_opacity(int idx) {
    float op = 1.0f;
    while (idx != -1) {
        op *= elements[idx].opacity;
        idx = elements[idx].parent_idx;
    }
    return op;
}

/* Sum of cur_tx/cur_ty of all ANCESTORS (excluding idx itself). A CSS transform
   on an element establishes a coordinate system for its descendants, so a
   child's on-screen position must include every ancestor's translate. Layout
   x/y are transform-free (parent.x + rel_x); the transform offset is applied
   here at draw time so animated transforms don't require re-layout. */
static void accum_ancestor_transform(int idx, float* tx, float* ty) {
    float ax = 0.0f, ay = 0.0f;
    for (int p = elements[idx].parent_idx; p != -1; p = elements[p].parent_idx) {
        ax += elements[p].cur_tx;
        ay += elements[p].cur_ty;
    }
    *tx = ax; *ty = ay;
}

static void get_element_draw_bounds(Element* e, float* out_x, float* out_y, float* out_w, float* out_h) {
    float scale = e->cur_scale;
    float dw = e->w * scale, dh = e->h * scale;
    float atx, aty;
    accum_ancestor_transform((int)(e - elements), &atx, &aty);
    *out_x = e->x + (e->w - dw) * 0.5f + e->cur_tx + atx;
    *out_y = e->y + (e->h - dh) * 0.5f + e->cur_ty + aty;
    *out_w = dw;
    *out_h = dh;
}

static int get_overflow_clip_rect(int idx, float* cx, float* cy, float* cw, float* ch) {
    int has = 0;
    int p = elements[idx].parent_idx;
    while (p != -1) {
        Element* par = &elements[p];
        if (overflow_clips(par->overflow_x) || overflow_clips(par->overflow_y)) {
            float ptx, pty;
            accum_ancestor_transform(p, &ptx, &pty);
            ptx += par->cur_tx; pty += par->cur_ty;
            float px = par->x + par->pad_l + ptx, py = par->y + par->pad_t + pty;
            float pw = par->w - par->pad_l - par->pad_r, ph = par->h - par->pad_t - par->pad_b;
            if (pw <= 0.0f || ph <= 0.0f) return 0;
            float clip_x = overflow_clips(par->overflow_x) ? px : -1e7f;
            float clip_y = overflow_clips(par->overflow_y) ? py : -1e7f;
            float clip_w = overflow_clips(par->overflow_x) ? pw : 2e7f;
            float clip_h = overflow_clips(par->overflow_y) ? ph : 2e7f;
            if (!has) {
                *cx = clip_x; *cy = clip_y; *cw = clip_w; *ch = clip_h;
                has = 1;
            } else {
                float nx = (*cx > clip_x) ? *cx : clip_x;
                float ny = (*cy > clip_y) ? *cy : clip_y;
                float nr = (*cx + *cw < clip_x + clip_w) ? *cx + *cw : clip_x + clip_w;
                float nb = (*cy + *ch < clip_y + clip_h) ? *cy + *ch : clip_y + clip_h;
                *cx = nx; *cy = ny;
                *cw = nr - nx; *ch = nb - ny;
            }
            if (*cw <= 0.0f || *ch <= 0.0f) return 0;
        }
        p = par->parent_idx;
    }
    return has;
}

static void set_element_scissor(int idx, int fbw, int fbh) {
    float cx, cy, cw, ch;
    if (!get_overflow_clip_rect(idx, &cx, &cy, &cw, &ch)) return;
    float sx = (float)fbw / window_width;
    float sy = (float)fbh / window_height;
    int sc_x = (int)(cx * sx + 0.5f);
    int sc_y = (int)((window_height - cy - ch) * sy + 0.5f);
    int sc_w = (int)(cw * sx + 0.5f);
    int sc_h = (int)(ch * sy + 0.5f);
    if (sc_w < 0) sc_w = 0;
    if (sc_h < 0) sc_h = 0;
    glEnable(GL_SCISSOR_TEST);
    glScissor(sc_x, sc_y, sc_w, sc_h);
}

void update_animations(double dt) {
    for (int i = 0; i < elem_count; i++) {
        Element* e = &elements[i];
        float speed = (e->anim_speed > 0.0f) ? e->anim_speed : 14.0f;
        float factor = 1.0f - expf(-(float)dt * speed);
        if (factor > 1.0f) factor = 1.0f;
        if (factor < 0.0f) factor = 0.0f;

        e->cur_r   += (e->r   - e->cur_r)   * factor;
        e->cur_g   += (e->g   - e->cur_g)   * factor;
        e->cur_b   += (e->b   - e->cur_b)   * factor;
        if (e->a <= 0.001f) {
            e->cur_a = 0.0f;
        } else if (e->a >= 0.999f) {
            e->cur_a += (1.0f - e->cur_a) * factor;
        } else {
            e->cur_a += (e->a - e->cur_a) * factor;
        }
        e->cur_bd_r += (e->bd_r - e->cur_bd_r) * factor;
        e->cur_bd_g += (e->bd_g - e->cur_bd_g) * factor;
        e->cur_bd_b += (e->bd_b - e->cur_bd_b) * factor;
        e->cur_bd_a += (e->bd_a - e->cur_bd_a) * factor;

        float press_scale = (e->cursor_pointer && e->is_active && e->drag_mode != 1) ? 0.96f : 1.0f;
        float target_scale = e->transform_scale * press_scale;
        e->cur_scale += (target_scale - e->cur_scale) * factor;
        e->cur_tx += (e->transform_tx - e->cur_tx) * factor;
        e->cur_ty += (e->transform_ty - e->cur_ty) * factor;
    }
}

static float css_anim_ease(int easing, float t) {
    if (easing == 1) return t * t * (3.0f - 2.0f * t);
    return t;
}

static const CssKeyframe* find_keyframe_anim(const char* name) {
    if (!name || !name[0]) return NULL;
    for (int i = 0; i < g_keyframe_count; i++)
        if (strcmp(g_keyframes[i].name, name) == 0)
            return &g_keyframes[i];
    return NULL;
}

static float kf_stop_width_px(const Element* e, const KeyframeStop* stop) {
    if (!stop->has_width) {
        if (e && e->anim_base_captured) {
            if (e->anim_base_w_pct) {
                int par = e->parent_idx;
                float pw = (par != -1) ? elements[par].w : window_width;
                return pw * e->raw_w;
            }
            return e->anim_base_w;
        }
        return 0.0f;
    }
    if (stop->width_pct) {
        int par = e ? e->parent_idx : -1;
        float pw = (par != -1) ? elements[par].w : window_width;
        return pw * stop->width;
    }
    return stop->width;
}

static float kf_stop_left_px(const Element* e, const KeyframeStop* stop) {
    if (!stop->has_left) {
        if (e && e->anim_base_captured) {
            if (e->anim_base_left_pct) {
                int par = e->parent_idx;
                float pw = (par != -1) ? elements[par].w : window_width;
                return pw * e->anim_base_left;
            }
            return e->anim_base_left;
        }
        return 0.0f;
    }
    if (stop->left_pct) {
        int par = e ? e->parent_idx : -1;
        float pw = (par != -1) ? elements[par].w : window_width;
        return pw * stop->left;
    }
    return stop->left;
}

static void lerp_keyframe_stop(const KeyframeStop* a, const KeyframeStop* b, float u,
                               KeyframeStop* out, const Element* e) {
    memset(out, 0, sizeof(*out));
    if (a->has_width || b->has_width || (e && e->anim_base_captured)) {
        out->has_width = 1;
        out->width_pct = 0;
        float av = kf_stop_width_px(e, a);
        float bv = kf_stop_width_px(e, b);
        out->width = av * (1.0f - u) + bv * u;
    }
    if (a->has_left || b->has_left || (e && e->anim_base_captured)) {
        out->has_left = 1;
        out->left_pct = 0;
        float av = kf_stop_left_px(e, a);
        float bv = kf_stop_left_px(e, b);
        out->left = av * (1.0f - u) + bv * u;
    }
    if (a->has_opacity || b->has_opacity) {
        out->has_opacity = 1;
        float av = a->has_opacity ? a->opacity : 1.0f;
        float bv = b->has_opacity ? b->opacity : 1.0f;
        out->opacity = av * (1.0f - u) + bv * u;
    }
    if (a->has_transform_scale || b->has_transform_scale) {
        out->has_transform_scale = 1;
        float av = a->has_transform_scale ? a->transform_scale : 1.0f;
        float bv = b->has_transform_scale ? b->transform_scale : 1.0f;
        out->transform_scale = av * (1.0f - u) + bv * u;
    }
    if (a->has_transform_tx || b->has_transform_tx) {
        out->has_transform_tx = 1;
        float av = a->has_transform_tx ? a->transform_tx : 0.0f;
        float bv = b->has_transform_tx ? b->transform_tx : 0.0f;
        out->transform_tx = av * (1.0f - u) + bv * u;
    }
    if (a->has_transform_ty || b->has_transform_ty) {
        out->has_transform_ty = 1;
        float av = a->has_transform_ty ? a->transform_ty : 0.0f;
        float bv = b->has_transform_ty ? b->transform_ty : 0.0f;
        out->transform_ty = av * (1.0f - u) + bv * u;
    }
}

static void sample_keyframe(const CssKeyframe* kf, float t, KeyframeStop* out, const Element* e) {
    memset(out, 0, sizeof(*out));
    if (!kf || kf->stop_count <= 0) return;
    if (t <= kf->stops[0].position) {
        lerp_keyframe_stop(&kf->stops[0], &kf->stops[0], 0.0f, out, e);
        return;
    }
    if (t >= kf->stops[kf->stop_count - 1].position) {
        lerp_keyframe_stop(&kf->stops[kf->stop_count - 1], &kf->stops[kf->stop_count - 1], 1.0f, out, e);
        return;
    }
    for (int i = 0; i < kf->stop_count - 1; i++) {
        const KeyframeStop* a = &kf->stops[i];
        const KeyframeStop* b = &kf->stops[i + 1];
        if (t >= a->position && t <= b->position) {
            float span = b->position - a->position;
            float u = (span > 0.0001f) ? (t - a->position) / span : 0.0f;
            lerp_keyframe_stop(a, b, u, out, e);
            return;
        }
    }
    lerp_keyframe_stop(&kf->stops[kf->stop_count - 1], &kf->stops[kf->stop_count - 1], 1.0f, out, e);
}

static void apply_keyframe_stop_to_element(Element* e, const KeyframeStop* stop) {
    if (stop->has_width) {
        e->anim_override_layout = 1;
        e->pct_w = 0;
        e->has_css_width = 1;
        e->css_width = stop->width;
        e->w = stop->width;
    }
    if (stop->has_left) {
        e->anim_override_layout = 1;
        e->has_left = 1;
        e->pct_left = 0;
        e->rel_x = stop->left;
        e->pos_overridden_x = 1;
    }
    if (stop->has_opacity) e->opacity = stop->opacity;
    if (stop->has_transform_scale) e->transform_scale = stop->transform_scale;
    if (stop->has_transform_tx) { e->transform_tx = stop->transform_tx; e->has_custom_bg = 0; }
    if (stop->has_transform_ty) e->transform_ty = stop->transform_ty;
}

static void update_css_keyframe_animations(double now) {
    for (int i = 0; i < elem_count; i++) {
        Element* e = &elements[i];
        if (!e->has_css_animation || !e->anim_name[0]) continue;
        const CssKeyframe* kf = find_keyframe_anim(e->anim_name);
        if (!kf) continue;
        float duration = e->anim_duration > 0.0f ? e->anim_duration : 1.0f;
        float elapsed = (float)(now - e->anim_start_time) - e->anim_delay;
        if (elapsed < 0.0f) continue;
        float cycle_t = elapsed / duration;
        int rev = 0;
        if (e->anim_infinite) {
            if (e->anim_alternate) {
                int seg = (int)cycle_t;
                rev = seg % 2;
                cycle_t = cycle_t - (float)seg;
                if (rev) cycle_t = 1.0f - cycle_t;
            } else {
                cycle_t = cycle_t - floorf(cycle_t);
            }
        } else {
            if (cycle_t > 1.0f) cycle_t = 1.0f;
            if (e->anim_alternate && cycle_t > 0.5f) cycle_t = 1.0f - (cycle_t - 0.5f) * 2.0f;
        }
        cycle_t = css_anim_ease(e->anim_easing, cycle_t);
        KeyframeStop sampled;
        sample_keyframe(kf, cycle_t, &sampled, e);
        apply_keyframe_stop_to_element(e, &sampled);
    }
}

static int take_screenshot(const char* path) {
    if (!path || !path[0] || !g_window) return 0;
    int fbw = 0, fbh = 0;
    glfwGetFramebufferSize(g_window, &fbw, &fbh);
    if (fbw <= 0 || fbh <= 0) return 0;
    size_t npix = (size_t)fbw * (size_t)fbh;
    unsigned char* pixels = (unsigned char*)malloc(npix * 4);
    unsigned char* flipped = (unsigned char*)malloc(npix * 3);
    if (!pixels || !flipped) { free(pixels); free(flipped); return 0; }
    glReadPixels(0, 0, fbw, fbh, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    for (int y = 0; y < fbh; y++) {
        const unsigned char* src = pixels + (size_t)(fbh - 1 - y) * (size_t)fbw * 4;
        unsigned char* dst = flipped + (size_t)y * (size_t)fbw * 3;
        for (int x = 0; x < fbw; x++) {
            dst[x * 3 + 0] = src[x * 4 + 0];
            dst[x * 3 + 1] = src[x * 4 + 1];
            dst[x * 3 + 2] = src[x * 4 + 2];
        }
    }
    int ok = stbi_write_png(path, fbw, fbh, 3, flipped, fbw * 3);
    free(pixels);
    free(flipped);
    if (ok) fprintf(stderr, "[vespera] Screenshot saved: %s (%dx%d)\n", path, fbw, fbh);
    else fprintf(stderr, "[vespera] Screenshot failed: %s\n", path);
    return ok != 0;
}

// ============================================================
// Render-order sorting by z-index
// ============================================================

// Effective z-index = max of self and all ancestors.
// Starts from element's own z_index so negative values (e.g. body=-9999) sort first.
static int effective_z_index(int idx) {
    int z = elements[idx].z_index;
    int p = elements[idx].parent_idx;
    while (p != -1) {
        if (elements[p].z_index > z) z = elements[p].z_index;
        p = elements[p].parent_idx;
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

// ---- font scanning helpers ----------------------------------------

// Returns 1 if the filename extension is a supported font format.
static int is_font_file(const char* name) {
    size_t n = strlen(name);
    if (n < 4) return 0;
    const char* ext = name + n - 4;
    return (strcasecmp(ext, ".ttf") == 0 ||
            strcasecmp(ext, ".otf") == 0 ||
            strcasecmp(ext + 1, "tc") == 0); // .ttc / .otc (n>=5 implied by ext check)
}

// Returns 1 if the filename looks like a bold variant.
static int is_bold_name(const char* name) {
    // Case-insensitive search for "bold" in the basename.
    char lower[512];
    size_t n = strlen(name);
    if (n >= sizeof(lower)) n = sizeof(lower) - 1;
    for (size_t i = 0; i < n; i++) lower[i] = (char)tolower((unsigned char)name[i]);
    lower[n] = '\0';
    return strstr(lower, "bold") != NULL;
}

// Dynamic font path list.
typedef struct {
    char** paths;
    int    count;
    int    cap;
} FontPathList;

static void fpl_add(FontPathList* fpl, const char* path) {
    if (fpl->count >= fpl->cap) {
        fpl->cap = fpl->cap ? fpl->cap * 2 : 64;
        fpl->paths = realloc(fpl->paths, sizeof(char*) * fpl->cap);
    }
    fpl->paths[fpl->count++] = strdup(path);
}

static void fpl_free(FontPathList* fpl) {
    for (int i = 0; i < fpl->count; i++) free(fpl->paths[i]);
    free(fpl->paths);
    fpl->paths = NULL; fpl->count = fpl->cap = 0;
}

// Recursively scan dir and collect font paths into reg/bold lists.
static void scan_fonts_dir(const char* dir, FontPathList* reg, FontPathList* bold) {
    DIR* d = opendir(dir);
    if (!d) return;
    struct dirent* ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue; // skip . and ..
        char path[1024];
        snprintf(path, sizeof(path), "%s/%s", dir, ent->d_name);

        struct stat st;
        if (stat(path, &st) != 0) continue;

        if (S_ISDIR(st.st_mode)) {
            scan_fonts_dir(path, reg, bold); // recurse
        } else if (S_ISREG(st.st_mode) && is_font_file(ent->d_name)) {
            if (is_bold_name(ent->d_name))
                fpl_add(bold, path);
            else
                fpl_add(reg, path);
        }
    }
    closedir(d);
}

// Try each path in the list; return first successfully loaded buffer.
static unsigned char* try_load_font_list(FontPathList* fpl) {
    for (int i = 0; i < fpl->count; i++) {
        unsigned char* buf = read_file_bytes(fpl->paths[i], NULL);
        if (buf) {
            fprintf(stderr, "[vespera] Font loaded: %s\n", fpl->paths[i]);
            return buf;
        }
    }
    return NULL;
}

// ------------------------------------------------------------------

void init_font() {
    FontPathList reg  = {0};
    FontPathList bold_list = {0};

    scan_fonts_dir("/usr/share/fonts", &reg, &bold_list);

    unsigned char* reg_buf = try_load_font_list(&reg);
    if (!reg_buf) { fprintf(stderr, "[vespera] Warning: no regular font found under /usr/share/fonts\n"); }
    else {
        bake_font_set(reg_buf, font_regular);
        free(reg_buf);
    }

    unsigned char* bold_buf = try_load_font_list(&bold_list);
    if (bold_buf) {
        bake_font_set(bold_buf, font_bold_atlas);
        bold_font_loaded = 1;
        free(bold_buf);
    }

    fpl_free(&reg);
    fpl_free(&bold_list);

    glCreateVertexArrays(1, &text_vao); glGenBuffers(1, &text_vbo);
    glBindVertexArray(text_vao); glBindBuffer(GL_ARRAY_BUFFER, text_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 4 * 4, NULL, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glEnableVertexAttribArray(0);
    if (reg_buf || bold_buf) font_loaded = 1;
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

static float measure_text_range(FontAtlas* atlas, const char* start, int len) {
    float w = 0.0f;
    for (int i = 0; i < len && start[i]; i++) {
        unsigned char c = (unsigned char)start[i];
        if (c >= 32 && c < 128) w += atlas->cdata[c - 32].xadvance;
    }
    return w;
}

static int fit_text_chars(FontAtlas* atlas, const char* text, int len, float max_w) {
    if (max_w <= 0.0f) return 0;
    float w = 0.0f;
    int last_fit = 0;
    for (int i = 0; i < len && text[i]; i++) {
        unsigned char c = (unsigned char)text[i];
        float adv = (c >= 32 && c < 128) ? atlas->cdata[c - 32].xadvance : 0.0f;
        if (w + adv > max_w && last_fit > 0) break;
        if (w + adv > max_w) return 0;
        w += adv;
        last_fit = i + 1;
    }
    return last_fit;
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

// Load (or retrieve cached) texture from file path.
static GLuint load_or_get_texture(const char* path) {
    if (!path || !path[0]) return 0;
    for (int i = 0; i < g_tex_count; i++)
        if (strcmp(g_tex_cache[i].path, path) == 0) return g_tex_cache[i].tex;
    if (g_tex_count >= MAX_TEXTURES) return 0;

    char resolved[512];
    resolve_resource_path(path, resolved, sizeof(resolved));

    stbi_set_flip_vertically_on_load(1);
    int w, h, ch;
    unsigned char* data = stbi_load(resolved, &w, &h, &ch, 4);
    if (!data) data = stbi_load(path, &w, &h, &ch, 4);
    if (!data) return 0;

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    strncpy(g_tex_cache[g_tex_count].path, path, sizeof(g_tex_cache[0].path) - 1);
    g_tex_cache[g_tex_count].path[sizeof(g_tex_cache[0].path) - 1] = '\0';
    g_tex_cache[g_tex_count].tex = tex;
    g_tex_count++;
    return tex;
}

// Draw a texture cropped to a rounded rect element.
static void draw_image(float x, float y, float w, float h, float radius, GLuint tex, float alpha) {
    if (!img_program || !tex || alpha <= 0.004f || w <= 0.0f || h <= 0.0f) return;
    float half_min = (w < h ? w : h) * 0.5f;
    if (radius > half_min) radius = half_min;
    glUseProgram(img_program);
    glUniform2f(img_loc.uResolution, window_width, window_height);
    glUniform2f(img_loc.uPos,  x, y);
    glUniform2f(img_loc.uSize, w, h);
    glUniform1f(img_loc.uRadius, radius);
    glUniform1f(img_loc.uAlpha, alpha);
    if (glActiveTexture_) glActiveTexture_(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i_(img_loc.uImage, 0);
    glBindVertexArray(g_rect_vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindTexture(GL_TEXTURE_2D, 0);
}

// Full-featured rect draw: solid color or gradient (linear/radial, multi-stop).
void draw_rect_full(float x, float y, float w, float h,
                    float r, float g, float b, float a,
                    float radius, float b_w,
                    float bd_r, float bd_g, float bd_b, float bd_a,
                    const Element* ge) {
    int grad_mode = (ge && ge->has_gradient) ? ge->grad_type : GRAD_NONE;
    if (a <= 0.0f && bd_a <= 0.0f && grad_mode == GRAD_NONE) return;

    // CSS border-radius: 50% is stored as 50 (parse_float_val ignores %).
    // Cap to min(w,h)/2 so "50%" on small elements (e.g. 13px buttons) forms a circle.
    float half_min = (w < h ? w : h) * 0.5f;
    if (radius > half_min) radius = half_min;

    glUseProgram(bg_program);
    glUniform2f(bg_loc.uResolution, window_width, window_height);
    glUniform2f(bg_loc.uPos,  x, y);
    glUniform2f(bg_loc.uSize, w, h);
    glUniform4f(bg_loc.uColor,       r,    g,    b,    a);
    glUniform4f(bg_loc.uBorderColor, bd_r, bd_g, bd_b, bd_a);
    glUniform1f(bg_loc.uBorderWidth, b_w);
    glUniform1f(bg_loc.uRadius,      radius);
    glUniform1i_(bg_loc.uGradient, grad_mode);
    if (ge && ge->has_gradient) {
        int sc = ge->grad_stop_count;
        if (sc < 2) sc = 2;
        if (sc > MAX_GRAD_STOPS) sc = MAX_GRAD_STOPS;
        glUniform1i_(bg_loc.uGradStopCount, sc);
        for (int i = 0; i < MAX_GRAD_STOPS; i++) {
            float pr = 0, pg = 0, pb = 0, pa = 0, pp = (float)i / (float)(MAX_GRAD_STOPS - 1);
            if (i < ge->grad_stop_count) {
                pr = ge->grad_stop_r[i]; pg = ge->grad_stop_g[i];
                pb = ge->grad_stop_b[i]; pa = ge->grad_stop_a[i];
                pp = ge->grad_stop_pos[i];
            }
            glUniform4f(bg_loc.uGradColors[i], pr, pg, pb, pa);
            glUniform1f(bg_loc.uGradStops[i],  pp);
        }
        glUniform1f(bg_loc.uGradAngle,  ge->grad_angle);
        glUniform2f(bg_loc.uGradCenter, ge->grad_rad_cx, ge->grad_rad_cy);
        glUniform1f(bg_loc.uGradRadius, ge->grad_rad_r);
    }

    glBindVertexArray(g_rect_vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

// Simplified solid-color rect (no gradient).
void draw_rect(float x, float y, float w, float h,
               float r, float g, float b, float a,
               float radius, float b_w,
               float bd_r, float bd_g, float bd_b, float bd_a) {
    draw_rect_full(x, y, w, h, r, g, b, a, radius, b_w, bd_r, bd_g, bd_b, bd_a, NULL);
}

// CSS box-shadow: Gaussian soft shadow using dedicated SDF shader.
static void draw_shadow(float ex, float ey, float ew, float eh,
                        float sh_dx, float sh_dy, float sh_blur,
                        float sh_r, float sh_g, float sh_b, float sh_a,
                        float radius, float eff_op) {
    if (!shadow_program || sh_a * eff_op <= 0.004f) return;
    float blur = sh_blur > 0.0f ? sh_blur : 0.0f;
    // Gaussian tail is visible up to ~1.65*blur from the shadow shape edge (discard < 0.004).
    float pad  = blur * 1.75f + 2.0f;

    // Shadow rect covers the full blurred area including offset:
    //   left edge  : min(0, sh_dx) - pad
    //   top edge   : min(0, sh_dy) - pad
    //   right edge : max(0, sh_dx) + pad
    //   bottom edge: max(0, sh_dy) + pad
    float left  = (sh_dx < 0.0f ? sh_dx : 0.0f) - pad;
    float top   = (sh_dy < 0.0f ? sh_dy : 0.0f) - pad;
    float right = (sh_dx > 0.0f ? sh_dx : 0.0f) + pad;
    float bot   = (sh_dy > 0.0f ? sh_dy : 0.0f) + pad;

    float sx   = ex + left;
    float sy   = ey + top;
    float sw   = ew + (right - left);
    float sh_h = eh + (bot  - top);

    // FragPos = vec2(aPos.x, 1-aPos.y)*uSize: x is right, y is UP (flipped from screen y).
    // inset = distance from shadow-rect corner to element corner, in FragPos space.
    // For x (no flip): inset_x = -left = pad + max(0, -sh_dx).
    // For y (flipped): the shadow rect y already encodes sh_dy via sh_h, so inset_y = -top.
    float inset_x = -left;
    float inset_y = -top;

    // uOffset in FragPos space: x needs explicit sh_dx shift because the inset only places
    // the element (not the shadow shape).  y is already baked in by the y-flip in the rect
    // height, so off_y must be 0 to avoid double-counting sh_dy.
    float off_x = sh_dx;
    float off_y = 0.0f;

    glUseProgram(shadow_program);
    glUniform2f(sh_loc.uResolution, window_width, window_height);
    glUniform2f(sh_loc.uPos,  sx, sy);
    glUniform2f(sh_loc.uSize, sw, sh_h);
    glUniform4f(sh_loc.uShadowColor, sh_r, sh_g, sh_b, sh_a * eff_op);
    glUniform2f(sh_loc.uElemSize, ew, eh);
    float half_min_s = (ew < eh ? ew : eh) * 0.5f;
    if (radius > half_min_s) radius = half_min_s;
    glUniform1f(sh_loc.uRadius, radius);
    glUniform1f(sh_loc.uBlur,   blur);
    glUniform2f(sh_loc.uShadowInset, inset_x, inset_y);
    glUniform2f(sh_loc.uOffset,      off_x, off_y);

    glBindVertexArray(g_rect_vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

void render_text_pass(FontAtlas* atlas, const char* text,
                      float start_x, float baseline_y,
                      float r, float g, float b, float a) {
    glUseProgram(text_program);
    glUniform2f(tx_loc.uResolution, window_width, window_height);
    glUniform4f(tx_loc.textColor,   r, g, b, a);
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

static int count_text_lines(FontAtlas* atlas, const char* text, float box_w,
                            float line_h, int max_lines,
                            int white_space, int text_overflow, int overflow_wrap) {
    const char* p = text;
    int line_no = 0;
    while (*p && line_no < max_lines) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;

        int remaining = (int)strlen(p);
        int hard_break = -1;
        for (int i = 0; i < remaining; i++) {
            if (p[i] == '\n' || p[i] == '\r') { hard_break = i; break; }
        }
        int para_len = hard_break >= 0 ? hard_break : remaining;
        if (para_len <= 0) { p += 1; continue; }

        int take = para_len;
        if (white_space == 1) {
            take = fit_text_chars(atlas, p, para_len, box_w);
            if (take < para_len && text_overflow == 1 && box_w > 0.0f) {
                float ell_w = measure_text_width(atlas, "...");
                int base = fit_text_chars(atlas, p, para_len, box_w - ell_w);
                if (base < 0) base = 0;
                take = base;
            }
        } else {
            take = fit_text_chars(atlas, p, para_len, box_w);
            if (take <= 0 && para_len > 0) take = 1;
            if (take < para_len && !overflow_wrap) {
                int last_space = -1;
                for (int i = 0; i < take; i++)
                    if (p[i] == ' ' || p[i] == '\t') last_space = i;
                if (last_space > 0) take = last_space;
            } else if (take < para_len) {
                int last_space = -1;
                for (int i = 0; i < take; i++)
                    if (p[i] == ' ' || p[i] == '\t') last_space = i;
                if (last_space > 0 && measure_text_range(atlas, p, take) > box_w * 0.65f)
                    take = last_space;
            }
        }

        int is_last_visible_line = (line_no == max_lines - 1);
        int has_more = (take < para_len) || (hard_break >= 0 && p[hard_break + 1]);
        if (white_space == 1 || is_last_visible_line) {
            line_no++;
            break;
        }
        (void)has_more;
        line_no++;
        p += take;
        while (*p == ' ' || *p == '\t') p++;
        if (hard_break >= 0 && take >= hard_break) p++;
    }
    return line_no;
}

void render_text(const char* text, float x, float y, float box_w, float box_h, int align,
                 int v_align, float r, float g, float b, float a, int fsize, int bold,
                 float css_line_height, int white_space, int text_overflow, int overflow_wrap) {
    if (!font_loaded || !text || !*text || a <= 0.0f) return;
    if (box_w <= 0.0f || box_h <= 0.0f) return;

    int is_fake_bold = 0;
    FontAtlas* atlas = get_atlas(fsize, bold, &is_fake_bold);
    if (!atlas->loaded) return;

    float used_size = font_sizes[0];
    for (int i = 0; i < NUM_FONT_SIZES; i++) {
        if (&font_regular[i] == atlas || &font_bold_atlas[i] == atlas) {
            used_size = font_sizes[i]; break;
        }
    }
    float cap_h   = used_size * 0.72f;
    float line_h = css_line_height > 0.0f ? css_line_height : used_size * 1.25f;
    if (line_h < cap_h + 1.0f) line_h = cap_h + 1.0f;

    int max_lines = (int)floorf(box_h / line_h);
    if (max_lines < 1) max_lines = (box_h >= cap_h) ? 1 : 0;
    if (max_lines <= 0) return;

    int line_count = count_text_lines(atlas, text, box_w, line_h, max_lines,
                                      white_space, text_overflow, overflow_wrap);
    if (line_count < 1) line_count = 1;
    float used_h = (float)line_count * line_h;
    float y_off = 0.0f;
    if (used_h < box_h) {
        if (v_align == 2)
            y_off = box_h - used_h;
        else if (v_align != 0)
            y_off = (box_h - used_h) * 0.5f;
    }

    const char* p = text;
    int line_no = 0;
    while (*p && line_no < max_lines) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;

        int remaining = (int)strlen(p);
        int hard_break = -1;
        for (int i = 0; i < remaining; i++) {
            if (p[i] == '\n' || p[i] == '\r') { hard_break = i; break; }
        }
        int para_len = hard_break >= 0 ? hard_break : remaining;
        if (para_len <= 0) { p += 1; continue; }

        int take = para_len;
        if (white_space == 1) {
            take = fit_text_chars(atlas, p, para_len, box_w);
            if (take < para_len && text_overflow == 1 && box_w > 0.0f) {
                float ell_w = measure_text_width(atlas, "...");
                int base = fit_text_chars(atlas, p, para_len, box_w - ell_w);
                if (base < 0) base = 0;
                take = base;
            }
        } else {
            take = fit_text_chars(atlas, p, para_len, box_w);
            if (take <= 0 && para_len > 0) take = 1;
            if (take < para_len && !overflow_wrap) {
                int last_space = -1;
                for (int i = 0; i < take; i++)
                    if (p[i] == ' ' || p[i] == '\t') last_space = i;
                if (last_space > 0) take = last_space;
            } else if (take < para_len) {
                int last_space = -1;
                for (int i = 0; i < take; i++)
                    if (p[i] == ' ' || p[i] == '\t') last_space = i;
                if (last_space > 0 && measure_text_range(atlas, p, take) > box_w * 0.65f)
                    take = last_space;
            }
        }

        int is_last_visible_line = (line_no == max_lines - 1);
        int has_more = (take < para_len) || (hard_break >= 0 && p[hard_break + 1]);
        char line[512];
        int n = take;
        if (n > (int)sizeof(line) - 8) n = (int)sizeof(line) - 8;
        memcpy(line, p, (size_t)n);
        line[n] = '\0';
        while (n > 0 && (line[n - 1] == ' ' || line[n - 1] == '\t')) line[--n] = '\0';

        if ((white_space == 1 || is_last_visible_line) && text_overflow == 1 && has_more) {
            float ell_w = measure_text_width(atlas, "...");
            while (n > 0 && measure_text_range(atlas, line, n) + ell_w > box_w)
                line[--n] = '\0';
            if (n + 3 < (int)sizeof(line)) strcat(line, "...");
        }

        float line_w = measure_text_width(atlas, line);
        float start_x = x;
        if      (align == 1) start_x = x + (box_w - line_w) / 2.0f;
        else if (align == 2) start_x = x + (box_w - line_w);
        if (start_x < x) start_x = x;

        int single_line = (white_space == 1 || line_count == 1);
        float baseline;
        if (single_line) {
            if (v_align == 2)
                baseline = y + box_h - (line_h - cap_h) * 0.5f;
            else if (v_align == 0)
                baseline = y + (line_h - cap_h) * 0.5f + cap_h;
            else
                baseline = y + (box_h - cap_h) * 0.5f + cap_h;
        } else {
            baseline = y + y_off + line_no * line_h + (line_h - cap_h) * 0.5f + cap_h;
        }
        if (baseline < y + cap_h) baseline = y + cap_h;
        if (baseline + (line_h - cap_h) * 0.5f > y + box_h + 0.5f) break;
        render_text_pass(atlas, line, start_x, baseline, r, g, b, a);
        if (is_fake_bold) render_text_pass(atlas, line, start_x + 1.0f, baseline, r, g, b, a);

        line_no++;
        if (white_space == 1 || is_last_visible_line) break;
        p += take;
        while (*p == ' ' || *p == '\t') p++;
        if (hard_break >= 0 && take >= hard_break) p++;
    }
}

static void draw_scrollbars(void) {
    for (int i = 0; i < elem_count; i++) {
        Element* c = &elements[i];
        if (!is_visible(i)) continue;
        float tr = 0.55f, tg = 0.56f, tb = 0.62f, ta = 0.18f;
        float thr = 0.42f, thg = 0.44f, thb = 0.52f, tha = 0.72f;
        if (c->has_scrollbar_color) {
            tr = c->sb_track_r; tg = c->sb_track_g; tb = c->sb_track_b; ta = c->sb_track_a;
            thr = c->sb_thumb_r; thg = c->sb_thumb_g; thb = c->sb_thumb_b; tha = c->sb_thumb_a;
        }
        float tx, ty, tw, th, ux, uy, uw, uh;
        int vis = 0;
        scrollbar_geom_y(c, &tx, &ty, &tw, &th, &ux, &uy, &uw, &uh, &vis);
        if (vis) {
            int hov = (i == g_scroll_hover_idx && g_scroll_hover_axis == 0);
            float track_a = hov ? (ta * 1.5f > 1.0f ? 1.0f : ta * 1.5f) : ta;
            float thumb_a = hov ? (tha * 1.15f > 1.0f ? 1.0f : tha * 1.15f) : tha;
            draw_rect(tx, ty, tw, th, tr, tg, tb, track_a, 2.5f, 0, 0, 0, 0, 0);
            draw_rect(ux, uy, uw, uh, thr, thg, thb, thumb_a, 2.5f, 0, 0, 0, 0, 0);
        }
        scrollbar_geom_x(c, &tx, &ty, &tw, &th, &ux, &uy, &uw, &uh, &vis);
        if (vis) {
            int hov = (i == g_scroll_hover_idx && g_scroll_hover_axis == 1);
            float track_a = hov ? (ta * 1.5f > 1.0f ? 1.0f : ta * 1.5f) : ta;
            float thumb_a = hov ? (tha * 1.15f > 1.0f ? 1.0f : tha * 1.15f) : tha;
            draw_rect(tx, ty, tw, th, tr, tg, tb, track_a, 2.5f, 0, 0, 0, 0, 0);
            draw_rect(ux, uy, uw, uh, thr, thg, thb, thumb_a, 2.5f, 0, 0, 0, 0, 0);
        }
    }
}

static void draw_focus_outlines(void) {
    if (g_focused_element_idx == -1 || !is_visible(g_focused_element_idx)) return;
    if (!g_focus_via_keyboard) return;
    Element* e = &elements[g_focused_element_idx];
    if (!e->has_outline) return;
    float ow = e->outline_width;
    if (ow <= 0.0f) return;
    float off = e->outline_offset;
    float or = e->ol_r;
    float og = e->ol_g;
    float ob = e->ol_b;
    float oa = e->ol_a;
    float eff_op = element_effective_opacity(g_focused_element_idx);
    float bx, by, bw, bh;
    get_element_draw_bounds(e, &bx, &by, &bw, &bh);
    float pad = off + ow;
    draw_rect(bx - pad, by - pad, bw + pad * 2.0f, bh + pad * 2.0f,
              0.0f, 0.0f, 0.0f, 0.0f, e->border_radius * e->cur_scale + off,
              ow, or, og, ob, oa * eff_op);
}

static int element_aria_hidden(int idx);

static void draw_a11y_live_region(void) {
    char buf[256];
    int assertive = 0;
    double now = glfwGetTime();

    if (g_a11y_live_until > now && g_a11y_live_msg[0]) {
        snprintf(buf, sizeof(buf), "%s", g_a11y_live_msg);
        assertive = g_a11y_live_assertive;
    } else if (g_focus_via_keyboard && g_focused_element_idx != -1) {
        Element* e = &elements[g_focused_element_idx];
        if (element_aria_hidden(g_focused_element_idx)) return;
        char label[200];
        if (e->aria_label[0])
            snprintf(label, sizeof(label), "%s", e->aria_label);
        else if (e->role[0] && e->text[0])
            snprintf(label, sizeof(label), "%s: %s", e->role, e->text);
        else if (e->text[0])
            snprintf(label, sizeof(label), "%s", e->text);
        else
            return;
        if (e->aria_expanded == 1)
            snprintf(buf, sizeof(buf), "%s, expanded", label);
        else if (e->aria_expanded == 0)
            snprintf(buf, sizeof(buf), "%s, collapsed", label);
        else
            snprintf(buf, sizeof(buf), "%s", label);
    } else
        return;

    float bar_h = 26.0f;
    float bar_y = window_height - bar_h - 6.0f;
    float accent_r = assertive ? 0.95f : 0.39f;
    float accent_g = assertive ? 0.55f : 0.40f;
    float accent_b = assertive ? 0.20f : 0.95f;
    draw_rect(12.0f, bar_y, window_width - 24.0f, bar_h,
              0.12f, 0.12f, 0.18f, 0.92f, 8.0f, 0, 0, 0, 0, 0);
    draw_rect(12.0f, bar_y, 4.0f, bar_h,
              accent_r, accent_g, accent_b, 1.0f, 2.0f, 0, 0, 0, 0, 0);
    float tw = measure_text_width(&font_regular[0], buf);
    if (tw <= 0.0f) tw = 100.0f;
    float tx = 20.0f;
    if (tx + tw > window_width - 20.0f) tx = window_width - 20.0f - tw;
    if (tx < 20.0f) tx = 20.0f;
    render_text(buf, tx, bar_y + 4.0f, window_width - 40.0f, bar_h - 4.0f, 0, 1,
                0.88f, 0.90f, 0.96f, 1.0f, 12, 0, 0.0f, 1, 1, 1);
}

// ============================================================
// Event callbacks
// ============================================================

static void modal_cancel_click(Element* e);

static void focus_element(int idx);
static void focus_element_ex(int idx, int via_keyboard);
static int element_is_inert(int idx);
static int element_aria_hidden(int idx);

static int element_aria_hidden(int idx) {
    while (idx != -1) {
        if (elements[idx].aria_hidden) return 1;
        idx = elements[idx].parent_idx;
    }
    return 0;
}

static int is_descendant_of(int idx, int ancestor) {
    if (ancestor == -1) return 0;
    while (idx != -1) {
        if (idx == ancestor) return 1;
        idx = elements[idx].parent_idx;
    }
    return 0;
}

static int get_focus_trap_root(void) {
    if (g_modal_overlay_idx != -1 && is_visible(g_modal_overlay_idx) &&
        !element_has_class(&elements[g_modal_overlay_idx], "hidden"))
        return g_modal_overlay_idx;
    if (g_select_panel_idx != -1 && is_visible(g_select_panel_idx) &&
        !element_has_class(&elements[g_select_panel_idx], "hidden"))
        return g_select_panel_idx;
    return -1;
}

static float content_offset_y(int target, int ancestor) {
    float y = 0.0f;
    for (int c = target; c != -1 && c != ancestor; c = elements[c].parent_idx)
        y += elements[c].rel_y + elements[c].margin_top;
    return y;
}

static float content_offset_x(int target, int ancestor) {
    float x = 0.0f;
    for (int c = target; c != -1 && c != ancestor; c = elements[c].parent_idx)
        x += elements[c].rel_x + elements[c].margin_left;
    return x;
}

static void scroll_into_view(int target) {
    if (target == -1) return;
    Element* e = &elements[target];
    for (int pass = 0; pass < 3; pass++) {
        for (int p = e->parent_idx; p != -1; p = elements[p].parent_idx) {
            Element* sc = &elements[p];
            float pad = sc->padding;
            float smt = e->scroll_margin_top;
            float smb = e->scroll_margin_bottom;
            float sml = e->scroll_margin_left;
            float smr = e->scroll_margin_right;
            float spt = sc->scroll_padding_top;
            float spb = sc->scroll_padding_bottom;
            float spl = sc->scroll_padding_left;
            float spr = sc->scroll_padding_right;
            if (overflow_scrollable(sc->overflow_y)) {
                float inner_h = sc->h - pad * 2.0f;
                float cy = content_offset_y(target, p);
                float ct = cy - smt;
                float cb = cy + e->h + smb;
                float vis_top = sc->scroll_top + spt;
                float vis_bot = sc->scroll_top + inner_h - spb;
                if (ct < vis_top)
                    set_scroll_top(p, ct - spt, 0);
                else if (cb > vis_bot)
                    set_scroll_top(p, cb - inner_h + spb, 0);
            }
            if (overflow_scrollable(sc->overflow_x)) {
                float inner_w = sc->w - pad * 2.0f;
                float cx = content_offset_x(target, p);
                float cl = cx - sml;
                float cr = cx + e->w + smr;
                float vis_left = sc->scroll_left + spl;
                float vis_right = sc->scroll_left + inner_w - spr;
                if (cl < vis_left)
                    set_scroll_left(p, cl - spl, 0);
                else if (cr > vis_right)
                    set_scroll_left(p, cr - inner_w + spr, 0);
            }
        }
    }
}

static int find_drag_window(int idx) {
    int found = idx;
    while (idx != -1) {
        if (element_has_class(&elements[idx], "window")) found = idx;
        idx = elements[idx].parent_idx;
    }
    return found;
}

static void bring_window_to_front(int idx) {
    int win = find_drag_window(idx);
    if (win == -1) return;
    if (!element_has_class(&elements[win], "window")) return;
    int old_focus = g_focused_idx;
    if (old_focus != -1 && old_focus != win)
        remove_class(&elements[old_focus], "focused");
    elements[win].z_index = ++g_top_z;
    g_focused_idx = win;
    add_class(&elements[win], "focused");
    update_element_style(&elements[win]);
    if (old_focus != -1 && old_focus != win)
        update_element_style(&elements[old_focus]);
}

static int hit_test_at(double xpos, double ypos) {
    build_render_order();
    for (int ri = elem_count - 1; ri >= 0; ri--) {
        int i = render_order[ri];
        Element* e = &elements[i];
        if (!is_rendered(i) || e->pointer_events_none || element_is_inert(i)) continue;
        float bx, by, bw, bh;
        get_element_draw_bounds(e, &bx, &by, &bw, &bh);
        if (xpos >= bx && xpos <= bx + bw &&
            ypos >= by && ypos <= by + bh) return i;
    }
    return -1;
}

static void set_window_cursor(GLFWwindow* window, int cursor_type) {
    if (cursor_type == g_current_cursor) return;
    g_current_cursor = cursor_type;
    GLFWcursor* cur = NULL;
    switch (cursor_type) {
        case 1: cur = g_hand_cursor; break;
        case 2: cur = g_cursor_ibeam; break;
        case 3: cur = g_cursor_crosshair; break;
        case 4: cur = g_cursor_hresize; break;
        case 5: cur = g_cursor_vresize; break;
        default: break;
    }
    glfwSetCursor(window, cur);
}

void recompute_hover(GLFWwindow* window, double xpos, double ypos) {
    g_scroll_hover_idx = -1;
    g_scroll_hover_axis = -1;
    for (int si = 0; si < elem_count; si++) {
        if (hit_scrollbar_thumb_y(si, xpos, ypos) || hit_scrollbar_track_y(si, xpos, ypos)) {
            g_scroll_hover_idx = si;
            g_scroll_hover_axis = 0;
            break;
        }
        if (hit_scrollbar_thumb_x(si, xpos, ypos) || hit_scrollbar_track_x(si, xpos, ypos)) {
            g_scroll_hover_idx = si;
            g_scroll_hover_axis = 1;
            break;
        }
    }

    int hit = hit_test_at(xpos, ypos);

    int best_cursor = 0;
    if (g_scroll_hover_axis == 0) best_cursor = 5;
    else if (g_scroll_hover_axis == 1) best_cursor = 4;
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
        if (should_hover && e->cursor_type > best_cursor)
            best_cursor = e->cursor_type;
    }

    set_window_cursor(window, best_cursor);
}

void cursor_position_callback(GLFWwindow* window, double xpos, double ypos) {
    if (g_scroll_drag_idx != -1) {
        Element* sc = &elements[g_scroll_drag_idx];
        float pad = sc->padding;
        if (g_scroll_drag_axis == 0) {
            float inner_h = sc->h - pad * 2.0f;
            float tx, ty, tw, th, ux, uy, uw, uh;
            int vis = 0;
            scrollbar_geom_y(sc, &tx, &ty, &tw, &th, &ux, &uy, &uw, &uh, &vis);
            if (vis) {
                float max_scroll = sc->scroll_content_h - inner_h;
                if (max_scroll < 0.0f) max_scroll = 0.0f;
                float scroll_range = th - uh;
                if (scroll_range > 0.0f && max_scroll > 0.0f) {
                    float thumb_y = (float)ypos - g_scroll_drag_off;
                    float ratio = (thumb_y - ty) / scroll_range;
                    if (ratio < 0.0f) ratio = 0.0f;
                    if (ratio > 1.0f) ratio = 1.0f;
                    float val = ratio * max_scroll;
                    sc->scroll_top = val;
                    sc->scroll_dest_top = val;
                }
            }
        } else {
            float inner_w = sc->w - pad * 2.0f;
            float tx, ty, tw, th, ux, uy, uw, uh;
            int vis = 0;
            scrollbar_geom_x(sc, &tx, &ty, &tw, &th, &ux, &uy, &uw, &uh, &vis);
            if (vis) {
                float max_scroll = sc->scroll_content_w - inner_w;
                if (max_scroll < 0.0f) max_scroll = 0.0f;
                float scroll_range = tw - uw;
                if (scroll_range > 0.0f && max_scroll > 0.0f) {
                    float thumb_x = (float)xpos - g_scroll_drag_off;
                    float ratio = (thumb_x - tx) / scroll_range;
                    if (ratio < 0.0f) ratio = 0.0f;
                    if (ratio > 1.0f) ratio = 1.0f;
                    float val = ratio * max_scroll;
                    sc->scroll_left = val;
                    sc->scroll_dest_left = val;
                }
            }
        }
        return;
    }
    if (drag_target_idx != -1) {
        if (fabs(xpos - g_press_x) > DRAG_THRESHOLD || fabs(ypos - g_press_y) > DRAG_THRESHOLD)
            g_drag_moved = 1;

        Element* d = &elements[drag_target_idx];
        if (g_drag_mode == 2) {
            int p = d->parent_idx;
            float parent_w = (p != -1) ? elements[p].w : window_width;
            float parent_x = (p != -1) ? elements[p].x : 0.0f;
            float new_rel_x = (float)xpos - drag_offset_x - parent_x;
            if (new_rel_x < 0.0f) new_rel_x = 0.0f;
            if (new_rel_x > parent_w - d->w) new_rel_x = parent_w - d->w;
            d->rel_x = new_rel_x;
            d->pos_overridden_x = 1;
        } else {
            float new_x = (float)xpos - drag_offset_x;
            float new_y = (float)ypos - drag_offset_y;
            if (d->parent_idx == -1) {
                d->rel_x = new_x;
                d->rel_y = new_y;
            } else {
                int p = d->parent_idx;
                d->rel_x = new_x - elements[p].x;
                d->rel_y = new_y - elements[p].y;
            }
            d->pos_overridden_x = 1;
            d->pos_overridden_y = 1;
        }
        return;
    }
    recompute_hover(window, xpos, ypos);
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    (void)mods;
    double mx, my;
    glfwGetCursorPos(window, &mx, &my);

    if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_LEFT) {
        g_drag_moved = 0;
        g_press_x = mx;
        g_press_y = my;

        for (int si = 0; si < elem_count; si++) {
            if (hit_scrollbar_thumb_y(si, mx, my)) {
                g_scroll_drag_idx = si;
                g_scroll_drag_axis = 0;
                float tx, ty, tw, th, ux, uy, uw, uh;
                int vis = 0;
                scrollbar_geom_y(&elements[si], &tx, &ty, &tw, &th, &ux, &uy, &uw, &uh, &vis);
                g_scroll_drag_off = (float)my - uy;
                return;
            }
            if (hit_scrollbar_thumb_x(si, mx, my)) {
                g_scroll_drag_idx = si;
                g_scroll_drag_axis = 1;
                float tx, ty, tw, th, ux, uy, uw, uh;
                int vis = 0;
                scrollbar_geom_x(&elements[si], &tx, &ty, &tw, &th, &ux, &uy, &uw, &uh, &vis);
                g_scroll_drag_off = (float)mx - ux;
                return;
            }
            if (hit_scrollbar_track_y(si, mx, my)) {
                scroll_track_click_y(si, mx, my);
                return;
            }
            if (hit_scrollbar_track_x(si, mx, my)) {
                scroll_track_click_x(si, mx, my);
                return;
            }
        }

        int hit = hit_test_at(mx, my);
        if (hit != -1) {
            Element* e = &elements[hit];
            if (g_focused_element_idx != -1 && g_focused_element_idx != hit)
                update_element_style(&elements[g_focused_element_idx]);
            focus_element(hit);
            bring_window_to_front(hit);
            e->is_active = 1;
            update_element_style(e);
            if (e->is_draggable) {
                g_drag_mode = e->drag_mode;
                if (e->drag_mode == 2) {
                    drag_target_idx = hit;
                    float bx, by, bw, bh;
                    get_element_draw_bounds(e, &bx, &by, &bw, &bh);
                    drag_offset_x = (float)mx - bx;
                    drag_offset_y = (float)my - by;
                } else {
                    int root = (e->parent_idx != -1) ? find_drag_window(e->parent_idx) : hit;
                    drag_target_idx = root;
                    drag_offset_x = (float)mx - elements[drag_target_idx].x;
                    drag_offset_y = (float)my - elements[drag_target_idx].y;
                }
            }
        }
    } else if (action == GLFW_RELEASE && button == GLFW_MOUSE_BUTTON_LEFT) {
        int hit = hit_test_at(mx, my);

        for (int i = 0; i < elem_count; i++) {
            if (elements[i].is_active) {
                elements[i].is_active = 0;
                int still_over = hit == i;
                if (still_over && elements[i].on_click && !g_drag_moved)
                    elements[i].on_click(&elements[i]);
                update_element_style(&elements[i]);
            }
        }

        if (!g_drag_moved && g_select_panel_idx != -1 && is_visible(g_select_panel_idx)) {
            Element* panel = &elements[g_select_panel_idx];
            if (!element_has_class(panel, "hidden")) {
                int inside = 0;
                for (int p = hit; p != -1; p = elements[p].parent_idx) {
                    if (p == g_select_panel_idx) { inside = 1; break; }
                    if (p == get_element_by_id("theme_select_box")) { inside = 1; break; }
                    if (p == get_element_by_id("theme_combo")) { inside = 1; break; }
                }
                if (!inside) {
                    add_class(panel, "hidden");
                    update_element_style(panel);
                    int box = get_element_by_id("theme_select_box");
                    if (box != -1) elements[box].aria_expanded = 0;
                }
            }
        }

        if (!g_drag_moved && g_modal_overlay_idx != -1 && is_visible(g_modal_overlay_idx)) {
            Element* overlay = &elements[g_modal_overlay_idx];
            if (!element_has_class(overlay, "hidden") && hit == g_modal_overlay_idx)
                modal_cancel_click(overlay);
        }

        drag_target_idx = -1;
        g_drag_mode = 0;
        g_scroll_drag_idx = -1;
        g_scroll_drag_axis = 0;
        recompute_hover(window, mx, my);
    }
}

// ============================================================
// Demo button callbacks (JS-compatible names via onclick="...")
// ============================================================

static void handle_close(Element* e);
static void handle_minimize(Element* e);
static void handle_maximize(Element* e);
static void modal_cancel_click(Element* e);
static void btn_click(Element* e);
static void nav_click(Element* e);
static void tool_click(Element* e);
static void toggle_click(Element* e);
static void checkbox_click(Element* e);
static void select_toggle_click(Element* e);
static void select_option_click(Element* e);
static void toast_dismiss(Element* e);
static void info_close_click(Element* e);
static void dock_toggle_info_click(Element* e);
static void dock_toggle_theme_click(Element* e);
static void dock_toggle_notify_click(Element* e);
static void dock_reopen_toast_click(Element* e);
static void modal_confirm_click(Element* e);
static void launcher_click(Element* e);
static int take_screenshot(const char* path);

void onScreenshot(Element* e) {
    (void)e;
    char path[512];
    time_t t = time(NULL);
    struct tm* tm_info = localtime(&t);
    if (tm_info && strftime(path, sizeof(path), "screenshot_%Y%m%d_%H%M%S.png", tm_info) > 0)
        take_screenshot(path);
    else
        take_screenshot("screenshot.png");
}

void onTabSwitch(Element* e) {
    if (!e || !e->data_tab[0]) return;
    char panel_id[64];
    snprintf(panel_id, sizeof(panel_id), "tab_%s", e->data_tab);
    for (int i = 0; i < elem_count; i++) {
        if (element_has_class(&elements[i], "nav_item"))
            remove_class(&elements[i], "active");
        if (element_has_class(&elements[i], "tab_panel"))
            remove_class(&elements[i], "active");
    }
    add_class(e, "active");
    update_element_style(e);
    int panel = get_element_by_id(panel_id);
    if (panel != -1) {
        add_class(&elements[panel], "active");
        update_element_style(&elements[panel]);
    }
}

void onToggleClick(Element* e) { toggle_click(e); }
void onCheckboxClick(Element* e) { checkbox_click(e); }
void onSwatchClick(Element* e) {
    int p = e->parent_idx;
    if (p != -1) {
        for (int i = 0; i < elem_count; i++) {
            if (elements[i].parent_idx == p && element_has_class(&elements[i], "swatch"))
                remove_class(&elements[i], "sel");
        }
    }
    add_class(e, "sel");
    update_element_style(e);
}

void onTabPillClick(Element* e) {
    int p = e->parent_idx;
    if (p != -1) {
        for (int i = 0; i < elem_count; i++) {
            if (elements[i].parent_idx == p && element_has_class(&elements[i], "tab_pill"))
                remove_class(&elements[i], "active");
        }
    }
    add_class(e, "active");
    update_element_style(e);
}

void onToolClick(Element* e) { tool_click(e); }
void onNavClick(Element* e) { nav_click(e); }
void onApply(Element* e) { btn_click(e); }
void onSelectToggle(Element* e) { select_toggle_click(e); }
void onSelectOption(Element* e) { select_option_click(e); }
void onToastDismiss(Element* e) { toast_dismiss(e); }
void onInfoClose(Element* e) { info_close_click(e); }
void onDockInfo(Element* e) { dock_toggle_info_click(e); }
void onDockTheme(Element* e) { dock_toggle_theme_click(e); }
void onDockNotify(Element* e) { dock_toggle_notify_click(e); }
void onDockToast(Element* e) { dock_reopen_toast_click(e); }
void onModalCancel(Element* e) { modal_cancel_click(e); }
void onModalConfirm(Element* e) { modal_confirm_click(e); }
void onClose(Element* e) { handle_close(e); }
void onMinimize(Element* e) { handle_minimize(e); }
void onMaximize(Element* e) { handle_maximize(e); }
void onLauncher(Element* e) { launcher_click(e); }

static void register_demo_js_handlers(void) {
    register_js_handler("onClose", onClose);
    register_js_handler("onMinimize", onMinimize);
    register_js_handler("onMaximize", onMaximize);
    register_js_handler("onApply", onApply);
    register_js_handler("onNavClick", onNavClick);
    register_js_handler("onTabSwitch", onTabSwitch);
    register_js_handler("onToolClick", onToolClick);
    register_js_handler("onToggleClick", onToggleClick);
    register_js_handler("onCheckboxClick", onCheckboxClick);
    register_js_handler("onSelectToggle", onSelectToggle);
    register_js_handler("onSelectOption", onSelectOption);
    register_js_handler("onToastDismiss", onToastDismiss);
    register_js_handler("onInfoClose", onInfoClose);
    register_js_handler("onDockInfo", onDockInfo);
    register_js_handler("onDockTheme", onDockTheme);
    register_js_handler("onDockNotify", onDockNotify);
    register_js_handler("onDockToast", onDockToast);
    register_js_handler("onModalCancel", onModalCancel);
    register_js_handler("onModalConfirm", onModalConfirm);
    register_js_handler("onScreenshot", onScreenshot);
    register_js_handler("onSwatchClick", onSwatchClick);
    register_js_handler("onTabPillClick", onTabPillClick);
    register_js_handler("onLauncher", onLauncher);
}

static void handle_close(Element* e) {
    (void)e;
    int title = get_element_by_id("title_text");
    if (title != -1) set_text(title, "Closing...");
    if (g_window) glfwSetWindowShouldClose(g_window, GLFW_TRUE);
}

static void btn_click(Element* e) {
    (void)e;
    if (g_modal_overlay_idx != -1) {
        g_focus_before_trap = g_focused_element_idx;
        remove_class(&elements[g_modal_overlay_idx], "hidden");
        update_element_style(&elements[g_modal_overlay_idx]);
        int mc = get_element_by_id("modal_cancel");
        if (mc != -1) focus_element(mc);
    }
}

static void modal_cancel_click(Element* e) {
    (void)e;
    if (g_modal_overlay_idx != -1) {
        add_class(&elements[g_modal_overlay_idx], "hidden");
        update_element_style(&elements[g_modal_overlay_idx]);
    }
    if (g_focus_before_trap != -1) {
        focus_element(g_focus_before_trap);
        g_focus_before_trap = -1;
    }
}

static void modal_confirm_click(Element* e) {
    (void)e;
    int desc = get_element_by_id("desc");
    if (desc != -1) set_text(desc, "Settings applied. Gradient CSS engine active!");
    if (g_modal_overlay_idx != -1) {
        add_class(&elements[g_modal_overlay_idx], "hidden");
        update_element_style(&elements[g_modal_overlay_idx]);
    }
    if (g_focus_before_trap != -1) {
        focus_element(g_focus_before_trap);
        g_focus_before_trap = -1;
    }
}

static void toggle_click(Element* e) {
    int knob = get_element_by_id("toggle_knob");
    if (element_has_class(e, "on")) {
        remove_class(e, "on");
        if (knob != -1) { elements[knob].rel_x = 3; elements[knob].pos_overridden_x = 1; }
    } else {
        add_class(e, "on");
        if (knob != -1) { elements[knob].rel_x = 23; elements[knob].pos_overridden_x = 1; }
    }
    update_element_style(e);
}

static void checkbox_click(Element* e) {
    if (element_has_class(e, "checked")) {
        remove_class(e, "checked");
        set_text(get_element_by_id("chk_label"), "Enable Notifications");
    } else {
        add_class(e, "checked");
        set_text(get_element_by_id("chk_label"), "Notifications: On");
    }
    update_element_style(e);
}

static void toast_dismiss(Element* e) {
    (void)e;
    if (g_toast_idx != -1) elements[g_toast_idx].display_none = 1;
}

static void tool_click(Element* e) {
    for (int i = 0; i < elem_count; i++) {
        if (element_has_class(&elements[i], "tool_btn"))
            remove_class(&elements[i], "active");
    }
    add_class(e, "active");
    update_element_style(e);
    int desc = get_element_by_id("desc");
    if (desc != -1) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Toolbar: %s selected. Flexbox layout active.", e->text);
        set_text(desc, buf);
    }
    int status = get_element_by_id("status_text");
    if (status != -1) {
        char buf[256];
        snprintf(buf, sizeof(buf), "Tool: %s", e->text);
        set_text(status, buf);
    }
}

static void nav_click(Element* e) {
    for (int i = 1; i <= 4; i++) {
        char nid[16];
        snprintf(nid, sizeof(nid), "nav_%d", i);
        int idx = get_element_by_id(nid);
        if (idx != -1) remove_class(&elements[idx], "active");
    }
    add_class(e, "active");
    update_element_style(e);
    int title = get_element_by_id("section_title");
    if (title != -1) set_text(title, e->text);
}

static void select_toggle_click(Element* e) {
    (void)e;
    if (g_select_panel_idx == -1) return;
    Element* panel = &elements[g_select_panel_idx];
    int box = get_element_by_id("theme_select_box");
    if (element_has_class(panel, "hidden")) {
        g_focus_before_trap = g_focused_element_idx;
        remove_class(panel, "hidden");
        update_element_style(panel);
        if (box != -1) elements[box].aria_expanded = 1;
        int opt = get_element_by_id("opt_blue");
        if (opt != -1) focus_element(opt);
    } else {
        add_class(panel, "hidden");
        update_element_style(panel);
        if (box != -1) elements[box].aria_expanded = 0;
        if (g_focus_before_trap != -1) {
            focus_element(g_focus_before_trap);
            g_focus_before_trap = -1;
        }
    }
}

static void select_option_click(Element* e) {
    int box = get_element_by_id("theme_select_box");
    if (box != -1) {
        char buf[96];
        snprintf(buf, sizeof(buf), "%.80s  v", e->text);
        set_text(box, buf);
    }
    if (g_select_panel_idx != -1) {
        add_class(&elements[g_select_panel_idx], "hidden");
        update_element_style(&elements[g_select_panel_idx]);
    }
    if (box != -1) elements[box].aria_expanded = 0;
    if (g_focus_before_trap != -1) {
        focus_element(g_focus_before_trap);
        g_focus_before_trap = -1;
    } else if (box != -1) {
        focus_element(box);
    }
}

static void info_close_click(Element* e) {
    (void)e;
    if (g_info_win_idx != -1) elements[g_info_win_idx].display_none = 1;
}

static void dock_toggle_info_click(Element* e) {
    (void)e;
    if (g_info_win_idx != -1)
        elements[g_info_win_idx].display_none = !elements[g_info_win_idx].display_none;
}

static void dock_toggle_theme_click(Element* e) {
    (void)e;
    int t = get_element_by_id("toggle_knob_track");
    if (t != -1) { toggle_click(&elements[t]); return; }
    for (int i = 0; i < elem_count; i++)
        if (element_has_class(&elements[i], "toggle")) { toggle_click(&elements[i]); return; }
}

static void dock_toggle_notify_click(Element* e) {
    (void)e;
    int c = get_element_by_id("checkbox_notify");
    if (c != -1) { checkbox_click(&elements[c]); return; }
    for (int i = 0; i < elem_count; i++)
        if (element_has_class(&elements[i], "checkbox")) { checkbox_click(&elements[i]); return; }
}

static void dock_reopen_toast_click(Element* e) {
    (void)e;
    if (g_toast_idx != -1) elements[g_toast_idx].display_none = 0;
}

static void handle_minimize(Element* e) {
    (void)e;
    if (g_window) glfwIconifyWindow(g_window);
}

static void handle_maximize(Element* e) {
    (void)e;
    if (!g_window) return;
    if (glfwGetWindowAttrib(g_window, GLFW_MAXIMIZED))
        glfwRestoreWindow(g_window);
    else
        glfwMaximizeWindow(g_window);
}

static void launcher_click(Element* e) {
    (void)e;
    fprintf(stderr, "[lu-shell] Launcher clicked\n");
}

static void update_clock(double now) {
    if (g_clock_idx == -1) return;
    if (now - g_last_clock_update < 1.0) return;
    g_last_clock_update = now;
    time_t t = time(NULL);
    struct tm* tm_info = localtime(&t);
    if (!tm_info) return;
    char buf[32];
    if (strftime(buf, sizeof(buf), "%H:%M", tm_info) > 0)
        set_text(g_clock_idx, buf);
}

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

static void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    (void)window;
    (void)width;
    (void)height;
    g_layout_dirty = 1;
}

static int element_is_inert(int idx) {
    if (idx == -1) return 0;
    if (elements[idx].inert) return 1;
    for (int p = elements[idx].parent_idx; p != -1; p = elements[p].parent_idx) {
        if (elements[p].inert) return 1;
    }
    int trap = get_focus_trap_root();
    if (trap != -1 && !is_descendant_of(idx, trap)) return 1;
    return 0;
}

static void focus_element(int idx) {
    focus_element_ex(idx, 0);
}

static void focus_element_ex(int idx, int via_keyboard) {
    if (idx != -1 && element_is_inert(idx)) return;
    int old = g_focused_element_idx;
    if (idx == g_focused_element_idx) {
        g_focus_via_keyboard = via_keyboard ? 1 : 0;
        if (idx != -1) update_element_style(&elements[idx]);
        return;
    }
    g_focused_element_idx = idx;
    g_focus_via_keyboard = via_keyboard ? 1 : 0;
    if (old != -1) update_focus_within_styles(old);
    if (idx != -1) {
        update_focus_within_styles(idx);
        scroll_into_view(idx);
    }
}

static int element_is_focusable(int idx) {
    if (element_is_inert(idx) || element_aria_hidden(idx)) return 0;
    Element* e = &elements[idx];
    if (!is_visible(idx) || e->display_none || e->pointer_events_none) return 0;
    if (e->tabindex == -1) return 0;
    if (e->tabindex >= 0) return 1;
    if (e->role[0] && strcmp(e->role, "button") == 0) return 1;
    if (e->role[0] && strcmp(e->role, "combobox") == 0) return 1;
    if (strcasecmp(e->type, "button") == 0) return 1;
    return e->on_click || e->cursor_pointer;
}

static int tab_order_cmp(const void* a, const void* b) {
    int ia = *(const int*)a;
    int ib = *(const int*)b;
    int ta = elements[ia].tabindex;
    int tb = elements[ib].tabindex;
    if (ta < 0) ta = 0;
    if (tb < 0) tb = 0;
    int pa = (elements[ia].tabindex > 0) ? 0 : 1;
    int pb = (elements[ib].tabindex > 0) ? 0 : 1;
    if (pa != pb) return pa - pb;
    if (pa == 0 && ta != tb) return ta - tb;
    return ia - ib;
}

static void focus_move_tab(int backward) {
    int trap = get_focus_trap_root();
    int order[MAX_ELEMENTS];
    int n = 0;
    for (int i = 0; i < elem_count; i++) {
        if (!element_is_focusable(i)) continue;
        if (trap != -1 && !is_descendant_of(i, trap)) continue;
        order[n++] = i;
    }
    if (n == 0) return;
    qsort(order, (size_t)n, sizeof(int), tab_order_cmp);

    int cur = -1;
    for (int i = 0; i < n; i++) {
        if (order[i] == g_focused_element_idx) { cur = i; break; }
    }
    int next;
    if (cur == -1) next = backward ? n - 1 : 0;
    else next = backward ? (cur - 1 + n) % n : (cur + 1) % n;
    focus_element_ex(order[next], 1);
}

static void focus_select_option_step(int backward) {
    if (g_select_panel_idx == -1) return;
    if (element_has_class(&elements[g_select_panel_idx], "hidden")) return;
    int options[MAX_ELEMENTS];
    int n = 0;
    for (int i = 0; i < elem_count; i++) {
        if (elements[i].parent_idx != g_select_panel_idx) continue;
        if (!element_has_class(&elements[i], "select_option")) continue;
        if (!element_is_focusable(i)) continue;
        options[n++] = i;
    }
    if (n == 0) return;
    int cur = -1;
    for (int i = 0; i < n; i++)
        if (options[i] == g_focused_element_idx) { cur = i; break; }
    int next = (cur == -1) ? 0 : backward ? (cur - 1 + n) % n : (cur + 1) % n;
    focus_element_ex(options[next], 1);
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    (void)scancode;
    if (action == GLFW_PRESS && key == GLFW_KEY_F12) {
        onScreenshot(NULL);
        return;
    }

    if (action == GLFW_PRESS && key == GLFW_KEY_ESCAPE) {
        int trap = get_focus_trap_root();
        if (trap != -1) {
            if (trap == g_modal_overlay_idx)
                modal_cancel_click(&elements[trap]);
            else if (trap == g_select_panel_idx) {
                add_class(&elements[trap], "hidden");
                update_element_style(&elements[trap]);
                if (g_focus_before_trap != -1)
                    focus_element(g_focus_before_trap);
            }
            return;
        }
        glfwSetWindowShouldClose(window, GLFW_TRUE);
        return;
    }

    if (action == GLFW_PRESS && key == GLFW_KEY_TAB) {
        focus_move_tab((mods & GLFW_MOD_SHIFT) != 0);
        return;
    }

    if ((action == GLFW_PRESS || action == GLFW_REPEAT) &&
        (key == GLFW_KEY_UP || key == GLFW_KEY_DOWN)) {
        int trap = get_focus_trap_root();
        if (trap == g_select_panel_idx) {
            focus_select_option_step(key == GLFW_KEY_UP);
            return;
        }
    }

    if (action == GLFW_PRESS &&
        (key == GLFW_KEY_ENTER || key == GLFW_KEY_KP_ENTER || key == GLFW_KEY_SPACE)) {
        if (g_focused_element_idx != -1) {
            Element* fe = &elements[g_focused_element_idx];
            if (fe->on_click) {
                fe->is_active = 1;
                update_element_style(fe);
                fe->on_click(fe);
                fe->is_active = 0;
                update_element_style(fe);
            }
        }
        return;
    }

    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    int start = g_focused_element_idx;
    if (start == -1) {
        double mx, my;
        glfwGetCursorPos(window, &mx, &my);
        start = hit_test_at(mx, my);
    }
    if (start == -1) return;

    int sy = find_scroll_target_y(start);
    int sx = find_scroll_target_x(start);
    float line = 20.0f;

    if (sy != -1 && (key == GLFW_KEY_UP || key == GLFW_KEY_DOWN ||
                     key == GLFW_KEY_PAGE_UP || key == GLFW_KEY_PAGE_DOWN ||
                     key == GLFW_KEY_HOME || key == GLFW_KEY_END)) {
        Element* sc = &elements[sy];
        float pad = sc->padding;
        float inner_h = sc->h - pad * 2.0f;
        float max_scroll = sc->scroll_content_h - inner_h;
        if (max_scroll < 0.0f) max_scroll = 0.0f;
        float page = inner_h * 0.85f;
        if (key == GLFW_KEY_UP) add_scroll_top(sy, -line, 0);
        else if (key == GLFW_KEY_DOWN) add_scroll_top(sy, line, 0);
        else if (key == GLFW_KEY_PAGE_UP) add_scroll_top(sy, -page, 0);
        else if (key == GLFW_KEY_PAGE_DOWN) add_scroll_top(sy, page, 0);
        else if (key == GLFW_KEY_HOME) set_scroll_top(sy, 0.0f, 0);
        else if (key == GLFW_KEY_END) set_scroll_top(sy, max_scroll, 0);
        return;
    }

    if (sx != -1 && (key == GLFW_KEY_LEFT || key == GLFW_KEY_RIGHT ||
                     ((mods & GLFW_MOD_SHIFT) &&
                      (key == GLFW_KEY_PAGE_UP || key == GLFW_KEY_PAGE_DOWN)))) {
        Element* sc = &elements[sx];
        float pad = sc->padding;
        float inner_w = sc->w - pad * 2.0f;
        float max_scroll = sc->scroll_content_w - inner_w;
        if (max_scroll < 0.0f) max_scroll = 0.0f;
        float page = inner_w * 0.85f;
        if (key == GLFW_KEY_LEFT) add_scroll_left(sx, -line, 0);
        else if (key == GLFW_KEY_RIGHT) add_scroll_left(sx, line, 0);
        else if (key == GLFW_KEY_PAGE_UP) add_scroll_left(sx, -page, 0);
        else if (key == GLFW_KEY_PAGE_DOWN) add_scroll_left(sx, page, 0);
        (void)mods;
    }
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
        } else if (strcmp(argv[i], "--screenshot") == 0 && i + 1 < argc) {
            strncpy(g_screenshot_path, argv[++i], sizeof(g_screenshot_path) - 1);
            g_screenshot_pending = 1;
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            fprintf(stderr, "lu-shell [--desktop] [--fullscreen] [--size WxH] [--layout PATH] [--css PATH] [--screenshot PATH]\n");
            fprintf(stderr, "  F12 — capture screenshot (PNG)\n");
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

#define DEMO_CSS_INCLUDE
static const char* default_css =
#include "demo.css.h"
;

#define DEMO_HTML_INCLUDE
static const char* default_html =
#include "demo.html.h"
;

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
    glfwSetErrorCallback(glfw_error_callback);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    if (g_fullscreen) {
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
        glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
    }

    const char* win_title = g_desktop_mode ? "Lu Shell" :
        (g_doc_title[0] ? g_doc_title : "Vespera GUI Engine");
    GLFWmonitor* monitor = g_fullscreen ? glfwGetPrimaryMonitor() : NULL;
    GLFWwindow* window = glfwCreateWindow((int)window_width, (int)window_height, win_title, monitor, NULL);
    g_window = window;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);
    load_gl_functions();
    g_hand_cursor     = glfwCreateStandardCursor(GLFW_HAND_CURSOR);
    g_cursor_ibeam      = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
    g_cursor_crosshair  = glfwCreateStandardCursor(GLFW_CROSSHAIR_CURSOR);
    g_cursor_hresize    = glfwCreateStandardCursor(GLFW_HRESIZE_CURSOR);
    g_cursor_vresize    = glfwCreateStandardCursor(GLFW_VRESIZE_CURSOR);

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

    GLuint svs   = compile_shader(bg_vs,     GL_VERTEX_SHADER);
    GLuint sfs   = compile_shader(shadow_fs, GL_FRAGMENT_SHADER);
    shadow_program = glCreateProgram();
    glAttachShader(shadow_program, svs); glAttachShader(shadow_program, sfs);
    glLinkProgram(shadow_program);

    GLuint ivs   = compile_shader(bg_vs,  GL_VERTEX_SHADER);
    GLuint ifs   = compile_shader(img_fs, GL_FRAGMENT_SHADER);
    img_program  = glCreateProgram();
    glAttachShader(img_program, ivs); glAttachShader(img_program, ifs);
    glLinkProgram(img_program);

    // Cache uniform locations once so every draw call skips the driver string lookup.
    bg_loc.uResolution  = glGetUniformLocation(bg_program, "uResolution");
    bg_loc.uPos         = glGetUniformLocation(bg_program, "uPos");
    bg_loc.uSize        = glGetUniformLocation(bg_program, "uSize");
    bg_loc.uColor       = glGetUniformLocation(bg_program, "uColor");
    bg_loc.uBorderColor = glGetUniformLocation(bg_program, "uBorderColor");
    bg_loc.uBorderWidth = glGetUniformLocation(bg_program, "uBorderWidth");
    bg_loc.uRadius      = glGetUniformLocation(bg_program, "uRadius");
    bg_loc.uGradient    = glGetUniformLocation(bg_program, "uGradient");
    bg_loc.uGradStopCount = glGetUniformLocation(bg_program, "uGradStopCount");
    bg_loc.uGradAngle   = glGetUniformLocation(bg_program, "uGradAngle");
    bg_loc.uGradCenter  = glGetUniformLocation(bg_program, "uGradCenter");
    bg_loc.uGradRadius  = glGetUniformLocation(bg_program, "uGradRadius");
    for (int i = 0; i < MAX_GRAD_STOPS; i++) {
        char uname[32];
        snprintf(uname, sizeof(uname), "uGradColors[%d]", i);
        bg_loc.uGradColors[i] = glGetUniformLocation(bg_program, uname);
        snprintf(uname, sizeof(uname), "uGradStops[%d]", i);
        bg_loc.uGradStops[i]  = glGetUniformLocation(bg_program, uname);
    }

    sh_loc.uResolution  = glGetUniformLocation(shadow_program, "uResolution");
    sh_loc.uPos         = glGetUniformLocation(shadow_program, "uPos");
    sh_loc.uSize        = glGetUniformLocation(shadow_program, "uSize");
    sh_loc.uShadowColor = glGetUniformLocation(shadow_program, "uShadowColor");
    sh_loc.uElemSize    = glGetUniformLocation(shadow_program, "uElemSize");
    sh_loc.uRadius      = glGetUniformLocation(shadow_program, "uRadius");
    sh_loc.uBlur        = glGetUniformLocation(shadow_program, "uBlur");
    sh_loc.uShadowInset = glGetUniformLocation(shadow_program, "uShadowInset");
    sh_loc.uOffset      = glGetUniformLocation(shadow_program, "uOffset");

    tx_loc.uResolution  = glGetUniformLocation(text_program, "uResolution");
    tx_loc.textColor    = glGetUniformLocation(text_program, "textColor");

    img_loc.uResolution = glGetUniformLocation(img_program, "uResolution");
    img_loc.uPos        = glGetUniformLocation(img_program, "uPos");
    img_loc.uSize       = glGetUniformLocation(img_program, "uSize");
    img_loc.uRadius     = glGetUniformLocation(img_program, "uRadius");
    img_loc.uAlpha      = glGetUniformLocation(img_program, "uAlpha");
    img_loc.uImage      = glGetUniformLocation(img_program, "uImage");

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    init_rect_geometry();
    init_font();

    g_css_from_document = 0;

    // Load HTML document (parses <link>/<style> in <head>), then fallback CSS
    set_html_base_dir(g_layout_path);
    char* html_str = read_file(g_layout_path);
    if (html_str) {
        parse_html(html_str);
        free(html_str);
    } else if (!g_desktop_mode) {
        set_html_base_dir("ui");
        parse_html(default_html);
    }

    if (!g_css_from_document) {
        char* css_str = read_file(g_css_path);
        if (css_str) { parse_css(css_str); free(css_str); }
        else if (!g_desktop_mode) { parse_css(default_css); }
    }

    // Inject a full-screen body background element driven by CSS body/html rules.
    // Inserted at the END with z_index=-9999 so it always sorts behind everything.
    if (elem_count < MAX_ELEMENTS) {
        Element body_e;
        memset(&body_e, 0, sizeof(body_e));
        strncpy(body_e.type, "body", 31);
        body_e.id_idx      = elem_count;
        body_e.parent_idx  = -1;
        body_e.z_index     = -9999;
        body_e.pct_w       = 1; body_e.raw_w = 1.0f;  // 100% of window
        body_e.pct_h       = 1; body_e.raw_h = 1.0f;
        body_e.x = 0.0f; body_e.y = 0.0f;
        body_e.w = window_width; body_e.h = window_height;
        body_e.opacity     = 1.0f;
        body_e.transform_scale = 1.0f;
        body_e.cur_scale   = 1.0f;
        body_e.anim_speed  = 14.0f;
        body_e.overflow_x  = OVERFLOW_VISIBLE;
        body_e.overflow_y  = OVERFLOW_VISIBLE;
        elements[elem_count] = body_e;
        update_element_style(&elements[elem_count]);
        // Force 100% size even if CSS min/max would resize
        elements[elem_count].pct_w = 1; elements[elem_count].raw_w = 1.0f;
        elements[elem_count].pct_h = 1; elements[elem_count].raw_h = 1.0f;
        elements[elem_count].has_min_width = 0; elements[elem_count].has_max_width = 0;
        elements[elem_count].has_min_height = 0; elements[elem_count].has_max_height = 0;
        Element* ne = &elements[elem_count];
        ne->cur_r = ne->r; ne->cur_g = ne->g; ne->cur_b = ne->b; ne->cur_a = ne->a;
        ne->cur_bd_r = ne->bd_r; ne->cur_bd_g = ne->bd_g;
        ne->cur_bd_b = ne->bd_b; ne->cur_bd_a = ne->bd_a;
        elem_count++;
    }

    g_layout_dirty = 1;
    register_demo_js_handlers();
    wire_element_onclick_handlers();

    g_clock_idx = get_element_by_id("clock");
    update_clock(glfwGetTime() + 1.0);

    int main_win = get_element_by_id("main_win");
    if (main_win != -1) {
        g_focused_idx = main_win;
        g_top_z = elements[main_win].z_index > g_top_z ? elements[main_win].z_index : g_top_z;
    }

    // Register JS-compatible onclick handlers (wired from HTML onclick="...")
    g_toast_idx          = get_element_by_id("toast");
    g_modal_overlay_idx  = get_element_by_id("modal_overlay");
    g_info_win_idx       = get_element_by_id("info_win");
    g_select_panel_idx   = get_element_by_id("select_panel");
    g_brightness_thumb_idx  = get_element_by_id("slider_thumb");
    g_brightness_track_idx  = get_element_by_id("slider_track");
    g_brightness_fill_idx   = get_element_by_id("slider_fill");
    g_brightness_value_idx  = get_element_by_id("slider_label");
    if (g_clock_idx == -1) g_clock_idx = get_element_by_id("clock");

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetCursorPosCallback(window, cursor_position_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwSetScrollCallback(window, scroll_callback);

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

        glClearColor(0.06f, 0.06f, 0.10f, 1.0f);  // dark fallback; body CSS drives actual bg
        glClear(GL_COLOR_BUFFER_BIT);

        if (window_width != g_last_window_w || window_height != g_last_window_h) {
            g_layout_dirty = 1;
            g_last_window_w = window_width;
            g_last_window_h = window_height;
        }

        tick_smooth_scroll(dt);
        if (g_layout_dirty) {
            update_layout_pass();
            g_layout_dirty = 0;
        }
        update_css_keyframe_animations(now);
        update_animations(dt);
        update_clock(now);

        if (g_brightness_thumb_idx != -1 && g_brightness_track_idx != -1) {
            Element* thumb = &elements[g_brightness_thumb_idx];
            Element* track = &elements[g_brightness_track_idx];
            float usable = track->w - thumb->w;
            float ratio = (usable > 0.0f) ? (thumb->rel_x / usable) : 0.0f;
            if (ratio < 0.0f) ratio = 0.0f;
            if (ratio > 1.0f) ratio = 1.0f;
            if (g_brightness_fill_idx != -1)
                elements[g_brightness_fill_idx].w = thumb->rel_x + thumb->w * 0.5f;
            g_layout_dirty = 1;
            if (g_brightness_value_idx != -1) {
                char buf[32];
                snprintf(buf, sizeof(buf), "Brightness: %d%%", (int)(ratio * 100.0f + 0.5f));
                set_text(g_brightness_value_idx, buf);
            }
        }

        build_render_order();
        glDisable(GL_SCISSOR_TEST);

        // --- Shadow + Element + Text pass (single pass, correct z-index ordering) ---
        // Shadow, element rect, and text are all rendered together per element so that
        // a higher-z element and its text correctly appear above lower-z elements.
        for (int ri = 0; ri < elem_count; ri++) {
            int i = render_order[ri];
            Element* e = &elements[i];
            if (!is_rendered(i)) continue;

            float eff_op = element_effective_opacity(i);
            float dx, dy, dw, dh;
            get_element_draw_bounds(e, &dx, &dy, &dw, &dh);
            float scale = e->cur_scale;

            // Shadow sub-pass — clipped by ancestor overflow:hidden per CSS spec.
            // Scissor is already disabled at loop start (glDisable at end of prev iter).
            if (e->has_shadow && e->sh_a > 0.0f) {
                set_element_scissor(i, fbw, fbh);
                draw_shadow(dx, dy, dw, dh,
                            e->sh_dx, e->sh_dy, e->sh_blur,
                            e->sh_r, e->sh_g, e->sh_b, e->sh_a,
                            e->border_radius * scale, eff_op);
            }

            Element draw_e = *e;
            if (draw_e.has_gradient) {
                for (int s = 0; s < draw_e.grad_stop_count; s++)
                    draw_e.grad_stop_a[s] *= eff_op;
            }

            set_element_scissor(i, fbw, fbh);
            draw_rect_full(dx, dy, dw, dh,
                           draw_e.cur_r, draw_e.cur_g, draw_e.cur_b, draw_e.cur_a * eff_op,
                           draw_e.border_radius * scale, draw_e.border_width,
                           draw_e.cur_bd_r, draw_e.cur_bd_g, draw_e.cur_bd_b, draw_e.cur_bd_a * eff_op,
                           &draw_e);

            // Background image: drawn over the fill colour, inset by border-width so
            // the border is still visible around the image.
            if (e->has_bg_image && e->bg_image_path[0]) {
                if (!e->bg_image_tex)
                    e->bg_image_tex = load_or_get_texture(e->bg_image_path);
                if (e->bg_image_tex) {
                    float bw = draw_e.border_width;
                    draw_image(dx + bw, dy + bw, dw - 2.0f * bw, dh - 2.0f * bw,
                               draw_e.border_radius * scale, e->bg_image_tex, eff_op);
                }
            }

            // Text drawn immediately after rect (same scissor, correct z-order).
            // Use dx/dy (transform-adjusted position) so text tracks translateX/Y.
            if (e->text[0]) {
                float pad = e->padding * scale;
                float box_w = dw - pad * 2.0f;
                float box_h = dh - pad * 2.0f;
                int text_align = e->text_align;
                if (text_align == 0) {
                    if (e->justify_content == FLEX_JUSTIFY_CENTER ||
                        e->justify_items  == FLEX_ALIGN_CENTER    ||
                        e->justify_self   == FLEX_ALIGN_CENTER)
                        text_align = 1;
                    else if (e->justify_content == FLEX_JUSTIFY_END ||
                             e->justify_self    == FLEX_ALIGN_END)
                        text_align = 2;
                }
                int v_align = 1;
                if (e->display_mode == DISPLAY_FLEX) {
                    if (e->align_items == FLEX_ALIGN_START)
                        v_align = 0;
                    else if (e->align_items == FLEX_ALIGN_END)
                        v_align = 2;
                }
                render_text(e->text,
                            dx + pad, dy + pad,
                            box_w, box_h, text_align, v_align,
                            e->t_r, e->t_g, e->t_b, e->t_a * eff_op,
                            e->font_size, e->font_bold,
                            e->line_height, e->white_space,
                            e->text_overflow, e->overflow_wrap);
            }
            glDisable(GL_SCISSOR_TEST);
        }

        draw_scrollbars();
        draw_focus_outlines();
        draw_a11y_live_region();

        glfwSwapBuffers(window);
        if (g_screenshot_pending && g_screenshot_path[0]) {
            take_screenshot(g_screenshot_path);
            g_screenshot_pending = 0;
        }
        glfwPollEvents();
    }

    glfwTerminate();
    return 0;
}
