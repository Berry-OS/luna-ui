**🌙 Luna UI**

Header-only multiplatform HTML/CSS → OpenGL UI host.

[![Sponsor](https://img.shields.io/badge/Sponsor%20this%20project-%E2%9D%A4%EF%B8%8F-white?logo=githubsponsors&logoColor=EA4AAA&labelColor=EA4AAA)](https://github.com/sponsors/yui0)

`luna-ui.h` is the only public include. It automatically selects one native host:

- `luna_windows.h` — Win32 + WGL/OpenGL 3.3
- `luna_linux.h` — X11 + GLX/OpenGL 3.3
- `luna_macos.h` — Cocoa + OpenGL 4.1
- `luna_ios.h` — UIKit + OpenGL ES 3
- `luna_android.h` — NativeActivity + EGL/OpenGL ES 3
- `luna_web.h` — Emscripten + WebGL 2

There is no `luna_platform.h`. Platform selection is performed inside `luna-ui.h`.

[![Sponsor](https://img.shields.io/badge/Sponsor%20this%20project-%E2%9D%A4%EF%B8%8F-white?logo=githubsponsors&logoColor=EA4AAA&labelColor=EA4AAA)](https://github.com/sponsors/yui0)

---

## 📦 Basic use

```c
#define LUNA_UI_IMPLEMENTATION
#include "luna-ui.h"

int main(void) {
    LunaAppConfig app = {0};
    app.title = "My Luna app";
    app.width = 1280;
    app.height = 720;
    app.resizable = 1;
    app.vsync = 1;
    app.html_path = "ui/main.html";
    app.css_path = "ui/main.css";
    return luna_app_run(&app);
}
```

Define `LUNA_UI_IMPLEMENTATION` in exactly one translation unit. Other C or C++ source files include `luna-ui.h` normally without loading any native SDK headers. On Apple targets, only the implementation translation unit must be compiled as `.m` or `.mm`; declaration-only includes remain ordinary C/C++.

The existing third-party dependencies from the original project remain adjacent to `luna-ui.h` and are not duplicated in this package:

- `stb_truetype.h`
- `stb_image.h`
- `stb_image_write.h`
- `cssparser.h`

---

## 🛠️ Platform services included

The selected `luna_*.h` host supplies the window/context lifecycle, event loop, mouse/touch and keyboard forwarding, resource loading, native font lookup, clipboard access, IME/text-input activation, cursor changes, DPI/display scale, window close/minimize/maximize, framebuffer resize handling, and swap timing.

Common helpers:

```c
void  luna_clipboard_set(const char* utf8);
char* luna_clipboard_get(void);
void  luna_clipboard_free(char* utf8);
float luna_platform_scale(void);
```

Text-field focus automatically activates the platform IME. The compact text editor currently has no range-selection model, so Ctrl/Cmd+C and X operate on the complete non-password value and V inserts at the caret.

---

## 🖥️ Desktop build

### Linux/X11

```sh
cc -std=c11 examples/example.c -o luna-example \
  -lX11 -lGL -ldl -lm
```

### Windows/MSVC

```bat
cl /std:c11 /O2 examples\example.c
```

The Windows header adds the required Win32/OpenGL libraries with MSVC `#pragma comment`. MinGW users should link `-lopengl32 -lgdi32 -luser32 -limm32 -lshell32`.

### macOS

Copy `examples/example.c` to `example.m`, or use an Objective-C entry file:

```sh
clang -O2 -x objective-c examples/example.c -o luna-example \
  -framework Cocoa -framework OpenGL -framework CoreText
```

---

## 🌐 Web

```sh
emcc examples/example.c -o examples/luna-example.js \
  -sUSE_WEBGL2=1 -sFULL_ES3=1 -sALLOW_MEMORY_GROWTH=1 \
  -sEXPORTED_FUNCTIONS=_main,_malloc,_free,_luna_web_commit_utf8 \
  -sEXPORTED_RUNTIME_METHODS=UTF8ToString,stringToUTF8,lengthBytesUTF8
```

Serve the `examples` directory over HTTP and open `index.html`.

---

## 📱 iOS

Add `examples/example_ios.m` and all Luna headers to an iOS application target.  
Link UIKit, QuartzCore, OpenGLES, and CoreText. The header supplies its own `UIApplicationDelegate`, view controller, `EAGLContext`, touch input, and `UIKeyInput` implementation.

---

## 🤖 Android

Use `android.app.NativeActivity` and set the shared library name in the Android manifest. The implementation exports `ANativeActivity_onCreate`.

The application config is supplied with a macro before including the header:

```c
#define LUNA_ANDROID_APP_CONFIG luna_android_make_config
#define LUNA_UI_IMPLEMENTATION
#include "luna-ui.h"

LunaAppConfig luna_android_make_config(void) {
    LunaAppConfig app = {0};
    app.html_path = "ui/main.html";
    app.css_path = "ui/main.css";
    return app;
}
```

Link `android`, `EGL`, `GLESv3`, `dl`, and `log`. HTML, CSS, images, and bundled fonts should be placed in the APK assets directory.

---

## 🔧 Custom host

To use the renderer inside an existing window system:

```c
#define LUNA_UI_NO_PLATFORM
#define LUNA_UI_IMPLEMENTATION
#include "luna-ui.h"
```

Provide a `LunaPlatform`, make a compatible GL context current, and call `luna_init`, the input forwarding functions, `luna_update`, and `luna_render` manually.

---

## ✅ Validation status

The generated core was syntax-checked as C11 and C++17. The Windows, Linux, Web, and Android hosts were also syntax-checked, including Android C and C++ JNI call paths and GLES headers that expose normal GL prototypes. Apple SDKs are not installed in the generation environment, so `luna_macos.h` and `luna_ios.h` still require a build and runtime pass in Xcode before release. Likewise, each native backend needs runtime testing on its actual OS/GPU/IME.

---

## 📝 Notes

The Linux native header currently targets X11/GLX. A Wayland host can still use the same core through `LUNA_UI_NO_PLATFORM`, and can later be added as another branch inside `luna_linux.h` without changing the public include.

macOS OpenGL and iOS OpenGL ES are legacy Apple APIs. They keep this version small and share the current renderer. A future Metal backend should replace only the rendering/host layer; the DOM, CSS, layout, input, and public API can remain unchanged.

---

## 📄 License

This project is licensed under the **Mozilla Public License 2.0** (MPL-2.0).

Copyright (c) Yuichiro Nakada, Berry OS / Luna Desktop contributors.

See the [LICENSE](LICENSE) file for the full text.

Third-party single-header libraries (`stb_*`) retain their original licenses.