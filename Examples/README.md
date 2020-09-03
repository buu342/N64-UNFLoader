# Examples
This folder contains example ROMs utilizing the USB and debug library in conjunction with UNFLoader. Usage instructions are provided below, but assume you already know how to upload a ROM using the tool and how to enable debug mode. If you need instructions on how to do that, execute UNFLoader with the `-help` argument to read up on how to do it for your specific flashcart. 

To build the ROMs, use the provided `makeme.bat` (requires libultra). Linux compatible shell scripts are unavailable for the time being, but will hopefully be included in the near future. The examples should be compatible with CrashOveride's Linux SDK port, however...

### 1. Hello World
A barebones ROM that prints `Hello World!` to the debug console upon booting and doesn't do anything else. Shows you how to print using either `debug_printf`, or `usb_write`. Also provides a playground you enable/disable the use of `osPiRaw` functions, as well as see how `DEBUG_MODE` affects the final ROM.

**Usage**
1) Upload the ROM.
2) `Hello World!` should print to your debug console screen.
3) Play around with the values of `USE_PRITNF` in `main.c`, `DEBUG_MODE` in `debug.h`, or `USE_OSRAW` in `usb.h`. Remember to clean the .o's as make doesn't detect changes to header files!

### 2. Thread Faults
A NuSys ROM where an animation plays. After a very specific sequence in the animation, a thread fault will occur, with the information being printed to your debug console. libultra provides a program called `objdump`, which when fed the .out of your ROM (generated during compilation), it will output the full disassembly of it (which you can use to help diagnose the crash). A batch file named `dump.bat` is provided to facilitate the usage of the program. The mistake in the code is a very subtle incorrect number, but hopefully you can diagnose the cause with the help of the dump. 

**Usage**
1) Upload the ROM.
2) Watch the incredible Pixar-level animation unfold.
3) Eventually, the game should hang, and fault information should print to your debug console.
4) Run the `dump.bat` provided in your ROM's directory, and after its execution is finished, a text file named `disassembly.txt` should appear alongside it.
5) Look at your debug console and copy the value of `pc` after the `0x`.
6) Open `disassembly.txt` in your text editor of choice and perform a search for your `pc` value. It should tell you which function caused the hang, and more-less where it happened in said function. 
7) Try to fix the program so the hang no longer occurs!
