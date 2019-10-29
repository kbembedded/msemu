#ifndef __DEBUG_H__
#define __DEBUG_H__

#include "msemu.h"

enum arguments {
	no_arg = 0,
	int_arg,
};

struct cmdtable {
	const char *cmd;
	char cmdlen;
	void (*func)(void *);
	const char *doc;
	char arg;
};

int debug_prompt(ms_ctx *ms);
void debug_dasm(ms_ctx *ms);

#endif
