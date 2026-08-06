# Integration checklist

1. Keep `luna-ui.h`, all `luna_*.h` files, and the original `stb_*` and
   `cssparser.h` dependencies in the same include tree.
2. Define `LUNA_UI_IMPLEMENTATION` in exactly one source file.
3. Compile that source as C11/C17 or C++17. On macOS/iOS compile it as Objective-C
   or Objective-C++.
4. Link the native libraries listed in `README.md`.
5. Put HTML, CSS, images, and bundled symbol fonts in the normal executable,
   app-bundle, APK-assets, or Emscripten virtual-filesystem location.
6. Run each backend on real hardware before distributing it. The renderer is
   shared, but windowing, IME, clipboard, DPI, and GPU-driver behavior are
   inherently platform-specific.

The public include remains:

```c
#define LUNA_UI_IMPLEMENTATION
#include "luna-ui.h"
```

No `luna_platform.h` is used. `luna-ui.h` selects the appropriate host only in
the implementation translation unit.
