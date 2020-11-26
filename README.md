# UNFLoader

[![Build Status](https://dev.azure.com/buu342/buu342/_apis/build/status/buu342.N64-UNFLoader?branchName=master)](https://dev.azure.com/buu342/buu342/_build/latest?definitionId=1&branchName=master)

**The code in this repo might be unstable! For stable versions, head to the [releases page](https://github.com/buu342/N64-UNFLoader/releases)**

UNFLoader is a USB ROM uploader (and debugging) tool designed to unify developer flashcarts for the Nintendo 64. The goal of this project is to provide developers with USB I/O functions that work without needing to worry about the target flashcart, provided by a single C file (`usb.c`) targeting libultra. An very basic debug library (`debug.c`) that makes use of said USB library is also provided.

Currently supported devices:
* 64Drive Hardware 1.0 (No longer comercially sold), using firmware 2.05+
* [64Drive Hardware 2.0](http://64drive.retroactive.be/), using firmware 2.05+
* EverDrive 3.0 (No longer comercially sold), using OS version 3.04+
* [EverDrive X7](https://krikzz.com/store/home/55-everdrive-64-x7.html), using OS version 3.04+
* [SummerCart64](https://github.com/Polprzewodnikowy/SummerCollection), only N64 -> PC debug supported
</br>

### Table of contents
* [UNFLoader](UNFLoader)
    - [System Requirements](UNFLoader#system-requirements)
    - [How to use UNFLoader](UNFLoader#how-to-use-unfloader)
    - [How to build UNFLoader for Windows](UNFLoader#how-to-build-unfloader-for-windows)
    - [How to build UNFLoader for Linux](UNFLoader#how-to-build-unfloader-for-linux)
* [USB + Debug Library](USB%2BDebug%20Library)
    - [How to use the USB library](USB%2BDebug%20Library#how-to-use-the-usb-library)
    - [How to use the Debug library](USB%2BDebug%20Library#how-to-use-the-debug-library)
* [Example ROMs](Examples)
    - [Print "Hello World"](Examples#1-hello-world)
    - [Handle thread faults](Examples#2-thread-faults)
    - [Read from USB](Examples#3-raw-usb-reading)
    - [Interpret USB commands](Examples#4-command-interpreter)
* [Important implementation details](#important-implementation-details)
* [Extending the libraries/loader program](#extending-the-librariesloader-program)
* [Known Issues](#known-issues)
* [Credits](#credits)
</br>

### Important implementation details
<details><summary>USB Library</summary>
<p>

**General**

* Due to the data header, a maximum of 8MB can be sent through USB in a single `usb_write` call.
* By default, the USB Buffers are located on the 63MB area in SDRAM, which means that it will overwrite ROM if your game is larger than 63MB. More space can be allocated by changing `usb.h`.
* Avoid using `usb_write` while there is data that needs to be read from the USB first, as this will cause lockups for 64Drive users and will potentially overwrite the USB buffers on the EverDrive. Use `usb_poll` to check if there is data left to service. If you are using the debug library, this is handled for you.


**64Drive**

* All data through USB is 4 byte aligned. This might result in up to 3 extra bytes being sent/received through USB, which will be padded with zeroes.


**EverDrive**

\<Nothing>


</p>
</details>

<details><summary>Debug Library</summary>
<p>

* The debug library runs on a dedicated thread, which will only execute if invoked by debug commands. All threads will be blocked until the USB thread is finished.
* Incoming USB data must be serviced first before you are able to write to USB. Every time a debug function is used, the library will first ensure there is no data to service before continuing. This means that incoming USB data **will only be read if a debug function is called**. Therefore, it is recommended to call `debug_pollcommands` as often as possible to ensure that data doesn't stay stuck waiting to be serviced. See Example 3 or 4 for examples on how to read incoming data.
</p>
</details>
</br>

### Extending the libraries/loader program
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
</br>
</br>
### Known issues

All known issues are mentioned in the [issues page](https://github.com/buu342/N64-UNFLoader/issues). Suggestions also belong there.
</br>
</br>
### Credits
Marshallh for providing the 64Drive USB application code which this program was based off of.

KRIKzz, saturnu, networkfusion, lambertjamesd, and jsdf for providing sample code for the EverDrive 3.0 and/or X7.

fraser and networkfusion for all the help provided during the development of this project as well as their support.

networkfusion for lending me his remote test rig for the ED3 and X7, as well as setting up the azure pipeline system and helping organize this repo.

danbolt for helping test this on Debian, as well as providing changes to get the tool compiling under macOS.

korgeaux for implementing support for his SummerCart64.

CrashOveride for ensuring the samples compile on his Linux libultra port. 

This project uses [lodePNG](https://github.com/lvandeve/lodepng) by Lode Vandevenne, [ncurses](https://invisible-island.net/ncurses/) by the GNU Project, [pdcurses](https://github.com/wmcbrine/PDCurses) by William McBrine, and the [D2XX drivers](https://www.ftdichip.com/Drivers/D2XX.htm) by FTDI.

The folk at N64Brew for being patient with me and helping test the program! Especially command_tab, networkfusion, CrashOveride, gravatos, PerKimba, manfried and kivan117. You guys are the reason this project was possible!
