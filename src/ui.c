
#include "ui.h"

#include "msemu.h"
#include "rawcga.h"
#include <SDL/SDL.h>
#include <SDL/SDL_rotozoom.h>

// Primary SDL screen surface
SDL_Surface* screen;

// Master palette
SDL_Color colors[6];

// Surface for the Mailstation LCD
SDL_Surface *lcd_surface;

// Surface to load CGA font data, for printing text with SDL
SDL_Surface *cgafont_surface = NULL;

// Cursor position for unfinished code to print text in SDL
int cursorX = 0;
int cursorY = 0;

//----------------------------------------------------------------------------
//
//  Graphically prints a character to the SDL screen
//
void printcharXY(SDL_Surface* surface, char mychar, int x, int y)
{
	// CGA font characters are 8x8
	SDL_Rect charoutarea;
	charoutarea.x = x;
	charoutarea.y = y;
	charoutarea.h = 8;
	charoutarea.w = 8;

	SDL_Rect letterarea;
	letterarea.x = 0;
	letterarea.y = 8 * mychar;
	letterarea.w = 8;
	letterarea.h = 8;

	//SDL_GetVideoSurface()
	if (SDL_BlitSurface(cgafont_surface, &letterarea, surface, &charoutarea) != 0) {
		printf("Error blitting text\n");
	}
}

//----------------------------------------------------------------------------
//
//  Graphically prints a string at the specified X/Y coords
//
void printstringXY(SDL_Surface* surface, char *mystring, int x, int y)
{
	while (*mystring)
	{
		printcharXY(surface, *mystring, x, y);
		mystring++;
		x += 8; // CGA font characters are 8x8

		//Move to the next line if necessary
		if (x > surface->w) {
			x = 0;
			y += 8;
		}
	}
}

//----------------------------------------------------------------------------
//
//  Graphically prints a string centered at the specified Y coordinate
//
void printstring_centered(SDL_Surface* surface, char *mystring, int y)
{
	int surface_cols = surface->w / 8;
	int x = (surface_cols - strlen(mystring)) / 2;
	printstringXY(surface, mystring, x * 8, y);
}

//----------------------------------------------------------------------------
//
//  Graphically prints a string at the current cursor position
//
void printstring(SDL_Surface* surface, char *mystring)
{
	while (*mystring)
	{
		if (*mystring == '\n') {
			cursorX = 0;
			cursorY++;
			mystring++;
			continue;
		}

		printcharXY(surface, *mystring, cursorX * 8, cursorY * 8);
		mystring++;
		cursorX++;

		if (cursorX * 8 >= surface->w) {
			cursorX = 0;
			cursorY++;
		}

		if (cursorY * 8 >= surface->h) {
			cursorY = 0;
		}
	}
}

/* XXX: This needs rework still*/
void ui_init(uint8_t* ms_lcd_buffer)
{
	SDL_Surface *cgafont_tmp = NULL;
	SDL_Color fontcolors[2];

	SDL_Init( SDL_INIT_VIDEO );

	/* Set up colors to be used by the LCD display from the Mailstation */
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
	screen = SDL_SetVideoMode(MS_LCD_WIDTH * 2, MS_LCD_HEIGHT * 2, 8, SDL_HWPALETTE);
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
	cgafont_tmp = SDL_CreateRGBSurfaceFrom(raw_cga_array,
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
	lcd_surface = SDL_CreateRGBSurfaceFrom(ms_lcd_buffer, 320, 240, 8, 320, 0,0,0,0);
	if (lcd_surface == NULL) {
		printf("Error creating LCD surface\n");
		//return 1;
	}

	// Set palette for LCD to global palette
	if (SDL_SetPalette(lcd_surface, SDL_LOGPAL|SDL_PHYSPAL, colors, 0, 6) != 1) {
		printf("Error setting palette on LCD\n");
	}
}

// TODO: This is no longer zoomed 2x like the LCD because the printstring
// functions are using the correct SDL surface.
// Need to find a better way to handle screen scaling as a final step before
// rendering the final surface instead of scaling each surface separately.
void ui_drawSplashScreen()
{
	SDL_Rect bg;
	bg.x = 0;
	bg.y = 0;
	bg.w = screen->w;
	bg.h = screen->h;

	if (SDL_FillRect(screen, &bg, 0x00000000)) {
		printf("Error drawing splashscreen background.\n");
	}

	printstring_centered(screen, "Mailstation Emulator", 4 * 8);
	printstring_centered(screen, "v0.2", 5 * 8);
	printstring_centered(screen, "F12 to Start", 15 * 8);

	SDL_Flip(screen);
}

void ui_drawLCD()
{
	SDL_Surface *lcd_surface2x = NULL;

	// Setup output rect to fill screen for now
	SDL_Rect outrect;
	outrect.x = 0;
	outrect.y = 0;
	outrect.w = MS_LCD_WIDTH;
	outrect.h = MS_LCD_HEIGHT;

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
}
