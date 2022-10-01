#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <z80ex/z80ex.h>

#include "msemu.h"
#include "io.h"
#include "mem.h"
#include "lcd.h"
// TODO: Safe unwinding in case of alloc failure?
int ms_state_save(ms_ctx *ms)
{
	/* If no state previously saved, allocate all buffers in state
	 * None of these are allocated with initialized data as every buffer
	 * gets clobbered in the save logic.
	 */
	if (ms->state == NULL) {
		/* First, allocate state struct */
		ms->state = (ms_ctx *)malloc(sizeof(struct ms_ctx));
		memset(ms->state, '\0', sizeof(struct ms_ctx));
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
	memcpy(ms->state->z80, ms->z80, sizeof(struct _z80_cpu_context));

	/* Now copy every other struct member. */
	ms->state->lcd_cas = ms->lcd_cas;
	ms->state->interrupt_mask = ms->interrupt_mask;
	memcpy(ms->state->key_matrix, ms->key_matrix, sizeof(ms->key_matrix));
	ms->state->power_state = ms->power_state;
	ms->state->ac_status = ms->ac_status;
	ms->state->batt_status = ms->batt_status;
	ms->state->power_button_n = ms->power_button_n;

	/* Every buffer, hardware state, and Z80 state should be fully copied now */

	return MS_OK;
}

int ms_state_load(ms_ctx *ms)
{
	assert(ms->state != NULL);

	/* Load everything from ms->state, back to ms, leaving state alone */
	io_copy(ms, ms->state);
	df_copy(ms, ms->state);
	cf_copy(ms, ms->state);
	ram_copy(ms, ms->state);
	lcd_copy(ms, ms->state);
	memcpy(ms->z80, ms->state->z80, sizeof(Z80EX_CONTEXT));

	/* Copy every struct member back */
	ms->lcd_cas = ms->state->lcd_cas;
	ms->interrupt_mask = ms->state->interrupt_mask;
	memcpy(ms->key_matrix, ms->state->key_matrix, sizeof(ms->key_matrix));
	ms->power_state = ms->state->power_state;
	ms->ac_status = ms->state->ac_status;
	ms->batt_status = ms->state->batt_status;
	ms->power_button_n = ms->state->power_button_n;

	return MS_OK;
}

int ms_state_deinit(ms_ctx *ms)
{
	assert(ms->state != NULL);

	/* Free every buffer in ms->state first */
	io_deinit(ms->state);
	df_deinit(ms->state, NULL);
	cf_deinit(ms->state, NULL);
	ram_deinit(ms->state); /* Frees ram and ram_image */
	lcd_deinit(ms->state);

	/* Need to manually free z80 context */
	free(ms->state->z80);

	/* Finally, free state and mark it empty */
	free(ms->state);
	ms->state = NULL;

	return MS_OK;
}
