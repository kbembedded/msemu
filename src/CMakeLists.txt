add_executable(msemu
	debug.c
	flashops.c
	main.c
	msemu.c
	ui.c
)

target_link_libraries(msemu
	SDL
	SDLmain
	SDL_gfx
	z80ex
	z80ex_dasm
)

target_compile_definitions(msemu PUBLIC
	LSB_FIRST
	X86_ASM)
