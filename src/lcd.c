#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "io.h"
#include "msemu.h"
#include "ui.h"

// Default screen size
#define MS_LCD_WIDTH    320
#define MS_LCD_HEIGHT   240

//----------------------------------------------------------------------------
//
//  Emulates writing to Mailstation LCD device
//
int lcd_write(uint8_t *onebit, uint32_t *RGBA8888, int *lcd_cas, uint8_t *io_buf, uint16_t newaddr, uint8_t val, int lcdnum)
{
	uint8_t *lcd_ptr;

	if (lcdnum == LCD_L) lcd_ptr = onebit;
	/* XXX: Fix the use of this magic number, replace with a const */
	else lcd_ptr = &onebit[4800];

	/* XXX: This might need to be reworked to use non-viewable LCD memory */
	// Wraps memory address if out of bounds
	if (newaddr >= 240) newaddr %= 240;

	// Check CAS bit on P2
	if (io_read(io_buf, MISC2) & 8)
	{
		log_debug(" * LCD%s W [%04X] <- %02X\n",
		  lcdnum == LCD_L ? "_L" : "_R", newaddr, val);

		// Write data to currently selected LCD column.
		// This is just used for reading back LCD contents to the Mailstation quickly.
		lcd_ptr[newaddr + (*lcd_cas * 240)] = val;


		/*
			Write directly to newer 8-bit lcd_data8 buffer now too.
			This is what will actually be drawn on the emulator screen now.
		*/

		// Reverse column # (MS col #0 starts on right side)
		int x = 19 - *lcd_cas;
		// Use right half if necessary
		if (lcdnum == LCD_R) x += 20;

		// Write out all 8 bits to separate bytes, using the current emulated LCD color
		int n;
		for (n = 0; n < 8; n++)
		{
			int idx = n + (x * 8) + (newaddr * 320);
			RGBA8888[idx] = ((val >> n) & 1 ? UI_LCD_PIXEL_ON : UI_LCD_PIXEL_OFF);
		}

	} else {
		log_debug(" * LCD%s W [ CAS] <- %02X\n",
		  lcdnum == LCD_L ? "_L" : "_R", newaddr, val);

		// If CAS line is low, set current column instead
		*lcd_cas = val;
	}

	return 0;
}

//----------------------------------------------------------------------------
//
//  Emulates reading from Mailstation LCD
//
uint8_t lcd_read(uint8_t *onebit, uint32_t *RGBA8888, int *lcd_cas, uint8_t *io_buf, uint16_t newaddr, int lcdnum)
{
	uint8_t *lcd_ptr;
	uint8_t ret;

	if (lcdnum == LCD_L) lcd_ptr = onebit;
	/* XXX: Fix the use of this magic number, replace with a const */
	else lcd_ptr = &onebit[4800];

	/* XXX: This might need to be reworked to use non-viewable LCD memory */
	// Wraps memory address if out of bounds
	if (newaddr >= 240) newaddr %= 240;

	// Check CAS bit on P2
	if (io_read(io_buf, MISC2) & 8)
	{
		// Return data on currently selected LCD column
		ret = lcd_ptr[newaddr + (*lcd_cas * 240)];
	} else {
		// Not sure what this normally returns when CAS bit low!
		ret = *lcd_cas;
	}

	log_debug(" * LCD%s R [%04X] -> %02X\n",
	  lcdnum == LCD_L ? "_L" : "_R", newaddr, ret);

	return ret;
}

int lcd_init(uint8_t **onebit, uint32_t **RGBA8888, int *lcd_cas)
{
	if (*onebit == NULL) {
		*onebit = (uint8_t *)calloc(((MS_LCD_WIDTH * MS_LCD_HEIGHT) / 8),
		  sizeof(uint8_t));
		if (*onebit == NULL) {
			printf("Unable to allocate 1-bit LCD buffer\n");
			exit(EXIT_FAILURE);
		}
	} else {
		memset(*onebit, '\0', (MS_LCD_WIDTH * MS_LCD_HEIGHT) / 8);
	}

	if (*RGBA8888 == NULL) {
		*RGBA8888 = (uint32_t *)calloc(  MS_LCD_WIDTH * MS_LCD_HEIGHT,
		  sizeof(uint32_t));
		if (*RGBA8888 == NULL) {
			printf("Unable to allocate RGB LCD buffer\n");
			exit(EXIT_FAILURE);
		}
	} else {
		memset(*RGBA8888, '\0', (MS_LCD_WIDTH * MS_LCD_HEIGHT) * sizeof(uint32_t));
	}

	*lcd_cas = 0;

	return MS_OK;
}
