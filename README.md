# UNFLoader
A universal N64 flashcart ROM uploader and debug library.


### Requirements:
* Windows XP or higher
* [This FDTI driver](http://www.ftdichip.com/Drivers/CDM/CDM21228_Setup.zip)


### Using UNFLoader


### Using the Debug Library


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

