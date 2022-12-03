#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "io.h"
#include "io_rtc.h"
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

struct _ms_ioctl {
	int num;
	uint8_t (*func)(void *, int, uint8_t);
	void *dat;
	struct _ms_ioctl *next;
};

struct io_maps {
	uint8_t *sim;
	uint8_t *parport;
	struct _ms_ioctl *ioctl_list;
};

int ms_ioctl(ms_ctx *ms, int num, int whatever, uint8_t val)
{
	struct io_maps *io = (struct io_maps *)ms->io;
	struct _ms_ioctl *ioctl_list = io->ioctl_list;
	int ret = MS_OK;

	/* Find matching ioctl number */
	if (ioctl_list != NULL) {
		for (;;) {
			if (num == ioctl_list->num) {
				ioctl_list->func(ioctl_list->dat, whatever, val);
				break;
			}

			if (ioctl_list->next == NULL) // XXX: throw error
				break;
		}
	}

	return ret;
}

/* Why is this half-assed ioctl system so convoluted? Because why the hell not? */
int ms_ioctl_register(ms_ctx *ms, int num, uint8_t (*func)(void *, int, uint8_t), void *dat)
{
	struct io_maps *io = (struct io_maps *)ms->io;
	struct _ms_ioctl *ioctl_list = io->ioctl_list;
	int ret = MS_OK;

	/* First, ensure that the requested ioctl number has not already been
	 * allocated. Walk the list checking each element. This is short enough
	 * of a time for our use. */
	if (ioctl_list != NULL) {
		for (;;) {
			if (num == ioctl_list->num) {
				ret = MS_ERR;
				goto out;
			}

			if (ioctl_list->next == NULL)
				break;

			ioctl_list = ioctl_list->next;
		}
	}

	/* If we made it this far than either ioctl_list is NULL (first time
	 * calling this function, or ioctl_list->next is NULL (this function
	 * had succeeded previously and there is already one member in the
	 * list. */
	if (ioctl_list == NULL) {
		ioctl_list = malloc(sizeof(struct _ms_ioctl));
		/* XXX: Check for error */
		io->ioctl_list = ioctl_list;
		ioctl_list->next = NULL;
	} else {
		/* Allocate a new member, set our pointer to that, set a NULL
		 * value to the next member for later */
		ioctl_list->next = malloc(sizeof(struct _ms_ioctl));
		/* XXX: Check for error */
		ioctl_list = ioctl_list->next;
		ioctl_list->next = NULL;
	}

	/* At this point, we're at a fresh struct and the requested ioctl number
	 * to register is unique. Add this info to the struct */
	ioctl_list->num = num;
	ioctl_list->func = func;
	ioctl_list->dat = dat;
out:
	return ret;
}

int ms_ioctl_deregister(ms_ctx *ms, int num)
{
	struct io_maps *io = (struct io_maps *)ms->io;
	struct _ms_ioctl *ioctl_list = io->ioctl_list;
	struct _ms_ioctl *prev = NULL;
	int ret = MS_OK;

	assert(ioctl_list != NULL);

	for (;;) {
		/* Walk through the list until we find a matching ioctl number.
		 * If we traverse to a next, save the last in a pointer so we
		 * can stitch the list back together. */
		if (ioctl_list->num == num) {
			/* If next is real and prev is not, then the first
			 * member needs to be removed. Save next, free the
			 * root of the struct, and set ms->io->ioctl_list
			 * to the saved next. */
			if (ioctl_list->next && prev == NULL) {
				io->ioctl_list = ioctl_list->next;
				free(ioctl_list);
			/* If next is real and prev is real, then we are
			 * somewhere in the middle. Set prev->next to be
			 * next to remove this member from the list, then
			 * free this member. */
			} else if (ioctl_list->next && prev) {
				prev->next = ioctl_list->next;
				free(ioctl_list);
			/* If next is not real then we are at the end.
			 * If prev is not not then this was the only member
			 * and we need to correctly NULL out ms->io->ioctl_list.
			 * If prev was real then we need to NULL out prev->next. */
			} else {
				free(ioctl_list);
				if(prev)
					prev->next = NULL;
				else
					io->ioctl_list = NULL;
			}

			break;
		}

		if(ioctl_list->next)
			ioctl_list = ioctl_list->next;
		else
			break;
	}

	return MS_OK;
}

/* Can be called multiple times, will zero the buffer if *io_buf is not null */
int io_init(ms_ctx *ms)
{
	struct io_maps *io = (struct io_maps *)ms->io;

	if (io == NULL) {
		io = malloc(sizeof(struct io_maps));
		if (io == NULL) {
			printf("Unable to allocate IO struct\n");
			exit(EXIT_FAILURE);
		}

		io->sim = calloc(SZ_256, sizeof(uint8_t));
		if (io->sim == NULL) {
			printf("Unable to allocate IO buffer\n");
			exit(EXIT_FAILURE);
		}

		/* The _ms_ioctl struct is allocted with the first register call */
		io->ioctl_list = NULL;


		ms->io = (void *)io;
		ioctl_test_init(ms);
	} else {
		/* Buffer is already allocated, just zero it out */
		memset(io->sim, '\0', SZ_256);
	}

	return MS_OK;
}

int io_deinit(ms_ctx *ms)
{
	struct io_maps *io = (struct io_maps *)ms->io;

	free(io->sim);
	free(io);
	ms->io = NULL;

	return MS_OK;
}

uint8_t io_read(ms_ctx *ms, unsigned int absolute_addr)
{
	struct io_maps *io = (struct io_maps *)ms->io;

	assert(io != NULL);

	if (absolute_addr >= RTC_SEC && absolute_addr <= RTC_10YR)
		return io_rtc_read(absolute_addr);

	return *(io->sim + absolute_addr);
}

int io_write(ms_ctx *ms, unsigned int absolute_addr, uint8_t val)
{
	struct io_maps *io = (struct io_maps *)ms->io;

	assert(io != NULL);
	*(io->sim + absolute_addr) = val;

	return MS_OK;
}
