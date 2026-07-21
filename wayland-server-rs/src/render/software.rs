/*
 * Copyright © 2026 Yuichiro Nakada / Project Vespera
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

use super::{Backend, Framebuffer};
use std::fs::File;
use std::io::Write;

pub struct SoftwareBackend {
  width: u32,
  height: u32,
  pub screenshot_path: Option<String>,
  frame: u64,
  fbdev: Option<File>,
  fb_bpp: u32,
}

impl SoftwareBackend {
  pub fn new(width: u32, height: u32) -> Self {
    SoftwareBackend {
      width,
      height,
      screenshot_path: None,
      frame: 0,
      fbdev: None,
      fb_bpp: 32,
    }
  }

  pub fn with_screenshot(mut self, path: impl Into<String>) -> Self {
    self.screenshot_path = Some(path.into());
    self
  }

  pub fn with_fbdev(mut self, path: &str) -> Self {
    match File::options().write(true).open(path) {
      Ok(f) => {
        eprintln!("[luna-compositor] software: writing frames to {}", path);
        self.fbdev = Some(f);
      }
      Err(e) => {
        eprintln!("[luna-compositor] software: cannot open fbdev {}: {}", path, e);
      }
    }
    self
  }

  fn write_ppm(&self, fb: &Framebuffer, path: &str) {
    let mut out = match File::create(path) {
      Ok(f) => std::io::BufWriter::new(f),
      Err(_) => return,
    };
    let _ = write!(out, "P6\n{} {}\n255\n", fb.width, fb.height);
    let mut row = Vec::with_capacity((fb.width * 3) as usize);
    for &px in &fb.pixels {
      row.push(((px >> 16) & 0xff) as u8);
      row.push(((px >> 8) & 0xff) as u8);
      row.push((px & 0xff) as u8);
    }
    let _ = out.write_all(&row);
  }

  fn write_fbdev(&mut self, fb: &Framebuffer) {
    let Some(f) = self.fbdev.as_mut() else { return };
    // Typical fbdev is 32bpp BGRA/XRGB; write 0xAARRGGBB little-endian.
    if self.fb_bpp == 32 {
      let mut bytes = Vec::with_capacity(fb.pixels.len() * 4);
      for &px in &fb.pixels {
        bytes.extend_from_slice(&px.to_le_bytes());
      }
      use std::io::Seek;
      let _ = f.seek(std::io::SeekFrom::Start(0));
      let _ = f.write_all(&bytes);
    }
  }
}

impl Backend for SoftwareBackend {
  fn size(&self) -> (u32, u32) { (self.width, self.height) }

  fn present(&mut self, fb: &Framebuffer) {
    self.frame += 1;
    if let Some(path) = self.screenshot_path.clone() {
      self.write_ppm(fb, &path);
    }
    self.write_fbdev(fb);
  }
}
