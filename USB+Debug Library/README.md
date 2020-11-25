# USB + Debug Library
This folder contains both the USB and debug library that works in tandem with UNFLoader's communication protocol. The libraries currently only target libultra.

### Table of Contents
* [How to use the USB library](#how-to-use-the-usb-library)
* [How to use the Debug library](#how-to-use-the-debug-library)

### How to use the USB library
Simply include the `usb.c` and `usb.h` in your project. You must call `usb_initialize()` once before doing anything else. The library features a read and write function for USB communication. You can edit `usb.h` to configure some aspects of the library.
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

// Use these to conveniently read the header from usb_poll()
#define USBHEADER_GETTYPE(header)
#define USBHEADER_GETSIZE(header)
```
</p>
</details>

### How to use the Debug library
The debug library is a basic practical implementation of the USB library. Simply include the `debug.c` and `debug.h` in your project (along with the usb library). You must call `debug_initialize()` once before doing anything else. If you are using this library, there is no need to worry about anything regarding the USB library as this one takes care of everything for you (initialization, includes, etc...). You can edit `debug.h` to enable/disable debug mode (which makes your ROM smaller if disabled), as well as configure other aspects of the library. The library features some basic debug functions and two threads: one that handles all USB calls, and another that catches `OS_EVENT_FAULT` events and dumps registers through USB. The library runs in its own thread, it blocks the thread that called a debug function until it is finished reading/writing to the USB.
<details><summary>Included functions list</summary>
<p>
    
```c
/*==============================
    debug_initialize
    Initializes the debug and USB library.
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
    debug_screenshot
    Sends the currently displayed framebuffer through USB.
    @param The size of each pixel of the framebuffer in bytes
           Typically 4 if 32-bit or 2 if 16-bit
    @param The width of the framebuffer
    @param The height of the framebuffer
==============================*/
void debug_screenshot(int size, int w, int h);

/*==============================
    debug_assert
    Halts the program if the expression fails.
    @param The expression to test
==============================*/
#define debug_assert(expr)

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