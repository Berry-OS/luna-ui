/*
 * Copyright © 2026 Yuichiro Nakada / Project Vespera
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

use crate::object::{DmabufParams, DmabufPlane, Object, Role, Surface};
use crate::protocol::{self, Interface};
use crate::render::{Backend, Framebuffer, InputEvent};
use crate::shell_ipc::ShellIpc;
use crate::shm::{ShmBuffer, ShmPool, FORMAT_ARGB8888, FORMAT_XRGB8888};
use crate::socket::{Conn, Listener};
use crate::types::Arg;
use crate::wire;
use std::collections::{HashMap, HashSet};
use std::ffi::CString;
use std::os::unix::io::RawFd;
use std::sync::mpsc;
use std::time::{SystemTime, UNIX_EPOCH};

// DRM fourcc / modifier for dmabuf
const DRM_FORMAT_ARGB8888: u32 = 0x3432_5241; // 'AR24'
const DRM_FORMAT_XRGB8888: u32 = 0x3432_5258; // 'XR24'
const DRM_FORMAT_MOD_LINEAR: u64 = 0;
const SERVER_ID_BASE: u32 = 0xff00_0000;

struct Global {
  name: u32,
  interface: &'static Interface,
  version: u32,
}

pub struct Client {
  pub conn: Conn,
  pub objects: HashMap<u32, Object>,
  server_next_id: u32,
}

impl Client {
  fn new(fd: RawFd) -> Self {
    let mut objects = HashMap::new();
    objects.insert(1, Object::new(&protocol::WL_DISPLAY, 1, Role::Display));
    Client {
      conn: Conn::new(fd),
      objects,
      server_next_id: SERVER_ID_BASE,
    }
  }

  fn alloc_server_id(&mut self) -> u32 {
    let id = self.server_next_id;
    self.server_next_id = self.server_next_id.wrapping_add(1);
    id
  }

  pub fn send(&mut self, object_id: u32, opcode: u16, args: &[Arg]) {
    let (payload, fds) = wire::encode_args(args);
    let msg = wire::build_message(object_id, opcode, &payload);
    self.conn.queue(&msg, &fds);
  }

  fn post_error(&mut self, object_id: u32, code: u32, msg: &str) {
    eprintln!("[luna-compositor] post_error obj={} code={} msg={:?}", object_id, code, msg);
    self.send(1, 0, &[Arg::Object(object_id), Arg::Uint(code), Arg::Str(Some(msg.to_string()))]);
    self.conn.flush();
    self.conn.closed = true;
  }
}

pub struct Server {
  listener: Listener,
  clients: HashMap<RawFd, Client>,
  globals: Vec<Global>,
  backend: Box<dyn Backend>,
  fb: Framebuffer,
  serial: u32,
  dirty: bool,
  frame_done: Vec<(RawFd, u32)>,
  epoll_fd: RawFd,
  signal_fd: RawFd,
  dmabuf_format_table: RawFd,
  dmabuf_format_table_size: usize,
  dmabuf_main_device: u64,

  input_rx: Option<mpsc::Receiver<InputEvent>>,
  input_wake_fd: RawFd,
  ptr_entered: bool,
  ptr_client_fd: RawFd,
  ptr_surface_id: u32,
  ptr_x: f32,
  ptr_y: f32,
  kbd_entered: bool,
  kbd_client_fd: RawFd,
  kbd_surface_id: u32,
  kbd_mods: u32,
  pressed_keys: HashSet<u32>,
  active_text_input: Option<(RawFd, u32, u32)>,

  shell_ipc: Option<ShellIpc>,
  focused_client_fd: RawFd,
  focused_surface_id: u32,
  shell_state_dirty: bool,
}

impl Server {
  pub fn new(socket_name: &str, backend: Box<dyn Backend>) -> std::io::Result<Self> {
    let listener = Listener::bind(socket_name)?;
    let (w, h) = backend.size();
    let epoll_fd = unsafe { libc::epoll_create1(libc::EPOLL_CLOEXEC) };
    if epoll_fd < 0 {
      return Err(std::io::Error::last_os_error());
    }
    let signal_fd = create_signal_fd()?;

    let (table_fd, table_size) = create_format_table();
    let main_device = detect_drm_device().unwrap_or(0);

    let mut server = Server {
      listener,
      clients: HashMap::new(),
      globals: Vec::new(),
      backend,
      fb: Framebuffer::new(w, h),
      serial: 1,
      dirty: true,
      frame_done: Vec::new(),
      epoll_fd,
      signal_fd,
      dmabuf_format_table: table_fd,
      dmabuf_format_table_size: table_size,
      dmabuf_main_device: main_device,

      input_rx: None,
      input_wake_fd: -1,
      ptr_entered: false,
      ptr_client_fd: -1,
      ptr_surface_id: 0,
      ptr_x: 0.5,
      ptr_y: 0.5,
      kbd_entered: false,
      kbd_client_fd: -1,
      kbd_surface_id: 0,
      kbd_mods: 0,
      pressed_keys: HashSet::new(),
      active_text_input: None,

      shell_ipc: ShellIpc::open(),
      focused_client_fd: -1,
      focused_surface_id: 0,
      shell_state_dirty: true,
    };

    if let Some(ref ipc) = server.shell_ipc {
      server.epoll_add(ipc.cmd_fd());
    }

    if let Some((rx, wake_fd)) = server.backend.take_input_channel() {
      server.input_rx = Some(rx);
      server.input_wake_fd = wake_fd;
      server.epoll_add(wake_fd);
    }

    server.epoll_add(server.listener.fd);
    server.epoll_add(server.signal_fd);

    // Advertise dmabuf only when feedback can be completed.  Mesa requests
    // both default and per-surface feedback, so a half-usable global makes
    // eglCreateWindowSurface fail instead of falling back to wl_shm.
    let mut advertised: Vec<(&'static Interface, u32)> = vec![
      (&protocol::WL_COMPOSITOR, 6),
      (&protocol::WL_SUBCOMPOSITOR, 1),
      (&protocol::WL_SHM, 1),
      (&protocol::WL_SEAT, 7),
      (&protocol::WL_OUTPUT, 4),
      (&protocol::WL_DATA_DEVICE_MANAGER, 3),
      (&protocol::ZWP_TEXT_INPUT_MANAGER_V3, 1),
      (&protocol::ZWP_INPUT_METHOD_MANAGER_V2, 1),
      (&protocol::ZWP_VIRTUAL_KEYBOARD_MANAGER_V1, 1),
      (&protocol::XDG_WM_BASE, 5),
    ];
    if server.dmabuf_format_table >= 0 && server.dmabuf_main_device != 0 {
      advertised.push((&protocol::ZWP_LINUX_DMABUF_V1, 4));
    } else {
      eprintln!("[luna-compositor] dmabuf feedback unavailable; using wl_shm");
    }
    for (i, (iface, ver)) in advertised.iter().enumerate() {
      server.globals.push(Global {
        name: (i + 1) as u32,
        interface: iface,
        version: *ver,
      });
    }

    Ok(server)
  }

  fn epoll_add(&self, fd: RawFd) {
    let mut ev = libc::epoll_event {
      events: libc::EPOLLIN as u32,
      u64: fd as u64,
    };
    unsafe { libc::epoll_ctl(self.epoll_fd, libc::EPOLL_CTL_ADD, fd, &mut ev) };
  }

  fn epoll_del(&self, fd: RawFd) { unsafe { libc::epoll_ctl(self.epoll_fd, libc::EPOLL_CTL_DEL, fd, std::ptr::null_mut()) }; }

  fn next_serial(&mut self) -> u32 {
    let s = self.serial;
    self.serial = self.serial.wrapping_add(1);
    s
  }

  pub fn run(&mut self) {
    let mut events = vec![libc::epoll_event { events: 0, u64: 0 }; 64];
    let mut running = true;
    while running {
      let timeout = if self.dirty { 0 } else { -1 };
      let n = unsafe { libc::epoll_wait(self.epoll_fd, events.as_mut_ptr(), events.len() as i32, timeout) };
      if n < 0 {
        let e = std::io::Error::last_os_error();
        if e.kind() == std::io::ErrorKind::Interrupted {
          continue;
        }
        break;
      }

      for ev in events.iter().take(n as usize) {
        let fd = ev.u64 as RawFd;
        if fd == self.listener.fd {
          self.accept_clients();
        } else if fd == self.signal_fd {
          running = self.process_signals();
          if !running {
            break;
          }
        } else if Some(fd) == self.shell_ipc.as_ref().map(|i| i.cmd_fd()) {
          self.handle_shell_commands();
        } else if fd == self.input_wake_fd && self.input_wake_fd >= 0 {
          let mut buf = [0u8; 8];
          unsafe { libc::read(self.input_wake_fd, buf.as_mut_ptr() as *mut libc::c_void, 8) };
          self.process_input_events();
        } else {
          self.recv_client(fd);
        }
      }

      if self.dirty {
        self.composite_and_present();
        self.dirty = false;
      }

      if self.shell_state_dirty {
        self.export_shell_state(false);
      }

      for c in self.clients.values_mut() {
        c.conn.flush();
      }
    }
  }

  fn accept_clients(&mut self) {
    loop {
      match self.listener.accept() {
        Ok(Some(fd)) => {
          self.epoll_add(fd);
          self.clients.insert(fd, Client::new(fd));
          eprintln!("[luna-compositor] client connected (fd={})", fd);
        }
        Ok(None) => break,
        Err(_) => break,
      }
    }
  }

  fn recv_client(&mut self, fd: RawFd) {
    let mut client = match self.clients.remove(&fd) {
      Some(c) => c,
      None => return,
    };

    let mut drop_client = false;
    loop {
      match client.conn.recv() {
        Ok(0) => break,
        Ok(_) => {}
        Err(_) => {
          drop_client = true;
          break;
        }
      }
    }

    while !client.conn.closed {
      let (msg, consumed) = match wire::decode_header(&client.conn.recv_buf) {
        Some(x) => x,
        None => break,
      };
      client.conn.recv_buf.drain(..consumed);
      self.handle_request(&mut client, msg);
    }

    if drop_client || client.conn.closed {
      self.epoll_del(fd);
      eprintln!("[luna-compositor] client disconnected (fd={})", fd);
      self.dirty = true;
      if self.ptr_client_fd == fd {
        self.ptr_entered = false;
        self.ptr_client_fd = -1;
      }
      if self.kbd_client_fd == fd {
        self.kbd_entered = false;
        self.kbd_client_fd = -1;
      }
      if self.active_text_input.map(|v| v.0) == Some(fd) {
        self.active_text_input = None;
        self.deactivate_input_methods(None);
      }
      if self.focused_client_fd == fd {
        self.focused_client_fd = -1;
        self.focused_surface_id = 0;
      }
      drop(client);
    } else {
      client.conn.flush();
      self.clients.insert(fd, client);
    }
  }

  fn handle_request(&mut self, client: &mut Client, msg: wire::RawMessage) {
    let iface = match client.objects.get(&msg.object_id) {
      Some(o) => o.interface,
      None => {
        eprintln!("[luna-compositor] unknown object id={} op={}", msg.object_id, msg.opcode);
        return;
      }
    };
    let sig = iface.request_sig(msg.opcode).unwrap_or("");
    let args = wire::decode_args(sig, &msg.payload, &mut client.conn.recv_fds);
    let id = msg.object_id;

    match iface.name {
      "wl_display" => self.req_display(client, id, msg.opcode, &args),
      "wl_registry" => self.req_registry(client, id, msg.opcode, &args),
      "wl_compositor" => self.req_compositor(client, msg.opcode, &args),
      "wl_subcompositor" => self.req_subcompositor(client, msg.opcode, &args),
      "wl_shm" => self.req_shm(client, msg.opcode, &args),
      "wl_shm_pool" => self.req_shm_pool(client, id, msg.opcode, &args),
      "wl_buffer" => self.req_simple_destroy(client, id, msg.opcode, 0),
      "wl_surface" => self.req_surface(client, id, msg.opcode, &args),
      "wl_region" => self.req_simple_destroy(client, id, msg.opcode, 0),
      "wl_seat" => self.req_seat(client, msg.opcode, &args),
      "wl_pointer" | "wl_keyboard" => self.req_input_device(client, id, msg.opcode),
      "wl_output" => {}
      "wl_data_device_manager" => self.req_ddm(client, msg.opcode, &args),
      "zwp_text_input_manager_v3" => self.req_text_input_manager(client, id, msg.opcode, &args),
      "zwp_text_input_v3" => self.req_text_input(client, id, msg.opcode, &args),
      "zwp_input_method_manager_v2" => self.req_input_method_manager(client, id, msg.opcode, &args),
      "zwp_input_method_v2" => self.req_input_method(client, id, msg.opcode, &args),
      "zwp_input_popup_surface_v2" => self.req_input_popup_surface(client, id, msg.opcode),
      "zwp_input_method_keyboard_grab_v2" => self.req_input_method_keyboard_grab(client, id, msg.opcode),
      "zwp_virtual_keyboard_manager_v1" => self.req_virtual_keyboard_manager(client, id, msg.opcode, &args),
      "zwp_virtual_keyboard_v1" => self.req_virtual_keyboard(client, id, msg.opcode, &args),
      "xdg_wm_base" => self.req_wm_base(client, id, msg.opcode, &args),
      "xdg_positioner" => self.req_positioner(client, id, msg.opcode, &args),
      "xdg_surface" => self.req_xdg_surface(client, id, msg.opcode, &args),
      "xdg_toplevel" => self.req_xdg_toplevel(client, id, msg.opcode, &args),
      "xdg_popup" => self.req_xdg_popup(client, id, msg.opcode, &args),
      "zwp_linux_dmabuf_v1" => self.req_dmabuf(client, id, msg.opcode, &args),
      "zwp_linux_buffer_params_v1" => self.req_dmabuf_params(client, id, msg.opcode, &args),
      "zwp_linux_dmabuf_feedback_v1" => self.req_simple_destroy(client, id, msg.opcode, 0),
      _ => {}
    }
  }

  fn req_display(&mut self, client: &mut Client, id: u32, opcode: u16, args: &[Arg]) {
    match opcode {
      0 => {
        let cb = args.get(0).map(|a| a.as_object()).unwrap_or(0);
        if cb == 0 {
          return;
        }
        client.objects.insert(cb, Object::new(&protocol::WL_CALLBACK, 1, Role::Callback));
        let serial = self.next_serial();
        client.send(cb, 0, &[Arg::Uint(serial)]);
        client.send(1, 1, &[Arg::Uint(cb)]);
        client.objects.remove(&cb);
      }
      1 => {
        let reg = args.get(0).map(|a| a.as_object()).unwrap_or(0);
        if reg == 0 {
          return;
        }
        client.objects.insert(reg, Object::new(&protocol::WL_REGISTRY, 1, Role::Registry));
        for g in &self.globals {
          client.send(reg, 0, &[Arg::Uint(g.name), Arg::Str(Some(g.interface.name.to_string())), Arg::Uint(g.version)]);
        }
      }
      _ => {
        let _ = id;
      }
    }
  }

  fn req_registry(&mut self, client: &mut Client, _id: u32, opcode: u16, args: &[Arg]) {
    if opcode != 0 {
      return;
    }
    let name = args.get(0).map(|a| a.as_uint()).unwrap_or(0);
    let iface_name = args.get(1).and_then(|a| a.as_str()).unwrap_or("");
    let req_ver = args.get(2).map(|a| a.as_uint()).unwrap_or(1);
    let new_id = args.get(3).map(|a| a.as_object()).unwrap_or(0);
    if new_id == 0 {
      return;
    }
    let global = self.globals.iter().find(|g| g.name == name);
    let iface = match (global, protocol::by_name(iface_name)) {
      (Some(g), _) => g.interface,
      (None, Some(i)) => i,
      _ => {
        client.post_error(1, 0, "bind: unknown global");
        return;
      }
    };
    let version = req_ver.min(iface.version);
    let role = role_for(iface.name);
    client.objects.insert(new_id, Object::new(iface, version, role));

    self.init_global_object(client, new_id, iface.name, version);
  }

  fn init_global_object(&mut self, client: &mut Client, id: u32, iface: &str, version: u32) {
    match iface {
      "wl_shm" => {
        client.send(id, 0, &[Arg::Uint(FORMAT_ARGB8888)]);
        client.send(id, 0, &[Arg::Uint(FORMAT_XRGB8888)]);
      }
      "wl_seat" => {
        client.send(id, 0, &[Arg::Uint(0x3)]);
        client.send(id, 1, &[Arg::Str(Some("seat0".into()))]);
      }
      "wl_output" => {
        let (w, h) = self.backend.size();
        client.send(id, 0, &[Arg::Int(0), Arg::Int(0), Arg::Int(300), Arg::Int(200), Arg::Int(0), Arg::Str(Some("berry-lab".into())), Arg::Str(Some("Vespera".into())), Arg::Int(0)]);
        client.send(id, 1, &[Arg::Uint(0x3), Arg::Int(w as i32), Arg::Int(h as i32), Arg::Int(60000)]);
        client.send(id, 3, &[Arg::Int(1)]);
        client.send(id, 2, &[]);
      }
      "zwp_linux_dmabuf_v1" => {
        // Pre-v4 clients learn formats via format/modifier events; v4+ uses get_default_feedback.
        if version < 4 {
          for &fmt in &[DRM_FORMAT_ARGB8888, DRM_FORMAT_XRGB8888] {
            client.send(id, 0, &[Arg::Uint(fmt)]);
            let m = DRM_FORMAT_MOD_LINEAR;
            client.send(
              id,
              1, // modifier(format, mod_hi, mod_lo)
              &[Arg::Uint(fmt), Arg::Uint((m >> 32) as u32), Arg::Uint(m as u32)],
            );
          }
        }
      }
      _ => {}
    }
  }

  fn req_compositor(&mut self, client: &mut Client, opcode: u16, args: &[Arg]) {
    match opcode {
      0 => {
        let nid = args.get(0).map(|a| a.as_object()).unwrap_or(0);
        if nid != 0 {
          client.objects.insert(nid, Object::new(&protocol::WL_SURFACE, 6, Role::Surface(Surface::default())));
        }
      }
      1 => {
        let nid = args.get(0).map(|a| a.as_object()).unwrap_or(0);
        if nid != 0 {
          client.objects.insert(nid, Object::new(&protocol::WL_REGION, 1, Role::Region));
        }
      }
      _ => {}
    }
  }

  fn req_subcompositor(&mut self, client: &mut Client, opcode: u16, args: &[Arg]) {
    if opcode == 1 {
      let nid = args.get(0).map(|a| a.as_object()).unwrap_or(0);
      let surf = args.get(1).map(|a| a.as_object()).unwrap_or(0);
      if nid != 0 {
        client.objects.insert(nid, Object::new(&protocol::WL_SUBSURFACE, 1, Role::Subsurface { surface_id: surf }));
      }
    }
  }

  fn req_shm(&mut self, client: &mut Client, opcode: u16, args: &[Arg]) {
    if opcode == 0 {
      let nid = args.get(0).map(|a| a.as_object()).unwrap_or(0);
      let fd = args.get(1).map(|a| a.as_fd()).unwrap_or(-1);
      let size = args.get(2).map(|a| a.as_int()).unwrap_or(0);
      let pool = ShmPool::map(fd, size.max(0) as usize);
      if nid != 0 {
        client.objects.insert(nid, Object::new(&protocol::WL_SHM_POOL, 1, Role::ShmPool { pool }));
      }
    }
  }

  fn req_shm_pool(&mut self, client: &mut Client, id: u32, opcode: u16, args: &[Arg]) {
    match opcode {
      0 => {
        let nid = args.get(0).map(|a| a.as_object()).unwrap_or(0);
        let offset = args.get(1).map(|a| a.as_int()).unwrap_or(0);
        let w = args.get(2).map(|a| a.as_int()).unwrap_or(0);
        let h = args.get(3).map(|a| a.as_int()).unwrap_or(0);
        let stride = args.get(4).map(|a| a.as_int()).unwrap_or(0);
        let format = args.get(5).map(|a| a.as_uint()).unwrap_or(0);

        let pool_rc = match client.objects.get(&id) {
          Some(Object {
            role: Role::ShmPool { pool: Some(p) },
            ..
          }) => p.clone(),
          _ => return,
        };
        if nid != 0 {
          let buf = ShmBuffer {
            pool: pool_rc,
            offset: offset.max(0) as usize,
            width: w,
            height: h,
            stride,
            format,
          };
          client.objects.insert(nid, Object::new(&protocol::WL_BUFFER, 1, Role::Buffer(buf)));
        }
      }
      1 => {
        client.objects.remove(&id);
      }
      _ => {}
    }
  }

  fn req_surface(&mut self, client: &mut Client, id: u32, opcode: u16, args: &[Arg]) {
    match opcode {
      0 => {
        if self.active_text_input.map(|v| (v.0, v.2)) == Some((client.conn.fd, id)) {
          self.active_text_input = None;
          self.deactivate_input_methods(None);
        }
        if self.focused_client_fd == client.conn.fd && self.focused_surface_id == id {
          self.focused_client_fd = -1;
          self.focused_surface_id = 0;
        }
        client.objects.remove(&id);
        self.dirty = true;
      }
      1 => {
        let buf = args.get(0).map(|a| a.as_object()).unwrap_or(0);
        if let Some(Object {
          role: Role::Surface(s),
          ..
        }) = client.objects.get_mut(&id)
        {
          s.pending_buffer = if buf == 0 { None } else { Some(buf) };
          s.pending_attach = true;
        }
      }
      3 => {
        let cb = args.get(0).map(|a| a.as_object()).unwrap_or(0);
        if cb != 0 {
          client.objects.insert(cb, Object::new(&protocol::WL_CALLBACK, 1, Role::Callback));
          if let Some(Object {
            role: Role::Surface(s),
            ..
          }) = client.objects.get_mut(&id)
          {
            s.frame_callbacks.push(cb);
          }
        }
      }
      6 => self.surface_commit(client, id),
      _ => {}
    }
  }

  fn surface_commit(&mut self, client: &mut Client, id: u32) {
    let (attach, pending_buf, frames) = match client.objects.get_mut(&id) {
      Some(Object {
        role: Role::Surface(s),
        ..
      }) => {
        let frames = std::mem::take(&mut s.frame_callbacks);
        (s.pending_attach, s.pending_buffer, frames)
      }
      _ => return,
    };

    let new_buffer = if attach {
      match pending_buf {
        None => None,
        Some(bid) => match client.objects.get(&bid) {
          Some(Object {
            role: Role::Buffer(b),
            ..
          }) => {
            let clone = b.clone();
            client.send(bid, 0, &[]);
            Some(clone)
          }
          _ => None,
        },
      }
    } else {
      None
    };

    if let Some(Object {
      role: Role::Surface(s),
      ..
    }) = client.objects.get_mut(&id)
    {
      if attach {
        s.current_buffer = new_buffer;
        s.mapped = s.current_buffer.is_some();
      }
      s.pending_attach = false;
      s.pending_buffer = None;
    }

    let fd = client.conn.fd;
    for cb in frames {
      self.frame_done.push((fd, cb));
    }

    if let Some(popup_id) = client.objects.iter().find_map(|(&oid, obj)| match obj.role {
      Role::InputPopupSurface { surface_id, .. } if surface_id == id => Some(oid),
      _ => None,
    }) {
      self.position_one_input_method_popup(client, popup_id);
    }
    self.dirty = true;
    self.shell_state_dirty = true;
  }

  fn surface_is_minimized(&self, client: &Client, surface_id: u32) -> bool {
    let Some(Object {
      role: Role::Surface(s),
      ..
    }) = client.objects.get(&surface_id)
    else {
      return false;
    };
    let Some(xdg_id) = s.xdg_surface_id else { return false };
    for obj in client.objects.values() {
      if let Role::XdgToplevel {
        xdg_surface_id,
        minimized,
        ..
      } = &obj.role
      {
        if *xdg_surface_id == xdg_id {
          return *minimized;
        }
      }
    }
    false
  }

  fn export_shell_state(&mut self, force: bool) {
    if let Some(ref mut ipc) = self.shell_ipc {
      ipc.export_state(&self.clients, self.focused_surface_id, force);
    }
    self.shell_state_dirty = false;
  }

  fn handle_shell_commands(&mut self) {
    let cmds = match self.shell_ipc.as_mut() {
      Some(ipc) => ipc.accept_commands(),
      None => return,
    };
    for cmd in cmds {
      if cmd.starts_with("tray_") {
        if let Some(ref mut ipc) = self.shell_ipc {
          ipc.handle_tray_command(&cmd);
        }
        self.shell_state_dirty = true;
        continue;
      }
      let mut parts = cmd.split_whitespace();
      match (parts.next(), parts.next()) {
        (Some("activate"), Some(id)) => {
          if let Ok(sid) = id.parse::<u32>() {
            self.activate_surface(sid);
          }
        }
        (Some("minimize"), Some(id)) => {
          if let Ok(sid) = id.parse::<u32>() {
            self.minimize_surface(sid);
          }
        }
        (Some("close"), Some(id)) => {
          if let Ok(sid) = id.parse::<u32>() {
            self.close_surface(sid);
          }
        }
        _ => {}
      }
    }
  }

  fn client_for_surface(&self, surface_id: u32) -> Option<(RawFd, &Client)> {
    for (&fd, client) in &self.clients {
      if client.objects.contains_key(&surface_id) {
        return Some((fd, client));
      }
    }
    None
  }

  fn activate_surface(&mut self, surface_id: u32) {
    let Some((fd, client)) = self.client_for_surface(surface_id) else { return };
    let xdg_id = match client.objects.get(&surface_id) {
      Some(Object {
        role: Role::Surface(s),
        ..
      }) => s.xdg_surface_id,
      _ => None,
    };
    if let Some(xdg_id) = xdg_id {
      for obj in self.clients.get_mut(&fd).unwrap().objects.values_mut() {
        if let Role::XdgToplevel {
          xdg_surface_id,
          minimized,
          ..
        } = &mut obj.role
        {
          if *xdg_surface_id == xdg_id {
            *minimized = false;
          }
        }
      }
    }
    self.focused_client_fd = fd;
    self.focused_surface_id = surface_id;
    self.ptr_entered = false;
    self.kbd_entered = false;
    self.dirty = true;
    self.shell_state_dirty = true;
    self.update_text_input_focus(fd, surface_id);
  }

  fn minimize_surface(&mut self, surface_id: u32) {
    let Some((fd, _)) = self.client_for_surface(surface_id) else { return };
    let xdg_id = match self.clients.get(&fd).unwrap().objects.get(&surface_id) {
      Some(Object {
        role: Role::Surface(s),
        ..
      }) => s.xdg_surface_id,
      _ => None,
    };
    if let Some(xdg_id) = xdg_id {
      for obj in self.clients.get_mut(&fd).unwrap().objects.values_mut() {
        if let Role::XdgToplevel {
          xdg_surface_id,
          minimized,
          ..
        } = &mut obj.role
        {
          if *xdg_surface_id == xdg_id {
            *minimized = true;
          }
        }
      }
    }
    if self.focused_client_fd == fd && self.focused_surface_id == surface_id {
      self.focused_client_fd = -1;
      self.focused_surface_id = 0;
      self.ptr_entered = false;
      self.kbd_entered = false;
      self.clear_text_input_focus();
    }
    self.dirty = true;
    self.shell_state_dirty = true;
  }

  fn close_surface(&mut self, surface_id: u32) {
    let Some((fd, client)) = self.client_for_surface(surface_id) else { return };
    let xdg_id = match client.objects.get(&surface_id) {
      Some(Object {
        role: Role::Surface(s),
        ..
      }) => s.xdg_surface_id,
      _ => None,
    };
    let toplevel_id = xdg_id.and_then(|xid| {
      client.objects.iter().find_map(|(&oid, obj)| {
        if let Role::XdgToplevel { xdg_surface_id, .. } = &obj.role {
          if *xdg_surface_id == xid {
            return Some(oid);
          }
        }
        None
      })
    });
    if let Some(tid) = toplevel_id {
      if let Some(client) = self.clients.get_mut(&fd) {
        client.send(tid, 0, &[]); // xdg_toplevel.close
        client.conn.flush();
      }
    }
    self.shell_state_dirty = true;
  }

  fn req_seat(&mut self, client: &mut Client, opcode: u16, args: &[Arg]) {
    let iface = match opcode {
      0 => &protocol::WL_POINTER,
      1 => &protocol::WL_KEYBOARD,
      _ => return,
    };
    let nid = args.get(0).map(|a| a.as_object()).unwrap_or(0);
    if nid != 0 {
      let role = if opcode == 0 { Role::Pointer } else { Role::Keyboard };
      client.objects.insert(nid, Object::new(iface, iface.version, role));

      // GTK4 crashes without a wl_keyboard keymap event.
      if opcode == 1 {
        self.send_keymap(client, nid);
      }
    }
  }

  fn send_keymap(&self, client: &mut Client, keyboard_id: u32) {
    const KEYMAP: &str = concat!(
      "xkb_keymap {\n",
      "  xkb_keycodes  \"evdev+aliases(qwerty)\" { include \"evdev+aliases(qwerty)\" };\n",
      "  xkb_types     \"complete\" { include \"complete\" };\n",
      "  xkb_compat    \"complete\" { include \"complete\" };\n",
      "  xkb_symbols   \"pc+us+inet(evdev)\" { include \"pc+us+inet(evdev)\" };\n",
      "};\n",
      "\0"
    );
    let data = KEYMAP.as_bytes();
    let name = std::ffi::CString::new("xkb-keymap").unwrap();
    let fd = unsafe { libc::memfd_create(name.as_ptr(), libc::MFD_CLOEXEC | libc::MFD_ALLOW_SEALING) };
    if fd < 0 {
      return;
    }
    let mut off = 0usize;
    while off < data.len() {
      let n = unsafe { libc::write(fd, data[off..].as_ptr() as *const libc::c_void, data.len() - off) };
      if n <= 0 {
        unsafe { libc::close(fd) };
        return;
      }
      off += n as usize;
    }
    // fd is closed after SCM_RIGHTS send in flush().
    client.send(keyboard_id, 0, &[Arg::Uint(1), Arg::Fd(fd), Arg::Uint(data.len() as u32)]);

    let repeat_opcode = match client.objects.get(&keyboard_id) {
      Some(Object {
        role: Role::InputMethodKeyboardGrab { .. },
        ..
      }) => 3,
      _ => 5,
    };
    client.send(keyboard_id, repeat_opcode, &[Arg::Int(0), Arg::Int(0)]);
  }

  fn req_input_device(&mut self, client: &mut Client, id: u32, opcode: u16) {
    let is_release = match client.objects.get(&id).map(|o| o.interface.name) {
      Some("wl_pointer") => opcode == 1,
      Some("wl_keyboard") => opcode == 0,
      _ => false,
    };
    if is_release {
      client.objects.remove(&id);
    }
  }

  fn req_ddm(&mut self, client: &mut Client, opcode: u16, args: &[Arg]) {
    let nid = args.get(0).map(|a| a.as_object()).unwrap_or(0);
    if nid == 0 {
      return;
    }
    let role = if opcode == 0 { Role::DataSource } else { Role::DataDevice };
    client.objects.insert(nid, Object::new(&protocol::WL_DATA_DEVICE_MANAGER, 3, role));
  }

  fn req_text_input_manager(&mut self, client: &mut Client, id: u32, opcode: u16, args: &[Arg]) {
    match opcode {
      0 => {
        client.objects.remove(&id);
      }
      1 => {
        let nid = args.get(0).map(|a| a.as_object()).unwrap_or(0);
        let seat_id = args.get(1).map(|a| a.as_object()).unwrap_or(0);
        if nid == 0 {
          return;
        }
        let surface_id = if self.focused_client_fd == client.conn.fd && self.focused_surface_id != 0 && client.objects.contains_key(&self.focused_surface_id) { Some(self.focused_surface_id) } else { None };
        client.objects.insert(
          nid,
          Object::new(
            &protocol::ZWP_TEXT_INPUT_V3,
            1,
            Role::TextInput {
              seat_id,
              surface_id,
              enabled: false,
              pending_enabled: None,
              surrounding_text: String::new(),
              cursor: 0,
              anchor: 0,
              text_change_cause: 0,
              content_hint: 0,
              content_purpose: 0,
              cursor_rect: (0, 0, 0, 0),
              commit_serial: 0,
            },
          ),
        );
        if let Some(surface_id) = surface_id {
          client.send(nid, 0, &[Arg::Object(surface_id)]);
        }
      }
      _ => {}
    }
  }

  fn req_text_input(&mut self, client: &mut Client, id: u32, opcode: u16, args: &[Arg]) {
    if opcode == 0 {
      if self.active_text_input.map(|v| (v.0, v.1)) == Some((client.conn.fd, id)) {
        self.active_text_input = None;
        self.deactivate_input_methods(None);
      }
      client.objects.remove(&id);
      return;
    }

    let mut commit = false;
    if let Some(Object {
      role: Role::TextInput {
        pending_enabled,
        surrounding_text,
        cursor,
        anchor,
        text_change_cause,
        content_hint,
        content_purpose,
        cursor_rect,
        ..
      },
      ..
    }) = client.objects.get_mut(&id)
    {
      match opcode {
        1 => *pending_enabled = Some(true),
        2 => *pending_enabled = Some(false),
        3 => {
          *surrounding_text = args.get(0).and_then(|a| a.as_str()).unwrap_or("").to_string();
          *cursor = args.get(1).map(|a| a.as_int()).unwrap_or(0);
          *anchor = args.get(2).map(|a| a.as_int()).unwrap_or(0);
        }
        4 => *text_change_cause = args.get(0).map(|a| a.as_uint()).unwrap_or(0),
        5 => {
          *content_hint = args.get(0).map(|a| a.as_uint()).unwrap_or(0);
          *content_purpose = args.get(1).map(|a| a.as_uint()).unwrap_or(0);
        }
        6 => *cursor_rect = (args.get(0).map(|a| a.as_int()).unwrap_or(0), args.get(1).map(|a| a.as_int()).unwrap_or(0), args.get(2).map(|a| a.as_int()).unwrap_or(0), args.get(3).map(|a| a.as_int()).unwrap_or(0)),
        7 => commit = true,
        _ => {}
      }
    }
    if !commit {
      return;
    }

    let (surface_id, enabled) = match client.objects.get_mut(&id) {
      Some(Object {
        role: Role::TextInput {
          surface_id,
          enabled,
          pending_enabled,
          commit_serial,
          ..
        },
        ..
      }) => {
        if let Some(value) = pending_enabled.take() {
          *enabled = value;
        }
        *commit_serial = commit_serial.wrapping_add(1);
        (*surface_id, *enabled)
      }
      _ => return,
    };
    let focused = self.focused_client_fd == client.conn.fd && surface_id == Some(self.focused_surface_id);
    if enabled && focused {
      let old = self.active_text_input.replace((client.conn.fd, id, surface_id.unwrap_or(0)));
      if old.map(|v| (v.0, v.1)) != Some((client.conn.fd, id)) {
        if old.is_some() {
          self.deactivate_input_methods(None);
        }
        self.activate_input_methods(client, id);
      } else {
        self.send_text_state_to_input_methods(client, id);
      }
      self.position_input_method_popups_for(client, id);
    } else if self.active_text_input.map(|v| (v.0, v.1)) == Some((client.conn.fd, id)) {
      self.active_text_input = None;
      self.deactivate_input_methods(None);
    }
  }

  fn req_input_method_manager(&mut self, client: &mut Client, id: u32, opcode: u16, args: &[Arg]) {
    match opcode {
      0 => {
        // input-method-v2 declares get_input_method(seat, new_id), unlike
        // text-input-v3's get_text_input(new_id, seat).
        let seat_id = args.get(0).map(|a| a.as_object()).unwrap_or(0);
        let nid = args.get(1).map(|a| a.as_object()).unwrap_or(0);
        if nid == 0 {
          return;
        }
        client.objects.insert(
          nid,
          Object::new(
            &protocol::ZWP_INPUT_METHOD_V2,
            1,
            Role::InputMethod {
              seat_id,
              pending_commit: None,
              pending_preedit: None,
              pending_delete: None,
            },
          ),
        );
        if let Some((target_fd, target_id, _)) = self.active_text_input {
          client.send(nid, 0, &[]);
          if let Some(target) = self.clients.get(&target_fd) {
            self.send_text_state(client, nid, target, target_id);
          }
        }
      }
      1 => {
        client.objects.remove(&id);
      }
      _ => {}
    }
  }

  fn req_input_method(&mut self, client: &mut Client, id: u32, opcode: u16, args: &[Arg]) {
    match opcode {
      0 => {
        if let Some(Object {
          role: Role::InputMethod { pending_commit, .. },
          ..
        }) = client.objects.get_mut(&id)
        {
          *pending_commit = Some(args.get(0).and_then(|a| a.as_str()).unwrap_or("").to_string());
        }
      }
      1 => {
        if let Some(Object {
          role: Role::InputMethod {
            pending_preedit, ..
          },
          ..
        }) = client.objects.get_mut(&id)
        {
          *pending_preedit = Some((args.get(0).and_then(|a| a.as_str()).unwrap_or("").to_string(), args.get(1).map(|a| a.as_int()).unwrap_or(0), args.get(2).map(|a| a.as_int()).unwrap_or(0)));
        }
      }
      2 => {
        if let Some(Object {
          role: Role::InputMethod { pending_delete, .. },
          ..
        }) = client.objects.get_mut(&id)
        {
          *pending_delete = Some((args.get(0).map(|a| a.as_uint()).unwrap_or(0), args.get(1).map(|a| a.as_uint()).unwrap_or(0)));
        }
      }
      3 => self.forward_input_method_commit(client, id),
      4 => {
        let nid = args.get(0).map(|a| a.as_object()).unwrap_or(0);
        let surface_id = args.get(1).map(|a| a.as_object()).unwrap_or(0);
        if nid == 0 || surface_id == 0 {
          return;
        }
        if let Some(Object {
          role: Role::Surface(surface),
          ..
        }) = client.objects.get_mut(&surface_id)
        {
          surface.input_method_popup = true;
          surface.popup = true;
        } else {
          client.post_error(id, 0, "input popup requires a wl_surface");
          return;
        }
        client.objects.insert(
          nid,
          Object::new(
            &protocol::ZWP_INPUT_POPUP_SURFACE_V2,
            1,
            Role::InputPopupSurface {
              surface_id,
              input_method_id: id,
            },
          ),
        );
        self.position_one_input_method_popup(client, nid);
      }
      5 => {
        let nid = args.get(0).map(|a| a.as_object()).unwrap_or(0);
        if nid != 0 {
          client.objects.insert(
            nid,
            Object::new(
              &protocol::ZWP_INPUT_METHOD_KEYBOARD_GRAB_V2,
              1,
              Role::InputMethodKeyboardGrab {
                input_method_id: id,
              },
            ),
          );
          self.send_keymap(client, nid);
        }
      }
      6 => {
        let popup_ids: Vec<u32> = client
          .objects
          .iter()
          .filter_map(|(&oid, obj)| match obj.role {
            Role::InputPopupSurface {
              input_method_id, ..
            } if input_method_id == id => Some(oid),
            _ => None,
          })
          .collect();
        for popup_id in popup_ids {
          self.req_input_popup_surface(client, popup_id, 0);
        }
        client.objects.remove(&id);
      }
      _ => {}
    }
  }

  fn req_input_popup_surface(&mut self, client: &mut Client, id: u32, opcode: u16) {
    if opcode != 0 {
      return;
    }
    let surface_id = match client.objects.get(&id) {
      Some(Object {
        role: Role::InputPopupSurface { surface_id, .. },
        ..
      }) => *surface_id,
      _ => return,
    };
    if let Some(Object {
      role: Role::Surface(surface),
      ..
    }) = client.objects.get_mut(&surface_id)
    {
      surface.input_method_popup = false;
      surface.popup = false;
      surface.mapped = false;
    }
    client.objects.remove(&id);
    self.dirty = true;
  }

  fn req_input_method_keyboard_grab(&mut self, client: &mut Client, id: u32, opcode: u16) {
    if opcode == 0 {
      client.objects.remove(&id);
    }
  }

  fn req_virtual_keyboard_manager(&mut self, client: &mut Client, id: u32, opcode: u16, args: &[Arg]) {
    match opcode {
      0 => {
        let authorized = client.objects.values().any(|obj| matches!(obj.role, Role::InputMethod { .. }));
        if !authorized {
          client.post_error(id, 0, "virtual keyboard requires an input-method object");
          return;
        }
        let seat_id = args.get(0).map(|a| a.as_object()).unwrap_or(0);
        let new_id = args.get(1).map(|a| a.as_object()).unwrap_or(0);
        if new_id == 0 {
          return;
        }
        client.objects.insert(
          new_id,
          Object::new(
            &protocol::ZWP_VIRTUAL_KEYBOARD_V1,
            1,
            Role::VirtualKeyboard {
              seat_id,
              keymap_set: false,
            },
          ),
        );
      }
      1 => {
        client.objects.remove(&id);
      }
      _ => {}
    }
  }

  fn req_virtual_keyboard(&mut self, client: &mut Client, id: u32, opcode: u16, args: &[Arg]) {
    match opcode {
      0 => {
        let fd = args.get(1).map(|a| a.as_fd()).unwrap_or(-1);
        if fd >= 0 {
          unsafe { libc::close(fd) };
        }
        if let Some(Object {
          role: Role::VirtualKeyboard { keymap_set, .. },
          ..
        }) = client.objects.get_mut(&id)
        {
          *keymap_set = true;
        }
      }
      1 => {
        if !Self::virtual_keyboard_has_keymap(client, id) {
          client.post_error(id, 0, "virtual keyboard key sent before keymap");
          return;
        }
        let time = args.get(0).map(|a| a.as_uint()).unwrap_or(0);
        let key = args.get(1).map(|a| a.as_uint()).unwrap_or(0);
        let state = args.get(2).map(|a| a.as_uint()).unwrap_or(0);
        self.forward_virtual_key(client, time, key, state);
      }
      2 => {
        if !Self::virtual_keyboard_has_keymap(client, id) {
          client.post_error(id, 0, "virtual keyboard modifiers sent before keymap");
          return;
        }
        let depressed = args.get(0).map(|a| a.as_uint()).unwrap_or(0);
        let latched = args.get(1).map(|a| a.as_uint()).unwrap_or(0);
        let locked = args.get(2).map(|a| a.as_uint()).unwrap_or(0);
        let group = args.get(3).map(|a| a.as_uint()).unwrap_or(0);
        self.forward_virtual_modifiers(client, depressed, latched, locked, group);
      }
      3 => {
        client.objects.remove(&id);
      }
      _ => {}
    }
  }

  fn virtual_keyboard_has_keymap(client: &Client, id: u32) -> bool {
    matches!(
      client.objects.get(&id),
      Some(Object {
        role: Role::VirtualKeyboard {
          keymap_set: true,
          ..
        },
        ..
      })
    )
  }

  fn keyboard_id(client: &Client) -> Option<u32> { client.objects.iter().find_map(|(&id, obj)| matches!(obj.role, Role::Keyboard).then_some(id)) }

  fn forward_virtual_key(&mut self, virtual_client: &mut Client, time: u32, key: u32, state: u32) {
    let target_fd = self.focused_client_fd;
    let surface_id = self.focused_surface_id;
    if target_fd < 0 || surface_id == 0 {
      return;
    }
    let serial = self.next_serial();
    if target_fd == virtual_client.conn.fd {
      if let Some(keyboard_id) = Self::keyboard_id(virtual_client) {
        virtual_client.send(keyboard_id, 3, &[Arg::Uint(serial), Arg::Uint(time), Arg::Uint(key), Arg::Uint(state)]);
      }
      return;
    }
    self.ensure_kbd_entered(target_fd, surface_id);
    if let Some(target) = self.clients.get_mut(&target_fd) {
      if let Some(keyboard_id) = Self::keyboard_id(target) {
        target.send(keyboard_id, 3, &[Arg::Uint(serial), Arg::Uint(time), Arg::Uint(key), Arg::Uint(state)]);
        target.conn.flush();
      }
    }
  }

  fn forward_virtual_modifiers(&mut self, virtual_client: &mut Client, depressed: u32, latched: u32, locked: u32, group: u32) {
    let target_fd = self.focused_client_fd;
    let surface_id = self.focused_surface_id;
    if target_fd < 0 || surface_id == 0 {
      return;
    }
    self.kbd_mods = depressed;
    let serial = self.next_serial();
    if target_fd == virtual_client.conn.fd {
      if let Some(keyboard_id) = Self::keyboard_id(virtual_client) {
        virtual_client.send(keyboard_id, 4, &[Arg::Uint(serial), Arg::Uint(depressed), Arg::Uint(latched), Arg::Uint(locked), Arg::Uint(group)]);
      }
      return;
    }
    self.ensure_kbd_entered(target_fd, surface_id);
    if let Some(target) = self.clients.get_mut(&target_fd) {
      if let Some(keyboard_id) = Self::keyboard_id(target) {
        target.send(keyboard_id, 4, &[Arg::Uint(serial), Arg::Uint(depressed), Arg::Uint(latched), Arg::Uint(locked), Arg::Uint(group)]);
        target.conn.flush();
      }
    }
  }

  fn text_input_state(client: &Client, id: u32) -> Option<(String, u32, u32, u32, u32, u32, (i32, i32, i32, i32))> {
    match client.objects.get(&id) {
      Some(Object {
        role: Role::TextInput {
          surrounding_text,
          cursor,
          anchor,
          text_change_cause,
          content_hint,
          content_purpose,
          cursor_rect,
          ..
        },
        ..
      }) => Some((surrounding_text.clone(), (*cursor).max(0) as u32, (*anchor).max(0) as u32, *text_change_cause, *content_hint, *content_purpose, *cursor_rect)),
      _ => None,
    }
  }

  fn send_text_state(&self, im: &mut Client, im_id: u32, target: &Client, target_id: u32) {
    let Some((text, cursor, anchor, cause, hint, purpose, _)) = Self::text_input_state(target, target_id) else {
      return;
    };
    im.send(im_id, 2, &[Arg::Str(Some(text)), Arg::Uint(cursor), Arg::Uint(anchor)]);
    im.send(im_id, 3, &[Arg::Uint(cause)]);
    im.send(im_id, 4, &[Arg::Uint(hint), Arg::Uint(purpose)]);
    im.send(im_id, 5, &[]);
  }

  fn activate_input_methods(&mut self, target: &Client, target_id: u32) {
    let Some((text, cursor, anchor, cause, hint, purpose, _)) = Self::text_input_state(target, target_id) else {
      return;
    };
    for im in self.clients.values_mut() {
      let ids: Vec<u32> = im.objects.iter().filter_map(|(&oid, obj)| matches!(obj.role, Role::InputMethod { .. }).then_some(oid)).collect();
      for im_id in ids {
        im.send(im_id, 0, &[]);
        im.send(im_id, 2, &[Arg::Str(Some(text.clone())), Arg::Uint(cursor), Arg::Uint(anchor)]);
        im.send(im_id, 3, &[Arg::Uint(cause)]);
        im.send(im_id, 4, &[Arg::Uint(hint), Arg::Uint(purpose)]);
        im.send(im_id, 5, &[]);
      }
    }
  }

  fn send_text_state_to_input_methods(&mut self, target: &Client, target_id: u32) {
    let Some((text, cursor, anchor, cause, hint, purpose, _)) = Self::text_input_state(target, target_id) else {
      return;
    };
    for im in self.clients.values_mut() {
      let ids: Vec<u32> = im.objects.iter().filter_map(|(&oid, obj)| matches!(obj.role, Role::InputMethod { .. }).then_some(oid)).collect();
      for im_id in ids {
        im.send(im_id, 2, &[Arg::Str(Some(text.clone())), Arg::Uint(cursor), Arg::Uint(anchor)]);
        im.send(im_id, 3, &[Arg::Uint(cause)]);
        im.send(im_id, 4, &[Arg::Uint(hint), Arg::Uint(purpose)]);
        im.send(im_id, 5, &[]);
      }
    }
  }

  fn deactivate_input_methods(&mut self, _current: Option<&mut Client>) {
    for im in self.clients.values_mut() {
      let ids: Vec<u32> = im.objects.iter().filter_map(|(&oid, obj)| matches!(obj.role, Role::InputMethod { .. }).then_some(oid)).collect();
      for id in ids {
        im.send(id, 1, &[]);
        im.send(id, 5, &[]);
      }
    }
    self.dirty = true;
  }

  fn forward_input_method_commit(&mut self, im: &mut Client, im_id: u32) {
    let (commit, preedit, delete) = match im.objects.get_mut(&im_id) {
      Some(Object {
        role: Role::InputMethod {
          pending_commit,
          pending_preedit,
          pending_delete,
          ..
        },
        ..
      }) => (pending_commit.take(), pending_preedit.take(), pending_delete.take()),
      _ => return,
    };
    let Some((target_fd, target_id, _)) = self.active_text_input else {
      return;
    };
    let serial = if target_fd == im.conn.fd {
      Self::forward_to_text_input(im, target_id, commit, preedit, delete)
    } else if let Some(target) = self.clients.get_mut(&target_fd) {
      Self::forward_to_text_input(target, target_id, commit, preedit, delete)
    } else {
      None
    };
    if serial.is_none() {
      self.active_text_input = None;
    }
  }

  fn forward_to_text_input(target: &mut Client, target_id: u32, commit: Option<String>, preedit: Option<(String, i32, i32)>, delete: Option<(u32, u32)>) -> Option<u32> {
    let serial = match target.objects.get(&target_id) {
      Some(Object {
        role: Role::TextInput { commit_serial, .. },
        ..
      }) => *commit_serial,
      _ => return None,
    };
    if let Some((text, begin, end)) = preedit {
      target.send(target_id, 2, &[Arg::Str(Some(text)), Arg::Int(begin), Arg::Int(end)]);
    }
    if let Some(text) = commit {
      target.send(target_id, 3, &[Arg::Str(Some(text))]);
    }
    if let Some((before, after)) = delete {
      target.send(target_id, 4, &[Arg::Uint(before), Arg::Uint(after)]);
    }
    target.send(target_id, 5, &[Arg::Uint(serial)]);
    Some(serial)
  }

  fn text_input_screen_rect(&self, target: &Client, target_id: u32) -> Option<(i32, i32, i32, i32)> {
    let (_, _, surface_id) = self.active_text_input?;
    let (_, _, _, _, _, _, (x, y, w, h)) = Self::text_input_state(target, target_id)?;
    let surface = match target.objects.get(&surface_id) {
      Some(Object {
        role: Role::Surface(surface),
        ..
      }) => surface,
      _ => return None,
    };
    let buffer = surface.current_buffer.as_ref()?;
    let (bw, bh) = self.backend.size();
    let ox = ((bw as i32 - buffer.width) / 2).max(0) + surface.x;
    let oy = ((bh as i32 - buffer.height) / 2).max(0) + surface.y;
    Some((ox + x, oy + y, w, h))
  }

  fn position_input_method_popups_for(&mut self, target: &Client, target_id: u32) {
    let Some(rect) = self.text_input_screen_rect(target, target_id) else {
      return;
    };
    let (bw, bh) = self.backend.size();
    for im in self.clients.values_mut() {
      Self::position_popups_in_client(im, rect, bw as i32, bh as i32);
    }
    self.dirty = true;
  }

  fn position_one_input_method_popup(&mut self, im: &mut Client, popup_id: u32) {
    let Some((target_fd, target_id, _)) = self.active_text_input else {
      return;
    };
    let Some(target) = self.clients.get(&target_fd) else {
      return;
    };
    let Some(rect) = self.text_input_screen_rect(target, target_id) else {
      return;
    };
    if !im.objects.contains_key(&popup_id) {
      return;
    }
    let (bw, bh) = self.backend.size();
    Self::position_popups_in_client(im, rect, bw as i32, bh as i32);
    self.dirty = true;
  }

  fn position_popups_in_client(im: &mut Client, rect: (i32, i32, i32, i32), output_w: i32, output_h: i32) {
    let popups: Vec<(u32, u32)> = im
      .objects
      .iter()
      .filter_map(|(&oid, obj)| match obj.role {
        Role::InputPopupSurface { surface_id, .. } => Some((oid, surface_id)),
        _ => None,
      })
      .collect();
    for (popup_id, surface_id) in popups {
      let mut popup_origin = (rect.0, rect.1 + rect.3);
      if let Some(Object {
        role: Role::Surface(surface),
        ..
      }) = im.objects.get_mut(&surface_id)
      {
        let (popup_w, popup_h) = surface.current_buffer.as_ref().map(|b| (b.width, b.height)).unwrap_or((0, 0));
        let below = rect.1 + rect.3;
        surface.x = rect.0.clamp(0, (output_w - popup_w).max(0));
        surface.y = if below + popup_h <= output_h { below } else { (rect.1 - popup_h).max(0) };
        popup_origin = (surface.x, surface.y);
      }
      im.send(popup_id, 0, &[Arg::Int(rect.0 - popup_origin.0), Arg::Int(rect.1 - popup_origin.1), Arg::Int(rect.2), Arg::Int(rect.3)]);
    }
  }

  fn req_wm_base(&mut self, client: &mut Client, id: u32, opcode: u16, args: &[Arg]) {
    match opcode {
      0 => {
        client.objects.remove(&id);
      }
      1 => {
        let nid = args.get(0).map(|a| a.as_object()).unwrap_or(0);
        if nid != 0 {
          client.objects.insert(
            nid,
            Object::new(
              &protocol::XDG_POSITIONER,
              5,
              Role::Positioner {
                size_w: 0,
                size_h: 0,
                anchor_x: 0,
                anchor_y: 0,
                anchor_w: 0,
                anchor_h: 0,
                offset_x: 0,
                offset_y: 0,
              },
            ),
          );
        }
      }
      2 => {
        let nid = args.get(0).map(|a| a.as_object()).unwrap_or(0);
        let surf = args.get(1).map(|a| a.as_object()).unwrap_or(0);
        if nid != 0 {
          client.objects.insert(
            nid,
            Object::new(
              &protocol::XDG_SURFACE,
              5,
              Role::XdgSurface {
                surface_id: surf,
                configured: false,
              },
            ),
          );
          if let Some(Object {
            role: Role::Surface(s),
            ..
          }) = client.objects.get_mut(&surf)
          {
            s.xdg_surface_id = Some(nid);
          }
        }
      }
      3 => {}
      _ => {}
    }
  }

  fn req_xdg_surface(&mut self, client: &mut Client, id: u32, opcode: u16, args: &[Arg]) {
    match opcode {
      0 => {
        client.objects.remove(&id);
      }
      1 => {
        let nid = args.get(0).map(|a| a.as_object()).unwrap_or(0);
        if nid != 0 {
          client.objects.insert(
            nid,
            Object::new(
              &protocol::XDG_TOPLEVEL,
              5,
              Role::XdgToplevel {
                xdg_surface_id: id,
                title: String::new(),
                app_id: String::new(),
                minimized: false,
              },
            ),
          );
          let states: Vec<u8> = Vec::new();
          client.send(
            nid,
            0, // xdg_toplevel.configure(width, height, states)
            &[Arg::Int(0), Arg::Int(0), Arg::Array(states)],
          );
          let serial = self.next_serial();
          client.send(id, 0, &[Arg::Uint(serial)]);
        }
      }
      2 => {
        let nid = args.get(0).map(|a| a.as_object()).unwrap_or(0);
        let parent_xdg_id = args.get(1).map(|a| a.as_object()).unwrap_or(0);
        let pos_id = args.get(2).map(|a| a.as_object()).unwrap_or(0);

        let (rel_x, rel_y, pop_w, pop_h) = match client.objects.get(&pos_id) {
          Some(Object {
            role: Role::Positioner {
              size_w,
              size_h,
              anchor_x,
              anchor_y,
              anchor_w,
              anchor_h,
              offset_x,
              offset_y,
            },
            ..
          }) => {
            let rx = anchor_x + offset_x;
            let ry = anchor_y + anchor_h + offset_y;
            let pw = if *size_w > 0 { *size_w } else { (*anchor_w).max(4) };
            let ph = if *size_h > 0 { *size_h } else { (*anchor_h).max(4) };
            (rx, ry, pw, ph)
          }
          _ => (0, 0, 200, 200),
        };

        let (parent_screen_x, parent_screen_y) = if parent_xdg_id != 0 {
          let parent_surf_id = match client.objects.get(&parent_xdg_id) {
            Some(Object {
              role: Role::XdgSurface { surface_id, .. },
              ..
            }) => *surface_id,
            _ => 0,
          };
          if parent_surf_id != 0 {
            let (bw, bh) = self.backend.size();
            match client.objects.get(&parent_surf_id) {
              Some(Object {
                role: Role::Surface(s),
                ..
              }) => {
                if let Some(buf) = &s.current_buffer {
                  (((bw as i32 - buf.width) / 2).max(0) + s.x, ((bh as i32 - buf.height) / 2).max(0) + s.y)
                } else {
                  (0, 0)
                }
              }
              _ => (0, 0),
            }
          } else {
            (0, 0)
          }
        } else {
          (0, 0)
        };

        let abs_x = parent_screen_x + rel_x;
        let abs_y = parent_screen_y + rel_y;

        if nid != 0 {
          client.objects.insert(nid, Object::new(&protocol::XDG_POPUP, 5, Role::XdgPopup { xdg_surface_id: id }));

          let surf_id = match client.objects.get(&id) {
            Some(Object {
              role: Role::XdgSurface { surface_id, .. },
              ..
            }) => *surface_id,
            _ => 0,
          };
          if let Some(Object {
            role: Role::Surface(s),
            ..
          }) = client.objects.get_mut(&surf_id)
          {
            s.x = abs_x;
            s.y = abs_y;
            s.popup = true;
          }

          // xdg_popup.configure uses parent-relative coordinates (protocol spec).
          client.send(nid, 0, &[Arg::Int(rel_x), Arg::Int(rel_y), Arg::Int(pop_w), Arg::Int(pop_h)]);
          let serial = self.next_serial();
          client.send(id, 0, &[Arg::Uint(serial)]);
        }
      }
      4 => {
        if let Some(Object {
          role: Role::XdgSurface { configured, .. },
          ..
        }) = client.objects.get_mut(&id)
        {
          *configured = true;
        }
      }
      _ => {}
    }
  }

  fn req_xdg_toplevel(&mut self, client: &mut Client, id: u32, opcode: u16, args: &[Arg]) {
    match opcode {
      0 => {
        client.objects.remove(&id);
        self.dirty = true;
        self.shell_state_dirty = true;
      }
      2 => {
        let title = args.get(0).and_then(|a| a.as_str()).unwrap_or("").to_string();
        if let Some(Object {
          role: Role::XdgToplevel { title: t, .. },
          ..
        }) = client.objects.get_mut(&id)
        {
          *t = title.clone();
        }
        self.shell_state_dirty = true;
      }
      3 => {
        let app_id = args.get(0).and_then(|a| a.as_str()).unwrap_or("").to_string();
        if let Some(Object {
          role: Role::XdgToplevel { app_id: a, .. },
          ..
        }) = client.objects.get_mut(&id)
        {
          *a = app_id.clone();
        }
        self.shell_state_dirty = true;
      }
      11 => {
        if let Some(Object {
          role: Role::XdgToplevel { minimized, .. },
          ..
        }) = client.objects.get_mut(&id)
        {
          *minimized = true;
        }
        self.dirty = true;
        self.shell_state_dirty = true;
      }
      _ => {}
    }
  }

  fn req_positioner(&mut self, client: &mut Client, id: u32, opcode: u16, args: &[Arg]) {
    match opcode {
      0 => {
        client.objects.remove(&id);
      }
      1 => {
        let w = args.get(0).map(|a| a.as_int()).unwrap_or(0);
        let h = args.get(1).map(|a| a.as_int()).unwrap_or(0);
        if let Some(Object {
          role: Role::Positioner { size_w, size_h, .. },
          ..
        }) = client.objects.get_mut(&id)
        {
          *size_w = w;
          *size_h = h;
        }
      }
      2 => {
        let x = args.get(0).map(|a| a.as_int()).unwrap_or(0);
        let y = args.get(1).map(|a| a.as_int()).unwrap_or(0);
        let w = args.get(2).map(|a| a.as_int()).unwrap_or(0);
        let h = args.get(3).map(|a| a.as_int()).unwrap_or(0);
        if let Some(Object {
          role: Role::Positioner {
            anchor_x,
            anchor_y,
            anchor_w,
            anchor_h,
            ..
          },
          ..
        }) = client.objects.get_mut(&id)
        {
          *anchor_x = x;
          *anchor_y = y;
          *anchor_w = w;
          *anchor_h = h;
        }
      }
      6 => {
        let x = args.get(0).map(|a| a.as_int()).unwrap_or(0);
        let y = args.get(1).map(|a| a.as_int()).unwrap_or(0);
        if let Some(Object {
          role: Role::Positioner {
            offset_x, offset_y, ..
          },
          ..
        }) = client.objects.get_mut(&id)
        {
          *offset_x = x;
          *offset_y = y;
        }
      }
      _ => {}
    }
  }

  fn req_xdg_popup(&mut self, client: &mut Client, id: u32, opcode: u16, _args: &[Arg]) {
    match opcode {
      0 => {
        if let Some(Object {
          role: Role::XdgPopup { xdg_surface_id },
          ..
        }) = client.objects.get(&id)
        {
          let xdg_surf = *xdg_surface_id;
          if let Some(Object {
            role: Role::XdgSurface { surface_id, .. },
            ..
          }) = client.objects.get(&xdg_surf)
          {
            let sid = *surface_id;
            if let Some(Object {
              role: Role::Surface(s),
              ..
            }) = client.objects.get_mut(&sid)
            {
              s.popup = false;
              s.mapped = false;
            }
          }
        }
        client.objects.remove(&id);
        self.dirty = true;
      }
      1 => {}
      2 => {}
      _ => {}
    }
  }

  fn req_simple_destroy(&mut self, client: &mut Client, id: u32, opcode: u16, destroy_op: u16) {
    if opcode == destroy_op {
      client.objects.remove(&id);
      self.dirty = true;
    }
  }

  fn req_dmabuf(&mut self, client: &mut Client, id: u32, opcode: u16, args: &[Arg]) {
    match opcode {
      0 => {
        client.objects.remove(&id);
      }
      1 => {
        let nid = args.get(0).map(|a| a.as_object()).unwrap_or(0);
        if nid != 0 {
          client.objects.insert(nid, Object::new(&protocol::ZWP_LINUX_BUFFER_PARAMS_V1, 4, Role::DmabufParams(DmabufParams::default())));
        }
      }
      2 => {
        let nid = args.get(0).map(|a| a.as_object()).unwrap_or(0);
        if nid != 0 {
          self.create_feedback(client, nid);
        }
      }
      3 => {
        let nid = args.get(0).map(|a| a.as_object()).unwrap_or(0);
        if nid != 0 {
          self.create_feedback(client, nid);
        }
      }
      _ => {}
    }
  }

  fn create_feedback(&mut self, client: &mut Client, id: u32) {
    client.objects.insert(id, Object::new(&protocol::ZWP_LINUX_DMABUF_FEEDBACK_V1, 4, Role::DmabufFeedback));

    let dev = self.dmabuf_main_device.to_ne_bytes().to_vec();

    if self.dmabuf_format_table >= 0 {
      // Conn owns and closes every queued SCM_RIGHTS fd after send.  Keep the
      // server's table fd persistent and transfer a fresh duplicate for each
      // default/surface feedback request.
      let table_fd = unsafe { libc::fcntl(self.dmabuf_format_table, libc::F_DUPFD_CLOEXEC, 0) };
      if table_fd < 0 {
        client.post_error(id, 0, "failed to duplicate dmabuf format table");
        return;
      }
      client.send(id, 1, &[Arg::Fd(table_fd), Arg::Uint(self.dmabuf_format_table_size as u32)]);
    }
    client.send(id, 2, &[Arg::Array(dev.clone())]);

    client.send(id, 4, &[Arg::Array(dev)]); // tranche_target_device
    let indices: Vec<u8> = [0u16, 1u16].iter().flat_map(|v| v.to_ne_bytes()).collect();
    client.send(id, 5, &[Arg::Array(indices)]); // tranche_formats
    client.send(id, 6, &[Arg::Uint(0)]);
    client.send(id, 3, &[]); // tranche_done
    client.send(id, 0, &[]);
  }

  fn req_dmabuf_params(&mut self, client: &mut Client, id: u32, opcode: u16, args: &[Arg]) {
    match opcode {
      0 => {
        client.objects.remove(&id); // destroy (Drop closes unconsumed fds)
      }
      1 => {
        let fd = args.get(0).map(|a| a.as_fd()).unwrap_or(-1);
        let plane_idx = args.get(1).map(|a| a.as_uint()).unwrap_or(0);
        let offset = args.get(2).map(|a| a.as_uint()).unwrap_or(0);
        let stride = args.get(3).map(|a| a.as_uint()).unwrap_or(0);
        let mod_hi = args.get(4).map(|a| a.as_uint()).unwrap_or(0);
        let mod_lo = args.get(5).map(|a| a.as_uint()).unwrap_or(0);
        let modifier = ((mod_hi as u64) << 32) | mod_lo as u64;
        if let Some(Object {
          role: Role::DmabufParams(p),
          ..
        }) = client.objects.get_mut(&id)
        {
          p.planes.push(DmabufPlane {
            fd,
            plane_idx,
            offset,
            stride,
            modifier,
          });
        } else if fd >= 0 {
          unsafe { libc::close(fd) };
        }
      }
      2 => {
        let w = args.get(0).map(|a| a.as_int()).unwrap_or(0);
        let h = args.get(1).map(|a| a.as_int()).unwrap_or(0);
        let fmt = args.get(2).map(|a| a.as_uint()).unwrap_or(0);
        match self.dmabuf_build(client, id, w, h, fmt) {
          Some(buf) => {
            let bid = client.alloc_server_id();
            client.objects.insert(bid, Object::new(&protocol::WL_BUFFER, 1, Role::Buffer(buf)));
            client.send(id, 0, &[Arg::NewId(bid)]); // created(buffer)
          }
          None => client.send(id, 1, &[]),
        }
      }
      3 => {
        let bid = args.get(0).map(|a| a.as_object()).unwrap_or(0);
        let w = args.get(1).map(|a| a.as_int()).unwrap_or(0);
        let h = args.get(2).map(|a| a.as_int()).unwrap_or(0);
        let fmt = args.get(3).map(|a| a.as_uint()).unwrap_or(0);
        match self.dmabuf_build(client, id, w, h, fmt) {
          Some(buf) if bid != 0 => {
            client.objects.insert(bid, Object::new(&protocol::WL_BUFFER, 1, Role::Buffer(buf)));
          }
          _ => client.post_error(id, 0, "dmabuf import failed"),
        }
      }
      _ => {}
    }
  }

  fn dmabuf_build(&mut self, client: &mut Client, params_id: u32, w: i32, h: i32, format: u32) -> Option<ShmBuffer> {
    let planes = match client.objects.get_mut(&params_id) {
      Some(Object {
        role: Role::DmabufParams(p),
        ..
      }) => {
        if p.used {
          return None;
        }
        p.used = true;
        std::mem::take(&mut p.planes)
      }
      _ => return None,
    };

    let internal = match format {
      DRM_FORMAT_ARGB8888 => Some(FORMAT_ARGB8888),
      DRM_FORMAT_XRGB8888 => Some(FORMAT_XRGB8888),
      _ => None,
    };
    let valid = planes.len() == 1 && w > 0 && h > 0 && planes[0].modifier == DRM_FORMAT_MOD_LINEAR && internal.is_some();

    if !valid {
      for p in &planes {
        unsafe { libc::close(p.fd) };
      }
      return None;
    }

    let pl = &planes[0];
    let size = pl.offset as usize + pl.stride as usize * h as usize;
    // map_dmabuf takes fd ownership; pool uses DMA_BUF_IOCTL_SYNC on CPU reads.
    let pool = ShmPool::map_dmabuf(pl.fd, size)?;
    Some(ShmBuffer {
      pool,
      offset: pl.offset as usize,
      width: w,
      height: h,
      stride: pl.stride as i32,
      format: internal.unwrap(),
    })
  }

  fn composite_and_present(&mut self) {
    let (w, h) = self.backend.size();
    if self.fb.width != w || self.fb.height != h {
      self.fb = Framebuffer::new(w, h);
    }
    self.fb.clear(0xff10_1014);

    let mut toplevels: Vec<(RawFd, u32, i32, i32, crate::shm::ShmBuffer)> = Vec::new();
    let mut popups: Vec<(i32, i32, crate::shm::ShmBuffer)> = Vec::new();
    for (&fd, client) in &self.clients {
      for (&surface_id, obj) in &client.objects {
        if let Role::Surface(s) = &obj.role {
          if !s.mapped || (s.xdg_surface_id.is_none() && !s.input_method_popup) {
            continue;
          }
          if s.input_method_popup && self.active_text_input.is_none() {
            continue;
          }
          if !s.popup && self.surface_is_minimized(client, surface_id) {
            continue;
          }
          if let Some(buf) = &s.current_buffer {
            let (dx, dy) = if s.popup { (s.x, s.y) } else { (((w as i32 - buf.width) / 2).max(0) + s.x, ((h as i32 - buf.height) / 2).max(0) + s.y) };
            if s.popup {
              popups.push((dx, dy, buf.clone()));
            } else {
              toplevels.push((fd, surface_id, dx, dy, buf.clone()));
            }
          }
        }
      }
    }
    toplevels.sort_by_key(|(fd, sid, _, _, _)| if *fd == self.focused_client_fd && *sid == self.focused_surface_id { 1 } else { 0 });
    for (_, _, dx, dy, buf) in &toplevels {
      self.fb.blit_shm(buf, *dx, *dy);
    }
    for (dx, dy, buf) in &popups {
      self.fb.blit_shm(buf, *dx, *dy);
    }

    self.backend.present(&self.fb);

    let ts = now_ms();
    let pending = std::mem::take(&mut self.frame_done);
    for (fd, cb) in pending {
      if let Some(client) = self.clients.get_mut(&fd) {
        client.send(cb, 0, &[Arg::Uint(ts)]);
        client.send(1, 1, &[Arg::Uint(cb)]);
        client.objects.remove(&cb);
      }
    }
  }

  fn process_input_events(&mut self) {
    let Some(rx) = self.input_rx.take() else { return };
    while let Ok(ev) = rx.try_recv() {
      match ev {
        InputEvent::PointerMotion { x, y } => {
          self.ptr_x = x.clamp(0.0, 1.0);
          self.ptr_y = y.clamp(0.0, 1.0);
          self.inject_ptr_motion(self.ptr_x, self.ptr_y);
        }
        InputEvent::PointerRelative { dx, dy } => {
          let (w, h) = self.backend.size();
          self.ptr_x = (self.ptr_x + dx / w.max(1) as f32).clamp(0.0, 1.0);
          self.ptr_y = (self.ptr_y + dy / h.max(1) as f32).clamp(0.0, 1.0);
          self.inject_ptr_motion(self.ptr_x, self.ptr_y);
        }
        InputEvent::PointerButton { button, pressed } => self.inject_ptr_button(button, pressed),
        InputEvent::PointerAxis { axis, value } => self.inject_ptr_axis(axis, value),
        InputEvent::Key { keycode, pressed } => self.inject_key(keycode, pressed),
        InputEvent::Reset => self.reset_input_state(),
        InputEvent::VtSwitch(vt) => self.backend.switch_vt(vt),
      }
    }
    self.input_rx = Some(rx);
  }

  fn reset_input_state(&mut self) {
    let keys = std::mem::take(&mut self.pressed_keys);
    for key in keys {
      self.inject_key(key, false);
    }
    self.kbd_mods = 0;
    self.ptr_entered = false;
    self.kbd_entered = false;
  }

  fn process_signals(&mut self) -> bool {
    loop {
      let mut info: libc::signalfd_siginfo = unsafe { std::mem::zeroed() };
      let n = unsafe { libc::read(self.signal_fd, &mut info as *mut libc::signalfd_siginfo as *mut libc::c_void, std::mem::size_of::<libc::signalfd_siginfo>()) };
      if n < 0 {
        break;
      }
      match info.ssi_signo as libc::c_int {
        libc::SIGUSR1 => self.backend.deactivate(),
        libc::SIGUSR2 => {
          self.backend.activate();
          self.dirty = true;
        }
        libc::SIGINT | libc::SIGTERM | libc::SIGHUP | libc::SIGQUIT => return false,
        _ => {}
      }
    }
    true
  }

  fn find_input_target(&self) -> Option<(RawFd, u32, u32, u32, i32, i32, i32, i32)> {
    let (bw, bh) = self.backend.size();
    let mut best: Option<(RawFd, u32, u32, u32, i32, i32, i32, i32)> = None;
    for (&fd, client) in &self.clients {
      let mut toplevel: Option<(u32, i32, i32, i32, i32)> = None;
      let mut popup: Option<(u32, i32, i32, i32, i32)> = None;
      let mut ptr_id = 0u32;
      let mut kbd_id = 0u32;
      for (&oid, obj) in &client.objects {
        match &obj.role {
          Role::Surface(s) if s.mapped && s.xdg_surface_id.is_some() && !s.input_method_popup => {
            if !s.popup && self.surface_is_minimized(client, oid) {
              continue;
            }
            if let Some(buf) = &s.current_buffer {
              if s.popup {
                popup = Some((oid, buf.width, buf.height, s.x, s.y));
              } else {
                let ox = ((bw as i32 - buf.width) / 2).max(0) + s.x;
                let oy = ((bh as i32 - buf.height) / 2).max(0) + s.y;
                toplevel = Some((oid, buf.width, buf.height, ox, oy));
              }
            }
          }
          Role::Pointer => {
            ptr_id = oid;
          }
          Role::Keyboard => {
            kbd_id = oid;
          }
          _ => {}
        }
      }
      if let Some((sid, sw, sh, ox, oy)) = popup.or(toplevel) {
        if fd == self.focused_client_fd && sid == self.focused_surface_id {
          return Some((fd, sid, ptr_id, kbd_id, sw, sh, ox, oy));
        }
        if best.is_none() {
          best = Some((fd, sid, ptr_id, kbd_id, sw, sh, ox, oy));
        }
      }
    }
    best
  }

  fn to_surface_fixed(&self, nx: f32, ny: f32, origin_x: i32, origin_y: i32) -> (i32, i32) {
    let (bw, bh) = self.backend.size();
    let sx = (nx * bw as f32) as i32 - origin_x;
    let sy = (ny * bh as f32) as i32 - origin_y;
    (sx * 256, sy * 256)
  }

  fn inject_ptr_motion(&mut self, nx: f32, ny: f32) {
    let Some((fd, surf_id, ptr_id, _, _sw, _sh, ox, oy)) = self.find_input_target() else { return };
    if ptr_id == 0 {
      return;
    }
    let (fx, fy) = self.to_surface_fixed(nx, ny, ox, oy);
    let ts = now_ms();
    let serial = self.next_serial();
    let entered = self.ptr_entered && self.ptr_client_fd == fd && self.ptr_surface_id == surf_id;
    {
      let client = self.clients.get_mut(&fd).unwrap();
      if !entered {
        client.send(ptr_id, 0, &[Arg::Uint(serial), Arg::Object(surf_id), Arg::Fixed(fx), Arg::Fixed(fy)]);
        client.send(ptr_id, 5, &[]);
      }
      client.send(ptr_id, 2, &[Arg::Uint(ts), Arg::Fixed(fx), Arg::Fixed(fy)]);
      client.send(ptr_id, 5, &[]);
      client.conn.flush();
    }
    if !entered {
      self.ptr_entered = true;
      self.ptr_client_fd = fd;
      self.ptr_surface_id = surf_id;
    }
    self.ensure_kbd_entered(fd, surf_id);
  }

  fn inject_ptr_button(&mut self, button: u32, pressed: bool) {
    let Some((fd, surf_id, ptr_id, _, _sw, _sh, ox, oy)) = self.find_input_target() else { return };
    if ptr_id == 0 {
      return;
    }
    if pressed {
      self.focused_client_fd = fd;
      self.focused_surface_id = surf_id;
      self.shell_state_dirty = true;
    }
    if !self.ptr_entered || self.ptr_client_fd != fd || self.ptr_surface_id != surf_id {
      let (fx, fy) = self.to_surface_fixed(0.5, 0.5, ox, oy);
      let serial = self.next_serial();
      {
        let client = self.clients.get_mut(&fd).unwrap();
        client.send(ptr_id, 0, &[Arg::Uint(serial), Arg::Object(surf_id), Arg::Fixed(fx), Arg::Fixed(fy)]);
        client.send(ptr_id, 5, &[]);
        client.conn.flush();
      }
      self.ptr_entered = true;
      self.ptr_client_fd = fd;
      self.ptr_surface_id = surf_id;
    }
    let serial = self.next_serial();
    let ts = now_ms();
    let state: u32 = if pressed { 1 } else { 0 };
    let client = self.clients.get_mut(&fd).unwrap();
    client.send(ptr_id, 3, &[Arg::Uint(serial), Arg::Uint(ts), Arg::Uint(button), Arg::Uint(state)]);
    client.send(ptr_id, 5, &[]);
    client.conn.flush();
  }

  fn inject_ptr_axis(&mut self, axis: u32, value: f32) {
    let Some((fd, _, ptr_id, _, _, _, _, _)) = self.find_input_target() else { return };
    if ptr_id == 0 {
      return;
    }
    let ts = now_ms();
    let fv = (value * 256.0) as i32;
    let client = self.clients.get_mut(&fd).unwrap();
    client.send(ptr_id, 4, &[Arg::Uint(ts), Arg::Uint(axis), Arg::Fixed(fv)]);
    client.send(ptr_id, 5, &[]);
    client.conn.flush();
  }

  fn inject_key(&mut self, keycode: u32, pressed: bool) {
    if pressed {
      self.pressed_keys.insert(keycode);
    } else {
      self.pressed_keys.remove(&keycode);
    }
    if let Some((fd, grab_id)) = self.find_input_method_keyboard_grab() {
      let mod_bit: u32 = match keycode {
        42 | 54 => 1,
        29 | 97 => 4,
        56 | 100 => 8,
        _ => 0,
      };
      if mod_bit != 0 {
        if pressed {
          self.kbd_mods |= mod_bit;
        } else {
          self.kbd_mods &= !mod_bit;
        }
      }
      let serial = self.next_serial();
      let modifiers_serial = self.next_serial();
      let ts = now_ms();
      let state = if pressed { 1 } else { 0 };
      if let Some(client) = self.clients.get_mut(&fd) {
        client.send(grab_id, 1, &[Arg::Uint(serial), Arg::Uint(ts), Arg::Uint(keycode), Arg::Uint(state)]);
        client.send(grab_id, 2, &[Arg::Uint(modifiers_serial), Arg::Uint(self.kbd_mods), Arg::Uint(0), Arg::Uint(0), Arg::Uint(0)]);
        client.conn.flush();
      }
      return;
    }
    let Some((fd, surf_id, _, kbd_id, _, _, _, _)) = self.find_input_target() else { return };
    if kbd_id == 0 {
      return;
    }
    self.ensure_kbd_entered(fd, surf_id);
    // modifier bitmask: shift=1, ctrl=4, alt=8
    let mod_bit: u32 = match keycode {
      42 | 54 => 1,  // ShiftLeft / ShiftRight
      29 | 97 => 4,  // ControlLeft / ControlRight
      56 | 100 => 8, // AltLeft / AltRight
      _ => 0,
    };
    if mod_bit != 0 {
      if pressed {
        self.kbd_mods |= mod_bit;
      } else {
        self.kbd_mods &= !mod_bit;
      }
    }
    let serial = self.next_serial();
    let s2 = self.next_serial();
    let ts = now_ms();
    let state: u32 = if pressed { 1 } else { 0 };
    let mods = self.kbd_mods;
    let client = self.clients.get_mut(&fd).unwrap();
    client.send(kbd_id, 3, &[Arg::Uint(serial), Arg::Uint(ts), Arg::Uint(keycode), Arg::Uint(state)]);
    client.send(kbd_id, 4, &[Arg::Uint(s2), Arg::Uint(mods), Arg::Uint(0), Arg::Uint(0), Arg::Uint(0)]);
    client.conn.flush();
  }

  fn find_input_method_keyboard_grab(&self) -> Option<(RawFd, u32)> {
    if self.active_text_input.is_none() {
      return None;
    }
    self.clients.iter().find_map(|(&fd, client)| client.objects.iter().find_map(|(&id, obj)| matches!(obj.role, Role::InputMethodKeyboardGrab { .. }).then_some((fd, id))))
  }

  fn ensure_kbd_entered(&mut self, fd: RawFd, surf_id: u32) {
    self.update_text_input_focus(fd, surf_id);
    if self.kbd_entered && self.kbd_client_fd == fd && self.kbd_surface_id == surf_id {
      return;
    }
    let kbd_id = match self.clients.get(&fd) {
      Some(c) => c.objects.iter().find(|(_, o)| matches!(o.role, Role::Keyboard)).map(|(&id, _)| id).unwrap_or(0),
      None => return,
    };
    if kbd_id == 0 {
      return;
    }
    let serial = self.next_serial();
    let keys: Vec<u8> = Vec::new();
    let client = self.clients.get_mut(&fd).unwrap();
    client.send(kbd_id, 1, &[Arg::Uint(serial), Arg::Object(surf_id), Arg::Array(keys)]);
    client.conn.flush();
    self.kbd_entered = true;
    self.kbd_client_fd = fd;
    self.kbd_surface_id = surf_id;
  }

  fn update_text_input_focus(&mut self, fd: RawFd, surf_id: u32) {
    let unchanged = self.active_text_input.map(|(active_fd, _, active_surface)| active_fd == fd && active_surface == surf_id).unwrap_or(false);
    if !unchanged && self.active_text_input.is_some() {
      self.active_text_input = None;
      self.deactivate_input_methods(None);
    }

    let client_fds: Vec<RawFd> = self.clients.keys().copied().collect();
    for client_fd in client_fds {
      let mut leaves = Vec::new();
      let mut enters = Vec::new();
      let mut activate = Vec::new();
      if let Some(client) = self.clients.get_mut(&client_fd) {
        for (&id, obj) in &mut client.objects {
          if let Role::TextInput {
            surface_id,
            enabled,
            ..
          } = &mut obj.role
          {
            let new_surface = (client_fd == fd).then_some(surf_id);
            if *surface_id != new_surface {
              if let Some(old) = *surface_id {
                leaves.push((id, old));
              }
              *surface_id = new_surface;
              if let Some(new_id) = new_surface {
                enters.push((id, new_id));
                if *enabled {
                  activate.push(id);
                }
              }
            }
          }
        }
        for (id, surface) in leaves {
          client.send(id, 1, &[Arg::Object(surface)]);
        }
        for (id, surface) in enters {
          client.send(id, 0, &[Arg::Object(surface)]);
        }
      }
      if client_fd == fd {
        for id in activate {
          if let Some(target) = self.clients.remove(&client_fd) {
            self.active_text_input = Some((client_fd, id, surf_id));
            self.activate_input_methods(&target, id);
            self.clients.insert(client_fd, target);
          }
        }
      }
    }
  }

  fn clear_text_input_focus(&mut self) {
    if self.active_text_input.take().is_some() {
      self.deactivate_input_methods(None);
    }
    for client in self.clients.values_mut() {
      let mut leaves = Vec::new();
      for (&id, obj) in &mut client.objects {
        if let Role::TextInput { surface_id, .. } = &mut obj.role {
          if let Some(old) = surface_id.take() {
            leaves.push((id, old));
          }
        }
      }
      for (id, surface_id) in leaves {
        client.send(id, 1, &[Arg::Object(surface_id)]);
      }
    }
  }
}

impl Drop for Server {
  fn drop(&mut self) {
    self.backend.shutdown();
    unsafe {
      libc::close(self.epoll_fd);
      libc::close(self.signal_fd);
      if self.dmabuf_format_table >= 0 {
        libc::close(self.dmabuf_format_table);
      }
      if self.input_wake_fd >= 0 {
        libc::close(self.input_wake_fd);
      }
    }
  }
}

fn role_for(iface: &str) -> Role {
  match iface {
    "wl_compositor" => Role::Compositor,
    "wl_subcompositor" => Role::Subcompositor,
    "wl_shm" => Role::Shm,
    "wl_seat" => Role::Seat,
    "wl_output" => Role::Output,
    "wl_data_device_manager" => Role::DataDeviceManager,
    "zwp_text_input_manager_v3" => Role::TextInputManager,
    "zwp_input_method_manager_v2" => Role::InputMethodManager,
    "zwp_virtual_keyboard_manager_v1" => Role::VirtualKeyboardManager,
    "xdg_wm_base" => Role::WmBase,
    "zwp_linux_dmabuf_v1" => Role::Dmabuf,
    _ => Role::Display,
  }
}

fn now_ms() -> u32 { SystemTime::now().duration_since(UNIX_EPOCH).map(|d| d.as_millis() as u32).unwrap_or(0) }

/// format_table layout: { u32 format; u32 pad; u64 modifier }[]
fn create_format_table() -> (RawFd, usize) {
  let entries: [(u32, u64); 2] = [(DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_LINEAR), (DRM_FORMAT_XRGB8888, DRM_FORMAT_MOD_LINEAR)];
  let mut data = Vec::with_capacity(entries.len() * 16);
  for (fmt, modi) in entries {
    data.extend_from_slice(&fmt.to_ne_bytes());
    data.extend_from_slice(&0u32.to_ne_bytes());
    data.extend_from_slice(&modi.to_ne_bytes());
  }

  let name = CString::new("vespera-dmabuf").unwrap();
  let fd = unsafe { libc::memfd_create(name.as_ptr(), libc::MFD_CLOEXEC | libc::MFD_ALLOW_SEALING) };
  if fd < 0 {
    return (-1, 0);
  }
  let mut off = 0usize;
  while off < data.len() {
    let n = unsafe { libc::write(fd, data[off..].as_ptr() as *const libc::c_void, data.len() - off) };
    if n <= 0 {
      break;
    }
    off += n as usize;
  }
  // Seal memfd read-only for client mmap.
  unsafe { libc::fcntl(fd, libc::F_ADD_SEALS, libc::F_SEAL_SHRINK | libc::F_SEAL_GROW | libc::F_SEAL_WRITE | libc::F_SEAL_SEAL) };
  (fd, data.len())
}

/// Detect dev_t of DRM device for dmabuf allocation.
fn detect_drm_device() -> Option<u64> {
  for p in ["/dev/dri/renderD128", "/dev/dri/card0"] {
    let cp = match CString::new(p) {
      Ok(c) => c,
      Err(_) => continue,
    };
    let mut st: libc::stat = unsafe { std::mem::zeroed() };
    if unsafe { libc::stat(cp.as_ptr(), &mut st) } == 0 {
      return Some(st.st_rdev as u64);
    }
  }
  None
}

fn create_signal_fd() -> std::io::Result<RawFd> {
  unsafe {
    let mut mask: libc::sigset_t = std::mem::zeroed();
    libc::sigemptyset(&mut mask);
    for signal in [libc::SIGINT, libc::SIGTERM, libc::SIGHUP, libc::SIGQUIT, libc::SIGUSR1, libc::SIGUSR2] {
      libc::sigaddset(&mut mask, signal);
    }
    if libc::pthread_sigmask(libc::SIG_BLOCK, &mask, std::ptr::null_mut()) != 0 {
      return Err(std::io::Error::last_os_error());
    }
    let fd = libc::signalfd(-1, &mask, libc::SFD_NONBLOCK | libc::SFD_CLOEXEC);
    if fd < 0 {
      Err(std::io::Error::last_os_error())
    } else {
      Ok(fd)
    }
  }
}

#[cfg(test)]
mod tests {
  use super::*;

  #[test]
  fn format_table_can_be_transferred_repeatedly() {
    let (fd, size) = create_format_table();
    assert!(fd >= 0);
    assert_eq!(size, 32);

    let first = unsafe { libc::fcntl(fd, libc::F_DUPFD_CLOEXEC, 0) };
    let second = unsafe { libc::fcntl(fd, libc::F_DUPFD_CLOEXEC, 0) };
    assert!(first >= 0 && second >= 0);
    assert_ne!(first, second);

    unsafe {
      libc::close(first);
      assert!(libc::fcntl(fd, libc::F_GETFD) >= 0);
      libc::close(second);
      assert!(libc::fcntl(fd, libc::F_GETFD) >= 0);
      libc::close(fd);
    }
  }
}
