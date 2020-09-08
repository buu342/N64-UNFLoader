#ifndef UNFL_USB_H
#define UNFL_USB_H
    
    /*********************************
             DataType macros
    *********************************/

    // Settings
    #define USE_OSRAW 0 // Use if you're doing USB operations without the PI Manager
    
    // Data types defintions
    #define DATATYPE_TEXT       0x01
    #define DATATYPE_RAWBINARY  0x02
    #define DATATYPE_HEADER     0x03
    #define DATATYPE_SCREENSHOT 0x04
    

    /*********************************
              USB Functions
    *********************************/

    /*==============================
        usb_initialize
        Initializes the USB buffers and pointers
    ==============================*/
    
    extern void usb_initialize();
    
    
    /*==============================
        usb_write
        Writes data to the USB.
        @param The DATATYPE that is being sent
        @param A buffer with the data to send
        @param The size of the data being sent
    ==============================*/
    
    extern void usb_write(int datatype, const void* data, int size);
    
    
    /*==============================
        usb_poll
        Unimplemented!
        Checks how many bytes are in the USB buffer.
        Only tells you the bytes left on a per command basis!
        @return The number of bytes left to read
    ==============================*/
    
    extern int  usb_poll();
    
    
    /*==============================
        usb_read
        Unimplemented!
        Reads bytes from the USB into the provided buffer
        @param The buffer to put the read data in
        @param The number of bytes to read
        @return 1 if success, 0 otherwise
    ==============================*/
    
    extern u8   usb_read(void* buffer, int size);

#endif