CC      = cc
CFLAGS  = -Wall -Wextra
BIN_DIR = bin
SRC_DIR = modules

SRCS    = $(wildcard $(SRC_DIR)/*.c)
TARGETS = $(patsubst $(SRC_DIR)/%.c, $(BIN_DIR)/%, $(SRCS))
TARGETS += $(BIN_DIR)/barbar

all: $(BIN_DIR) $(TARGETS)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(BIN_DIR)/barbar: barbar.c config.h util.c
	$(CC) $(CFLAGS) barbar.c util.c -o $(BIN_DIR)/barbar

$(BIN_DIR)/%: $(SRC_DIR)/%.c util.c
	$(CC) $(CFLAGS) $< util.c -o $(BIN_DIR)/$*

install: all
	mkdir -p $(HOME)/.local/bin
	cp $(BIN_DIR)/* $(HOME)/.local/bin/
	@echo "installed all binaries to $(HOME)/.local/bin"

clean:
	rm -rf $(BIN_DIR)
	@echo "cleaned all built binaries"

uninstall:
	rm -f $(HOME)/.local/bin/bartime $(HOME)/.local/bin/dailies $(HOME)/.local/bin/pomodoro $(HOME)/.local/bin/barbar
	@echo "uninstalled binaries from $(HOME)/.local/bin"
