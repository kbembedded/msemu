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
#include "z80em/Z80.h"
#include "z80em/Z80IO.h"
#include <SDL/SDL.h>
#include <SDL/SDL_rotozoom.h>

// This is the embedded font we need to print graphical text.
char *rawcga_start = &raw_cga_array[0];

// Default screen size
const int SCREENWIDTH = 320;
const int SCREENHEIGHT = 240;

const int LCD_LEFT = 1;
const int LCD_RIGHT = 2;

struct mshw ms;

// Stores current Mailstation LCD column
uint8_t lcd_cas = 0;

// Last SDL tick at which LCD was updated.  Used for timing LCD refreshes to screen.
uint32_t lcd_lastupdate = 0;

// Primary SDL screen surface
SDL_Surface *screen;

// Master palette
SDL_Color colors[6];

// Default entry of color palette to draw Mailstation LCD with
uint8_t LCD_fg_color = 3;  // LCD black
uint8_t LCD_bg_color = 2;  // LCD green

// Surface for the Mailstation LCD, will be 320x240
SDL_Surface *lcd_surface;

// Surface to load CGA font data, for printing text with SDL
SDL_Surface *cgafont_surface = NULL;

// Cursor position for unfinished code to print text in SDL
int cursorX = 0;
int cursorY = 0;



// Bits specify which interrupts have been triggered (returned on P3)
uint8_t interrupts_active = 0;

// This is set if the dataflash contents are changed, so that we can write the contents to file.
int8_t dataflash_updated = 0;

// This is set if hardware power off is detected (via P28), to halt emulation
int poweroff = 0;

// Stores the page/device numbers of the two middle 16KB slots of address space
uint8_t slot4000_page = 0;
uint8_t slot4000_device = 0;
uint8_t slot8000_page = 0;
uint8_t slot8000_device = 0;

// Holds power button status (returned in P9.4)
uint8_t power_button = 0;

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
void generateLCD();
void printstring(char*);
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
//  Draws the LCD to the screen
//
void drawLCD()
{
	SDL_Surface *lcd_surface2x = NULL;

	// Setup output rect to fill screen for now
	SDL_Rect outrect;
	outrect.x = 0;
	outrect.y = 0;
	outrect.w = SCREENWIDTH;
	outrect.h = SCREENWIDTH;

	// Double surface size to 640x480
	lcd_surface2x = zoomSurface(lcd_surface, (double)2.0, (double)2.0, 0);
	// If we don't clear the color key, it constantly overlays just the primary color during blit!
	SDL_SetColorKey(lcd_surface2x, 0, 0);

	// Draw to screen
	if (SDL_BlitSurface(lcd_surface2x, NULL, screen, &outrect) != 0) {
		printf("Error blitting LCD to screen: %s\n",SDL_GetError());
	}

	//SDL_UpdateRect(SDL_GetVideoSurface(), 0,0, SCREENWIDTH, SCREENHEIGHT);
	SDL_Flip(screen);

	// Dump 2x surface
	SDL_FreeSurface(lcd_surface2x);

	// Screen has been updated, don't need to do it again until changes are made
	lcd_lastupdate = 0;
}



//----------------------------------------------------------------------------
//
//  Emulates writing to Mailstation LCD device
//
void writeLCD(uint16_t newaddr, uint8_t val, int lcdnum)
{
	uint8_t *lcd_ptr;
	if (lcdnum == LCD_LEFT) lcd_ptr = ms.lcd_dat1bit;
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
		lcd_ptr[newaddr + (lcd_cas * 240)] = val;


		/*
			Write directly to newer 8-bit lcd_data8 buffer now too.
			This is what will actually be drawn on the emulator screen now.
		*/

		// Reverse column # (MS col #0 starts on right side)
		int x = 19 - lcd_cas;
		// Use right half if necessary
		if (lcdnum == LCD_RIGHT) x += 20;

		// Write out all 8 bits to separate bytes, using the current emulated LCD color
		int n;
		for (n = 0; n < 8; n++)
		{
			ms.lcd_dat8bit[n + (x * 8) + (newaddr * 320)] = ((val >> n) & 1 ? LCD_fg_color : LCD_bg_color);
		}

		// Let main loop know to update screen with new LCD data
		lcd_lastupdate = SDL_GetTicks();
	}

	// If CAS line is low, set current column instead
	else	lcd_cas = val;

}


//----------------------------------------------------------------------------
//
//  Emulates reading from Mailstation LCD
//
uint8_t readLCD(uint16_t newaddr, int lcdnum)
{
	uint8_t *lcd_ptr;
	if (lcdnum == LCD_LEFT) lcd_ptr = ms.lcd_dat1bit;
	/* XXX: Fix the use of this magic number, replace with a const */
	else lcd_ptr = &ms.lcd_dat1bit[4800];
	if (!lcd_ptr) return 0;

	// Wraps memory address if out of bounds
	while (newaddr >= 240) newaddr -= 240;

	// Check CAS bit on P2
	if (ms.io[2] & 8)
	{
		// Return data on currently selected LCD column
		return lcd_ptr[newaddr + (lcd_cas * 240)];
	}

	// Not sure what this normally returns when CAS bit low!
	else return lcd_cas;

}



//----------------------------------------------------------------------------
//
//  Graphically prints a character to the SDL screen
//
void printcharXY(char mychar, int x, int y)
{
	SDL_Rect letterarea;
	SDL_Rect charoutarea;

	// CGA font characters are 8x8
	letterarea.w = letterarea.h = charoutarea.w = charoutarea.h = 8;
	letterarea.x = 0;
	letterarea.y = 8 * mychar;
	charoutarea.x = x;
	charoutarea.y = y;

	//SDL_GetVideoSurface()
	if (SDL_BlitSurface(cgafont_surface, &letterarea, lcd_surface, &charoutarea) != 0) printf("Error blitting text\n");
}


//----------------------------------------------------------------------------
//
//  Graphically prints a string at the specified X/Y coords
//
void printstringXY(char *mystring, int x, int y)
{
	while (*mystring)
	{
		printcharXY(*mystring,x,y);
		mystring++;
		// CGA font characters are 8x8
		x+=8;
		if (x > lcd_surface->w) { x = 0; y += 8; } //Move to the next line
	}
}

//----------------------------------------------------------------------------
//
//  Graphically prints a string centered at the specified Y coordinate
//
void printstring_centered(char *mystring, int y)
{
	int surface_cols = lcd_surface->w / 8;
	int x = (surface_cols - strlen(mystring)) / 2;
	printstringXY(mystring, x * 8, y);
}


//----------------------------------------------------------------------------
//
//  Graphically prints a string at the current cursor position
//
void printstring(char *mystring)
{
	while (*mystring)
	{
		if (*mystring == '\n') { cursorX = 0; cursorY++; mystring++; continue; }
		printcharXY(*mystring,cursorX * 8, cursorY * 8);
		mystring++;
		cursorX++;
		if (cursorX * 8 >= lcd_surface->w) { cursorX = 0; cursorY++; }
		if (cursorY * 8 >= lcd_surface->h) cursorY = 0;
	}
}


/* Read a uint8_t from the RAM buffer
 *
 * TODO: Add a debug hook here
 */
inline uint8_t readRAM(unsigned int translated_addr)
{
	return ms.ram[translated_addr];
}

/* Write a uint8_t to the RAM buffer
 *
 * TODO: Add a debug hook here
 */
inline void writeRAM(unsigned int translated_addr, uint8_t val)
{
	ms.ram[translated_addr] = val;
}



/* z80em Read memory callback function.
 *
 * Return a uint8_t from the address given to us.
 * This function needs to figure out what slot the requested address lies in,
 * figure out what page of which device is in that slot, and return data.
 *
 * We also need to translate the address requested to the correct offset of the
 * slotted page of the requested device.
 */
/* XXX: Can the current page/device logic be cleaned up to flow better? */
unsigned Z80_RDMEM(dword A)
{
	uint16_t addr = (uint16_t)A;
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
		current_page = slot4000_page;
		current_device = slot4000_device;
	}

	// Slot 0x8000
	if (addr >= 0x8000 && addr < 0xC000)
	{
		newaddr = addr - 0x8000;
		current_page = slot8000_page;
		current_device = slot8000_device;
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
			  Z80_GetPC(),current_page);
		}
		return readCodeFlash(translated_addr);

	  case 1: /* Dataflash */
		if (current_page >= 8) {
			log_error("[%04X] * INVALID RAM PAGE: %d\n",
			  Z80_GetPC(), current_page);
		}
		return readRAM(translated_addr);

	  case 2: /* LCD, left side */
		return readLCD(newaddr, LCD_LEFT);
		case 3: /* Dataflash */
		if (current_page >= 32) {
			log_error("[%04X] * INVALID DATAFLASH PAGE: %d\n",
			  Z80_GetPC(), current_page);
		}
		return readDataflash(translated_addr);

	  case 4: /* LCD, right side */
		return readLCD(newaddr, LCD_RIGHT);

	  case 5: /* MODEM */
		log_debug("[%04X] * READ FROM MODEM UNSUPPORTED: %04X\n",
		  Z80_GetPC(), newaddr);
		break;

	  default:
		log_error("[%04X] * READ FROM UNKNOWN DEVICE: %d\n",
		  Z80_GetPC(), current_device);
		break;
	}

	return 0;
}


/* z80em Write memory callback function.
 *
 * Write a uint8_t to the address given to us.
 * This function needs to figure out what slot the requested address lies in,
 * figure out what page of which device is in that slot, and return data.
 *
 * We also need to translate the address requested to the correct offset of the
 * slotted page of the requested device.
 */
/* XXX: Can the current page/device logic be cleaned up to flow better? */
void Z80_WRMEM(dword A,uint8_t val)
{
	uint16_t addr = (uint16_t)A;
	uint16_t newaddr = 0;
	uint8_t current_page = 0;
	uint8_t current_device = 0;
	uint32_t translated_addr;


	// Slot 0x0000 - always codeflash page 0
	if (addr < 0x4000)
	{
		log_error("[%04X] * CAN'T WRITE TO CODEFLASH SLOT 0: 0x%X\n",
		  Z80_GetPC(), addr);
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
		current_page = slot4000_page;
		current_device = slot4000_device;
	}

	// Slot 0x8000
	if (addr >= 0x8000 && addr < 0xC000)
	{
		newaddr = addr - 0x8000;
		current_page = slot8000_page;
		current_device = slot8000_device;
	}

	translated_addr = newaddr + (current_page * 0x4000);


	/* NOTE: While the read path seems to have the situation where the
	 * upper 4 bits of "current device" are set, I've never seen that be
	 * a problem here. Regardless, still mask those off.
	 */
	switch(current_device & 0x0F) {
	  case 0: /* Codeflash */
		log_error("[%04X] * WRITE TO CODEFLASH UNSUPPORTED\n",
		  Z80_GetPC());
		break;

	  case 1: /* RAM */
		if (current_page >= 8) {
			log_error("[%04X] * INVALID RAM PAGE: %d\n",
			  Z80_GetPC(), current_page);
		}
		writeRAM(translated_addr, val);
		break;

	  case 2: /* LCD, left side */
		writeLCD(newaddr, val, LCD_LEFT);
		break;

	  case 3: /* Dataflash */
		if (current_page >= 32) {
			log_error("[%04X] * INVALID DATAFLASH PAGE: %d\n",
			  Z80_GetPC(), current_page);
		}
		dataflash_updated = writeDataflash(translated_addr, val);
		break;

	  case 4: /* LCD, right side */
		writeLCD(newaddr, val, LCD_RIGHT);
		break;

	  case 5: /* MODEM */
		log_debug("[%04X] WRITE TO MODEM UNSUPPORTED: %04X %02X\n",
		  Z80_GetPC(), newaddr, val);
		break;

	  default:
		log_error("[%04X] * WRITE TO UNKNOWN DEVICE: %d\n",
		  Z80_GetPC(), current_device);
		break;
	}
}



//----------------------------------------------------------------------------
//
//  Z80em I/O port input handler
//
uint8_t Z80_In (uint8_t Port)
{
	uint16_t addr = (uint16_t)Port;

	// This is for the RTC on P10-1C
	time_t theTime;
	time( &theTime );
	struct tm *rtc_time = localtime(&theTime);

	log_debug("[%04X] * IO <- %04X - %02X\n", Z80_GetPC(), addr,ms.io[addr]);

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
			return interrupts_active;
			break;

		// page/device ports
		case 0x05:
		case 0x06:
		case 0x07:
		case 0x08:
			return ms.io[addr];


		// acknowledge power good + power button status
		case 0x09:
			return (uint8_t)0xE0 | ((power_button & 1) << 4);


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
//  Z80em I/O port output handler
//
/* XXX: Clean up this LED code at some point, have "real" LED on SDL window */
void Z80_Out (uint8_t Port,uint8_t val)
{
	uint16_t addr = (uint16_t)Port;
	static uint8_t tmp_reg;

	log_debug("[%04X] * IO -> %04X - %02X\n",Z80_GetPC(), addr, val);

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
			interrupts_active &= val;
			ms.io[addr] = val;
			break;

		// set slot4000 page
		case 0x05:
			slot4000_page = val;
			ms.io[addr] = slot4000_page;
			break;

		// set slot4000 device
		case 0x06:
			slot4000_device = val;
			ms.io[addr] = slot4000_device;
			break;

		// set slot8000 page
		case 0x07:
			slot8000_page = val;
			ms.io[addr] = slot8000_page;
			break;

		// set slot8000 device
		case 0x08:
			slot8000_device = val;
			ms.io[addr] = slot8000_device;
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


//----------------------------------------------------------------------------
//
//  Z80em library declarations
//

// Current IRQ status. Checked after EI occurs.  We won't need it (for now).
int Z80_IRQ;

// Run after a RETI
void Z80_Reti (void)
{
	return;
}

// Run after a RETN
void Z80_Retn (void)
{
	return;
}

// Can emulate stuff which we don't need
void Z80_Patch (Z80_Regs *Regs)
{
	return;
}

// Handler fired when Z80_ICount hits 0
int Z80_Interrupt(void)
{
	static int icount = 0;

	// Interrupt occurs at 64hz.  So this counter reduces to 1 sec intervals
	if (icount++ >= 64)
	{
		icount = 0;

		// do time16 interrupt
		if (ms.io[3] & 0x10 && !(interrupts_active & 0x10))
		{
			interrupts_active |= 0x10;
			return Z80_NMI_INT;
		}
	}

	// Trigger keyboard interrupt if necessary (64hz)
	if (ms.io[3] & 2 && !(interrupts_active & 2))
	{
		interrupts_active |= 2;
		return Z80_NMI_INT;
	}

	// Otherwise ignore this
	return Z80_IGNORE_INT;
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
	poweroff = 0;
	interrupts_active = 0;
	Z80_Reset();
}


//----------------------------------------------------------------------------
//
//  Disables emulation, displays opening screen
//
void powerOff()
{
	poweroff = 1;

	// clear LCD
	memset(ms.lcd_dat8bit, 0, 320*240);
	printstring_centered("Mailstation Emulator", 4 * 8);
	printstring_centered("v0.1a", 5 * 8);
	printstring_centered("Created by Jeff Bowman", 8 * 8);
	printstring_centered("(fyberoptic@gmail.com)", 9 * 8);
	printstring_centered("F12 to Start", 15 * 8);

	drawLCD();
}

/* XXX: This needs rework still*/
void msemustartSDL(void)
{
	SDL_Surface *cgafont_tmp = NULL;
	SDL_Color fontcolors[2];

	SDL_Init( SDL_INIT_VIDEO );

	/* Set up colors to be used by the LCD display from the MailStation */
	/* TODO: Can this be done as a single 24bit quantity?
	 */
	memset(colors,0,sizeof(SDL_Color) * 6);
	/* Black */
	colors[0].r = 0x00; colors[0].g = 0x00; colors[0].b = 0x00;
	/* Green */
	colors[1].r = 0x00; colors[1].g = 0xff; colors[1].b = 0x00;
	/* LCD Off-Green */
	colors[2].r = 0x9d; colors[2].g = 0xe0; colors[2].b = 0x8c;
	/* LCD Pixel Black */
	colors[3].r = 0x26; colors[3].g = 0x21; colors[3].b = 0x14;
	/* Blue */
	colors[4].r = 0x00; colors[4].g = 0x00; colors[4].b = 0xff;
	/* Yellow */
	colors[5].r = 0xff; colors[5].g = 0xff; colors[5].b = 0x00;


	/* Set up SDL screen
	 *
	 * TODO:
	 *   Worth implementing a resize feature?
	 */
	screen = SDL_SetVideoMode(SCREENWIDTH*2, SCREENHEIGHT*2, 8,
	  SDL_HWPALETTE);
	/*XXX: Check screen value */
	if (SDL_SetPalette(screen, SDL_LOGPAL|SDL_PHYSPAL, colors, 0, 6) != 1) {
		printf("Error setting palette\n");
	}

	// Set window caption
	SDL_WM_SetCaption("Mailstation Emulator", NULL);



	/* XXX: This color set up is really strange, fontcolors sets up yellow
	 * on black font. However, it seems to be linked with the colors
	 * array above. If one were to remove colors[5], that causes the
	 * font color to cange. I hope that moving to a newer SDL version
	 * will improve this.
	 */
	// Load embedded font for graphical print commands
	cgafont_tmp = SDL_CreateRGBSurfaceFrom((uint8_t*)rawcga_start,
	  8, 2048, 1, 1,  0,0,0,0);
	if (cgafont_tmp == NULL) {
		printf("Error creating font surface\n");
		//return 1;
	}

	// Setup font palette
	memset(fontcolors, 0, sizeof(fontcolors));
	// Use yellow foreground, black background
	fontcolors[1].r = fontcolors[1].g = 0xff;
	// Write palette to surface
	if (SDL_SetPalette(cgafont_tmp, SDL_LOGPAL|SDL_PHYSPAL, fontcolors,
	  0, 2) != 1) {
		printf("Error setting palette on font\n");
	}

	// Convert the 1-bit font surface to match the display
	cgafont_surface = SDL_DisplayFormat(cgafont_tmp);
	// Free the 1-bit version
	SDL_FreeSurface(cgafont_tmp);


	// Create 8-bit LCD surface for drawing to emulator screen
	lcd_surface = SDL_CreateRGBSurfaceFrom(ms.lcd_dat8bit, 320, 240, 8, 320, 0,0,0,0);
	if (lcd_surface == NULL) {
		printf("Error creating LCD surface\n");
		//return 1;
	}

	// Set palette for LCD to global palette
	if (SDL_SetPalette(lcd_surface, SDL_LOGPAL|SDL_PHYSPAL, colors, 0, 6) != 1) printf("Error setting palette on LCD\n");
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
	int silent = 0;
	int execute_counter = 0;
	int exitemu = 0;
	uint32_t lasttick = SDL_GetTicks();
	uint32_t currenttick;
	SDL_Event event;

	static struct option long_opts[] = {
	  { "help", no_argument, NULL, 'h' },
	  { "codeflash", required_argument, NULL, 'c' },
	  { "dataflash", required_argument, NULL, 'd' },
	  { "logfile", optional_argument, NULL, 'l' },
	  { "silent", no_argument, NULL, 's' },
	  { NULL, no_argument, NULL, 0}
	};

	/* TODO:
	 *   Rework main and break out in to smaller functions.
	 *   Set up buffers and parse files
	 *   Set up SDL calls
	 *   Then get in to loop
	 */

	/* Process arguments */
	/* TODO:
	 *   Rework runsilent flag and setup
	 */
	/* TODO:
	 *   Implement the old debug flags below
	 *   Implement some internal debugging? Z80 state machine, etc.
	 *
	 *      //for (n = 1; n < argc; n++) {
         *      // Print all DebugOut lines to console
	 *      //      if (strcmp(argv[n],"/console") == 0) runsilent = 0;
	 *
	 *      // Print all DebugOut lines to file
	 *      //      if (strcmp(argv[n],"/debug") == 0) debugoutfile = fopen("debug.out","w");
	 *      //}
	 */
	while ((c = getopt_long(argc, argv,
	  "hc:d:l:s", long_opts, NULL)) != -1) {
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
		  case 's':
		    silent = 1;
		    break;
		  case 'h':
		  default:
			printf("Usage: %s [-s] [-c <path>] [-d <path>] [-l <path>]\n", argv[0]);
			printf(" -c <path>   | path to codeflash (default: %s)\n", codeflash_path);
			printf(" -d <path>   | path to dataflash (default: %s)\n", dataflash_path);
			printf(" -l <path>   | path to log file\n");
			printf(" -s          | runs silently\n");
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
	ms.lcd_dat1bit = (uint8_t *)calloc(((SCREENWIDTH * SCREENHEIGHT) / 8),
	  sizeof(uint8_t));
	ms.lcd_dat8bit = (uint8_t *)calloc(  SCREENWIDTH * SCREENHEIGHT,
	  sizeof(uint8_t));

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

	msemustartSDL();




	/* Set up and start Z80 emulation */
	Z80_Reset();			/* Reset CPU state */
	Z80_Running = 1;		/* When 0, emulation terminates */
	Z80_ICount = 0;			/* T-state count */
	Z80_IRQ = Z80_IGNORE_INT;	/* Current IRQ status. */
	/* MS runs at 12 MHz, divide by 64 for KB IRQ rate */
	Z80_IPeriod = 187500;		/* Number of T-states per interrupt */

	// Display startup message
	powerOff();

	lasttick = SDL_GetTicks();

	while (!exitemu)
	{
		currenttick = SDL_GetTicks();

		/* Let the Z80 process code at regular intervals */
		if (!poweroff) {
			/* XXX: Can replace with SDL_TICKS_PASSED with new
			 * SDL version.
			 */
			execute_counter += currenttick - lasttick;
			if (execute_counter > 15) {
				execute_counter = 0;
				Z80_Execute();
			}
		}

		/* Update LCD if modified (at 20ms rate) */
		/* TODO: Check over this whole process logic */
		/* NOTE: Cursory glance suggests the screen updates 20ms after
		 * the screen array changed.
		 */
		if ((lcd_lastupdate != 0) &&
		  (currenttick - lcd_lastupdate >= 20)) {
			drawLCD();
		}

		/* Write dataflash buffer to disk if it was modified */
		if (dataflash_updated) {
			buftoflash(ms.dataflash, dataflash_path, MEBIBYTE/2);
			/* XXX: Check return value */
			dataflash_updated = 0;
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
					power_button = 1;
					if (poweroff)
					{
						printf("POWER ON\n");

						resetMailstation();
					}
				} else {
					power_button = 0;
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
							if (!poweroff)
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

