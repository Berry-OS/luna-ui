# Cross-platform example builds

The `examples/Makefile` provides a single entry point for the Luna UI native hosts.
Run all commands from `examples/`.

## Outputs

| Target | Output |
| --- | --- |
| `make linux` | `build/linux/luna-example` |
| `make windows` | `build/windows/luna-example.exe` |
| `make macos` | `build/macos/LunaExample.app` |
| `make android-so` | `build/android/obj/lib/<abi>/libluna-example.so` |
| `make android` | `build/android/LunaExample-debug.apk` |
| `make ios` | `build/ios/LunaExample.app` |
| `make ios-sim` | `build/ios-simulator/LunaExample.app` |
| `make web` | `build/web/luna-example.js` and `.wasm` |

`APP_NAME` and `BUNDLE_ID` can be overridden on the command line:

```sh
make macos APP_NAME=MyApp BUNDLE_ID=com.example.myapp
```

## Linux

Install a C compiler, GLFW development files, OpenGL development files, `pkg-config`, and GNU Make.

```sh
make linux
./build/linux/luna-example
```

## Windows

The target uses MinGW-w64. On Windows it defaults to `gcc`; from Linux/macOS it defaults to `x86_64-w64-mingw32-gcc`.

```sh
make windows
```

Override the compiler when necessary:

```sh
make windows WINDOWS_CC=/path/to/x86_64-w64-mingw32-gcc
```

## macOS

macOS builds require macOS with Xcode Command Line Tools. The implementation translation unit is compiled as Objective-C and linked with Cocoa, OpenGL, and CoreText.

```sh
make macos
open build/macos/LunaExample.app
```

## Android

Android builds use the NativeActivity host. `make android-so` builds only the native shared library. `make android` additionally packages it into a zip-aligned, debug-signed APK.

Required tools:

- Android SDK platform matching `ANDROID_TARGET_API`
- Android SDK build-tools (`aapt2`, `zipalign`, `apksigner`)
- Android NDK
- `zip`
- a JDK `keytool` when `~/.android/debug.keystore` does not already exist

Set the SDK and NDK roots:

```sh
export ANDROID_SDK_ROOT="$HOME/Android/Sdk"
export ANDROID_NDK_ROOT="$ANDROID_SDK_ROOT/ndk/29.0.0"
make android
```

The defaults are API 24 and `arm64-v8a`. They can be changed:

```sh
make android \
  ANDROID_API=24 \
  ANDROID_TARGET_API=35 \
  ANDROID_ABI=arm64-v8a
```

Supported ABI names in the Makefile are `arm64-v8a`, `armeabi-v7a`, `x86_64`, and `x86`.

Install the debug APK with ADB:

```sh
adb install -r build/android/LunaExample-debug.apk
```

## iOS device

iOS builds require macOS with Xcode. Without `IOS_SIGN_IDENTITY`, the Makefile creates an unsigned `.app`, which is useful as a build artifact but cannot be installed on a normal device.

```sh
make ios
```

To sign with a valid Apple development identity:

```sh
make ios IOS_SIGN_IDENTITY="Apple Development: Your Name (TEAMID)"
```

Provisioning is intentionally left to the developer/Xcode environment because device installation requires a matching provisioning profile and entitlements for the selected bundle identifier.

## iOS Simulator

```sh
make ios-sim
```

On Apple Silicon the default simulator architecture is `arm64`; on Intel Macs it is `x86_64`.

A built app can be installed into a booted simulator with:

```sh
xcrun simctl install booted build/ios-simulator/LunaExample.app
xcrun simctl launch booted org.berryos.lunaui.example
```

## Web

Activate Emscripten so `emcc` is on `PATH`, then run:

```sh
make web
```

Serve `build/web/` over HTTP rather than opening the HTML file directly.

## Native and aggregate targets

`make native` builds the current desktop host: Linux on Linux, macOS on macOS, or Windows on Windows.

`make all-platforms` builds the targets that are reasonable on the current host. Apple targets are never claimed to be cross-compilable from Linux or Windows.

## Cleaning

```sh
make clean
```
