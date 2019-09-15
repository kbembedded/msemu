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
#include "rawcga.h"
#include "logger.h"
#include "msemu.h"
#include "flashops.h"
#include "ui.h"

#include <SDL/SDL.h>
#include <SDL/SDL_rotozoom.h>
#include <z80ex/z80ex.h>
#include <z80ex/z80ex_dasm.h>

struct mshw ms;

// Default entry of color palette to draw Mailstation LCD with
uint8_t LCD_fg_color = 3;  // LCD black
uint8_t LCD_bg_color = 2;  // LCD green

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


// Some function declarations for later
void powerOff();




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
//  Emulates writing to Mailstation LCD device
//
void writeLCD(uint16_t newaddr, uint8_t val, int lcdnum)
{
	uint8_t *lcd_ptr;
	if (lcdnum == MS_LCD_LEFT) lcd_ptr = ms.lcd_dat1bit;
	/* XXX: Fix the use of this magic number, replace with a const */
	else lcd_ptr = &ms.lcd_dat1bit[4800];
	if (!lcd_ptr) return;

	// Wraps memory address if out of bounds
	while (newaddr >= 240) newaddr -= 240;

	// Check CAS bit on P2
	if (ms.io[2] & 8)
	{
		// Write data to currently selected LCD column.
		// This is just used for reading back LCD contents to the Mailstation quickly.
		lcd_ptr[newaddr + (ms.lcd_cas * 240)] = val;


		/*
			Write directly to newer 8-bit lcd_data8 buffer now too.
			This is what will actually be drawn on the emulator screen now.
		*/

		// Reverse column # (MS col #0 starts on right side)
		int x = 19 - ms.lcd_cas;
		// Use right half if necessary
		if (lcdnum == MS_LCD_RIGHT) x += 20;

		// Write out all 8 bits to separate bytes, using the current emulated LCD color
		int n;
		for (n = 0; n < 8; n++)
		{
			ms.lcd_dat8bit[n + (x * 8) + (newaddr * 320)] = ((val >> n) & 1 ? LCD_fg_color : LCD_bg_color);
		}

		// Let main loop know to update screen with new LCD data
		ms.lcd_lastupdate = SDL_GetTicks();
	}

	// If CAS line is low, set current column instead
	else	ms.lcd_cas = val;

}


//----------------------------------------------------------------------------
//
//  Emulates reading from Mailstation LCD
//
uint8_t readLCD(uint16_t newaddr, int lcdnum)
{
	uint8_t *lcd_ptr;
	if (lcdnum == MS_LCD_LEFT) lcd_ptr = ms.lcd_dat1bit;
	/* XXX: Fix the use of this magic number, replace with a const */
	else lcd_ptr = &ms.lcd_dat1bit[4800];
	if (!lcd_ptr) return 0;

	// Wraps memory address if out of bounds
	while (newaddr >= 240) newaddr -= 240;

	// Check CAS bit on P2
	if (ms.io[2] & 8)
	{
		// Return data on currently selected LCD column
		return lcd_ptr[newaddr + (ms.lcd_cas * 240)];
	}

	// Not sure what this normally returns when CAS bit low!
	else return ms.lcd_cas;

}


/* Read a uint8_t from the RAM buffer
 *
 * TODO: Add a debug hook here
 */
uint8_t readRAM(unsigned int translated_addr)
{
	uint8_t val = ms.ram[translated_addr];
	log_debug(" * RAM READ  [%04X] -> %02X\n", translated_addr, val);
	return val;
}

/* Write a uint8_t to the RAM buffer
 *
 * TODO: Add a debug hook here
 */
void writeRAM(unsigned int translated_addr, uint8_t val)
{
	log_debug(" * RAM WRITE [%04X] <- %02X\n", translated_addr, val);
	ms.ram[translated_addr] = val;
}



/* z80ex Read memory callback function.
 *
 * Return a uint8_t from the address given to us.
 * This function needs to figure out what slot the requested address lies in,
 * figure out what page of which device is in that slot, and return data.
 *
 * We also need to translate the address requested to the correct offset of the
 * slotted page of the requested device.
 */
/* XXX: Can the current page/device logic be cleaned up to flow better? */
// TODO: Actually use cpu/user_data instead of globals.
Z80EX_BYTE z80ex_mread(
	Z80EX_CONTEXT *cpu,
	Z80EX_WORD addr,
	int m1_state,
	void *user_data)
{
	uint16_t newaddr = 0;
	uint8_t current_page = 0;
	uint8_t current_device = 0;


	// Slot 0x0000 - always codeflash page 0
	if (addr < 0x4000) return readCodeFlash(addr);

	// Slot 0xC000 - always RAM page 0
	if (addr >= 0xC000) return readRAM(addr-0xC000);

	// Slot 0x4000
	if (addr >= 0x4000 && addr < 0x8000)
	{
		newaddr = addr - 0x4000;
		current_page = ms.slot4000_page;
		current_device = ms.slot4000_device;
	}

	// Slot 0x8000
	if (addr >= 0x8000 && addr < 0xC000)
	{
		newaddr = addr - 0x8000;
		current_page = ms.slot8000_page;
		current_device = ms.slot8000_device;
	}

	unsigned int translated_addr = newaddr + (current_page * 0x4000);

	/* NOTE: It appears that some times the mailstation will emit a
	 * current device > 0xF. It appears the upper 4 bits are dontcares
	 * to the slot handling logic.
	 */
	switch (current_device & 0x0F)	{
	  case 0: /* Codeflash */
		if (current_page >= 64) {
			log_error("[%04X] * INVALID CODEFLASH PAGE: %d\n",
			  z80ex_get_reg(ms.z80, regPC),current_page);
		}
		return readCodeFlash(translated_addr);

	  case 1: /* Dataflash */
		if (current_page >= 8) {
			log_error("[%04X] * INVALID RAM PAGE: %d\n",
			  z80ex_get_reg(ms.z80, regPC), current_page);
		}
		return readRAM(translated_addr);

	  case 2: /* LCD, left side */
		return readLCD(newaddr, MS_LCD_LEFT);
		case 3: /* Dataflash */
		if (current_page >= 32) {
			log_error("[%04X] * INVALID DATAFLASH PAGE: %d\n",
			  z80ex_get_reg(ms.z80, regPC), current_page);
		}
		return readDataflash(translated_addr);

	  case 4: /* LCD, right side */
		return readLCD(newaddr, MS_LCD_RIGHT);

	  case 5: /* MODEM */
		log_debug("[%04X] * READ FROM MODEM UNSUPPORTED: %04X\n",
		  z80ex_get_reg(ms.z80, regPC), newaddr);
		break;

	  default:
		log_error("[%04X] * READ FROM UNKNOWN DEVICE: %d\n",
		  z80ex_get_reg(ms.z80, regPC), current_device);
		break;
	}

	return 0;
}


/* z80ex Write memory callback function.
 *
 * Write a uint8_t to the address given to us.
 * This function needs to figure out what slot the requested address lies in,
 * figure out what page of which device is in that slot, and return data.
 *
 * We also need to translate the address requested to the correct offset of the
 * slotted page of the requested device.
 */
/* XXX: Can the current page/device logic be cleaned up to flow better? */
// TODO: Actually use cpu/user_data instead of globals.
void z80ex_mwrite(
	Z80EX_CONTEXT *cpu,
	Z80EX_WORD addr,
	Z80EX_BYTE val,
	void *user_data)
{
	uint16_t newaddr = 0;
	uint8_t current_page = 0;
	uint8_t current_device = 0;
	uint32_t translated_addr;


	// Slot 0x0000 - always codeflash page 0
	if (addr < 0x4000)
	{
		log_error("[%04X] * CAN'T WRITE TO CODEFLASH SLOT 0: 0x%X\n",
		  z80ex_get_reg(ms.z80, regPC), addr);
		return;
	}

	// Slot 0xC000 - always RAM page 0
	if (addr >= 0xC000)
	{
		writeRAM((addr-0xC000), val);
		return;
	}

	// Slot 0x4000
	if (addr >= 0x4000 && addr < 0x8000)
	{
		newaddr = addr - 0x4000;
		current_page = ms.slot4000_page;
		current_device = ms.slot4000_device;
	}

	// Slot 0x8000
	if (addr >= 0x8000 && addr < 0xC000)
	{
		newaddr = addr - 0x8000;
		current_page = ms.slot8000_page;
		current_device = ms.slot8000_device;
	}

	translated_addr = newaddr + (current_page * 0x4000);


	/* NOTE: While the read path seems to have the situation where the
	 * upper 4 bits of "current device" are set, I've never seen that be
	 * a problem here. Regardless, still mask those off.
	 */
	switch(current_device & 0x0F) {
	  case 0: /* Codeflash */
		log_error("[%04X] * WRITE TO CODEFLASH UNSUPPORTED\n",
		  z80ex_get_reg(ms.z80, regPC));
		break;

	  case 1: /* RAM */
		if (current_page >= 8) {
			log_error("[%04X] * INVALID RAM PAGE: %d\n",
			  z80ex_get_reg(ms.z80, regPC), current_page);
		}
		writeRAM(translated_addr, val);
		break;

	  case 2: /* LCD, left side */
		writeLCD(newaddr, val, MS_LCD_LEFT);
		break;

	  case 3: /* Dataflash */
		if (current_page >= 32) {
			log_error("[%04X] * INVALID DATAFLASH PAGE: %d\n",
			  z80ex_get_reg(ms.z80, regPC), current_page);
		}
		ms.dataflash_updated = writeDataflash(translated_addr, val);
		break;

	  case 4: /* LCD, right side */
		writeLCD(newaddr, val, MS_LCD_RIGHT);
		break;

	  case 5: /* MODEM */
		log_debug("[%04X] WRITE TO MODEM UNSUPPORTED: %04X %02X\n",
		  z80ex_get_reg(ms.z80, regPC), newaddr, val);
		break;

	  default:
		log_error("[%04X] * WRITE TO UNKNOWN DEVICE: %d\n",
		  z80ex_get_reg(ms.z80, regPC), current_device);
		break;
	}
}



//----------------------------------------------------------------------------
//
//  z80ex I/O port input handler
//
// TODO: Actually use cpu/user_data instead of globals.
Z80EX_BYTE z80ex_pread (
	Z80EX_CONTEXT *cpu,
	Z80EX_WORD port,
	void *user_data)
{
	uint16_t addr = port;

	// This is for the RTC on P10-1C
	time_t theTime;
	time( &theTime );
	struct tm *rtc_time = localtime(&theTime);

	log_debug("[%04X] * IO  [%04X] -> %02X\n", z80ex_get_reg(ms.z80, regPC), addr,ms.io[addr]);

	//if (addr < 5 || addr > 8) printf("* IO <- %04X - %02X\n",addr,ms.io[addr]);

	switch (addr)
	{

		// emulate keyboard matrix output
		case 0x01:
			{
				// keyboard row is 10 bits wide, P1.x = low bits, P2.0-1 = high bits
				unsigned short kbaddr = ms.io[1] + ((ms.io[2] & 3) << 8);

				// all returned bits should be high unless a key is pressed
				uint8_t kbresult = 0xFF;

				// check for a key pressed in active row(s)
				int n;
				for (n = 0; n < 10; n++)
				{
					if (!((kbaddr >> n) & 1)) kbresult &= ms.key_matrix[n];
				}

				return kbresult;
			}
			break;


		// NOTE: lots of activity on these two during normal loop
		case 0x21:
		case 0x1D:

		case 0x02:
			return ms.io[addr];



		// return currently triggered interrupts
		case 0x03:
			/*	p3.7 = Caller id handler
				p3.5 = maybe rtc???
				p3.6 = Modem handler
				p3.4 = increment time16
				p3.3 = null
				p3.0 = null
				p3.1 = Keyboard handler
				p3.2 = null
			*/
			return ms.interrupt_mask;
			break;

		// page/device ports
		case 0x05:
		case 0x06:
		case 0x07:
		case 0x08:
			return ms.io[addr];


		// acknowledge power good + power button status
		case 0x09:
			return (uint8_t)0xE0 | ((ms.power_button & 1) << 4);


		// These are all for the RTC
		case 0x10: //seconds
			return (hex2bcd(rtc_time->tm_sec) & 0x0F);
		case 0x11: //10 seconds
			return ((hex2bcd(rtc_time->tm_sec) & 0xF0) >> 4);
		case 0x12: // minutes
			return (hex2bcd(rtc_time->tm_min) & 0x0F);
		case 0x13: // 10 minutes
			return ((hex2bcd(rtc_time->tm_min) & 0xF0) >> 4);
		case 0x14: // hours
			return (hex2bcd(rtc_time->tm_hour) & 0x0F);
		case 0x15: // 10 hours
			return ((hex2bcd(rtc_time->tm_hour) & 0xF0) >> 4);
		case 0x16: // day of week
			return rtc_time->tm_wday;
		case 0x17: // days
			return (hex2bcd(rtc_time->tm_mday) & 0x0F);
		case 0x18: // 10 days
			return ((hex2bcd(rtc_time->tm_mday) & 0xF0) >> 4);
		case 0x19: // months
			return (hex2bcd(rtc_time->tm_mon + 1) & 0x0F);
		case 0x1A: // 10 months
			return ((hex2bcd(rtc_time->tm_mon + 1) & 0xF0) >> 4);
		case 0x1B: // years
			return (hex2bcd(rtc_time->tm_year + 80) & 0x0F);
		case 0x1C: // 10 years
			return ((hex2bcd(rtc_time->tm_year + 80) & 0xF0) >> 4);
			break;

		default:
			//printf("* UNKNOWN IO <- %04X - %02X\n",addr, ms.io[addr]);
			return ms.io[addr];
	}

}



//----------------------------------------------------------------------------
//
//  z80ex I/O port output handler
//
/* XXX: Clean up this LED code at some point, have "real" LED on SDL window */
// TODO: Actually use cpu/user_data instead of globals.
void z80ex_pwrite (
	Z80EX_CONTEXT *cpu,
	Z80EX_WORD port,
	Z80EX_BYTE val,
	void *user_data)
{
	uint16_t addr = port;
	static uint8_t tmp_reg;

	log_debug("[%04X] * IO  [%04X] <- %02X\n",z80ex_get_reg(ms.z80, regPC), addr, val);

	//if (addr < 5 || addr > 8) printf("* IO -> %04X - %02X\n",addr, val);

	switch (addr)
	{
		case 0x02:
			if ((tmp_reg & (1 << 4)) != (val & (1 << 4))) {
				if (val & (1 << 4)) {
					tmp_reg |= (1 << 4);
					printf("LED on\n");
				} else {
					tmp_reg &= ~(1 << 4);
					printf("LED off\n");
				}
			}
		case 0x01:

		// Note: Lots of activity on these next ones during normal loop
		case 0x2C:
		case 0x2D:
		case 0x1D:
			ms.io[addr] = val;
			break;


		// set interrupt masks
		case 0x03:
			ms.interrupt_mask &= val;
			ms.io[addr] = val;
			break;

		// set slot4000 page
		case 0x05:
			ms.slot4000_page = val;
			ms.io[addr] = ms.slot4000_page;
			break;

		// set slot4000 device
		case 0x06:
			ms.slot4000_device = val;
			ms.io[addr] = ms.slot4000_device;
			break;

		// set slot8000 page
		case 0x07:
			ms.slot8000_page = val;
			ms.io[addr] = ms.slot8000_page;
			break;

		// set slot8000 device
		case 0x08:
			ms.slot8000_device = val;
			ms.io[addr] = ms.slot8000_device;
			break;

		// check for hardware power off bit in P28
		case 0x028:
			if (val & 1)
			{
				printf("POWER OFF\n");
				powerOff();
			}
			ms.io[addr] = val;
			break;

		// otherwise just save written value
		default:
			//printf("* UNKNOWN IO -> %04X - %02X\n",addr, val);
			ms.io[addr] = val;
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
void generateKeyboardMatrix(int scancode, int eventtype)
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
				ms.key_matrix[i/8] &= ~((uint8_t)1 << (i%8));
			} else {
				ms.key_matrix[i/8] |= ((uint8_t)1 << (i%8));
			}
		}
	}
}

// Current IRQ status. Checked after EI occurs.  We won't need it (for now).
// TODO: Z80em - is there a z80ex equivalent?
// int Z80_IRQ;

// Run after a RETI
// TODO: Actually use cpu/user_data instead of globals.
void z80ex_reti (
	Z80EX_CONTEXT *cpu,
	void *user_data)
{
	return;
}

// Run after a RETN
// TODO: Z80em - is there a z80ex equivalent?
// void Z80_Retn (void)
// {
// 	return;
// }

// Can emulate stuff which we don't need
// TODO: Z80em - is there a z80ex equivalent?
// void Z80_Patch (Z80_Regs *Regs)
// {
// 	return;
// }

// Handler fired when Z80_ICount hits 0
// TODO: Actually use cpu/user_data instead of globals.
Z80EX_BYTE z80ex_intread (
	Z80EX_CONTEXT *cpu,
	void *user_data)
{
	static int icount = 0;

	// Interrupt occurs at 64hz.  So this counter reduces to 1 sec intervals
	if (icount++ >= 64)
	{
		icount = 0;

		// do time16 interrupt
		if (ms.io[3] & 0x10 && !(ms.interrupt_mask & 0x10))
		{
			ms.interrupt_mask |= 0x10;
			return -2; //Z80_NMI_INT;
		}
	}

	// Trigger keyboard interrupt if necessary (64hz)
	if (ms.io[3] & 2 && !(ms.interrupt_mask & 2))
	{
		ms.interrupt_mask |= 2;
		return -2; //Z80_NMI_INT;
	}

	// Otherwise ignore this
	return -1; //Z80_IGNORE_INT;
}

// Handler for returning the byte for a given address.
// Used by the disassembler.
Z80EX_BYTE z80ex_dasm_readbyte (
	Z80EX_WORD addr,
	void *user_data
) {
	return ms.codeflash[addr];
}

//----------------------------------------------------------------------------
//
//  Resets Mailstation state
//
void resetMailstation()
{
	memset(ms.lcd_dat8bit, 0, 320*240);
	memset(ms.io,0,64 * 1024);
	// XXX: Mailstation normally retains RAM I believe.  But Mailstation OS
	// won't warm-boot properly if we don't erase!  Not sure why yet.
	memset(ms.ram,0,128 * 1024);
	ms.power_state = MS_POWERSTATE_ON;
	ms.interrupt_mask = 0;
	z80ex_reset(ms.z80);

	// Mailstation seems to expect registers to be initialized to zero,
	// so we set them all to zero here to be safe.
	z80ex_set_reg(ms.z80, regAF, 0);
	z80ex_set_reg(ms.z80, regAF_, 0);
	z80ex_set_reg(ms.z80, regBC, 0);
	z80ex_set_reg(ms.z80, regBC_, 0);
	z80ex_set_reg(ms.z80, regDE, 0);
	z80ex_set_reg(ms.z80, regDE_, 0);
	z80ex_set_reg(ms.z80, regHL, 0);
	z80ex_set_reg(ms.z80, regHL_, 0);
	z80ex_set_reg(ms.z80, regIX, 0);
	z80ex_set_reg(ms.z80, regIY, 0);
	z80ex_set_reg(ms.z80, regSP, 0);
	z80ex_set_reg(ms.z80, regPC, 0);
}


//----------------------------------------------------------------------------
//
//  Disables emulation, displays opening screen
//
void powerOff()
{
	ms.power_state = MS_POWERSTATE_OFF;

	// clear LCD
	memset(ms.lcd_dat8bit, 0, MS_LCD_WIDTH * MS_LCD_HEIGHT);
	ui_drawSplashScreen();
}

/* Main
 *
 * TODO:
 *   Support newer SDL versions
 */
int main(int argc, char *argv[])
{
	char *codeflash_path = "codeflash.bin";
	char *dataflash_path = "dataflash.bin";
	char* logpath = NULL;
	int c;
	int silent = 1;
	int execute_counter = 0;
	int exitemu = 0;
	uint32_t lasttick = SDL_GetTicks();
	uint32_t currenttick;
	SDL_Event event;

	// These are for the disassembler
	int dasm_buffer_len = 256;
	char dasm_buffer[dasm_buffer_len];
	int dasm_tstates = 0;
	int dasm_tstates2 = 0;

	static struct option long_opts[] = {
	  { "help", no_argument, NULL, 'h' },
	  { "codeflash", required_argument, NULL, 'c' },
	  { "dataflash", required_argument, NULL, 'd' },
	  { "logfile", optional_argument, NULL, 'l' },
	  { "verbose", no_argument, NULL, 'v' },
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
			silent = 0;
			break;
		  case 'h':
		  default:
			printf("Usage: %s [-s] [-c <path>] [-d <path>] [-l <path>]\n", argv[0]);
			printf(" -c <path>   | path to codeflash (default: %s)\n", codeflash_path);
			printf(" -d <path>   | path to dataflash (default: %s)\n", dataflash_path);
			printf(" -l <path>   | path to log file\n");
			printf(" -v          | verbose logging\n");
			printf(" -h          | show this usage menu\n");
			return 1;
		}
	}

	// Initialize logging
	log_init(logpath, silent);

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
	ms.codeflash = (uint8_t *)calloc(MEBIBYTE, sizeof(uint8_t));
	ms.dataflash = (uint8_t *)calloc(MEBIBYTE/2, sizeof(uint8_t));
	ms.ram = (uint8_t *)calloc(MEBIBYTE/8, sizeof(uint8_t));
	ms.io = (uint8_t *)calloc(MEBIBYTE/16, sizeof(uint8_t));
	ms.lcd_dat1bit = (uint8_t *)calloc(((MS_LCD_WIDTH * MS_LCD_HEIGHT) / 8),
	  sizeof(uint8_t));
	ms.lcd_dat8bit = (uint8_t *)calloc(  MS_LCD_WIDTH * MS_LCD_HEIGHT,
	  sizeof(uint8_t));

	/* Initialize flags. */
	ms.lcd_lastupdate = 0;
	ms.lcd_cas = 0;
	ms.dataflash_updated = 0;
	ms.interrupt_mask = 0;
	ms.slot4000_page = 0;
	ms.slot4000_device = 0;
	ms.slot8000_page = 0;
	ms.slot8000_device = 0;
	ms.power_button = 0;
	ms.power_state = MS_POWERSTATE_OFF;

	/* Set up keyboard emulation array */
	memset(ms.key_matrix, 0xff, sizeof(ms.key_matrix));

	/* Open codeflash and dump it in to a buffer.
	 * The codeflash should be exactly 1 MiB.
	 * Its possible to have a short dump, where the remaining bytes are
	 * assumed to be zero.
	 * It should never be longer either. If it is, we just pretend like
	 * we didn't notice. This might be unwise behavior.
	 */
	flashtobuf(ms.codeflash, codeflash_path, MEBIBYTE);

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
	flashtobuf(ms.dataflash, dataflash_path, MEBIBYTE/2);
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

	ui_init(&raw_cga_array[0], ms.lcd_dat8bit);

	ms.z80 = z80ex_create(
		z80ex_mread, (void*)&ms,
		z80ex_mwrite, (void*)&ms,
		z80ex_pread, (void*)&ms,
		z80ex_pwrite, (void*)&ms,
		z80ex_intread, (void*)&ms
	);
	z80ex_set_reti_callback(ms.z80, z80ex_reti, (void*)&ms);

	// TODO: These are old Z80em bindings,
	//   they seem kind of important...
	// Z80_Running = 1;		/* When 0, emulation terminates */
	// Z80_ICount = 0;			/* T-state count */
	// Z80_IRQ = Z80_IGNORE_INT;	/* Current IRQ status. */
	// /* MS runs at 12 MHz, divide by 64 for KB IRQ rate */
	// Z80_IPeriod = 187500;		/* Number of T-states per interrupt */

	// Display startup message
	powerOff();

	lasttick = SDL_GetTicks();

	while (!exitemu)
	{
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
			if (execute_counter > 15) {
				execute_counter = 0;

				memset(&dasm_buffer, 0, dasm_buffer_len);

				printf("[%04X] - ", z80ex_get_reg(ms.z80, regPC));
				z80ex_dasm(
					&dasm_buffer[0], dasm_buffer_len,
					0,
					&dasm_tstates, &dasm_tstates2,
					z80ex_dasm_readbyte,
					z80ex_get_reg(ms.z80, regPC),
					0);
				printf("%-15s  t=%d", dasm_buffer, dasm_tstates);
				if(dasm_tstates2) {
					printf("/%d", dasm_tstates2);
				}
				printf("\n");

				z80ex_step(ms.z80);
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
			int ret = buftoflash(ms.dataflash, dataflash_path, MEBIBYTE/2);
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
			/* XXX: Figure out why this requires a double press? */
			if (event.key.keysym.sym == SDLK_F12)
			{
				if (event.type == SDL_KEYDOWN)
				{
					ms.power_button = 1;
					if (ms.power_state == MS_POWERSTATE_OFF)
					{
						printf("POWER ON\n");

						resetMailstation();
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
							  resetMailstation();
							break;
						  default:
							break;
						}
					}
				} else {
					/* Proces the key for the MS */
					generateKeyboardMatrix(
					  event.key.keysym.sym, event.type);
				}
			}
		}
		// Update SDL ticks
		lasttick = currenttick;
	}


	log_shutdown();

	return 0;
}
