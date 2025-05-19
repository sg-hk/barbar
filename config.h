static const char *MODULES[] = { 
	"ip",
	"pomodoro", 
	"ccq",
	"dailies", 
	"bartime" 
};
static const int NUM_MODULES = sizeof(MODULES) / sizeof(MODULES[0]);

#define SEP		" | "
#define MSG_SIZE        64
#define MAX_READ        256
#define RFRSH_SEC       0
#define RFRSH_NSEC      500000000
#define MAX_JSON_OUTPUT_SIZE 1024

#define SHM_NAME        "/shm_barbar"
#define SEM_NAME        "/sem_barbar"
