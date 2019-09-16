#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <stdio.h>

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

struct breakpoints {
	uint16_t pc;
	struct breakpoints *next;
};

int debug_prompt(void);

#endif
