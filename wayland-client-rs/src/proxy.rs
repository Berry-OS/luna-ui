/*
 * Copyright © 2026 Yuichiro Nakada / Project Vespera
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */


use std::os::raw::c_void;
use crate::interfaces::WlInterface;

pub const WL_PROXY_FLAG_ID_DELETED: u32 = 1 << 0;
pub const WL_PROXY_FLAG_DESTROYED:  u32 = 1 << 1;
pub const WL_PROXY_FLAG_WRAPPER:    u32 = 1 << 2;

/// wl_proxy layout matches libwayland wayland-private.h.
#[repr(C)]
pub struct Proxy {
    pub interface:       *const WlInterface,                 pub implementation:  *const c_void,                      pub id:              u32,                                pub _id_pad:         u32,                                pub display:         *mut crate::display::Display,       pub queue:           *mut crate::types::wl_event_queue,     pub flags:           u32,                                pub refcount:        i32,                                pub user_data:       *mut c_void,                        pub listener:        *const c_void,                      pub version:         u32,                                pub _version_pad:    u32,                                pub tag:             *const *const std::os::raw::c_char, }

unsafe impl Send for Proxy {}
unsafe impl Sync for Proxy {}

impl Proxy {
    pub fn new(
        id: u32,
        interface: *const WlInterface,
        version: u32,
        display: *mut crate::display::Display,
    ) -> Self {
        Proxy {
            interface,
            implementation: std::ptr::null(),
            id,
            _id_pad: 0,
            display,
            queue: std::ptr::null_mut(),
            flags: 0,
            refcount: 1,
            user_data: std::ptr::null_mut(),
            listener: std::ptr::null(),
            version,
            _version_pad: 0,
            tag: std::ptr::null(),
        }
    }

    pub fn is_deleted(&self) -> bool {
        self.flags & WL_PROXY_FLAG_ID_DELETED != 0
    }
}
