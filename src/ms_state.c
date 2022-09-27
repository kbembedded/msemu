#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <z80ex/z80ex.h>

#include "msemu.h"
#include "io.h"
#include "mem.h"
// TODO: Safe unwinding in case of alloc failure?
int ms_state_save(ms_ctx *ms)
{
	/* If no state previously saved, allocate all buffers in state
	 * None of these are allocated with initialized data as every buffer
	 * gets clobbered in the save logic.
	 */
	if (ms->state == NULL) {
		/* First, allocate state struct */
		ms->state = (ms_ctx *)malloc(sizeof struct ms_ctx);
		memset(ms->state, '\0', sizeof(struct ms_ctx);
		// XXX: Error checking
		/* Next, allocate each peripheral buffer via normal init methods */
		io_init(ms->state);
		df_init(ms->state);
		cf_init(ms->state);
		ram_init(ms->state);
		/* After ram_init(), both ram and ram_image are allocated, if
		 * ram_image is not used in the main context, free it in the
		 * save state. ram_copy() will do all of the right things.
		 */
		if (ms->ram_image == NULL) {
			free(ms->state->ram_image);
			ms->state->ram_image = NULL;
		}
		lcd_init(ms->state);

		/* Need to manually allocate z80 context */
		ms->state->z80 = (Z80EX_CONTEXT *)malloc(sizeof(Z80EX_CONTEXT));
		/* XXX: Check for errors */

	}

	/* Now, we copy current buffers to the state save buffer */
	io_copy(ms->state, ms);
	df_copy(ms->state, ms);
	cf_copy(ms->state, ms);
	ram_copy(ms->state, ms);
	lcd_copy(ms->state, ms);
	memcpy(ms->state->z80, ms->z80, sizeof(Z80EX_CONTEXT));

	/* Now copy every other struct member. */
	ms->state->lcd_cas = ms->lcd_cas;
	ms->state->interrupt_mask = ms->interrupt_mask;
	memcpy(ms->state->key_matrix, ms->key_matrix, sizeof(ms->key_matrix));
	ms->state->power_state = ms->power_state;
	ms->state->ac_status = ms->ac_status;
	ms->state->batt_status = ms->batt_status;
	ms->state->power_button_n = ms->power_button_n;

	/* Ever buffer, hardware state, and Z80 state should be fully copied now */

	return MS_OK;
}
