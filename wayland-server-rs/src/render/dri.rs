/*
 * Copyright © 2026 Yuichiro Nakada / Project Vespera
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

use super::vt::VtSession;
use super::{Backend, Framebuffer, InputEvent};
use crate::input::evdev::EvdevInput;
use libc::{c_void, ioctl, mmap, munmap, open, MAP_FAILED, MAP_SHARED, O_CLOEXEC, O_RDWR, PROT_READ, PROT_WRITE};
use std::ffi::CString;
use std::os::unix::io::RawFd;
use std::sync::mpsc;

// ioctl numbers (asm-generic/ioctl.h)
const DRM_BASE: u64 = 0x64; // 'd'
fn iowr<T>(nr: u64) -> u64 { (3u64 << 30) | (DRM_BASE << 8) | nr | ((std::mem::size_of::<T>() as u64) << 16) }
fn io(nr: u64) -> u64 { (DRM_BASE << 8) | nr }

#[repr(C)]
#[derive(Default)]
struct ModeCardRes {
  fb_id_ptr: u64,
  crtc_id_ptr: u64,
  connector_id_ptr: u64,
  encoder_id_ptr: u64,
  count_fbs: u32,
  count_crtcs: u32,
  count_connectors: u32,
  count_encoders: u32,
  min_width: u32,
  max_width: u32,
  min_height: u32,
  max_height: u32,
}

#[repr(C)]
#[derive(Clone, Copy)]
struct ModeInfo {
  clock: u32,
  hdisplay: u16,
  hsync_start: u16,
  hsync_end: u16,
  htotal: u16,
  hskew: u16,
  vdisplay: u16,
  vsync_start: u16,
  vsync_end: u16,
  vtotal: u16,
  vscan: u16,
  vrefresh: u32,
  flags: u32,
  type_: u32,
  name: [u8; 32],
}
impl Default for ModeInfo {
  fn default() -> Self { unsafe { std::mem::zeroed() } }
}

#[repr(C)]
#[derive(Default)]
struct ModeGetConnector {
  encoders_ptr: u64,
  modes_ptr: u64,
  props_ptr: u64,
  prop_values_ptr: u64,
  count_modes: u32,
  count_props: u32,
  count_encoders: u32,
  encoder_id: u32,
  connector_id: u32,
  connector_type: u32,
  connector_type_id: u32,
  connection: u32,
  mm_width: u32,
  mm_height: u32,
  subpixel: u32,
  pad: u32,
}

#[repr(C)]
#[derive(Default)]
struct ModeGetEncoder {
  encoder_id: u32,
  encoder_type: u32,
  crtc_id: u32,
  possible_crtcs: u32,
  possible_clones: u32,
}

#[repr(C)]
#[derive(Default)]
struct ModeCreateDumb {
  height: u32,
  width: u32,
  bpp: u32,
  flags: u32,
  handle: u32,
  pitch: u32,
  size: u64,
}

#[repr(C)]
#[derive(Default)]
struct ModeMapDumb {
  handle: u32,
  pad: u32,
  offset: u64,
}

#[repr(C)]
#[derive(Default)]
struct ModeFbCmd {
  fb_id: u32,
  width: u32,
  height: u32,
  pitch: u32,
  bpp: u32,
  depth: u32,
  handle: u32,
}

#[repr(C)]
#[derive(Default)]
struct ModeDestroyDumb {
  handle: u32,
}

#[repr(C)]
struct ModeCrtc {
  set_connectors_ptr: u64,
  count_connectors: u32,
  crtc_id: u32,
  fb_id: u32,
  x: u32,
  y: u32,
  gamma_size: u32,
  mode_valid: u32,
  mode: ModeInfo,
}
impl Default for ModeCrtc {
  fn default() -> Self { unsafe { std::mem::zeroed() } }
}

const DRM_MODE_CONNECTED: u32 = 1;

pub struct DriBackend {
  fd: i32,
  map: *mut u8,
  map_len: usize,
  pitch: u32,
  width: u32,
  height: u32,
  connector_id: u32,
  crtc_id: u32,
  fb_id: u32,
  handle: u32,
  mode: ModeInfo,
  saved_crtc: ModeCrtc,
  active: bool,
  master: bool,
  vt: VtSession,
  input: Option<EvdevInput>,
}

impl DriBackend {
  pub fn open_any(tty: Option<&str>, with_input: bool) -> Option<Self> {
    for n in 0..4 {
      let path = format!("/dev/dri/card{}", n);
      if let Some(b) = Self::open(&path, tty, with_input) {
        return Some(b);
      }
    }
    None
  }

  pub fn open(path: &str, tty: Option<&str>, with_input: bool) -> Option<Self> {
    let cpath = CString::new(path).ok()?;
    let fd = unsafe { open(cpath.as_ptr(), O_RDWR | O_CLOEXEC) };
    if fd < 0 {
      eprintln!("[luna-compositor] dri: open({}) failed: {}", path, std::io::Error::last_os_error());
      return None;
    }
    let vt = match VtSession::open(tty) {
      Ok(vt) => vt,
      Err(e) => {
        eprintln!("[luna-compositor] vt: failed to acquire {}: {}", tty.unwrap_or("/dev/tty"), e);
        unsafe { libc::close(fd) };
        return None;
      }
    };
    let master = unsafe { ioctl(fd, io(0x1e)) } == 0;
    if !master {
      eprintln!("[luna-compositor] dri: failed to acquire DRM master: {}", std::io::Error::last_os_error());
      unsafe { libc::close(fd) };
      return None;
    }
    let input = if with_input {
      match EvdevInput::start() {
        Ok(input) => Some(input),
        Err(e) => {
          eprintln!("[luna-compositor] input disabled: {}", e);
          None
        }
      }
    } else {
      None
    };
    match unsafe { Self::setup(fd, vt, input) } {
      Some(b) => {
        eprintln!("[luna-compositor] dri: {} ready ({}x{}, pitch={})", path, b.width, b.height, b.pitch);
        Some(b)
      }
      None => {
        eprintln!(
          "[luna-compositor] dri: {} open ok but mode set failed \
                     (another compositor may hold DRM master)",
          path
        );
        unsafe { libc::close(fd) };
        None
      }
    }
  }

  unsafe fn setup(fd: i32, vt: VtSession, input: Option<EvdevInput>) -> Option<Self> {
    let mut res = ModeCardRes::default();
    if ioctl(fd, iowr::<ModeCardRes>(0xA0), &mut res) != 0 {
      return None;
    }
    let mut connectors = vec![0u32; res.count_connectors as usize];
    let mut encoders = vec![0u32; res.count_encoders as usize];
    let mut crtcs = vec![0u32; res.count_crtcs as usize];
    res.connector_id_ptr = connectors.as_mut_ptr() as u64;
    res.encoder_id_ptr = encoders.as_mut_ptr() as u64;
    res.crtc_id_ptr = crtcs.as_mut_ptr() as u64;
    res.fb_id_ptr = 0;
    if ioctl(fd, iowr::<ModeCardRes>(0xA0), &mut res) != 0 {
      return None;
    }

    for &cid in &connectors {
      let mut conn = ModeGetConnector::default();
      conn.connector_id = cid;
      if ioctl(fd, iowr::<ModeGetConnector>(0xA7), &mut conn) != 0 {
        continue;
      }
      if conn.connection != DRM_MODE_CONNECTED || conn.count_modes == 0 {
        continue;
      }
      let mut modes = vec![ModeInfo::default(); conn.count_modes as usize];
      let mut conn_encs = vec![0u32; conn.count_encoders as usize];
      let mut props = vec![0u32; conn.count_props as usize];
      let mut prop_vals = vec![0u64; conn.count_props as usize];
      conn.modes_ptr = modes.as_mut_ptr() as u64;
      conn.encoders_ptr = conn_encs.as_mut_ptr() as u64;
      conn.props_ptr = props.as_mut_ptr() as u64;
      conn.prop_values_ptr = prop_vals.as_mut_ptr() as u64;
      if ioctl(fd, iowr::<ModeGetConnector>(0xA7), &mut conn) != 0 {
        continue;
      }
      let mode = modes[0];

      let mut enc = ModeGetEncoder::default();
      enc.encoder_id = conn.encoder_id;
      if ioctl(fd, iowr::<ModeGetEncoder>(0xA6), &mut enc) != 0 || enc.crtc_id == 0 {
        continue;
      }
      let mut saved_crtc = ModeCrtc::default();
      saved_crtc.crtc_id = enc.crtc_id;
      if ioctl(fd, iowr::<ModeCrtc>(0xA1), &mut saved_crtc) != 0 {
        continue;
      }

      let w = mode.hdisplay as u32;
      let h = mode.vdisplay as u32;

      let mut create = ModeCreateDumb::default();
      create.width = w;
      create.height = h;
      create.bpp = 32;
      if ioctl(fd, iowr::<ModeCreateDumb>(0xB2), &mut create) != 0 {
        continue;
      }

      let mut fbcmd = ModeFbCmd::default();
      fbcmd.width = w;
      fbcmd.height = h;
      fbcmd.bpp = 32;
      fbcmd.depth = 24;
      fbcmd.pitch = create.pitch;
      fbcmd.handle = create.handle;
      if ioctl(fd, iowr::<ModeFbCmd>(0xAE), &mut fbcmd) != 0 {
        destroy_dumb(fd, create.handle);
        continue;
      }

      let mut mapreq = ModeMapDumb::default();
      mapreq.handle = create.handle;
      if ioctl(fd, iowr::<ModeMapDumb>(0xB3), &mut mapreq) != 0 {
        remove_fb(fd, fbcmd.fb_id);
        destroy_dumb(fd, create.handle);
        continue;
      }
      let ptr = mmap(std::ptr::null_mut(), create.size as usize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mapreq.offset as i64);
      if ptr == MAP_FAILED {
        remove_fb(fd, fbcmd.fb_id);
        destroy_dumb(fd, create.handle);
        continue;
      }

      let mut crtc = ModeCrtc::default();
      crtc.crtc_id = enc.crtc_id;
      crtc.fb_id = fbcmd.fb_id;
      crtc.mode = mode;
      crtc.mode_valid = 1;
      crtc.count_connectors = 1;
      crtc.set_connectors_ptr = &cid as *const u32 as u64;
      if ioctl(fd, iowr::<ModeCrtc>(0xA2), &mut crtc) != 0 {
        munmap(ptr, create.size as usize);
        remove_fb(fd, fbcmd.fb_id);
        destroy_dumb(fd, create.handle);
        continue;
      }

      return Some(DriBackend {
        fd,
        map: ptr as *mut u8,
        map_len: create.size as usize,
        pitch: create.pitch,
        width: w,
        height: h,
        connector_id: cid,
        crtc_id: enc.crtc_id,
        fb_id: fbcmd.fb_id,
        handle: create.handle,
        mode,
        saved_crtc,
        active: true,
        master: true,
        vt,
        input,
      });
    }
    None
  }
}

impl Backend for DriBackend {
  fn size(&self) -> (u32, u32) { (self.width, self.height) }

  fn present(&mut self, fb: &Framebuffer) {
    if !self.active {
      return;
    }
    // ARGB → XRGB8888 little-endian for scanout buffer
    let copy_w = fb.width.min(self.width);
    let copy_h = fb.height.min(self.height);
    for y in 0..copy_h {
      let dst_row = unsafe { self.map.add((y * self.pitch) as usize) as *mut u32 };
      let src_row = &fb.pixels[(y * fb.width) as usize..];
      for x in 0..copy_w {
        unsafe { *dst_row.add(x as usize) = src_row[x as usize] };
      }
    }
  }

  fn take_input_channel(&mut self) -> Option<(mpsc::Receiver<InputEvent>, RawFd)> { self.input.as_mut().and_then(EvdevInput::take_channel) }

  fn deactivate(&mut self) {
    if !self.active {
      return;
    }
    if let Some(input) = &self.input {
      input.set_active(false);
    }
    unsafe {
      self.restore_crtc();
      if self.master {
        ioctl(self.fd, io(0x1f));
        self.master = false;
      }
    }
    self.active = false;
    self.vt.release();
    eprintln!("[luna-compositor] VT released");
  }

  fn activate(&mut self) {
    if self.active {
      return;
    }
    self.vt.acknowledge_acquire();
    unsafe {
      self.master = ioctl(self.fd, io(0x1e)) == 0;
      if self.master && self.set_luna_crtc() {
        self.active = true;
      }
    }
    if self.active {
      if let Some(input) = &self.input {
        input.set_active(true);
      }
      eprintln!("[luna-compositor] VT acquired");
    } else {
      eprintln!("[luna-compositor] failed to resume DRM after VT acquire");
    }
  }

  fn switch_vt(&mut self, vt: u8) {
    if let Err(e) = self.vt.switch_to(vt) {
      eprintln!("[luna-compositor] VT_ACTIVATE({}) failed: {}", vt, e);
    }
  }

  fn shutdown(&mut self) {
    if let Some(input) = &self.input {
      input.set_active(false);
    }
    self.input.take();
  }
}

impl DriBackend {
  unsafe fn restore_crtc(&mut self) {
    let cid = self.connector_id;
    self.saved_crtc.count_connectors = 1;
    self.saved_crtc.set_connectors_ptr = &cid as *const u32 as u64;
    ioctl(self.fd, iowr::<ModeCrtc>(0xA2), &mut self.saved_crtc);
  }

  unsafe fn set_luna_crtc(&mut self) -> bool {
    let cid = self.connector_id;
    let mut crtc = ModeCrtc::default();
    crtc.crtc_id = self.crtc_id;
    crtc.fb_id = self.fb_id;
    crtc.mode = self.mode;
    crtc.mode_valid = 1;
    crtc.count_connectors = 1;
    crtc.set_connectors_ptr = &cid as *const u32 as u64;
    ioctl(self.fd, iowr::<ModeCrtc>(0xA2), &mut crtc) == 0
  }
}

impl Drop for DriBackend {
  fn drop(&mut self) {
    if let Some(input) = &self.input {
      input.set_active(false);
    }
    unsafe {
      if self.active && self.master {
        self.restore_crtc();
      }
      if !self.map.is_null() {
        munmap(self.map as *mut c_void, self.map_len);
        self.map = std::ptr::null_mut();
      }
      remove_fb(self.fd, self.fb_id);
      destroy_dumb(self.fd, self.handle);
      if self.master {
        ioctl(self.fd, io(0x1f));
        self.master = false;
      }
      self.vt.restore();
      libc::close(self.fd);
    }
  }
}

unsafe fn remove_fb(fd: RawFd, fb_id: u32) {
  if fb_id != 0 {
    let mut id = fb_id;
    ioctl(fd, iowr::<u32>(0xAF), &mut id);
  }
}

unsafe fn destroy_dumb(fd: RawFd, handle: u32) {
  if handle != 0 {
    let mut destroy = ModeDestroyDumb { handle };
    ioctl(fd, iowr::<ModeDestroyDumb>(0xB4), &mut destroy);
  }
}

#[cfg(test)]
mod tests {
  use super::*;

  #[test]
  fn drm_abi_layouts_match_linux() {
    assert_eq!(std::mem::size_of::<ModeCardRes>(), 64);
    assert_eq!(std::mem::size_of::<ModeCrtc>(), 104);
    assert_eq!(iowr::<ModeCrtc>(0xA2), 0xc068_64a2);
  }
}
