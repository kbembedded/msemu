#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "msemu.h"

static void leave_prompt(void *nan);
static void help(void *nan);
static void list_bp(void *nan);
static void set_bp(void *nan);
static void examine(void *nan);

static MSHW *ms_debug;

static const struct cmdtable cmds[] = {
	{ "c", 1, leave_prompt, "[C]ontinue execution", no_arg },
	{ "s", 1, leave_prompt, "[S]ingle step execution", no_arg },
	{ "h", 1, help, "Display this [H]elp menu", no_arg },
	{ "l", 1, list_bp, "[L]ist the current breakpoint", no_arg },
	{ "b", 1, set_bp, "Set a [B]reakpoint on PC, \'b <PC>\', -1 to disable", int_arg },
	{ "e", 1, examine, "[E]xamine current registers", no_arg },
};
#define NUMCMDS sizeof cmds / sizeof cmds[0]

static void leave_prompt(void *nan)
{
	;
}

static void help(void *nan)
{
	int i = NUMCMDS;
	printf("Available commands:\n");
	while(i--) printf("%s - %s\n", cmds[i].cmd, cmds[i].doc);
}

static void set_bp(void *pc)
{
	int32_t new_bp = *(unsigned long *)pc;

	ms_debug->bp = new_bp;
}

static void list_bp(void *nan)
{
	printf("BP is 0x%04X\n", ms_debug->bp);
}

static void examine(void *nan)
{
}


int debug_prompt(MSHW *ms)
{
	static int print_warn = 0;
	int i;
	unsigned long int val;
	char buf[10];

	ms_debug = ms;

	if (!print_warn) {
		printf("NOTE! THIS DEBUGGER IS ROUGH AND WILL LET YOU SHOOT YOURSELF "
		  "IN THE FOOT!\n");
		print_warn = 1;
	}

	if (ms_debug->bp == z80ex_get_reg(ms->z80, regPC)) {
		printf("Reached breakpoint on PC, 0x%04X\n", ms_debug->bp);
	}

	while (1) {
		printf("> ");
		bzero(&buf, sizeof(buf));
		fgets(buf, sizeof(buf), stdin);

		i = NUMCMDS;
		while (i--) {
			if(!strncmp(buf, cmds[i].cmd, cmds[i].cmdlen)) {
				if (i == 0) return 0; /* Hack, "c" */
				if (i == 1) return 1; /* Hack, "s" */
				switch (cmds[i].arg) {
				  case no_arg:
					cmds[i].func(0);
					break;
				  case int_arg:
					val = strtoul(&(buf[cmds[i].cmdlen+1]), 0, 0);
					cmds[i].func(&val);
					break;
				  default:
					break;
				}
			}
		}
	}
}
