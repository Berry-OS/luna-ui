/* luna_web.h - header-only Emscripten/WebGL 2 host for Luna UI */

#if defined(LUNA_UI_PLATFORM_PRELUDE)
#ifndef LUNA_WEB_PRELUDE_INCLUDED
#define LUNA_WEB_PRELUDE_INCLUDED
#include <emscripten.h>
#include <emscripten/html5.h>
#include <GLES3/gl3.h>
#include <stdlib.h>
#include <string.h>
#define LUNA_UI_GLES3 1
#define LUNA_UI_PLATFORM_GL_INCLUDED 1
#ifndef LUNA_WEB_CANVAS
#define LUNA_WEB_CANVAS "#canvas"
#endif
#endif
#endif

#if defined(LUNA_UI_PLATFORM_BODY)
#ifndef LUNA_WEB_BODY_INCLUDED
#define LUNA_WEB_BODY_INCLUDED
#ifdef __cplusplus
extern "C" {
#endif
EMSCRIPTEN_WEBGL_CONTEXT_HANDLE luna_web_context(void);
#ifdef __cplusplus
}
#endif
#endif

#if defined(LUNA_UI_PLATFORM_BODY) && defined(LUNA_UI_IMPLEMENTATION) && !defined(LUNA_WEB_IMPLEMENTATION_INCLUDED)
#define LUNA_WEB_IMPLEMENTATION_INCLUDED

typedef struct LunaWebState {
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE context;
    LunaAppConfig config;
    double previous;
    int running;
    int css_width;
    int css_height;
    char* clipboard_cache;
} LunaWebState;

static LunaWebState luna_web;

static double luna_web_time_impl(void){return emscripten_get_now()/1000.0;}
static void*luna_web_get_proc(const char*name){return emscripten_webgl_get_proc_address(name);}

static unsigned char*luna_web_read_resource(const char*path,size_t*out_size){FILE*f;long n;unsigned char*p;if(out_size)*out_size=0;if(!path)return NULL;f=fopen(path,"rb");if(!f&&path[0]=='/')f=fopen(path+1,"rb");if(!f)return NULL;fseek(f,0,SEEK_END);n=ftell(f);fseek(f,0,SEEK_SET);p=(unsigned char*)malloc((size_t)n+1u);if(p&&fread(p,1,(size_t)n,f)==(size_t)n){p[n]=0;if(out_size)*out_size=(size_t)n;}else{free(p);p=NULL;}fclose(f);return p;}
static unsigned char*luna_web_load_font(int role,size_t*out_size){const char*paths[6]={0};int n=0;switch(role){case LUNA_FONT_REGULAR:paths[n++]="fonts/Inter-Regular.ttf";paths[n++]="ui/fonts/Inter-Regular.ttf";break;case LUNA_FONT_BOLD:paths[n++]="fonts/Inter-Bold.ttf";paths[n++]="ui/fonts/Inter-Bold.ttf";break;case LUNA_FONT_CJK:paths[n++]="fonts/NotoSansCJK-Regular.ttc";paths[n++]="ui/fonts/NotoSansJP-Regular.ttf";break;case LUNA_FONT_MONO:paths[n++]="fonts/RobotoMono-Regular.ttf";break;case LUNA_FONT_SYMBOLS:paths[n++]="fonts/LunaSymbols-Solid.otf";paths[n++]="ui/fonts/LunaSymbols-Solid.otf";break;case LUNA_FONT_BRANDS:paths[n++]="fonts/LunaSymbols-Brands.otf";paths[n++]="ui/fonts/LunaSymbols-Brands.otf";break;}for(int i=0;i<n;i++){unsigned char*p=luna_web_read_resource(paths[i],out_size);if(p)return p;}return NULL;}

EM_JS(void,luna_web_js_set_cursor2,(int type),{
    const values=['default','pointer','text','crosshair','ew-resize','ns-resize'];
    const c=document.querySelector('#canvas');if(c)c.style.cursor=values[type]||'default';
});
EM_JS(void,luna_web_js_clipboard_set,(const char*text),{const s=UTF8ToString(text);if(navigator.clipboard&&navigator.clipboard.writeText)navigator.clipboard.writeText(s).catch(()=>{});});
EM_JS(double,luna_web_js_scale,(),{return window.devicePixelRatio||1;});
EM_JS(void,luna_web_js_install_ime,(),{
    if(Module.lunaHiddenInput)return;
    const input=document.createElement('textarea');
    input.setAttribute('aria-hidden','true');input.autocapitalize='off';input.autocomplete='off';input.spellcheck=false;
    Object.assign(input.style,{position:'fixed',left:'-10000px',top:'0',width:'1px',height:'1px',opacity:'0'});
    document.body.appendChild(input);Module.lunaHiddenInput=input;
    input.addEventListener('input',()=>{if(input.value){const n=lengthBytesUTF8(input.value)+1;const p=_malloc(n);stringToUTF8(input.value,p,n);_luna_web_commit_utf8(p);_free(p);input.value=String();}});
});
EM_JS(void,luna_web_js_text_input,(int enabled),{const i=Module.lunaHiddenInput;if(!i)return;if(enabled)i.focus({preventScroll:true});else i.blur();});
EM_JS(void,luna_web_js_set_title,(const char*title),{document.title=UTF8ToString(title);});
EM_JS(int,luna_web_js_notify,(const char*title,const char*message),{
    if(!('Notification' in window)||Notification.permission!=='granted')return 0;
    new Notification(UTF8ToString(title),{body:UTF8ToString(message)});return 1;
});

static void luna_web_set_cursor(int type){luna_web_js_set_cursor2(type);}
static void luna_web_close(void){luna_web.running=0;emscripten_cancel_main_loop();}
static void luna_web_iconify(void){}static void luna_web_maximize(void){}static void luna_web_redraw(void){}
static float luna_web_scale(void){return(float)luna_web_js_scale();}
static void luna_web_set_clipboard(const char*text){free(luna_web.clipboard_cache);luna_web.clipboard_cache=luna_strdup_local(text?text:"");luna_web_js_clipboard_set(text?text:"");}
static char*luna_web_get_clipboard(void){return luna_web.clipboard_cache?luna_strdup_local(luna_web.clipboard_cache):NULL;}
static void luna_web_text_input(int enabled,float x,float y,float w,float h){(void)x;(void)y;(void)w;(void)h;luna_web_js_text_input(enabled);}
static void luna_web_begin_move(void){}static void luna_web_begin_resize(int edge){(void)edge;}
static void luna_web_set_title(const char*title){luna_web_js_set_title(title?title:"");}
static int luna_web_system_notify(const char*app_name,int kind,const char*title,const char*message){(void)app_name;(void)kind;return luna_web_js_notify(title?title:"",message?message:"");}

static int luna_web_mods(EM_BOOL shift,EM_BOOL ctrl,EM_BOOL alt,EM_BOOL meta){int m=0;if(shift)m|=LUNA_MOD_SHIFT;if(ctrl)m|=LUNA_MOD_CONTROL;if(alt)m|=LUNA_MOD_ALT;if(meta)m|=LUNA_MOD_SUPER;return m;}
static int luna_web_keycode(const EmscriptenKeyboardEvent*e){int k=e->keyCode;switch(k){case 32:return LUNA_KEY_SPACE;case 27:return LUNA_KEY_ESCAPE;case 13:return LUNA_KEY_ENTER;case 9:return LUNA_KEY_TAB;case 8:return LUNA_KEY_BACKSPACE;case 46:return LUNA_KEY_DELETE;case 39:return LUNA_KEY_RIGHT;case 37:return LUNA_KEY_LEFT;case 40:return LUNA_KEY_DOWN;case 38:return LUNA_KEY_UP;case 33:return LUNA_KEY_PAGE_UP;case 34:return LUNA_KEY_PAGE_DOWN;case 36:return LUNA_KEY_HOME;case 35:return LUNA_KEY_END;case 123:return LUNA_KEY_F12;default:return k;}}
static void luna_web_emit_utf8(const char*s){const unsigned char*p=(const unsigned char*)s;while(p&&*p){unsigned cp;if(*p<0x80)cp=*p++;else if((*p&0xe0)==0xc0&&p[1]){cp=((p[0]&31)<<6)|(p[1]&63);p+=2;}else if((*p&0xf0)==0xe0&&p[1]&&p[2]){cp=((p[0]&15)<<12)|((p[1]&63)<<6)|(p[2]&63);p+=3;}else if((*p&0xf8)==0xf0&&p[1]&&p[2]&&p[3]){cp=((p[0]&7)<<18)|((p[1]&63)<<12)|((p[2]&63)<<6)|(p[3]&63);p+=4;}else{p++;continue;}luna_char(cp);}}
EMSCRIPTEN_KEEPALIVE void luna_web_commit_utf8(const char*s){luna_web_emit_utf8(s);}

static EM_BOOL luna_web_mouse(int type,const EmscriptenMouseEvent*e,void*ud){(void)ud;double x=e->targetX,y=e->targetY;luna_mouse_move(x,y);int b=e->button==0?LUNA_MOUSE_BUTTON_LEFT:e->button==2?LUNA_MOUSE_BUTTON_RIGHT:LUNA_MOUSE_BUTTON_MIDDLE;int mods=luna_web_mods(e->shiftKey,e->ctrlKey,e->altKey,e->metaKey);if(type==EMSCRIPTEN_EVENT_MOUSEDOWN)luna_mouse_button(b,LUNA_PRESS,mods,x,y);else if(type==EMSCRIPTEN_EVENT_MOUSEUP)luna_mouse_button(b,LUNA_RELEASE,mods,x,y);return EM_TRUE;}
static EM_BOOL luna_web_wheel(int type,const EmscriptenWheelEvent*e,void*ud){(void)type;(void)ud;luna_scroll(-e->deltaX/100.0,-e->deltaY/100.0);return EM_TRUE;}
static EM_BOOL luna_web_key(int type,const EmscriptenKeyboardEvent*e,void*ud){(void)ud;int action=type==EMSCRIPTEN_EVENT_KEYUP?LUNA_RELEASE:(e->repeat?LUNA_REPEAT:LUNA_PRESS);luna_key(luna_web_keycode(e),e->which,action,luna_web_mods(e->shiftKey,e->ctrlKey,e->altKey,e->metaKey));return e->keyCode==9||e->keyCode==8||e->keyCode>=33&&e->keyCode<=40;}
static EM_BOOL luna_web_touch(int type,const EmscriptenTouchEvent*e,void*ud){
    (void)ud;
    int phase=type==EMSCRIPTEN_EVENT_TOUCHSTART?LUNA_TOUCH_DOWN:
              type==EMSCRIPTEN_EVENT_TOUCHMOVE?LUNA_TOUCH_MOVE:
              type==EMSCRIPTEN_EVENT_TOUCHEND?LUNA_TOUCH_UP:LUNA_TOUCH_CANCEL;
    int emitted=0;
    for(int i=0;i<e->numTouches;i++){
        const EmscriptenTouchPoint*p=&e->touches[i];
        if(!p->isChanged)continue;
        LunaTouchEvent event;memset(&event,0,sizeof(event));
        event.id=(int64_t)p->identifier;event.phase=phase;
        event.tool=LUNA_TOUCH_TOOL_FINGER;event.x=p->targetX;event.y=p->targetY;
        event.pressure=(phase==LUNA_TOUCH_UP||phase==LUNA_TOUCH_CANCEL)?0.0f:1.0f;
        if(!luna_web.config.on_touch||
           !luna_web.config.on_touch(&event,luna_web.config.userdata))luna_touch(&event);
        emitted=1;
    }
    return emitted?EM_TRUE:EM_FALSE;
}
static EM_BOOL luna_web_resize(int type,const EmscriptenUiEvent*e,void*ud){(void)type;(void)e;(void)ud;double w,h,dpr=luna_web_js_scale();emscripten_get_element_css_size(LUNA_WEB_CANVAS,&w,&h);if(w<=0)w=luna_web.config.width;if(h<=0)h=luna_web.config.height;emscripten_set_canvas_element_size(LUNA_WEB_CANVAS,(int)(w*dpr),(int)(h*dpr));luna_web.css_width=(int)w;luna_web.css_height=(int)h;luna_resize((float)w,(float)h);luna_framebuffer_resized();return EM_TRUE;}

static void luna_web_frame(void*ud){(void)ud;if(!luna_web.running)return;double now=luna_web_time_impl(),dt=now-luna_web.previous;luna_web.previous=now;if(dt<0||dt>.25)dt=1.0/60.0;if(luna_web.config.on_frame)luna_web.config.on_frame(dt,luna_web.config.userdata);luna_update(now,dt);int w=0,h=0;emscripten_get_canvas_element_size(LUNA_WEB_CANVAS,&w,&h);if(w>0&&h>0){luna_render(w,h);if(luna_web.config.on_render)luna_web.config.on_render(w,h,luna_web.config.userdata);}}

EMSCRIPTEN_WEBGL_CONTEXT_HANDLE luna_web_context(void){return luna_web.context;}void*luna_app_native_handle(void){return(void*)(intptr_t)luna_web.context;}void luna_app_quit(void){luna_web_close();}void luna_app_request_redraw(void){}

int luna_app_run(const LunaAppConfig*user_cfg){LunaAppConfig cfg;EmscriptenWebGLContextAttributes a;LunaPlatform p;LunaInitConfig i;memset(&cfg,0,sizeof(cfg));if(user_cfg)cfg=*user_cfg;if(!cfg.title)cfg.title="Luna UI";if(cfg.width<=0)cfg.width=1024;if(cfg.height<=0)cfg.height=768;memset(&luna_web,0,sizeof(luna_web));luna_web.config=cfg;
    emscripten_webgl_init_context_attributes(&a);a.alpha=cfg.transparent?EM_TRUE:EM_FALSE;a.depth=EM_FALSE;a.stencil=EM_FALSE;a.antialias=EM_TRUE;a.majorVersion=2;a.minorVersion=0;a.enableExtensionsByDefault=EM_TRUE;
    luna_web.context=emscripten_webgl_create_context(LUNA_WEB_CANVAS,&a);if(luna_web.context<=0)return 1;if(emscripten_webgl_make_context_current(luna_web.context)!=EMSCRIPTEN_RESULT_SUCCESS)return 1;
    memset(&p,0,sizeof(p));p.struct_size=sizeof(p);p.api_version=LUNA_UI_API_VERSION;p.get_time=luna_web_time_impl;p.get_proc=luna_web_get_proc;p.set_cursor=luna_web_set_cursor;p.request_close=luna_web_close;p.iconify=luna_web_iconify;p.maximize_toggle=luna_web_maximize;p.request_redraw=luna_web_redraw;p.read_resource=luna_web_read_resource;p.load_font=luna_web_load_font;p.set_clipboard=luna_web_set_clipboard;p.get_clipboard=NULL;p.text_input=luna_web_text_input;p.get_scale=luna_web_scale;p.begin_move=luna_web_begin_move;p.begin_resize=luna_web_begin_resize;p.set_title=luna_web_set_title;p.system_notify=luna_web_system_notify;luna_set_platform(&p);
    luna_web_resize(0,NULL,NULL);memset(&i,0,sizeof(i));i.width=(float)luna_web.css_width;i.height=(float)luna_web.css_height;i.get_proc=luna_web_get_proc;i.frameless=1;if(!luna_init(&i))return 1;if(cfg.html)luna_parse_html(cfg.html);else if(cfg.html_path)luna_load_html_file(cfg.html_path);if(cfg.css)luna_parse_css(cfg.css);else if(cfg.css_path)luna_load_css_file(cfg.css_path);luna_inject_body_background();if(cfg.on_init)cfg.on_init(cfg.userdata);luna_wire_onclick_handlers();
    emscripten_set_mousedown_callback(LUNA_WEB_CANVAS,NULL,EM_TRUE,luna_web_mouse);emscripten_set_mouseup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW,NULL,EM_TRUE,luna_web_mouse);emscripten_set_mousemove_callback(LUNA_WEB_CANVAS,NULL,EM_TRUE,luna_web_mouse);emscripten_set_wheel_callback(LUNA_WEB_CANVAS,NULL,EM_TRUE,luna_web_wheel);emscripten_set_keydown_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW,NULL,EM_TRUE,luna_web_key);emscripten_set_keyup_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW,NULL,EM_TRUE,luna_web_key);emscripten_set_touchstart_callback(LUNA_WEB_CANVAS,NULL,EM_TRUE,luna_web_touch);emscripten_set_touchmove_callback(LUNA_WEB_CANVAS,NULL,EM_TRUE,luna_web_touch);emscripten_set_touchend_callback(LUNA_WEB_CANVAS,NULL,EM_TRUE,luna_web_touch);emscripten_set_touchcancel_callback(LUNA_WEB_CANVAS,NULL,EM_TRUE,luna_web_touch);emscripten_set_resize_callback(EMSCRIPTEN_EVENT_TARGET_WINDOW,NULL,EM_TRUE,luna_web_resize);luna_web_js_install_ime();luna_web.running=1;luna_web.previous=luna_web_time_impl();emscripten_set_main_loop_arg(luna_web_frame,NULL,0,cfg.vsync?1:0);return 0;}

#endif
#endif
