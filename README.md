# MailStation EMUlator

This emulator is still obviously in early stages, but so far it seems to be capable of running the Mailstation OS for its core functionality. The actual emulation is handled by the [z80ex library](https://sourceforge.net/projects/z80ex/).

A ROM image of the Mailstation codeflash, the main firmware, is required. By default, `msemu` looks for `./codeflash.bin`, or the path can be specified on the command line. This can be used to switch between firmware versions or run custom code.

Functionality has really only been tested with DET1 models, firmware up to and 3.03A. It would be interesting to see what happens with other firmware versions.

Additionally, the Mailstation has a dataflash ROM inside. While not exactly required by `msemu`, an image is still needed. A real image can be used, or `msemu` can generate a blank one which the Mailstation firmware will initialize it for use. By default, this file is `./dataflash.bin` if the path is not otherwise specified.

Once the Mailstation firmware is fully booted, it can be interacted with through the keyboard as on real hardware. Some keys are remapped while others are currently completely unmapped. See below for key mappings in msemu.

There is also a **_very_** rudimentary interactive debugger implemented. This allows for setting a breakpoint on a specific PC address, single stepping (which also shows the current opcode and any IO or MEM accesses), and changing the current terminal verbosity output. More information about this is below.


### Keyboard Mappings and Control

#### Mappings
```
F12     - Mailstation Power button
L_CTRL  - Function/Fn
Home    - Main
End     - Back
F1      - F1 (leftmost grey key underneath the LCD screen)
F2      - F2
F3      - F3
F4      - F4
F5      - F5 (rightmost grey key underneath the LCD screen)
```

##### Unmapped
A handful of Mailstation  keys are currently unmapped, these are:
```
@
Get E-Mail
Size
Spell
Print
```

#### Control
The graphical window has a couple of control keys:
```
ESC       - Exits the emulator (this is a normal method of shutdown)
R_CTRL+R  - Force emulator reset; Z80 resets to PC 0x0000
```


### Debugger
The 'msemu' contains an interactive debugger. Be warned, its operation is very rough. Once 'msemu' is started, ctrl+c can be pressed on the terminal window, not the graphical LCD window, to break execution and issue a few simple commands. It allows for a single breakpoint to be set when the PC reaches the specified value.
```
Available commands:
f - Force debug printing o[F]f during exec
o - Force debug printing [O]n during exec
e - [E]xamine current registers
b - Set a [B]reakpoint on PC, 'b <PC>', -1 to disable
l - [L]ist the current breakpoint
h - Display this [H]elp menu
s - [S]ingle step execution
c - [C]ontinue execution
q - [Q]uit emulation and exit completely
```
Note that 'q' will exit the emulator the same as pressing ESC on the graphical window.

### Currently Known Shortcomings
Things NOT emulated:
- The modem.
- The parallel port.
- Some of the keyboard buttons
- Variable CPU speed
- Variable timer/interrupt speeds
- Writing to the real-time clock
- Standard INT handling


### Building
This project currently requires libz80ex, SDL 1.2 and SDL-gfx 1.2.

Simply run

`make`

The application can be started with:

`./src/msemu -c /path/to/codeflash.bin -d /path/to/dataflash.bin`

If not provided, `msemu` will attempt to open `./codeflash.bin` and `./dataflash.bin` As noted above, codeflash.bin is required for execution as this is the main firmware ROM. If dataflash.bin is not provided, `./dataflash.bin` will be created and populated.


### History
This work was originally pioneered by and would not have existed without the effort put in by [Fyberoptic](http://www.fybertech.net/mailstation). A huge thanks to Fyberoptic for letting development of this continue nearly a decade after it was originally started. The first major public release of `msemu`, marked rev 0.1a, was dated 2010/01/05.
