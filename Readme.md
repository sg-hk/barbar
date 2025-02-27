# Barbar

Barbar is a small X11 status updater that reads and concatenates messages from named pipes, then updates the root window title. By default, it reads from FIFOs in `/tmp/bar`, concatenates messages with `" | "` as a separator, and polls for event every half second. These settings can be changed in `config.h`.

Using FIFOs leads to a flexible interface where everything is simply an event-driven string read with minimal overhead.

## Make commands

To build the project run
```sh
make       
sudo make install
```

To uninstall the project run
```sh
sudo make uninstall # Remove the installed binary
make clean          # Remove compiled binary and build artifacts
```

## Configuration

See `config.h`. Recompile whenever you make a change.
