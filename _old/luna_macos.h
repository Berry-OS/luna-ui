/* luna_macos.h - header-only Cocoa/OpenGL host for Luna UI.
 * Compile the implementation translation unit as Objective-C (.m) or
 * Objective-C++ (.mm). */

#if defined(LUNA_UI_PLATFORM_PRELUDE)
#ifndef LUNA_MACOS_PRELUDE_INCLUDED
#define LUNA_MACOS_PRELUDE_INCLUDED
#ifndef __OBJC__
#error "luna_macos.h requires Objective-C; compile this translation unit as .m or .mm"
#endif
#import <Cocoa/Cocoa.h>
#import <CoreText/CoreText.h>
#import <OpenGL/gl3.h>
#import <dlfcn.h>
#define LUNA_UI_PLATFORM_GL_INCLUDED 1
#endif
#endif /* LUNA_UI_PLATFORM_PRELUDE */

#if defined(LUNA_UI_PLATFORM_BODY)
#ifndef LUNA_MACOS_BODY_INCLUDED
#define LUNA_MACOS_BODY_INCLUDED
#ifdef __cplusplus
extern "C" {
#endif
NSWindow* luna_macos_window(void);
NSView* luna_macos_view(void);
#ifdef __cplusplus
}
#endif
#endif

#if defined(LUNA_UI_IMPLEMENTATION) && !defined(LUNA_MACOS_IMPLEMENTATION_INCLUDED)
#define LUNA_MACOS_IMPLEMENTATION_INCLUDED

@interface LunaMacView : NSOpenGLView <NSTextInputClient>
@property(nonatomic,strong) NSMutableAttributedString* lunaMarkedText;
@end

@interface LunaMacDelegate : NSObject <NSApplicationDelegate, NSWindowDelegate>
@end

typedef struct LunaMacState {
    NSWindow* window;
    LunaMacView* view;
    NSTimer* timer;
    LunaAppConfig config;
    double previous;
    int running;
    int mouse_down;
} LunaMacState;

static LunaMacState luna_mac;

static double luna_mac_time(void) {
    return [NSProcessInfo processInfo].systemUptime;
}

static void* luna_mac_get_proc(const char* name) {
    return dlsym(RTLD_DEFAULT, name);
}

static unsigned char* luna_mac_read_nsurl(NSURL* url, size_t* out_size) {
    NSData* data;
    unsigned char* p;
    if (out_size) *out_size = 0;
    if (!url) return NULL;
    data = [NSData dataWithContentsOfURL:url];
    if (!data) return NULL;
    p = (unsigned char*)malloc(data.length + 1u);
    if (!p) return NULL;
    memcpy(p, data.bytes, data.length);
    p[data.length] = 0;
    if (out_size) *out_size = data.length;
    return p;
}

static unsigned char* luna_mac_read_resource(const char* path, size_t* out_size) {
    NSString* s;
    NSURL* url;
    if (!path || !path[0]) return NULL;
    s = [NSString stringWithUTF8String:path];
    url = [NSURL fileURLWithPath:s];
    {
        unsigned char* p = luna_mac_read_nsurl(url, out_size);
        if (p) return p;
    }
    if (![s isAbsolutePath]) {
        NSString* ext = [s pathExtension];
        NSString* base = [s stringByDeletingPathExtension];
        url = [[NSBundle mainBundle] URLForResource:base
                                      withExtension:ext.length ? ext : nil];
        {
            unsigned char* p = luna_mac_read_nsurl(url, out_size);
            if (p) return p;
        }
        url = [NSURL fileURLWithPath:[[[NSBundle mainBundle] resourcePath]
                                      stringByAppendingPathComponent:s]];
        return luna_mac_read_nsurl(url, out_size);
    }
    return NULL;
}

static unsigned char* luna_mac_font_by_name(CFStringRef name, size_t* out_size) {
    CTFontRef font = CTFontCreateWithName(name, 16.0, NULL);
    CFURLRef url;
    unsigned char* p = NULL;
    if (!font) return NULL;
    url = (CFURLRef)CTFontCopyAttribute(font, kCTFontURLAttribute);
    if (url) {
        p = luna_mac_read_nsurl((__bridge NSURL*)url, out_size);
        CFRelease(url);
    }
    CFRelease(font);
    return p;
}

static unsigned char* luna_mac_load_font(int role, size_t* out_size) {
    switch (role) {
        case LUNA_FONT_REGULAR:
            return luna_mac_font_by_name(CFSTR("Helvetica Neue"), out_size);
        case LUNA_FONT_BOLD:
            return luna_mac_font_by_name(CFSTR("Helvetica Neue Bold"), out_size);
        case LUNA_FONT_CJK: {
            unsigned char* p = luna_mac_font_by_name(CFSTR("Hiragino Sans"), out_size);
            if (!p) p = luna_mac_font_by_name(CFSTR("PingFang SC"), out_size);
            return p;
        }
        case LUNA_FONT_MONO:
            return luna_mac_font_by_name(CFSTR("Menlo"), out_size);
        case LUNA_FONT_SYMBOLS:
            return luna_mac_read_resource("fonts/LunaSymbols-Solid.otf", out_size);
        case LUNA_FONT_BRANDS:
            return luna_mac_read_resource("fonts/LunaSymbols-Brands.otf", out_size);
        default: return NULL;
    }
}

static int luna_mac_mods(NSEventModifierFlags flags) {
    int mods = 0;
    if (flags & NSEventModifierFlagShift) mods |= LUNA_MOD_SHIFT;
    if (flags & NSEventModifierFlagControl) mods |= LUNA_MOD_CONTROL;
    if (flags & NSEventModifierFlagOption) mods |= LUNA_MOD_ALT;
    if (flags & NSEventModifierFlagCommand) mods |= LUNA_MOD_SUPER;
    return mods;
}

static int luna_mac_key(NSEvent* e) {
    switch (e.keyCode) {
        case 49: return LUNA_KEY_SPACE;
        case 53: return LUNA_KEY_ESCAPE;
        case 36: case 76: return LUNA_KEY_ENTER;
        case 48: return LUNA_KEY_TAB;
        case 51: return LUNA_KEY_BACKSPACE;
        case 117: return LUNA_KEY_DELETE;
        case 124: return LUNA_KEY_RIGHT;
        case 123: return LUNA_KEY_LEFT;
        case 125: return LUNA_KEY_DOWN;
        case 126: return LUNA_KEY_UP;
        case 116: return LUNA_KEY_PAGE_UP;
        case 121: return LUNA_KEY_PAGE_DOWN;
        case 115: return LUNA_KEY_HOME;
        case 119: return LUNA_KEY_END;
        case 111: return LUNA_KEY_F12;
        default: {
            NSString* chars = e.charactersIgnoringModifiers;
            if (chars.length) {
                unichar c = [chars characterAtIndex:0];
                if (c >= 'a' && c <= 'z') c = (unichar)(c - 'a' + 'A');
                return (int)c;
            }
            return (int)e.keyCode;
        }
    }
}

static void luna_mac_emit_string(NSString* text) {
    NSUInteger i = 0;
    while (i < text.length) {
        unichar a = [text characterAtIndex:i++];
        uint32_t cp = a;
        if (a >= 0xD800 && a <= 0xDBFF && i < text.length) {
            unichar b = [text characterAtIndex:i];
            if (b >= 0xDC00 && b <= 0xDFFF) {
                ++i;
                cp = 0x10000u + (((uint32_t)a - 0xD800u) << 10)
                               + ((uint32_t)b - 0xDC00u);
            }
        }
        luna_char(cp);
    }
}

static void luna_mac_set_cursor(int type) {
    NSCursor* cursor = [NSCursor arrowCursor];
    switch (type) {
        case 1: cursor = [NSCursor pointingHandCursor]; break;
        case 2: cursor = [NSCursor IBeamCursor]; break;
        case 3: cursor = [NSCursor crosshairCursor]; break;
        case 4: cursor = [NSCursor resizeLeftRightCursor]; break;
        case 5: cursor = [NSCursor resizeUpDownCursor]; break;
    }
    [cursor set];
}

static void luna_mac_close(void) {
    dispatch_async(dispatch_get_main_queue(), ^{ [luna_mac.window performClose:nil]; });
}
static void luna_mac_iconify(void) { [luna_mac.window miniaturize:nil]; }
static void luna_mac_maximize(void) { [luna_mac.window zoom:nil]; }
static void luna_mac_redraw(void) { [luna_mac.view setNeedsDisplay:YES]; }

static void luna_mac_set_clipboard(const char* utf8) {
    NSPasteboard* pb = [NSPasteboard generalPasteboard];
    [pb clearContents];
    [pb setString:[NSString stringWithUTF8String:utf8 ? utf8 : ""]
          forType:NSPasteboardTypeString];
}

static char* luna_mac_get_clipboard(void) {
    NSString* s = [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString];
    const char* u;
    char* p;
    size_t n;
    if (!s) return NULL;
    u = s.UTF8String;
    n = strlen(u) + 1;
    p = (char*)malloc(n);
    if (p) memcpy(p, u, n);
    return p;
}

static void luna_mac_text_input(int enabled, float x, float y, float w, float h) {
    (void)x; (void)y; (void)w; (void)h;
    /* The OpenGL view remains first responder for shortcuts and focus
     * navigation. Only the IME marked-text state is ended on blur. */
    [luna_mac.window makeFirstResponder:luna_mac.view];
    if (!enabled) [luna_mac.view unmarkText];
}

static float luna_mac_scale(void) {
    return luna_mac.window ? (float)luna_mac.window.backingScaleFactor : 1.0f;
}

@implementation LunaMacView

- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent*)event { (void)event; return YES; }
- (BOOL)isOpaque { return !luna_mac.config.transparent; }

- (NSPoint)lunaPoint:(NSEvent*)event {
    NSPoint p = [self convertPoint:event.locationInWindow fromView:nil];
    return NSMakePoint(p.x, self.bounds.size.height - p.y);
}

- (void)reshape {
    [super reshape];
    NSRect b = [self convertRectToBacking:self.bounds];
    luna_resize((float)self.bounds.size.width, (float)self.bounds.size.height);
    luna_framebuffer_resized();
    (void)b;
}

- (void)mouseMoved:(NSEvent*)e { NSPoint p=[self lunaPoint:e]; luna_mouse_move(p.x,p.y); }
- (void)mouseDragged:(NSEvent*)e { [self mouseMoved:e]; }
- (void)rightMouseDragged:(NSEvent*)e { [self mouseMoved:e]; }
- (void)otherMouseDragged:(NSEvent*)e { [self mouseMoved:e]; }
- (void)mouseDown:(NSEvent*)e { NSPoint p=[self lunaPoint:e]; luna_mouse_button(LUNA_MOUSE_BUTTON_LEFT,LUNA_PRESS,luna_mac_mods(e.modifierFlags),p.x,p.y); }
- (void)mouseUp:(NSEvent*)e { NSPoint p=[self lunaPoint:e]; luna_mouse_button(LUNA_MOUSE_BUTTON_LEFT,LUNA_RELEASE,luna_mac_mods(e.modifierFlags),p.x,p.y); }
- (void)rightMouseDown:(NSEvent*)e { NSPoint p=[self lunaPoint:e]; luna_mouse_button(LUNA_MOUSE_BUTTON_RIGHT,LUNA_PRESS,luna_mac_mods(e.modifierFlags),p.x,p.y); }
- (void)rightMouseUp:(NSEvent*)e { NSPoint p=[self lunaPoint:e]; luna_mouse_button(LUNA_MOUSE_BUTTON_RIGHT,LUNA_RELEASE,luna_mac_mods(e.modifierFlags),p.x,p.y); }
- (void)otherMouseDown:(NSEvent*)e { NSPoint p=[self lunaPoint:e]; luna_mouse_button(LUNA_MOUSE_BUTTON_MIDDLE,LUNA_PRESS,luna_mac_mods(e.modifierFlags),p.x,p.y); }
- (void)otherMouseUp:(NSEvent*)e { NSPoint p=[self lunaPoint:e]; luna_mouse_button(LUNA_MOUSE_BUTTON_MIDDLE,LUNA_RELEASE,luna_mac_mods(e.modifierFlags),p.x,p.y); }
- (void)scrollWheel:(NSEvent*)e { luna_scroll(e.scrollingDeltaX / 10.0, e.scrollingDeltaY / 10.0); }

- (void)keyDown:(NSEvent*)e {
    luna_key(luna_mac_key(e), (int)e.keyCode, e.isARepeat ? LUNA_REPEAT : LUNA_PRESS,
             luna_mac_mods(e.modifierFlags));
    [self interpretKeyEvents:@[e]];
}
- (void)keyUp:(NSEvent*)e {
    luna_key(luna_mac_key(e), (int)e.keyCode, LUNA_RELEASE,
             luna_mac_mods(e.modifierFlags));
}

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange {
    (void)replacementRange;
    NSString* s = [string isKindOfClass:[NSAttributedString class]] ?
                  [(NSAttributedString*)string string] : (NSString*)string;
    luna_mac_emit_string(s);
    self.lunaMarkedText = nil;
}
- (void)setMarkedText:(id)string selectedRange:(NSRange)selectedRange replacementRange:(NSRange)replacementRange {
    (void)selectedRange; (void)replacementRange;
    NSAttributedString* a = [string isKindOfClass:[NSAttributedString class]] ? string :
        [[NSAttributedString alloc] initWithString:(NSString*)string];
    self.lunaMarkedText = [[NSMutableAttributedString alloc] initWithAttributedString:a];
}
- (void)unmarkText { self.lunaMarkedText = nil; }
- (BOOL)hasMarkedText { return self.lunaMarkedText.length != 0; }
- (NSRange)markedRange { return self.hasMarkedText ? NSMakeRange(0,self.lunaMarkedText.length) : NSMakeRange(NSNotFound,0); }
- (NSRange)selectedRange { return NSMakeRange(0,0); }
- (NSArray<NSAttributedStringKey>*)validAttributesForMarkedText { return @[]; }
- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range actualRange:(NSRangePointer)actualRange { (void)range; if(actualRange)*actualRange=NSMakeRange(NSNotFound,0); return nil; }
- (NSUInteger)characterIndexForPoint:(NSPoint)point { (void)point; return 0; }
- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actualRange {
    (void)range; if(actualRange)*actualRange=NSMakeRange(0,0);
    NSRect r = [self.window convertRectToScreen:[self convertRect:self.bounds toView:nil]];
    r.size = NSMakeSize(1,20); return r;
}
- (void)doCommandBySelector:(SEL)selector { (void)selector; }

@end

static void luna_mac_frame(NSTimer* timer) {
    (void)timer;
    if (!luna_mac.running || !luna_mac.view) return;
    @autoreleasepool {
        double now = luna_mac_time();
        double dt = now - luna_mac.previous;
        NSRect backing;
        luna_mac.previous = now;
        if (dt < 0.0 || dt > 0.25) dt = 1.0/60.0;
        if (luna_mac.config.on_frame)
            luna_mac.config.on_frame(dt, luna_mac.config.userdata);
        luna_update(now, dt);
        [[luna_mac.view openGLContext] makeCurrentContext];
        backing = [luna_mac.view convertRectToBacking:luna_mac.view.bounds];
        if (backing.size.width > 0 && backing.size.height > 0) {
            luna_render((int)backing.size.width, (int)backing.size.height);
            [[luna_mac.view openGLContext] flushBuffer];
        }
    }
}

@implementation LunaMacDelegate
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication*)sender { (void)sender; return YES; }
- (void)windowWillClose:(NSNotification*)notification { (void)notification; luna_mac.running = 0; [NSApp terminate:nil]; }
@end

NSWindow* luna_macos_window(void) { return luna_mac.window; }
NSView* luna_macos_view(void) { return luna_mac.view; }
void* luna_app_native_handle(void) { return (__bridge void*)luna_mac.window; }
void luna_app_quit(void) { luna_mac_close(); }
void luna_app_request_redraw(void) { luna_mac_redraw(); }

int luna_app_run(const LunaAppConfig* user_cfg) {
    LunaAppConfig cfg;
    LunaMacDelegate* delegate;
    NSOpenGLPixelFormatAttribute attrs[] = {
        NSOpenGLPFAOpenGLProfile, NSOpenGLProfileVersion4_1Core,
        NSOpenGLPFAColorSize, 24,
        NSOpenGLPFAAlphaSize, 8,
        NSOpenGLPFADoubleBuffer,
        NSOpenGLPFAAccelerated,
        0
    };
    NSOpenGLPixelFormat* format;
    NSUInteger style;
    LunaPlatform platform;
    LunaInitConfig init;

    memset(&cfg,0,sizeof(cfg));
    if (user_cfg) cfg=*user_cfg;
    if (!cfg.title) cfg.title="Luna UI";
    if (cfg.width<=0) cfg.width=1024;
    if (cfg.height<=0) cfg.height=768;
    if (!user_cfg) { cfg.resizable=1; cfg.vsync=1; }
    memset(&luna_mac,0,sizeof(luna_mac));
    luna_mac.config=cfg;

    [NSApplication sharedApplication];
    [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
    delegate=[LunaMacDelegate new];
    NSApp.delegate=delegate;

    style = cfg.frameless ? NSWindowStyleMaskBorderless :
        (NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskMiniaturizable |
         (cfg.resizable ? NSWindowStyleMaskResizable : 0));
    luna_mac.window=[[NSWindow alloc] initWithContentRect:NSMakeRect(0,0,cfg.width,cfg.height)
                                                styleMask:style
                                                  backing:NSBackingStoreBuffered
                                                    defer:NO];
    [luna_mac.window setTitle:[NSString stringWithUTF8String:cfg.title]];
    [luna_mac.window center];
    [luna_mac.window setAcceptsMouseMovedEvents:YES];
    if (cfg.transparent) { luna_mac.window.opaque=NO; luna_mac.window.backgroundColor=[NSColor clearColor]; }
    luna_mac.window.delegate=delegate;

    format=[[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
    if (!format) return 1;
    luna_mac.view=[[LunaMacView alloc] initWithFrame:luna_mac.window.contentView.bounds pixelFormat:format];
    luna_mac.view.autoresizingMask=NSViewWidthSizable|NSViewHeightSizable;
    [luna_mac.window setContentView:luna_mac.view];
    [[luna_mac.view openGLContext] makeCurrentContext];
    { GLint swap=cfg.vsync?1:0; [[luna_mac.view openGLContext] setValues:&swap forParameter:NSOpenGLCPSwapInterval]; }

    memset(&platform,0,sizeof(platform));
    platform.struct_size=sizeof(platform);
    platform.api_version=LUNA_UI_API_VERSION;
    platform.get_time=luna_mac_time;
    platform.get_proc=luna_mac_get_proc;
    platform.set_cursor=luna_mac_set_cursor;
    platform.request_close=luna_mac_close;
    platform.iconify=luna_mac_iconify;
    platform.maximize_toggle=luna_mac_maximize;
    platform.request_redraw=luna_mac_redraw;
    platform.read_resource=luna_mac_read_resource;
    platform.load_font=luna_mac_load_font;
    platform.set_clipboard=luna_mac_set_clipboard;
    platform.get_clipboard=luna_mac_get_clipboard;
    platform.text_input=luna_mac_text_input;
    platform.get_scale=luna_mac_scale;
    luna_set_platform(&platform);

    memset(&init,0,sizeof(init));
    init.width=(float)cfg.width; init.height=(float)cfg.height;
    init.get_proc=luna_mac_get_proc; init.frameless=cfg.frameless;
    if (!luna_init(&init)) return 1;
    if (cfg.html) luna_parse_html(cfg.html); else if (cfg.html_path) luna_load_html_file(cfg.html_path);
    if (cfg.css) luna_parse_css(cfg.css); else if (cfg.css_path) luna_load_css_file(cfg.css_path);
    luna_inject_body_background();
    if (cfg.on_init) cfg.on_init(cfg.userdata);
    luna_wire_onclick_handlers();

    [luna_mac.window makeKeyAndOrderFront:nil];
    [luna_mac.window makeFirstResponder:luna_mac.view];
    [NSApp activateIgnoringOtherApps:YES];
    luna_mac.running=1;
    luna_mac.previous=luna_mac_time();
    luna_mac.timer=[NSTimer scheduledTimerWithTimeInterval:1.0/60.0 repeats:YES block:^(NSTimer* t){ luna_mac_frame(t); }];
    [NSApp run];

    [luna_mac.timer invalidate];
    if (cfg.on_shutdown) cfg.on_shutdown(cfg.userdata);
    [[luna_mac.view openGLContext] makeCurrentContext];
    luna_shutdown();
    return 0;
}

#endif /* implementation */
#endif /* platform body */
