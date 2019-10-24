
CC = gcc
LIBS = -lz80ex -lz80ex_dasm -lSDLmain  -lSDL  -lSDL_gfx
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

CFLAGS = -Wall -DLSB_FIRST -DX86_ASM

ifdef DEBUG
	CFLAGS += -O0 -g -DDEBUG
else
	CFLAGS += -Os
endif


msemu.exe: main.o msemu.o flashops.o logger.o ui.o debug.o
	$(CC) $(CFLAGS) main.o logger.o flashops.o ui.o msemu.o debug.o  -o $(OUTFILE) $(USINGMINGW) $(LIBS)

main.o: main.c
	$(CC) $(CFLAGS) -c main.c -o main.o

msemu.o: msemu.c
	$(CC) $(CFLAGS) -c msemu.c -o msemu.o

flashops.o: flashops.c
	$(CC) $(CFLAGS) -c flashops.c -o flashops.o

logger.o: logger.c
	$(CC) $(CFLAGS) -c logger.c -o logger.o

ui.o: ui.c
	$(CC) $(CFLAGS) -c ui.c -o ui.o

debug.o: debug.c
	$(CC) $(CFLAGS) -c debug.c -o debug.o

clean:
	rm -f *.o msemu
