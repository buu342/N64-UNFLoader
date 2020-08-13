#ifndef UNFL_USB_H
#define UNFL_USB_H

    /*********************************
             DataType macros
    *********************************/

    // Data types defintions
    #define DATATYPE_TEXT       0x01
    #define DATATYPE_RAWBINARY  0x02
    #define DATATYPE_SCREENSHOT 0x03
    

    /*********************************
              USB Functions
    *********************************/

    extern void usb_write(int datatype, const void* data, int size);
    extern u8   usb_read();

#endif