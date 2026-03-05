# pidgin-matrix-ui/Makefile
PLUGIN_NAME = pidgin-matrix-ui
SOURCES = pidgin_matrix_ui.c
HEADERS = 
GTK_VERSION = 2.0 # Pidgin typically uses GTK+ 2.0

# Use pkg-config to get CFLAGS and LDFLAGS for Pidgin and GTK+
PIDGIN_CFLAGS = $(shell pkg-config --cflags purple pidgin gtk+-$(GTK_VERSION))
PIDGIN_LIBS = $(shell pkg-config --libs purple pidgin gtk+-$(GTK_VERSION))

CFLAGS = -g -Wall -fPIC $(PIDGIN_CFLAGS) -I../plugin_src
LDFLAGS = -shared $(PIDGIN_LIBS)

# Output filename for the plugin
TARGET = $(PLUGIN_NAME).so

.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) $(LDFLAGS) -o $@

clean:
	$(RM) $(TARGET) *.o

install: all
	@echo "Installing $(TARGET) to ~/.purple/plugins/"
	@mkdir -p $(HOME)/.purple/plugins/
	@cp $(TARGET) $(HOME)/.purple/plugins/
