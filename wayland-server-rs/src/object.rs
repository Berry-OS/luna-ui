/*
 * Copyright © 2026 Yuichiro Nakada / Project Vespera
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */


use crate::protocol::Interface;
use crate::shm::{ShmBuffer, ShmPool};
use std::os::unix::io::RawFd;
use std::rc::Rc;

pub struct DmabufPlane {
    pub fd: RawFd,
    pub plane_idx: u32,
    pub offset: u32,
    pub stride: u32,
    pub modifier: u64,
}

#[derive(Default)]
pub struct DmabufParams {
    pub planes: Vec<DmabufPlane>,
    /// create / create_immed may run only once
    pub used: bool,
}

impl Drop for DmabufParams {
    fn drop(&mut self) {
        // Close unconsumed fds.
        for p in &self.planes {
            unsafe { libc::close(p.fd) };
        }
    }
}

#[derive(Default)]
pub struct Surface {
    pub pending_buffer: Option<u32>,
    pub pending_attach: bool,
    pub current_buffer: Option<ShmBuffer>,
    pub x: i32,
    pub y: i32,
    pub frame_callbacks: Vec<u32>,
    pub xdg_surface_id: Option<u32>,
    pub mapped: bool,
    pub popup: bool,
}

pub enum Role {
    Display,
    Registry,
    Callback,
    Compositor,
    Subcompositor,
    Subsurface { surface_id: u32 },
    Shm,
    Output,
    Seat,
    Pointer,
    Keyboard,
    DataDeviceManager,
    DataDevice,
    DataSource,
    WmBase,
    Positioner {
        size_w: i32,
        size_h: i32,
        anchor_x: i32,
        anchor_y: i32,
        anchor_w: i32,
        anchor_h: i32,
        offset_x: i32,
        offset_y: i32,
    },
    Region,
    Dmabuf,
    DmabufParams(DmabufParams),
    DmabufFeedback,
    ShmPool { pool: Option<Rc<ShmPool>> },
    Buffer(ShmBuffer),
    Surface(Surface),
    XdgSurface { surface_id: u32, configured: bool },
    XdgToplevel { xdg_surface_id: u32, title: String },
    XdgPopup { xdg_surface_id: u32 },
}

pub struct Object {
    pub interface: &'static Interface,
    pub version: u32,
    pub role: Role,
}

impl Object {
    pub fn new(interface: &'static Interface, version: u32, role: Role) -> Self {
        Object {
            interface,
            version,
            role,
        }
    }
}
