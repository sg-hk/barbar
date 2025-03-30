#ifndef IPCUTIL_H
#define IPCUTIL_H

void log_err(const char *fmt, ...);
void write_to_slot(const char *module_name, const char *fmt, ...);

#endif // IPCUTIL_H
