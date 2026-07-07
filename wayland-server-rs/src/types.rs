/*
 * Copyright © 2026 Yuichiro Nakada / Project Vespera
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */


use std::os::unix::io::RawFd;

pub type Fixed = i32;

#[inline]
pub fn fixed_to_f64(f: Fixed) -> f64 {
    f as f64 / 256.0
}

#[inline]
pub fn f64_to_fixed(d: f64) -> Fixed {
    (d * 256.0) as i32
}

#[derive(Debug, Clone)]
pub enum Arg {
    Int(i32),
    Uint(u32),
    Fixed(Fixed),
    Str(Option<String>),
    Object(u32),
    NewId(u32),
    Array(Vec<u8>),
    Fd(RawFd),
}

impl Arg {
    pub fn as_uint(&self) -> u32 {
        match self {
            Arg::Uint(v) | Arg::NewId(v) | Arg::Object(v) => *v,
            Arg::Int(v) => *v as u32,
            _ => 0,
        }
    }

    pub fn as_int(&self) -> i32 {
        match self {
            Arg::Int(v) | Arg::Fixed(v) => *v,
            Arg::Uint(v) | Arg::NewId(v) | Arg::Object(v) => *v as i32,
            _ => 0,
        }
    }

    pub fn as_object(&self) -> u32 {
        match self {
            Arg::Object(v) | Arg::NewId(v) | Arg::Uint(v) => *v,
            _ => 0,
        }
    }

    pub fn as_str(&self) -> Option<&str> {
        match self {
            Arg::Str(s) => s.as_deref(),
            _ => None,
        }
    }

    pub fn as_fd(&self) -> RawFd {
        match self {
            Arg::Fd(fd) => *fd,
            _ => -1,
        }
    }
}
