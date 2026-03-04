PURPLE_CFLAGS := $(shell pkg-config --cflags purple)
PURPLE_LIBS := $(shell pkg-config --libs purple)
GLIB_CFLAGS := $(shell pkg-config --cflags glib-2.0)
GLIB_LIBS := $(shell pkg-config --libs glib-2.0)
OPENSSL_LIBS := $(shell pkg-config --libs openssl)
SQLITE_LIBS := $(shell pkg-config --libs sqlite3)

PLUGIN_DIR := $(shell pkg-config --variable=plugindir purple)
DATA_DIR := $(shell pkg-config --variable=datadir purple)

CC ?= gcc
CFLAGS += -g -O2 -Wall -fPIC $(PURPLE_CFLAGS) $(GLIB_CFLAGS) -I./src -I./plugin_src
LDFLAGS += -shared

RUST_LIB := target/release/libpurple_matrix_rust.a
TARGET := libpurple-matrix-rust.so

SOURCES := plugin_src/matrix_core.c \
           plugin_src/matrix_account.c \
           plugin_src/matrix_blist.c \
           plugin_src/matrix_chat.c \
           plugin_src/matrix_commands.c \
           plugin_src/matrix_utils.c

OBJECTS := $(SOURCES:.c=.o)

all: $(TARGET)

$(RUST_LIB):
	cargo build --release

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS) $(RUST_LIB)
	$(CC) $(LDFLAGS) -o $@ $(OBJECTS) -Wl,--whole-archive $(RUST_LIB) -Wl,--no-whole-archive $(PURPLE_LIBS) $(GLIB_LIBS) $(SQLITE_LIBS) $(OPENSSL_LIBS) -lpthread -ldl -lm

clean:
	rm -f plugin_src/*.o *.so
	cargo clean

install: $(TARGET)
	mkdir -p $(DESTDIR)$(PLUGIN_DIR)
	install -m 0755 $(TARGET) $(DESTDIR)$(PLUGIN_DIR)
	mkdir -p $(DESTDIR)/usr/share/pixmaps/pidgin/protocols/16
	mkdir -p $(DESTDIR)/usr/share/pixmaps/pidgin/protocols/22
	mkdir -p $(DESTDIR)/usr/share/pixmaps/pidgin/protocols/48
	install -m 0644 icons/16/matrix.png $(DESTDIR)/usr/share/pixmaps/pidgin/protocols/16/matrix.png
	install -m 0644 icons/22/matrix.png $(DESTDIR)/usr/share/pixmaps/pidgin/protocols/22/matrix.png
	install -m 0644 icons/48/matrix.png $(DESTDIR)/usr/share/pixmaps/pidgin/protocols/48/matrix.png

.PHONY: all clean install
