
#include "ui.h"

#include "msemu.h"
#include "rawcga.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

// Main window
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
TTF_Font* font = NULL;
SDL_Color font_color = { 0xff, 0xff, 0x00 }; /* Yellow */

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
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		printf("Failed to initialize SDL: %s\n", SDL_GetError());
	}

	if (TTF_Init() != 0) {
		printf("Failed to initialize SDL_TTF: %s\n", TTF_GetError());
	}

	window = SDL_CreateWindow(
		"MailStation Emulator",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		320, 240, SDL_WINDOW_RESIZABLE);

	renderer = SDL_CreateRenderer(window, -1, 0);
	SDL_SetRenderDrawColor(renderer, 0xff, 0xff, 0xff, 0xff);

	// This allows us to assume the window size is 320x240,
	// but SDL will scale/letterbox it to whatever size the window is.
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	SDL_RenderSetLogicalSize(renderer, 320, 240);

	/* Load font */
	font = TTF_OpenFont("fonts/kongtext.ttf", 8);
	if (!font) {
		printf("Failed to load font: %s\n", TTF_GetError());
	}

	/* Prepare the MailStation LCD surface */
	lcd_surface = SDL_CreateRGBSurfaceFrom(ms_lcd_buffer, 320, 240, 32, 1280, 0,0,0,0);
	if (!lcd_surface) {
		printf("Error creating LCD surface: %s\n", SDL_GetError());
	}
	lcd_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 320, 240);

	/* Set up splash screen */
	// printstring_centered(splashscreen_surface, "Mailstation Emulator", 4 * 8);
	// printstring_centered(splashscreen_surface, "v0.2", 5 * 8);
	// printstring_centered(splashscreen_surface, "F12 to Start", 15 * 8);

}

void ui_splashscreen_show()
{
	printf("Splashscreen ON\n");
	splashscreen_show = 1;
}

void ui_splashscreen_hide()
{
	printf("Splashscreen OFF\n");
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
