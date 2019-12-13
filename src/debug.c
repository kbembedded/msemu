#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "logger.h"
#include "msemu.h"

#include <z80ex/z80ex_dasm.h>
#include <z80ex/z80ex.h>

enum arguments {
	no_arg = 0,
	int_arg = 1,
};

struct cmdtable {
	const char *cmd;
	uint8_t cmdlen;
	void (*func)(void *);
	const char *doc;
	char arg;
};

typedef struct debug_bp {
	int32_t mr;
	int32_t mw;
	int32_t pc;
	int32_t hits;
} debug_bp;

z80ex_mread_cb ms_mread;

static ms_ctx *ms_debug;
static debug_bp bp;
static struct sigaction sigact;

static void leave_prompt(void *nan);
static void help(void *nan);
static void list_bp(void *nan);
static void set_bpc(void *pc);
static void set_bmw(void *addr);
static void set_bmr(void *addr);
static void examine(void *nan);
static void force_on(void *nan);
static void force_off(void *nan);

static const struct cmdtable cmds[] = {
	{ "q", 1, leave_prompt, "[Q]uit emulation and exit completely", no_arg },
	{ "c", 1, leave_prompt, "[C]ontinue execution", no_arg },
	{ "s", 1, leave_prompt, "[S]ingle step execution", no_arg },
	{ "h", 1, help, "Display this [H]elp menu", no_arg },
	{ "l", 1, list_bp, "[L]ist the current breakpoints", no_arg },
	{ "bpc", 3, set_bpc, "Breakpoint on PC, -1 to disable", int_arg },
	{ "bmw", 3, set_bmw, "Breakpoint on mem write, -1 to disable",
	  int_arg },
	{ "bmr", 3, set_bmr, "Breakpoint on mem read, -1 to disable",
	  int_arg },
	{ "e", 1, examine, "[E]xamine current register state", no_arg },
	{ "o", 1, force_on, "Force debug printing [O]n during exec", no_arg },
	{ "f", 1, force_off, "Force debug printing o[F]f during exec", no_arg },
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

static void force_on(void *nan)
{
	log_set(0);
}

static void force_off(void *nan)
{
	log_set(1);
}

static void set_bpc(void *pc)
{
	int32_t new_bp = *(unsigned long *)pc;
	bp.pc = new_bp;
}

static void set_bmw(void *addr)
{
	int32_t new_bp = *(unsigned long *)addr;
	bp.mw = new_bp;
}

static void set_bmr(void *addr)
{
	int32_t new_bp = *(unsigned long *)addr;
	bp.mr = new_bp;
}

static void list_bp(void *nan)
{
	printf("PC breakpoint is 0x%04X\n", bp.pc);
	printf("MEM read breakpoint is 0x%04X\n", bp.mr);
	printf("MEM write breakpoint is 0x%04X\n", bp.mw);
}

static void examine(void *nan)
{
	/* TODO: In addition to regs, also list current slot mapping */
}

/* Debug support */
void sigint(int sig)
{
	bp.hits++;
	printf("\nReceived SIGINT, interrupting\n");
}

/* XXX: Set this up to also be passed the FP for readbyte */
void debug_init(ms_ctx* ms, z80ex_mread_cb z80ex_mread)
{
	ms_debug = ms;
	ms_mread = z80ex_mread;

	bp.pc = -1;
	bp.mr = -1;
	bp.mw = -1;
	bp.hits = 0;

	// Override ctrl+c to drop to debug console
	sigact.sa_handler = sigint;
	sigaction(SIGINT, &sigact, NULL);
}

int debug_prompt(void)
{
	static int print_warn = 0;
	int i;
	unsigned long int val;
	char buf[32];

	bp.hits = 0;

	if (!print_warn) {
		printf("NOTE! THIS DEBUGGER IS ROUGH AND WILL LET YOU SHOOT "
		  "YOURSELF IN THE FOOT!\n");
		print_warn = 1;
	}

	while (1) {
		printf("> ");
		bzero(&buf, sizeof(buf));
		fgets(buf, sizeof(buf), stdin);

		i = NUMCMDS;
		while (i--) {
			if(!strncmp(buf, cmds[i].cmd, cmds[i].cmdlen)) {
				if (i == 0) return -1; /* Hack, "q" */
				if (i == 1) return 0;  /* Hack, "c" */
				if (i == 2) {
					bp.hits++;
					return 1;  /* Hack, "s" */
				}
				switch (cmds[i].arg) {
				  case no_arg:
					cmds[i].func(0);
					break;
				  case int_arg:
					val = strtoul(&(buf[cmds[i].cmdlen]), 0, 0);
					cmds[i].func(&val);
					break;
				  default:
					break;
				}
			}
		}
	}
}

Z80EX_BYTE debug_dasm_readbyte (Z80EX_WORD addr, void *user_data)
{
	return ms_mread(ms_debug->z80, addr, 0, user_data);
}

void debug_dasm(void)
{
        int dasm_buffer_len = 256;
        char dasm_buffer[dasm_buffer_len];
        int dasm_tstates = 0;
        int dasm_tstates2 = 0;

	bzero(&dasm_buffer, dasm_buffer_len);
	z80ex_dasm(
	  &dasm_buffer[0], dasm_buffer_len,
	  0,
	  &dasm_tstates, &dasm_tstates2,
	  debug_dasm_readbyte,
	  z80ex_get_reg(ms_debug->z80, regPC),
	  ms_debug);
	log_debug("%-15s  t=%d", dasm_buffer, dasm_tstates);
	if(dasm_tstates2) {
		log_debug("/%d", dasm_tstates2);
	}
	log_debug("\n");

}

int debug_isbreak(void)
{
	return !!bp.hits;
}

int debug_testbp(enum bp_type type, Z80EX_WORD addr)
{
	switch (type) {
	  case bpPC:
		if (addr == bp.pc) {
			printf("Reached breakpoint on PC, 0x%04X\n", bp.pc);
			bp.hits++;
		}
		break;
	  case bpMR:
		if (addr == bp.mr) {
			printf("Reached breakpoint on MEM read, 0x%04X\n",
			  bp.mr);
			bp.hits++;
		}
		break;
	  case bpMW:
		if (addr == bp.mw) {
			printf("Reached breakpoint on MEM write, 0x%04X\n",
			  bp.mw);
			bp.hits++;
		}
		break;
	  default:
		printf("Invalid breakpoint type!\n");
		break;
	}

	return debug_isbreak();
}
