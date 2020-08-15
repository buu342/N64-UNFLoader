# Examples
This folder contains example ROMs utilizing the USB and debug library in conjunction with UNFLoader. Usage instructions are provided below, but assume you already know how to upload a ROM using the tool and how to enable debug mode. If you need instructions on how to do that, execute UNFLoader with the `-help` argument to read up on how to do it for your specific flashcart. 

To build the ROMs, use the provided `makeme.bat` (requires libultra). Linux compatible shell scripts are unavailable for the time being, but will hopefully be included in the near future...

### 1. Hello World
A barebones ROM that prints `Hello World!` to the debug console upon booting and doesn't do anything else. Shows you how to print using either `debug_printf`, or `usb_write`. Also provides a playground you enable/disable the use of `osPiRaw` functions, as well as see how `DEBUG_MODE` affects the final ROM.

**Usage**
1) Upload the ROM.
2) `Hello World!` should print to your debug console screen.
3) Play around with the values of `USE_PRITNF` in `main.c`, `DEBUG_MODE` in `debug.h`, or `USE_OSRAW` in `usb.h`. Remember to clean the .o's as make doesn't detect changes to header files!