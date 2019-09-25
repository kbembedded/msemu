/* Mailstation Emulator
 *
 * Copyright (c) 2010 Jeff Bowman
 * Copyright (c) 2019 KBEmbedded
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>
#include "rawcga.h"
#include "logger.h"
#include "msemu.h"
#include "flashops.h"
#include "ui.h"
#include "debug.h"

#include <SDL/SDL.h>
#include <SDL/SDL_rotozoom.h>
#include <z80ex/z80ex.h>
#include <z80ex/z80ex_dasm.h>

// Default entry of color palette to draw Mailstation LCD with
uint8_t LCD_fg_color = 3;  // LCD black
uint8_t LCD_bg_color = 2;  // LCD green

int debug_console;

// This table translates PC scancodes to the Mailstation key matrix
int32_t keyTranslateTable[10][8] = {
	{ SDLK_HOME, SDLK_END, 0, SDLK_F1, SDLK_F2, SDLK_F3, SDLK_F4, SDLK_F5 },
	{ 0, 0, 0, SDLK_AT, 0, 0, 0, SDLK_PAGEUP },
	{ SDLK_BACKQUOTE, SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5, SDLK_6, SDLK_7 },
	{ SDLK_8, SDLK_9, SDLK_0, SDLK_MINUS, SDLK_EQUALS, SDLK_BACKSPACE, SDLK_BACKSLASH, SDLK_PAGEDOWN },
	{ SDLK_TAB, SDLK_q, SDLK_w, SDLK_e, SDLK_r, SDLK_t, SDLK_y, SDLK_u },
	{ SDLK_i, SDLK_o, SDLK_p, SDLK_LEFTBRACKET, SDLK_RIGHTBRACKET, SDLK_SEMICOLON, SDLK_QUOTE, SDLK_RETURN },
	{ SDLK_CAPSLOCK, SDLK_a, SDLK_s, SDLK_d, SDLK_f, SDLK_g, SDLK_h, SDLK_j },
	{ SDLK_k, SDLK_l, SDLK_COMMA, SDLK_PERIOD, SDLK_SLASH, SDLK_UP, SDLK_DOWN, SDLK_RIGHT },
	{ SDLK_LSHIFT, SDLK_z, SDLK_x, SDLK_c, SDLK_v, SDLK_b, SDLK_n, SDLK_m },
	{ SDLK_LCTRL, 0, 0, SDLK_SPACE, 0, 0, SDLK_RSHIFT, SDLK_LEFT }
};

//----------------------------------------------------------------------------
//
//  Convert uint8_t to BCD format
//
unsigned char hex2bcd (unsigned char x)
{
	unsigned char y;
	y = (x / 10) << 4;
	y = y | (x % 10);
	return y;
}

//----------------------------------------------------------------------------
//
//  Disables emulation, displays opening screen
//
void powerOff(MSHW* ms)
{
	ms->power_state = MS_POWERSTATE_OFF;

	// clear LCD
	memset(ms->lcd_dat8bit, 0, MS_LCD_WIDTH * MS_LCD_HEIGHT);
	ui_drawSplashScreen();
}

//----------------------------------------------------------------------------
//
//  Emulates writing to Mailstation LCD device
//
void writeLCD(MSHW* ms, uint16_t newaddr, uint8_t val, int lcdnum)
{
	uint8_t *lcd_ptr;
	if (lcdnum == LCD_L) lcd_ptr = ms->lcd_dat1bit;
	/* XXX: Fix the use of this magic number, replace with a const */
	else lcd_ptr = &ms->lcd_dat1bit[4800];

	/* XXX: This might need to be reworked to use non-viewable LCD memory */
	// Wraps memory address if out of bounds
	if (newaddr >= 240) newaddr %= 240;

	// Check CAS bit on P2
	if (ms->io[MISC2] & 8)
	{
		log_debug(" * LCD%s W [%04X] <- %02X\n",
		  lcdnum == LCD_L ? "_L" : "_R", newaddr, val);

		// Write data to currently selected LCD column.
		// This is just used for reading back LCD contents to the Mailstation quickly.
		lcd_ptr[newaddr + (ms->lcd_cas * 240)] = val;


		/*
			Write directly to newer 8-bit lcd_data8 buffer now too.
			This is what will actually be drawn on the emulator screen now.
		*/

		// Reverse column # (MS col #0 starts on right side)
		int x = 19 - ms->lcd_cas;
		// Use right half if necessary
		if (lcdnum == LCD_R) x += 20;

		// Write out all 8 bits to separate bytes, using the current emulated LCD color
		int n;
		for (n = 0; n < 8; n++)
		{
			ms->lcd_dat8bit[n + (x * 8) + (newaddr * 320)] = ((val >> n) & 1 ? LCD_fg_color : LCD_bg_color);
		}

		// Let main loop know to update screen with new LCD data
		ms->lcd_lastupdate = SDL_GetTicks();
	} else {
		log_debug(" * LCD%s W [ CAS] <- %02X\n",
		  lcdnum == LCD_L ? "_L" : "_R", newaddr, val);

		// If CAS line is low, set current column instead
		ms->lcd_cas = val;
	}
}


//----------------------------------------------------------------------------
//
//  Emulates reading from Mailstation LCD
//
uint8_t readLCD(MSHW* ms, uint16_t newaddr, int lcdnum)
{
	uint8_t *lcd_ptr;
	uint8_t ret;

	if (lcdnum == LCD_L) lcd_ptr = ms->lcd_dat1bit;
	/* XXX: Fix the use of this magic number, replace with a const */
	else lcd_ptr = &ms->lcd_dat1bit[4800];

	/* XXX: This might need to be reworked to use non-viewable LCD memory */
	// Wraps memory address if out of bounds
	if (newaddr >= 240) newaddr %= 240;

	// Check CAS bit on P2
	if (ms->io[MISC2] & 8)
	{
		// Return data on currently selected LCD column
		ret = lcd_ptr[newaddr + (ms->lcd_cas * 240)];
	} else {
		// Not sure what this normally returns when CAS bit low!
		ret = ms->lcd_cas;
	}

	log_debug(" * LCD%s R [%04X] -> %02X\n",
	  lcdnum == LCD_L ? "_L" : "_R", newaddr, ret);

	return ret;
}

/* z80ex Read memory callback function.
 *
 * Return a uint8_t from the address given to us.
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
	MSHW* ms = (MSHW*)user_data;
	int slot = ((addr & 0xC000) >> 14);
	int dev = 0xFF;

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
		break;
	  /* TODO: Add page range check */
	  case 1:
		dev = (ms->io[SLOT4_DEV] & 0x0F);
		break;
	  case 2:
		dev = (ms->io[SLOT8_DEV] & 0x0F);
		break;
	  case 3:
		dev = RAM;
		break;
	}


	switch (dev) {
	  /* Right now, readLCD() needs an addr & 0x3FFF.
	   * This falls within the range of the buffer regardless of the slot
	   * the actual device is in.
	   *
	   * Unlike the write path, reading from dataflash doesn't require an
	   * external call and can be handled like CF or RAM.
	   */
	  case LCD_L:
	  case LCD_R:
		ret = readLCD(ms, (addr - (slot << 14)), dev);
		break;

	  case MODEM:
		ret = 0;
		log_debug(" * MODEM R is not supported\n");
		break;

	  case CF:
	  case DF:
	  case RAM:
		ret = *(uint8_t *)(ms->slot_map[slot] + (addr & 0x3FFF));
		log_debug(" * MEM   R [%04X] -> %02X\n", addr, ret);
		break;

	  default:
		log_error(" * MEM   R [%04X] FROM INVALID DEV %02X\n", addr, dev);
		ret = 0;
		break;
	}

	return ret;
}


/* z80ex Write memory callback function.
 *
 * Write a uint8_t to the address given to us.
 * This function needs to figure out what slot the requested address lies in,
 * figure out what page of which device is in that slot, and return data.
 */
void z80ex_mwrite(
	Z80EX_CONTEXT *cpu,
	Z80EX_WORD addr,
	Z80EX_BYTE val,
	void *user_data)
{

	MSHW* ms = (MSHW*)user_data;
	int slot = ((addr & 0xC000) >> 14);
	int dev = 0xFF, page = 0xFF;

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
		dev = (ms->io[SLOT4_DEV] & 0x0F);
		page = ms->io[SLOT4_PAGE];
		break;
	  case 2:
		dev = (ms->io[SLOT8_DEV] & 0x0F);
		page = ms->io[SLOT8_PAGE];
		break;
	  case 3:
		dev = RAM;
		page = 0;
		break;
	}


	switch (dev) {
	  /* Right now, writeLCD() and writeDataflash() need an addr & 0x3FFF.
	   * This falls within the range of the buffer regardless of the slot
	   * the actual device is in. Since slots are paged, the final address
	   * passed to writeDataflash() is offset by page_sz * page_num
	   */
	  case LCD_L:
	  case LCD_R:
		writeLCD(ms, (addr - (slot << 14)), val, dev);
		break;
	  case DF:
		ms->dataflash_updated = writeDataflash(ms,
		  ((addr - (slot << 14)) + (0x4000 * page)), val);
		break;

	  case MODEM:
		log_debug(" * MODEM W is not supported\n");
		break;

	  case RAM:
		*(uint8_t *)(ms->slot_map[slot] + (addr & 0x3FFF)) = val;
		log_debug(" * MEM   W [%04X] <- %02X\n", addr, val);
		break;

	  case CF:
		log_error(" * CF    W [%04X] INVALID, CANNOT W TO CF\n",
		  addr);
		break;

	  default:
		log_error(" * MEM   W [%04X] TO INVALID DEV %02X\n", addr, dev);
		break;
	}
}


/* z80ex Read from PORT callback function
 *
 * Return a Z80EX_BYTE (uint8_t basically) value of the requested PORT number.
 * Many ports are read and written as normal and have no emulation impact.
 * However a handful of ports do special things and are required for useable
 * emulation. These cases are specially handled as needed.
 *
 * See Mailstation documentation for specific PORT layouts and uses.
 */
/* TODO: Use enum for bit positions within regs */
Z80EX_BYTE z80ex_pread (
	Z80EX_CONTEXT *cpu,
	Z80EX_WORD port,
	void *user_data)
{

	MSHW* ms = (MSHW*)user_data;

	time_t theTime;
	struct tm *rtc_time = NULL;

	uint16_t kbaddr;
	uint8_t kbresult;
	int i;

	Z80EX_BYTE ret = 0;

	/* TODO: Refactor this so we're not getting the time on EVERY single
	 * PORT read.
	 */
	time( &theTime );
	rtc_time = localtime(&theTime);

	/* z80ex sets the upper bits of the port address, we don't want that */
	port &= 0xFF;

	log_debug(" * IO    R [  %02X] -> %02X\n", port, ms->io[port]);

	switch (port) {
	  case KEYBOARD:// emulate keyboard matrix output
		// keyboard row is 10 bits wide, P1.x = low bits, P2.0-1 = high bits
		kbaddr = ms->io[KEYBOARD] + ((ms->io[MISC2] & 3) << 8);

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
	  /* TODO: Implement hooks here to set various power
	   * states for testing?
	   */
	  case MISC9:
		ret = (uint8_t)0xE0 | ((~ms->power_button & 1) << 4);
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
		ret = ms->io[port];
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
/* XXX: Clean up this LED code at some point, have "real" LED on SDL window */
void z80ex_pwrite (
	Z80EX_CONTEXT *cpu,
	Z80EX_WORD port,
	Z80EX_BYTE val,
	void *user_data)
{

	MSHW* ms = (MSHW*)user_data;
	static uint8_t tmp_reg;


	/* z80ex sets the upper bits of the port address, we don't want that */
	port &= 0xFF;

	log_debug(" * IO    W [  %02X] <- %02X\n", port, val);

	switch (port) {
	  case MISC2:
		if ((tmp_reg & (1 << 4)) != (val & (1 << 4))) {
			if (val & (1 << 4)) {
				tmp_reg |= (1 << 4);
				printf("LED on\n");
			} else {
				tmp_reg &= ~(1 << 4);
				printf("LED off\n");
			}
		}
		ms->io[port] = val;
		break;

	  // set interrupt masks
	  case IRQ_MASK:
		ms->interrupt_mask &= val;
		ms->io[port] = val;
		break;

	  /* For each slot, recalculate the slot_map from the now set device
	   * and page combination.
	   *
	   * NOTE!
	   * It's been observed that writes of SLOTX_DEV ports, the upper 4
	   * bits are sometimes set. The meaning of these bits is unknown and
	   * may just be "dontcare" to the decode logic; so the MS firmware
	   * does not worry about clearing them when writing the respective
	   * PORT. It is unknown if not writing the full 8bit value to the PORT
	   * is problematic. Therefore, when setting up the slot_map we AND
	   * the lower 4 bits.
	   */
	  case SLOT4_PAGE:
	  case SLOT4_DEV:
		ms->io[port] = val;
		ms->slot_map[1] = ms->dev_map[((ms->io[SLOT4_DEV]) & 0x0F)] +
		  (ms->io[SLOT4_PAGE] * 0x4000);
		break;

	  case SLOT8_PAGE:
	  case SLOT8_DEV:
		ms->io[port] = val;
		ms->slot_map[2] = ms->dev_map[((ms->io[SLOT8_DEV]) & 0x0F)] +
		  (ms->io[SLOT8_PAGE] * 0x4000);
		break;

	  // check for hardware power off bit in P28
	  case UNKNOWN0x28:
		if (val & 1) {
			printf("POWER OFF\n");
			powerOff(ms);
		}
		ms->io[port] = val;
		break;

	  // otherwise just save written value
	  default:
		//printf("* UNKNOWN IO -> %04X - %02X\n",port, val);
		ms->io[port] = val;
		break;
	}
}



/* Translate real input keys to MS keyboard matrix
 *
 * The lookup matrix is currently [10][8], we just walk the whole thing like
 * one long buffer and do the math later to figure out exactly what bit was
 * pressed.
 *
 * TODO: Would it make sense to rework keyTranslateTable to a single buffer
 * anyway? Right now the declaration looks crowded and would need some rework
 * already.
 */
void generateKeyboardMatrix(MSHW* ms, int scancode, int eventtype)
{
	uint32_t i = 0;
	int32_t *keytbl_ptr = &keyTranslateTable[0][0];

	for (i = 0; i < (sizeof(keyTranslateTable)/sizeof(int32_t)); i++) {
		if (scancode == *(keytbl_ptr + i)) {
			/* Couldn't avoid the magic numbers below. As noted,
			 * kTT array is [10][8], directly mapping the MS matrix
			 * of 10 bytes to represent the whole keyboard. Divide
			 * by 8 to get the uint8_t the scancode falls in, and mod
			 * 8 to get the bit in that uint8_t that matches the code.
			 */
			if (eventtype == SDL_KEYDOWN) {
				ms->key_matrix[i/8] &= ~((uint8_t)1 << (i%8));
			} else {
				ms->key_matrix[i/8] |= ((uint8_t)1 << (i%8));
			}
		}
	}
}

/* z80ex emulation requires that intread callback be defined.
 * As far as is known, there is no INT used on the Mailsattion. The only IRQs
 * are NMI for timers, which launch other handlers from there. If the error
 * below is ever encountered, that means that this actually is used somewhere.
 */
Z80EX_BYTE z80ex_intread (
	Z80EX_CONTEXT *cpu,
	void *user_data)
{
	log_error("z80ex_intread is not implemented.");
	abort();
}

// Processes interrupts. Should be called on every interrupt period.
// Returns number of tstates spent processing interrupt.
int process_interrupts (MSHW* ms)
{
	static int icount = 0;

	// Interrupt occurs at 64hz.  So this counter reduces to 1 sec intervals
	if (icount++ >= 64)
	{
		icount = 0;

		// do time16 interrupt
		if (ms->io[3] & 0x10 && !(ms->interrupt_mask & 0x10))
		{
			ms->interrupt_mask |= 0x10;
			return z80ex_nmi(ms->z80);
		}
	}

	// Trigger keyboard interrupt if necessary (64hz)
	if (ms->io[3] & 2 && !(ms->interrupt_mask & 2))
	{
		ms->interrupt_mask |= 2;
		return z80ex_nmi(ms->z80);
	}

	// Otherwise ignore this
	return 0;
}

//----------------------------------------------------------------------------
//
//  Resets Mailstation state
//
void resetMailstation(MSHW* ms)
{
	memset(ms->lcd_dat8bit, 0, 320*240);
	memset(ms->io, 0, 64 * 1024);
	// XXX: Mailstation normally retains RAM I believe.  But Mailstation OS
	// won't warm-boot properly if we don't erase!  Not sure why yet.
	memset((uint8_t *)ms->dev_map[RAM], 0, 128 * 1024);
	ms->power_state = MS_POWERSTATE_ON;
	ms->interrupt_mask = 0;
	z80ex_reset(ms->z80);
}

/* Debug support */
void sigint(int sig)
{
	debug_console = 1;
	printf("\nReceived SIGINT, interrupting\n");
}

/* Main
 *
 */
int main(int argc, char *argv[])
{
	char *codeflash_path = "codeflash.bin";
	char *dataflash_path = "dataflash.bin";
	char* logpath = NULL;
	int c;
	int opt_verbose = 0;
	int single_step = 0;

	MSHW ms;
	int execute_counter = 0;
	int tstate_counter = 0;
	/* XXX: interrupt_period can change if running at different freq */
	int interrupt_period = 187500;
	int exitemu = 0;
	uint32_t lasttick = SDL_GetTicks();
	uint32_t currenttick;
	SDL_Event event;

	struct sigaction sigact;

	static struct option long_opts[] = {
	  { "help", no_argument, NULL, 'h' },
	  { "codeflash", required_argument, NULL, 'c' },
	  { "dataflash", required_argument, NULL, 'd' },
	  { "logfile", optional_argument, NULL, 'l' },
	  { "verbose", no_argument, NULL, 'v' },
	/* TODO: Add argument to start with debug console open, e.g. execution
	 * halted.
	 */
	  { NULL, no_argument, NULL, 0}
	};

	/* TODO:
	 *   Rework main and break out in to smaller functions.
	 *   Set up buffers and parse files
	 *   Set up SDL calls
	 *   Then get in to loop
	 */

	/* Process arguments */
	while ((c = getopt_long(argc, argv,
	  "hc:d:l:v", long_opts, NULL)) != -1) {
		switch(c) {
		  case 'c':
			codeflash_path = malloc(strlen(optarg)+1);
			/* TODO: Implement error handling here */
			strncpy(codeflash_path, optarg, strlen(optarg)+1);
			break;
		  case 'd':
			dataflash_path = malloc(strlen(optarg)+1);
			/* TODO: Implement error handling here */
			strncpy(dataflash_path, optarg, strlen(optarg)+1);
			break;
		  case 'l':
			logpath = malloc(strlen(optarg) + 1);
			/* TODO: Implement error handling here */
			strncpy(logpath, optarg, strlen(optarg) + 1);
		  	break;
		  case 'v':
			opt_verbose = 1;
			break;
		  case 'h':
		  default:
			printf("Usage: %s [-c <path>] [-d <path>] [-l <path>]\n", argv[0]);
			printf(" -c <path>   | path to codeflash (default: %s)\n", codeflash_path);
			printf(" -d <path>   | path to dataflash (default: %s)\n", dataflash_path);
			printf(" -l <path>   | path to log file\n");
			printf(" -v          | verbose output to terminal\n");
			printf(" -h          | show this usage menu\n");
			return 1;
		}
	}

	// Initialize logging
	log_init(logpath, opt_verbose);

	/* Allocate and clear buffers.
	 * Codeflash is 1 MiB
	 * Dataflash is 512 KiB
	 * RAM is 128 KiB
	 * IO is 64 KiB (Not sure how much is necessary here)
	 * LCD has two buffers, 8bit and 1bit. The Z80 emulation writes to the
	 * 1bit buffer, this then translates to the 8bit buffer for SDLs use.
	 *
	 * TODO: Add error checking on the buffer allocation
	 */
	ms.dev_map[CF] = (uintptr_t)calloc(MEBIBYTE, sizeof(uint8_t));
	ms.dev_map[DF] = (uintptr_t)calloc(MEBIBYTE/2, sizeof(uint8_t));
	ms.dev_map[RAM] = (uintptr_t)calloc(MEBIBYTE/8, sizeof(uint8_t));
	ms.io = (uint8_t *)calloc(MEBIBYTE/16, sizeof(uint8_t));
	ms.lcd_dat1bit = (uint8_t *)calloc(((MS_LCD_WIDTH * MS_LCD_HEIGHT) / 8),
	  sizeof(uint8_t));
	/* XXX: MAGIC NUMBEERRRR */
	/* ms.dev_map[LCD_R] = ms.dev_map[LCD_L] + 4800; */
	ms.lcd_dat8bit = (uint8_t *)calloc(  MS_LCD_WIDTH * MS_LCD_HEIGHT,
	  sizeof(uint8_t));

	/* Initialize flags. */
	ms.lcd_lastupdate = 0;
	ms.lcd_cas = 0;
	ms.dataflash_updated = 0;
	ms.interrupt_mask = 0;
	ms.power_button = 0;
	ms.power_state = MS_POWERSTATE_OFF;
	ms.bp = -1;

	/* Initialize the slot_map */
	ms.slot_map[0] = ms.dev_map[CF]; /* slot0000 is always CF_0 */
	ms.slot_map[1] = ms.dev_map[((ms.io[SLOT4_DEV]) & 0x0F)] +
	  (ms.io[SLOT4_PAGE] * 0x4000);
	ms.slot_map[2] = ms.dev_map[((ms.io[SLOT8_DEV]) & 0x0F)] +
	  (ms.io[SLOT8_PAGE] * 0x4000);
	ms.slot_map[3] = ms.dev_map[RAM]; /* slotC000 is always RAM_0 */

	/* Set up keyboard emulation array */
	memset(ms.key_matrix, 0xff, sizeof(ms.key_matrix));

	/* Open codeflash and dump it in to a buffer.
	 * The codeflash should be exactly 1 MiB.
	 * Its possible to have a short dump, where the remaining bytes are
	 * assumed to be zero.
	 * It should never be longer either. If it is, we just pretend like
	 * we didn't notice. This might be unwise behavior.
	 */
	flashtobuf((uint8_t *)ms.dev_map[CF], codeflash_path, MEBIBYTE);

	/* Open dataflash and dump it in to a buffer.
	 * The codeflash should be exactly 512 KiB.
	 * Its possible to have a short dump, where the remaining bytes are
	 * assumed to be zero. But it in practice shouldn't happen.
	 * It should never be longer either. If it is, we just pretend like
	 * we didn't notice. This might be unwise behavior.
	 * If ./dataflash.bin does not exist, and --dataflash <path> is not
	 * passed, then create a new dataflash in RAM which will get written
	 * to ./dataflash.bin
	 */
	flashtobuf((uint8_t *)ms.dev_map[DF], dataflash_path, MEBIBYTE/2);
	/* XXX: Check return of this func, create new dataflash image! */
#if 0
	if (!ret && !opt_dataflash) {
		generate new dataflash image
		XXX: Generate random serial number?
		printf("Generating new dataflash image. Saving to file: %s\n",
		  dataflash_path);
#endif


	/* TODO: Add git tags to this, because thats neat */
	printf("\nMailstation Emulator v0.1\n");
	printf("\nPress ctrl+c to enter interactive Mailstation debugger\n");

	ui_init(&raw_cga_array[0], ms.lcd_dat8bit);

	ms.z80 = z80ex_create(
		z80ex_mread, (void*)&ms,
		z80ex_mwrite, (void*)&ms,
		z80ex_pread, (void*)&ms,
		z80ex_pwrite, (void*)&ms,
		z80ex_intread, (void*)&ms
	);

	/* NOTE:
	 * The z80ex library can hook in to RETI opcodes. Allowing us to exec
	 * code here to emulate PIO or other. At this time, I don't think
	 * there is any reason to need this.
	 */

	/* Override ctrl+c to drop to debug console */
	sigact.sa_handler = sigint;
	sigaction(SIGINT, &sigact, NULL);

	// Display startup message
	powerOff(&ms);

	lasttick = SDL_GetTicks();

	while (!exitemu)
	{
		if (debug_console) {
			switch (debug_prompt(&ms)) {
			  case -1: /* Quit */
				exitemu = 1;
			  case 0: /* Continue */
				debug_console = 0;
				single_step = 0;
				break;
			  case 1: /* Single step */
				debug_console = 1;
				single_step = 1;
				break;
			}
		}

		currenttick = SDL_GetTicks();

		/* Let the Z80 process code at regular intervals */
		if (ms.power_state == MS_POWERSTATE_ON) {
			/* XXX: Can replace with SDL_TICKS_PASSED with new
			 * SDL version.
			 */
			/* NOTE: The point of this is to effectively match
			 * execution speed with realtime. Normally, the MS
			 * sees an interrupt every 64 Hz, this handles keyboard
			 * reading as well as incrementing timers.
			 * With a 64 Hz interrupt, this means an interrupt
			 * occurs every 15 ms. The z80em tool just goes as
			 * fast as it can, there is no (known) way to spec how
			 * long in real time a single T-State should take.
			 * The workaround for this, is to delay execution for
			 * 15 SDL ticks, or 15 ms, every interrupt period to
			 * get roughly time accurate execution.
			 */
			execute_counter += currenttick - lasttick;

			/* This loop is our real-time gating. The only reason
			 * this loop should exit is:
			 * We're single stepping
			 * To process an NMI (every 64 Hz)
			 *
			 * Once we process an interrupt, stall execution
			 * until the interrupt would have happened realtime.
			 * This is based on CPU execution speed, default is
			 * 12 MHz but can be changed on the fly via a port wr.
			 *
			 * Due to how z80ex processes steps, after a call to
			 * z80ex_step() it might not be possible to NMI, if
			 * thats the case, keep stepping until an NMI is
			 * possible.
			 *
			 * This step process also means we need to ensure
			 * we only decode full opcodes for dasm, the
			 * z80ex_last_op_type() returns a 0 when the last op
			 * was complete.
			 *
			 * In order to have correct and full simulation, single
			 * step actions happen here, that way the SDL window
			 * can be updated, keys can be parsed, etc.
			 */
			if (single_step) {
				log_push(1);
				do {
					if (!z80ex_last_op_type(ms.z80)) {
						debug_dasm(&ms);
					}
					tstate_counter += z80ex_step(ms.z80);
				} while (z80ex_last_op_type(ms.z80));
				log_pop();

			} else if (execute_counter > 15) {
				execute_counter = 0;
				while (tstate_counter < interrupt_period ||
				  !z80ex_nmi_possible(ms.z80)) {
					if (!log_isverbose() && !z80ex_last_op_type(ms.z80)){
						debug_dasm(&ms);
					}
					if (z80ex_get_reg(ms.z80, regPC) == ms.bp) {
						debug_console = 1;
						break;
					}
					tstate_counter += z80ex_step(ms.z80);
				}
			}

			if (tstate_counter >= interrupt_period) {
				tstate_counter += process_interrupts(&ms);
				tstate_counter %= interrupt_period;
			}

		}

		/* Update LCD if modified (at 20ms rate) */
		/* TODO: Check over this whole process logic */
		/* NOTE: Cursory glance suggests the screen updates 20ms after
		 * the screen array changed.
		 */
		if (ms.power_state == MS_POWERSTATE_ON && (ms.lcd_lastupdate != 0) &&
		  (currenttick - ms.lcd_lastupdate >= 20)) {
			ui_drawLCD();
			ms.lcd_lastupdate = 0;
		}

		/* Write dataflash buffer to disk if it was modified */
		if (ms.dataflash_updated) {
			int ret = buftoflash((uint8_t *)ms.dev_map[DF], dataflash_path, MEBIBYTE/2);
			if (ret != 0) {
				log_error(
					"Failed writing dataflash to disk (%s), err 0x%08X\n",
					dataflash_path, ret);
			} else {
				ms.dataflash_updated = 0;
			}
		}

		/* XXX: All of this needs to be reworked to be far more
		 * efficient.
		 */
		// Check SDL events
		while (SDL_PollEvent(&event))
		{
			/* Exit if SDL quits, or Escape key was pushed */
			if ((event.type == SDL_QUIT) ||
			  ((event.type == SDL_KEYDOWN) &&
				(event.key.keysym.sym == SDLK_ESCAPE))) {
				exitemu = 1;
			}

			/* Emulate power button with F12 key */
			if (event.key.keysym.sym == SDLK_F12)
			{
				if (event.type == SDL_KEYDOWN)
				{
					ms.power_button = 1;
					if (ms.power_state == MS_POWERSTATE_OFF)
					{
						printf("POWER ON\n");

						resetMailstation(&ms);
					}
				} else {
					ms.power_button = 0;
				}
			}

			/* Handle other input events */
			if ((event.type == SDL_KEYDOWN) ||
			  (event.type == SDL_KEYUP)) {
				/* Keys pressed whie right ctrl is held */
				if (event.key.keysym.mod & KMOD_RCTRL)
				{
					if (event.type == SDL_KEYDOWN)
					{
						switch (event.key.keysym.sym) {
						  /* Reset whole system */
						  case SDLK_r:
							if (ms.power_state == MS_POWERSTATE_ON)
							  resetMailstation(&ms);
							break;
						  default:
							break;
						}
					}
				} else {
					/* Proces the key for the MS */
					generateKeyboardMatrix(&ms, event.key.keysym.sym, event.type);
				}
			}
		}
		// Update SDL ticks
		lasttick = currenttick;
	}


	log_shutdown();

	return 0;
}
