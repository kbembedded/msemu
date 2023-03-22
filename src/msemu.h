#ifndef __MSEMU_H_
#define __MSEMU_H_

#include <stdint.h>
#include <z80ex/z80ex.h>

// Return codes
#define MS_OK    0
#define MS_ERR   1

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

enum ms_ac_status {
	AC_FAIL = 0,
	AC_GOOD = 1,
	AC_TOGGLE,
};

enum ms_batt_status {
	BATT_DEPLETE,
	BATT_LOW,
	BATT_HIGH,
	BATT_CYCLE,
};

typedef struct ms_ctx {
	Z80EX_CONTEXT* z80;

	uint8_t *io;
	uint8_t *df;
	uint8_t *cf;
	uint8_t *ram;
	uint8_t *ram_image;

	uint32_t *lcd_datRGBA8888;
	uint8_t *lcd_dat1bit;

	// Stores current selected LCD column.
	int lcd_cas;

	// Bits specify which interrupts have been triggered (returned on P3)
	// XXX: I think this can be removed
	uint8_t interrupt_mask;

	uint8_t key_matrix[10];

	// Holds current power state (on or off)
	// XXX: I think this can go away?
	uint8_t power_state;

	/* Inputs to the CPU IO that are generated in hardware and wired to
	 * IO ports. These need to be maintained in the context of the machine
	 * rather than the IO port itself since the MailStation firmware could
	 * (and does) write to these bits when it really shouldn't.
	 */

	// State of the AC adapter
	int ac_status;

	// State of the battery
	int batt_status;

	// State of the power button
	/* This is not in the keyboard envelope, and is instead directly
	 * connected to an IO pin since this is also a part of the WAKE
	 * circuit. However, a scancode is indeed generated for this key. This
	 * means that the firmware itself reads the IO pin and generates a
	 * scancode when needed. This button is active low and its status is
	 * returned in P9.4
	 */
	int power_button_n;

	//Store the current device and page offset for each slot.
	/* This is used to save the current offset for each slot once the
	 * respective IO port is written. Reduces need to calculate the base
	 * offset on every mem access.
	 *
	 * NOTE! This didn't have as much of an impact as I'd hoped. Might not
	 * be worth maintaining this.
	 */
	int slot4_dev;
	int slot4_page_offs;
	int slot8_dev;
	int slot8_page_offs;
} ms_ctx;

typedef struct ms_opts {
	// Codeflash path
	char *cf_path;

	// Dataflash path
	char *df_path;

	// RAM image path
	char *ram_path;

	// Save dataflash back to disk
	int df_save_to_disk;

	// Initial battery state
	int batt_start;

	// Initial AC state;
	int ac_start;
} ms_opts;

/**
 * Initializes a mailstation emulator.
 *
 * ms      - ref to mailstation emulator
 * options - initialization options
 *
 * Returns `MS_OK` on success, error code on failure
 */
int ms_init(ms_ctx* ms, ms_opts* options);
int ms_deinit(ms_ctx* ms, ms_opts* options);

/**
 * Runs the mailstation emulation
 *
 * ms - ref to mailstation emulator
 *
 * Returns `MS_OK` on success, error code on failure
 */
int ms_run(ms_ctx* ms);

void ms_power_on_reset(ms_ctx *ms);
void ms_power_hint(ms_ctx *ms);
void ms_power_batt_set_status(ms_ctx *ms, int status);
void ms_power_ac_set_status(ms_ctx *ms, int status);

#endif // __MSEMU_H_
