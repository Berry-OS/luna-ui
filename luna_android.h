/* luna_android.h - header-only NativeActivity/EGL/OpenGL ES 3 host.
 * Before including l-u, applications may define:
 *   #define LUNA_ANDROID_APP_CONFIG my_config_function
 * where my_config_function returns LunaAppConfig. */

#if defined(LUNA_UI_PLATFORM_PRELUDE)
#ifndef LUNA_ANDROID_PRELUDE_INCLUDED
#define LUNA_ANDROID_PRELUDE_INCLUDED
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif
#include <jni.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/input.h>
#include <android/looper.h>
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#include <pthread.h>
#include <dlfcn.h>
#include <time.h>
#include <unistd.h>
#ifndef EGL_OPENGL_ES3_BIT
#define EGL_OPENGL_ES3_BIT 0x00000040
#endif
#define LUNA_UI_GLES3 1
#define LUNA_UI_PLATFORM_GL_INCLUDED 1
#endif
#endif

#if defined(LUNA_UI_PLATFORM_BODY)
#ifndef LUNA_ANDROID_BODY_INCLUDED
#define LUNA_ANDROID_BODY_INCLUDED
#ifdef __cplusplus
extern "C" {
#endif
ANativeActivity* luna_android_activity(void);
ANativeWindow* luna_android_window(void);
#ifdef __cplusplus
}
#endif
#endif

#if defined(LUNA_UI_IMPLEMENTATION) && !defined(LUNA_ANDROID_IMPLEMENTATION_INCLUDED)
#define LUNA_ANDROID_IMPLEMENTATION_INCLUDED

#ifndef LUNA_ANDROID_APP_CONFIG
static LunaAppConfig luna_android_default_app_config(void) {
    LunaAppConfig c; memset(&c,0,sizeof(c));
    c.title="Luna UI"; c.width=1280; c.height=720; c.vsync=1;
    c.html_path="ui/main.html"; c.css_path="ui/main.css";
    return c;
}
#define LUNA_ANDROID_APP_CONFIG luna_android_default_app_config
#else
extern LunaAppConfig LUNA_ANDROID_APP_CONFIG(void);
#endif

typedef struct LunaAndroidState {
    ANativeActivity* activity;
    ANativeWindow* window;
    AInputQueue* input;
    ALooper* looper;
    pthread_t thread;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int running;
    int destroy_requested;
    int window_changed;
    int input_changed;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    int fb_width;
    int fb_height;
    int core_initialized;
    LunaAppConfig config;
} LunaAndroidState;

static LunaAndroidState luna_android;

static void luna_android_sleep_millis(long ms){struct timespec ts;ts.tv_sec=ms/1000;ts.tv_nsec=(ms%1000)*1000000L;nanosleep(&ts,NULL);}
static double luna_android_time_impl(void){struct timespec ts;clock_gettime(CLOCK_MONOTONIC,&ts);return(double)ts.tv_sec+(double)ts.tv_nsec/1e9;}
static void*luna_android_get_proc(const char*name){void*p=(void*)eglGetProcAddress(name);if(!p)p=dlsym(RTLD_DEFAULT,name);return p;}

static unsigned char*luna_android_read_asset(const char*path,size_t*out_size){
    AAsset*a;off_t n;unsigned char*p;if(out_size)*out_size=0;if(!luna_android.activity||!path)return NULL;
    while(path[0]=='/')path++;a=AAssetManager_open(luna_android.activity->assetManager,path,AASSET_MODE_BUFFER);if(!a)return NULL;
    n=AAsset_getLength(a);p=(unsigned char*)malloc((size_t)n+1u);if(p){int got=AAsset_read(a,p,(size_t)n);if(got!=(int)n){free(p);p=NULL;}else{p[n]=0;if(out_size)*out_size=(size_t)n;}}AAsset_close(a);return p;
}
static unsigned char*luna_android_read_file_path(const char*path,size_t*out_size){FILE*f;long n;unsigned char*p;if(out_size)*out_size=0;f=fopen(path,"rb");if(!f)return NULL;fseek(f,0,SEEK_END);n=ftell(f);fseek(f,0,SEEK_SET);p=(unsigned char*)malloc((size_t)n+1u);if(p&&fread(p,1,(size_t)n,f)==(size_t)n){p[n]=0;if(out_size)*out_size=(size_t)n;}else{free(p);p=NULL;}fclose(f);return p;}
static unsigned char*luna_android_read_resource(const char*path,size_t*out_size){unsigned char*p=luna_android_read_asset(path,out_size);if(!p)p=luna_android_read_file_path(path,out_size);return p;}
static unsigned char*luna_android_load_font(int role,size_t*out_size){
    const char*paths[8]={0};int n=0;
    switch(role){
        case LUNA_FONT_REGULAR:paths[n++]="fonts/Inter-Regular.ttf";paths[n++]="/system/fonts/Roboto-Regular.ttf";break;
        case LUNA_FONT_BOLD:paths[n++]="fonts/Inter-Bold.ttf";paths[n++]="/system/fonts/Roboto-Bold.ttf";break;
        case LUNA_FONT_CJK:paths[n++]="fonts/NotoSansCJK-Regular.ttc";paths[n++]="/system/fonts/NotoSansCJK-Regular.ttc";paths[n++]="/system/fonts/NotoSansJP-Regular.otf";break;
        case LUNA_FONT_MONO:paths[n++]="fonts/RobotoMono-Regular.ttf";paths[n++]="/system/fonts/RobotoMono-Regular.ttf";break;
        case LUNA_FONT_SYMBOLS:paths[n++]="fonts/LunaSymbols-Solid.otf";paths[n++]="ui/fonts/LunaSymbols-Solid.otf";break;
        case LUNA_FONT_BRANDS:paths[n++]="fonts/LunaSymbols-Brands.otf";paths[n++]="ui/fonts/LunaSymbols-Brands.otf";break;
    }
    for(int i=0;i<n;i++){unsigned char*p=luna_android_read_resource(paths[i],out_size);if(p)return p;}return NULL;
}

static int luna_android_keycode(int key){switch(key){case AKEYCODE_SPACE:return LUNA_KEY_SPACE;case AKEYCODE_ESCAPE:return LUNA_KEY_ESCAPE;case AKEYCODE_ENTER:return LUNA_KEY_ENTER;case AKEYCODE_TAB:return LUNA_KEY_TAB;case AKEYCODE_DEL:return LUNA_KEY_BACKSPACE;case AKEYCODE_FORWARD_DEL:return LUNA_KEY_DELETE;case AKEYCODE_DPAD_RIGHT:return LUNA_KEY_RIGHT;case AKEYCODE_DPAD_LEFT:return LUNA_KEY_LEFT;case AKEYCODE_DPAD_DOWN:return LUNA_KEY_DOWN;case AKEYCODE_DPAD_UP:return LUNA_KEY_UP;case AKEYCODE_PAGE_UP:return LUNA_KEY_PAGE_UP;case AKEYCODE_PAGE_DOWN:return LUNA_KEY_PAGE_DOWN;case AKEYCODE_MOVE_HOME:return LUNA_KEY_HOME;case AKEYCODE_MOVE_END:return LUNA_KEY_END;case AKEYCODE_F12:return LUNA_KEY_F12;default:return key;}}
static int luna_android_mods(int meta){int m=0;if(meta&AMETA_SHIFT_ON)m|=LUNA_MOD_SHIFT;if(meta&AMETA_CTRL_ON)m|=LUNA_MOD_CONTROL;if(meta&AMETA_ALT_ON)m|=LUNA_MOD_ALT;if(meta&AMETA_META_ON)m|=LUNA_MOD_SUPER;return m;}

#ifdef __cplusplus
#define LUNA_ANDROID_JNI_ENV(env,method,...) (env)->method(__VA_ARGS__)
#define LUNA_ANDROID_JNI_VM(vm,method,...)   (vm)->method(__VA_ARGS__)
#else
#define LUNA_ANDROID_JNI_ENV(env,method,...) (*(env))->method((env),__VA_ARGS__)
#define LUNA_ANDROID_JNI_VM(vm,method,...)   (*(vm))->method((vm),__VA_ARGS__)
#endif

static JNIEnv* luna_android_get_env(int* attached){
    JNIEnv* env=NULL; JavaVM* vm;
    if(attached)*attached=0;
    if(!luna_android.activity||!(vm=luna_android.activity->vm))return NULL;
    if(LUNA_ANDROID_JNI_VM(vm,GetEnv,(void**)&env,JNI_VERSION_1_6)!=JNI_OK){
        if(LUNA_ANDROID_JNI_VM(vm,AttachCurrentThread,&env,NULL)!=JNI_OK)return NULL;
        if(attached)*attached=1;
    }
    return env;
}
static void luna_android_release_env(int attached){
    if(attached&&luna_android.activity&&luna_android.activity->vm) {
#ifdef __cplusplus
        luna_android.activity->vm->DetachCurrentThread();
#else
        (*(luna_android.activity->vm))->DetachCurrentThread(luna_android.activity->vm);
#endif
    }
}

static int luna_android_unicode_char(int keycode,int meta){
    int attached=0,result=0;JNIEnv*env=luna_android_get_env(&attached);if(!env)return 0;
    jclass cls=LUNA_ANDROID_JNI_ENV(env,FindClass,"android/view/KeyCharacterMap");
    if(cls){jmethodID load=LUNA_ANDROID_JNI_ENV(env,GetStaticMethodID,cls,"load","(I)Landroid/view/KeyCharacterMap;");jobject map=load?LUNA_ANDROID_JNI_ENV(env,CallStaticObjectMethod,cls,load,-1):NULL;if(map){jmethodID get=LUNA_ANDROID_JNI_ENV(env,GetMethodID,cls,"get","(II)I");if(get)result=LUNA_ANDROID_JNI_ENV(env,CallIntMethod,map,get,keycode,meta);LUNA_ANDROID_JNI_ENV(env,DeleteLocalRef,map);}LUNA_ANDROID_JNI_ENV(env,DeleteLocalRef,cls);}
    luna_android_release_env(attached);return result;
}

static jobject luna_android_clipboard_manager(JNIEnv*env){
    jobject manager=NULL; jclass cls; jmethodID get_service; jstring name;
    if(!env||!luna_android.activity||!luna_android.activity->clazz)return NULL;
    cls=LUNA_ANDROID_JNI_ENV(env,GetObjectClass,luna_android.activity->clazz);if(!cls)return NULL;
    get_service=LUNA_ANDROID_JNI_ENV(env,GetMethodID,cls,"getSystemService","(Ljava/lang/String;)Ljava/lang/Object;");
    name=LUNA_ANDROID_JNI_ENV(env,NewStringUTF,"clipboard");
    if(get_service&&name)manager=LUNA_ANDROID_JNI_ENV(env,CallObjectMethod,luna_android.activity->clazz,get_service,name);
    if(name)LUNA_ANDROID_JNI_ENV(env,DeleteLocalRef,name);LUNA_ANDROID_JNI_ENV(env,DeleteLocalRef,cls);return manager;
}
static void luna_android_set_clipboard(const char*utf8){
    int attached=0;JNIEnv*env=luna_android_get_env(&attached);jobject manager;jclass clip_cls,mgr_cls;jmethodID make_clip,set_clip;jstring label,text;jobject clip;
    if(!env)return;manager=luna_android_clipboard_manager(env);if(!manager){luna_android_release_env(attached);return;}
    clip_cls=LUNA_ANDROID_JNI_ENV(env,FindClass,"android/content/ClipData");mgr_cls=LUNA_ANDROID_JNI_ENV(env,GetObjectClass,manager);
    make_clip=clip_cls?LUNA_ANDROID_JNI_ENV(env,GetStaticMethodID,clip_cls,"newPlainText","(Ljava/lang/CharSequence;Ljava/lang/CharSequence;)Landroid/content/ClipData;"):NULL;
    set_clip=mgr_cls?LUNA_ANDROID_JNI_ENV(env,GetMethodID,mgr_cls,"setPrimaryClip","(Landroid/content/ClipData;)V"):NULL;
    label=LUNA_ANDROID_JNI_ENV(env,NewStringUTF,"Luna UI");text=LUNA_ANDROID_JNI_ENV(env,NewStringUTF,utf8?utf8:"");
    clip=(make_clip&&label&&text)?LUNA_ANDROID_JNI_ENV(env,CallStaticObjectMethod,clip_cls,make_clip,label,text):NULL;
    if(clip&&set_clip)LUNA_ANDROID_JNI_ENV(env,CallVoidMethod,manager,set_clip,clip);
    if(clip)LUNA_ANDROID_JNI_ENV(env,DeleteLocalRef,clip);if(text)LUNA_ANDROID_JNI_ENV(env,DeleteLocalRef,text);if(label)LUNA_ANDROID_JNI_ENV(env,DeleteLocalRef,label);if(mgr_cls)LUNA_ANDROID_JNI_ENV(env,DeleteLocalRef,mgr_cls);if(clip_cls)LUNA_ANDROID_JNI_ENV(env,DeleteLocalRef,clip_cls);LUNA_ANDROID_JNI_ENV(env,DeleteLocalRef,manager);luna_android_release_env(attached);
}
static char*luna_android_get_clipboard(void){
    int attached=0;JNIEnv*env=luna_android_get_env(&attached);jobject manager,clip,item,text_obj;char*out=NULL;jclass mgr_cls,clip_cls,item_cls,obj_cls;jmethodID get_clip,get_item,coerce,to_string;
    if(!env)return NULL;manager=luna_android_clipboard_manager(env);if(!manager){luna_android_release_env(attached);return NULL;}
    mgr_cls=LUNA_ANDROID_JNI_ENV(env,GetObjectClass,manager);get_clip=mgr_cls?LUNA_ANDROID_JNI_ENV(env,GetMethodID,mgr_cls,"getPrimaryClip","()Landroid/content/ClipData;"):NULL;clip=get_clip?LUNA_ANDROID_JNI_ENV(env,CallObjectMethod,manager,get_clip):NULL;
    clip_cls=clip?LUNA_ANDROID_JNI_ENV(env,GetObjectClass,clip):NULL;get_item=clip_cls?LUNA_ANDROID_JNI_ENV(env,GetMethodID,clip_cls,"getItemAt","(I)Landroid/content/ClipData$Item;"):NULL;item=get_item?LUNA_ANDROID_JNI_ENV(env,CallObjectMethod,clip,get_item,0):NULL;
    item_cls=item?LUNA_ANDROID_JNI_ENV(env,GetObjectClass,item):NULL;coerce=item_cls?LUNA_ANDROID_JNI_ENV(env,GetMethodID,item_cls,"coerceToText","(Landroid/content/Context;)Ljava/lang/CharSequence;"):NULL;text_obj=coerce?LUNA_ANDROID_JNI_ENV(env,CallObjectMethod,item,coerce,luna_android.activity->clazz):NULL;
    obj_cls=text_obj?LUNA_ANDROID_JNI_ENV(env,GetObjectClass,text_obj):NULL;to_string=obj_cls?LUNA_ANDROID_JNI_ENV(env,GetMethodID,obj_cls,"toString","()Ljava/lang/String;"):NULL;
    if(to_string){jstring js=(jstring)LUNA_ANDROID_JNI_ENV(env,CallObjectMethod,text_obj,to_string);if(js){const char*u=LUNA_ANDROID_JNI_ENV(env,GetStringUTFChars,js,NULL);if(u){out=luna_strdup_local(u);LUNA_ANDROID_JNI_ENV(env,ReleaseStringUTFChars,js,u);}LUNA_ANDROID_JNI_ENV(env,DeleteLocalRef,js);}}
    if(obj_cls)LUNA_ANDROID_JNI_ENV(env,DeleteLocalRef,obj_cls);if(text_obj)LUNA_ANDROID_JNI_ENV(env,DeleteLocalRef,text_obj);if(item_cls)LUNA_ANDROID_JNI_ENV(env,DeleteLocalRef,item_cls);if(item)LUNA_ANDROID_JNI_ENV(env,DeleteLocalRef,item);if(clip_cls)LUNA_ANDROID_JNI_ENV(env,DeleteLocalRef,clip_cls);if(clip)LUNA_ANDROID_JNI_ENV(env,DeleteLocalRef,clip);if(mgr_cls)LUNA_ANDROID_JNI_ENV(env,DeleteLocalRef,mgr_cls);LUNA_ANDROID_JNI_ENV(env,DeleteLocalRef,manager);luna_android_release_env(attached);return out;
}

static void luna_android_set_cursor(int t){(void)t;}static void luna_android_iconify(void){}static void luna_android_maximize(void){}
static void luna_android_close(void){pthread_mutex_lock(&luna_android.mutex);luna_android.destroy_requested=1;pthread_cond_signal(&luna_android.cond);pthread_mutex_unlock(&luna_android.mutex);}
static void luna_android_redraw(void){}
static float luna_android_scale(void){ return 1.0f; }
static void luna_android_text_input(int enabled,float x,float y,float w,float h){(void)x;(void)y;(void)w;(void)h;if(!luna_android.activity)return;if(enabled)ANativeActivity_showSoftInput(luna_android.activity,ANATIVEACTIVITY_SHOW_SOFT_INPUT_FORCED);else ANativeActivity_hideSoftInput(luna_android.activity,ANATIVEACTIVITY_HIDE_SOFT_INPUT_NOT_ALWAYS);}

static int luna_android_egl_init(ANativeWindow*window){
    EGLint major,minor,num;EGLConfig config;EGLint cfg[]={EGL_RENDERABLE_TYPE,EGL_OPENGL_ES3_BIT,EGL_SURFACE_TYPE,EGL_WINDOW_BIT,EGL_RED_SIZE,8,EGL_GREEN_SIZE,8,EGL_BLUE_SIZE,8,EGL_ALPHA_SIZE,8,EGL_NONE};EGLint ctx[]={EGL_CONTEXT_CLIENT_VERSION,3,EGL_NONE};
    luna_android.display=eglGetDisplay(EGL_DEFAULT_DISPLAY);if(luna_android.display==EGL_NO_DISPLAY||!eglInitialize(luna_android.display,&major,&minor))return 0;
    if(!eglChooseConfig(luna_android.display,cfg,&config,1,&num)||num<1)return 0;eglBindAPI(EGL_OPENGL_ES_API);
    luna_android.context=eglCreateContext(luna_android.display,config,EGL_NO_CONTEXT,ctx);if(luna_android.context==EGL_NO_CONTEXT)return 0;
    luna_android.surface=eglCreateWindowSurface(luna_android.display,config,window,NULL);if(luna_android.surface==EGL_NO_SURFACE)return 0;
    if(!eglMakeCurrent(luna_android.display,luna_android.surface,luna_android.surface,luna_android.context))return 0;
    eglQuerySurface(luna_android.display,luna_android.surface,EGL_WIDTH,&luna_android.fb_width);eglQuerySurface(luna_android.display,luna_android.surface,EGL_HEIGHT,&luna_android.fb_height);eglSwapInterval(luna_android.display,luna_android.config.vsync?1:0);return 1;
}
static void luna_android_egl_shutdown(void){if(luna_android.display!=EGL_NO_DISPLAY){eglMakeCurrent(luna_android.display,EGL_NO_SURFACE,EGL_NO_SURFACE,EGL_NO_CONTEXT);if(luna_android.surface!=EGL_NO_SURFACE)eglDestroySurface(luna_android.display,luna_android.surface);if(luna_android.context!=EGL_NO_CONTEXT)eglDestroyContext(luna_android.display,luna_android.context);eglTerminate(luna_android.display);}luna_android.display=EGL_NO_DISPLAY;luna_android.surface=EGL_NO_SURFACE;luna_android.context=EGL_NO_CONTEXT;}

static int luna_android_core_init(void){LunaPlatform p;LunaInitConfig i;memset(&p,0,sizeof(p));p.struct_size=sizeof(p);p.api_version=LUNA_UI_API_VERSION;p.get_time=luna_android_time_impl;p.get_proc=luna_android_get_proc;p.set_cursor=luna_android_set_cursor;p.request_close=luna_android_close;p.iconify=luna_android_iconify;p.maximize_toggle=luna_android_maximize;p.request_redraw=luna_android_redraw;p.read_resource=luna_android_read_resource;p.load_font=luna_android_load_font;p.set_clipboard=luna_android_set_clipboard;p.get_clipboard=luna_android_get_clipboard;p.text_input=luna_android_text_input;p.get_scale=luna_android_scale;luna_set_platform(&p);memset(&i,0,sizeof(i));i.width=(float)luna_android.fb_width;i.height=(float)luna_android.fb_height;i.get_proc=luna_android_get_proc;i.frameless=1;if(!luna_init(&i))return 0;if(luna_android.config.html)luna_parse_html(luna_android.config.html);else if(luna_android.config.html_path)luna_load_html_file(luna_android.config.html_path);if(luna_android.config.css)luna_parse_css(luna_android.config.css);else if(luna_android.config.css_path)luna_load_css_file(luna_android.config.css_path);luna_inject_body_background();if(luna_android.config.on_init)luna_android.config.on_init(luna_android.config.userdata);luna_wire_onclick_handlers();luna_android.core_initialized=1;return 1;}

static int luna_android_process_input(AInputEvent*e){int type=AInputEvent_getType(e);if(type==AINPUT_EVENT_TYPE_MOTION){int action=AMotionEvent_getAction(e),masked=action&AMOTION_EVENT_ACTION_MASK;float x=AMotionEvent_getX(e,0),y=AMotionEvent_getY(e,0);luna_mouse_move(x,y);if(masked==AMOTION_EVENT_ACTION_DOWN||masked==AMOTION_EVENT_ACTION_POINTER_DOWN)luna_mouse_button(LUNA_MOUSE_BUTTON_LEFT,LUNA_PRESS,0,x,y);else if(masked==AMOTION_EVENT_ACTION_UP||masked==AMOTION_EVENT_ACTION_POINTER_UP||masked==AMOTION_EVENT_ACTION_CANCEL)luna_mouse_button(LUNA_MOUSE_BUTTON_LEFT,LUNA_RELEASE,0,x,y);else if(masked==AMOTION_EVENT_ACTION_SCROLL)luna_scroll(AMotionEvent_getAxisValue(e,AMOTION_EVENT_AXIS_HSCROLL,0),AMotionEvent_getAxisValue(e,AMOTION_EVENT_AXIS_VSCROLL,0));return 1;}if(type==AINPUT_EVENT_TYPE_KEY){int act=AKeyEvent_getAction(e),key=AKeyEvent_getKeyCode(e),meta=AKeyEvent_getMetaState(e);int la=act==AKEY_EVENT_ACTION_DOWN?(AKeyEvent_getRepeatCount(e)?LUNA_REPEAT:LUNA_PRESS):LUNA_RELEASE;luna_key(luna_android_keycode(key),AKeyEvent_getScanCode(e),la,luna_android_mods(meta));if(la==LUNA_PRESS){int cp=luna_android_unicode_char(key,meta);if(cp>0)luna_char((unsigned)cp);}return 1;}return 0;}

static void*luna_android_thread(void*unused){(void)unused;luna_android.looper=ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);double prev=luna_android_time_impl();AInputQueue*attached=NULL;while(1){ANativeWindow*window;AInputQueue*input;pthread_mutex_lock(&luna_android.mutex);while(!luna_android.destroy_requested&&!luna_android.window&&!luna_android.input)pthread_cond_wait(&luna_android.cond,&luna_android.mutex);window=luna_android.window;input=luna_android.input;pthread_mutex_unlock(&luna_android.mutex);if(luna_android.destroy_requested)break;if(input!=attached){if(attached)AInputQueue_detachLooper(attached);attached=input;if(attached)AInputQueue_attachLooper(attached,luna_android.looper,1,NULL,NULL);}int ident;while((ident=ALooper_pollAll(0,NULL,NULL,NULL))>=0){(void)ident;if(attached){AInputEvent*e;while(AInputQueue_getEvent(attached,&e)>=0){if(AInputQueue_preDispatchEvent(attached,e))continue;int handled=luna_android_process_input(e);AInputQueue_finishEvent(attached,e,handled);}}}
        if(window&&luna_android.surface==EGL_NO_SURFACE){if(luna_android_egl_init(window)&&luna_android_core_init())prev=luna_android_time_impl();}
        if(!window&&luna_android.surface!=EGL_NO_SURFACE){if(luna_android.core_initialized){if(luna_android.config.on_shutdown)luna_android.config.on_shutdown(luna_android.config.userdata);luna_shutdown();luna_android.core_initialized=0;}luna_android_egl_shutdown();}
        if(luna_android.core_initialized&&window){double now=luna_android_time_impl(),dt=now-prev;prev=now;if(dt<0||dt>.25)dt=1.0/60.0;if(luna_android.config.on_frame)luna_android.config.on_frame(dt,luna_android.config.userdata);luna_update(now,dt);luna_render(luna_android.fb_width,luna_android.fb_height);eglSwapBuffers(luna_android.display,luna_android.surface);}else luna_android_sleep_millis(10);
    }if(attached)AInputQueue_detachLooper(attached);if(luna_android.core_initialized){if(luna_android.config.on_shutdown)luna_android.config.on_shutdown(luna_android.config.userdata);luna_shutdown();}luna_android_egl_shutdown();return NULL;}

static void luna_android_on_destroy(ANativeActivity*a){(void)a;luna_android_close();pthread_join(luna_android.thread,NULL);pthread_cond_destroy(&luna_android.cond);pthread_mutex_destroy(&luna_android.mutex);}
static void luna_android_on_window_created(ANativeActivity*a,ANativeWindow*w){(void)a;pthread_mutex_lock(&luna_android.mutex);luna_android.window=w;luna_android.window_changed=1;pthread_cond_signal(&luna_android.cond);pthread_mutex_unlock(&luna_android.mutex);}
static void luna_android_on_window_destroyed(ANativeActivity*a,ANativeWindow*w){(void)a;(void)w;pthread_mutex_lock(&luna_android.mutex);luna_android.window=NULL;luna_android.window_changed=1;pthread_cond_signal(&luna_android.cond);pthread_mutex_unlock(&luna_android.mutex);}
static void luna_android_on_input_created(ANativeActivity*a,AInputQueue*q){(void)a;pthread_mutex_lock(&luna_android.mutex);luna_android.input=q;luna_android.input_changed=1;pthread_cond_signal(&luna_android.cond);pthread_mutex_unlock(&luna_android.mutex);}
static void luna_android_on_input_destroyed(ANativeActivity*a,AInputQueue*q){(void)a;(void)q;pthread_mutex_lock(&luna_android.mutex);luna_android.input=NULL;luna_android.input_changed=1;pthread_cond_signal(&luna_android.cond);pthread_mutex_unlock(&luna_android.mutex);}

ANativeActivity*luna_android_activity(void){return luna_android.activity;}ANativeWindow*luna_android_window(void){return luna_android.window;}void*luna_app_native_handle(void){return luna_android.window;}void luna_app_quit(void){luna_android_close();}void luna_app_request_redraw(void){}
int luna_app_run(const LunaAppConfig*cfg){if(cfg)luna_android.config=*cfg;return 0;}

#ifdef __cplusplus
extern "C"
#endif
__attribute__((visibility("default"))) void ANativeActivity_onCreate(ANativeActivity*activity,void*savedState,size_t savedStateSize){(void)savedState;(void)savedStateSize;memset(&luna_android,0,sizeof(luna_android));luna_android.activity=activity;luna_android.display=EGL_NO_DISPLAY;luna_android.surface=EGL_NO_SURFACE;luna_android.context=EGL_NO_CONTEXT;luna_android.config=LUNA_ANDROID_APP_CONFIG();pthread_mutex_init(&luna_android.mutex,NULL);pthread_cond_init(&luna_android.cond,NULL);activity->callbacks->onDestroy=luna_android_on_destroy;activity->callbacks->onNativeWindowCreated=luna_android_on_window_created;activity->callbacks->onNativeWindowDestroyed=luna_android_on_window_destroyed;activity->callbacks->onInputQueueCreated=luna_android_on_input_created;activity->callbacks->onInputQueueDestroyed=luna_android_on_input_destroyed;pthread_create(&luna_android.thread,NULL,luna_android_thread,NULL);}

#endif
#endif
