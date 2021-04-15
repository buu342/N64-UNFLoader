# Libultra Examples
This folder contains example ROMs written in libultra utilizing the USB and debug library in conjunction with UNFLoader. Usage instructions are provided below, but assume you already know how to upload a ROM using the tool and how to enable debug mode. If you need instructions on how to do that, execute UNFLoader with the `-help` argument to read up on how to do it for your specific flashcart. 

To build the ROMs, use the provided `makeme.bat`. On Linux, using CrashOveride's SDK port, simply just call `make`.


### 1. Hello World
A barebones ROM that prints `Hello World!` to the debug console upon booting and doesn't do anything else. Shows you how to print using either `debug_printf`, or `usb_write`. Also provides a playground you enable/disable the use of `osPiRaw` functions, as well as see how `DEBUG_MODE` affects the final ROM.

**Usage**
1) Upload the ROM.
2) `Hello World!` should print to your debug console screen.
3) Play around with the values of `USE_PRITNF` in `main.c`, `DEBUG_MODE` in `debug.h`, or `USE_OSRAW` in `usb.h`. Remember to clean the .o's as make doesn't detect changes to header files!


### 2. Thread Faults
A NuSys ROM where an animation plays. After a very specific sequence in the animation, a thread fault will occur, with the information being printed to your debug console. libultra provides a program called `objdump`, which when fed the .out of your ROM (generated during compilation), it will output the full disassembly of it (which you can use to help diagnose the crash). A batch file named `dump.bat` is provided to facilitate the usage of the program. The mistake in the code is a very subtle incorrect number, but hopefully you can diagnose the cause with the help of the dump. 

**Credits:**
* Arena background from Dragon Ball Z: Super Butouden
* EnteiTH for the character sprites

**Usage**
1) Upload the ROM.
2) Watch the incredible Pixar-level animation unfold.
3) Eventually, the game should hang, and fault information should print to your debug console.
4) Run the `dump.bat` provided in your ROM's directory, and after its execution is finished, a text file named `disassembly.txt` should appear alongside it.
5) Look at your debug console and copy the value of `pc` after the `0x`.
6) Open `disassembly.txt` in your text editor of choice and perform a search for your `pc` value. It should tell you which function caused the hang, and more-less where it happened in said function. 
7) Try to fix the program so the hang no longer occurs!


### 3. Raw USB Reading
A barebones ROM that demonstrates the `usb_read` and `usb_poll` functions by echoing back data sent from the PC. Does not output anything to the TV.

**Usage**
1) Upload the ROM.
2) Type something into the command prompt and press enter.
3) The N64 should echo back what you wrote.
4) Try uploading a file mid command by wrapping it in `@` symbols. Example: `Hello @abc.txt@ World`.
5) Try uploading a file by itself. Example: `@abc.txt@`.


### 4. Command Interpreter
A ROM that demonstrates the command functions provided by the debug library. Allows you to modify parts of the ROM during execution, such as the square's rotation, its texture, or the background color.

**Usage**
1) Upload the ROM.
2) On boot, a bunch of commands will appear on screen. 
3) Play around with the different commands. Try seeing what happens when you provide more/less/wrong arguments.
4) For replacing the texture, use the three provided `tex#.bin` files. They're 32x32 32-bit RGBA textures in binary format.
