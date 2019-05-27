#ifndef __MSEMU_H__
#define __MSEMU_H_

#include <string.h>
#include <stdint.h>

#define	MEBIBYTE	0x100000

void DebugOut(char *mystring,...);
void ErrorOut(char *mystring,...);

struct mshw {
	uint8_t *ram;
	uint8_t *io;
	uint8_t *lcd_dat8bit;
	/* TODO: Might be able to remove this 1bit screen representation */
	uint8_t *lcd_dat1bit;
	uint8_t *codeflash;
	uint8_t *dataflash;
	uint8_t key_matrix[10];
};

#endif
