#ifndef __DEBUG_H__
#define __DEBUG_H__

#include "msemu.h"

void debug_init(ms_ctx* ms, z80ex_mread_cb z80ex_mread);
int debug_prompt(void);
void debug_dasm(void);
int debug_testbp(void);
int debug_isbreak(void);

#endif
