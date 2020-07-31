# UNFLoader
A universal N64 flashcart ROM uploader and debug library

uses the PDCurses lib from https://github.com/wmcbrine/PDCurses

Built with commands:
set PDCURSES_SRCDIR=<DIR>\PDCurses\
cd PDCURSES_SRCDIR=<DIR>\PDCurses\wincon
nmake -f Makefile.vc

This build suffers from an access violation when trying to write text on Windows 10.