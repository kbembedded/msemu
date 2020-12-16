#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "msemu.h"
#include "io.h"

#include <z80ex/z80ex_dasm.h>
#include <z80ex/z80ex.h>

enum arguments {
	no_arg = 0,
	int_arg = 1,
};

enum levels {
	LOG_TRACE = 0x01,
	LOG_DBG   = 0x02,
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

static ms_ctx *ms;
static debug_bp bp;
#if !defined(_MSC_VER)
static struct sigaction sigact;
#endif
extern const char* const ms_dev_map_text[];

static int dbg_level;

static void leave_prompt(void *nan);
static void help(void *nan);
static void md(void *addr);
static void mw(void *addr);
static void list_bp(void *nan);
static void set_bpc(void *pc);
static void set_bmw(void *addr);
static void set_bmr(void *addr);
static void examine(void *nan);
static void trace_on(void *nan);
static void trace_off(void *nan);
static void dbg_on(void *nan);
static void dbg_off(void *nan);
static void dump_stack(void *nan);

static const struct cmdtable cmds[] = {
	{ "q", 1, leave_prompt, "[Q]uit emulation and exit completely", no_arg },
	{ "c", 1, leave_prompt, "[C]ontinue execution", no_arg },
	{ "s", 1, leave_prompt, "[S]ingle step execution", no_arg },
	{ "l", 1, list_bp, "[L]ist the current breakpoints", no_arg },
	{ "bmw", 3, set_bmw, "Breakpoint on mem write, \'bmw <addr>\', "
	  "-1 to disable", int_arg },
	{ "bmr", 3, set_bmr, "Breakpoint on mem read, \'bmr <addr>\', "
	  "-1 to disable", int_arg },
	{ "bpc", 3, set_bpc, "Breakpoint on PC, \'bpc <PC>\', -1 to disable",
	  int_arg },
	{ "md", 2, md, "Display memory at address, \'md <addr>\'", int_arg },
	{ "mw", 2, mw, "Edit memory at address, \'mw <addr> <val>\' "
	  "(UNIMPLEMENTED)", int_arg },
	{ "e", 1, examine, "[E]xamine current register state", no_arg },
	{ "dbgoff", 5, dbg_off, "Disable debug output during exec", no_arg },
	{ "dbgon", 4, dbg_on, "Enable debug output during exec", no_arg },
	{ "troff", 5, trace_off, "Disable trace output during exec", no_arg },
	{ "tron", 4, trace_on, "Enable trace output during exec", no_arg },
	{ "dumpstack", 9, dump_stack, "Dump stack from SP to 0xFFFF", no_arg },
	{ "h", 1, help, "Display this [H]elp menu", no_arg },
};
#define NUMCMDS sizeof cmds / sizeof cmds[0]

void log_debug(char *str, ...)
{
	va_list argp;

	if (!(dbg_level & LOG_DBG)) {
	        return;
	}

	va_start(argp, str);
	vprintf(str, argp);
	va_end(argp);
}

void log_trace(char *str, ...)
{
	va_list argp;

	/* Only print if trace level is enabled and/or we're in a break */
	if (!(dbg_level & LOG_TRACE) && !debug_isbreak()) {
	        return;
	}

	va_start(argp, str);
	vprintf(str, argp);
	va_end(argp);
}

void log_error(char *str, ...)
{
	va_list argp;

	va_start(argp, str);
	vprintf(str, argp);
	va_end(argp);
}

static void leave_prompt(void *nan)
{
	;
}

static void help(void *nan)
{
	int i = NUMCMDS;
	printf("Available commands:\n");
	while(i--) printf("%-8s - %s\n", cmds[i].cmd, cmds[i].doc);
}

static void trace_on(void *nan)
{
	dbg_level |= LOG_TRACE;
}

static void trace_off(void *nan)
{
	dbg_level &= ~LOG_TRACE;
}

static void dbg_on(void *nan)
{
	dbg_level |= LOG_DBG;
}

static void dbg_off(void *nan)
{
	dbg_level &= ~LOG_DBG;
}

static void md(void *addr)
{
	uint16_t new_addr = *(unsigned long *)addr;

	printf("0x%04X: 0x%02X\n", new_addr, ms_mread(ms->z80, new_addr, 0, ms));
}

static void mw(void *nan)
{
}

static void list_bp(void *nan)
{
	printf("PC breakpoint is 0x%04X\n", bp.pc);
	printf("MEM read breakpoint is 0x%04X\n", bp.mr);
	printf("MEM write breakpoint is 0x%04X\n", bp.mw);
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

static void dump_stack(void *nan)
{
	uint16_t sp = z80ex_get_reg(ms->z80,regSP);

	/* This covers both even and odd SP start locations. The MS firmware
	 * uses 0xFFFF as the reset SP, while custom code could be using 0x0000
	 * 0x0000 as top of stack is safe since the SP technically points to
	 * most recent byte on the stack; any stack operations first dec SP. */
	for (; sp != 0x0000; ) {
		// prints 0x00SP 0x(SP)(SP+1)
		printf("0x%04X: 0x%02X\n", sp, ms_mread(ms->z80, sp, 0, ms));
		sp++;
	};

}

static void examine(void *nan)
{
	printf("AF:  0x%04X\tBC:  0x%04X\tDE:  0x%04X\tHL:  0x%04X\n"
	       "AF': 0x%04X\tBC': 0x%04X\tDE': 0x%04X\tHL': 0x%04X\n"
	       "IX:  0x%04X\tIY:  0x%04X\tPC:  0x%04X\tSP:  0x%04X\n"
	       "I:   0x%02X\tR:   0x%02X\tIM:  0x%04X\tIFF1: 0x%04X\tIFF2: 0x%04X\n",
	z80ex_get_reg(ms->z80,regAF), z80ex_get_reg(ms->z80,regBC),
	z80ex_get_reg(ms->z80,regDE), z80ex_get_reg(ms->z80,regHL),
	z80ex_get_reg(ms->z80,regAF_), z80ex_get_reg(ms->z80,regBC_),
	z80ex_get_reg(ms->z80,regDE_), z80ex_get_reg(ms->z80,regHL_),
	z80ex_get_reg(ms->z80,regIX), z80ex_get_reg(ms->z80,regIY),
	z80ex_get_reg(ms->z80,regPC), z80ex_get_reg(ms->z80,regSP),
	z80ex_get_reg(ms->z80,regI), z80ex_get_reg(ms->z80,regR),
	z80ex_get_reg(ms->z80,regIM), z80ex_get_reg(ms->z80,regIFF1),
	z80ex_get_reg(ms->z80,regIFF2));

	printf("slot4000: %sp%02d\n", ms_dev_map_text[ms->io[SLOT4_DEV] & 0x0F],
	  ms->io[SLOT4_PAGE]);
	printf("slot8000: %sp%02d\n", ms_dev_map_text[ms->io[SLOT8_DEV] & 0x0F],
	  ms->io[SLOT8_PAGE]);
}

/* Debug support */
void sigint(int sig)
{
	bp.hits++;
	printf("\nReceived SIGINT, interrupting\n");
}

void debug_init(ms_ctx* msctx, z80ex_mread_cb z80ex_mread)
{
	ms = msctx;
	ms_mread = z80ex_mread;

	bp.pc = -1;
	bp.mr = -1;
	bp.mw = -1;
	bp.hits = 0;

	// Override ctrl+c to drop to debug console
#if defined(_MSC_VER)
	signal(SIGINT, sigint);
#else
	sigact.sa_handler = sigint;
	sigaction(SIGINT, &sigact, NULL);
#endif
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
		memset(&buf, 0, sizeof(buf));
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
	int dbg_level_q = dbg_level;
	Z80EX_BYTE val;

	dbg_level &= ~LOG_DBG;
	val = ms_mread(ms->z80, addr, 0, user_data);
	dbg_level = dbg_level_q;

	return val;
}

#define DASM_BUFFER_LEN 256
void debug_dasm(void)
{
	char dasm_buffer[DASM_BUFFER_LEN];
	int dasm_tstates = 0;
	int dasm_tstates2 = 0;

	if (!debug_isbreak() && !(dbg_level & LOG_TRACE)) return;

	memset(&dasm_buffer, 0, DASM_BUFFER_LEN);
	z80ex_dasm(
	  &dasm_buffer[0], DASM_BUFFER_LEN,
	  0,
	  &dasm_tstates, &dasm_tstates2,
	  debug_dasm_readbyte,
	  z80ex_get_reg(ms->z80, regPC),
	  ms);
	log_trace("%04x: %-15s  t=%d", z80ex_get_reg(ms->z80, regPC),
	  dasm_buffer, dasm_tstates);
	if(dasm_tstates2) {
		log_trace("/%d", dasm_tstates2);
	}
	log_trace("\n");

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

void debug_msfw_lut(ms_ctx *ms)
{
	uint16_t sp;

	switch (z80ex_get_reg(ms->z80, regPC)) {
	  case 0x0692:
		sp = z80ex_get_reg(ms->z80, regSP);
		printf("New Widget, type: 0x%04X, flags: 0x%04X, left: 0x%04X,"\
		  " top: 0x%04X, width: 0x%04X, height: 0x%04X, appid: 0x%04X,"\
		  " seqnum: 0x%04X, str_etc: 0x%04X\n",
		  (ms_mread(ms->z80, sp+2, 0, ms) | ms_mread(ms->z80, sp+3, 0, ms) << 8),
		  (ms_mread(ms->z80, sp+4, 0, ms) | ms_mread(ms->z80, sp+5, 0, ms) << 8),
		  (ms_mread(ms->z80, sp+6, 0, ms) | ms_mread(ms->z80, sp+7, 0, ms) << 8),
		  (ms_mread(ms->z80, sp+8, 0, ms) | ms_mread(ms->z80, sp+9, 0, ms) << 8),
		  (ms_mread(ms->z80, sp+10, 0, ms) | ms_mread(ms->z80, sp+11, 0, ms) << 8),
		  (ms_mread(ms->z80, sp+12, 0, ms) | ms_mread(ms->z80, sp+13, 0, ms) << 8),
		  (ms_mread(ms->z80, sp+14, 0, ms) | ms_mread(ms->z80, sp+15, 0, ms) << 8),
		  (ms_mread(ms->z80, sp+16, 0, ms) | ms_mread(ms->z80, sp+17, 0, ms) << 8),
		  (ms_mread(ms->z80, sp+18, 0, ms) | ms_mread(ms->z80, sp+19, 0, ms) << 8));
		break;
	  case 0x064A:
		sp = z80ex_get_reg(ms->z80, regSP);
		printf("Widget event, handle: 0x%04X, signal: 0x%04X,"\
		  " arg1: 0x%04X, arg2: 0x%04X\n",
		  (ms_mread(ms->z80, sp+2, 0, ms) | ms_mread(ms->z80, sp+3, 0, ms) << 8),
		  (ms_mread(ms->z80, sp+4, 0, ms) | ms_mread(ms->z80, sp+5, 0, ms) << 8),
		  (ms_mread(ms->z80, sp+6, 0, ms) | ms_mread(ms->z80, sp+7, 0, ms) << 8),
		  (ms_mread(ms->z80, sp+8, 0, ms) | ms_mread(ms->z80, sp+9, 0, ms) << 8));
		break;
	  default:
		break;
	}
}
