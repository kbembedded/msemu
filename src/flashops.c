#include "flashops.h"

#include "msemu.h"
#include "logger.h"
#include <z80ex/z80ex.h>

extern struct mshw ms;

/* Return a byte from codeflash buffer
 *
 * TODO:
 *   Write better documentation on this
 *   Add debug hook
 *   Ensure this is used in multiple places when reading from codeflash
 */
inline uint8_t readCodeFlash(uint32_t translated_addr)
{
	return ms.codeflash[translated_addr];
}

/* Write a byte to codeflash buffer
 *
 * TODO:
 *   Implement writing to codeflash
 */
inline void writeCodeFlash(uint32_t translated_addr)
{
	return;
}

/* Return a byte from dataflash
 *
 * Unlike the write path, this does not need to directly emulate 28SF040 read
 * cycles. We just need to return data.
 *
 * TODO: Add debugging potential here
 */
inline uint8_t readDataflash(unsigned int translated_addr)
{
	return ms.dataflash[translated_addr];
}


/* Write a byte to dataflash while handling commands intended for 28SF040 flash
 *
 * The Z80 will output commands and an addres. In order to properly handle a
 * write, we have to interpret these commands like it were an actualy 28SF040.
 *
 * Software data protection status is _NOT_ implemented and seems to not be
 * necessary in normal application flow.
 *
 * Returns 0 on success. 1 if the dataflash was actually modified.
 *
 * TODO: Add debugging hook here.
 */
int8_t writeDataflash(unsigned int translated_addr, uint8_t val)
{
	static uint8_t cycle;
	static uint8_t cmd;
	int8_t modified = 0;

	if (!cycle) {
		switch (val) {
		  case 0xFF: /* Reset dataflash, single cycle */
			log_debug("[%04X] * Dataflash Reset\n", z80ex_get_reg(ms.z80, regPC));
			break;
		  case 0x00: /* Not sure what cmd is, but only one cycle? */
			log_debug("[%04X] * Dataflash cmd 0x00\n", z80ex_get_reg(ms.z80, regPC));
			break;
		  case 0xC3: /* Not sure what cmd is, but only one cycle? */
			log_debug("[%04X] * Dataflash cmd 0xC3\n", z80ex_get_reg(ms.z80, regPC));
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
			log_debug("[%04X] * Dataflash Sector-Erase: 0x%X\n",
			  z80ex_get_reg(ms.z80, regPC), translated_addr);
			memset(&ms.dataflash[translated_addr], 0xFF, 0x100);
			modified = 1;
			break;
		  case 0x10: /* Byte program */
			log_debug("[%04X] * Dataflash Byte-Prog: 0x%X = %02X\n",
			  z80ex_get_reg(ms.z80, regPC),translated_addr,val);
			ms.dataflash[translated_addr] = val;
			modified = 1;
			break;
		  case 0x30: /* Chip erase, execute cmd is 0x30 */
			if (val != 0x30) break;
			log_debug("[%04X] * Dataflash Chip erase\n");
			memset(ms.dataflash, 0xFF, MEBIBYTE/2);
			modified = 1;
			break;
		  case 0x90: /* Read ID */
			log_debug("[%04X] * Dataflash Read ID\n", z80ex_get_reg(ms.z80, regPC));
			break;
		  default:
			log_error(
			  "[%04X] * INVALID DATAFLASH CMD SEQ: %02X %02X\n",
			  z80ex_get_reg(ms.z80, regPC), cmd, val);
			break;
		}
		cycle = 0;
	}

	return modified;
}

/* Open flash file from path and pull its contents to memory
 *
 * XXX: Mostly not complete, still don't know exactly how all of these will
 * work together.
 */
int flashtobuf(uint8_t *buf, const char *file_path, ssize_t sz)
{
	FILE *fd;

	fd = fopen(file_path, "rb");
	if (fd)
	{
		//fseek(codeflash_fd, 0, SEEK_END);
		/* TODO: Add debugout print here */
		//printf("Loading Codeflash ROM:\n  %s (%ld bytes)\n",
		//  codeflash_path, ftell(codeflash_fd));
		//fseek(codeflash_fd, 0, SEEK_SET);
		fread(buf, sizeof(uint8_t), sz, fd);
		fclose(fd);
		return 0;
	} else {
		/* XXX: Move this outside of this function */
		printf("Couldn't open flash file: %s\n", file_path);
		return 1;
	}
}

int buftoflash(uint8_t *buf, const char *file_path, ssize_t sz)
{
	FILE *fd;

	/* XXX: Move this print to debug out */
	/* XXX: Check error codes */
	printf("Writing dataflash...\n");
	fd = fopen(file_path, "wb");
	fwrite(buf, sizeof(uint8_t), sz, fd);
	fclose(fd);
	return 0;
}
