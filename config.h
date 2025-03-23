#pragma once

/* 
 * Name of the shared memory and semaphore objects.
 * Do not change this unless necessary.
 */
static const char *SHM_NAME = "/barbar_shm";
static const char *SEM_NAME = "/barbar_sem";

/* 
 * Number of modules writing to shared memory.
 * This determines the number of slots in the shared memory layout.
 * Ensure it matches the size of MODULE_NAMES.
 */
static const int NUM_MODULES = 3;

/* Maximum length of an individual string from a module */
static const int MSG_SIZE = 64;

/* 
 * Names of the modules, in order from left to right on the status bar.
 * The index of each name is its assigned slot ID.
 */
static const char *MODULE_NAMES[] = {
    "ctimer",
    "date",
    "horologion"
};

/* Maximum total length of the string passed to xsetroot */
static const int MAX_READ = 256;

/* Separator between messages */
static const char *SEP = " | ";

/* Refresh rate in seconds */
static const float RFRSH = 0.5;
