#ifndef __DEBUG_H__
#define __DEBUG_H__

#include "msemu.h"

/* enum's to when calling debug_testbp() */
enum bp_type {
	bpPC,
	bpMR,
	bpMW,
};

/* Initialize the debug layer. 
 * The z80ex_dasm() call uses the mread callback to read the instruction at the
 * current PC. In order to get that info, the mread callback, which uses ms_ctx
 * as the user_data, need to both be passed to this function.
 * This additionally sets up a signal handler for SIGINT to catch ctrl+c
 */
void debug_init(ms_ctx* msctx, z80ex_mread_cb z80ex_mread);

/* Provide interactive prompt.
 * When called, will consume the terminal to provide a simple interactive debug
 * interface. See the output of the 'h' command in the debug interface for more
 * information on commands.
 * Generally, debug_prompt() should only be called when debug_isbreak() is true.
 * Calling debug_prompt() will clear any currently hit breakpoints, but will not
 * clear the breakpoint address.
 */
int debug_prompt(void);

/* Disassemble current instruction at PC.
 * Note that the PC must be at the start of a full instruction when this is
 * called.
 * This will only do anything if trace output is enabled, or if debug_isbreak
 * is true, i.e. we've hit a breakpoint. See output of 'h' in interactive debug
 * interface for enabling/disabling trace.
 */
void debug_dasm(void);

/* Test if breakpoint has been hit.
 * Breakpoints are set via interactive debug interface. They can be set on PC,
 * mem read address, and mem write address. In order to test these however,
 * debug_testbp() must be called with the proper enum type as well as the
 * relevant address.
 * If a breakpoint has been hit, this will return 1 and also cause debug_isbreak
 * to return 1. 
 */
int debug_testbp(enum bp_type type, Z80EX_WORD addr);

/* Returns true if breakpoint was hit.
 * Call debug_testbp() to test a breakpoint. Once a breakpoint is hit, this func
 * will return 1 until debug_prompt() is called to clear the hit breakpoints.
 */
int debug_isbreak(void);

/* Print error string.
 * Always goes to terminal
 */
void log_error(char *str, ...);

/* Print trace information
 * Will print information to terminal only when trace is enabled or if
 * debug_isbreak() is true.
 * See 'h' command of interactive debug interface for trace output control.
 */
void log_trace(char *str, ...);

/* Print debug information
 * Will print information to terminal only when debug is enabled.
 * See 'h' command of interactive debug interface for debug output control.
 */
void log_debug(char *str, ...);

#endif
