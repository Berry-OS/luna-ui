#!/bin/sh
# gen_include.sh - CSS/HTML を C文字列リテラルの .h ファイルに変換する
# 使い方: ./gen_include.sh demo.css demo.html

set -e

to_c_string_h() {
    input="$1"
    output="${input}.h"

    printf '' > "$output"

    while IFS= read -r line || [ -n "$line" ]; do
        # バックスラッシュ → \\ に (必ず最初に行う)
        line=$(printf '%s' "$line" | sed 's/\\/\\\\/g')
        # ダブルクォート → \" に
        line=$(printf '%s' "$line" | sed 's/"/\\"/g')
        printf '    "%s\\n"\n' "$line" >> "$output"
    done < "$input"

    printf 'Generated: %s\n' "$output"
}

if [ $# -eq 0 ]; then
    to_c_string_h demo.css
    to_c_string_h demo.html
else
    for f in "$@"; do
        to_c_string_h "$f"
    done
fi
