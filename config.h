/* ----------- */
/* Main config */

static const char *SHM_NAME = "shm_name";
static const char *SEM_NAME = "sem_name";
/* Names of the modules, in order from left to right on the status bar. */
static const char *MODULE_NAMES[] = {
    "pomodoro",
    "horologion"
};
static const int NUM_MODULES = sizeof(MODULE_NAMES) / sizeof(MODULE_NAMES[0]);
/* Separator between messages */
static const char *SEP = " | ";

/* Maximum length of an individual string from a module */
static const int MSG_SIZE = 64;
/* Maximum total length of the string passed to xsetroot */
static const int MAX_READ = 256;
/* Refresh rate in seconds */
static const float RFRSH = 0.5;


/* ----------------- */
/* Horologion config */

static const double latitude = 0.0;
static const double longitude = 0.0;
static const double altitude = 0.0;
