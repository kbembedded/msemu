/*
	Mailstation Emulator v0.1a
	(01/05/2010)
	
	Required libraries to compile:
	  - SDL
	    (http://www.libsdl.org/)
	  - SDL_gfx
	    (http://www.ferzkopp.net/joomla/content/view/19/14/)
	  - z80em
	    (http://www.komkon.org/~dekogel/misc.html)
			
	
	This software is free to use/modify/distribute for 
	non-commercial purposes only.
	
	If you modify and redistribute this software, you
	must credit the original author.
	
	Feel free to contact me at fyberoptic@gmail.com for
	any questions/comments!
	
	http://www.fybertech.net/mailstation
	
	
	Copyright (c) 2010 Jeff Bowman 	
*/



#ifdef WIN32
	// Use at least XP, for AttachConsole
	#define WINVER 0x0501
	#include <windows.h>
	#include <wincon.h>
#else
	typedef unsigned int DWORD;
#endif


#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <strings.h>
#include <time.h>
#include "rawcga.h"
#include "z80em/Z80.h"
#include "z80em/Z80IO.h"
#include <SDL/SDL.h>
#include <SDL/SDL_rotozoom.h>

typedef unsigned short ushort;

#define LCD_LEFT 1
#define LCD_RIGHT 2


// This is the embedded font we need to print graphical text.
char *rawcga_start = &raw_cga_array[0];

// Default screen size
int SCREENWIDTH = 320;
int SCREENHEIGHT = 240;



// Mailstation RAM - 128KB
byte *ram = NULL;

// Mailstation I/O ports - 64KB
byte *ioports = NULL;

// Mailstation codeflash - 1MB (depends on loaded codeflash file!)
byte *codeflash = NULL;

// Mailstation dataflash - 512KB (depends on loaded dataflash file!)
byte *dataflash = NULL;



// Mailstation left/right LCD halves - 4800 bytes each
byte *lcd_left = NULL;
byte *lcd_right = NULL;

// Buffer for storing 8-bit version of LCD data (for drawing to screen)
byte *lcd_data8 = NULL;

// Stores current Mailstation LCD column
byte lcd_cas = 0;

// Last SDL tick at which LCD was updated.  Used for timing LCD refreshes to screen.
DWORD lcd_lastupdate = 0;

// Primary SDL screen surface
SDL_Surface *screen;

// Master palette
SDL_Color colors[256];

// Default entry of color palette to draw Mailstation LCD with
byte LCD_color = 3;  // green

// Surface for the Mailstation LCD, will be 320x240 
SDL_Surface *lcd_surface;

// Whether we're currently fullscreen or not
int isfullscreen = 0;

// Whether Mailstation LCD is scaled
int LCD_2X = 0;

// Surface to load CGA font data, for printing text with SDL
SDL_Surface *cgafont_surface = NULL;

// This is the surface that all text print functions go on
SDL_Surface *textsurface = NULL;

// Cursor position for unfinished code to print text in SDL
int cursorX = 0;
int cursorY = 0;



// Bits specify which interrupts have been triggered (returned on P3)
byte interrupts_active = 0;

// This is set if the dataflash contents are changed, so that we can write the contents to file.
int dataflash_updated = 0;

// This is set if hardware power off is detected (via P28), to halt emulation
int poweroff = 0;

// Stores the page/device numbers of the two middle 16KB slots of address space
byte slot4000_page = 0;
byte slot4000_device = 0;
byte slot8000_page = 0;
byte slot8000_device = 0;

// This is used for writing to dataflash, to determine what Flash chip command to use
byte dataflash_lastwrite = 0;



// Default codeflash file to load
char codeflash_filename_default[] = "codeflash.bin";

// Stores name of codeflash file if specified on command line
char *codeflash_filename = codeflash_filename_default;

// Default dataflash file to load/create
char dataflash_filename[] = "dataflash.bin";

// This handle is used for outputting all debug info to a file with /debug
FILE *debugoutfile = NULL;



// If this is true, then certain debug output isn't displayed to console (slows down emulation)
int runsilent = 1;


// Holds power button status (returned in P9.4)
byte power_button = 0;

// Simulates keyboard hardware matrix
byte keypress_array[10];

// This table translates PC scancodes to the Mailstation key matrix
int keyTranslateTable[10][8] = {
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
//  Convert byte to BCD format
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
//  Outputs general messages
//
void myprintf(char *mystring, ...)
{
	if (runsilent) return;
	
	va_list argptr;
	va_start( argptr, mystring );
	
	char newstring[1024];	
	vsprintf(newstring, mystring, argptr);
	
	// Print to SDL surface
	//printstring(newstring);
	
	// Print to console
	puts(newstring);
		
	va_end( argptr );
}

//----------------------------------------------------------------------------
//
//  Outputs debug messages
//
void DebugOut(char *mystring,...)
{	
	if (!debugoutfile && runsilent) return;
	
	va_list argptr;
	va_start( argptr, mystring );
	
	char newstring[1024];	
	vsprintf(newstring, mystring, argptr);
	
	// If debug file open, print there
	if (debugoutfile) fputs(newstring,debugoutfile);
	
	// If not silent, print to screen too
	if (!runsilent)
	{
		// Print to SDL surface 
		//printstring(newstring);
		
		// Print to console
		printf("%s",newstring);		
	}
		
	va_end( argptr );	
}




//----------------------------------------------------------------------------
//
//  Draws the LCD to the screen
//
void drawLCD()
{	
	// Set output surface to 1x version for now
	SDL_Surface *lcd_surface_temp = lcd_surface;
	
	// Setup output rect to fill screen for now
	SDL_Rect outrect;
	outrect.x = 0;
	outrect.y = 0;
	outrect.w = SCREENWIDTH;
	outrect.h = SCREENWIDTH;
	
	if (LCD_2X)
	{
		// Double surface size to 640x480
		SDL_Surface *lcd_surface2x = zoomSurface(lcd_surface, (double)2.0, (double)2.0, 0);
		// If we don't clear the color key, it constantly overlays just the primary color during blit!
		SDL_SetColorKey(lcd_surface2x, 0, 0);
		
		// Switch output surface to this one
		lcd_surface_temp = lcd_surface2x;
	}
	
	// If fullscreen but not 2X, center display	
	else if (isfullscreen && !LCD_2X)
	{
		outrect.x = 160;
		outrect.y = 120;
		outrect.w = 320;
		outrect.h = 240;
	}
	
	// Draw to screen
	if (SDL_BlitSurface(lcd_surface_temp, NULL, screen, &outrect) != 0) printf("Error blitting LCD to screen: %s\n",SDL_GetError());
	//SDL_UpdateRect(SDL_GetVideoSurface(), 0,0, SCREENWIDTH, SCREENHEIGHT);
	SDL_Flip(screen);
	
	if (LCD_2X)
	{
		// Dump 2x surface
		SDL_FreeSurface(lcd_surface_temp);
	}
	
	// Screen has been updated, don't need to do it again until changes are made
	lcd_lastupdate = 0;
}



//----------------------------------------------------------------------------
//
//  Emulates writing to Mailstation LCD device
//
void writeLCD(ushort newaddr, byte val, int lcdnum)
{		
	byte *lcd_ptr;
	if (lcdnum == LCD_LEFT) lcd_ptr = lcd_left;
	if (lcdnum == LCD_RIGHT) lcd_ptr = lcd_right;
	if (!lcd_ptr) return;
	
	// Wraps memory address if out of bounds	
	while (newaddr >= 240) newaddr -= 240;
	
	// Check CAS bit on P2
	if (ioports[2] & 8)
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
			lcd_data8[n + (x * 8) + (newaddr * 320)] = ((val >> n) & 1 ? LCD_color : 0);
		}
		
		// Let main loop know to update screen with new LCD data
		lcd_lastupdate = SDL_GetTicks();
	}
	
	// If CAS line is low, set current column instead
	else	lcd_cas = val;
	
}


//----------------------------------------------------------------------------
//
//  Clears LCD display buffer (just the SDL surface data, not the Mailstation's)
//
void clearLCD()
{
	memset(lcd_data8, 0, 320*240);
}



//----------------------------------------------------------------------------
//
//  Emulates reading from Mailstation LCD
//
byte readLCD(ushort newaddr, int lcdnum)
{		
	byte *lcd_ptr;
	if (lcdnum == LCD_LEFT) lcd_ptr = lcd_left;
	if (lcdnum == LCD_RIGHT) lcd_ptr = lcd_right;
	if (!lcd_ptr) return 0;
	
	// Wraps memory address if out of bounds
	while (newaddr >= 240) newaddr -= 240;
	
	// Check CAS bit on P2
	if (ioports[2] & 8)
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
	if (SDL_BlitSurface(cgafont_surface, &letterarea, textsurface, &charoutarea) != 0) printf("Error blitting text\n");
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
		if (x > textsurface->w) { x = 0; y += 8; }
	}
}

//----------------------------------------------------------------------------
//
//  Graphically prints a string centered at the specified Y coordinate
//
void printstring_centered(char *mystring, int y)
{
	int surface_cols = textsurface->w / 8;
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
		if (cursorX * 8 >= textsurface->w) { cursorX = 0; cursorY++; }
		if (cursorY * 8 >= textsurface->h) cursorY = 0;
	}	
}





//----------------------------------------------------------------------------
//
//  This function emulates reading from a 28SF040 flash chip.
//
//  NOTE: It currently does not support Data Protect or Data Unprotect 
//  sequences.
//
byte readDataflash(unsigned int translated_addr)
{
	// Limit to 512KB
	return dataflash[translated_addr & 0x7FFFF];
}


//----------------------------------------------------------------------------
//
//  This function emulates writing to a 28SF040 flash chip.
//
//  NOTE: It currently supports Sector-Erase and Byte-Program commands ONLY.
//
void writeDataflash(unsigned int translated_addr, byte val)
{
	// Limit to 512KB
	translated_addr &= 0x7FFFF;
	
	if (dataflash_lastwrite > 0)
	{
		switch(dataflash_lastwrite)
		{
			case 0x20:							
				translated_addr &= 0xFFFFFF00;
				DebugOut("[%04X] * Dataflash Sector-Erase: 0x%X\n", Z80_GetPC(), translated_addr);
				memset(dataflash + translated_addr, 0xFF, 256);
				dataflash_updated = 1;
				break;
				
			case 0x10:
				DebugOut("[%04X] * Dataflash Byte-Program: 0x%X = %02X\n",Z80_GetPC(),translated_addr,val);
				dataflash[translated_addr] = val;
				dataflash_updated = 1;
				break;
				
			case 0x30:
				DebugOut("[%04X] * UNHANDLED DATAFLASH CHIP-ERASE\n");
				break;
				
			default:
				DebugOut("[%04X] * INVALID DATAFLASH COMMAND SEQUENCE: %02X %02X\n", dataflash_lastwrite, val);
		}
		// Make sure Read-ID command stays in buffer since read is required afterward
		if (val != 0x90) dataflash_lastwrite = 0;
	}
	// Ignore Reset command
	else if (val != 0xFF) dataflash_lastwrite = val;
}


//----------------------------------------------------------------------------
//
//  Reads Mailstation RAM
//
byte readRAM(unsigned int translated_addr)
{
	// Limit to 128KB
	return ram[translated_addr & 0x1FFFF];
}


//----------------------------------------------------------------------------
//
//  Writes Mailstation RAM
//
void writeRAM(unsigned int translated_addr, byte val)
{
	// Limit to 128KB
	ram[translated_addr & 0x1FFFF] = val;
}



//----------------------------------------------------------------------------
//
//  Z80em Read Memory handler
//
unsigned Z80_RDMEM(dword A)
{	
	ushort addr = (ushort)A;	
	ushort newaddr;
	byte current_page;
	byte current_device;

	
	// Slot 0x0000 - always codeflash page 0
	if (addr < 16384) return codeflash[addr];

	// Slot 0xC000 - always RAM page 0
	if (addr >= 49152) return ram[addr - 49152];	
	
	// Slot 0x4000
	if (addr >= 16384 && addr < 32768)
	{
		newaddr = addr - 16384;		
		current_page = slot4000_page;
		current_device = slot4000_device;
	}
	
	// Slot 0x8000
	if (addr >= 32768 && addr < 49152)
	{
		newaddr = addr - 32768;		
		current_page = slot8000_page;
		current_device = slot8000_device;
	}
		
	unsigned int translated_addr = newaddr + (current_page * 16384);
		
	switch(current_device)
	{
			case 0:
				if (current_page >= 64) DebugOut("[%04X] * INVALID CODEFLASH PAGE: %d\n", Z80_GetPC(),current_page);
				// Limit to 1MB
				return codeflash[translated_addr & 0xFFFFF];

			case 1:				
				if (current_page >= 8) DebugOut("[%04X] * INVALID RAM PAGE: %d\n", Z80_GetPC(),current_page);				
				return readRAM(translated_addr);

			case 2:				
				return readLCD(newaddr,LCD_LEFT);
				break;

			case 3:				
				if (current_page >= 32) DebugOut("[%04X] * INVALID DATAFLASH PAGE: %d\n",Z80_GetPC(),current_page);				
				return readDataflash(translated_addr);
				
			case 4:				
				return readLCD(newaddr,LCD_RIGHT);
				break;
				
			case 5:				
				DebugOut("[%04X] * READ FROM MODEM UNSUPPORTED: %04X\n",Z80_GetPC(), newaddr);
				/*switch (newaddr)
				{
					case 0x0002:
						return 0xC2;						
				}*/
				break;
				
			default:
				DebugOut("[%04X] * READ FROM UNKNOWN DEVICE: %d\n", Z80_GetPC(), current_device);
			
	}

	return 0;		

}


//----------------------------------------------------------------------------
//
//  Z80em Write Memory handler
//
void Z80_WRMEM(dword A,byte val)
{
	ushort addr = (ushort)A;
	ushort newaddr;	
	byte current_page;
	byte current_device;
	
	
	// Slot 0x0000 - always codeflash page 0
	if (addr < 16384)
	{
		DebugOut("CAN'T WRITE TO CODEFLASH SLOT 0\n"); 
		return; 
	}	

	// Slot 0xC000 - always RAM page 0
	if (addr >= 49152)
	{
		ram[addr - 49152] = val;
		return; 
	}
	
	// Slot 0x4000
	if (addr >= 16384 && addr < 32768)
	{
		newaddr = addr - 16384;		
		current_page = slot4000_page;
		current_device = slot4000_device;
	}	
	
	// Slot 0x8000
	if (addr >= 32768 && addr < 49152)
	{
		newaddr = addr - 32768;		
		current_page = slot8000_page;
		current_device = slot8000_device;
	}
		
	unsigned int translated_addr = newaddr + (current_page * 16384);
	
	
	switch(current_device)
	{
			case 0:
				DebugOut("WRITE TO CODEFLASH UNSUPPORTED\n");				
				break;
				
			case 1:
				if (current_page >= 8) DebugOut("[%04X] * INVALID RAM PAGE: %d\n", Z80_GetPC(), current_page);
				// Limit to 128KB				
				writeRAM(translated_addr,val);				
				break;
				
			case 2:				
				writeLCD(newaddr,val,LCD_LEFT);					
				break;
				
			case 3:								
				if (current_page >= 32) DebugOut("[%04X] * INVALID DATAFLASH PAGE: %d\n", Z80_GetPC(), current_page);				
				writeDataflash(translated_addr,val);					
				break;
				
			case 4:				
				writeLCD(newaddr,val,LCD_RIGHT);
				break;
				
			case 5:				
				DebugOut("[%04X] WRITE TO MODEM UNSUPPORTED: %04X %02X\n",Z80_GetPC(), newaddr, val);
				/*switch(newaddr)
				{
					case 0x0001:
						// Trigger modem interrupt
						if (val & 2 && ioports[3] & 64) { printf("Triggering transmit empty interrupt\n"); interrupts_active |= 64; }
						break;
				}*/
				
				break;
				
			default:
				DebugOut("[%04X] * WRITE TO UNKNOWN DEVICE: %d\n", Z80_GetPC(), current_device);
			
	}
}



//----------------------------------------------------------------------------
//
//  Z80em I/O port input handler
//
byte Z80_In (byte Port)
{	
	ushort addr = (ushort)Port;
	
	// This is for the RTC on P10-1C
	time_t theTime;
	time( &theTime );
	struct tm *rtc_time = localtime(&theTime);
	
	DebugOut("[%04X] * IO <- %04X - %02X\n", Z80_GetPC(), addr,ioports[addr]);
	
	//if (addr < 5 || addr > 8) printf("* IO <- %04X - %02X\n",addr,ioports[addr]);
	
	switch (addr)
	{
		
		// emulate keyboard matrix output
		case 0x01:					
			{
				// keyboard row is 10 bits wide, P1.x = low bits, P2.0-1 = high bits
				unsigned short kbaddr = ioports[1] + ((ioports[2] & 3) << 8);
				
				// all returned bits should be high unless a key is pressed
				byte kbresult = 0xFF;
				
				// check for a key pressed in active row(s)
				int n;
				for (n = 0; n < 10; n++)
				{
					if (!((kbaddr >> n) & 1)) kbresult &= keypress_array[n];
				}		
				
				return kbresult;
			}			
			break;
		
		
		// NOTE: lots of activity on these two during normal loop
		case 0x21: 
		case 0x1D:
			
		case 0x02:
			return ioports[addr];
		
			
		
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
			return ioports[addr];
			
		
		// acknowledge power good + power button status
		case 0x09:			
			return (byte)0xE0 | ((power_button & 1) << 4);				
						
			
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
			//printf("* UNKNOWN IO <- %04X - %02X\n",addr, ioports[addr]);
			return ioports[addr];
	}
	
}



//----------------------------------------------------------------------------
//
//  Z80em I/O port output handler
//
void Z80_Out (byte Port,byte val)
{
	ushort addr = (ushort)Port;
	
	DebugOut("[%04X] * IO -> %04X - %02X\n",Z80_GetPC(), addr, val);
	
	//if (addr < 5 || addr > 8) printf("* IO -> %04X - %02X\n",addr, val);
		
	switch (addr)
	{		
		case 0x01:
		case 0x02:
		
		// Note: Lots of activity on these next ones during normal loop
		case 0x2C:
		case 0x2D:
		case 0x1D:
			ioports[addr] = val;
			break;
		
		
		// set interrupt masks
		case 0x03:			
			interrupts_active &= val;
			ioports[addr] = val;
			break;
			
		// set slot4000 page
		case 0x05:			
			slot4000_page = val;
			ioports[addr] = slot4000_page;
			break;
		
		// set slot4000 device
		case 0x06:
			slot4000_device = val;
			ioports[addr] = slot4000_device;
			break;
		
		// set slot8000 page
		case 0x07:
			slot8000_page = val;
			ioports[addr] = slot8000_page;
			break;
		
		// set slot8000 device
		case 0x08:
			slot8000_device = val;
			ioports[addr] = slot8000_device;
			break;
			
		// check for hardware power off bit in P28
		case 0x028:			
			if (val & 1)			
			{
				printf("POWER OFF\n");
				powerOff();
			}
			ioports[addr] = val;
			break;
			
		// otherwise just save written value
		default:
			//printf("* UNKNOWN IO -> %04X - %02X\n",addr, val);
			ioports[addr] = val;
	}	
}



//----------------------------------------------------------------------------
//
//  Translates PC keys to Mailstation keyboard matrix
//
void generateKeyboardMatrix(int scancode, int eventtype)
{
	int rows,cols;
	for (rows = 0; rows < 10; rows++)
	{
		for (cols = 0; cols < 8; cols++)
		{
			if (scancode == keyTranslateTable[rows][cols])
			{
				//keypress_array[5] &= 0x7F;
				if (eventtype == SDL_KEYDOWN)
				{
					keypress_array[rows] &= ~((byte)1 << cols);
				}
				else
				{
					keypress_array[rows] |= ((byte)1 << cols);
				}
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
	static icount = 0;		
	
	// Interrupt occurs at 64hz.  So this counter reduces to 1 sec intervals
	if (icount++ >= 64)
	{
		icount = 0;
				
		// do time16 interrupt
		if (ioports[3] & 0x10 && !(interrupts_active & 0x10))
		{				
			interrupts_active |= 0x10;
			return Z80_NMI_INT;
		}
	}
	
	// Trigger keyboard interrupt if necessary (64hz)
	if (ioports[3] & 2 && !(interrupts_active & 2))
	{
		interrupts_active |= 2;
		return Z80_NMI_INT;
	}		
	
	// Otherwise ignore this
	return Z80_IGNORE_INT;
}


//----------------------------------------------------------------------------
//
//  Convoluted process to connect STDIO to Windows console I/O
//
#ifdef WIN32
void RedirectIOToConsole()
{
	
	// SDL hijacks STDIO and outputs to file.  This kills that (but annoyingly leaves the file behind).	
	// NOTE: The SDL library used to build the official EXE has this redirection disabled by default.
	/*
	freopen ("CONOUT$", "w", stdout);
	freopen ("CONIN$", "w", stdin);	
	freopen ("CONOUT$", "w", stderr);	
	*/

	// Allocate a console for this app (try to attach first due to SDL allocating a console).
	//if (!AttachConsole(-1)) AllocConsole(); 	
	if (!AttachConsole(-1)) return;
    
	HANDLE newConsoleInput = GetStdHandle(STD_INPUT_HANDLE);
	HANDLE newConsoleOutput = GetStdHandle(STD_OUTPUT_HANDLE);

	int inFd = _open_osfhandle((long)newConsoleInput, _O_TEXT);
	int outFd = _open_osfhandle((long)newConsoleOutput, _O_TEXT);
   
	FILE* consoleIn = _fdopen(inFd, "r");
	FILE* consoleOut = _fdopen(outFd, "w");

	setvbuf(consoleIn, NULL, _IONBF, 0);
	setvbuf(consoleOut, NULL, _IONBF, 0);
    
	*stdin = *consoleIn;
	*stdout = *consoleOut;
	
	// Make cout, cin, etc, point to console as well.
	// Except we're in standard C so forget this for now.
	//ios::sync_with_stdio();*/

}
#else
void RedirectIOToConsole()
{
	// If not in Windows, we don't need this crap.
	return;
}
#endif





//----------------------------------------------------------------------------
//
//  Resets Mailstation state
//
void resetMailstation()
{
	clearLCD();
	memset(ioports,0,64 * 1024);
	// NOTE: Mailstation normally retains RAM I believe.  But Mailstation OS 
	// won't warm-boot properly if we don't erase!  Not sure why yet.
	memset(ram,0,128 * 1024);
	poweroff = 0;
	interrupts_active = 0;
	dataflash_lastwrite = 0;	
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
	memset(lcd_data8, 0, 320*240);
	textsurface = lcd_surface;	
	printstring_centered("Mailstation Emulator", 4 * 8);
	printstring_centered("v0.1a", 5 * 8);
	printstring_centered("Created by Jeff Bowman", 8 * 8);
	printstring_centered("(fyberoptic@gmail.com)", 9 * 8);
	printstring_centered("F12 to Start", 15 * 8);
	
	drawLCD();	
}


int setVideo(int width, int height, int fullscreen)
{
	int SDLflags = SDL_HWPALETTE;
	//SDL_DOUBLEBUF | SDL_HWSURFACE
	
	if (fullscreen) SDLflags |= SDL_FULLSCREEN;
	
	// Setup screen
	screen = SDL_SetVideoMode(width, height, 8, SDLflags);
	
	if (screen == NULL) return 1;
	
	// Write palette
	if (SDL_SetPalette(screen, SDL_LOGPAL|SDL_PHYSPAL, colors, 0, 256) != 1) printf("Error setting palette\n");
	
	isfullscreen = fullscreen;
	
	return 0;
}


//----------------------------------------------------------------------------
//
//  main() or WinMain()
//
//  WinMain is used to avoid SDL from overriding the main() function, which
//  the SDLmain library then overrides STDIO and generates a useless stdout.txt 
//  which we don't want.
//
//  NOTE:  Version of SDL compiled into official msemu.exe now has this disabled.
//
//int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int sw)
int main(int argc, char *argv[])
{	
	
	// Redirect STDIO to Windows console
	RedirectIOToConsole();
	
	// Process arguments, when main() is used
	int n;
	for (n = 1; n < argc; n++)
	{
		// If doesn't start with /, then use as the codeflash image filename
		if ((argv[n])[0] != '/') codeflash_filename = argv[n];		
		
		// Print all DebugOut lines to console
		else if (strcmp(argv[n],"/console") == 0) runsilent = 0;
			
		// Print all DebugOut lines to file
		else if (strcmp(argv[n],"/debug") == 0) debugoutfile = fopen("debug.out","w");
	}
	
	
	/*
	// Crap to simulate argv and argc when using WinMain()
	int argc;	
	LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
	if (argc > 1)
	{
		int n;
		for (n = 1; n < argc; n++)
		{
			char tempstring[1024];
			WideCharToMultiByte(CP_UTF8, (DWORD)NULL, argv[n], (int)-1, tempstring, sizeof(tempstring), NULL, NULL);			
			
			// If doesn't start with /, then use as the codeflash image filename
			if (tempstring[0] != '/')
			{
				codeflash_filename = malloc(strlen(tempstring) + 1);
				strcpy(codeflash_filename,tempstring);
			}
			//if (strcmp(tempstring,"/silent") == 0) runsilent = 1;
			//if (strcmp(tempstring,"/debug") == 0) debugoutfile = fopen("debug.out","wb");
			
		}
	}
	LocalFree(argv);
	*/
	
	
	SDL_Init( SDL_INIT_VIDEO );
	
	// Setup some colors	
	memset(colors,0,sizeof(SDL_Color) * 256);
	colors[1].r = colors[1].g = colors[1].b = 0xFF;
	colors[2].r = 0xff;	
	colors[3].g = 0xff;	
	colors[4].b = 0xff;	
	colors[5].r = colors[5].g = 0xff;

	
	// Setup screen
	setVideo(SCREENWIDTH, SCREENHEIGHT, 0);	
	
	
	// Set window caption
	SDL_WM_SetCaption("Mailstation Emulator",NULL);
		
	
	
	// Load embedded font for graphical print commands	
	SDL_Surface *cga_temp = SDL_CreateRGBSurfaceFrom((byte*)rawcga_start, 8, 2048, 1, 1,  0,0,0,0);
	if (cga_temp == NULL) { printf("Error creating font surface\n"); return 1; }
		
	// Setup font palette
	SDL_Color fontcolors[2];
	memset(fontcolors,0,sizeof(fontcolors));	
	// Use yellow
	fontcolors[1].r = fontcolors[1].g = 0xff;
	// Write palette to surface
	if (SDL_SetPalette(cga_temp, SDL_LOGPAL|SDL_PHYSPAL, fontcolors, 0, 2) != 1) printf("Error setting palette on font\n");
		
	// Convert the 1-bit font surface to match the display
	cgafont_surface = SDL_DisplayFormat(cga_temp);
	// Dump the 1-bit version
	SDL_FreeSurface(cga_temp);
	
	
		
	
	printf("\nMailstation Emulator v0.1\n");
	printf("Created by Jeff Bowman (fyberoptic@gmail.com)\n\n");
	
	
	// read codeflash file
	FILE *codeflash_file = fopen(codeflash_filename, "rb");
	if (codeflash_file)
	{
		fseek(codeflash_file,0,SEEK_END);
		int filesize = ftell(codeflash_file);
		
		printf("Loading Codeflash ROM:\n  %s (%d bytes)\n",codeflash_filename, filesize);
		fseek(codeflash_file,0,SEEK_SET);
		// Allocate the full 1MB
		codeflash = (byte*)calloc(1024*1024,1);
		// If file is too big, only read first 1MB
		fread(codeflash , 1, (filesize > 1024*1024 ? 1024*1024 : filesize), codeflash_file);
		
		fclose(codeflash_file);		
	}
	else { printf("Couldn't open codeflash file: %s\n",codeflash_filename); return 1; }


	// read dataflash file
	FILE *dataflash_file = fopen(dataflash_filename, "rb");
	if (dataflash_file)
	{
		fseek(dataflash_file,0,SEEK_END);
		int filesize = ftell(dataflash_file);
		
		printf("Loading Dataflash ROM:\n  %s (%d bytes)\n",dataflash_filename, filesize);
		fseek(dataflash_file,0,SEEK_SET);
		// Allocate full 512KB
		dataflash = (byte*)calloc(512*1024,1);
		// If file is too big, only read first 512KB
		fread(dataflash , 1, (filesize > 512*1024 ? 512*1024 : filesize), dataflash_file);
		
		fclose(dataflash_file);		
	}
	// if one doesn't exist, then generate a fresh 512KB
	else
	{		
		printf("Generating new dataflash file: %s\n",dataflash_filename);
		dataflash = calloc(1,512*1024);
	}	

	putchar('\n');
	
		
	// Allocate 128KB for system memory
	ram = (byte *)calloc(131072, 1);

	// Allocate 64KB for I/O port storage (z80em doesn't use more than 256 though apparently)
	ioports = (byte *)calloc(65536,1);
	
	// Allocate hardware buffers for LCD
	lcd_left = (byte *)calloc(4800,1);
	lcd_right = (byte *)calloc(4800,1);
	
	
	// Generate an 8-bit 320x240 buffer for LCD
	lcd_data8 = (byte *)calloc(76800,1);
	
	// Create 8-bit LCD surface for drawing to emulator screen
	lcd_surface = SDL_CreateRGBSurfaceFrom(lcd_data8, 320, 240, 8, 320, 0,0,0,0);
	if (lcd_surface == NULL) { printf("Error creating LCD surface\n"); return 1; }

	// Set palette for LCD to global palette	
	if (SDL_SetPalette(lcd_surface, SDL_LOGPAL|SDL_PHYSPAL, colors, 0, 256) != 1) printf("Error setting palette on LCD\n");

	
	// Print graphical text onto the LCD surface
	textsurface = lcd_surface;	

	// Setup keyboard emulation
	memset(keypress_array,0xff,sizeof(keypress_array));
	
	
	// Setup Z80 emulation
	Z80_Running = 1;			/* When 0, emulation terminates                */	
	Z80_ICount = 0;			/* T-state count                               */
	Z80_IRQ = Z80_IGNORE_INT;	/* Current IRQ status. Checked after EI occurs */
	// Mailstation OS runs at 12mhz, so divide by 64 for keyboard interrupt rate
	Z80_IPeriod = 187500;		/* Number of T-states per interrupt            */	
		

	// Reset CPU state
	Z80_Reset();
	
	// Used for emulation timing
	int execute_counter = 0;	
	
	// When this != 0, program loop exits
	int exitemu = 0;
	
	DWORD lasttick = SDL_GetTicks();
	DWORD currenttick;
	
	// Display startup message
	powerOff();
	
	while (!exitemu)
	{				
		
		currenttick = SDL_GetTicks();
		
		// Do CPU cycles
		if (!poweroff)
		{
			execute_counter += currenttick - lasttick;
			if (execute_counter > 15) { execute_counter = 0; Z80_Execute(); }
		}
				
		// Update LCD if modified (at 20ms rate)
		if ( (lcd_lastupdate != 0) && (currenttick - lcd_lastupdate >= 20) ) 		
		{ 
			//printf("[%d] UPDATE LCD\n", currenttick); 
			drawLCD(); 
		}
		
		// Write dataflash out if modified		
		if (dataflash_updated)
		{			
			printf("Writing dataflash...\n");
			FILE *df = fopen(dataflash_filename, "wb");
			fwrite(dataflash,512*1024,1,df);
			fclose(df);
			dataflash_updated = 0;
		}
				

		// Check SDL events
		SDL_Event event;
		while (SDL_PollEvent( &event ))
		{
			// Exit if asked
			if( event.type == SDL_QUIT ) exitemu = 1;
			
			// Exit on escape key
			else if( event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE ) exitemu = 1;
			
			// Emulate power button with F12 key
			else if (event.key.keysym.sym == SDLK_F12)
			{				
				if (event.type == SDL_KEYDOWN)
				{					
					power_button = 1;
					if (poweroff)
					{						
						printf("POWER ON\n");	
						
						resetMailstation();
					}
					//else power_button = 1;
				}
				else power_button = 0;
			}
			
			// Toggle fullscreen
			else if (event.key.keysym.sym == SDLK_F10 && event.type == SDL_KEYDOWN)
			{
				if (isfullscreen)
				{
					LCD_2X = 0;
					setVideo(SCREENWIDTH, SCREENHEIGHT, 0);
				}
				else
				{
					LCD_2X = 1;
					setVideo(SCREENWIDTH * 2, SCREENHEIGHT * 2, 1);
				}
				
				// Redraw any screen contents after switch
				if (poweroff) powerOff();
				else drawLCD();
			}
			
			// Toggle screen magnify
			else if (event.key.keysym.sym == SDLK_F9 && event.type == SDL_KEYDOWN)
			{
				// Toggle LCD 2X size
				clearLCD();
				drawLCD();
								
				// Toggle var
				LCD_2X = !LCD_2X;
								
				if (LCD_2X && !isfullscreen) setVideo(SCREENWIDTH * 2, SCREENHEIGHT * 2, 0);
				else if (!LCD_2X && !isfullscreen) setVideo(SCREENWIDTH, SCREENHEIGHT, 0);
												
				// Redisplay intro message if off
				if (poweroff) powerOff();
				// Otherwise trigger an LCD refresh
				else lcd_lastupdate = SDL_GetTicks();				
			}
			
			// Handle other input
			else if ( event.type == SDL_KEYDOWN || event.type == SDL_KEYUP )
			{
				// Process keys pressed while right control held
				if (event.key.keysym.mod & KMOD_RCTRL)
				{									
					if (event.type == SDL_KEYDOWN)
					{
						switch (event.key.keysym.sym)
						{
							case SDLK_r:
								printf("RESETTING...\n");						
								/*if (debugoutfile)
								{
									fclose(debugoutfile);
									debugoutfile = fopen("debugreset.out","wb");
								}*/						
								if (!poweroff) resetMailstation();
								break;							
							
							
							case SDLK_1:
								LCD_color = 1;								
								break;
							
							case SDLK_2:
								LCD_color = 2;								
								break;
							
							case SDLK_3:
								LCD_color = 3;								
								break;

							case SDLK_4:
								LCD_color = 4;								
								break;								

							case SDLK_5:
								LCD_color = 5;								
								break;
						}
					}
				}
								
				// Process keys for keyboard matrix emulation
				else generateKeyboardMatrix(event.key.keysym.sym, event.type);
			}		
		}
		
		
		// Update SDL ticks
		lasttick = currenttick;		
	}	

	
	if (debugoutfile) fclose(debugoutfile);
	
	return 0;
}
