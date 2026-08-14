/* luna_windows.h - header-only Win32/WGL host for Luna UI */

#if defined(LUNA_UI_PLATFORM_PRELUDE)
#ifndef LUNA_WINDOWS_PRELUDE_INCLUDED
#define LUNA_WINDOWS_PRELUDE_INCLUDED
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <imm.h>
#include <GL/gl.h>
#define LUNA_UI_PLATFORM_GL_INCLUDED 1
#endif
#endif /* LUNA_UI_PLATFORM_PRELUDE */

#if defined(LUNA_UI_PLATFORM_BODY)
#ifndef LUNA_WINDOWS_BODY_INCLUDED
#define LUNA_WINDOWS_BODY_INCLUDED

#ifdef __cplusplus
extern "C" {
#endif

typedef struct LunaWindowsOptions {
    HINSTANCE instance;
    HICON icon;
    const wchar_t* class_name;
} LunaWindowsOptions;

void luna_windows_set_options(const LunaWindowsOptions* options);
HWND luna_windows_hwnd(void);

#ifdef __cplusplus
}
#endif
#endif /* LUNA_WINDOWS_BODY_INCLUDED */

#if defined(LUNA_UI_PLATFORM_BODY) && defined(LUNA_UI_IMPLEMENTATION) && !defined(LUNA_WINDOWS_IMPLEMENTATION_INCLUDED)
#define LUNA_WINDOWS_IMPLEMENTATION_INCLUDED

#if defined(_MSC_VER)
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "imm32.lib")
#pragma comment(lib, "shell32.lib")
#endif

#ifndef WGL_CONTEXT_MAJOR_VERSION_ARB
#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_FLAGS_ARB         0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB  0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#endif

typedef HGLRC (WINAPI *LunaWglCreateContextAttribsARB)(HDC, HGLRC, const int*);
typedef BOOL  (WINAPI *LunaWglSwapIntervalEXT)(int);

typedef struct LunaWindowsState {
    HINSTANCE instance;
    HWND hwnd;
    HDC dc;
    HGLRC glrc;
    HMODULE opengl32;
    LARGE_INTEGER qpc_frequency;
    int running;
    int redraw;
    int mouse_captured;
    int notify_icon_added;
    wchar_t pending_high_surrogate;
    LunaAppConfig config;
    LunaWindowsOptions options;
} LunaWindowsState;

static LunaWindowsState luna_win;
static LunaWindowsOptions luna_win_options;

static wchar_t* luna_win_utf8_to_wide(const char* text) {
    int n;
    wchar_t* out;
    if (!text) text = "";
    n = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    if (n <= 0) return NULL;
    out = (wchar_t*)malloc((size_t)n * sizeof(wchar_t));
    if (!out) return NULL;
    if (!MultiByteToWideChar(CP_UTF8, 0, text, -1, out, n)) {
        free(out);
        return NULL;
    }
    return out;
}

static char* luna_win_wide_to_utf8(const wchar_t* text) {
    int n;
    char* out;
    if (!text) return NULL;
    n = WideCharToMultiByte(CP_UTF8, 0, text, -1, NULL, 0, NULL, NULL);
    if (n <= 0) return NULL;
    out = (char*)malloc((size_t)n);
    if (!out) return NULL;
    if (!WideCharToMultiByte(CP_UTF8, 0, text, -1, out, n, NULL, NULL)) {
        free(out);
        return NULL;
    }
    return out;
}

static unsigned char* luna_win_read_wide_file(const wchar_t* path, size_t* out_size) {
    FILE* fp;
    long len;
    unsigned char* data;
    size_t got;
    if (out_size) *out_size = 0;
    fp = _wfopen(path, L"rb");
    if (!fp) return NULL;
    if (fseek(fp, 0, SEEK_END) != 0) { fclose(fp); return NULL; }
    len = ftell(fp);
    if (len < 0 || fseek(fp, 0, SEEK_SET) != 0) { fclose(fp); return NULL; }
    data = (unsigned char*)malloc((size_t)len + 1u);
    if (!data) { fclose(fp); return NULL; }
    got = fread(data, 1, (size_t)len, fp);
    fclose(fp);
    if (got != (size_t)len) { free(data); return NULL; }
    data[len] = 0;
    if (out_size) *out_size = (size_t)len;
    return data;
}

static int luna_win_is_absolute_utf8(const char* path) {
    return path && (path[0] == '/' || path[0] == '\\' ||
                    (path[0] && path[1] == ':'));
}

static unsigned char* luna_win_read_resource(const char* path, size_t* out_size) {
    wchar_t* wide = NULL;
    unsigned char* data = NULL;
    if (!path || !path[0]) return NULL;
    wide = luna_win_utf8_to_wide(path);
    if (wide) {
        data = luna_win_read_wide_file(wide, out_size);
        free(wide);
        if (data) return data;
    }
    if (!luna_win_is_absolute_utf8(path)) {
        wchar_t exe[MAX_PATH];
        DWORD n = GetModuleFileNameW(NULL, exe, MAX_PATH);
        if (n > 0 && n < MAX_PATH) {
            wchar_t* slash = wcsrchr(exe, L'\\');
            if (slash) {
                wchar_t* rel = luna_win_utf8_to_wide(path);
                if (rel) {
                    *slash = 0;
                    if (wcslen(exe) + 1 + wcslen(rel) + 1 < MAX_PATH) {
                        wcscat(exe, L"\\");
                        wcscat(exe, rel);
                        data = luna_win_read_wide_file(exe, out_size);
                    }
                    free(rel);
                }
            }
        }
    }
    return data;
}

static unsigned char* luna_win_load_font(int role, size_t* out_size) {
    wchar_t windows_dir[MAX_PATH];
    const wchar_t* names[8] = {0};
    int count = 0;
    if (GetWindowsDirectoryW(windows_dir, MAX_PATH) == 0) return NULL;
    switch (role) {
        case LUNA_FONT_REGULAR:
            names[count++] = L"segoeui.ttf"; names[count++] = L"arial.ttf"; break;
        case LUNA_FONT_BOLD:
            names[count++] = L"segoeuib.ttf"; names[count++] = L"arialbd.ttf"; break;
        case LUNA_FONT_CJK:
            names[count++] = L"YuGothR.ttc"; names[count++] = L"meiryo.ttc";
            names[count++] = L"msgothic.ttc"; break;
        case LUNA_FONT_MONO:
            names[count++] = L"consola.ttf"; names[count++] = L"cour.ttf"; break;
        case LUNA_FONT_SYMBOLS: {
            static const char* const paths[] = {
                "fonts/LunaSymbols-Solid.otf", "ui/fonts/LunaSymbols-Solid.otf",
                "skins/fonts/LunaSymbols-Solid.otf"
            };
            for (size_t i = 0; i < sizeof(paths)/sizeof(paths[0]); ++i) {
                unsigned char* p = luna_win_read_resource(paths[i], out_size);
                if (p) return p;
            }
            return NULL;
        }
        case LUNA_FONT_BRANDS: {
            static const char* const paths[] = {
                "fonts/LunaSymbols-Brands.otf", "ui/fonts/LunaSymbols-Brands.otf",
                "skins/fonts/LunaSymbols-Brands.otf"
            };
            for (size_t i = 0; i < sizeof(paths)/sizeof(paths[0]); ++i) {
                unsigned char* p = luna_win_read_resource(paths[i], out_size);
                if (p) return p;
            }
            return NULL;
        }
        default: return NULL;
    }
    for (int i = 0; i < count; ++i) {
        wchar_t path[MAX_PATH];
        _snwprintf(path, MAX_PATH - 1, L"%ls\\Fonts\\%ls", windows_dir, names[i]);
        path[MAX_PATH - 1] = 0;
        unsigned char* p = luna_win_read_wide_file(path, out_size);
        if (p) return p;
    }
    return NULL;
}

static double luna_win_time(void) {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    return (double)now.QuadPart / (double)luna_win.qpc_frequency.QuadPart;
}

static void* luna_win_get_proc(const char* name) {
    void* p = (void*)wglGetProcAddress(name);
    if (p == NULL || p == (void*)1 || p == (void*)2 || p == (void*)3 ||
        p == (void*)(intptr_t)-1) {
        if (!luna_win.opengl32) luna_win.opengl32 = LoadLibraryW(L"opengl32.dll");
        if (luna_win.opengl32) p = (void*)GetProcAddress(luna_win.opengl32, name);
    }
    return p;
}

static int luna_win_mods(void) {
    int mods = 0;
    if (GetKeyState(VK_SHIFT) & 0x8000) mods |= LUNA_MOD_SHIFT;
    if (GetKeyState(VK_CONTROL) & 0x8000) mods |= LUNA_MOD_CONTROL;
    if (GetKeyState(VK_MENU) & 0x8000) mods |= LUNA_MOD_ALT;
    if ((GetKeyState(VK_LWIN) | GetKeyState(VK_RWIN)) & 0x8000) mods |= LUNA_MOD_SUPER;
    return mods;
}

static int luna_win_key(WPARAM vk) {
    switch (vk) {
        case VK_SPACE: return LUNA_KEY_SPACE;
        case VK_ESCAPE: return LUNA_KEY_ESCAPE;
        case VK_RETURN: return LUNA_KEY_ENTER;
        case VK_TAB: return LUNA_KEY_TAB;
        case VK_BACK: return LUNA_KEY_BACKSPACE;
        case VK_DELETE: return LUNA_KEY_DELETE;
        case VK_RIGHT: return LUNA_KEY_RIGHT;
        case VK_LEFT: return LUNA_KEY_LEFT;
        case VK_DOWN: return LUNA_KEY_DOWN;
        case VK_UP: return LUNA_KEY_UP;
        case VK_PRIOR: return LUNA_KEY_PAGE_UP;
        case VK_NEXT: return LUNA_KEY_PAGE_DOWN;
        case VK_HOME: return LUNA_KEY_HOME;
        case VK_END: return LUNA_KEY_END;
        case VK_F12: return LUNA_KEY_F12;
        default:
            if (vk >= 'A' && vk <= 'Z') return (int)vk;
            if (vk >= '0' && vk <= '9') return (int)vk;
            return (int)vk;
    }
}

static void luna_win_emit_utf16(wchar_t wc) {
    uint32_t cp;
    if (wc >= 0xD800 && wc <= 0xDBFF) {
        luna_win.pending_high_surrogate = wc;
        return;
    }
    if (wc >= 0xDC00 && wc <= 0xDFFF && luna_win.pending_high_surrogate) {
        cp = 0x10000u + (((uint32_t)luna_win.pending_high_surrogate - 0xD800u) << 10)
                     + ((uint32_t)wc - 0xDC00u);
        luna_win.pending_high_surrogate = 0;
    } else {
        luna_win.pending_high_surrogate = 0;
        cp = (uint32_t)wc;
    }
    luna_char(cp);
}

static void luna_win_set_cursor(int cursor_type) {
    LPCTSTR id = IDC_ARROW;
    switch (cursor_type) {
        case 1: id = IDC_HAND; break;
        case 2: id = IDC_IBEAM; break;
        case 3: id = IDC_CROSS; break;
        case 4: id = IDC_SIZEWE; break;
        case 5: id = IDC_SIZENS; break;
    }
    SetCursor(LoadCursor(NULL, id));
}

static void luna_win_close(void) {
    if (luna_win.hwnd) PostMessageW(luna_win.hwnd, WM_CLOSE, 0, 0);
}

static void luna_win_iconify(void) {
    if (luna_win.hwnd) ShowWindow(luna_win.hwnd, SW_MINIMIZE);
}

static void luna_win_maximize_toggle(void) {
    if (!luna_win.hwnd) return;
    ShowWindow(luna_win.hwnd, IsZoomed(luna_win.hwnd) ? SW_RESTORE : SW_MAXIMIZE);
}

static void luna_win_begin_move(void) {
    if (!luna_win.hwnd) return;
    ReleaseCapture();
    SendMessageW(luna_win.hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
}

static void luna_win_begin_resize(int edge) {
    WPARAM hit = 0;
    if (!luna_win.hwnd) return;
    switch (edge) {
        case LUNA_RESIZE_EDGE_LEFT: hit = HTLEFT; break;
        case LUNA_RESIZE_EDGE_RIGHT: hit = HTRIGHT; break;
        case LUNA_RESIZE_EDGE_TOP: hit = HTTOP; break;
        case LUNA_RESIZE_EDGE_BOTTOM: hit = HTBOTTOM; break;
        case LUNA_RESIZE_EDGE_TOP_LEFT: hit = HTTOPLEFT; break;
        case LUNA_RESIZE_EDGE_TOP_RIGHT: hit = HTTOPRIGHT; break;
        case LUNA_RESIZE_EDGE_BOTTOM_LEFT: hit = HTBOTTOMLEFT; break;
        case LUNA_RESIZE_EDGE_BOTTOM_RIGHT: hit = HTBOTTOMRIGHT; break;
        default: return;
    }
    ReleaseCapture();
    SendMessageW(luna_win.hwnd, WM_NCLBUTTONDOWN, hit, 0);
}

static void luna_win_set_title(const char* title) {
    wchar_t* w = luna_win_utf8_to_wide(title ? title : "");
    if (w && luna_win.hwnd) SetWindowTextW(luna_win.hwnd, w);
    free(w);
}

static int luna_win_system_notify(const char* app_name, int kind,
                                  const char* title, const char* message) {
    NOTIFYICONDATAW data;
    wchar_t* wt = luna_win_utf8_to_wide(title ? title : "");
    wchar_t* wm = luna_win_utf8_to_wide(message ? message : "");
    wchar_t* wa = luna_win_utf8_to_wide(app_name ? app_name : "Luna");
    if (!luna_win.hwnd || !wt || !wm || !wa) { free(wt); free(wm); free(wa); return 0; }
    memset(&data, 0, sizeof(data));
    data.cbSize = sizeof(data);
    data.hWnd = luna_win.hwnd;
    data.uID = 1;
    data.uFlags = NIF_ICON | NIF_TIP | NIF_INFO;
    data.hIcon = luna_win.options.icon ? luna_win.options.icon : LoadIcon(NULL, IDI_APPLICATION);
    wcsncpy(data.szTip, wa, sizeof(data.szTip)/sizeof(data.szTip[0]) - 1);
    wcsncpy(data.szInfoTitle, wt, sizeof(data.szInfoTitle)/sizeof(data.szInfoTitle[0]) - 1);
    wcsncpy(data.szInfo, wm, sizeof(data.szInfo)/sizeof(data.szInfo[0]) - 1);
    data.dwInfoFlags = kind == LUNA_NOTIFY_ERROR ? NIIF_ERROR :
                       kind == LUNA_NOTIFY_WARNING ? NIIF_WARNING : NIIF_INFO;
    if (!luna_win.notify_icon_added) {
        luna_win.notify_icon_added = Shell_NotifyIconW(NIM_ADD, &data) ? 1 : 0;
    }
    if (luna_win.notify_icon_added) Shell_NotifyIconW(NIM_MODIFY, &data);
    free(wt); free(wm); free(wa);
    return luna_win.notify_icon_added;
}

static void luna_win_request_redraw_impl(void) {
    luna_win.redraw = 1;
    if (luna_win.hwnd) InvalidateRect(luna_win.hwnd, NULL, FALSE);
}

static void luna_win_set_clipboard(const char* utf8) {
    wchar_t* w = luna_win_utf8_to_wide(utf8 ? utf8 : "");
    if (!w) return;
    if (OpenClipboard(luna_win.hwnd)) {
        size_t bytes = (wcslen(w) + 1) * sizeof(wchar_t);
        HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (mem) {
            void* dst = GlobalLock(mem);
            if (dst) { memcpy(dst, w, bytes); GlobalUnlock(mem); }
            EmptyClipboard();
            if (!SetClipboardData(CF_UNICODETEXT, mem)) GlobalFree(mem);
        }
        CloseClipboard();
    }
    free(w);
}

static char* luna_win_get_clipboard(void) {
    char* out = NULL;
    if (OpenClipboard(luna_win.hwnd)) {
        HANDLE h = GetClipboardData(CF_UNICODETEXT);
        if (h) {
            const wchar_t* w = (const wchar_t*)GlobalLock(h);
            if (w) { out = luna_win_wide_to_utf8(w); GlobalUnlock(h); }
        }
        CloseClipboard();
    }
    return out;
}

static void luna_win_text_input(int enabled, float x, float y, float w, float h) {
    HIMC imc;
    (void)w; (void)h;
    if (!luna_win.hwnd) return;
    imc = ImmGetContext(luna_win.hwnd);
    if (imc) {
        ImmSetOpenStatus(imc, enabled ? TRUE : FALSE);
        if (enabled) {
            COMPOSITIONFORM cf;
            memset(&cf, 0, sizeof(cf));
            cf.dwStyle = CFS_POINT;
            cf.ptCurrentPos.x = (LONG)x;
            cf.ptCurrentPos.y = (LONG)y;
            ImmSetCompositionWindow(imc, &cf);
        }
        ImmReleaseContext(luna_win.hwnd, imc);
    }
}

static float luna_win_scale(void) {
    if (!luna_win.hwnd) return 1.0f;
    HMODULE user32 = GetModuleHandleW(L"user32.dll");
    typedef UINT (WINAPI *GetDpiForWindowFn)(HWND);
    GetDpiForWindowFn fn = user32 ? (GetDpiForWindowFn)GetProcAddress(user32, "GetDpiForWindow") : NULL;
    return fn ? (float)fn(luna_win.hwnd) / 96.0f : 1.0f;
}

#ifdef WM_POINTERDOWN
static int luna_win_pointer_event(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    UINT32 id = GET_POINTERID_WPARAM(wp);
    POINTER_INPUT_TYPE pointer_type;
    LunaTouchEvent event;
    POINT point;
    if (!GetPointerType(id, &pointer_type) ||
        (pointer_type != PT_TOUCH && pointer_type != PT_PEN)) return 0;
    memset(&event, 0, sizeof(event));
    event.id = (int64_t)id;
    event.phase = msg == WM_POINTERDOWN ? LUNA_TOUCH_DOWN :
                  msg == WM_POINTERUP ? LUNA_TOUCH_UP :
                  msg == WM_POINTERCAPTURECHANGED ? LUNA_TOUCH_CANCEL : LUNA_TOUCH_MOVE;
    event.tool = pointer_type == PT_PEN ? LUNA_TOUCH_TOOL_STYLUS
                                        : LUNA_TOUCH_TOOL_FINGER;
    point.x = GET_X_LPARAM(lp); point.y = GET_Y_LPARAM(lp);
    ScreenToClient(hwnd, &point);
    event.x = point.x; event.y = point.y;
    event.pressure = event.phase == LUNA_TOUCH_UP || event.phase == LUNA_TOUCH_CANCEL
                         ? 0.0f : 1.0f;
    if (pointer_type == PT_TOUCH) {
        POINTER_TOUCH_INFO info;
        if (GetPointerTouchInfo(id, &info)) {
            POINT a = {info.rcContact.left, info.rcContact.top};
            POINT b = {info.rcContact.right, info.rcContact.bottom};
            ScreenToClient(hwnd, &a); ScreenToClient(hwnd, &b);
            event.radius_x = (float)abs(b.x - a.x) * 0.5f;
            event.radius_y = (float)abs(b.y - a.y) * 0.5f;
            if (info.touchMask & TOUCH_MASK_PRESSURE)
                event.pressure = (float)info.pressure / 1024.0f;
        }
    } else {
        POINTER_PEN_INFO info;
        if (GetPointerPenInfo(id, &info)) {
            if (info.penFlags & PEN_FLAG_ERASER) event.tool = LUNA_TOUCH_TOOL_ERASER;
            if (info.penMask & PEN_MASK_PRESSURE)
                event.pressure = (float)info.pressure / 1024.0f;
            if (info.penMask & PEN_MASK_TILT_X) event.tilt_x = (float)info.tiltX;
            if (info.penMask & PEN_MASK_TILT_Y) event.tilt_y = (float)info.tiltY;
        }
    }
    if (!luna_win.config.on_touch ||
        !luna_win.config.on_touch(&event, luna_win.config.userdata)) luna_touch(&event);
    luna_win.redraw = 1;
    return 1;
}
#endif

static LRESULT CALLBACK luna_win_wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
#ifdef WM_POINTERDOWN
        case WM_POINTERDOWN:
        case WM_POINTERUPDATE:
        case WM_POINTERUP:
        case WM_POINTERCAPTURECHANGED:
            if (luna_win_pointer_event(hwnd, msg, wp, lp)) return 0;
            break;
#endif
        case WM_CLOSE:
            luna_win.running = 0;
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            luna_win.running = 0;
            PostQuitMessage(0);
            return 0;
        case WM_ERASEBKGND:
            return 1;
        case WM_SIZE: {
            RECT r;
            GetClientRect(hwnd, &r);
            luna_resize((float)(r.right-r.left), (float)(r.bottom-r.top));
            luna_framebuffer_resized();
            luna_win.redraw = 1;
            return 0;
        }
        case WM_MOUSEMOVE:
            luna_mouse_move((double)GET_X_LPARAM(lp), (double)GET_Y_LPARAM(lp));
            luna_win.redraw = 1;
            return 0;
        case WM_LBUTTONDOWN: case WM_RBUTTONDOWN: case WM_MBUTTONDOWN: {
            int b = msg == WM_LBUTTONDOWN ? LUNA_MOUSE_BUTTON_LEFT :
                    msg == WM_RBUTTONDOWN ? LUNA_MOUSE_BUTTON_RIGHT : LUNA_MOUSE_BUTTON_MIDDLE;
            SetCapture(hwnd); luna_win.mouse_captured = 1;
            luna_mouse_button(b, LUNA_PRESS, luna_win_mods(),
                              GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            luna_win.redraw = 1;
            return 0;
        }
        case WM_LBUTTONUP: case WM_RBUTTONUP: case WM_MBUTTONUP: {
            int b = msg == WM_LBUTTONUP ? LUNA_MOUSE_BUTTON_LEFT :
                    msg == WM_RBUTTONUP ? LUNA_MOUSE_BUTTON_RIGHT : LUNA_MOUSE_BUTTON_MIDDLE;
            luna_mouse_button(b, LUNA_RELEASE, luna_win_mods(),
                              GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            if (luna_win.mouse_captured) { ReleaseCapture(); luna_win.mouse_captured = 0; }
            luna_win.redraw = 1;
            return 0;
        }
        case WM_MOUSEWHEEL:
            luna_scroll(0.0, (double)GET_WHEEL_DELTA_WPARAM(wp) / (double)WHEEL_DELTA);
            luna_win.redraw = 1;
            return 0;
        case WM_MOUSEHWHEEL:
            luna_scroll((double)GET_WHEEL_DELTA_WPARAM(wp) / (double)WHEEL_DELTA, 0.0);
            luna_win.redraw = 1;
            return 0;
        case WM_KEYDOWN: case WM_SYSKEYDOWN:
            luna_key(luna_win_key(wp), (int)((lp >> 16) & 0xff),
                     (lp & (1u << 30)) ? LUNA_REPEAT : LUNA_PRESS, luna_win_mods());
            luna_win.redraw = 1;
            return 0;
        case WM_KEYUP: case WM_SYSKEYUP:
            luna_key(luna_win_key(wp), (int)((lp >> 16) & 0xff),
                     LUNA_RELEASE, luna_win_mods());
            luna_win.redraw = 1;
            return 0;
        case WM_CHAR:
            luna_win_emit_utf16((wchar_t)wp);
            luna_win.redraw = 1;
            return 0;
        case WM_SETCURSOR:
            if (LOWORD(lp) == HTCLIENT) {
                int type = g_current_cursor >= 0 ? g_current_cursor : 0;
                LPCTSTR id = IDC_ARROW;
                switch (type) {
                    case 1: id = IDC_HAND; break; case 2: id = IDC_IBEAM; break;
                    case 3: id = IDC_CROSS; break; case 4: id = IDC_SIZEWE; break;
                    case 5: id = IDC_SIZENS; break;
                }
                SetCursor(LoadCursor(NULL, id));
                return TRUE;
            }
            break;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps); EndPaint(hwnd, &ps);
            luna_win.redraw = 1;
            return 0;
        }
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

void luna_windows_set_options(const LunaWindowsOptions* options) {
    if (options) luna_win_options = *options;
    else memset(&luna_win_options, 0, sizeof(luna_win_options));
}

HWND luna_windows_hwnd(void) { return luna_win.hwnd; }
void* luna_app_native_handle(void) { return (void*)luna_win.hwnd; }
void luna_app_quit(void) { luna_win_close(); }
void luna_app_request_redraw(void) { luna_win_request_redraw_impl(); }

static int luna_win_create_gl(const LunaAppConfig* cfg) {
    PIXELFORMATDESCRIPTOR pfd;
    int pf;
    HGLRC temporary;
    LunaWglCreateContextAttribsARB create_attribs;
    memset(&pfd, 0, sizeof(pfd));
    pfd.nSize = sizeof(pfd);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = cfg->transparent ? 8 : 0;
    pfd.cDepthBits = 0;
    pfd.iLayerType = PFD_MAIN_PLANE;
    pf = ChoosePixelFormat(luna_win.dc, &pfd);
    if (!pf || !SetPixelFormat(luna_win.dc, pf, &pfd)) return 0;
    temporary = wglCreateContext(luna_win.dc);
    if (!temporary || !wglMakeCurrent(luna_win.dc, temporary)) return 0;
    create_attribs = (LunaWglCreateContextAttribsARB)wglGetProcAddress("wglCreateContextAttribsARB");
    if (create_attribs) {
        const int attrs[] = {
            WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
            WGL_CONTEXT_MINOR_VERSION_ARB, 3,
            WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
            0
        };
        luna_win.glrc = create_attribs(luna_win.dc, 0, attrs);
    }
    if (luna_win.glrc) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(temporary);
        if (!wglMakeCurrent(luna_win.dc, luna_win.glrc)) return 0;
    } else {
        luna_win.glrc = temporary;
    }
    return 1;
}

int luna_app_run(const LunaAppConfig* user_cfg) {
    WNDCLASSEXW wc;
    RECT wr;
    DWORD style, exstyle;
    wchar_t* title;
    const wchar_t* cls;
    MSG msg;
    double previous;
    LunaPlatform platform;
    LunaInitConfig init;
    LunaAppConfig cfg;
    HMODULE user32;
    typedef BOOL (WINAPI *SetProcessDpiAwarenessContextFn)(HANDLE);

    memset(&cfg, 0, sizeof(cfg));
    if (user_cfg) cfg = *user_cfg;
    if (!cfg.title) cfg.title = "Luna UI";
    if (cfg.width <= 0) cfg.width = 1024;
    if (cfg.height <= 0) cfg.height = 768;
    if (!user_cfg) { cfg.resizable = 1; cfg.vsync = 1; }

    memset(&luna_win, 0, sizeof(luna_win));
    luna_win.options = luna_win_options;
    luna_win.config = cfg;
    luna_win.instance = luna_win.options.instance ? luna_win.options.instance : GetModuleHandleW(NULL);
    QueryPerformanceFrequency(&luna_win.qpc_frequency);

    user32 = GetModuleHandleW(L"user32.dll");
    if (user32) {
        SetProcessDpiAwarenessContextFn dpi =
            (SetProcessDpiAwarenessContextFn)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
        if (dpi) dpi((HANDLE)-4); /* DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 */
    }

    cls = luna_win.options.class_name ? luna_win.options.class_name : L"LunaUIWindow";
    memset(&wc, 0, sizeof(wc));
    wc.cbSize = sizeof(wc);
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = luna_win_wndproc;
    wc.hInstance = luna_win.instance;
    wc.hIcon = luna_win.options.icon;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.lpszClassName = cls;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return 1;

    style = cfg.frameless ? WS_POPUP : WS_OVERLAPPEDWINDOW;
    if (!cfg.resizable && !cfg.frameless)
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    exstyle = cfg.transparent ? WS_EX_LAYERED : 0;
    wr.left = wr.top = 0; wr.right = cfg.width; wr.bottom = cfg.height;
    AdjustWindowRectEx(&wr, style, FALSE, exstyle);
    title = luna_win_utf8_to_wide(cfg.title);
    luna_win.hwnd = CreateWindowExW(exstyle, cls, title ? title : L"Luna UI", style,
                                    CW_USEDEFAULT, CW_USEDEFAULT,
                                    wr.right - wr.left, wr.bottom - wr.top,
                                    NULL, NULL, luna_win.instance, NULL);
    free(title);
    if (!luna_win.hwnd) return 1;
    luna_win.dc = GetDC(luna_win.hwnd);
    if (!luna_win.dc || !luna_win_create_gl(&cfg)) return 1;

    {
        LunaWglSwapIntervalEXT swap_interval =
            (LunaWglSwapIntervalEXT)luna_win_get_proc("wglSwapIntervalEXT");
        if (swap_interval) swap_interval(cfg.vsync ? 1 : 0);
    }

    memset(&platform, 0, sizeof(platform));
    platform.struct_size = sizeof(platform);
    platform.api_version = LUNA_UI_API_VERSION;
    platform.get_time = luna_win_time;
    platform.get_proc = luna_win_get_proc;
    platform.set_cursor = luna_win_set_cursor;
    platform.request_close = luna_win_close;
    platform.iconify = luna_win_iconify;
    platform.maximize_toggle = luna_win_maximize_toggle;
    platform.request_redraw = luna_win_request_redraw_impl;
    platform.read_resource = luna_win_read_resource;
    platform.load_font = luna_win_load_font;
    platform.set_clipboard = luna_win_set_clipboard;
    platform.get_clipboard = luna_win_get_clipboard;
    platform.text_input = luna_win_text_input;
    platform.get_scale = luna_win_scale;
    platform.begin_move = luna_win_begin_move;
    platform.begin_resize = luna_win_begin_resize;
    platform.set_title = luna_win_set_title;
    platform.system_notify = luna_win_system_notify;
    luna_set_platform(&platform);

    memset(&init, 0, sizeof(init));
    init.width = (float)cfg.width;
    init.height = (float)cfg.height;
    init.get_proc = luna_win_get_proc;
    init.frameless = cfg.frameless;
    if (!luna_init(&init)) return 1;

    if (cfg.html) luna_parse_html(cfg.html);
    else if (cfg.html_path && !luna_load_html_file(cfg.html_path))
        fprintf(stderr, "[luna-ui] could not load HTML: %s\n", cfg.html_path);
    if (cfg.css) luna_parse_css(cfg.css);
    else if (cfg.css_path && !luna_load_css_file(cfg.css_path))
        fprintf(stderr, "[luna-ui] could not load CSS: %s\n", cfg.css_path);
    luna_inject_body_background();
    if (cfg.on_init) cfg.on_init(cfg.userdata);
    luna_wire_onclick_handlers();

    ShowWindow(luna_win.hwnd, SW_SHOW);
    UpdateWindow(luna_win.hwnd);
    luna_win.running = 1;
    luna_win.redraw = 1;
    previous = luna_win_time();

    while (luna_win.running) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) luna_win.running = 0;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        if (!luna_win.running) break;
        {
            double now = luna_win_time();
            double dt = now - previous;
            RECT r;
            previous = now;
            if (dt < 0.0 || dt > 0.25) dt = 1.0 / 60.0;
            if (cfg.on_frame) cfg.on_frame(dt, cfg.userdata);
            luna_update(now, dt);
            GetClientRect(luna_win.hwnd, &r);
            if (r.right > r.left && r.bottom > r.top) {
                luna_render(r.right-r.left, r.bottom-r.top);
                if (cfg.on_render)
                    cfg.on_render(r.right-r.left, r.bottom-r.top, cfg.userdata);
                SwapBuffers(luna_win.dc);
            }
        }
        if (!cfg.vsync) Sleep(1);
    }

    if (cfg.on_shutdown) cfg.on_shutdown(cfg.userdata);
    if (luna_win.notify_icon_added && luna_win.hwnd) {
        NOTIFYICONDATAW data; memset(&data, 0, sizeof(data));
        data.cbSize = sizeof(data); data.hWnd = luna_win.hwnd; data.uID = 1;
        Shell_NotifyIconW(NIM_DELETE, &data);
        luna_win.notify_icon_added = 0;
    }
    luna_shutdown();
    if (luna_win.glrc) { wglMakeCurrent(NULL, NULL); wglDeleteContext(luna_win.glrc); }
    if (luna_win.dc && luna_win.hwnd) ReleaseDC(luna_win.hwnd, luna_win.dc);
    if (luna_win.hwnd && IsWindow(luna_win.hwnd)) DestroyWindow(luna_win.hwnd);
    if (luna_win.opengl32) FreeLibrary(luna_win.opengl32);
    return 0;
}

#endif /* LUNA_UI_IMPLEMENTATION */
#endif /* LUNA_UI_PLATFORM_BODY */
