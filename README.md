# UNFLoader
![Build Status](https://github.com/buu342/N64-UNFLoader/actions/workflows/build.yml/badge.svg) [![Build Status](https://dev.azure.com/buu342/buu342/_apis/build/status/buu342.N64-UNFLoader?branchName=master)](https://dev.azure.com/buu342/buu342/_build/latest?definitionId=1&branchName=master)

**The code in this repo might be unstable! For stable releases, head to the [releases page](https://github.com/buu342/N64-UNFLoader/releases)**

UNFLoader is a USB ROM uploader (and debugging) tool designed to unify developer flashcarts for the Nintendo 64. The goal of this project is to provide developers with USB I/O functions that work without needing to worry about the target flashcart, provided by a single C file (`usb.c`) targeting both libultra and libdragon. A very basic debug library (`debug.c`) that makes use of said USB library is also provided.

Currently supported flashcarts:
* 64Drive Hardware 1.0 (No longer comercially sold), using firmware 2.05+
* [64Drive Hardware 2.0](http://64drive.retroactive.be/), using firmware 2.05+
* EverDrive 3.0 (No longer comercially sold), using OS version 3.04+ (Please note that OS v3.07 onwards is **not** compatible with the 3.0).
* [EverDrive X7](https://krikzz.com/store/home/55-everdrive-64-x7.html), using OS version 3.04+
* [SC64](https://github.com/Polprzewodnikowy/SummerCollection)


### Table of contents
* [UNFLoader](UNFLoader/README.md)
    - [System Requirements](UNFLoader/README.md#system-requirements)
    - [How to use UNFLoader](UNFLoader/README.md#how-to-use-unfloader)
    - [How to build UNFLoader for Windows](UNFLoader/README.md#how-to-build-unfloader-for-windows)
    - [How to build UNFLoader for macOS](UNFLoader/README.md#how-to-build-unfloader-for-macOS)
    - [How to build UNFLoader for Linux](UNFLoader/README.md#how-to-build-unfloader-for-linux)
* [USB + Debug Library](USB%2BDebug%20Library/README.md)
    - [How to use the USB library](USB%2BDebug%20Library/README.md#how-to-use-the-usb-library)
    - [How to use the Debug library](USB%2BDebug%20Library/README.md#how-to-use-the-debug-library)
    - [How to debug with GDB](USB%2BDebug%20Library/README.md#how-to-debug-with-gdb)
        * [Notes about GDB debugging](USB%2BDebug%20Library/README.md#notes-about-gdb-debugging)
        * [Notes about GDB debugging with Libultra](USB%2BDebug%20Library/README.md#notes-about-gdb-debugging-with-libultra)
        * [Notes about GDB debugging with Libdragon](USB%2BDebug%20Library/README.md#notes-about-gdb-debugging-with-libdragon)
    - [How these libraries work](USB%2BDebug%20Library/README.md#how-these-libraries-work)
* [Example ROMs](Examples/README.md)
    - [Libultra](Examples/Libultra/README.md#libultra-examples)
    - [Libdragon](Examples/Libdragon/README.md#libdragon-examples)
* [Extending UNFLoader](#extending-unfloader)
* [Known Issues and Suggestions](#known-issues-and-suggestions)
* [Credits](#credits)


### Extending UNFLoader

This repository has a [wiki](https://github.com/buu342/N64-UNFLoader/wiki) where the tool, its libraries, and the supported flashcarts are all documented. It is recommended that you read through that to familiarize yourself with everything. If you want to submit changes, feel free to fork the repository and make pull requests!


### Known issues and suggestions

All known issues are mentioned in the [issues page](https://github.com/buu342/N64-UNFLoader/issues). Suggestions also belong there for the sake of keeping everything in one place.


### Credits
* Marshallh for providing the 64Drive USB application code which this program was based off of.
* KRIKzz, saturnu, NetworkFusion, lambertjamesd, Wiseguy, and jsdf for providing sample code for the EverDrive 3.0 and/or X7.
* fraser and NetworkFusion for all the help provided during the development of this project as well as their support.
* NetworkFusion for lending me his remote test rig for the ED64-V3 and ED64-X7, as well as setting up the azure pipeline system and helping organize this repo.
* danbolt for helping test this on Debian, as well as providing changes to get the tool compiling under macOS.
* Polprzewodnikowy for implementing support for his SC64, as well as setting up GitHub actions and providing some fixes/improvements to the library.
* CrashOveride for ensuring the samples compile on his Linux libultra port.
* The folk at N64Brew for being patient with me and helping test the program! Especially command_tab, NetworkFusion, CrashOveride, gravatos, PerKimba, manfried, Wiseguy, Zygal, kivan117, and Kokiri. You guys are the reason this project was possible!



This project uses [lodePNG](https://github.com/lvandeve/lodepng) by Lode Vandevenne, [ncurses](https://invisible-island.net/ncurses/) by the GNU Project, [pdcurses](https://github.com/wmcbrine/PDCurses) by William McBrine, and the [D2XX drivers](https://www.ftdichip.com/Drivers/D2XX.htm) by FTDI.
