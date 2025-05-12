#include "config.h"

/* define exactly once */
const char *MODULES[] = { 
    "pomodoro", 
    "dailies", 
    "bartime" 
};
const int NUM_MODULES = sizeof(MODULES) / sizeof(MODULES[0]);

const Target TARGETS[] = {
    { "theory",    120  },
    { "practice",  120  },
    { "coding",     60  }
};
const int TARGET_COUNT = sizeof(TARGETS) / sizeof(TARGETS[0]);

const int FIRSTHOUR = 6;
const char *LOGPATH = "/.local/share/dailies/log";

const char *date_format = "%a %d %b %H:%M";
