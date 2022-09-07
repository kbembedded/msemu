#ifndef __LCD_H__
#define __LCD_H__

#include <stdint.h>
#include "msemu.h"


//----------------------------------------------------------------------------
//
//  Emulates writing to Mailstation LCD device
//
void lcd_write(ms_ctx *ms, uint16_t newaddr, uint8_t val, int lcdnum);

//----------------------------------------------------------------------------
//
//  Emulates reading from Mailstation LCD
//
uint8_t lcd_read(ms_ctx *ms, uint16_t newaddr, int lcdnum);

int lcd_init(ms_ctx *ms);

int lcd_deinit(ms_ctx *ms);

#endif // __LCD_H__
