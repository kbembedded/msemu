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
int lcd_write(ms_ctx *ms, uint16_t newaddr, uint8_t val, int lcdnum)
{
	uint8_t *lcd_ptr;
	int x;
	int n;
	int idx;

	lcd_ptr = ms->lcd_dat1bit;
	/* XXX: Magic number that points to where the start of the LCD_R half
	 * is in the buffer */
	if (lcdnum == LCD_R) lcd_ptr += 4800;

	/* XXX: This might need to be reworked to use non-viewable LCD memory */
	// Wraps memory address if out of bounds
	if (newaddr >= 240) newaddr %= 240;

	// Check CAS bit on P2
	if (io_read(ms, MISC2) & 8) {
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
		x = 19 - ms->lcd_cas;
		// Use right half if necessary
		if (lcdnum == LCD_R) x += 20;

		// Write out all 8 bits to separate bytes, using the current emulated LCD color
		for (n = 0; n < 8; n++) {
			idx = n + (x * 8) + (newaddr * 320);
			ms->lcd_datRGBA8888[idx] = ((val >> n) & 1 ? UI_LCD_PIXEL_ON : UI_LCD_PIXEL_OFF);
		}

	} else {
		log_debug(" * LCD%s W [ CAS] <- %02X\n",
		  lcdnum == LCD_L ? "_L" : "_R", newaddr, val);

		// If CAS line is low, set current column instead
		ms->lcd_cas = val;
	}

	return 0;
}

//----------------------------------------------------------------------------
//
//  Emulates reading from Mailstation LCD
//
uint8_t lcd_read(ms_ctx *ms, uint16_t newaddr, int lcdnum)
{
	uint8_t *lcd_ptr;
	uint8_t ret;

	lcd_ptr = ms->lcd_dat1bit;
	/* XXX: Magic number that points to where the start of the LCD_R half
	 * is in the buffer */
	if (lcdnum == LCD_R) lcd_ptr += 4800;

	/* XXX: This might need to be reworked to use non-viewable LCD memory */
	// Wraps memory address if out of bounds
	if (newaddr >= 240) newaddr %= 240;

	// Check CAS bit on P2
	if (io_read(ms, MISC2) & 8)
	{
		// Return data on currently selected LCD column
		ret = lcd_ptr[newaddr + (ms->lcd_cas * 240)];
	} else {
		// Not sure what this normally returns when CAS bit low!
		// XXX: TEST THIS
		ret = ms->lcd_cas;
	}

	log_debug(" * LCD%s R [%04X] -> %02X\n",
	  lcdnum == LCD_L ? "_L" : "_R", newaddr, ret);

	return ret;
}

int lcd_init(ms_ctx *ms)
{
	if (ms->lcd_dat1bit == NULL) {
		ms->lcd_dat1bit = (uint8_t *)calloc(
			((MS_LCD_WIDTH * MS_LCD_HEIGHT) / 8), sizeof(uint8_t));
		if (ms->lcd_dat1bit == NULL) {
			printf("Unable to allocate 1-bit LCD buffer\n");
			exit(EXIT_FAILURE);
		}
	} else {
		memset(ms->lcd_dat1bit, '\0',
			(MS_LCD_WIDTH * MS_LCD_HEIGHT) / 8);
	}

	if (ms->lcd_datRGBA8888 == NULL) {
		ms->lcd_datRGBA8888 = (uint32_t *)calloc(
			MS_LCD_WIDTH * MS_LCD_HEIGHT, sizeof(uint32_t));
		if (ms->lcd_datRGBA8888 == NULL) {
			printf("Unable to allocate RGB LCD buffer\n");
			exit(EXIT_FAILURE);
		}
	} else {
		memset(ms->lcd_datRGBA8888, '\0',
			(MS_LCD_WIDTH * MS_LCD_HEIGHT) * sizeof(uint32_t));
	}

	ms->lcd_cas = 0;

	return MS_OK;
}

int lcd_deinit(ms_ctx *ms)
{
	free(ms->lcd_dat1bit);
	free(ms->lcd_datRGBA8888);
	ms->lcd_dat1bit = NULL;
	ms->lcd_datRGBA8888 = NULL;

	return MS_OK;
}

int lcd_copy(ms_ctx *dest, ms_ctx *src)
{
	assert(dest->lcd_dat1bit != NULL);
	assert(dest->lcd_datRGBA8888 != NULL);
	assert(src->lcd_dat1bit != NULL);
	assert(src->lcd_datRGBA8888 != NULL);

	memcpy(dest->lcd_dat1bit, src->lcd_dat1bit,
		((MS_LCD_WIDTH * MS_LCD_HEIGHT) / 8));
	memcpy(dest->lcd_datRGBA8888, src->lcd_datRGBA8888,
		(MS_LCD_WIDTH * MS_LCD_HEIGHT));

	return MS_OK;
}
