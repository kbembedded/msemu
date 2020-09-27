#include "debug.h"
#include "flashops.h"
#include "msemu.h"
#include "sizes.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>


/* Helper functions for loading and saving a file to a buffer.
 *
 * Arguments for both are the same, buffer, file path, and size in bytes
 *
 * Should return number of bytes read from file, 0 on error
 */
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
int df_init(uint8_t **df_buf, ms_opts *options)
{
	assert(*df_buf == NULL);

	*df_buf = (uint8_t *)calloc(SZ_512K, sizeof(uint8_t));
	if (*df_buf == NULL) {
		printf("Unable to allocate dataflash buffer\n");
		exit(EXIT_FAILURE);
	}

        /* Open dataflash and dump it in to a buffer.
         * The dataflash should be exactly 512 KiB.
         * Its possible to have a short dump, where the remaining bytes are
         * assumed to be zero. But it in practice shouldn't happen.
         * It should never be longer either. If it is, we just pretend like
         * we didn't notice. This might be unwise behavior.
         */
	if (!filetobuf(*df_buf, options->df_path, SZ_512K)) {
                printf("Existing dataflash image not found at '%s', creating "
                  "a new dataflah image.\n", options->df_path);
		return ENOENT;
	}

	return MS_OK;
}

int df_deinit(uint8_t **df_buf, ms_opts *options)
{
	int ret = MS_OK;

	assert(*df_buf != NULL);

	if (options->df_save_to_disk) {
		ret = buftofile(*df_buf, options->df_path, SZ_512K);
		if (ret < SZ_512K) {
			printf("Failed writing dataflash, only wrote %d\n",
			  ret);
			ret = EIO;
		}
	}
	free(*df_buf);
	*df_buf = NULL;

	return ret;
};

uint8_t df_read(uint8_t *df_buf, unsigned int absolute_addr)
{
	/* XXX: Add hook here for lock/unlock */
	return *(df_buf + absolute_addr);
}

/* Interpret commands intended for 28SF040 flash
 *
 * The Z80 will output commands and an addres. In order to properly handle a
 * write, we have to interpret these commands like it were an actual 28SF040.
 *
 * The read path of the dataflash does not have a command associated with it,
 * therefore this is function is only useful for writing/erasing the DF
 *
 * Software data protection status is _NOT_ implemented by this function!
 *
 * Currently just returns MS_OK, could potentially return operational errors,
 * e.g. Cycle/Command mismatches, unknown/invalid commands, etc.
 * While there are a few different possible errors, they are not indicated
 * as a return value at this time.
 */
int df_write(uint8_t *df_buf, unsigned int absolute_addr, uint8_t val)
{
	static uint8_t cycle;
	static uint8_t cmd;

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
			absolute_addr &= 0xFFFFFF00;
			log_debug(" * DF    Sector-Erase: 0x%X\n", absolute_addr);
			memset((df_buf + absolute_addr), 0xFF, 0x100);
			break;
		  case 0x10: /* Byte program */
			log_debug(" * DF    W [%04X] <- %02X\n", absolute_addr,val);
			*(df_buf + absolute_addr) = val;
			break;
		  case 0x30: /* Chip erase, execute cmd is 0x30 */
			if (val != 0x30) break;
			log_debug(" * DF    Chip erase\n");
			memset(df_buf, 0xFF, SZ_512K);
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
int cf_init(uint8_t **cf_buf, ms_opts *options)
{
	assert(*cf_buf == NULL);

	*cf_buf = (uint8_t *)calloc(SZ_1M, sizeof(uint8_t));
	if (*cf_buf == NULL) {
		printf("Unable to allocate codeflash buffer\n");
		exit(EXIT_FAILURE);
	}

        /* Open codeflash and dump it in to a buffer.
         * The codeflash should be exactly 1 MiB.
         * Its possible to have a short dump, where the remaining bytes are
         * assumed to be zero.
         * It should never be longer either. If it is, we just pretend like
         * we didn't notice. This might be unwise behavior.
         */
	if (!filetobuf(*cf_buf, options->cf_path, SZ_1M)) {
                log_error("Failed to load codeflash from '%s'.\n", options->cf_path);
		free(*cf_buf);
		*cf_buf = NULL;
                return ENOENT;
        }

	return MS_OK;
}

int cf_deinit(uint8_t **cf_buf, ms_opts *options)
{
	assert(*cf_buf != NULL);

	free(*cf_buf);
	*cf_buf = NULL;

	return 0;
}

uint8_t cf_read(uint8_t *cf_buf, unsigned int absolute_addr)
{
	return *(cf_buf + absolute_addr);
}

int cf_write(uint8_t *cf_buf, unsigned int absolute_addr, uint8_t val)
{
	printf("CF write not implemented!\n");

	return 0;
}

