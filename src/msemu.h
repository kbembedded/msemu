#ifndef __MSEMU_H_
#define __MSEMU_H_

#include <string.h>
#include <stdint.h>
#include <z80ex/z80ex.h>

#define	MEBIBYTE	0x100000

// Return codes
#define MS_OK    0
#define MS_ERR   1

// Debugger Flags
#define MS_DBG_ON           1 << 0
#define MS_DBG_SINGLE_STEP  1 << 1

// Default screen size
#define MS_LCD_WIDTH 	320
#define MS_LCD_HEIGHT	240

// Power state constants
#define MS_POWERSTATE_ON  1
#define MS_POWERSTATE_OFF 0

enum ms_dev_map {
	CF    = 0x00,
	RAM   = 0x01,
	LCD_L = 0x02,
	DF    = 0x03,
	LCD_R = 0x04,
	MODEM = 0x05,

	DEV_CNT,
};

enum ms_port_map {
	KEYBOARD	= 0x01,
	MISC2		= 0x02,
		/*	p3.7 = Caller id handler
			p3.5 = maybe rtc???
			p3.6 = Modem handler
			p3.4 = increment time16
			p3.3 = null
			p3.0 = null
			p3.1 = Keyboard handler
			p3.2 = null
		*/
	IRQ_MASK	= 0x03,
	UNKNOWN0x4	= 0x04,
	SLOT4_PAGE	= 0x05,
	SLOT4_DEV	= 0x06,
	SLOT8_PAGE	= 0x07,
	SLOT8_DEV	= 0x08,
	MISC9		= 0x09, /* Printer ctrl, pwr ok, pwr btn */
	RTC_SEC		= 0x10, /* BCD, ones place seconds */
	RTC_10SEC	= 0x11, /* BCD, tens place seconds */
	RTC_MIN		= 0x12, /* BCD, ones place minutes */
	RTC_10MIN	= 0x13, /* BCD, tens place minutes */
	RTC_HR		= 0x14, /* BCD, ones place hours */
	RTC_10HR	= 0x15, /* BCD, tens place hours */
	RTC_DOW		= 0x16, /* BCD, day of week */
	RTC_DOM		= 0x17, /* BCD, ones place day of month */
	RTC_10DOM	= 0x18, /* BCD, tens place day of month */
	RTC_MON		= 0x19, /* BCD, ones place month */
	RTC_10MON	= 0x1A, /* BCD, tens place month */
	RTC_YR		= 0x1B, /* BCD, ones place years since 1980 */
	RTC_10YR	= 0x1C, /* BCD, tens place years since 1980 */
	RTC_CTRL1	= 0x1D, /* Unknown */
	RTC_CTRL2	= 0x1E, /* Unknown */
	RTC_CTRL3	= 0x1F, /* Unknown */

	PRINT_STATUS	= 0x21, /* Unknown */
	PRINT_DDR	= 0x2C,
	PRINT_DR	= 0x2D,

	UNKNOWN0x28	= 0x28,

	PORT_CNT	= 0x10000, /* XXX: Currently 64 KiB is used, not sure
				    * what the actual needed amount is.
				    */
};

typedef struct mshw {
	Z80EX_CONTEXT* z80;

	uintptr_t slot_map[4];

	uint8_t *io;
	uintptr_t dev_map[DEV_CNT];

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

	// Signals if the dataflash contents have been changed.
	// Should be cleared once dataflash is written back to disk.
	uint8_t dataflash_updated;

	uint8_t key_matrix[10];

	// Holds power button status (returned in P9.4)
	uint8_t power_button;

	// Holds current power state (on or off)
	uint8_t power_state;

	// Holds debugging state (on or off)
	uint8_t debugger_state;

	// Single breakpoint on a specified PC
	int32_t bp;
} MSHW;

typedef struct MsOpts {
	// Codeflash path
	char* cf_path;

	// Dataflash path
	char* df_path;
} MsOpts;

/**
 * Initializes a mailstation emulator.
 *
 * ms      - ref to mailstation emulator
 * options - initialization options
 *
 * Returns `MS_OK` on success, error code on failure
 */
int ms_init(MSHW* ms, MsOpts* options);

/**
 * Runs the mailstation emulation
 *
 * ms - ref to mailstation emulator
 *
 * Returns `MS_OK` on success, error code on failure
 */
int ms_run(MSHW* ms);

#endif // __MSEMU_H_
