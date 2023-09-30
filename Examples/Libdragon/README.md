# Libdragon Examples
This folder contains example ROMs written in libdragon utilizing the USB and debug library in conjunction with UNFLoader. Usage instructions are provided below, but assume you already know how to upload a ROM using the tool and how to enable debug mode. If you need instructions on how to do that, execute UNFLoader with the `-help` argument to read up on how to do it for your specific flashcart. 

To build the ROMs, you must be on the unstable branch of Libdragon, and simply call make.

Please note that usb.c and usb.h are part of libdragon, therefore are not included in the sample projects. If you are using a really old version of libdragon for some reason, then these will need to be included and added to the makefiles.


### 1. Hello World
A barebones ROM that prints `Hello World!` to the debug console upon booting and doesn't do anything else. Shows you how to print using either `debug_printf`, or `usb_write`. Also allows you to see how `DEBUG_MODE` affects the final ROM.

**Usage**
1) Upload the ROM.
2) `Hello World!` should print to your debug console screen.
3) Play around with the values of `USE_PRITNF` in `main.c`, `DEBUG_MODE` in `debug.h`. Remember to clean the .o's as make doesn't detect changes to header files!


### 2. Raw USB Reading
A barebones ROM that demonstrates the `usb_read` and `usb_poll` functions by echoing back data sent from the PC. Does not output anything to the TV.

**Usage**
1) Upload the ROM.
2) Type something into the command prompt and press enter.
3) The N64 should echo back what you wrote.
4) Try uploading a file mid command by wrapping it in `@` symbols. Example: `Hello @abc.txt@ World`.
5) Try uploading a file by itself. Example: `@abc.txt@`.


### 3. Command Interpreter
A ROM that demonstrates the command functions provided by the debug library. Allows you to modify parts of the ROM during execution, such as the square's movement, its texture, or the background color. The alternative textures were created using `mksprite` with the `--compress 0` flag, as Libdragon does not yet expose the decompression library outside of the asset functions.

**Usage**
1) Upload the ROM.
2) On boot, a bunch of commands will appear on screen. 
3) Play around with the different commands. Try seeing what happens when you provide more/less/wrong arguments.
4) For replacing the texture, use the three provided `tex#.bin` files. They're 32x32 32-bit RGBA textures in binary format.
5) If you own a 64Drive, you can press the button on the back of the cartridge to execute the `command_button` function.
