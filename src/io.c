#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

/* Can be called multiple times, will zero the buffer if *io_buf is not null */
int io_init(uint8_t **io_buf)
{
	if (*io_buf == NULL) {
		*io_buf = (uint8_t *)calloc(SZ_256, sizeof(uint8_t));
		if (*io_buf == NULL) {
			printf("Unable to allocate IO buffer\n");
			exit(EXIT_FAILURE);
		}
	} else {
		/* Buffer is already allocated, just zero it out */
		memset(*io_buf, '\0', SZ_256);
	}

	return MS_OK;
}

int io_deinit(uint8_t **io_buf)
{
	assert(*io_buf != NULL);
	free(*io_buf);

	return MS_OK;
}

uint8_t io_read(uint8_t *io_buf, unsigned int absolute_addr)
{
	assert(io_buf != NULL);
	return *(io_buf + absolute_addr);
}

int io_write(uint8_t *io_buf, unsigned int absolute_addr, uint8_t val)
{
	assert(io_buf != NULL);
	*(io_buf + absolute_addr) = val;

	return MS_OK;
}
