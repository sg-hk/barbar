CC      = cc
CFLAGS  = -Wall -Wextra
HELPERS = util.c config.c
BIN_DIR = $(HOME)/.local/bin
TARGETS = barbar bartime pomodoro dailies

all: $(TARGETS)

%: %.c $(HELPERS)
	$(CC) $(CFLAGS) $(HELPERS) $< -o $@

install: all
	mkdir -p $(BIN_DIR)
	for bin in $(TARGETS); do cp $$bin $(BIN_DIR)/; done

clean:
	rm -f $(TARGETS)

uninstall:
	for bin in $(TARGETS); do rm -f $(BIN_DIR)/$$bin; done
