#ifndef _LOGGER_H_
#define _LOGGER_H_

/**
 * Initializes the logger
 */
void log_init(const char* logpath, int silent);

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

#endif // _LOGGER_H_