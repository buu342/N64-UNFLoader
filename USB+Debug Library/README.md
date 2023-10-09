# USB + Debug Library
This folder contains both the USB and debug library that works in tandem with UNFLoader's communication protocol. The USB and debug library are available for both libultra and libdragon. For basic examples regarding how to use the library, take a look at the [samples folder](../Examples).
</br>
</br>

### Table of Contents
* [How to use the USB library](#how-to-use-the-usb-library)
* [How to use the Debug library](#how-to-use-the-debug-library)
* [How these libraries work](#how-these-libraries-work)
</br>

### How to use the USB library
Simply include the `usb.c` and `usb.h` in your project. If you intend to use the USB library in libdragon, you must uncomment `#define LIBDRAGON` in `usb.h`. </br></br>
You must call `usb_initialize()` once before doing anything else. The library features a read and write function for USB communication. You can edit `usb.h` to configure some aspects of the library.
To write to USB, simply call `usb_write` and fill in the required data accordingly. An application must be running on the PC which can service the incoming data, such as UNFLoader in debug mode.
To be able to read from USB, you must first call `usb_poll`. If it returns a nonzero value, then there is data available for you to read using `usb_read` and other similar functions. You can find information about the incoming data by using the `USBHEADER_GETX` helper macros. Polling should be done once per game loop.
<details><summary>Included functions list</summary>
<p>
    
```c
/*==============================
    usb_initialize
    Initializes the USB buffers and pointers
    @return 1 if the USB initialization was successful, 0 if not
==============================*/
char usb_initialize();

/*==============================
    usb_getcart
    Returns which flashcart is currently connected
    @return The CART macro that corresponds to the identified flashcart
==============================*/
char usb_getcart();

/*==============================
    usb_write
    Writes data to the USB.
    Will not write if there is data to read from USB
    @param The DATATYPE that is being sent
    @param A buffer with the data to send
    @param The size of the data being sent
==============================*/
void usb_write(int datatype, const void* data, int size);

/*==============================
    usb_poll
    Returns the header of data being received via USB
    The first byte contains the data type, the next 3 the number of bytes left to read
    @return The data header, or 0
==============================*/
u32 usb_poll();

/*==============================
    usb_read
    Reads bytes from USB into the provided buffer
    @param The buffer to put the read data in
    @param The number of bytes to read
==============================*/
void usb_read(void* buffer, int size);

/*==============================
    usb_skip
    Skips a USB read by the specified amount of bytes
    @param The number of bytes to skip
==============================*/
void usb_skip(int nbytes);

/*==============================
    usb_rewind
    Rewinds a USB read by the specified amount of bytes
    @param The number of bytes to rewind
==============================*/
void usb_rewind(int nbytes);

/*==============================
    usb_purge
    Purges the incoming USB data
==============================*/
void usb_purge();

/*==============================
    usb_timedout
    Checks if the USB timed out recently
    @return 1 if the USB timed out, 0 if not
==============================*/
extern char usb_timedout();

/*==============================
    usb_sendheartbeat
    Sends a heartbeat packet to the PC
    This is done once automatically at initialization,
    but can be called manually to ensure that the
    host side tool is aware of the current USB protocol
    version.
==============================*/
void usb_sendheartbeat();

// Use these to conveniently read the header from usb_poll()
#define USBHEADER_GETTYPE(header)
#define USBHEADER_GETSIZE(header)
```
</p>
</details>
</br>

### How to use the Debug library
The debug library is a basic practical implementation of the USB library. Simply include the `debug.c` and `debug.h` in your project (along with the usb library). If you intend to use the USB library in libdragon, you must uncomment `#define LIBDRAGON` in `usb.h`. </br></br>
You must call `debug_initialize()` once before doing anything else. If you are using this library, there is no need to worry about anything regarding the USB library as this one takes care of everything for you (initialization, includes, etc...). You can edit `debug.h` to enable/disable debug mode (which makes your ROM smaller if disabled), as well as configure other aspects of the library. The library features some basic debug functions and, if you are using libultra, two threads: one that handles all USB calls, and another that catches `OS_EVENT_FAULT` events and dumps registers through USB. The library runs in its own thread, it blocks the thread that called a debug function until it is finished reading/writing to the USB. If you are using libdragon, the library will instead halt the program until it is finished reading/writing to the USB.

Similar to the USB library, you must call poll for incoming USB data. This can be done in two ways, either by manually calling `debug_pollcommands` once per game loop, or you can set the `AUTOPOLL_ENABLED` macro to `1` so that it is done automatically at an interval of `AUTOPOLL_TIME` milliseconds. Changes to the 64Drive's button state are only detected during said polling.
<details><summary>Included functions list</summary>
<p>
    
```c
/*==============================
    debug_initialize
    Initializes the debug and USB library.
    Should be called last during game initialization.
==============================*/
void debug_initialize();

/*==============================
    debug_printf
    Prints a formatted message to the developer's command prompt.
    Supports up to 256 characters.
    @param A string to print
    @param variadic arguments to print as well
==============================*/
void debug_printf(const char* message, ...);

/*==============================
    debug_dumpbinary
    Dumps a binary file through USB
    @param The file to dump
    @param The size of the file
==============================*/
void debug_dumpbinary(void* file, int size);

/*==============================
    debug_screenshot
    Sends the currently displayed framebuffer through USB.
    DOES NOT PAUSE DRAWING THREAD! Using outside the drawing
    thread may lead to a screenshot with visible tearing
==============================*/
void debug_screenshot();

/*==============================
    debug_assert
    Halts the program if the expression fails.
    @param The expression to test
==============================*/
#define debug_assert(expr);
        
/*==============================
    debug_64drivebutton
    Assigns a function to be executed when the 64drive button is pressed.
    @param The function pointer to execute
    @param Whether or not to execute the function only on pressing (ignore holding the button down)
==============================*/
void debug_64drivebutton(void(*execute)(), char onpress);

/*==============================
    debug_pollcommands
    Check the USB for incoming commands.
==============================*/
void debug_pollcommands();

/*==============================
    debug_addcommand
    Adds a command for the USB to read.
    @param The command name
    @param The command description
    @param The function pointer to execute                                                                                  
==============================*/
void debug_addcommand(char* command, char* description, char*(*execute)());

/*==============================
    debug_parsecommand
    Stores the next part of the incoming command into the provided buffer.
    Make sure the buffer can fit the amount of data from debug_sizecommand!
    If you pass NULL, it skips this command.
    @param The buffer to store the data in
==============================*/
void debug_parsecommand(void* buffer);

/*==============================
    debug_sizecommand
    Returns the size of the data from this part of the command.
    @return The size of the data in bytes, or 0
==============================*/
int debug_sizecommand();

/*==============================
    debug_printcommands
    Prints a list of commands to the developer's command prompt.
==============================*/
void debug_printcommands();
```
</p>
</details>
</br>

### How these libraries work
I recommend developers check out the [wiki](../../../wiki) chapters 1 and 2 to get a full understanding of the communication protocol. The debug library abstracts this information away as much as possible, so if you didn't fully understand what was in those pages it's not a big concern. A summary of the most important tidbits is provided here:
<details><summary>USB Library</summary>
<p>

**General**

* Due to the data header, a maximum of 8MB can be sent through USB in a single `usb_write` call.
* By default, the USB Buffers are located on the 63MB area in SDRAM, which means that it will overwrite ROM if your game is larger than 63MB. More space can be allocated by changing `usb.h`.
* Avoid using `usb_write` while there is data that needs to be read from the USB first, as this will cause lockups for 64Drive users and will potentially overwrite the USB buffers on the EverDrive. Use `usb_poll` to check if there is data left to service. If you are using the debug library, this is handled for you.


**64Drive**

* All data through USB is 4 byte aligned. This might result in up to 3 extra bytes being sent/received through USB, which will be padded with zeroes.


**EverDrive**

* All data through USB is 2 byte aligned. This might result in 1 extra byte being sent/received through USB, which will be padded with zeroes.


**SC64**

\<Nothing>

</p>
</details>

<details><summary>Debug Library</summary>
<p>

* The debug library runs on a dedicated thread, which will only execute if invoked by debug commands. All threads will be blocked until the USB thread is finished. Libdragon does not have threads, so instead it'll block the entire program.
* Incoming USB data must be serviced first before you are able to write to USB. Every time a debug function is used, the library will first ensure there is no data to service before continuing. This means that incoming USB data **will only be read if a debug function is called**. Therefore, it is recommended to call `debug_pollcommands` as often as possible to ensure that data doesn't stay stuck waiting to be serviced. See Example 3 or 4 for examples on how to read incoming data.
</p>
</details>
</br>
