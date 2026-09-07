#!/bin/sh
set -eu

: "${ANDROID_SDK_ROOT:?ANDROID_SDK_ROOT is required}"
: "${ANDROID_TARGET_API:?ANDROID_TARGET_API is required}"
: "${ANDROID_ABI:?ANDROID_ABI is required}"
: "${APP_NAME:?APP_NAME is required}"
: "${BUNDLE_ID:?BUNDLE_ID is required}"
: "${LIB_PATH:?LIB_PATH is required}"
: "${OUTPUT_APK:?OUTPUT_APK is required}"

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
WORK_DIR=$(dirname "$OUTPUT_APK")/package
MANIFEST="$WORK_DIR/AndroidManifest.xml"
ANDROID_JAR="$ANDROID_SDK_ROOT/platforms/android-$ANDROID_TARGET_API/android.jar"

if [ ! -f "$ANDROID_JAR" ]; then
    echo "error: Android platform android-$ANDROID_TARGET_API is not installed" >&2
    echo "       expected: $ANDROID_JAR" >&2
    exit 1
fi

BUILD_TOOLS_DIR=${ANDROID_BUILD_TOOLS_DIR:-}
if [ -z "$BUILD_TOOLS_DIR" ]; then
    BUILD_TOOLS_DIR=$(find "$ANDROID_SDK_ROOT/build-tools" -mindepth 1 -maxdepth 1 -type d 2>/dev/null | sort -V | tail -n 1 || true)
fi
if [ -z "$BUILD_TOOLS_DIR" ] || [ ! -d "$BUILD_TOOLS_DIR" ]; then
    echo "error: Android SDK build-tools are not installed" >&2
    exit 1
fi

AAPT2="$BUILD_TOOLS_DIR/aapt2"
ZIPALIGN="$BUILD_TOOLS_DIR/zipalign"
APKSIGNER="$BUILD_TOOLS_DIR/apksigner"
for tool in "$AAPT2" "$ZIPALIGN" "$APKSIGNER"; do
    if [ ! -x "$tool" ]; then
        echo "error: required Android tool not found: $tool" >&2
        exit 1
    fi
done
if ! command -v zip >/dev/null 2>&1; then
    echo "error: zip command is required" >&2
    exit 1
fi

rm -rf "$WORK_DIR"
mkdir -p "$WORK_DIR/lib/$ANDROID_ABI"
cp "$LIB_PATH" "$WORK_DIR/lib/$ANDROID_ABI/libluna-example.so"
sed -e "s/\${BUNDLE_ID}/$BUNDLE_ID/g" \
    -e "s/\${APP_NAME}/$APP_NAME/g" \
    "$SCRIPT_DIR/AndroidManifest.xml" > "$MANIFEST"

UNALIGNED="$WORK_DIR/unaligned.apk"
WITH_LIB="$WORK_DIR/with-lib.apk"
ALIGNED="$WORK_DIR/aligned.apk"

"$AAPT2" link \
    -I "$ANDROID_JAR" \
    --manifest "$MANIFEST" \
    --min-sdk-version "${ANDROID_API:-24}" \
    --target-sdk-version "$ANDROID_TARGET_API" \
    -o "$UNALIGNED"

cp "$UNALIGNED" "$WITH_LIB"
(
    cd "$WORK_DIR"
    zip -q -0 -u "$WITH_LIB" "lib/$ANDROID_ABI/libluna-example.so"
)

"$ZIPALIGN" -f 4 "$WITH_LIB" "$ALIGNED"

KEYSTORE=${ANDROID_DEBUG_KEYSTORE:-"$HOME/.android/debug.keystore"}
if [ ! -f "$KEYSTORE" ]; then
    if ! command -v keytool >/dev/null 2>&1; then
        echo "error: debug keystore missing and keytool was not found" >&2
        exit 1
    fi
    mkdir -p "$(dirname "$KEYSTORE")"
    keytool -genkeypair -v \
        -keystore "$KEYSTORE" \
        -storepass android \
        -alias androiddebugkey \
        -keypass android \
        -dname "CN=Android Debug,O=Android,C=US" \
        -keyalg RSA -keysize 2048 -validity 10000 >/dev/null 2>&1
fi

mkdir -p "$(dirname "$OUTPUT_APK")"
"$APKSIGNER" sign \
    --ks "$KEYSTORE" \
    --ks-key-alias androiddebugkey \
    --ks-pass pass:android \
    --key-pass pass:android \
    --out "$OUTPUT_APK" \
    "$ALIGNED"

"$APKSIGNER" verify "$OUTPUT_APK"
echo "Built $OUTPUT_APK"
