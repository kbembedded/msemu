#ifndef _UI_H_
#define _UI_H_

#include <stdint.h>

// RGBA8888 colors
#define UI_COLOR_BLACK     (0x00000000)
#define UI_COLOR_GREEN     (0x00ff0000)
#define UI_COLOR_DIM_GREEN (0x8ce09d00)
#define UI_COLOR_DARK_GREY (0x14212600)
#define UI_COLOR_BLUE      (0x0000ff00)
#define UI_COLOR_YELLOW    (0x00ffff00)


// These are the colors used for the LCD screen
#define UI_LCD_PIXEL_ON  UI_COLOR_DARK_GREY
#define UI_LCD_PIXEL_OFF UI_COLOR_DIM_GREEN

/**
 * Initializes the user interface.
 *
 * This interface is generic to avoid polluting the emulator code
 * with SDL code.
 *
 * \param lcd_buffer    - pointer to the Mailstation 8-bit LCD buffer
 */
void ui_init(uint32_t* lcd_buffer);

/**
 * Shows the splash screen.
 */
void ui_splashscreen_show();

/**
 * Hides the splash screen.
 */
void ui_splashscreen_hide();

/**
 * Tells the UI to update the LCD texture
 * from the LCD buffer.
 */
void ui_update_lcd();

/**
 * Renders the UI
 */
void ui_render();

#endif // _UI_H_
