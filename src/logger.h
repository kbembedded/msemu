#ifndef _LOGGER_H_
#define _LOGGER_H_

/**
 * Initializes the logger
 */
void log_init(const char* logpath, int verb);

/**
 * Shuts down the logger (closes the log file)
 */
void log_shutdown();

/**
 * Debug level logging
 */
void log_debug(char* mystring, ...);

/**
 * Error level logging
 */
void log_error(char *mystring, ...);

/**
 * Save the current terminal verbosity level and set it to val
 * NOTE: There can only be one push before a pop is required. Attempting to
 * push twice without a pop will assert trip.
 */
void log_push(int val);

/**
 * Restore previously pushed terminal verbosity level
 * NOTE: A pop is not possible without a push. Attempting to push without a
 * value previously popped will assert trip.
 */
void log_pop(void);

/**
 * Returns 1 if verbose output is currently enabled, either from a push or
 * the set setting
 */
int log_isverbose(void);

/**
 * Force the current terminal verbose setting to val. This will only affect
 * the current setting and not a pushed setting. i.e. If log_push(1) is used,
 * a log_set(0) will override the current setting. A log_pop() will restory the
 * saved verbosity level from the original log_push(1) that occurred.
 */
void log_set(int val);

#endif // _LOGGER_H_
