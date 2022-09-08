/* Mailstation Emulator
 *
 * Copyright (c) 2010 Jeff Bowman
 * Copyright (c) 2019 KBEmbedded
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <assert.h>
#include <ctype.h> // for isalnum
#include <memory.h>
#include <time.h>

#include "debug.h"
#include "mem.h"
#include "lcd.h"
#include "msemu.h"
#include "io.h"
#include "sizes.h"
#include "ui.h"

#include <SDL2/SDL.h>
#include <errno.h>
#include <stdlib.h>
#include <z80ex/z80ex.h>
#include <z80ex/z80ex_dasm.h>

const char* const ms_dev_map_text[] = {
	"CF",
	"RAM",
	"LCD_L",
	"DF",
	"LCD_R",
	"MODEM",
	NULL
};

//----------------------------------------------------------------------------
//
//  Convert uint8_t to BCD format
//
static unsigned char hex2bcd (unsigned char x)
{
	unsigned char y;
	y = (x / 10) << 4;
	y = y | (x % 10);
	return y;
}

#define DF_SN_OFFS     0x7FFC8

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
static void ms_set_df_rnd_serial(ms_ctx *ms)
{
	int i;
	uint8_t *df_buf = ms->df;
	uint8_t rnd;

	df_buf += DF_SN_OFFS;

	for (i = 0; i < 15; i++) {
		do {
			rnd = rand();
		} while (!isalnum(rnd));
		*df_buf = rnd;
		df_buf++;
	}

	*df_buf = '-';
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
static int ms_serial_valid(ms_ctx *ms)
{
	int i;
	uint8_t *df_buf = ms->df;
	int ret = MS_OK;

	df_buf += DF_SN_OFFS;

	for (i = 0; i < 16; i++) {
		if (!isalnum(*df_buf) && *df_buf != '-') ret = MS_ERR;
		df_buf++;
	}

	return ret;
}

//----------------------------------------------------------------------------
//
//  Resets Mailstation state
//
void ms_power_on_reset(ms_ctx *ms)
{
	/* NOTE: While it might seem like it should be done, the keyboard
	 * matrix, ms->key_matrix, should NOT be reset here. Since this is
	 * set/cleared by UI functions as time goes on, clearing this for
	 * a reset could lose keys that are being held down */

	lcd_init(ms);
	ram_init(ms, NULL);
	io_init(ms);
	ms->power_state = MS_POWERSTATE_ON;
	ms->interrupt_mask = 0;
	z80ex_reset(ms->z80);
}

//----------------------------------------------------------------------------
//
//  Disables emulation, displays opening screen
//
void ms_power_off(ms_ctx* ms)
{
	ms->power_state = MS_POWERSTATE_OFF;

	ui_splashscreen_show();
}

/* z80ex Read memory callback function.
 *
 * Return a Z80EX_BYTE (uint8_t) from the address given to us.
 * This function needs to figure out what slot the requested address lies in,
 * figure out what page of which device is in that slot, and return data.
 *
 */
Z80EX_BYTE z80ex_mread(
	Z80EX_CONTEXT *cpu,
	Z80EX_WORD addr,
	int m1_state,
	void *user_data)
{

	Z80EX_BYTE ret;
	ms_ctx* ms = (ms_ctx*)user_data;
	int slot = ((addr & 0xC000) >> 14);
	int dev, page;

	debug_testbp(bpMR, addr);

	/* slot4 and slot8 are dynamic, if the requested address falls
	 * in this range, then we need to set up the device we're going
	 * to talk to. The other two slots are hard-coded and the dev/page
	 * information is unused there.
	 *
	 * NOTE: It _might_ be possible to optimize this away in the future.
	 * NOTE: The SLOTX_DEV is stored in the PORT buffer with the upper
	 * 4 bits set, this can screw up our logic here. The reason for the
	 * bits being set is unknown at this time.
	 */
	switch (slot) {
	  case 0:
		dev = CF;
		page = 0;
		break;
	  /* TODO: Add page range check */
	  case 1:
		dev = (io_read(ms, SLOT4_DEV) & 0x0F);
		page = io_read(ms, SLOT4_PAGE);
		break;
	  case 2:
		dev = (io_read(ms, SLOT8_DEV) & 0x0F);
		page = io_read(ms, SLOT8_PAGE);
		break;
	  case 3:
		dev = RAM;
		page = 0;
		break;
	}


	/* Nearly all read functions are passed an absolute address inside the
	 * device. This is generally calculated by taking the lower 14bits of
	 * the address (this localizes the address inside the slot) and adding
	 * that to (page * page_size).
	 * In the case of the LCD which only uses a single page, just the
	 * lower 14bits of address are passed.
	 */
	switch (dev) {
	  case LCD_L:
	  case LCD_R:
		ret = lcd_read(ms, (addr & ~0xC000), dev);
		break;

	  case MODEM:
		ret = 0;
		log_debug(" * MODEM R is not supported\n");
		break;

	  case CF:
		ret = cf_read(ms, ((addr & ~0xC000) + (0x4000 * page)));
		break;

	  case DF:
		ret = df_read(ms, ((addr & ~0xC000) + (0x4000 * page)));
		break;

	  case RAM:
		ret = ram_read(ms, ((addr & ~0xC000) + (0x4000 * page)));
		log_debug(" * MEM   R [%04X] -> %02X\n", addr, ret);
		break;

	  default:
		log_error(" * MEM   R [%04X] FROM INVALID DEV %02X @ %04X\n",
		  addr, dev, z80ex_get_reg(ms->z80, regPC));
		ret = 0;
		break;
	}

	return ret;
}


/* z80ex Write memory callback function.
 *
 * Write a uint8_t to the address given to us.
 * This function needs to figure out what slot the requested address lies in,
 * figure out what page of which device is in that slot, and write data.
 */
void z80ex_mwrite(
	Z80EX_CONTEXT *cpu,
	Z80EX_WORD addr,
	Z80EX_BYTE val,
	void *user_data)
{

	ms_ctx* ms = (ms_ctx*)user_data;
	int slot = ((addr & 0xC000) >> 14);
	int dev = 0xFF, page = 0xFF;

	debug_testbp(bpMW, addr);

	/* slot4 and slot8 are dynamic, if the requested address falls
	 * in this range, then we need to set up the device we're going
	 * to talk to. The other two slots are hard-coded and the dev/page
	 * information is unused there.
	 *
	 * NOTE: It _might_ be possible to optimize this away in the future.
	 * NOTE: The SLOTX_DEV is stored in the PORT buffer with the upper
	 * 4 bits set, this can screw up our logic here. The reason for the
	 * bits being set is unknown at this time.
	 */
	switch (slot) {
	  case 0:
		dev = CF;
		page = 0;
		break;
	  /* TODO: Add page range check */
	  case 1:
		dev = (io_read(ms, SLOT4_DEV) & 0x0F);
		page = io_read(ms, SLOT4_PAGE);
		break;
	  case 2:
		dev = (io_read(ms, SLOT8_DEV) & 0x0F);
		page = io_read(ms, SLOT8_PAGE);
		break;
	  case 3:
		dev = RAM;
		page = 0;
		break;
	}


	switch (dev) {
	/* Nearly all read functions are passed an absolute address inside the
	 * device. This is generally calculated by taking the lower 14bits of
	 * the address (this localizes the address inside the slot) and adding
	 * that to (page * page_size).
	 * In the case of the LCD which only uses a single page, just the
	 * lower 14bits of address are passed.
	 */
	  case LCD_L:
	  case LCD_R:
		lcd_write(ms, (addr & ~0xC000), val, dev);
		break;

	  case DF:
		df_write(ms, ((addr & ~0xC000) + (0x4000 * page)), val);
		break;

	  case MODEM:
		log_debug(" * MODEM W is not supported\n");
		break;

	  case RAM:
		ram_write(ms, ((addr & ~0xC000) + (0x4000 * page)), val);
		log_debug(" * MEM   W [%04X] <- %02X\n", addr, val);
		break;

	  case CF:
		log_error(" * CF    W [%04X] INVALID, CANNOT W TO CF @ %04X\n",
		  addr, z80ex_get_reg(ms->z80, regPC));
		break;

	  default:
		log_error(" * MEM   W [%04X] TO INVALID DEV %02X\n @ %04X",
		  addr, dev), z80ex_get_reg(ms->z80, regPC);
		break;
	}
}


/* z80ex Read from PORT callback function
 *
 * Return a Z80EX_BYTE (uint8_t) value of the requested PORT number.
 * Many ports are read and written as normal and have no emulation impact.
 * However a handful of ports do special things and are required for useable
 * emulation. These cases are specially handled as needed.
 *
 * See Mailstation documentation for specific PORT layouts and uses.
 */
Z80EX_BYTE z80ex_pread (
	Z80EX_CONTEXT *cpu,
	Z80EX_WORD port,
	void *user_data)
{

	ms_ctx* ms = (ms_ctx*)user_data;

	time_t theTime;
	struct tm *rtc_time = NULL;

	uint16_t kbaddr;
	uint8_t kbresult;
	uint8_t tmp;
	int i;

	Z80EX_BYTE ret = 0;

	/* Z80 IO commands end up with the upper byte of the port address set,
	 * this appears to be unused in the MS and only the lower byte should
	 * be evaluated for the port number */
	port &= 0xFF;

	/* Get the time only if we're accessing timer registers */
	if (port >= RTC_SEC && port <= RTC_10YR) {
		time( &theTime );
		rtc_time = localtime(&theTime);
	}

	log_debug(" * IO    R [  %02X] -> %02X\n", port, io_read(ms, port));

	switch (port) {
	  case KEYBOARD:// emulate keyboard matrix output
		// keyboard row is 10 bits wide, P1.x = low bits, P2.0-1 = high bits
		/* XXX: This is REAL clunky */
		kbaddr = (io_read(ms, KEYBOARD)) + (((io_read(ms, MISC2)) & 3) << 8);

		// all returned bits should be high unless a key is pressed
		kbresult = 0xFF;

		// check for a key pressed in active row(s)
		for (i = 0; i < 10; i++) {
			if (!((kbaddr >> i) & 1)) kbresult &= ms->key_matrix[i];
		}
		ret = kbresult;

		break;

	  // return currently triggered interrupts
	  case IRQ_MASK:
		ret = ms->interrupt_mask;
		break;

	  // acknowledge power good + power button status
	  // Also has some parport control bits
	  case MISC9:
		ret = 0;
		if (ms->ac_status) ret |= MISC9_AC_GOOD;
		if (ms->power_button_n) ret |= MISC9_PWR_BTN;
		switch (ms->batt_status) {
		case BATT_HIGH:
			ret |= (MISC9_BATT_HIGH | MISC9_BATT_GOOD);
			break;
		case BATT_LOW:
			ret |= MISC9_BATT_GOOD;
			break;
		}
		ret = ret | (io_read(ms, MISC9) & 0x0F);
		break;

	  // These are all for the RTC
	  case RTC_SEC:		//seconds
		ret = (hex2bcd(rtc_time->tm_sec) & 0x0F);
		break;
	  case RTC_10SEC:	//10 seconds
		ret = ((hex2bcd(rtc_time->tm_sec) & 0xF0) >> 4);
		break;
	  case RTC_MIN:		// minutes
		ret = (hex2bcd(rtc_time->tm_min) & 0x0F);
		break;
	  case RTC_10MIN:	// 10 minutes
		ret = ((hex2bcd(rtc_time->tm_min) & 0xF0) >> 4);
		break;
	  case RTC_HR:		// hours
		ret = (hex2bcd(rtc_time->tm_hour) & 0x0F);
		break;
	  case RTC_10HR:	// 10 hours
		ret = ((hex2bcd(rtc_time->tm_hour) & 0xF0) >> 4);
		break;
	  case RTC_DOW:		// day of week
		ret = rtc_time->tm_wday;
		break;
	  case RTC_DOM:		// days
		ret = (hex2bcd(rtc_time->tm_mday) & 0x0F);
		break;
	  case RTC_10DOM:	// 10 days
		ret = ((hex2bcd(rtc_time->tm_mday) & 0xF0) >> 4);
		break;
	  case RTC_MON:		// months
		ret = (hex2bcd(rtc_time->tm_mon + 1) & 0x0F);
		break;
	  case RTC_10MON:	// 10 months
		ret = ((hex2bcd(rtc_time->tm_mon + 1) & 0xF0) >> 4);
		break;
	  case RTC_YR:		// years since 1980
		ret = (hex2bcd(rtc_time->tm_year + 80) & 0x0F);
		break;
	  case RTC_10YR:	// 10s years since 1980
		ret = ((hex2bcd(rtc_time->tm_year + 80) & 0xF0) >> 4);
		break;

	  default:
		ret = io_read(ms, port);
		break;
	}

	return ret;
}



/* z80ex Write to PORT callback function
 *
 * Write a uint8_t value to the port address given to us.
 * Many ports are read and written as normal and have no emulation impact.
 * However a handful of ports do special things and are required for useable
 * emulation. These cases are specially handled as needed.
 *
 * See Mailstation documentation for specific PORT layouts and uses.
 */
void z80ex_pwrite (
	Z80EX_CONTEXT *cpu,
	Z80EX_WORD port,
	Z80EX_BYTE val,
	void *user_data)
{
	uint8_t tmp;
	ms_ctx* ms = (ms_ctx*)user_data;


	/* Z80 IO commands end up with the upper byte of the port address set,
	 * this appears to be unused in the MS and only the lower byte should
	 * be evaluated for the port number */
	port &= 0xFF;

	log_debug(" * IO    W [  %02X] <- %02X\n", port, val);

	switch (port) {
	  case MISC2:
		ui_update_led(!!(val & MISC2_LED_BIT));
		io_write(ms, port, val);
		break;

	  // set interrupt masks
	  case IRQ_MASK:
		ms->interrupt_mask &= val;
		io_write(ms, port, val);
		break;

	  // check for hardware power off bit in P28
	  case UNKNOWN0x28:
		if (val & 1) {
			printf("POWER OFF\n");
			ms_power_off(ms);
		}
		io_write(ms, port, val);
		break;

	  // otherwise just save written value
	  default:
		io_write(ms, port, val);
		break;
	}
}



/* z80ex emulation requires that intread callback be defined. Used for IM 2.
 * Under normal execution, IM 2 is not used, however, some of the hacks used
 * for installing a custom interrupt handler use IM 2. For these to work, we
 * must provide intread capability. In theory, the return data should be fully
 * random as in real hardware these pins all go high impedance with no pull
 * resistors.
 *
 * XXX: In practice, this causes issues if the return is 0xff, though this
 * is likely application depedant. Because of that, just return 0x00 for
 * now.
 */
Z80EX_BYTE z80ex_intread (
	Z80EX_CONTEXT *cpu,
	void *user_data)
{
	return 0x00;
}

/* Processes interrupts. Should be called on every interrupt period.
 * Returns number of tstates spent processing interrupt.
 *
 * NOTE: It is unknown if an NMI ever occurs on the Mailstation at this time.
 * Disassembly of the firmware yields no calls to RETN, however the code at
 * 0x0066 is set up to handle an NMI. That is, 0x0065 is a NOP since the opcode
 * at 0x0066 is the start of a jump. If the jump were to start at 0x0065 then
 * 0x0066 would not have a valid instruction. This hints that there is some
 * expectation of an NMI occurring.
 */
int process_interrupts (ms_ctx* ms)
{
	static int icount = 0;

	/* XXX: This is a hack and the whole interrupt system needs to be
	 * refactored at some point in the future.
	 * The next line is needed because of a potential condition where an
	 * interrupt would be called, but the Z80 has interrupts currently
	 * disabled. This results in interrupt_mask getting set, but z80ex_int
	 * call gets rejected because interrupts are disabled. With the mask
	 * set, all future time interrupts here just get ignored because of
	 * the if statements below checking the port and the interrupt_mask.
	 * A proper interrupt implementation needs to occur at some point,
	 * however, the CPU in the MS probably needs to be a bit better
	 * understood first.*/
	if (!z80ex_int_possible(ms->z80)) return 0;

	// Interrupt occurs at 64hz.  So this counter reduces to 1 sec intervals
	if (icount++ >= 64)
	{
		icount = 0;

		// do time16 interrupt
		if ((io_read(ms, IRQ_MASK) & 0x10) && !(ms->interrupt_mask & 0x10))
		{
			ms->interrupt_mask |= 0x10;
			return z80ex_int(ms->z80);
		}
	}

	// Trigger keyboard interrupt if necessary (64hz)
	if ((io_read(ms, IRQ_MASK) & 2) && !(ms->interrupt_mask & 2))
	{
		ms->interrupt_mask |= 2;
		return z80ex_int(ms->z80);
	}

	/* XXX: Hack to always call interrupt.
	 * When testing FyOS v0.1, if this int doesn't fire then it breaks.
	 * v0.1 Uses INT mode 2 with a hack to implement its own INT handler,
	 * and for some reason needs an interrupt more than just the two
	 * masks that are used at the moment. There might be another timer?
	 * Either way, this should be addressed at some point. */
	return z80ex_int(ms->z80);
	// Otherwise ignore this
	return 0;
}

/* Enable, disable, or toggle AC adapter status
 *
 * Writes to the ms_ctx tracking variable
 */
void ms_power_ac_set_status(ms_ctx *ms, int status)
{
	if (status == AC_TOGGLE) {
		ms->ac_status ^= AC_GOOD;
	} else {
		ms->ac_status = status;
	}

	switch (ms->ac_status) {
	case AC_GOOD:
		printf("AC good\n");
		break;
	case AC_FAIL:
		printf("AC fail\n");
		break;
	}
}

/* Set battery voltage status, high, low, depleted
 *
 * Writes to the ms_ctx tracking variable
 */
void ms_power_batt_set_status(ms_ctx *ms, int status)
{
	if (status == BATT_CYCLE) {
		ms->batt_status++;
		if (ms->batt_status == BATT_CYCLE)
			ms->batt_status = BATT_DEPLETE;
	} else {
		ms->batt_status = status;
	}

	ui_update_battery(ms->batt_status);
}

int ms_init(ms_ctx* ms, ms_opts* options)
{
	/* Allocate and clear buffers.
	 * Codeflash is 1 MiB
	 * Dataflash is 512 KiB
	 * RAM is 128 KiB
	 * IO is 256 B
	 * LCD has two buffers, 8bit and 1bit. The Z80 emulation writes to the
	 * 1bit buffer, this then translates to the 8bit buffer for SDLs use.
	 */

	/* Seed (non-critical) RNG with time */
	srand((unsigned int)time(NULL));

	/* Initialize hardware states of the MailStation. */
	ms->interrupt_mask = 0;
	ms->power_button_n = 1;
	ms->power_state = MS_POWERSTATE_OFF;
	ms_power_ac_set_status(ms, options->ac_start);
	ms_power_batt_set_status(ms, options->batt_start);

	/* Set up keyboard emulation array */
	memset(ms->key_matrix, 0xff, sizeof(ms->key_matrix));

	/* Create and set up Z80 machine and access funcs */
	ms->z80 = z80ex_create(
		z80ex_mread, (void*)ms,
		z80ex_mwrite, (void*)ms,
		z80ex_pread, (void*)ms,
		z80ex_pwrite, (void*)ms,
		z80ex_intread, (void*)ms
	);

	/* Initialize buffers for emulating the various peripherals */
	if (lcd_init(ms)) return MS_ERR;
	if (io_init(ms)) return MS_ERR;
	if (ram_init(ms, options)) return MS_ERR;
	if (cf_init(ms, options) == ENOENT) return MS_ERR;

	/* If opening a new, blank, DF buffer, then assign it a random MS
	 * compatible serial number.
	 * If the DF file opened has an invalid serial number, just complain
	 * loudly with a warning. */
	if (df_init(ms, options) == ENOENT) {
		ms_set_df_rnd_serial(ms);
	}
	if (ms_serial_valid(ms)) {
		printf("WARNING! Dataflash does not have valid serial num!\n");
		printf("This may not be a dataflash image!\n\n");
	}

	/* Set up debug hooks */
	debug_init(ms, z80ex_mread);

	printf("\nPress ctrl+c to enter interactive Mailstation debugger\n");

	return MS_OK;
}

int ms_deinit(ms_ctx *ms, ms_opts *options)
{
	io_deinit(ms);
	ram_deinit(ms);
	lcd_deinit(ms);
	cf_deinit(ms, options);
	df_deinit(ms, options);

	return 0;
}

int ms_run(ms_ctx* ms)
{
	// TODO: Consider removing dependency on SDL here and having
	//     hooks for the UI code to attach to instead.

	int execute_counter = 0;
	int tstate_counter = 0;
	/* XXX: interrupt_period can change if running at different freq */
	int interrupt_period = 187500;
	int exitemu = 0;
	uint32_t lasttick = SDL_GetTicks();
	uint32_t currenttick;

	/* NOTE:
	 * The z80ex library can hook in to RETI opcodes. Allowing us to exec
	 * code here to emulate PIO or other. At this time, I don't think
	 * there is any reason to need this.
	 */

	// Display startup message
	ms_power_off(ms);

	lasttick = SDL_GetTicks();

	while (!exitemu)
	{
		if (debug_isbreak()) {
			if (debug_prompt() == -1) break;
		}

		/* XXX: Can replace with SDL_TICKS_PASSED with new
		 * SDL version. */
		currenttick = SDL_GetTicks();

		/* Let the Z80 process code in chunks of time to better match
		 * real time Mailstation behavior.
		 *
		 * Execution is gated to roughly a regular timer interrupt rate.
		 * z80ex provides a callback for every T state that could be
		 * used to gate execution in the future.
		 * Every 15 ms (64 hz) a timer interrupt fires in the MS to
		 * handle key input, with a counter incrementing every 1 s. Run
		 * Z80 as fast as possible for 15 ms worth of T states, then
		 * stop and try to INT. The timer to raise an INT could be done
		 * async to more accurately model the MS.
		 *
		 * The execution loop below will run until a pre-determined num
		 * of T states has passed (based on default 12 MHz execution),
		 * and the last Z80 step was a complete instruction and not a
		 * prefix. Some opcodes actually have two bytes associated with
		 * the actual instruction. After this, an INT is attempted.
		 *
		 * Execution loop will only stop prematurely if a breakpoint on
		 * the PC is hit. Interrupting with ctrl+c in terminal will
		 * cause this loop to exit after the next instruction. Pressing
		 * esc on the SDL window will only process after this loop has
		 * completed.*/
		if (ms->power_state == MS_POWERSTATE_ON) {
			execute_counter += currenttick - lasttick;
			if (execute_counter > 15 || debug_isbreak()) {
				if (execute_counter > 15) execute_counter = 0;

				while (tstate_counter < interrupt_period) {
					debug_dasm();
					do {
						tstate_counter += z80ex_step(ms->z80);
					} while (z80ex_last_op_type(ms->z80));

					if (debug_testbp(bpPC,
					  z80ex_get_reg(ms->z80, regPC))) {
						break;
					}
				}
			}

			if (tstate_counter >= interrupt_period) {
				tstate_counter += process_interrupts(ms);
				tstate_counter %= interrupt_period;
			}

		}

		ui_update_lcd();

		if (ui_kbd_process(ms)) break;

		ui_render();

		// Update SDL ticks
		lasttick = currenttick;
	}

	return MS_OK;
}
