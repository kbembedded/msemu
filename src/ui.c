
#include "ui.h"

#include "msemu.h"
#include "rawcga.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL2_rotozoom.h>

// Main window
SDL_Window* window;
SDL_Renderer* renderer;

// Splashscreen
SDL_Surface* splashscreen_surface = NULL;
SDL_Texture* splashscreen_tex = NULL;
SDL_Rect splashscreen_srcRect = { 0, 0, 320, 240 };
SDL_Rect splashscreen_dstRect = { 0, 0, 320, 240 };
int splashscreen_show = 0;

// LCD
SDL_Surface* lcd_surface = NULL;
SDL_Texture* lcd_tex = NULL;
SDL_Rect lcd_srcRect = { 0, 0, 320, 240 };
SDL_Rect lcd_dstRect = { 0, 0, 320, 240 };

SDL_Color fontcolors[2] = {
	{ 0x00, 0x00, 0x00 }, /* Black */
	{ 0xff, 0xff, 0x00 }, /* Yellow */
};

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
		printf("Error blitting text: %s\n", SDL_GetError());
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
void ui_init(uint32_t* ms_lcd_buffer)
{
	SDL_Surface *cgafont_tmp = NULL;

	SDL_Palette* fontPalette = NULL;

	SDL_Init( SDL_INIT_VIDEO );

	window = SDL_CreateWindow(
		"MailStation Emulator",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		320, 240, SDL_WINDOW_RESIZABLE);

	renderer = SDL_CreateRenderer(window, -1, 0);

	// This allows us to assume the window size is 320x240,
	// but SDL will scale/letterbox it to whatever size the window is.
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	SDL_RenderSetLogicalSize(renderer, 320, 240);

	/* Set up color palettes */
	fontPalette = SDL_AllocPalette(2);
	if (SDL_SetPaletteColors(fontPalette, fontcolors, 0, 2) != 0) {
		printf("Failed to set font palette colors: %s\n", SDL_GetError());
	}

	/* Prepare the font surface */
	/* XXX: This color set up is really strange, fontcolors sets up yellow
	 * on black font. However, it seems to be linked with the colors
	 * array above. If one were to remove colors[5], that causes the
	 * font color to cange. I hope that moving to a newer SDL version
	 * will improve this.
	 */
	cgafont_tmp = SDL_CreateRGBSurfaceFrom(raw_cga_array,
	  8, 2048, 1, 1,  0,0,0,0);
	if (cgafont_tmp == NULL) {
		printf("Error creating font surface\n");
	}

	if (SDL_SetPaletteColors(cgafont_tmp->format->palette, fontcolors, 0, 2) != 0) {
		printf("Error setting palette on font\n");
	}

	// Convert the 1-bit font surface to match the display
	cgafont_surface = SDL_ConvertSurfaceFormat(cgafont_tmp, SDL_GetWindowPixelFormat(window), 0);
	// Free the 1-bit version
	SDL_FreeSurface(cgafont_tmp);

	/* Prepare the MailStation LCD surface */
	lcd_surface = SDL_CreateRGBSurfaceFrom(ms_lcd_buffer, 320, 240, 32, 1280, 0,0,0,0);
	if (!lcd_surface) {
		printf("Error creating LCD surface: %s\n", SDL_GetError());
	}
	lcd_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 320, 240);

	/* Set up splash screen */
	splashscreen_surface = SDL_CreateRGBSurface(0, 320, 240, 8, 0, 0, 0, 0);
	if (!splashscreen_surface) {
		printf("Error creating splashscreen surface: %s\n", SDL_GetError());
	}

	// SDL_FillRect(splashscreen_surface, &splashscreen_srcRect, SDL_MapRGB(splashscreen_surface->format, 0xFF, 0x00, 0x00));
	printstring_centered(splashscreen_surface, "Mailstation Emulator", 4 * 8);
	printstring_centered(splashscreen_surface, "v0.2", 5 * 8);
	printstring_centered(splashscreen_surface, "F12 to Start", 15 * 8);

	splashscreen_tex = SDL_CreateTextureFromSurface(renderer, splashscreen_surface);
}

void ui_splashscreen_show()
{
	splashscreen_show = 1;
}

void ui_splashscreen_hide()
{
	splashscreen_show = 0;
}

void ui_update_lcd()
{
	if (SDL_UpdateTexture(lcd_tex, &lcd_srcRect, lcd_surface->pixels, lcd_surface->pitch) != 0)  {
		printf("Failed to update LCD: %s\n", SDL_GetError());
	}
}

void ui_render()
{
	SDL_RenderClear(renderer);

	// Render LCD
	SDL_RenderCopy(
		renderer, lcd_tex,
		&lcd_srcRect, &lcd_dstRect);

	// We're rendering back to front, so we always check
	// this last so the splash screen always appears on top.
	if (splashscreen_show) {
		SDL_RenderCopy(
			renderer, splashscreen_tex,
			&splashscreen_srcRect, &splashscreen_dstRect);
	}

	SDL_RenderPresent(renderer);
}
