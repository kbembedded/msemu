
#include "ui.h"

#include "config.h"
#include "fonts.h"
#include "images.h"
#include "io.h"
#include "msemu.h"
#include <stdio.h>
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
SDL_Surface* splashscreen_surface = NULL;
SDL_Texture* splashscreen_tex = NULL;
SDL_Rect splashscreen_srcRect = { 0, 0, 320, 128 };
SDL_Rect splashscreen_dstRect = { 0, 112, LOGICAL_WIDTH, 256 };
int splashscreen_show = 0;

// Version
SDL_Surface* version_surface = NULL;
SDL_Texture* version_tex = NULL;
SDL_Rect version_srcRect = { 0, 0, 72, 24 };
SDL_Rect version_dstRect = { LOGICAL_WIDTH - 80, LOGICAL_HEIGHT - 142, 72, 24 };
TTF_Font* font = NULL;
SDL_Color font_color = { 0x9d, 0xe0, 0x8c };

// LCD
SDL_Surface* lcd_surface = NULL;
SDL_Texture* lcd_tex = NULL;
SDL_Rect lcd_srcRect = { 0, 0, 320, 240 };
SDL_Rect lcd_dstRect = { 0, 0, LOGICAL_WIDTH, LOGICAL_HEIGHT };

// LED
#define UI_LED_IMAGE_SIZE 32
SDL_Surface* led_surface = NULL;
SDL_Texture* led_tex = NULL;
SDL_Rect led_srcRect = {0, 0 , UI_LED_IMAGE_SIZE, UI_LED_IMAGE_SIZE};
SDL_Rect led_dstRect = {LOGICAL_WIDTH - 48, LOGICAL_HEIGHT - 96, UI_LED_IMAGE_SIZE, UI_LED_IMAGE_SIZE};

// AC
#define UI_AC_IMAGE_SIZE 32
SDL_Surface* ac_surface = NULL;
SDL_Texture* ac_tex = NULL;
SDL_Rect ac_srcRect = {0, 0 , UI_AC_IMAGE_SIZE, UI_AC_IMAGE_SIZE};
SDL_Rect ac_dstRect = {LOGICAL_WIDTH - 80, 72, UI_AC_IMAGE_SIZE, UI_AC_IMAGE_SIZE};

// Battery
#define UI_BATTERY_IMAGE_SIZE 32
SDL_Surface* battery_surface = NULL;
SDL_Texture* battery_tex = NULL;
SDL_Rect battery_srcRect = {0, 0 , UI_BATTERY_IMAGE_SIZE, UI_BATTERY_IMAGE_SIZE};
SDL_Rect battery_dstRect = {LOGICAL_WIDTH - 48, 72, UI_BATTERY_IMAGE_SIZE, UI_BATTERY_IMAGE_SIZE};

// Keyboard
// This table translates PC scancodes to the Mailstation key matrix
static int32_t sdl_to_ms_kbd_LUT[10][8] = {
	{ SDLK_HOME, SDLK_END, SDLK_INSERT, SDLK_F1, SDLK_F2, SDLK_F3, SDLK_F4, SDLK_F5 },
	{ 0, 0, 0, SDLK_F6, SDLK_F7, SDLK_F8, SDLK_F9, SDLK_PAGEUP },
	{ SDLK_BACKQUOTE, SDLK_1, SDLK_2, SDLK_3, SDLK_4, SDLK_5, SDLK_6, SDLK_7 },
	{ SDLK_8, SDLK_9, SDLK_0, SDLK_MINUS, SDLK_EQUALS, SDLK_BACKSPACE, SDLK_BACKSLASH, SDLK_PAGEDOWN },
	{ SDLK_TAB, SDLK_q, SDLK_w, SDLK_e, SDLK_r, SDLK_t, SDLK_y, SDLK_u },
	{ SDLK_i, SDLK_o, SDLK_p, SDLK_LEFTBRACKET, SDLK_RIGHTBRACKET, SDLK_SEMICOLON, SDLK_QUOTE, SDLK_RETURN },
	{ SDLK_CAPSLOCK, SDLK_a, SDLK_s, SDLK_d, SDLK_f, SDLK_g, SDLK_h, SDLK_j },
	{ SDLK_k, SDLK_l, SDLK_COMMA, SDLK_PERIOD, SDLK_SLASH, SDLK_UP, SDLK_DOWN, SDLK_RIGHT },
	{ SDLK_LSHIFT, SDLK_z, SDLK_x, SDLK_c, SDLK_v, SDLK_b, SDLK_n, SDLK_m },
	{ SDLK_LCTRL, 0, 0, SDLK_SPACE, 0, 0, SDLK_RSHIFT, SDLK_LEFT }
};

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
		"MailStation EMUlator",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		LOGICAL_WIDTH, LOGICAL_HEIGHT, SDL_WINDOW_RESIZABLE);

	renderer = SDL_CreateRenderer(window, -1, 0);
	SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xff);

	// This allows us to assume the window size is 320x240,
	// but SDL will scale/letterbox it to whatever size the window is.
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
	SDL_RenderSetLogicalSize(renderer, LOGICAL_WIDTH, LOGICAL_HEIGHT);

	/* Prepare the Splashscreen surface */
	stream = SDL_RWFromConstMem(splash_png, splash_png_size);
	if (!stream) {
		printf("Error creating Splashscreen stream: %s\n", SDL_GetError());
		abort();
	}

	splashscreen_surface = IMG_LoadPNG_RW(stream);
	if (!splashscreen_surface) {
		printf("Error creating Splashscreen surface: %s\n", SDL_GetError());
		abort();
	}

	splashscreen_tex = SDL_CreateTextureFromSurface(renderer, splashscreen_surface);
		if (!splashscreen_tex) {
		printf("Error creating Splashscreen texture: %s\n", SDL_GetError());
		abort();
	}

	/* Prepare the Version surface */
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

	version_surface = TTF_RenderText_Blended_Wrapped(
		font, VERSION_STR, font_color, LOGICAL_WIDTH);
	if (!version_surface) {
		printf("Error creating Version surface: %s\n", TTF_GetError());
		abort();
	}

	version_tex = SDL_CreateTextureFromSurface(renderer, version_surface);
	if (!version_tex) {
		printf("Error creating Version texture: %s\n", SDL_GetError());
		abort();
	}

	/* Prepare the MailStation LCD surface */
	lcd_surface = SDL_CreateRGBSurfaceFrom(ms_lcd_buffer, 320, 240, 32, 1280, 0, 0, 0, 0);
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

	/* Prepare the MailStation AC surface */
	stream = SDL_RWFromConstMem(ac_png, ac_png_size);
	if (!stream) {
		printf("Error creating AC stream: %s\n", SDL_GetError());
		abort();
	}

	ac_surface = IMG_LoadPNG_RW(stream);
	if (!ac_surface) {
		printf("Error creating AC surface: %s\n", SDL_GetError());
		abort();
	}

	ac_tex = SDL_CreateTextureFromSurface(renderer, ac_surface);
	if (!ac_tex) {
		printf("Error creating AC texture: %s\n", SDL_GetError());
	}

	/* Prepare the MailStation Battery surface */
	stream = SDL_RWFromConstMem(battery_png, battery_png_size);
	if (!stream) {
		printf("Error creating Battery stream: %s\n", SDL_GetError());
		abort();
	}

	battery_surface = IMG_LoadPNG_RW(stream);
	if (!battery_surface) {
		printf("Error creating Battery surface: %s\n", SDL_GetError());
		abort();
	}

	battery_tex = SDL_CreateTextureFromSurface(renderer, battery_surface);
	if (!battery_tex) {
		printf("Error creating Battery texture: %s\n", SDL_GetError());
	}
}

void ui_splashscreen_show()
{
	splashscreen_show = 1;
}

void ui_splashscreen_hide()
{
	splashscreen_show = 0;
}

void ui_update_led(uint8_t on)
{
	led_srcRect.x = UI_LED_IMAGE_SIZE * on;
}

void ui_update_ac(uint8_t on){
	ac_srcRect.x = UI_AC_IMAGE_SIZE * on;
}

void ui_update_battery(int status)
{
	battery_srcRect.x = UI_BATTERY_IMAGE_SIZE * status;
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

	if (splashscreen_show) {
		// Render Splashscreen
		SDL_RenderCopy(
			renderer, splashscreen_tex,
			&splashscreen_srcRect, &splashscreen_dstRect);
		SDL_RenderCopy(
			renderer, version_tex,
			&version_srcRect, &version_dstRect);
	} else {
		// Render LCD
		SDL_RenderCopy(
			renderer, lcd_tex,
			&lcd_srcRect, &lcd_dstRect);
	}

	// Render LED
	SDL_RenderCopy(
		renderer, led_tex,
		&led_srcRect, &led_dstRect);

		// Render AC
	SDL_RenderCopy(
		renderer, ac_tex,
		&ac_srcRect, &ac_dstRect);

		// Render Battery
	SDL_RenderCopy(
		renderer, battery_tex,
		&battery_srcRect, &battery_dstRect);

	SDL_RenderPresent(renderer);
}

/* Translate real input keys to MS keyboard matrix
 *
 * The lookup matrix is currently [10][8], we just walk the whole thing like
 * one long buffer and do the math later to figure out exactly what bit was
 * pressed.
 *
 * TODO: Would it make sense to rework keyTranslateTable to a single buffer
 * anyway? Right now the declaration looks crowded and would need some rework
 * already.
 */
static void ui_set_ms_kbd(ms_ctx* ms, int scancode, int eventtype)
{
	uint32_t i = 0;
	int32_t *keytbl_ptr = &sdl_to_ms_kbd_LUT[0][0];

	for (i = 0; i < (sizeof(sdl_to_ms_kbd_LUT)/sizeof(int32_t)); i++) {
		if (scancode == *(keytbl_ptr + i)) {
			/* Couldn't avoid the magic numbers below. As noted,
			 * kTT array is [10][8], directly mapping the MS matrix
			 * of 10 bytes to represent the whole keyboard. Divide
			 * by 8 to get the uint8_t the scancode falls in, and mod
			 * 8 to get the bit in that uint8_t that matches the code.
			 */
			if (eventtype == SDL_KEYDOWN) {
				ms->key_matrix[i/8] &= ~((uint8_t)1 << (i%8));
				break;
			} else {
				ms->key_matrix[i/8] |= ((uint8_t)1 << (i%8));
				break;
			}
		}

	}
}

int ui_kbd_process(ms_ctx *ms)
{
	static uint8_t val;

	SDL_Event event;
	// Check SDL events
	while (SDL_PollEvent(&event))
	{
		/* Exit if SDL quits, or Escape key was pushed */
		if ((event.type == SDL_QUIT) ||
		  ((event.type == SDL_KEYDOWN) &&
			(event.key.keysym.sym == SDLK_ESCAPE))) {
			return 1;
		}


		/* Handle other input events */
		if ((event.type == SDL_KEYDOWN) ||
		  (event.type == SDL_KEYUP)) {

			/* First, check to see if F12 was pressed */
			if (event.key.keysym.sym == SDLK_F12) {
				if (event.type == SDL_KEYDOWN) {
					ms->power_button_n = 0;
				} else if (event.type == SDL_KEYUP) {
					ms->power_button_n = 1;
				}
				ms_power_hint(ms);
			}
			/* Keys pressed while right ctrl is held */
			if (event.key.keysym.mod & KMOD_RCTRL) {
				if (event.type == SDL_KEYDOWN) {
					switch (event.key.keysym.sym) {
					  /* Reset whole system */
					  case SDLK_r:
						ms_power_on_reset(ms);
						break;
					  case SDLK_a:
						ms_power_ac_set_status(ms, AC_TOGGLE);
						break;
					  case SDLK_b:
						ms_power_batt_set_status(ms, BATT_CYCLE);
						break;
					  case SDLK_p:
						ms_ioctl(ms, 1, &val);
						break;
					  default:
						printf("val is currently 0x%x\n", val);
						break;
					}
				}
			} else {
				/* Proces the key for the MS */
				ui_set_ms_kbd(ms, event.key.keysym.sym, event.type);
			}
		}
	}

	return 0;
}
