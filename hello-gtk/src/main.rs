/*
 * Copyright © 2026 Yuichiro Nakada / Project Vespera
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

use gtk4::prelude::*;
use gtk4::{Application, ApplicationWindow, Box as GtkBox, Button, Label, Orientation};

const APP_ID: &str = "net.vespera.hello-gtk";

fn main() {
    let app = Application::builder()
        .application_id(APP_ID)
        .build();

    app.connect_activate(build_ui);
    app.run();
}

fn build_ui(app: &Application) {
    let label = Label::builder()
        .label("Vespera: pure Rust Wayland client")
        .margin_top(24)
        .margin_bottom(8)
        .build();

    let button = Button::builder()
        .label("Click me!")
        .margin_bottom(24)
        .margin_start(24)
        .margin_end(24)
        .build();

    let counter = std::cell::Cell::new(0u32);
    let label_clone = label.clone();
    button.connect_clicked(move |_| {
        counter.set(counter.get() + 1);
        label_clone.set_label(&format!("Clicked {} times", counter.get()));
    });

    let vbox = GtkBox::builder()
        .orientation(Orientation::Vertical)
        .spacing(8)
        .build();
    vbox.append(&label);
    vbox.append(&button);

    let window = ApplicationWindow::builder()
        .application(app)
        .title("Hello GTK (Vespera)")
        .default_width(400)
        .default_height(200)
        .child(&vbox)
        .build();

    window.present();
}
