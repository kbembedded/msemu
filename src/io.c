#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "msemu.h"
#include "sizes.h"

/* Can be called multiple times, will zero the buffer if *io_buf is not null */
/* Standard Z80 IO access will use lower 8bit of ADDR to be IO address with
 * upper 8bit of ADDR being the value to write to the IO port
 */
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
