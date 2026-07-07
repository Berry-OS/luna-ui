/*
 * Copyright © 2026 Yuichiro Nakada / Project Vespera
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

#![allow(clippy::missing_safety_doc)]

pub mod object;
pub mod protocol;
pub mod render;
pub mod server;
pub mod shm;
pub mod socket;
pub mod types;
pub mod wire;

pub use render::{software::SoftwareBackend, Backend, Framebuffer};
pub use server::Server;

pub fn run(socket_name: &str, backend: Box<dyn Backend>) -> std::io::Result<()> {
    let mut server = Server::new(socket_name, backend)?;
    eprintln!(
        "[vespera-server] listening on $XDG_RUNTIME_DIR/{}",
        socket_name
    );
    server.run();
    Ok(())
}
