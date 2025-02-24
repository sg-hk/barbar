#ifndef CONFIG_H
#define CONFIG_H

/* Directory containing the FIFOs */
#define DIRP "/tmp/bar"

/* Maximum total length of the string passed to xsetroot */
#define MAX_READ 256 

/* Maximum length of an individual string from a FIFO */
#define MSG_SIZE 64

/* Separator between messages */
#define SEP " | "

/* Refresh rate in seconds. Fractions are fine */
#define RFRSH 2

#endif /* CONFIG_H */
