#include "logger.h"

#include <stdarg.h>
#include <stdio.h>

// This handle is used for outputting all debug info
// to a file with /debug
static FILE* debugoutfile = NULL;

// If this is true, then certain debug output isn't
// displayed to console (slows down emulation)
static int runsilent = 1;

void log_init(const char* logpath, int silent)
{
	if (logpath)
	{
		debugoutfile = fopen(logpath, "w");
	}
	
	runsilent = silent;
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
	if (!debugoutfile && runsilent)
	{
		return;
	}

	va_list argptr;
	va_start(argptr, mystring);

	char newstring[1024];
	vsprintf(newstring, mystring, argptr);

	// If debug file open, print there
	if (debugoutfile)
	{
		fputs(newstring, debugoutfile);
	}

	// If not silent, print to screen too
	if (!runsilent)
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