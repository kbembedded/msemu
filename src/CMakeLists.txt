if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
	set (PLATFORM_SOURCES
		platform/windows/getopt.c
		platform/windows/strndup.c
	)
endif ()

add_executable(msemu
	${PLATFORM_SOURCES}
	debug.c
	mem.c
	lcd.c
	main.c
	msemu.c
	io.c
	io_rtc.c
	io_ioctltest.c
	ui.c
)

if (BUILD_DEPENDENCIES)
	# Adds dependency on locally build copy of z80ex
	add_dependencies(msemu Z80EX)

	# Adds downloaded dependencies to include/link dirs
	target_include_directories(msemu PRIVATE ${EXTERNAL_INCLUDE_DIR})
	target_link_directories(msemu PRIVATE ${EXTERNAL_LIBRARY_DIR})

	# Copy DLL's to target dir
	file(GLOB EXTERNAL_DLLS "${EXTERNAL_LIBRARY_DIR}/*.dll")
	foreach(EXTERNAL_DLL IN LISTS EXTERNAL_DLLS)
		add_custom_command(TARGET msemu POST_BUILD
			COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EXTERNAL_DLL} $<TARGET_FILE_DIR:msemu>
			COMMENT "Copying ${EXTERNAL_DLL} to build directory"
		)
	endforeach()
endif  ()

target_include_directories(
	msemu PRIVATE
	${CMAKE_BINARY_DIR}/include
)

target_link_directories(
	msemu PRIVATE
	${CMAKE_BINARY_DIR}/lib
	${CMAKE_BINARY_DIR}/bin
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
