#ifndef __DEVICE_USB_HEADER
#define __DEVICE_USB_HEADER

    #include <stdint.h>

    #define USB_PURGE_RX  1
    #define USB_PURGE_TX  2

    #define USB_BITMODE_RESET          0x00
    #define USB_BITMODE_ASYNC_BITBANG  0x01
    #define USB_BITMODE_MPSSE          0x02
    #define USB_BITMODE_SYNC_BITBANG   0x04
    #define USB_BITMODE_MCU_HOST       0x08
    #define USB_BITMODE_FAST_SERIAL    0x10
    #define USB_BITMODE_CBUS_BITBANG   0x20
    #define USB_BITMODE_SYNC_FIFO      0x40

    enum {
        USB_OK,
        USB_INVALID_HANDLE,
        USB_DEVICE_NOT_FOUND,
        USB_DEVICE_NOT_OPENED,
        USB_IO_ERROR,
        USB_INSUFFICIENT_RESOURCES,
        USB_INVALID_PARAMETER,
        USB_INVALID_BAUD_RATE,
        USB_DEVICE_NOT_OPENED_FOR_ERASE,
        USB_DEVICE_NOT_OPENED_FOR_WRITE,
        USB_FAILED_TO_WRITE_DEVICE,
        USB_EEPROM_READ_FAILED,
        USB_EEPROM_WRITE_FAILED,
        USB_EEPROM_ERASE_FAILED,
        USB_EEPROM_NOT_PRESENT,
        USB_EEPROM_NOT_PROGRAMMED,
        USB_INVALID_ARGS,
        USB_NOT_SUPPORTED,
        USB_OTHER_ERROR,
        USB_DEVICE_LIST_NOT_READY,
    };

    typedef void*    USBHandle;
    typedef uint32_t USBStatus;

    typedef struct {
        uint32_t flags;
        uint32_t type;
        uint32_t id;
        uint32_t locid;
        char serial[16];
        char description[64];
        USBHandle handle;
    } USB_DeviceInfoListNode;


    USBStatus device_usb_createdeviceinfolist(uint32_t* num_devices);
    USBStatus device_usb_getdeviceinfolist(USB_DeviceInfoListNode* list, uint32_t* num_devices);

    USBStatus device_usb_open(int32_t devnumber, USBHandle handle);
    USBStatus device_usb_close(USBHandle handle);

    USBStatus device_usb_write(USBHandle handle, USBHandle buffer, uint32_t size, uint32_t* written);
    USBStatus device_usb_read(USBHandle handle, USBHandle buffer, uint32_t size, uint32_t* read);
    USBStatus device_usb_getqueuestatus(USBHandle handle, uint32_t* bytesleft);

    USBStatus device_usb_resetdevice(USBHandle handle);
    USBStatus device_usb_settimeouts(USBHandle handle, uint32_t readtimout, uint32_t writetimout);
    USBStatus device_usb_setbitmode(USBHandle handle, uint8_t mask, uint8_t enable);
    USBStatus device_usb_purge(USBHandle handle, uint32_t mask);

#endif