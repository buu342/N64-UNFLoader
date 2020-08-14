# UNFLoader
**This project is in development and was only uploaded here to facilitate its development.**

UNFLoader is a USB ROM uploader (and debugging) tool designed to unify developer flashcarts for the Nintendo 64. The goal of this project is to provide developers with USB I/O functions that work without needing to worry about the target flashcart, provided by a single C file (`usb.c`) targeting libultra. I have also implemented a very basic debug library (`debug.c`) that makes use of said USB library.
Currently supported devices:
* [64Drive Hardware 1.0 and 2.0](http://64drive.retroactive.be/)
* EverDrive 3.0 (No longer comercially sold)
* [EverDrive X7](https://krikzz.com/store/home/55-everdrive-64-x7.html)


### Current TODO list:
* **Library** - Write sample demos.
* **UNFLoader** - Implement scrolling.
* **UNFLoader** - Implement command history.
* **UNFLoader** - Implement binary file uploading via USB.
* **UNFLoader** - Optimize file uploading speed for the EverDrive 3.0.
* **UNFLoader** - Optimize file uploading speed for the EverDrive X7.
* **UNFLoader** - Look into the possibility of implementing remote console reset.
* **UNFLoader + Library** - Implement binary data reading from USB for the 64Drive.
* **UNFLoader + Library** - Implement binary data reading from USB for the EverDrive 3.0.
* **UNFLoader + Library** - Implement binary data reading from USB for the EverDrive X7.
* **UNFLoader + Library** - Implement data checksum.


### Requirements:
* Windows XP or higher
* [This FDTI driver](http://www.ftdichip.com/Drivers/CDM/CDM21228_Setup.zip)


### Using UNFLoader
Simply execute the program for a full list of commands. If you run the program with the `-help` argument, you have access to even more information (such as how to upload via USB with your specific flashcart). The most basic usage is `UNFLoader.exe -r PATH/TO/ROM.n64`. Append `-d` to enable debug mode, which allows you to receive/send input from/to the console (Assuming you're using the included USB+debug libraries). Append `-l` to enable listen mode, which will automatically reupload a ROM once a change has been detected.


### Using the USB Library
Simply include the `usb.c` and `usb.h` in your project. The library features a read (unimplemented) and write function for USB communication.
Here are the included functions:
```c
/*==============================
    usb_write
    Writes data to the USB.
    @param The DATATYPE that is being sent
    @param A buffer with the data to send
    @param The size of the data being sent
==============================*/
void usb_write(int datatype, const void* data, int size);
```


### Using the Debug Library
Simply include the `debug.c` and `debug.h` in your project. You can edit `debug.h` to enable/disable debug mode (which makes your ROM smaller if disabled), as well as configure other aspects of the library. The library features some basic debug functions and a thread that prints fault information.
Here are the included functions:
```c
/*==============================
    debug_printf
    Prints a formatted message to the developer's command prompt.
    Supports up to 256 characters.
    @param A string to print
    @param Variadic arguments to print as well
==============================*/
void debug_printf(const char* message, ...);

/*==============================
    debug_screenshot
    Sends the currently displayed framebuffer through USB.
    Does not pause the drawing thread!
    @param The size of each pixel of the framebuffer in bytes
           Typically 4 if 32-bit or 2 if 16-bit
    @param The width of the framebuffer
    @param The height of the framebuffer
==============================*/
void debug_screenshot(int size, int w, int h)

/*==============================
    debug_assert
    Halts the program if the expression returns false
    @param The expression to test
 ==============================*/
#define debug_assert(expression)
```


### Building the UNFLoader program
Simply load the project file in Visual Studio 2019 or higher.
The Include folder should already have everything you need, but if you wish to build/retrieve the libraries yourself:

**pdcurses.lib**
* Grab the latest version of PDCurses from [here](https://github.com/wmcbrine/PDCurses).
* Extract the contents of the zip (preferrably somewhere with no spaces in the file path, like `c:\pdcurses`).
* Open the Visual Studio Command Prompt (Tools->Command Line->Developer Command Prompt).
* Run the command `set PDCURSES_SRCDIR=c:\PATH\TO\pdcurses`, obviously replacing the path with your one.
* CD into the `pdcurses/wincon` folder.
* Run the command `nmake -f Makefile.vc` to build pdcurses.
* Copy the `pdcurses.lib` that was compiled from the wincon folder to `UNFLoader/Include`, replacing the pdcurses library in there.
* Copy the `curses.h`, `curspriv.h`, and `panel.h` from the pdcurses directory and put them in `UNFLoader/Include`.
* Open `curses.h` and uncomment the line with `#define MOUSE_MOVED` to fix a warning due to `wincon.h`.

**ftd2xx.lib**
* Download the FTDI driver provided in the **Requirements** section and extract the executable from the zip.
* This is a self extracting executable, meaning you can open the .exe with with a zip program. 
* Grab `ftd2xx.h` and put it in `UNFLoader/Include`.
* Grab `ftd2xx.lib` from `i386` or `amd64` (depending on your CPU architecture) and put it in `UNFLoader/Include`.

**lodepng**
* Download the latest version of LodePNG from [here](https://lodev.org/lodepng/).
* Place `lodepng.cpp` and `lodepng.h` in `UNFLoader/Include`.

Once you have all of these files built and put in the `Include` folder, you're set to compile!


### Extending the debug library/loader program
All data gets sent in the following manner:
* 4 Bytes with `'D' 'M' 'A' '@'` to signalize data start.
* 4 Bytes with 1 byte for the data type and 3 for the size of the data.
* N bytes with the data.
* 4 bytes with `'C' 'M' 'P' 'H'` to signalize data end.

The data type mentioned is up to the developer to implement. If you wish to add more data types so that the data is handled differently on the PC side, you must make changes to UNFLoader's `debug.c` file to add support for said command (check the `debug_main()` function's switch statement). Make sure you also add your new data types to the USB library's `usb.h`! Here's a list of default data types:
```c
// Incoming data is text for printf
#define DATATYPE_TEXT       0x01

// Incoming data is raw binary
#define DATATYPE_RAWBINARY  0x02

// Incoming data describes contents of next incoming data
// Use to help you process the data that comes after it (see screenshot implmentation)
#define DATATYPE_HEADER     0x03

// Incoming data is a framebuffer
#define DATATYPE_SCREENSHOT 0x04
```
There is no checksum in place to detect the authenticity of the data. This might be implemented at a later date...


### Important implementation details
**General**
* Due to the data header, a maximum of 8MB can be sent through USB in a single `usb_write` call.

**64Drive**
* The USB Buffers are located on the 63MB area in SDRAM. This is a problem if your game is 64MB, and can be fixed by putting the 64Drive in extended address mode. This however, will break HW1 compatibility.
* All data through USB is 4 byte aligned. This might result in up to 3 extra bytes being sent through USB. 

**EverDrive 3.0**

\<None>

**EverDrive X7**

\<None>


### Credits
Marshallh for providing the 64Drive USB application code which this program was based off of.

KRIKzz, saturnu and jsdf for providing sample code for the EverDrive 3.0 and/or X7.

networkfusion and fraser for all the help provided during the development of this project as well as their support.

The folk at N64Brew for being patient with me and helping test the program! Especially command_tab, networkfusion, CrashOveride, Gravatos, PerKimba, Manfried and Kivan117. You guys are the reason this project was possible!
