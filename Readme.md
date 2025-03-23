# Barbar

Barbar is a small X11 status updater that reads and concatenates messages from shared memory, then updates the root window title. By default, it concatenates messages with `" | "` as a separator, and polls for event every half second. These settings can be changed in `config.h`.

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
