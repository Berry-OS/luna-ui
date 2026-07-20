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
use std::collections::HashMap;
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
        objects.insert(
            1,
            Object::new(&protocol::WL_DISPLAY, 1, Role::Display),
        );
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
        eprintln!("[vespera-server] post_error obj={} code={} msg={:?}", object_id, code, msg);
        self.send(
            1,
            0,
            &[
                Arg::Object(object_id),
                Arg::Uint(code),
                Arg::Str(Some(msg.to_string())),
            ],
        );
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
    dmabuf_format_table: RawFd,
    dmabuf_format_table_size: usize,
    dmabuf_main_device: u64,

    input_rx: Option<mpsc::Receiver<InputEvent>>,
    input_wake_fd: RawFd,
    ptr_entered: bool,
    ptr_client_fd: RawFd,
    ptr_surface_id: u32,
    kbd_entered: bool,
    kbd_client_fd: RawFd,
    kbd_surface_id: u32,
    kbd_mods: u32,

    shell_ipc: Option<ShellIpc>,
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
            dmabuf_format_table: table_fd,
            dmabuf_format_table_size: table_size,
            dmabuf_main_device: main_device,

            input_rx: None,
            input_wake_fd: -1,
            ptr_entered: false,
            ptr_client_fd: -1,
            ptr_surface_id: 0,
            kbd_entered: false,
            kbd_client_fd: -1,
            kbd_surface_id: 0,
            kbd_mods: 0,

            shell_ipc: ShellIpc::open(),
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

        // Advertise SHM and dmabuf (GTK4 prefers dmabuf).
        let advertised: &[(&'static Interface, u32)] = &[
            (&protocol::WL_COMPOSITOR, 6),
            (&protocol::WL_SUBCOMPOSITOR, 1),
            (&protocol::WL_SHM, 1),
            (&protocol::WL_SEAT, 7),
            (&protocol::WL_OUTPUT, 4),
            (&protocol::WL_DATA_DEVICE_MANAGER, 3),
            (&protocol::XDG_WM_BASE, 5),
            (&protocol::ZWP_LINUX_DMABUF_V1, 4),
        ];
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

    fn epoll_del(&self, fd: RawFd) {
        unsafe {
            libc::epoll_ctl(
                self.epoll_fd,
                libc::EPOLL_CTL_DEL,
                fd,
                std::ptr::null_mut(),
            )
        };
    }

    fn next_serial(&mut self) -> u32 {
        let s = self.serial;
        self.serial = self.serial.wrapping_add(1);
        s
    }

    pub fn run(&mut self) {
        let mut events = vec![libc::epoll_event { events: 0, u64: 0 }; 64];
        loop {
            let timeout = if self.dirty { 0 } else { -1 };
            let n = unsafe {
                libc::epoll_wait(
                    self.epoll_fd,
                    events.as_mut_ptr(),
                    events.len() as i32,
                    timeout,
                )
            };
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
                    eprintln!("[vespera-server] client connected (fd={})", fd);
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
            eprintln!("[vespera-server] client disconnected (fd={})", fd);
            self.dirty = true;
            if self.ptr_client_fd == fd { self.ptr_entered = false; self.ptr_client_fd = -1; }
            if self.kbd_client_fd == fd { self.kbd_entered = false; self.kbd_client_fd = -1; }
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
                eprintln!("[vespera-server] unknown object id={} op={}", msg.object_id, msg.opcode);
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
                client
                    .objects
                    .insert(cb, Object::new(&protocol::WL_CALLBACK, 1, Role::Callback));
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
                client
                    .objects
                    .insert(reg, Object::new(&protocol::WL_REGISTRY, 1, Role::Registry));
                for g in &self.globals {
                    client.send(
                        reg,
                        0,
                        &[
                            Arg::Uint(g.name),
                            Arg::Str(Some(g.interface.name.to_string())),
                            Arg::Uint(g.version),
                        ],
                    );
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
        client
            .objects
            .insert(new_id, Object::new(iface, version, role));

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
                client.send(
                    id,
                    0,
                    &[
                        Arg::Int(0),
                        Arg::Int(0),
                        Arg::Int(300),
                        Arg::Int(200),
                        Arg::Int(0),
                        Arg::Str(Some("berry-lab".into())),
                        Arg::Str(Some("Vespera".into())),
                        Arg::Int(0),
                    ],
                );
                client.send(
                    id,
                    1,
                    &[Arg::Uint(0x3), Arg::Int(w as i32), Arg::Int(h as i32), Arg::Int(60000)],
                );
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
                            &[
                                Arg::Uint(fmt),
                                Arg::Uint((m >> 32) as u32),
                                Arg::Uint(m as u32),
                            ],
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
                    client.objects.insert(
                        nid,
                        Object::new(&protocol::WL_SURFACE, 6, Role::Surface(Surface::default())),
                    );
                }
            }
            1 => {

                let nid = args.get(0).map(|a| a.as_object()).unwrap_or(0);
                if nid != 0 {
                    client
                        .objects
                        .insert(nid, Object::new(&protocol::WL_REGION, 1, Role::Region));
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
                client.objects.insert(
                    nid,
                    Object::new(
                        &protocol::WL_SUBSURFACE,
                        1,
                        Role::Subsurface { surface_id: surf },
                    ),
                );
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
                client.objects.insert(
                    nid,
                    Object::new(&protocol::WL_SHM_POOL, 1, Role::ShmPool { pool }),
                );
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
                    client
                        .objects
                        .insert(nid, Object::new(&protocol::WL_BUFFER, 1, Role::Buffer(buf)));
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
                    client
                        .objects
                        .insert(cb, Object::new(&protocol::WL_CALLBACK, 1, Role::Callback));
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

        self.dirty = true;
        self.shell_state_dirty = true;
    }

    fn surface_is_minimized(&self, client: &Client, surface_id: u32) -> bool {
        let Some(Object { role: Role::Surface(s), .. }) = client.objects.get(&surface_id) else {
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
            Some(Object { role: Role::Surface(s), .. }) => s.xdg_surface_id,
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
        self.focused_surface_id = surface_id;
        self.ptr_entered = false;
        self.kbd_entered = false;
        self.dirty = true;
        self.shell_state_dirty = true;
    }

    fn minimize_surface(&mut self, surface_id: u32) {
        let Some((fd, _)) = self.client_for_surface(surface_id) else { return };
        let xdg_id = match self.clients.get(&fd).unwrap().objects.get(&surface_id) {
            Some(Object { role: Role::Surface(s), .. }) => s.xdg_surface_id,
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
        if self.focused_surface_id == surface_id {
            self.focused_surface_id = 0;
            self.ptr_entered = false;
            self.kbd_entered = false;
        }
        self.dirty = true;
        self.shell_state_dirty = true;
    }

    fn close_surface(&mut self, surface_id: u32) {
        let Some((fd, client)) = self.client_for_surface(surface_id) else { return };
        let xdg_id = match client.objects.get(&surface_id) {
            Some(Object { role: Role::Surface(s), .. }) => s.xdg_surface_id,
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
        let fd = unsafe {
            libc::memfd_create(name.as_ptr(), libc::MFD_CLOEXEC | libc::MFD_ALLOW_SEALING)
        };
        if fd < 0 { return; }
        let mut off = 0usize;
        while off < data.len() {
            let n = unsafe {
                libc::write(fd, data[off..].as_ptr() as *const libc::c_void, data.len() - off)
            };
            if n <= 0 { unsafe { libc::close(fd) }; return; }
            off += n as usize;
        }
        // fd is closed after SCM_RIGHTS send in flush().
        client.send(
            keyboard_id,
            0,
            &[Arg::Uint(1), Arg::Fd(fd), Arg::Uint(data.len() as u32)],
        );

        client.send(keyboard_id, 5, &[Arg::Int(0), Arg::Int(0)]);
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
        let role = if opcode == 0 {
            Role::DataSource
        } else {
            Role::DataDevice
        };
        client.objects.insert(
            nid,
            Object::new(&protocol::WL_DATA_DEVICE_MANAGER, 3, role),
        );
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
                        Object::new(&protocol::XDG_POSITIONER, 5, Role::Positioner {
                            size_w: 0, size_h: 0,
                            anchor_x: 0, anchor_y: 0, anchor_w: 0, anchor_h: 0,
                            offset_x: 0, offset_y: 0,
                        }),
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
                        role: Role::Positioner { size_w, size_h, anchor_x, anchor_y, anchor_w, anchor_h, offset_x, offset_y },
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
                        Some(Object { role: Role::XdgSurface { surface_id, .. }, .. }) => *surface_id,
                        _ => 0,
                    };
                    if parent_surf_id != 0 {
                        let (bw, bh) = self.backend.size();
                        match client.objects.get(&parent_surf_id) {
                            Some(Object { role: Role::Surface(s), .. }) => {
                                if let Some(buf) = &s.current_buffer {
                                    (((bw as i32 - buf.width) / 2).max(0) + s.x,
                                     ((bh as i32 - buf.height) / 2).max(0) + s.y)
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
                    client.objects.insert(
                        nid,
                        Object::new(&protocol::XDG_POPUP, 5, Role::XdgPopup { xdg_surface_id: id }),
                    );

                    let surf_id = match client.objects.get(&id) {
                        Some(Object { role: Role::XdgSurface { surface_id, .. }, .. }) => *surface_id,
                        _ => 0,
                    };
                    if let Some(Object { role: Role::Surface(s), .. }) = client.objects.get_mut(&surf_id) {
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
            0 => { client.objects.remove(&id); }
            1 => {
                let w = args.get(0).map(|a| a.as_int()).unwrap_or(0);
                let h = args.get(1).map(|a| a.as_int()).unwrap_or(0);
                if let Some(Object {
                    role: Role::Positioner { size_w, size_h, .. }, ..
                }) = client.objects.get_mut(&id) {
                    *size_w = w; *size_h = h;
                }
            }
            2 => {
                let x = args.get(0).map(|a| a.as_int()).unwrap_or(0);
                let y = args.get(1).map(|a| a.as_int()).unwrap_or(0);
                let w = args.get(2).map(|a| a.as_int()).unwrap_or(0);
                let h = args.get(3).map(|a| a.as_int()).unwrap_or(0);
                if let Some(Object {
                    role: Role::Positioner { anchor_x, anchor_y, anchor_w, anchor_h, .. }, ..
                }) = client.objects.get_mut(&id) {
                    *anchor_x = x; *anchor_y = y; *anchor_w = w; *anchor_h = h;
                }
            }
            6 => {
                let x = args.get(0).map(|a| a.as_int()).unwrap_or(0);
                let y = args.get(1).map(|a| a.as_int()).unwrap_or(0);
                if let Some(Object {
                    role: Role::Positioner { offset_x, offset_y, .. }, ..
                }) = client.objects.get_mut(&id) {
                    *offset_x = x; *offset_y = y;
                }
            }
            _ => {}
        }
    }

    fn req_xdg_popup(&mut self, client: &mut Client, id: u32, opcode: u16, _args: &[Arg]) {
        match opcode {
            0 => {
                if let Some(Object { role: Role::XdgPopup { xdg_surface_id }, .. }) = client.objects.get(&id) {
                    let xdg_surf = *xdg_surface_id;
                    if let Some(Object { role: Role::XdgSurface { surface_id, .. }, .. }) = client.objects.get(&xdg_surf) {
                        let sid = *surface_id;
                        if let Some(Object { role: Role::Surface(s), .. }) = client.objects.get_mut(&sid) {
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
                    client.objects.insert(
                        nid,
                        Object::new(
                            &protocol::ZWP_LINUX_BUFFER_PARAMS_V1,
                            4,
                            Role::DmabufParams(DmabufParams::default()),
                        ),
                    );
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
        client.objects.insert(
            id,
            Object::new(&protocol::ZWP_LINUX_DMABUF_FEEDBACK_V1, 4, Role::DmabufFeedback),
        );

        let dev = self.dmabuf_main_device.to_ne_bytes().to_vec();

        if self.dmabuf_format_table >= 0 {
            client.send(
                id,
                1,
                &[
                    Arg::Fd(self.dmabuf_format_table),
                    Arg::Uint(self.dmabuf_format_table_size as u32),
                ],
            );
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
                        client.objects.insert(
                            bid,
                            Object::new(&protocol::WL_BUFFER, 1, Role::Buffer(buf)),
                        );
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
                        client.objects.insert(
                            bid,
                            Object::new(&protocol::WL_BUFFER, 1, Role::Buffer(buf)),
                        );
                    }
                    _ => client.post_error(id, 0, "dmabuf import failed"),
                }
            }
            _ => {}
        }
    }

    fn dmabuf_build(
        &mut self,
        client: &mut Client,
        params_id: u32,
        w: i32,
        h: i32,
        format: u32,
    ) -> Option<ShmBuffer> {
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
        let valid = planes.len() == 1
            && w > 0
            && h > 0
            && planes[0].modifier == DRM_FORMAT_MOD_LINEAR
            && internal.is_some();

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

        let mut toplevels: Vec<(u32, i32, i32, crate::shm::ShmBuffer)> = Vec::new();
        let mut popups: Vec<(i32, i32, crate::shm::ShmBuffer)> = Vec::new();
        for client in self.clients.values() {
            for (&surface_id, obj) in &client.objects {
                if let Role::Surface(s) = &obj.role {
                    if !s.mapped || s.xdg_surface_id.is_none() {
                        continue;
                    }
                    if !s.popup && self.surface_is_minimized(client, surface_id) {
                        continue;
                    }
                    if let Some(buf) = &s.current_buffer {
                        let (dx, dy) = if s.popup {
                            (s.x, s.y)
                        } else {
                            (((w as i32 - buf.width) / 2).max(0) + s.x,
                             ((h as i32 - buf.height) / 2).max(0) + s.y)
                        };
                        if s.popup {
                            popups.push((dx, dy, buf.clone()));
                        } else {
                            toplevels.push((surface_id, dx, dy, buf.clone()));
                        }
                    }
                }
            }
        }
        toplevels.sort_by_key(|(sid, _, _, _)| if *sid == self.focused_surface_id { 1 } else { 0 });
        for (_, dx, dy, buf) in &toplevels {
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
                InputEvent::PointerMotion { x, y } => self.inject_ptr_motion(x, y),
                InputEvent::PointerButton { button, pressed } => self.inject_ptr_button(button, pressed),
                InputEvent::PointerAxis { axis, value } => self.inject_ptr_axis(axis, value),
                InputEvent::Key { keycode, pressed } => self.inject_key(keycode, pressed),
            }
        }
        self.input_rx = Some(rx);
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
                    Role::Surface(s) if s.mapped && s.xdg_surface_id.is_some() => {
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
                    Role::Pointer => { ptr_id = oid; }
                    Role::Keyboard => { kbd_id = oid; }
                    _ => {}
                }
            }
            if let Some((sid, sw, sh, ox, oy)) = popup.or(toplevel) {
                if sid == self.focused_surface_id {
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
        if ptr_id == 0 { return; }
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
        if ptr_id == 0 { return; }
        if pressed {
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
        if ptr_id == 0 { return; }
        let ts = now_ms();
        let fv = (value * 256.0) as i32;
        let client = self.clients.get_mut(&fd).unwrap();
        client.send(ptr_id, 4, &[Arg::Uint(ts), Arg::Uint(axis), Arg::Fixed(fv)]);
        client.send(ptr_id, 5, &[]);
        client.conn.flush();
    }

    fn inject_key(&mut self, keycode: u32, pressed: bool) {
        let Some((fd, surf_id, _, kbd_id, _, _, _, _)) = self.find_input_target() else { return };
        if kbd_id == 0 { return; }
        self.ensure_kbd_entered(fd, surf_id);
        // modifier bitmask: shift=1, ctrl=4, alt=8
        let mod_bit: u32 = match keycode {
            42 | 54 => 1,   // ShiftLeft / ShiftRight
            29 | 97 => 4,   // ControlLeft / ControlRight
            56 | 100 => 8,  // AltLeft / AltRight
            _ => 0,
        };
        if mod_bit != 0 {
            if pressed { self.kbd_mods |= mod_bit; } else { self.kbd_mods &= !mod_bit; }
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

    fn ensure_kbd_entered(&mut self, fd: RawFd, surf_id: u32) {
        if self.kbd_entered && self.kbd_client_fd == fd && self.kbd_surface_id == surf_id {
            return;
        }
        let kbd_id = match self.clients.get(&fd) {
            Some(c) => c.objects.iter()
                .find(|(_, o)| matches!(o.role, Role::Keyboard))
                .map(|(&id, _)| id)
                .unwrap_or(0),
            None => return,
        };
        if kbd_id == 0 { return; }
        let serial = self.next_serial();
        let keys: Vec<u8> = Vec::new();
        let client = self.clients.get_mut(&fd).unwrap();
        client.send(kbd_id, 1, &[Arg::Uint(serial), Arg::Object(surf_id), Arg::Array(keys)]);
        client.conn.flush();
        self.kbd_entered = true;
        self.kbd_client_fd = fd;
        self.kbd_surface_id = surf_id;
    }
}

impl Drop for Server {
    fn drop(&mut self) {
        unsafe {
            libc::close(self.epoll_fd);
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
        "xdg_wm_base" => Role::WmBase,
        "zwp_linux_dmabuf_v1" => Role::Dmabuf,
        _ => Role::Display,
    }
}

fn now_ms() -> u32 {
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_millis() as u32)
        .unwrap_or(0)
}

/// format_table layout: { u32 format; u32 pad; u64 modifier }[]
fn create_format_table() -> (RawFd, usize) {
    let entries: [(u32, u64); 2] = [
        (DRM_FORMAT_ARGB8888, DRM_FORMAT_MOD_LINEAR),
        (DRM_FORMAT_XRGB8888, DRM_FORMAT_MOD_LINEAR),
    ];
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
        let n = unsafe {
            libc::write(
                fd,
                data[off..].as_ptr() as *const libc::c_void,
                data.len() - off,
            )
        };
        if n <= 0 {
            break;
        }
        off += n as usize;
    }
    // Seal memfd read-only for client mmap.
    unsafe {
        libc::fcntl(
            fd,
            libc::F_ADD_SEALS,
            libc::F_SEAL_SHRINK | libc::F_SEAL_GROW | libc::F_SEAL_WRITE | libc::F_SEAL_SEAL,
        )
    };
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
