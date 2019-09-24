#include "logger.h"

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>

// This handle is used for outputting all debug info
// to a file with /debug
static FILE* debugoutfile = NULL;

/* If verbose is set, then the terminal will output a lot of data, this can
 * slow down emulation.
 * verbose_q is used as a single element stack, the current verbose setting
 * can be pushed to verbose_q, and popped from it. This allows for temporarily
 * enabling or disabling the verbose terminal output.
 * refcnt is a simple counter to ensure that only a single push occurs or a pop
 * only occurs on a pushed value.
 */
static int verbose = 1;
static int verbose_q = 1;
static int refcnt = 0;

/* Push the current terminal verbosity to verbose_q, and set the current
 * verbosity to val.
 */
void log_push(int val)
{
	assert(refcnt == 0);
	verbose_q = verbose;
	verbose = !!val;
	refcnt++;
}

/* Pop the previous terminal verbosity level from verbose_q, verbose contents
 * are clobbered.
 */
void log_pop(void)
{
	assert(refcnt == 1);
	verbose = verbose_q;
	refcnt--;
}

/* Force the current terminal verbosity to a new value. This only affects
 * verbose and not verbose_q. That is, if a verbosity has been pushed, then
 * set will only affect the current verbosity, not the value that was pushed.
 * A pop will restore the original verbosity level that was set before the
 * push.
 */
void log_set(int val)
{
	verbose = !!val;
}

/* Returns the current value of verbose */
int log_isverbose(void)
{
	return verbose;
}

void log_init(const char* logpath, int verb)
{
	if (logpath)
	{
		debugoutfile = fopen(logpath, "w");
	}
	
	verbose = verb;
}

void log_shutdown()
{
	if (debugoutfile)
	{
		fclose(debugoutfile);
	}
}

void log_debug(char *mystring, ...)
{
	va_list argptr;
	char newstring[1024];

	if (!debugoutfile && !verbose) {
		return;
	}

	va_start(argptr, mystring);

	vsprintf(newstring, mystring, argptr);

	// If debug file open, print there
	if (debugoutfile)
	{
		fputs(newstring, debugoutfile);
	}

	// If not silent, print to screen too
	if (verbose)
	{
		// Print to SDL surface
		//printstring(newstring);

		// Print to console
		printf("%s", newstring);
	}

	va_end(argptr);
}

void log_error(char *mystring, ...)
{
	va_list argptr;
	va_start(argptr, mystring);

	char newstring[1024];
	vsprintf(newstring, mystring, argptr);

	// If debug file open, print there
	if (debugoutfile)
	{
		fputs(newstring, debugoutfile);
	}

	printf("%s", newstring);

	va_end(argptr);
}
