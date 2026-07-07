/*
 * Copyright © 2026 Yuichiro Nakada / Project Vespera
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */


pub mod software;

#[cfg(all(target_os = "linux", feature = "dri"))]
pub mod dri;

#[cfg(target_arch = "wasm32")]
pub mod webgl;

#[cfg(all(not(target_arch = "wasm32"), feature = "webgl"))]
pub mod webgl_server;

use crate::shm::{ShmBuffer, FORMAT_XRGB8888};

pub struct Framebuffer {
    pub width: u32,
    pub height: u32,
    pub pixels: Vec<u32>,
}

impl Framebuffer {
    pub fn new(width: u32, height: u32) -> Self {
        Framebuffer {
            width,
            height,
            pixels: vec![0xff10_1014; (width * height) as usize],
        }
    }

    pub fn clear(&mut self, argb: u32) {
        for p in self.pixels.iter_mut() {
            *p = argb;
        }
    }

    /// Blit SHM/dmabuf with clipping; dmabuf triggers DMA_BUF_IOCTL_SYNC around CPU reads.
    pub fn blit_shm(&mut self, buf: &ShmBuffer, dx: i32, dy: i32) {
        let src_x0 = (-dx).max(0);
        let src_y0 = (-dy).max(0);
        let dst_x0 = dx.max(0) as u32;
        let dst_y0 = dy.max(0) as u32;
        let copy_w = (buf.width - src_x0)
            .min(self.width as i32 - dst_x0 as i32)
            .max(0) as u32;
        let copy_h = (buf.height - src_y0)
            .min(self.height as i32 - dst_y0 as i32)
            .max(0) as u32;
        if copy_w == 0 || copy_h == 0 {
            return;
        }

        buf.begin_cpu_read();

        let stride = buf.stride as usize;
        let base_src = buf.offset + src_y0 as usize * stride + src_x0 as usize * 4;
        let opaque = buf.format == FORMAT_XRGB8888;

        for row in 0..copy_h as usize {
            let src_off = base_src + row * stride;
            let dst_start =
                (dst_y0 as usize + row) * self.width as usize + dst_x0 as usize;
            if let Some(src_row) = buf.pool.slice(src_off, copy_w as usize * 4) {
                let dst_row =
                    &mut self.pixels[dst_start..dst_start + copy_w as usize];
                if opaque {
                    // XRGB: opaque, byte swap only
                    for (i, dst) in dst_row.iter_mut().enumerate() {
                        let b = src_row[i * 4] as u32;
                        let g = src_row[i * 4 + 1] as u32;
                        let r = src_row[i * 4 + 2] as u32;
                        *dst = 0xff00_0000 | (r << 16) | (g << 8) | b;
                    }
                } else {
                    for (i, dst) in dst_row.iter_mut().enumerate() {
                        let v = u32::from_le_bytes([
                            src_row[i * 4],
                            src_row[i * 4 + 1],
                            src_row[i * 4 + 2],
                            src_row[i * 4 + 3],
                        ]);
                        *dst = blend(*dst, v);
                    }
                }
            }
        }

        buf.end_cpu_read();
    }
}

#[inline]
fn blend(dst: u32, src: u32) -> u32 {
    let a = (src >> 24) & 0xff;
    if a == 0xff {
        return src;
    }
    if a == 0 {
        return dst;
    }
    let ia = 255 - a;
    let mix = |sh: u32| {
        let s = (src >> sh) & 0xff;
        let d = (dst >> sh) & 0xff;
        ((s * a + d * ia) / 255) & 0xff
    };
    0xff00_0000 | (mix(16) << 16) | (mix(8) << 8) | mix(0)
}

#[cfg(not(target_arch = "wasm32"))]
#[derive(Debug, Clone)]
pub enum InputEvent {
    PointerMotion { x: f32, y: f32 },
    PointerButton { button: u32, pressed: bool },
    PointerAxis { axis: u32, value: f32 },
    Key { keycode: u32, pressed: bool },
}

pub trait Backend {
    fn size(&self) -> (u32, u32);
    fn present(&mut self, fb: &Framebuffer);
    /// Take input channel once (WebGL backend): (receiver, eventfd for epoll wakeup).
    #[cfg(not(target_arch = "wasm32"))]
    fn take_input_channel(
        &mut self,
    ) -> Option<(std::sync::mpsc::Receiver<InputEvent>, std::os::unix::io::RawFd)> {
        None
    }
}
