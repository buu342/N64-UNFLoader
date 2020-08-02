# UNFLoader
UNFLoader is a USB ROM uploading (and debugging) tool designed to unify developer flashcarts. 
Currently supported devices:
* 64Drive Hardware 1.0 and 2.0
* EverDrive 3.0
* EverDrive X7


### Requirements:
* Windows XP or higher
* [This FDTI driver](http://www.ftdichip.com/Drivers/CDM/CDM21228_Setup.zip)


### Using UNFLoader
Simply execute the program for a full list of commands. If you run the program with the `-help` argument, you have access to even more information (such as how to upload via USB with your specific flashcart, how to debug thread faults, etc...)

### Using the Debug Library
Simply include the debug.c and debug.h in your project.

### Building the UNFLoader program
Simply load the project file in Visual Studio 2019 or higher.
The Include folder should already have everything you need, but if you wish to build/retrieve the libraries yourself:
**pdcurses.lib**
* Grab the latest version of PDCurses from [here](https://github.com/wmcbrine/PDCurses)
* Extract the contents of the zip (preferrably somewhere with no spaces in the file path, like `c:\pdcurses`)
* Open the Visual Studio Command Prompt (Tools->Command Line->Developer Command Prompt)
* Run the command `set PDCURSES_SRCDIR=c:\PATH\TO\pdcurses`, obviously replacing the path with your one.
* CD into the `pdcurses/wincon` folder
* Run the command `nmake -f Makefile.vc` to build pdcurses
* Copy the `pdcurses.lib` that was compiled from the wincon folder to `UNFLoader/Include`, replacing the pdcurses library in there.
* Copy the `curses.h`, `curspriv.h`, and `panel.h` from the pdcurses directory and put them in `UNFLoader/Include`.
* Open `curses.h` and uncomment the line with `#define MOUSE_MOVED` to fix a warning due to `wincon.h`.
**ftd2xx.lib**
* Download the FTDI driver provided in the **Requirements** section and extract the executable form the zip
* This is a self extracting executable, meaning you can open the .exe with with a zip program. 
* Grab `ftd2xx.h` and put it in `UNFLoader/Include`.
* Grab `ftd2xx.lib` from `i386` or `amd64` (depending on your CPU architecture) and put it in `UNFLoader/Include`.
Once you have all of these files built and put in the `Include` folder, you're set!

### Credits

