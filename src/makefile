
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

CFLAGS = -O2 -DLSB_FIRST -DX86_ASM


msemu.exe: rawcga.o	msemu.o
	$(CC) $(CFLAGS) z80em/Z80.o  rawcga.o msemu.o  -o $(OUTFILE) $(USINGMINGW) $(LIBS) 
	
msemu.o: msemu.c
	$(CC) $(CFLAGS) -c msemu.c -o msemu.o

rawcga.o: 
	objcopy -I binary -O elf32-i386  --binary-architecture i386 rawcga.bin rawcga.o
 
 