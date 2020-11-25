# UNFLoader

This folder contains information on how to use, and how to build UNFLoader for both Windows and Linux operating systems.
</br>
</br>
### Table of Contents
* [How to use UNFLoader](#how-to-use-unfloader)
* [How to Build UNFLoader for Windows](#how-to-build-unfloader-for-windows)
* [How to Build UNFLoader for Linux](#how-to-build-unfloader-for-linux)
</br>
</br>
### How to use UNFLoader
Simply execute the program for a full list of commands. If you run the program with the `-help` argument, you have access to even more information (such as how to upload via USB with your specific flashcart). 
The most basic usage is `UNFLoader.exe -r PATH/TO/ROM.n64`. 

Append `-d` to enable debug mode, which allows you to receive/send input from/to the console (Assuming you're using the included USB+debug libraries). If you wrap a part of a command in '@' characters, the data will be treated as a file and will be uploaded to the cart. When uploading files in a command, the filepath wrapped between the '@' characters will be replaced with the size of the data inside the file, with the data in the file itself being appended after. For example, if there is a file called `file.txt` with 4 bytes containing `abcd`, sending the following command: `commandname arg1 arg2 @file.txt@ arg4` will send `commandname arg1 arg2 @4@abcd arg4` to the console.

Append `-l` to enable listen mode, which will automatically reupload a ROM once a change has been detected.
</br>
</br>
### How to Build UNFLoader for Windows
Simply load the project file in Visual Studio 2019 or higher. The <a href="https://docs.microsoft.com/en-us/cpp/build/configuring-programs-for-windows-xp?view=msvc-160">Windows XP toolset</a> needs to be installed, as it is optional in VS 2019.

**The Include folder should already have everything you need for Windows.**
<details><summary>Updating libs to latest version (if required)</summary>

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
</details>
</br>

### How to Build UNFLoader for Linux
You need to have the FTDI driver installed (This is in the [requirements](https://github.com/buu342/N64-UNFLoader#requirements) section so you should've already done this step), as well as ncurses.
Install ncurses by invoking:

```
sudo apt-get install libncurses5-dev libncursesw5-dev
```

Once the dependencies are installed, simply execute the makefile:

```
make -f Makefile
```

**The Include folder should already have everything you need for Linux.**
<details><summary>Updating libs to latest version (if required)</summary>

**ftd2xx + WinTypes**
* Download the FTDI driver provided in the **Requirements** section and extract the zip.
* Go into the `release` folder.
* Grab `ftd2xx.h` and `WinTypes.h` and put it in `UNFLoader/Include`.

**lodepng**
* Download the latest version of LodePNG from [here](https://lodev.org/lodepng/).
* Place `lodepng.cpp` and `lodepng.h` in `UNFLoader/Include`.

Once you have all of these files built and put in the `Include` folder, you're set to compile!
</details>