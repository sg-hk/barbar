CC      = cc
CFLAGS  = -Wall -Wextra
LDFLAGS = -lX11

PREFIX  = /usr/local
BINDIR  = $(PREFIX)/bin

TARGET  = barbar
TARGET_SRC = barbar.c

UTIL    = util.c util.h
CONFIG  = config.h

# Detect all .c files in modules/
MODULE_DIR = modules
MODULE_SRCS = $(wildcard $(MODULE_DIR)/*.c)
MODULES = $(basename $(notdir $(MODULE_SRCS)))

BINARIES = $(TARGET) $(MODULES)

all: $(BINARIES)

$(TARGET): $(TARGET_SRC) $(CONFIG)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(MODULES): %: $(MODULE_DIR)/%.c $(UTIL) $(CONFIG)
	$(CC) $(CFLAGS) -DMODULE_NAME=\"${@}\" -o $@ $< util.c

install: all
	mkdir -p $(BINDIR)
	cp $(BINARIES) $(BINDIR)/

uninstall:
	rm -f $(addprefix $(BINDIR)/, $(BINARIES))

clean:
	rm -f $(BINARIES)
