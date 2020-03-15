
CC = gcc
LIBS = -lz80ex -lz80ex_dasm -lSDLmain  -lSDL  -lSDL_gfx

OBJS := main.o msemu.o flashops.o ui.o debug.o

CFLAGS = -Wall

ifdef DEBUG
	CFLAGS += -O0 -g -DDEBUG
else
	CFLAGS += -Os
endif

.PHONY: all clean

all: msemu

msemu: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o msemu
