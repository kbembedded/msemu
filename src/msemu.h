#ifndef __MSEMU_H_
#define __MSEMU_H_

#include <string.h>
#include <stdint.h>

#define	MEBIBYTE	0x100000

// Default screen size
#define MS_LCD_WIDTH 	320
#define MS_LCD_HEIGHT	240

// Constants to identify each side of the LCD
#define MS_LCD_LEFT 	1
#define MS_LCD_RIGHT 	2

struct mshw {
	uint8_t *ram;
	uint8_t *io;

	uint8_t *lcd_dat8bit;
	/* TODO: Might be able to remove this 1bit screen representation */
	uint8_t *lcd_dat1bit;

	// Stores current LCD column.
	uint8_t lcd_cas;

	// timestamp of last lcd draw, used to decide
	// whether the lcd should be redrawn.
	uint32_t lcd_lastupdate;


	uint8_t *codeflash;
	uint8_t *dataflash;
	uint8_t key_matrix[10];
};

#endif // __MSEMU_H_
