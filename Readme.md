# Barbar

Barbar is a minimal X11 status updater. It reads fixed-size strings from shared memory, joins them with a separator, and sets the result as the root window name. By default, it uses `" | "` as the separator and updates every 0.5 seconds. These settings are defined in `config.h`.

## Modules

Modules are in the `modules/` directory. Each is a standalone program that writes to a specific slot in shared memory, based on its name (defined via `#define MODULE_NAME`) and its position in the `MODULE_NAMES` array in `config.h`.

Each module uses a shared utility (`write_to_slot()`) to format and write its output. Modules run independently and update their assigned slot.

## Make Commands

To build and install the project:

```sh
make
sudo make install
```

To uninstall and clean up:

```sh
sudo make uninstall 
make clean          
```
