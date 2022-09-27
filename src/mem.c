#include "debug.h"
#include "mem.h"
#include "msemu.h"
#include "sizes.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

#if defined(_MSC_VER)
#include <basetsd.h>
typedef SSIZE_T ssize_t;
#endif

/* Struct to handle software data protection of the 28SF040
 * The dataflash starts out locked, so in order to get back to the locked
 * state, the whole array must be traversed. In msemu, a counter can be
 * used to watch the reads, once the unlock set is matched, then the device
 * will remain unlocked until successfully matching the lock set.
 */

/* The dataflash buffer is used to track this state machine. Its a little
 * clever so here is how it works:
 *
 * 512K+1 byte is allocated for the DF, the +1 is used to track the state
 *   of the lock/unlock pattern, and if the DF is able to be written
 * The DF starts out in the locked state
 * In the state machine, only the least significant 13 bits of address
 *  matter for advancing to the next state. As quoted in the datasheet:
 *  "Addresses >A12 are 'dont care'"
 * Each read of DF, the address is compared to the next address in the
 *   pattern. If the address matches, then move to the next state.
 * The pacing of this state machine is such that 0x0 through 0x6 are
 *   the states to unlock. If 0x7 is reached, the DF is unlocked and
 *   the state machine is advanced to step 0x8 and will await the
 *   next states to re-lock the DF.
 * If at any time, there is an address miss or a DF write, the current
 *   state is ANDed against ~0x7. This has the effect of clearing the state
 *   machine to either the base locked or unlocked state; that is either
 *   0x0 or 0x8
 * The most significant bit of the tracking byte is used to note the state of
 *   the software data protect and is evaluated if there is an attempt to write
 *   to the DF
 *
 * Tracking byte:
 * 8'b0xxxxxxx - Dataflash is locked (default state)
 * 8'b1xxxxxxx - Dataflash is unlocked
 * bits 6:4 are unused at this time
 */
const uint16_t df_unlock_lock_arr[] = {
	0x1823,
	0x1820,
	0x1822,
	0x0418,
	0x041B,
	0x0419,
	0x041A,

	0x0000,

	0x1823,
	0x1820,
	0x1822,
	0x0418,
	0x041B,
	0x0419,
	0x040A,

	0x0000
};

/* Helper functions for loading and saving a file to a buffer.
 *
 * Arguments for both are the same, buffer, file path, and size in bytes
 *
 * Should return number of bytes read from file, 0 on error
 */
/* BUG: These functions should both probably be reworked to use ferror()
 * to detect if there was an error, no file, etc. */
static int filetobuf(uint8_t *buf, const char *file_path, ssize_t sz)
{
	FILE *fd = 0;
	int ret = 0;

	fd = fopen(file_path, "rb");
	if (fd) {
		ret = fread(buf, sizeof(uint8_t), sz, fd);
		fclose(fd);
	}

	return ret;
}

static int buftofile(uint8_t *buf, const char *file_path, ssize_t sz)
{
	FILE *fd = 0;
	int ret = 0;

	fd = fopen(file_path, "wb");
	if (fd) {
		ret = fwrite(buf, sizeof(uint8_t), sz, fd);
		fclose(fd);
	}

	return ret;
}

/****************************************************
 * Dataflash Functions
 ***************************************************/
/* DF init
 * Allocates buffer in ms_ctx
 *
 * This should only ever be called once per lifetime of an emulator instance
 * as this is non-volatile there is no reason to re-initialize it. The
 * option to not write the DF buffer back to disk is for the lifetime of the
 * emulator instance rather than a single power cycle
 */
int df_init(ms_ctx *ms)
{
	assert(ms->df == NULL);

	/* Allocate 512K+1 and store the state of the writeprotect in the +1
	 * This should not and does not get written back to disk! */
	ms->df = (uint8_t *)calloc(SZ_512K+1, sizeof(uint8_t));
	if (ms->df == NULL) {
		printf("Unable to allocate dataflash buffer\n");
		exit(EXIT_FAILURE);
	}

	return MS_OK;
}

/* DF populate
 * Populates an allocated buffer from a file. If the file does not exist then
 * a new one will be created during df_deinit. It is the responsibility of
 * the main MailStation runtime logic to give some defaults to the DF if
 * no valid file is passed in options.
 *
 * This can be called multiple times through the lifetime of an emulator
 * instance, however, why?
 */
int df_populate(ms_ctx *ms, ms_opts *options)
{
	int ret = MS_OK;

	assert(ms->df != NULL);

        /* Open dataflash and dump it in to a buffer.
         * The dataflash should be exactly 512 KiB.
         * Its possible to have a short dump, where the remaining bytes are
         * assumed to be zero. But it in practice shouldn't happen.
         * It should never be longer either. If it is, we just pretend like
         * we didn't notice. This might be unwise behavior.
         */
	if (!filetobuf(ms->df, options->df_path, SZ_512K)) {
                printf("Existing dataflash image not found at '%s', creating "
                  "a new dataflah image.\n", options->df_path);
		ret = ENOENT;
	}

	return ret;
}

int df_deinit(ms_ctx *ms, ms_opts *options)
{
	int ret = MS_OK;

	assert(ms->df != NULL);

	if (options->df_save_to_disk) {
		ret = buftofile(ms->df, options->df_path, SZ_512K);
		if (ret < SZ_512K) {
			printf("Failed writing dataflash, only wrote %d\n",
			  ret);
			ret = EIO;
		}
	}
	free(ms->df);
	ms->df = NULL;

	return ret;
};

uint8_t df_read(ms_ctx *ms, unsigned int absolute_addr)
{
	volatile uint8_t *wp_track = (ms->df + SZ_512K);

	/* See top of file for explanation of code protect and tracking it */

	/* If the address matches the next number in the sequence, advance
	 * to the next state. Setting or clearing the write allow bit in
	 * the tracking byte as needed */
	if ((absolute_addr & 0x1FFF) == df_unlock_lock_arr[(*wp_track & 0xF)]) {
		(*wp_track)++;
		if ((*wp_track & 0xF) == 0x7) {
			(*wp_track)++;
			*wp_track |= 0x80; // Set write allowed bit
		} else if ((*wp_track & 0xF) == 0xF) {
			/* Clear write allowed bit and return counter to start
			 * of unlock state machine */
			*wp_track = 0;
		}
	} else {
		/* If did not match the next state, clear the sequence track */
		*wp_track &= ~(0x7);
	}

	return *(ms->df + absolute_addr);
}

/* Interpret commands intended for 28SF040 flash
 *
 * The Z80 will output commands and an address. In order to properly handle a
 * write, we have to interpret these commands like it were an actual 28SF040.
 *
 * The read path of the dataflash does not have a command associated with it,
 * therefore this is function is only useful for writing/erasing the DF
 *
 * Currently just returns MS_OK, could potentially return operational errors,
 * e.g. Cycle/Command mismatches, unknown/invalid commands, etc.
 * While there are a few different possible errors, they are not indicated
 * as a return value at this time.
 */
int df_write(ms_ctx *ms, unsigned int absolute_addr, uint8_t val)
{
	static uint8_t cycle;
	static uint8_t cmd;
	volatile uint8_t *wp_track = (ms->df + SZ_512K);

	/* ANY write to DF will break the current software protect state
	 * machine sequence! */
	*wp_track &= ~(0x7);

	if (!cycle) {
		switch (val) {
		  case 0xFF: /* Reset dataflash, single cycle */
			log_debug(" * DF    Reset\n");
			break;
		  case 0x00: /* Not sure what cmd is, but only one cycle? */
			log_debug(" * DF    CMD 0x00\n");
			break;
		  case 0xC3: /* Not sure what cmd is, but only one cycle? */
			log_debug(" * DF    CMD 0xC3\n");
			break;
		  default:
			cmd = val;
			cycle++;
			break;
		}
	} else {
		switch(cmd) {
		  case 0x20: /* Sector erase, execute cmd is 0xD0 */
			if (val != 0xD0) break;
			if (!(*wp_track & 0x80)) {
				log_error(" * DF    Attempted sector erase while DF locked!\n");
				break;
			}
			absolute_addr &= 0xFFFFFF00;
			log_debug(" * DF    Sector-Erase: 0x%X\n", absolute_addr);
			memset((ms->df + absolute_addr), 0xFF, 0x100);
			break;
		  case 0x10: /* Byte program */
			if (!(*wp_track & 0x80)) {
				log_error(" * DF    Attempted byte program while DF locked!\n");
				break;
			}
			log_debug(" * DF    W [%04X] <- %02X\n", absolute_addr,val);
			*(ms->df + absolute_addr) = val;
			break;
		  case 0x30: /* Chip erase, execute cmd is 0x30 */
			if (val != 0x30) break;
			if (!(*wp_track & 0x80)) {
				log_error(" * DF    Attempted chip erase while DF locked!\n");
				break;
			}
			log_debug(" * DF    Chip erase\n");
			memset(ms->df, 0xFF, SZ_512K);
			break;
		  case 0x90: /* Read ID */
			/* XXX: Currently does not do any operation with this
			 * command. Does not seem to affect MS operation though
			 */
			log_debug(" * DF    Read ID\n");
			break;
		  default:
			log_error(
			  " * DF    INVALID CMD SEQ: %02X %02X\n", cmd, val);
			break;
		}
		cycle = 0;
	}

	return MS_OK;
}


/****************************************************
 * Codeflash Functions
 ***************************************************/
/* CF init
 * Allocates buffer in ms_ctx
 *
 * This should only ever be called once per lifetime of an emulator instance
 * as this is non-volatile there is no reason to re-initialize it.
 */
int cf_init(ms_ctx *ms)
{
	assert(ms->cf == NULL);

	ms->cf = (uint8_t *)calloc(SZ_1M, sizeof(uint8_t));
	if (ms->cf == NULL) {
		printf("Unable to allocate codeflash buffer\n");
		exit(EXIT_FAILURE);
	}

	return MS_OK;
}

/* CF populate
 * Populates an allocated buffer from a file. If the file does not exist then
 * there isn't much point to executing but that is the main MailStation runtime
 * to decide that.
 *
 * This can be called multiple times through the lifetime of an emulator
 * instance, however, why?
 */
int cf_populate(ms_ctx *ms, ms_opts *options)
{
	int ret = MS_OK;
        /* Open codeflash and dump it in to a buffer.
         * The codeflash should be exactly 1 MiB.
         * Its possible to have a short dump, where the remaining bytes are
         * assumed to be zero.
         * It should never be longer either. If it is, we just pretend like
         * we didn't notice. This might be unwise behavior.
         */
	if (!filetobuf(ms->cf, options->cf_path, SZ_1M)) {
                log_error("Failed to load codeflash from '%s'.\n", options->cf_path);
		free(ms->cf);
		ms->cf = NULL;
		ret = ENOENT;
        }

	return ret;
}

int cf_deinit(ms_ctx *ms, ms_opts *options)
{
	assert(ms->cf != NULL);

	/* Once CF writing is implemented, add writeback process here */

	free(ms->cf);
	ms->cf = NULL;

	return 0;
}

uint8_t cf_read(ms_ctx *ms, unsigned int absolute_addr)
{
	return *(ms->cf + absolute_addr);
}

int cf_write(ms_ctx *ms, unsigned int absolute_addr, uint8_t val)
{
	printf("CF write not implemented!\n");

	return 0;
}


/****************************************************
 * RAM Functions
 ***************************************************/

/* This function has some subtle complexity and different calling "modes"
 * The first time this is called, the buffer is allocated. If, as part of
 * the commandline options, a path to a binary was passed, this binary is
 * captured to a separate buffer.
 * After which, the buffer is stuffed with random data similar to how SRAM
 * functions at power-on. (It has been observed that keeping RAM state between
 * boots causes issues. This is a good indication that SRAM retention modes are
 * not used by the MS in its powered down state).
 * At this point, if the image buffer is valid, it is then loaded in to the
 * actual RAM buffer.
 * Subequent calls to ram_init() can be made. The *options arg can be NULL
 * for these. The RAM is given new random data, and if the image buffer is
 * valid, this is re-applied to RAM.
 */
/* XXX: I'm bored of documenting, do that later */
int ram_init(ms_ctx *ms)
{
	int i;
	uint8_t *ram_ptr;

	if (ms->ram == NULL) {
		ms->ram = (uint8_t *)malloc(SZ_128K);
		if (ms->ram == NULL) {
			printf("Unable to allocate RAM buffer\n");
			exit(EXIT_FAILURE);
		}

		/* Allocate ram_image buffer now, it is the responsibility of
		 * ram_populate() (or other caller) to deallocate it if it is
		 * unused!
		 * At init time, it should be unallocated, if not NULL then
		 * something went wrong.
		 */
		assert(ms->ram_image == NULL);
		ms->ram_image = (uint8_t *)malloc(SZ_128K);
		if (ms->ram_image == NULL) {
			printf("Unable to allocate RAM image buffer\n");
			exit(EXIT_FAILURE);
		}
	}

	/* Either the buffers were freshly allocated or we're here to re-init
	 * them with random data/pre-made image.
	 */
	if (ms->ram_image != NULL) {
		/* The ram_image buffer should already have been randomized
		 * and then valid data overlayed on top of it.
		 */
		memcpy(ms->ram, ms->ram_image, SZ_128K);
	} else {
		ram_ptr = ms->ram;
		for (i = 0; i < SZ_128K; i++) {
			*ram_ptr = rand() & 0xFF;
			ram_ptr++;
		}
	}

	return MS_OK;
}

int ram_populate(ms_ctx *ms, ms_opts *options)
{
	int i;
	uint8_t *ram_ptr;
	int image_len;
	int ret = MS_OK;

	assert(ms->ram != NULL);

	/* Both buffers are freshly allocated and the main RAM buffer is
	 * random. If there is a RAM image passed in options, load that in
	 * to ram_image and load that in to RAM. If no RAM image passed, free
	 * the ram_image buffer.
	 */


	/* If a RAM image file was specified, load it. Otherwise, the RAM
	 * buffer will just keep its random contents.
	 * The image file can really be any length. If it is shorter than
	 * 128 KiB, then that shouldn't cause any issues since the buffer
	 * was already populated with random contents.
	 */
	if (options->ram_path != NULL) {
		/* Buffer has been allocated, throw random data in it
		 * to simulate SRAM startup */
		ram_ptr = ms->ram_image;
		for (i = 0; i < SZ_128K; i++) {
			*ram_ptr = rand() & 0xFF;
			ram_ptr++;
		}

		/* Now attempt to load the RAM image buffer from file. If it
		 * fails for any reason, free the buffer. */
		image_len = filetobuf(ms->ram_image, options->ram_path, SZ_128K);
		if (!image_len) {
			log_error("Failed to load RAM image from '%s'.\n", options->ram_path);
			free(ms->ram_image);
			ms->ram_image = NULL;
			ret = ENOENT;
		}
	} else {
		/* Free the ram_image buffer, we're never going to use it. */
		free(ms->ram_image);
		ms->ram_image = NULL;
	}

	return ret;
}

int ram_deinit(ms_ctx *ms)
{
	free(ms->ram);
	free(ms->ram_image);
	ms->ram = NULL;
	ms->ram_image = NULL;
	return MS_OK;
}

uint8_t ram_read(ms_ctx *ms, unsigned int absolute_addr)
{
	return *(ms->ram + absolute_addr);
}

int ram_write(ms_ctx *ms, unsigned int absolute_addr, uint8_t val)
{
	*(ms->ram + absolute_addr) = val;
	return MS_OK;
}
