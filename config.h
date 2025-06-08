#ifndef CONFIG_H
#define CONFIG_H

/* separator between modules */
static const char SEP[] = "｜";
/* modules appear in LTR order in the bar */
/* names must match in w2s() calls */
static const char *MODULES[] = { 
	"music",
	"cpom", 
	"ccqwatch",
	"pomwatch", 
	"bartime" 
};

/* maximum individual module string size */
#define MSG_LEN 64
/* maximum total output string size */
#define MAX_LEN 256
/* interestingly, this needs to be a #define for the compiler to
 * accept char slots[NUM_MODULES][MSG_LEN] as constant-size */


/* music module refresh rate */
static const int MUSIC_S = 1;
static const int MUSIC_NS = 0;

#endif
