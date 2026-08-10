# Build the standalone Luna UI demos.

CC       ?= gcc
CPPFLAGS ?= -I..
CFLAGS   ?= -Os -Wall -Wextra
GLFW_CFLAGS := $(shell pkg-config --cflags glfw3 2>/dev/null)
GLFW_LIBS   := $(shell pkg-config --libs glfw3 2>/dev/null)
CPPFLAGS   += -DLUNA_LINUX_NO_WAYLAND
LUNA_SHELL  ?= ../../luna-shell
GEN         := ../../luna-shell/ui/gen_include.sh
COMMON      := ../luna-ui.h ../stb_truetype.h ../stb_image_write.h

.PHONY: all clean

all: opengl_gui luna-ui sample_01 sample_02

demo.css.h demo.html.h: demo.css demo.html $(GEN)
	$(GEN) demo.css demo.html

luna-ui.css.h luna-ui.html.h: luna-ui.css luna-ui.html $(GEN)
	$(GEN) luna-ui.css luna-ui.html

opengl_gui: opengl_gui.c $(COMMON) demo.css.h demo.html.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(GLFW_CFLAGS) $< -o $@ -lm -lGL $(GLFW_LIBS)

luna-ui: luna-ui.c $(COMMON) luna-ui.css.h luna-ui.html.h
	$(CC) $(CFLAGS) $(CPPFLAGS) $(GLFW_CFLAGS) $< -o $@ -lm -lGL $(GLFW_LIBS)

sample_01: sample_01.c $(COMMON)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(GLFW_CFLAGS) $< -o $@ -lm -lGL $(GLFW_LIBS)

sample_02: sample_02.c $(COMMON)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(GLFW_CFLAGS) $< -o $@ -lm -lGL $(GLFW_LIBS)

clean:
	rm -f opengl_gui luna-ui sample_01 sample_02 demo.css.h demo.html.h luna-ui.css.h luna-ui.html.h
