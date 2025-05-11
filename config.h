/* left-to-right module order on the status bar */
static const char *MODULES[] = { 
	"pomodoro", 
	"dailies", 
	"time" 
};

#define  NUM_MODULES	sizeof(MODULES) / sizeof(MODULES[0])

/* separator shown between module strings */
#define SEP             " | "          

/* limits */
#define MSG_SIZE        64	       /* individual slot len */
#define MAX_READ        256            /* total bar status len */

/* bar update interval */
#define RFRSH_SEC       0              /* seconds */
#define RFRSH_NSEC      500000000      /* nanoseconds (0-999 999 999) */

/* shared names */
#define SHM_NAME        "/shm_barbar"
#define SEM_NAME        "/sem_barbar"
