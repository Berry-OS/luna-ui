# Luna UI cross-platform example build
#
# Common entry points:
#   make linux
#   make windows
#   make macos
#   make android
#   make ios
#   make ios-sim
#   make web
#   make all-platforms   # builds everything supported by the current host/toolchain

.DEFAULT_GOAL := native

ROOT := ..
BUILD ?= build
APP_NAME ?= LunaExample
BUNDLE_ID ?= org.berryos.lunaui.example

COMMON_CPPFLAGS := -I$(ROOT)
COMMON_CFLAGS := -O2 -std=gnu11 -Wall -Wextra

UNAME_S := $(shell uname -s 2>/dev/null || echo Unknown)
HOST_WINDOWS := $(if $(filter Windows_NT,$(OS)),1,0)

.PHONY: native linux windows macos android android-so ios ios-sim web \
        all-platforms check css-parity clean help

help:
	@printf '%s\n' \
	  'Luna UI example targets:' \
	  '  make native       Build for the current desktop host' \
	  '  make linux        Linux/GLFW executable' \
	  '  make windows      Windows .exe (MinGW-w64)' \
	  '  make macos        macOS .app (macOS + Xcode)' \
	  '  make android      Signed Android debug APK (SDK + NDK + JDK)' \
	  '  make android-so   Android NativeActivity shared library only' \
	  '  make ios          iOS device .app (macOS + Xcode; signing optional)' \
	  '  make ios-sim      iOS Simulator .app (macOS + Xcode)' \
	  '  make web          Emscripten JS/WASM build' \
	  '  make clean        Remove build outputs'

ifeq ($(HOST_WINDOWS),1)
native: windows
else ifeq ($(UNAME_S),Darwin)
native: macos
else
native: linux
endif

# -----------------------------------------------------------------------------
# Linux / GLFW + OpenGL
# -----------------------------------------------------------------------------
CC ?= cc
GLFW_CFLAGS := $(shell pkg-config --cflags glfw3 2>/dev/null)
GLFW_LIBS := $(shell pkg-config --libs glfw3 2>/dev/null)

linux: $(BUILD)/linux/luna-example

$(BUILD)/linux/luna-example: example.c $(ROOT)/luna-ui.h $(ROOT)/luna_glfw.h
	@mkdir -p $(dir $@)
	$(CC) $(COMMON_CPPFLAGS) $(COMMON_CFLAGS) $(GLFW_CFLAGS) \
		-o $@ example.c $(GLFW_LIBS) -lGL -lm

# -----------------------------------------------------------------------------
# Windows / MinGW-w64
# -----------------------------------------------------------------------------
ifeq ($(HOST_WINDOWS),1)
WINDOWS_CC ?= gcc
else
WINDOWS_CC ?= x86_64-w64-mingw32-gcc
endif
WINDOWS_CFLAGS ?= -O2 -std=gnu11 -Wall -Wextra
WINDOWS_LIBS := -lopengl32 -lgdi32 -luser32 -limm32 -lshell32

windows: $(BUILD)/windows/luna-example.exe

$(BUILD)/windows/luna-example.exe: example.c $(ROOT)/luna-ui.h $(ROOT)/luna_windows.h
	@mkdir -p $(dir $@)
	$(WINDOWS_CC) $(COMMON_CPPFLAGS) $(WINDOWS_CFLAGS) \
		-o $@ example.c -mwindows $(WINDOWS_LIBS)

# -----------------------------------------------------------------------------
# macOS / Cocoa + OpenGL
# -----------------------------------------------------------------------------
MACOS_CC := xcrun --sdk macosx clang
MACOS_APP := $(BUILD)/macos/$(APP_NAME).app
MACOS_BIN := $(MACOS_APP)/Contents/MacOS/$(APP_NAME)
MACOS_PLIST := platform/macos/Info.plist

macos: $(MACOS_BIN)

$(MACOS_BIN): example.c $(ROOT)/luna-ui.h $(ROOT)/luna_macos.h $(MACOS_PLIST)
	@command -v xcrun >/dev/null || { echo 'error: Xcode Command Line Tools are required'; exit 1; }
	@mkdir -p $(MACOS_APP)/Contents/MacOS $(MACOS_APP)/Contents/Resources
	@sed -e 's/$${BUNDLE_ID}/$(BUNDLE_ID)/g' \
	     -e 's/$${APP_NAME}/$(APP_NAME)/g' $(MACOS_PLIST) > $(MACOS_APP)/Contents/Info.plist
	$(MACOS_CC) -O2 -I$(ROOT) -x objective-c example.c \
		-o $(MACOS_BIN) \
		-framework Cocoa -framework OpenGL -framework CoreText
	@printf 'APPL????' > $(MACOS_APP)/Contents/PkgInfo
	@echo "Built $(MACOS_APP)"

# -----------------------------------------------------------------------------
# Android / NativeActivity + EGL + OpenGL ES 3
# -----------------------------------------------------------------------------
ANDROID_API ?= 24
ANDROID_TARGET_API ?= $(ANDROID_API)
ANDROID_ABI ?= arm64-v8a
ANDROID_SDK_ROOT ?= $(ANDROID_HOME)
ANDROID_NDK_ROOT ?= $(ANDROID_NDK_HOME)

ifeq ($(UNAME_S),Darwin)
ANDROID_HOST_TAG ?= darwin-x86_64
else ifeq ($(HOST_WINDOWS),1)
ANDROID_HOST_TAG ?= windows-x86_64
else
ANDROID_HOST_TAG ?= linux-x86_64
endif

ANDROID_TRIPLE_arm64-v8a := aarch64-linux-android
ANDROID_TRIPLE_x86_64 := x86_64-linux-android
ANDROID_TRIPLE_armeabi-v7a := armv7a-linux-androideabi
ANDROID_TRIPLE_x86 := i686-linux-android
ANDROID_TRIPLE := $(ANDROID_TRIPLE_$(ANDROID_ABI))
ANDROID_TOOLCHAIN := $(ANDROID_NDK_ROOT)/toolchains/llvm/prebuilt/$(ANDROID_HOST_TAG)
ANDROID_CC := $(ANDROID_TOOLCHAIN)/bin/$(ANDROID_TRIPLE)$(ANDROID_API)-clang
ANDROID_LIB := $(BUILD)/android/obj/lib/$(ANDROID_ABI)/libluna-example.so
ANDROID_APK := $(BUILD)/android/$(APP_NAME)-debug.apk

android-so: $(ANDROID_LIB)

$(ANDROID_LIB): example_android.c $(ROOT)/luna-ui.h $(ROOT)/luna_android.h
	@if [ -z "$(ANDROID_NDK_ROOT)" ]; then echo 'error: set ANDROID_NDK_ROOT or ANDROID_NDK_HOME'; exit 1; fi
	@if [ -z "$(ANDROID_TRIPLE)" ]; then echo 'error: unsupported ANDROID_ABI=$(ANDROID_ABI)'; exit 1; fi
	@if [ ! -x "$(ANDROID_CC)" ]; then echo "error: Android compiler not found: $(ANDROID_CC)"; exit 1; fi
	@mkdir -p $(dir $@)
	"$(ANDROID_CC)" -O2 -std=gnu11 -fPIC -shared -I$(ROOT) \
		example_android.c -o $@ \
		-landroid -lEGL -lGLESv3 -ldl -llog

android: $(ANDROID_APK)

$(ANDROID_APK): $(ANDROID_LIB) platform/android/AndroidManifest.xml platform/android/build-apk.sh
	@if [ -z "$(ANDROID_SDK_ROOT)" ]; then echo 'error: set ANDROID_SDK_ROOT or ANDROID_HOME'; exit 1; fi
	@ANDROID_SDK_ROOT="$(ANDROID_SDK_ROOT)" \
	 ANDROID_API="$(ANDROID_API)" \
	 ANDROID_TARGET_API="$(ANDROID_TARGET_API)" \
	 ANDROID_ABI="$(ANDROID_ABI)" \
	 APP_NAME="$(APP_NAME)" \
	 BUNDLE_ID="$(BUNDLE_ID)" \
	 LIB_PATH="$(abspath $(ANDROID_LIB))" \
	 OUTPUT_APK="$(abspath $(ANDROID_APK))" \
	 sh platform/android/build-apk.sh

# -----------------------------------------------------------------------------
# iOS / UIKit + OpenGL ES
# -----------------------------------------------------------------------------
IOS_MIN ?= 13.0
IOS_ARCH ?= arm64
IOS_SIM_ARCH ?= $(if $(filter arm64,$(shell uname -m 2>/dev/null)),arm64,x86_64)
IOS_SIGN_IDENTITY ?=
IOS_APP := $(BUILD)/ios/$(APP_NAME).app
IOS_BIN := $(IOS_APP)/$(APP_NAME)
IOS_SIM_APP := $(BUILD)/ios-simulator/$(APP_NAME).app
IOS_SIM_BIN := $(IOS_SIM_APP)/$(APP_NAME)
IOS_PLIST := platform/ios/Info.plist

IOS_DEVICE_SDK := $(shell xcrun --sdk iphoneos --show-sdk-path 2>/dev/null)
IOS_SIM_SDK := $(shell xcrun --sdk iphonesimulator --show-sdk-path 2>/dev/null)

ios: $(IOS_BIN)

$(IOS_BIN): example_ios.m $(ROOT)/luna-ui.h $(ROOT)/luna_ios.h $(IOS_PLIST)
	@command -v xcrun >/dev/null || { echo 'error: Xcode is required for iOS builds'; exit 1; }
	@mkdir -p $(IOS_APP)
	@sed -e 's/$${BUNDLE_ID}/$(BUNDLE_ID)/g' \
	     -e 's/$${APP_NAME}/$(APP_NAME)/g' $(IOS_PLIST) > $(IOS_APP)/Info.plist
	xcrun --sdk iphoneos clang -O2 -I$(ROOT) -x objective-c \
		-arch $(IOS_ARCH) -isysroot "$(IOS_DEVICE_SDK)" \
		-miphoneos-version-min=$(IOS_MIN) example_ios.m \
		-o $(IOS_BIN) \
		-framework UIKit -framework QuartzCore -framework OpenGLES -framework CoreText
	@if [ -n "$(IOS_SIGN_IDENTITY)" ]; then \
		codesign --force --sign "$(IOS_SIGN_IDENTITY)" --timestamp=none $(IOS_APP); \
	else \
		echo 'Built unsigned device app. Set IOS_SIGN_IDENTITY to codesign it.'; \
	fi
	@echo "Built $(IOS_APP)"

ios-sim: $(IOS_SIM_BIN)

$(IOS_SIM_BIN): example_ios.m $(ROOT)/luna-ui.h $(ROOT)/luna_ios.h $(IOS_PLIST)
	@command -v xcrun >/dev/null || { echo 'error: Xcode is required for iOS Simulator builds'; exit 1; }
	@mkdir -p $(IOS_SIM_APP)
	@sed -e 's/$${BUNDLE_ID}/$(BUNDLE_ID)/g' \
	     -e 's/$${APP_NAME}/$(APP_NAME)/g' $(IOS_PLIST) > $(IOS_SIM_APP)/Info.plist
	xcrun --sdk iphonesimulator clang -O2 -I$(ROOT) -x objective-c \
		-arch $(IOS_SIM_ARCH) -isysroot "$(IOS_SIM_SDK)" \
		-mios-simulator-version-min=$(IOS_MIN) example_ios.m \
		-o $(IOS_SIM_BIN) \
		-framework UIKit -framework QuartzCore -framework OpenGLES -framework CoreText
	@codesign --force --sign - --timestamp=none $(IOS_SIM_APP) >/dev/null 2>&1 || true
	@echo "Built $(IOS_SIM_APP)"

# -----------------------------------------------------------------------------
# Web / Emscripten + WebGL 2
# -----------------------------------------------------------------------------
EMCC ?= emcc
WEB_DIR := $(BUILD)/web
WEB_JS := $(WEB_DIR)/luna-example.js

web: $(WEB_JS)

$(WEB_JS): example.c $(ROOT)/luna-ui.h $(ROOT)/luna_web.h
	@command -v $(EMCC) >/dev/null || { echo 'error: emcc not found; activate/install Emscripten'; exit 1; }
	@mkdir -p $(WEB_DIR)
	$(EMCC) -O2 -I$(ROOT) example.c -o $(WEB_JS) \
		-sUSE_WEBGL2=1 -sFULL_ES3=1 -sALLOW_MEMORY_GROWTH=1 \
		-sEXPORTED_FUNCTIONS=_main,_malloc,_free,_luna_web_commit_utf8 \
		-sEXPORTED_RUNTIME_METHODS=UTF8ToString,stringToUTF8,lengthBytesUTF8
	@cp index.html $(WEB_DIR)/index.html
	@echo "Built $(WEB_DIR)"

# -----------------------------------------------------------------------------
# Existing parity test
# -----------------------------------------------------------------------------
css-parity: $(ROOT)/css_parity.c $(ROOT)/luna-ui.h example.c
	@mkdir -p $(BUILD)
	$(CC) $(COMMON_CPPFLAGS) $(COMMON_CFLAGS) -o $(BUILD)/css-parity $(ROOT)/css_parity.c -lGL -lm

check: css-parity
	$(BUILD)/css-parity

# Build what the current host can reasonably support without pretending that
# Apple SDKs are cross-platform.
ifeq ($(UNAME_S),Darwin)
all-platforms: macos windows android ios-sim web
else ifeq ($(HOST_WINDOWS),1)
all-platforms: windows android web
else
all-platforms: linux windows android web
endif

clean:
	rm -rf $(BUILD)
