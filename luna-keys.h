/*
 * luna-keys.h — host-neutral input constants for Luna UI.
 *
 * A leaf header: it includes nothing and defines nothing but constants, so it
 * can be pulled in from a platform host's *prelude* pass (before luna-ui.h's
 * body) as well as from application code.
 *
 * The numeric values deliberately match GLFW 3.4.  Luna's hosts have always
 * fed GLFW-numbered key codes into luna_key(), and applications compare
 * against them directly; keeping the numbers lets GLFW-free hosts coexist with
 * the GLFW host during the migration and turns application updates into a
 * rename rather than a semantic review of every comparison.
 *
 * Copyright © 2026 Yuichiro Nakada / Project Luna (Vespera) — MPL 2.0
 */
#ifndef LUNA_KEYS_H
#define LUNA_KEYS_H

/* Key/button actions, as delivered to luna_key() and luna_mouse_button(). */
enum {
    LUNA_RELEASE = 0,
    LUNA_PRESS   = 1,
    LUNA_REPEAT  = 2
};

/* "No constraint" sentinel for luna_platform_set_size_limits(). */
#define LUNA_DONT_CARE (-1)

enum {
    LUNA_MOUSE_BUTTON_LEFT   = 0,
    LUNA_MOUSE_BUTTON_RIGHT  = 1,
    LUNA_MOUSE_BUTTON_MIDDLE = 2,
    LUNA_MOUSE_BUTTON_4      = 3,
    LUNA_MOUSE_BUTTON_5      = 4,
    LUNA_MOUSE_BUTTON_6      = 5,
    LUNA_MOUSE_BUTTON_7      = 6,
    LUNA_MOUSE_BUTTON_8      = 7,
    LUNA_MOUSE_BUTTON_LAST   = 7
};

enum {
    LUNA_MOD_SHIFT     = 0x0001,
    LUNA_MOD_CONTROL   = 0x0002,
    LUNA_MOD_ALT       = 0x0004,
    LUNA_MOD_SUPER     = 0x0008,
    LUNA_MOD_CAPS_LOCK = 0x0010,
    LUNA_MOD_NUM_LOCK  = 0x0020
};

/*
 * Physical key identity, layout-independent.  Text always arrives separately
 * through luna_char(); never derive characters from these.
 */
enum {
    LUNA_KEY_UNKNOWN       = -1,

    /* Printable ASCII subset, values equal to their US-layout characters. */
    LUNA_KEY_SPACE         = 32,
    LUNA_KEY_APOSTROPHE    = 39,  /* ' */
    LUNA_KEY_COMMA         = 44,  /* , */
    LUNA_KEY_MINUS         = 45,  /* - */
    LUNA_KEY_PERIOD        = 46,  /* . */
    LUNA_KEY_SLASH         = 47,  /* / */
    LUNA_KEY_0             = 48,
    LUNA_KEY_1             = 49,
    LUNA_KEY_2             = 50,
    LUNA_KEY_3             = 51,
    LUNA_KEY_4             = 52,
    LUNA_KEY_5             = 53,
    LUNA_KEY_6             = 54,
    LUNA_KEY_7             = 55,
    LUNA_KEY_8             = 56,
    LUNA_KEY_9             = 57,
    LUNA_KEY_SEMICOLON     = 59,  /* ; */
    LUNA_KEY_EQUAL         = 61,  /* = */
    LUNA_KEY_A             = 65,
    LUNA_KEY_B             = 66,
    LUNA_KEY_C             = 67,
    LUNA_KEY_D             = 68,
    LUNA_KEY_E             = 69,
    LUNA_KEY_F             = 70,
    LUNA_KEY_G             = 71,
    LUNA_KEY_H             = 72,
    LUNA_KEY_I             = 73,
    LUNA_KEY_J             = 74,
    LUNA_KEY_K             = 75,
    LUNA_KEY_L             = 76,
    LUNA_KEY_M             = 77,
    LUNA_KEY_N             = 78,
    LUNA_KEY_O             = 79,
    LUNA_KEY_P             = 80,
    LUNA_KEY_Q             = 81,
    LUNA_KEY_R             = 82,
    LUNA_KEY_S             = 83,
    LUNA_KEY_T             = 84,
    LUNA_KEY_U             = 85,
    LUNA_KEY_V             = 86,
    LUNA_KEY_W             = 87,
    LUNA_KEY_X             = 88,
    LUNA_KEY_Y             = 89,
    LUNA_KEY_Z             = 90,
    LUNA_KEY_LEFT_BRACKET  = 91,  /* [ */
    LUNA_KEY_BACKSLASH     = 92,  /* \ */
    LUNA_KEY_RIGHT_BRACKET = 93,  /* ] */
    LUNA_KEY_GRAVE_ACCENT  = 96,  /* ` */
    LUNA_KEY_WORLD_1       = 161, /* non-US #1 */
    LUNA_KEY_WORLD_2       = 162, /* non-US #2 */

    /* Function keys. */
    LUNA_KEY_ESCAPE        = 256,
    LUNA_KEY_ENTER         = 257,
    LUNA_KEY_TAB           = 258,
    LUNA_KEY_BACKSPACE     = 259,
    LUNA_KEY_INSERT        = 260,
    LUNA_KEY_DELETE        = 261,
    LUNA_KEY_RIGHT         = 262,
    LUNA_KEY_LEFT          = 263,
    LUNA_KEY_DOWN          = 264,
    LUNA_KEY_UP            = 265,
    LUNA_KEY_PAGE_UP       = 266,
    LUNA_KEY_PAGE_DOWN     = 267,
    LUNA_KEY_HOME          = 268,
    LUNA_KEY_END           = 269,
    LUNA_KEY_CAPS_LOCK     = 280,
    LUNA_KEY_SCROLL_LOCK   = 281,
    LUNA_KEY_NUM_LOCK      = 282,
    LUNA_KEY_PRINT_SCREEN  = 283,
    LUNA_KEY_PAUSE         = 284,
    LUNA_KEY_F1            = 290,
    LUNA_KEY_F2            = 291,
    LUNA_KEY_F3            = 292,
    LUNA_KEY_F4            = 293,
    LUNA_KEY_F5            = 294,
    LUNA_KEY_F6            = 295,
    LUNA_KEY_F7            = 296,
    LUNA_KEY_F8            = 297,
    LUNA_KEY_F9            = 298,
    LUNA_KEY_F10           = 299,
    LUNA_KEY_F11           = 300,
    LUNA_KEY_F12           = 301,
    LUNA_KEY_F13           = 302,
    LUNA_KEY_F14           = 303,
    LUNA_KEY_F15           = 304,
    LUNA_KEY_F16           = 305,
    LUNA_KEY_F17           = 306,
    LUNA_KEY_F18           = 307,
    LUNA_KEY_F19           = 308,
    LUNA_KEY_F20           = 309,
    LUNA_KEY_F21           = 310,
    LUNA_KEY_F22           = 311,
    LUNA_KEY_F23           = 312,
    LUNA_KEY_F24           = 313,
    LUNA_KEY_F25           = 314,

    /* Keypad. */
    LUNA_KEY_KP_0          = 320,
    LUNA_KEY_KP_1          = 321,
    LUNA_KEY_KP_2          = 322,
    LUNA_KEY_KP_3          = 323,
    LUNA_KEY_KP_4          = 324,
    LUNA_KEY_KP_5          = 325,
    LUNA_KEY_KP_6          = 326,
    LUNA_KEY_KP_7          = 327,
    LUNA_KEY_KP_8          = 328,
    LUNA_KEY_KP_9          = 329,
    LUNA_KEY_KP_DECIMAL    = 330,
    LUNA_KEY_KP_DIVIDE     = 331,
    LUNA_KEY_KP_MULTIPLY   = 332,
    LUNA_KEY_KP_SUBTRACT   = 333,
    LUNA_KEY_KP_ADD        = 334,
    LUNA_KEY_KP_ENTER      = 335,
    LUNA_KEY_KP_EQUAL      = 336,

    /* Modifiers. */
    LUNA_KEY_LEFT_SHIFT    = 340,
    LUNA_KEY_LEFT_CONTROL  = 341,
    LUNA_KEY_LEFT_ALT      = 342,
    LUNA_KEY_LEFT_SUPER    = 343,
    LUNA_KEY_RIGHT_SHIFT   = 344,
    LUNA_KEY_RIGHT_CONTROL = 345,
    LUNA_KEY_RIGHT_ALT     = 346,
    LUNA_KEY_RIGHT_SUPER   = 347,
    LUNA_KEY_MENU          = 348,
    LUNA_KEY_LAST          = 348
};

/*
 * Linux evdev scancode → LUNA_KEY_*.
 *
 * Wayland's wl_keyboard.key carries a raw evdev code, and X11 keycodes are the
 * same numbering biased by 8 — but only on evdev-backed X servers, so the X11
 * host resolves through keysyms instead and does not use this table.
 *
 * Define LUNA_KEYS_EVDEV_TABLE in exactly one translation unit to emit it.
 */
#endif /* LUNA_KEYS_H */

#if defined(LUNA_KEYS_EVDEV_TABLE) && !defined(LUNA_KEYS_EVDEV_TABLE_EMITTED)
#define LUNA_KEYS_EVDEV_TABLE_EMITTED 1
static const short luna_evdev_to_key[256] = {
    /*   0 */ -1, LUNA_KEY_ESCAPE, LUNA_KEY_1, LUNA_KEY_2, LUNA_KEY_3,
    /*   5 */ LUNA_KEY_4, LUNA_KEY_5, LUNA_KEY_6, LUNA_KEY_7, LUNA_KEY_8,
    /*  10 */ LUNA_KEY_9, LUNA_KEY_0, LUNA_KEY_MINUS, LUNA_KEY_EQUAL,
    /*  14 */ LUNA_KEY_BACKSPACE, LUNA_KEY_TAB, LUNA_KEY_Q, LUNA_KEY_W,
    /*  18 */ LUNA_KEY_E, LUNA_KEY_R, LUNA_KEY_T, LUNA_KEY_Y, LUNA_KEY_U,
    /*  23 */ LUNA_KEY_I, LUNA_KEY_O, LUNA_KEY_P, LUNA_KEY_LEFT_BRACKET,
    /*  27 */ LUNA_KEY_RIGHT_BRACKET, LUNA_KEY_ENTER, LUNA_KEY_LEFT_CONTROL,
    /*  30 */ LUNA_KEY_A, LUNA_KEY_S, LUNA_KEY_D, LUNA_KEY_F, LUNA_KEY_G,
    /*  35 */ LUNA_KEY_H, LUNA_KEY_J, LUNA_KEY_K, LUNA_KEY_L,
    /*  39 */ LUNA_KEY_SEMICOLON, LUNA_KEY_APOSTROPHE, LUNA_KEY_GRAVE_ACCENT,
    /*  42 */ LUNA_KEY_LEFT_SHIFT, LUNA_KEY_BACKSLASH, LUNA_KEY_Z, LUNA_KEY_X,
    /*  46 */ LUNA_KEY_C, LUNA_KEY_V, LUNA_KEY_B, LUNA_KEY_N, LUNA_KEY_M,
    /*  51 */ LUNA_KEY_COMMA, LUNA_KEY_PERIOD, LUNA_KEY_SLASH,
    /*  54 */ LUNA_KEY_RIGHT_SHIFT, LUNA_KEY_KP_MULTIPLY, LUNA_KEY_LEFT_ALT,
    /*  57 */ LUNA_KEY_SPACE, LUNA_KEY_CAPS_LOCK, LUNA_KEY_F1, LUNA_KEY_F2,
    /*  61 */ LUNA_KEY_F3, LUNA_KEY_F4, LUNA_KEY_F5, LUNA_KEY_F6, LUNA_KEY_F7,
    /*  66 */ LUNA_KEY_F8, LUNA_KEY_F9, LUNA_KEY_F10, LUNA_KEY_NUM_LOCK,
    /*  70 */ LUNA_KEY_SCROLL_LOCK, LUNA_KEY_KP_7, LUNA_KEY_KP_8, LUNA_KEY_KP_9,
    /*  74 */ LUNA_KEY_KP_SUBTRACT, LUNA_KEY_KP_4, LUNA_KEY_KP_5, LUNA_KEY_KP_6,
    /*  78 */ LUNA_KEY_KP_ADD, LUNA_KEY_KP_1, LUNA_KEY_KP_2, LUNA_KEY_KP_3,
    /*  82 */ LUNA_KEY_KP_0, LUNA_KEY_KP_DECIMAL, -1, -1, LUNA_KEY_WORLD_1,
    /*  87 */ LUNA_KEY_F11, LUNA_KEY_F12, -1, -1, -1, -1, -1, -1, -1,
    /*  96 */ LUNA_KEY_KP_ENTER, LUNA_KEY_RIGHT_CONTROL, LUNA_KEY_KP_DIVIDE,
    /*  99 */ LUNA_KEY_PRINT_SCREEN, LUNA_KEY_RIGHT_ALT, -1, LUNA_KEY_HOME,
    /* 103 */ LUNA_KEY_UP, LUNA_KEY_PAGE_UP, LUNA_KEY_LEFT, LUNA_KEY_RIGHT,
    /* 107 */ LUNA_KEY_END, LUNA_KEY_DOWN, LUNA_KEY_PAGE_DOWN, LUNA_KEY_INSERT,
    /* 111 */ LUNA_KEY_DELETE, -1, -1, -1, -1, -1, -1, -1,
    /* 119 */ LUNA_KEY_PAUSE, -1, -1, -1, -1, LUNA_KEY_WORLD_2,
    /* 125 */ LUNA_KEY_LEFT_SUPER, LUNA_KEY_RIGHT_SUPER, LUNA_KEY_MENU,
    /* 128 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /* 144 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /* 160 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /* 176 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /* 192 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /* 208 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /* 224 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    /* 240 */ -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

/* Evdev scancode → LUNA_KEY_*, or LUNA_KEY_UNKNOWN when unmapped. */
static inline int luna_key_from_evdev(unsigned int code) {
    if (code >= 256) return LUNA_KEY_UNKNOWN;
    return luna_evdev_to_key[code];
}
#endif /* LUNA_KEYS_EVDEV_TABLE */

#ifndef LUNA_KEYS_BUTTONS_DEFINED
#define LUNA_KEYS_BUTTONS_DEFINED 1

/* Linux evdev pointer buttons (linux/input-event-codes.h) → LUNA_MOUSE_*. */
#define LUNA_BTN_LEFT   0x110
#define LUNA_BTN_RIGHT  0x111
#define LUNA_BTN_MIDDLE 0x112
#define LUNA_BTN_SIDE   0x113
#define LUNA_BTN_EXTRA  0x114

static inline int luna_button_from_evdev(unsigned int code) {
    switch (code) {
    case LUNA_BTN_LEFT:   return LUNA_MOUSE_BUTTON_LEFT;
    case LUNA_BTN_RIGHT:  return LUNA_MOUSE_BUTTON_RIGHT;
    case LUNA_BTN_MIDDLE: return LUNA_MOUSE_BUTTON_MIDDLE;
    case LUNA_BTN_SIDE:   return LUNA_MOUSE_BUTTON_4;
    case LUNA_BTN_EXTRA:  return LUNA_MOUSE_BUTTON_5;
    default:              return -1;
    }
}

#endif /* LUNA_KEYS_BUTTONS_DEFINED */
