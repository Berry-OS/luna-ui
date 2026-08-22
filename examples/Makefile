# Native Luna UI example. Linux uses GLFW as its only window-system boundary.
CC ?= cc
.DEFAULT_GOAL := all
CFLAGS ?= -O2 -std=gnu11 -Wall -Wextra
CPPFLAGS += -I..

TARGET := example
SAMPLES := tooltip
GLFW_CFLAGS := $(shell pkg-config --cflags glfw3 2>/dev/null)
GLFW_LIBS := $(shell pkg-config --libs glfw3 2>/dev/null)
LIBS := $(GLFW_LIBS) -lGL -lm

.PHONY: all check css-parity clean
all: $(TARGET) $(SAMPLES)

$(TARGET): example.c ../luna-ui.h ../luna_glfw.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GLFW_CFLAGS) -o $@ $< $(LIBS)

tooltip: tooltip.c ../luna-ui.h ../luna_glfw.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(GLFW_CFLAGS) -o $@ $< $(LIBS)

css-parity: ../css_parity.c ../luna-ui.h example.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ ../css_parity.c -lGL -lm

check: css-parity
	./css-parity

clean:
	rm -f $(TARGET) $(SAMPLES) css-parity
