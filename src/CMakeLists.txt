add_executable(msemu
	debug.c
	flashops.c
	main.c
	msemu.c
	io.c
	ui.c
)

target_link_libraries(msemu
	SDL2
	SDL2main
	SDL2_image
	SDL2_ttf
	z80ex
	z80ex_dasm
)

function(create_resources dir output)
	# Create empty output file
	file(WRITE ${output} "")
	# Collect input files
	file(GLOB bins ${dir}/*)
	# Iterate through input files
	foreach(bin ${bins})
		# Get short filename
		string(REGEX MATCH "([^/]+)$" filename ${bin})
		# Replace filename spaces & extension separator for C compatibility
		string(REGEX REPLACE "\\.| |-" "_" filename ${filename})
		# Read hex data from file
		file(READ ${bin} filedata HEX)
		# Convert hex data for C compatibility
		string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," filedata ${filedata})
		# Append data to output file
		file(APPEND ${output} "const uint8_t ${filename}[] = {${filedata}};\nconst uint16_t ${filename}_size = sizeof(${filename});\n")
	endforeach()
endfunction()

create_resources(${CMAKE_SOURCE_DIR}/src/images ${CMAKE_BINARY_DIR}/images.h)
create_resources(${CMAKE_SOURCE_DIR}/src/fonts ${CMAKE_BINARY_DIR}/fonts.h)
