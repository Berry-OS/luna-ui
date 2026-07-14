# Copyright © 2026 Yuichiro Nakada / Project Vespera
#
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at https://mozilla.org/MPL/2.0/.

PROFILE ?= release
TARGET  := target/$(PROFILE)
PORT    ?= 8081
PREFIX  ?= /usr/local
LU_LIB  := $(PREFIX)/lib/lu
# e.g. make webgl APP=/usr/bin/gtk4-demo
APP     ?= $(TARGET)/hello-gtk

GLFW_CFLAGS := $(shell pkg-config --cflags glfw3 2>/dev/null)
GLFW_LIBS   := $(shell pkg-config --libs glfw3 2>/dev/null)

.PHONY: all build build-dri build-webgl build-desktop build-shell symlinks \
        run server demo webgl run-gtk desktop lu-session install stop clean \
        opengl_gui luna-ui sample

all: build symlinks

build:
	cargo build $(if $(filter release,$(PROFILE)),--release,)

build-dri:
	cargo build -p wayland-server-rs --features dri $(if $(filter release,$(PROFILE)),--release,)

build-webgl:
	cargo build --features webgl $(if $(filter release,$(PROFILE)),--release,)

build-desktop: build-dri build-shell symlinks

build-shell: opengl_gui luna-ui

# Symlink libwayland-client.so.0 (SONAME expected by GTK4)
symlinks:
	@echo "→ Creating symlinks in $(TARGET)/"
	ln -sf libwayland_client.so $(TARGET)/libwayland-client.so.0
	ln -sf libwayland_client.so $(TARGET)/libwayland-client.so
	ln -sf vespera-server $(TARGET)/lu-compositor
	ln -sf ../../opengl_gui $(TARGET)/lu-shell
	@echo "✓ Done"

UI_DIR = ui

$(UI_DIR)/demo.css.h $(UI_DIR)/demo.html.h: $(UI_DIR)/demo.css $(UI_DIR)/demo.html $(UI_DIR)/gen_include.sh
	cd $(UI_DIR) && ./gen_include.sh demo.css demo.html

$(UI_DIR)/luna-ui.css.h $(UI_DIR)/luna-ui.html.h: $(UI_DIR)/luna-ui.css $(UI_DIR)/luna-ui.html $(UI_DIR)/gen_include.sh
	cd $(UI_DIR) && ./gen_include.sh luna-ui.css luna-ui.html

opengl_gui: $(UI_DIR)/opengl_gui.c $(UI_DIR)/luna-ui.h $(UI_DIR)/stb_truetype.h $(UI_DIR)/stb_image_write.h $(UI_DIR)/demo.css.h $(UI_DIR)/demo.html.h
	@echo "→ Building lu-shell (opengl_gui)"
	gcc -O2 -Wall -Wextra $(GLFW_CFLAGS) -I$(UI_DIR) $(UI_DIR)/opengl_gui.c -o opengl_gui -lm -lGL $(GLFW_LIBS)

luna-ui: $(UI_DIR)/luna-ui.c $(UI_DIR)/luna-ui.h $(UI_DIR)/stb_truetype.h $(UI_DIR)/stb_image_write.h $(UI_DIR)/luna-ui.css.h $(UI_DIR)/luna-ui.html.h
	@echo "→ Building Aurora Noir demo (luna-ui)"
	gcc -O2 -Wall -Wextra $(GLFW_CFLAGS) -I$(UI_DIR) $(UI_DIR)/luna-ui.c -o luna-ui -lm -lGL $(GLFW_LIBS)

sample: $(UI_DIR)/sample_02.c $(UI_DIR)/luna-ui.h $(UI_DIR)/stb_truetype.h $(UI_DIR)/stb_image_write.h
	@echo "→ Building Luna UI sample"
	gcc -O2 -Wall -Wextra $(GLFW_CFLAGS) -I$(UI_DIR) $(UI_DIR)/sample_02.c -o $(UI_DIR)/sample -lm -lGL $(GLFW_LIBS)

# Run app with Rust libwayland-client preloaded
run: build symlinks
	LD_LIBRARY_PATH=$(PWD)/$(TARGET):$$LD_LIBRARY_PATH \
	LD_PRELOAD=$(PWD)/$(TARGET)/libwayland-client.so.0 \
	$(APP)

# Start pure Rust compositor (no libwayland-server)
server: build
	$(TARGET)/vespera-server --socket wayland-1 --screenshot /tmp/vespera.ppm

# Start compositor and connect a GTK app via Rust libwayland-client
demo: build symlinks
	@echo "→ Starting compositor (wayland-1)"
	export XDG_RUNTIME_DIR=$${XDG_RUNTIME_DIR:-/tmp}; \
	  $(TARGET)/vespera-server --socket wayland-1 --screenshot /tmp/vespera.ppm & \
	sleep 0.5; \
	echo "→ Connecting app: $(APP)"; \
	XDG_RUNTIME_DIR=$${XDG_RUNTIME_DIR:-/tmp} \
	WAYLAND_DISPLAY=wayland-1 \
	LD_LIBRARY_PATH=$(PWD)/$(TARGET):$$LD_LIBRARY_PATH \
	LD_PRELOAD=$(PWD)/$(TARGET)/libwayland-client.so.0 \
	  $(APP)

# WebGL browser display
#
#   make webgl                    # hello-gtk in browser
#   make webgl APP=gtk4-demo      # any GTK4 app
#   make webgl APP=/usr/bin/foo PORT=9090
#
# Open http://localhost:$(PORT)/ for live streaming
webgl: build-webgl symlinks
	@echo "→ Starting WebGL compositor (port=$(PORT))"
	export XDG_RUNTIME_DIR=$${XDG_RUNTIME_DIR:-/tmp}; \
	  $(TARGET)/vespera-server --socket wayland-webgl --backend webgl --port $(PORT) & \
	echo $$! > /tmp/vespera-server.pid; \
	sleep 0.5; \
	echo ""; \
	echo "  Open in browser → http://localhost:$(PORT)/"; \
	echo ""; \
	echo "→ Launching app: $(APP)"; \
	XDG_RUNTIME_DIR=$${XDG_RUNTIME_DIR:-/tmp} \
	WAYLAND_DISPLAY=wayland-webgl \
	LD_LIBRARY_PATH=$(PWD)/$(TARGET):$$LD_LIBRARY_PATH \
	LD_PRELOAD=$(PWD)/$(TARGET)/libwayland-client.so.0 \
	  $(APP); \
	$(MAKE) stop

# Invoke run-gtk directly (pass app via APP=...)
#   make run-gtk APP="gtk4-demo --some-flag"
run-gtk: build-webgl symlinks
	PROFILE=$(PROFILE) PORT=$(PORT) ./run-gtk $(APP)

# Full Lu Desktop session (compositor + shell)
desktop: build-desktop
	PROFILE=$(PROFILE) BACKEND=dri ./lu-session

# Software backend desktop (VM / no GPU)
desktop-soft: build symlinks build-shell
	PROFILE=$(PROFILE) BACKEND=software ./lu-session

lu-session: build-desktop
	chmod +x lu-session

install: build-desktop
	install -d $(PREFIX)/bin $(LU_LIB) $(PREFIX)/share/lu-desktop/shell
	install -m 755 lu-session $(PREFIX)/bin/lu-session
	install -m 755 $(TARGET)/vespera-server $(PREFIX)/bin/lu-compositor
	install -m 755 opengl_gui $(PREFIX)/bin/lu-shell
	install -m 755 $(TARGET)/libwayland_client.so $(LU_LIB)/
	ln -sf libwayland_client.so $(LU_LIB)/libwayland-client.so.0
	ln -sf libwayland_client.so $(LU_LIB)/libwayland-client.so
	install -m 644 desktop/shell/layout.html desktop/shell/style.css $(PREFIX)/share/lu-desktop/shell/
	install -m 644 ui/luna-ui.h ui/cssparser.h $(PREFIX)/share/lu-desktop/shell/
	install -m 644 README.md $(PREFIX)/share/doc/lu-desktop/README.md 2>/dev/null || true
	install -m 644 systemd/lu-desktop.service /etc/systemd/system/lu-desktop.service 2>/dev/null || \
	  install -m 644 systemd/lu-desktop.service $(PREFIX)/share/lu-desktop/lu-desktop.service
	@echo "✓ Installed to $(PREFIX)"
	@echo "  Enable boot: systemctl enable lu-desktop.service"

# Stop background compositor
stop:
	@if [ -f /tmp/vespera-server.pid ]; then \
	  PID=$$(cat /tmp/vespera-server.pid); \
	  kill "$$PID" 2>/dev/null && echo "→ Compositor stopped (PID=$$PID)" || true; \
	  rm -f /tmp/vespera-server.pid; \
	fi

clean:
	cargo clean
	rm -f opengl_gui luna-ui $(UI_DIR)/sample $(UI_DIR)/demo.css.h $(UI_DIR)/demo.html.h $(UI_DIR)/luna-ui.css.h $(UI_DIR)/luna-ui.html.h
