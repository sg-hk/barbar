#ifndef CONFIG_H
#define CONFIG_H

/* declarations only: no definitions */
extern const char *MODULES[];
extern const int NUM_MODULES;

#define SEP             " | "          
#define MSG_SIZE        64
#define MAX_READ        256
#define RFRSH_SEC       0
#define RFRSH_NSEC      500000000

#define SHM_NAME        "/shm_barbar"
#define SEM_NAME        "/sem_barbar"

/* Dailies declarations */
typedef struct Target {
    const char *domain;
    const int time;
} Target;

extern const Target TARGETS[];
extern const int TARGET_COUNT;
extern const int FIRSTHOUR;
extern const char *LOGPATH;

/* Bartime */
extern const char *date_format;

#endif
