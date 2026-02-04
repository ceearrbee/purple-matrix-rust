PURPLE_CFLAGS := $(shell pkg-config --cflags purple)
PURPLE_LIBS := $(shell pkg-config --libs purple)
GLIB_CFLAGS := $(shell pkg-config --cflags glib-2.0)
GLIB_LIBS := $(shell pkg-config --libs glib-2.0)
OPENSSL_LIBS := $(shell pkg-config --libs openssl)

PLUGIN_DIR := $(shell pkg-config --variable=plugindir purple)
DATA_DIR := $(shell pkg-config --variable=datadir purple)


SQLITE_LIBS := $(shell pkg-config --libs sqlite3)

CC ?= gcc
CFLAGS += -g -O2 -Wall -fPIC $(PURPLE_CFLAGS) $(GLIB_CFLAGS) -I./src
LDFLAGS += -shared

RUST_LIB := target/release/libpurple_matrix_rust.a
TARGET := libpurple-matrix-rust.so

all: $(TARGET)

$(RUST_LIB):
	cargo build --release

plugin.o: plugin.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): plugin.o $(RUST_LIB)
	$(CC) $(LDFLAGS) -o $@ plugin.o $(RUST_LIB) $(PURPLE_LIBS) $(GLIB_LIBS) $(SQLITE_LIBS) $(OPENSSL_LIBS) -lpthread -ldl -lm

clean:
	rm -f *.o *.so
	cargo clean

install: $(TARGET)
	mkdir -p $(DESTDIR)$(PLUGIN_DIR)
	install -m 0755 $(TARGET) $(DESTDIR)$(PLUGIN_DIR)

.PHONY: all clean install
