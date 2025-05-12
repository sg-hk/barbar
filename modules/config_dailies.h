typedef struct {
	const char *domain;
	int time;
} Target;

static const Target TARGETS[] = {
	{ "theory",    120 },
	{ "practice",  120 },
	{ "coding",     60 }
};

#define TARGET_COUNT   (int)(sizeof(TARGETS) / sizeof(TARGETS[0]))
#define FIRSTHOUR      6
#define DAILIES_LOG_PATH  "/.local/share/dailies_log"
