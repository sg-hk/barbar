# Barbar

Barbar is a small X11 status updater that reads and concatenates messages from named pipes, then updates the root window title. By default, it reads from FIFOs in `/tmp/bar`, concatenates messages with `" | "` as a separator, and refreshes every 2 seconds. These settings can be changed in `config.h`.

## Make commands

To build the project run
```bash
make       
sudo make install
```

To uninstall the project run
```bash
sudo make uninstall # Remove the installed binary
make clean          # Remove compiled binary and build artifacts
```

## Configuration

Modify `config.h` before building to adjust settings like the FIFO directory, refresh rate, maximum message size, and separators. Recompile whenever you make a change.
