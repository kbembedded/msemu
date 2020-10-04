add_executable(msemu
	debug.c
	flashops.c
	main.c
	msemu.c
	ui.c
)

target_link_libraries(msemu
	SDL2
	SDL2main
	SDL2_ttf
	z80ex
	z80ex_dasm
)

file(COPY ${CMAKE_SOURCE_DIR}/src/fonts DESTINATION ${CMAKE_BINARY_DIR})
