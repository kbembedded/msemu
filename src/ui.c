
#include "ui.h"

#include "config.h"
#include "fonts.h"
#include "images.h"
#include "msemu.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>

// Main window
SDL_Window* window = NULL;
SDL_Renderer* renderer = NULL;
SDL_RWops* stream = NULL;
#define LOGICAL_WIDTH  640
#define LOGICAL_HEIGHT 480

// Splashscreen
extern const uint8_t kongtext_ttf[];
extern const uint16_t kongtext_ttf_size;
SDL_Surface* splashscreen_surface = NULL;
SDL_Texture* splashscreen_tex = NULL;
SDL_Rect splashscreen_srcRect = { 0, 0, 0, 0 };
SDL_Rect splashscreen_dstRect = { 0, 0, 0, 0 };
TTF_Font* font = NULL;
SDL_Color font_color = { 0xff, 0xff, 0x00 }; /* Yellow */
int splashscreen_show = 0;

// LCD
SDL_Surface* lcd_surface = NULL;
SDL_Texture* lcd_tex = NULL;
SDL_Rect lcd_srcRect = { 0, 0, 320, 240 };
SDL_Rect lcd_dstRect = { 0, 0, LOGICAL_WIDTH, LOGICAL_HEIGHT };

// LED
#define UI_LED_IMAGE_SIZE 32
extern const uint8_t led_png[];
extern const uint16_t led_png_size;
SDL_Surface* led_surface = NULL;
SDL_Texture* led_tex = NULL;
SDL_Rect led_srcRect = {0, 0 , UI_LED_IMAGE_SIZE, UI_LED_IMAGE_SIZE};
SDL_Rect led_dstRect = {LOGICAL_WIDTH - 48, LOGICAL_HEIGHT - 96, UI_LED_IMAGE_SIZE, UI_LED_IMAGE_SIZE};

/* XXX: This needs rework still*/
void ui_init(uint32_t* ms_lcd_buffer)
{
	/* Initialize SDL, SDL_IMG, & SDL_TTF */
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		printf("Failed to initialize SDL: %s\n", SDL_GetError());
		abort();
	}

	if (IMG_Init(IMG_INIT_PNG) != IMG_INIT_PNG) {
		printf("Failed to initialize SDL_IMG: %s\n", IMG_GetError());
		abort();
	}

	if (TTF_Init() != 0) {
		printf("Failed to initialize SDL_TTF: %s\n", TTF_GetError());
		abort();
	}

	/* Create/configure window & renderer */
	window = SDL_CreateWindow(
		"MailStation Emulator",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		LOGICAL_WIDTH, LOGICAL_HEIGHT, SDL_WINDOW_RESIZABLE);

	renderer = SDL_CreateRenderer(window, -1, 0);
	SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xff);

	// This allows us to assume the window size is 320x240,
	// but SDL will scale/letterbox it to whatever size the window is.
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
	SDL_RenderSetLogicalSize(renderer, LOGICAL_WIDTH, LOGICAL_HEIGHT);

	/* Load font */
	stream = SDL_RWFromConstMem(kongtext_ttf, kongtext_ttf_size);
	if (!stream) {
		printf("Error creating font stream: %s\n", SDL_GetError());
		abort();
	}

	font = TTF_OpenFontRW(stream, 0, 16);
	if (!font) {
		printf("Failed to load font: %s\n", TTF_GetError());
		abort();
	}

	/* Prepare the splashscreen surface */
	splashscreen_surface = TTF_RenderText_Blended_Wrapped(
		font, SPLASH_STR \
			"F12 to Start", font_color, LOGICAL_WIDTH);
	if (!splashscreen_surface) {
		printf("Error creating splashscreen surface: %s\n", TTF_GetError());
		abort();
	}

	splashscreen_tex = SDL_CreateTextureFromSurface(renderer, splashscreen_surface);
	if (!splashscreen_tex) {
		printf("Error creating splashscreen texture: %s\n", SDL_GetError());
		abort();
	}

	splashscreen_srcRect.w = splashscreen_surface->w;
	splashscreen_srcRect.h = splashscreen_surface->h;
	splashscreen_dstRect.w = splashscreen_surface->w;
	splashscreen_dstRect.h = splashscreen_surface->h;

	/* Prepare the MailStation LCD surface */
	lcd_surface = SDL_CreateRGBSurfaceFrom(ms_lcd_buffer, 320, 240, 32, 1280, 0,0,0,0);
	if (!lcd_surface) {
		printf("Error creating LCD surface: %s\n", SDL_GetError());
		abort();
	}

	lcd_tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, 320, 240);
	if (!lcd_tex) {
		printf("Error creating LCD texture: %s\n", SDL_GetError());
		abort();
	}

	/* Prepare the MailStation LED surface */
	stream = SDL_RWFromConstMem(led_png, led_png_size);
	if (!stream) {
		printf("Error creating LED stream: %s\n", SDL_GetError());
		abort();
	}

	led_surface = IMG_LoadPNG_RW(stream);
	if (!led_surface) {
		printf("Error creating LED surface: %s\n", SDL_GetError());
		abort();
	}

	led_tex = SDL_CreateTextureFromSurface(renderer, led_surface);
	if (!led_tex) {
		printf("Error creating LED texture: %s\n", SDL_GetError());
	}
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

void ui_update_led(uint8_t on)
{
	led_srcRect.x = UI_LED_IMAGE_SIZE * on;
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

	if (!splashscreen_show) {
		// Render LCD
		SDL_RenderCopy(
			renderer, lcd_tex,
			&lcd_srcRect, &lcd_dstRect);

		// Render LED
		SDL_RenderCopy(
			renderer, led_tex,
			&led_srcRect, &led_dstRect);
	}

	// We're rendering back to front, so we always check
	// this last so the splash screen always appears on top.
	if (splashscreen_show) {
		SDL_RenderCopy(
			renderer, splashscreen_tex,
			&splashscreen_srcRect, &splashscreen_dstRect);
	}

	SDL_RenderPresent(renderer);
}
