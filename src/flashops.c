#include "debug.h"
#include "flashops.h"
#include "msemu.h"
#include "sizes.h"

#include <ctype.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>

/* Interpret commands intended for 28SF040 flash
 *
 * The Z80 will output commands and an addres. In order to properly handle a
 * write, we have to interpret these commands like it were an actual 28SF040.
 *
 * The read path of the dataflash does not have a command associated with it,
 * therefore this is function is only useful for writing/erasing the DF
 *
 * Software data protection status is _NOT_ implemented by this functions, it
 * seems to not be use by the Mailstation in normal application flow.
 *
 * Returns 1 if the dataflash was actually modified. Otherwise returns 0
 *
 * While there are a few different possible errors, they are not indicated
 * as a return value at this time.
 *
 * TODO: Add debugging hook here.
 */
int df_parse_cmd (ms_ctx* ms, unsigned int translated_addr, uint8_t val)
{
	static uint8_t cycle;
	static uint8_t cmd;
	int modified = 0;

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
			translated_addr &= 0xFFFFFF00;
			log_debug(" * DF    Sector-Erase: 0x%X\n", translated_addr);
			memset((uint8_t *)(ms->dev_map[DF] + translated_addr), 0xFF, 0x100);
			modified = 1;
			break;
		  case 0x10: /* Byte program */
			log_debug(" * DF    W [%04X] <- %02X\n", translated_addr,val);
			*(uint8_t *)(ms->dev_map[DF] + translated_addr) = val;
			modified = 1;
			break;
		  case 0x30: /* Chip erase, execute cmd is 0x30 */
			if (val != 0x30) break;
			log_debug(" * DF    Chip erase\n");
			memset((uint8_t *)ms->dev_map[DF], 0xFF, SZ_512K);
			modified = 1;
			break;
		  case 0x90: /* Read ID */
			log_debug(" * DF    Read ID\n");
			break;
		  default:
			log_error(
			  " * DF    INVALID CMD SEQ: %02X %02X\n", cmd, val);
			break;
		}
		cycle = 0;
	}

	return modified;
}

/* Generate and set a random serial number to dataflash buffer that is valid
 * for Mailstation
 *
 * Disassembly of codeflash found that the verification process for serial num
 * is that it starts at 0x7FFC8, is 16 bytes long, each character must be
 * ASCII '0'-'9', 'A'-'Z', 'a'-'z', or '-'
 * In all observed Mailstations, the last character is '-', but it does
 * not seem to be enforced in Mailstation firmware. Nevertheless, the last char
 * is set to a '-'
 */
void df_set_rnd_serial(ms_ctx *ms)
{
	int i;
	uint8_t rnd;
	uint8_t *buf;

	buf = (uint8_t *)(ms->dev_map[DF] + DF_SN_OFFS);

	srandom((unsigned int)time(NULL));
	for (i = 0; i < 15; i++) {
		do {
			rnd = random();
		} while (!isalnum(rnd));
		*buf = rnd;
		buf++;
	}

	*buf = '-';
}

/* Check if serial number in dataflash buffer is valid for Mailstation
 *
 * Returns 1 if number is valid, 0 if invalid.
 *
 * Disassembly of codeflash found that the verification process for serial num
 * is that it starts at 0x7FFC8, is 16 bytes long, each character must be
 * ASCII '0'-'9', 'A'-'Z', 'a'-'z', or '-'
 * In all observed Mailstations, the last character is '-', but it does
 * not seem to be enforced in Mailstation firmware; it is not enforced here
 */
int df_serial_valid(ms_ctx *ms)
{
	int i;
	int ret = 1;
	uint8_t *buf;

	buf = (uint8_t *)(ms->dev_map[DF] + DF_SN_OFFS);

	for (i = 0; i < 16; i++) {
		if (!isalnum(*buf) && *buf != '-') ret = 0;
		buf++;
	}

	return ret;
}

/*
 * XXX: Mostly not complete, still don't know exactly how all of these will
 * work together.
 */
int filetobuf(uint8_t *buf, const char *file_path, ssize_t sz)
{
	FILE *fd = 0;
	int ret = 0;

	fd = fopen(file_path, "rb");
	if (fd)
	{
		ret = fread(buf, sizeof(uint8_t), sz, fd);
		fclose(fd);
	}

	return ret;
}

int buftofile(uint8_t *buf, const char *file_path, ssize_t sz)
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
