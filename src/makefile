
CC = gcc
LIBS = -lSDLmain  -lSDL  -lSDL_gfx
OUTFILE = msemu


# Check if Windows
ifeq (${OS},Windows_NT)
	USINGMINGW = -mwindows -lmingw32  
	OUTFILE = msemu.exe
endif



# Following #defines for Z80em can be used:
# DEBUG			Compile with builtin debugger
# LSB_FIRST		Compile for a low-endian machine
# X86_ASM			Use inline 80x86 assembly (GCC only)

CFLAGS = -Os -Wall -DLSB_FIRST -DX86_ASM -DDEBUG


msemu.exe: msemu.o flashops.o
	$(CC) $(CFLAGS) z80em/Z80.o z80em/Z80Debug.o flashops.o  msemu.o  -o $(OUTFILE) $(USINGMINGW) $(LIBS) 
	
msemu.o: msemu.c
	$(CC) $(CFLAGS) -c msemu.c -o msemu.o

flashops.o: flashops.c
	$(CC) $(CFLAGS) -c flashops.c -o flashops.o

clean:
	rm -f flashops.o msemu.o msemu
