/*
 * Copyright © 2026 Yuichiro Nakada / Project Vespera
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

fn main() {
    println!("cargo:rustc-cdylib-link-arg=-Wl,-soname,libwayland-client.so.0");

    let out_dir = std::path::PathBuf::from(std::env::var("OUT_DIR").unwrap());

    cc::Build::new()
        .file("src/wl_marshal.c")
        .flag("-std=c11")
        .flag("-O2")
        .compile("wl_marshal");

    // Force-export C symbols so the linker does not GC wl_proxy_marshal_flags.
    let archive = out_dir.join("libwl_marshal.a");
    println!(
        "cargo:rustc-cdylib-link-arg=-Wl,--whole-archive,{},--no-whole-archive",
        archive.display()
    );

    let ver = std::path::PathBuf::from(std::env::var("CARGO_MANIFEST_DIR").unwrap())
        .join("wl_marshal.ver");
    println!("cargo:rustc-cdylib-link-arg=-Wl,--version-script={}", ver.display());
    println!("cargo:rerun-if-changed=wl_marshal.ver");
    println!("cargo:rerun-if-changed=src/wl_marshal.c");
}
