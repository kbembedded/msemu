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

typedef struct ms_hw {
	Z80EX_CONTEXT* z80;

	uintptr_t slot_map[4];

	uint8_t *io;
	uint8_t *df;
	uint8_t *cf;
	uint8_t *ram;
	uintptr_t dev_map[DEV_CNT];

	uint32_t *lcd_datRGBA8888;
	/* TODO: Might be able to remove this 1bit screen representation */
	uint8_t *lcd_dat1bit;

	// Stores current LCD column.
	int lcd_cas;

	// timestamp of last lcd draw, used to decide
	// whether the lcd should be redrawn.
	uint32_t lcd_lastupdate;

	// Bits specify which interrupts have been triggered (returned on P3)
	uint8_t interrupt_mask;

	uint8_t key_matrix[10];

	// Holds power button status (returned in P9.4)
	uint8_t power_button;

	// Holds current power state (on or off)
	uint8_t power_state;
} ms_ctx;

typedef struct ms_opts {
	// Codeflash path
	char* cf_path;

	// Dataflash path
	char* df_path;

	// Save dataflash back to disk
	int df_save_to_disk;
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

#endif // __MSEMU_H_
