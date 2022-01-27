#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "io.h"
#include "msemu.h"
#include "sizes.h"


/* Z80 IO has a number of different use cases with different bus behavior for
 * different IO instructions. This makes it possible for clever hardware to
 * take advantage of specific instructions in specific ways. e.g. ZX81 and
 * ZX Spectrum use "IN r,(C)" using all 16 bits of IO addressing to
 * set up the keyboard scan matrix and read back a line all in one command.
 *
 * In all of the IN and OUT variations in the Z80 manual, it is specifically
 * called out in multiple places that there are 256 possible ports. It would
 * appear that the 256 count of ports is a holdover from the 8080, with the
 * Z80 adding the expanded instructions that can use the full 16 bit space.
 *
 * It does not appear that the MS uses more than 256 ports, as it only ever
 * seems to functionally use the 8080 style IO instructions. It is worth
 * noting that the Z80 style IO instructions _do_ appear in a disassembly,
 * but the areas of the firmware they appear in do not look like actual
 * executable code and may be just data.
 *
 * All of this is to note that if there are weird port errors, it might be
 * because this implementation is limited to 256 ports.
 */

/* The options struct passed here will tell us if we should enable the RAM
 * mod on startup after init. This is a hack and probably should be refactored
 * at some point to be a bit cleaner
 */
/* Can be called multiple times, will zero the IO buffer on subsequent calls */
int io_init(ms_ctx *ms, ms_opts *options)
{
	static int do_ram_mod; /* XXX: hack */

	if (ms->io == NULL) {
		ms->io = (uint8_t *)calloc(SZ_256, sizeof(uint8_t));
		if (ms->io == NULL) {
			printf("Unable to allocate IO buffer\n");
			exit(EXIT_FAILURE);
		}
		do_ram_mod = options->ram_mod_por;
	} else {
		/* Buffer is already allocated, just zero it out */
		memset(ms->io, '\0', SZ_256);
	}

	if (do_ram_mod) {
		io_write(ms, UNKNOWN0x28,
			io_read(ms, UNKNOWN0x28) | 0x8);
	}

	return MS_OK;
}

int io_deinit(ms_ctx *ms)
{
	free(ms->io);
	ms->io = NULL;

	return MS_OK;
}

uint8_t io_read(ms_ctx *ms, unsigned int absolute_addr)
{
	assert(ms->io != NULL);
	return *(ms->io + absolute_addr);
}

int io_write(ms_ctx *ms, unsigned int absolute_addr, uint8_t val)
{
	assert(ms->io != NULL);
	*(ms->io + absolute_addr) = val;

	return MS_OK;
}
