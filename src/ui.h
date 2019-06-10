#ifndef _UI_H_
#define _UI_H_

#include <stdint.h>

/**
 * Initializes the user interface.
 * 
 * This interface is generic to avoid polluting the emulator code
 * with SDL code.
 * 
 * \param raw_cga_array - pointer to the cga font array
 * \param lcd_buffer    - pointer to the Mailstation 8-bit LCD buffer
 */
void ui_init(char* raw_cga_array, uint8_t* lcd_buffer);

/**
 * Draws the splash screen.
 */
void ui_drawSplashScreen();

/**
 * Draws the Mailstation LCD to the UI.
 */
void ui_drawLCD();

#endif // _UI_H_