#ifndef __LCD_H__
#define __LCD_H__

#include <stdint.h>
#include "msemu.h"


//----------------------------------------------------------------------------
//
//  Emulates writing to Mailstation LCD device
//
void lcd_write(uint8_t *onebit, uint32_t *RGBA8888, int *lcd_cas, uint8_t *io_buf, uint16_t newaddr, uint8_t val, int lcdnum);

//----------------------------------------------------------------------------
//
//  Emulates reading from Mailstation LCD
//
uint8_t lcd_read(uint8_t *onebit, uint32_t *RGBA8888, int *lcd_cas, uint8_t *io_buf, uint16_t newaddr, int lcdnum);

int lcd_init(uint8_t **onebit, uint32_t **RGBA8888, int *lcd_cas);
//uint8_t **lcd?

int lcd_deinit(ms_ctx *ms);

#endif // __LCD_H__
