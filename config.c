#include "config.h"

/* module names and number */
const char *MODULES[] = { 
    "pomodoro", 
    "dailies", 
    "bartime" 
};
const int NUM_MODULES = sizeof(MODULES) / sizeof(MODULES[0]);

/* dailies target domain-study time pairs */
const Target TARGETS[] = {
    { "theory",    120  },
    { "practice",  120  },
    { "coding",     60  }
};
const int TARGET_COUNT = sizeof(TARGETS) / sizeof(TARGETS[0]);

/* dailies day start hour */
const int FIRSTHOUR = 6;

/* bartime date formatting */
const char *date_format = "%a %d %b %H:%M";

/* the pomodoro defaults (t:25,l:15,s:5,n:4,f:4) are in pomodoro.c */
