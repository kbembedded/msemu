#ifndef __MSEMU_H_
#define __MSEMU_H_

#include <string.h>
#include <stdint.h>
#include <z80ex/z80ex.h>

#define	MEBIBYTE	0x100000

// Default screen size
#define MS_LCD_WIDTH 	320
#define MS_LCD_HEIGHT	240

// Constants to identify each side of the LCD
#define MS_LCD_LEFT 	1
#define MS_LCD_RIGHT 	2

// Power state constants
#define MS_POWERSTATE_ON  1
#define MS_POWERSTATE_OFF 0

enum ms_device_map {
	CF 	= 0x00,
	RAM 	= 0x01,
	LCD_L 	= 0x02,
	DF 	= 0x03,
	LCD_R 	= 0x04,
	MODEM 	= 0x05,

	DEV_CNT,
};

typedef struct mshw {
	Z80EX_CONTEXT* z80;
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

	// Bits specify which interrupts have been triggered (returned on P3)
	uint8_t interrupt_mask;

	// Stores the page/device numbers of the two middle 16KB slots of address space
	uint8_t slot4000_page;
	uint8_t slot4000_device;
	uint8_t slot8000_page;
	uint8_t slot8000_device;

	uint8_t *codeflash;
	uint8_t *dataflash;

	// Signals if the dataflash contents have been changed.
	// Should be cleared once dataflash is written back to disk.
	uint8_t dataflash_updated;

	uint8_t key_matrix[10];

	// Holds power button status (returned in P9.4)
	uint8_t power_button;

	// Holds current power state (on or off)
	uint8_t power_state;
} MSHW;

#endif // __MSEMU_H_
