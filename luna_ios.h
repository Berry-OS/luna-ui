/* luna_ios.h - header-only UIKit/OpenGL ES 3 host for Luna UI.
 * Compile the implementation translation unit as Objective-C (.m) or .mm. */

#if defined(LUNA_UI_PLATFORM_PRELUDE)
#ifndef LUNA_IOS_PRELUDE_INCLUDED
#define LUNA_IOS_PRELUDE_INCLUDED
#ifndef __OBJC__
#error "luna_ios.h requires Objective-C; compile this translation unit as .m or .mm"
#endif
#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import <CoreText/CoreText.h>
#import <OpenGLES/ES3/gl.h>
#import <OpenGLES/ES3/glext.h>
#import <dlfcn.h>
#define LUNA_UI_GLES3 1
#define LUNA_UI_PLATFORM_GL_INCLUDED 1
#endif
#endif

#if defined(LUNA_UI_PLATFORM_BODY)
#ifndef LUNA_IOS_BODY_INCLUDED
#define LUNA_IOS_BODY_INCLUDED
#ifdef __cplusplus
extern "C" {
#endif
UIWindow* luna_ios_window(void);
UIView* luna_ios_view(void);
#ifdef __cplusplus
}
#endif
#endif

#if defined(LUNA_UI_PLATFORM_BODY) && defined(LUNA_UI_IMPLEMENTATION) && !defined(LUNA_IOS_IMPLEMENTATION_INCLUDED)
#define LUNA_IOS_IMPLEMENTATION_INCLUDED

@interface LunaIOSView : UIView <UIKeyInput>
@property(nonatomic,strong) EAGLContext* context;
@property(nonatomic,strong) CADisplayLink* displayLink;
@property(nonatomic,assign) GLuint framebuffer;
@property(nonatomic,assign) GLuint colorRenderbuffer;
@property(nonatomic,assign) GLint framebufferWidth;
@property(nonatomic,assign) GLint framebufferHeight;
@property(nonatomic,assign) double previous;
@end

@interface LunaIOSController : UIViewController
@end

@interface LunaIOSDelegate : UIResponder <UIApplicationDelegate>
@property(nonatomic,strong) UIWindow* window;
@end

typedef struct LunaIOSState {
    LunaAppConfig config;
    UIWindow* window;
    LunaIOSView* view;
    int initialized;
} LunaIOSState;

static LunaIOSState luna_ios;

static double luna_ios_time_impl(void) { return CACurrentMediaTime(); }
static void* luna_ios_get_proc(const char* name) { return dlsym(RTLD_DEFAULT,name); }

static unsigned char* luna_ios_read_url(NSURL* url,size_t* out_size) {
    NSData* d; unsigned char* p;
    if(out_size)*out_size=0;
    if(!url)return NULL;
    d=[NSData dataWithContentsOfURL:url]; if(!d)return NULL;
    p=(unsigned char*)malloc(d.length+1u); if(!p)return NULL;
    memcpy(p,d.bytes,d.length); p[d.length]=0;
    if(out_size)*out_size=d.length; return p;
}

static unsigned char* luna_ios_read_resource(const char* path,size_t* out_size) {
    NSString* s; NSURL* u; unsigned char* p;
    if(!path||!path[0])return NULL;
    s=[NSString stringWithUTF8String:path];
    u=[NSURL fileURLWithPath:s]; p=luna_ios_read_url(u,out_size); if(p)return p;
    u=[NSURL fileURLWithPath:[[[NSBundle mainBundle] resourcePath] stringByAppendingPathComponent:s]];
    return luna_ios_read_url(u,out_size);
}

static unsigned char* luna_ios_font_name(CFStringRef name,size_t* out_size) {
    CTFontRef f=CTFontCreateWithName(name,16,NULL); CFURLRef u; unsigned char* p=NULL;
    if(!f)return NULL; u=(CFURLRef)CTFontCopyAttribute(f,kCTFontURLAttribute);
    if(u){p=luna_ios_read_url((__bridge NSURL*)u,out_size);CFRelease(u);} CFRelease(f); return p;
}

static unsigned char* luna_ios_load_font(int role,size_t* out_size) {
    switch(role){
        case LUNA_FONT_REGULAR:return luna_ios_font_name(CFSTR("Helvetica Neue"),out_size);
        case LUNA_FONT_BOLD:return luna_ios_font_name(CFSTR("Helvetica Neue Bold"),out_size);
        case LUNA_FONT_CJK:{unsigned char*p=luna_ios_font_name(CFSTR("Hiragino Sans"),out_size);if(!p)p=luna_ios_font_name(CFSTR("PingFang SC"),out_size);return p;}
        case LUNA_FONT_MONO:return luna_ios_font_name(CFSTR("Menlo"),out_size);
        case LUNA_FONT_SYMBOLS:return luna_ios_read_resource("fonts/LunaSymbols-Solid.otf",out_size);
        case LUNA_FONT_BRANDS:return luna_ios_read_resource("fonts/LunaSymbols-Brands.otf",out_size);
        default:return NULL;
    }
}

static void luna_ios_close(void) { /* iOS applications do not programmatically terminate. */ }
static void luna_ios_redraw(void) { [luna_ios.view setNeedsDisplay]; }
static void luna_ios_set_cursor(int type){(void)type;}
static void luna_ios_iconify(void){}
static void luna_ios_maximize(void){}
static float luna_ios_scale(void){return (float)[UIScreen mainScreen].scale;}
static void luna_ios_set_clipboard(const char* text){[UIPasteboard generalPasteboard].string=[NSString stringWithUTF8String:text?text:""];}
static char* luna_ios_get_clipboard(void){NSString*s=[UIPasteboard generalPasteboard].string;if(!s)return NULL;const char*u=s.UTF8String;size_t n=strlen(u)+1;char*p=(char*)malloc(n);if(p)memcpy(p,u,n);return p;}
static void luna_ios_text_input(int enabled,float x,float y,float w,float h){(void)x;(void)y;(void)w;(void)h;if(enabled)[luna_ios.view becomeFirstResponder];else[luna_ios.view resignFirstResponder];}
static void luna_ios_begin_move(void){}
static void luna_ios_begin_resize(int edge){(void)edge;}
static void luna_ios_set_title(const char* title){
    UIViewController* c=luna_ios.window.rootViewController;
    if(c)c.title=[NSString stringWithUTF8String:title?title:""];
}
static int luna_ios_system_notify(const char* app_name,int kind,const char* title,const char* message){
    (void)app_name;(void)kind;(void)title;(void)message;return 0;
}

static int luna_ios_setup_core(LunaIOSView* view){
    LunaPlatform p; LunaInitConfig init; LunaAppConfig*cfg=&luna_ios.config;
    memset(&p,0,sizeof(p));p.struct_size=sizeof(p);p.api_version=LUNA_UI_API_VERSION;
    p.get_time=luna_ios_time_impl;p.get_proc=luna_ios_get_proc;p.set_cursor=luna_ios_set_cursor;
    p.request_close=luna_ios_close;p.iconify=luna_ios_iconify;p.maximize_toggle=luna_ios_maximize;
    p.request_redraw=luna_ios_redraw;p.read_resource=luna_ios_read_resource;p.load_font=luna_ios_load_font;
    p.set_clipboard=luna_ios_set_clipboard;p.get_clipboard=luna_ios_get_clipboard;p.text_input=luna_ios_text_input;p.get_scale=luna_ios_scale;
    p.begin_move=luna_ios_begin_move;p.begin_resize=luna_ios_begin_resize;p.set_title=luna_ios_set_title;p.system_notify=luna_ios_system_notify;
    luna_set_platform(&p);
    memset(&init,0,sizeof(init));init.width=(float)view.bounds.size.width;init.height=(float)view.bounds.size.height;init.get_proc=luna_ios_get_proc;init.frameless=1;
    if(!luna_init(&init))return 0;
    if(cfg->html)luna_parse_html(cfg->html);else if(cfg->html_path)luna_load_html_file(cfg->html_path);
    if(cfg->css)luna_parse_css(cfg->css);else if(cfg->css_path)luna_load_css_file(cfg->css_path);
    luna_inject_body_background();if(cfg->on_init)cfg->on_init(cfg->userdata);luna_wire_onclick_handlers();
    luna_ios.initialized=1;return 1;
}

@implementation LunaIOSView
+ (Class)layerClass{return[CAEAGLLayer class];}
- (instancetype)initWithFrame:(CGRect)frame{
    if((self=[super initWithFrame:frame])){
        self.contentScaleFactor=[UIScreen mainScreen].scale;self.multipleTouchEnabled=YES;
        CAEAGLLayer*l=(CAEAGLLayer*)self.layer;l.opaque=!luna_ios.config.transparent;l.drawableProperties=@{kEAGLDrawablePropertyRetainedBacking:@NO,kEAGLDrawablePropertyColorFormat:kEAGLColorFormatRGBA8};
        self.context=[[EAGLContext alloc]initWithAPI:kEAGLRenderingAPIOpenGLES3];if(!self.context)return nil;
        [EAGLContext setCurrentContext:self.context];glGenFramebuffers(1,&_framebuffer);glGenRenderbuffers(1,&_colorRenderbuffer);
        glBindFramebuffer(GL_FRAMEBUFFER,_framebuffer);glBindRenderbuffer(GL_RENDERBUFFER,_colorRenderbuffer);
        [self.context renderbufferStorage:GL_RENDERBUFFER fromDrawable:l];glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,_colorRenderbuffer);
        glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_WIDTH,&_framebufferWidth);glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_HEIGHT,&_framebufferHeight);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)return nil;
        if(!luna_ios_setup_core(self))return nil;
        _previous=luna_ios_time_impl();_displayLink=[CADisplayLink displayLinkWithTarget:self selector:@selector(frame:)];[_displayLink addToRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
    }return self;
}
- (void)layoutSubviews{
    [EAGLContext setCurrentContext:self.context];glBindRenderbuffer(GL_RENDERBUFFER,self.colorRenderbuffer);
    [self.context renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer*)self.layer];
    glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_WIDTH,&_framebufferWidth);glGetRenderbufferParameteriv(GL_RENDERBUFFER,GL_RENDERBUFFER_HEIGHT,&_framebufferHeight);
    glBindFramebuffer(GL_FRAMEBUFFER,self.framebuffer);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_RENDERBUFFER,self.colorRenderbuffer);
    luna_resize((float)self.bounds.size.width,(float)self.bounds.size.height);luna_framebuffer_resized();
}
- (void)frame:(CADisplayLink*)link{(void)link;double now=luna_ios_time_impl(),dt=now-self.previous;self.previous=now;if(dt<0||dt>.25)dt=1.0/60.0;if(luna_ios.config.on_frame)luna_ios.config.on_frame(dt,luna_ios.config.userdata);[EAGLContext setCurrentContext:self.context];glBindFramebuffer(GL_FRAMEBUFFER,self.framebuffer);luna_update(now,dt);luna_render(self.framebufferWidth,self.framebufferHeight);if(luna_ios.config.on_render)luna_ios.config.on_render(self.framebufferWidth,self.framebufferHeight,luna_ios.config.userdata);glBindRenderbuffer(GL_RENDERBUFFER,self.colorRenderbuffer);[self.context presentRenderbuffer:GL_RENDERBUFFER];}
- (BOOL)canBecomeFirstResponder{return YES;}
- (BOOL)hasText{return YES;}
- (void)insertText:(NSString*)text{NSUInteger i=0;while(i<text.length){unichar a=[text characterAtIndex:i++];uint32_t cp=a;if(a>=0xD800&&a<=0xDBFF&&i<text.length){unichar b=[text characterAtIndex:i];if(b>=0xDC00&&b<=0xDFFF){i++;cp=0x10000+((a-0xD800)<<10)+(b-0xDC00);}}luna_char(cp);}}
- (void)deleteBackward{luna_key(LUNA_KEY_BACKSPACE,0,LUNA_PRESS,0);luna_key(LUNA_KEY_BACKSPACE,0,LUNA_RELEASE,0);}
- (UIKeyboardType)keyboardType{return UIKeyboardTypeDefault;}
- (void)lunaEmitTouches:(NSSet<UITouch*>*)touches phase:(int)phase{
    for(UITouch*t in touches){
        CGPoint p=[t locationInView:self];LunaTouchEvent event;memset(&event,0,sizeof(event));
        event.id=(int64_t)(intptr_t)(__bridge void*)t;event.phase=phase;
        event.tool=LUNA_TOUCH_TOOL_FINGER;
#if __IPHONE_OS_VERSION_MAX_ALLOWED >= 90000
        if(t.type==UITouchTypeStylus)event.tool=LUNA_TOUCH_TOOL_STYLUS;
#endif
        event.x=p.x;event.y=p.y;event.radius_x=event.radius_y=(float)t.majorRadius;
        event.pressure=t.maximumPossibleForce>0?(float)(t.force/t.maximumPossibleForce):1.0f;
        if(phase==LUNA_TOUCH_UP||phase==LUNA_TOUCH_CANCEL)event.pressure=0.0f;
        if(event.tool==LUNA_TOUCH_TOOL_STYLUS){
            CGFloat altitude=t.altitudeAngle,azimuth=[t azimuthAngleInView:self];
            float tilt=(float)((M_PI_2-altitude)*180.0/M_PI);
            event.tilt_x=tilt*(float)cos(azimuth);event.tilt_y=tilt*(float)sin(azimuth);
        }
        if(!luna_ios.config.on_touch||
           !luna_ios.config.on_touch(&event,luna_ios.config.userdata))luna_touch(&event);
    }
}
- (void)touchesBegan:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event{(void)event;[self lunaEmitTouches:touches phase:LUNA_TOUCH_DOWN];}
- (void)touchesMoved:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event{(void)event;[self lunaEmitTouches:touches phase:LUNA_TOUCH_MOVE];}
- (void)touchesEnded:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event{(void)event;[self lunaEmitTouches:touches phase:LUNA_TOUCH_UP];}
- (void)touchesCancelled:(NSSet<UITouch*>*)touches withEvent:(UIEvent*)event{(void)event;[self lunaEmitTouches:touches phase:LUNA_TOUCH_CANCEL];}
- (void)dealloc{[_displayLink invalidate];if(luna_ios.initialized){if(luna_ios.config.on_shutdown)luna_ios.config.on_shutdown(luna_ios.config.userdata);[EAGLContext setCurrentContext:self.context];luna_shutdown();luna_ios.initialized=0;}if(_framebuffer)glDeleteFramebuffers(1,&_framebuffer);if(_colorRenderbuffer)glDeleteRenderbuffers(1,&_colorRenderbuffer);}
@end

@implementation LunaIOSController
- (void)loadView{self.view=[[LunaIOSView alloc]initWithFrame:[UIScreen mainScreen].bounds];luna_ios.view=(LunaIOSView*)self.view;}
- (BOOL)prefersStatusBarHidden{return YES;}
@end

@implementation LunaIOSDelegate
- (BOOL)application:(UIApplication*)application didFinishLaunchingWithOptions:(NSDictionary*)options{(void)application;(void)options;self.window=[[UIWindow alloc]initWithFrame:[UIScreen mainScreen].bounds];luna_ios.window=self.window;self.window.rootViewController=[LunaIOSController new];[self.window makeKeyAndVisible];return YES;}
@end

UIWindow*luna_ios_window(void){return luna_ios.window;}UIView*luna_ios_view(void){return luna_ios.view;}
void*luna_app_native_handle(void){return(__bridge void*)luna_ios.window;}
void luna_app_quit(void){luna_ios_close();}void luna_app_request_redraw(void){luna_ios_redraw();}
int luna_app_run(const LunaAppConfig*cfg){memset(&luna_ios,0,sizeof(luna_ios));if(cfg)luna_ios.config=*cfg;if(!luna_ios.config.title)luna_ios.config.title="Luna UI";return UIApplicationMain(0,NULL,nil,NSStringFromClass([LunaIOSDelegate class]));}

#endif
#endif
