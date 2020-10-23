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
            Convenience macros
    *********************************/
    
    #define USBHEADER_GETTYPE(header) ((header & 0xFF000000) >> 24)
    #define USBHEADER_GETSIZE(header) ((header & 0x00FFFFFF))
    #define USBHEADER_CREATE(type, left) (((type<<24) | (left & 0x00FFFFFF)))
    

    /*********************************
              USB Functions
    *********************************/

    /*==============================
        usb_initialize
        Initializes the USB buffers and pointers
        @return 1 if the USB initialization was successful, 0 if not
    ==============================*/
    
    extern char usb_initialize();
    
    
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
        Returns the header of data being received via USB
        The first byte contains the data type, the next 3 the size left to read
        @return The data header, or 0
    ==============================*/
    
    extern int usb_poll();
    
    
    /*==============================
        usb_read
        Reads bytes from the USB into the provided buffer
        @param The buffer to put the read data in
        @param The number of bytes to read
    ==============================*/
    
    extern void usb_read(void* buffer, int size);

#endif