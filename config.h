/* Directory containing the FIFOs */
static const char *DIRP = "/tmp/bar";

/* Order of status bar information 
 * Last element is at right most of bar */
static const char *FIFO_ORDER[] = { 
        "fifo_ctimer", 
        "fifo_date", 
        "fifo_horologion",
        NULL
};

/* Maximum total length of the string passed to xsetroot */
static const int MAX_READ = 256;

/* Maximum length of an individual string from a FIFO */
static const int MSG_SIZE = 64;

/* Separator between messages */
static const char *SEP = " | ";

/* Refresh rate in seconds */
static const float RFRSH = 0.5;
