
                          Mailstation Emulator v0.1a
                                  (1/5/2010)

                 Created by Jeff Bowman (fyberoptic@gmail.com)
                     http://www.fybertech.net/mailstation


-----------------------------------------------------------------------------

This emulator is still obviously in early stages, but so far it seems to be
capable of running the Mailstation OS for its core functionality.

To use it, you need a ROM image of the Mailstation OS.  By default, msemu
looks for this to be named "codeflash.bin".  You can also specify an alternate
name on the command line if you wish.  This lets you easily switch between 
different firmware versions, or even your own custom code.

Another ROM image used by msemu is "dataflash.bin", which holds your personal
settings.  By default, if this file does not exist, msemu will create a new
one automatically.  But you can also include versions extracted from your 
actual Mailstation if you wish, as long as you name it as mentioned.  You'll
need to run the matching Mailstation OS version in the emulator, however.

Once the Mailstation OS is started up, you can interact with it with the 
keyboard, just as you would on the Mailstation itself, though with a few 
differences.  For example, "Function" is mapped to the left control key.  The
Mailstation "Home" button is Home on your keyboard, but the "Back" button has 
been remapped to the End key.  F1 through F5 simulate the five buttons along 
the top of the Mailstation keyboard.  And arrow keys are still arrow keys.


Emulator Keys (NOTE: CTRL means right control key, not left):
  
  F9     - Toggle 2X screen size.
  F10    - Toggle full-screen.  
  F12    - Mailstation power button.
           NOTE: Currently you have to press this twice to power off again.
  CTRL R - Forces an emulator reset.
  CTRL 1 - Changes LCD color to white.
  CTRL 2 - Changes LCD color to red.
  CTRL 3 - Changes LCD color to green (default).
  CTRL 4 - Changes LCD color to blue.
  CTRL 5 - Changes LCD color to yellow.
  Escape - Exits the emulator.


Command Line Switches:
  /console - Writes debugging information to the console window (a lot of it!)
  /debug   - Writes debugging information to debug.out file


Things NOT emulated:

  - The modem.
  - The parallel port.
  - Some of the keyboard buttons
  - Variable CPU speed
  - Variable timer/interrupt speeds
  - Writing to the real-time clock
  - Lots of other stuff probably!


  
Credits:
  
  - z80em library
    http://www.komkon.org/~dekogel/misc.html

  - SDL library
    http://www.libsdl.org/
    
  - SDL_gfx library
    http://www.ferzkopp.net/joomla/content/view/19/14/
    
    


Version History:
  
  v0.1a (1/5/2010)
    - Code clean-up
    - Can now be compiled under Linux
    - Included source code
    - Fixed codeflash/dataflash to always allocate 1MB/512KB buffers, 
      regardless of input file sizes.  Some codeflash images are
      a tad truncated, which resulted in reading outside of the
      allocated buffer in some Mailstation operations.
    - Added /console and /debug switches back in.
      

  
  v0.1 (1/3/2010)
    - Initial release!
  	
  