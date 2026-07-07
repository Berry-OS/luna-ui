/*
 * Copyright © 2026 Yuichiro Nakada / Project Vespera
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

use std::os::raw::{c_char, c_int, c_void};

pub type wl_fixed_t = i32;

pub fn wl_fixed_to_double(f: wl_fixed_t) -> f64 {
    f as f64 / 256.0
}

pub fn wl_fixed_from_double(d: f64) -> wl_fixed_t {
    (d * 256.0) as i32
}

#[repr(C)]
pub struct wl_array {
    pub size: usize,
    pub alloc: usize,
    pub data: *mut c_void,
}

#[repr(C)]
pub union wl_argument {
    pub i: i32,
    pub u: u32,
    pub f: wl_fixed_t,
    pub s: *const c_char,
    pub o: *mut c_void,
    pub n: u32,
    pub a: *mut wl_array,
    pub h: c_int,
}

impl Default for wl_argument {
    fn default() -> Self {
        wl_argument { u: 0 }
    }
}

#[repr(C)]
pub struct wl_message {
    pub name: *const c_char,
    pub signature: *const c_char,
    pub types: *const *const crate::interfaces::WlInterface,
}

unsafe impl Send for wl_message {}
unsafe impl Sync for wl_message {}

#[repr(C)]
pub struct wl_list {
    pub prev: *mut wl_list,
    pub next: *mut wl_list,
}

impl wl_list {
    pub const fn new() -> Self {
        wl_list { prev: std::ptr::null_mut(), next: std::ptr::null_mut() }
    }
}

#[repr(C)]
pub struct wl_event_queue {
    pub _opaque: u8,
}
