#include <stdlib.h>
#include <string.h>
#include "device_usb.h"
#ifndef LINUX
    #include "Include/ftd2xx.h"
#else
    #include <libusb-1.0/libusb.h>
    #include <libftdi1/ftdi.h>
#endif


/*********************************
              Macros
*********************************/

#ifdef LINUX
    #define BUFFER_SIZE 8*1024*1024
#endif


/*********************************
         Global Variables
*********************************/

#ifdef LINUX
    ftdi_context* context = NULL;
    ftdi_device_list* devlist = NULL;
    uint8_t* readbuffer = NULL;
    uint32_t readbuffer_left = 0;
    uint32_t readbuffer_offset = 0;
#endif


/*********************************
         Global Variables
*********************************/

/*==============================
    device_usb_createdeviceinfolist
    Creates a list of known devices
    @param  A pointer to an integer to fill with the number of detected devices
    @return The USB status
==============================*/

USBStatus device_usb_createdeviceinfolist(uint32_t* num_devices)
{
    #ifndef LINUX
        return FT_CreateDeviceInfoList((LPDWORD)num_devices);
    #else
        int count;
        int status = USB_OK;

        // Initialize FTDI
        if (context == NULL)
            context = ftdi_new();

        // Initialize the usb_read buffer
        if (readbuffer == NULL)
        {
            readbuffer = (uint8_t*)malloc(BUFFER_SIZE);
            if (readbuffer == NULL)
                return USB_INSUFFICIENT_RESOURCES;
        }

        // Initialize the device list and store the number of devices in the pointer
        if (devlist != NULL)
            ftdi_list_free(&devlist);
        count = ftdi_usb_find_all(context, &devlist, 0, 0);
        if (count < 0)
            status = USB_DEVICE_LIST_NOT_READY;
        else
            (*num_devices) = count;
        return status;
    #endif
}


/*==============================
    device_usb_getdeviceinfolist
    Populates a list of known devices
    @param  A pointer to a list of USB device descriptions to fill
    @param  A pointer to an integer with the number of detected devices
    @return The USB status
==============================*/

USBStatus device_usb_getdeviceinfolist(USB_DeviceInfoListNode* list, uint32_t* num_devices)
{
    #ifndef LINUX
        uint32_t i;
        USBStatus status;
        FT_DEVICE_LIST_INFO_NODE* ftdidevs = (FT_DEVICE_LIST_INFO_NODE*)malloc(sizeof(FT_DEVICE_LIST_INFO_NODE)*(*num_devices));
        if (ftdidevs == NULL)
            return USB_INSUFFICIENT_RESOURCES;
        status = FT_GetDeviceInfoList(ftdidevs, (LPDWORD)num_devices);

        // Fill in our USB data structures
        for (i=0; i<(*num_devices); i++)
        {
            list[i].flags = ftdidevs[i].Flags;
            list[i].type = ftdidevs[i].Type;
            list[i].id = ftdidevs[i].ID;
            list[i].locid = ftdidevs[i].LocId;
            memcpy(&list[i].serial, ftdidevs[i].SerialNumber, sizeof(char)*16);
            memcpy(&list[i].description, ftdidevs[i].Description, sizeof(char)*64);
            list[i].handle = ftdidevs[i].ftHandle;
        }

        // Cleanup
        free(ftdidevs);
        return status;
    #else
        int count = 0;
        ftdi_device_list* curdev;
        for (curdev = devlist; curdev != NULL; curdev = curdev->next)
        {
            libusb_device* dev = curdev->dev;
            struct libusb_device_descriptor desc;
            char manufacturer[16], description[64], id[16];

            // Get the device strings
            if (ftdi_usb_get_strings(context, curdev->dev, manufacturer, 16, description, 64, id, 16) < 0)
                return USB_DEVICE_NOT_OPENED;
            if (libusb_get_device_descriptor(dev, &desc) < 0)
                return USB_DEVICE_NOT_OPENED;

            // Fill in our USB data structure
            list[count].flags = 0;
            list[count].type = 0;
            list[count].id = (0x0403 << 16) | desc.idProduct;
            list[count].locid = 0;
            memcpy(&list[count].serial, manufacturer, sizeof(char)*16);
            memcpy(&list[count].description, description, sizeof(char)*64);

            count++;
        }
        return USB_OK;
    #endif
}


/*==============================
    device_usb_open
    Opens a USB device
    @param  The device number to open
    @param  The USB handle to use
    @return The USB status
==============================*/

USBStatus device_usb_open(int32_t devnumber, USBHandle* handle)
{
    #ifndef LINUX
        return FT_Open(devnumber, handle);
    #else
        int curdev_index = 0;
        ftdi_device_list* curdev = devlist;

        // Find the device we want to open
        while (curdev_index < devnumber)
            curdev = curdev->next;

        // Open the device
        if (ftdi_usb_open_dev(context, devlist[0].dev) < 0)
            return USB_DEVICE_NOT_OPENED;
        (*handle) = (void*)context;
        return USB_OK;
    #endif 
}


/*==============================
    device_usb_close
    Closes a USB device
    @param  The USB handle to use
    @return The USB status
==============================*/

USBStatus device_usb_close(USBHandle handle)
{
    #ifndef LINUX
        return FT_Close(handle);
    #else
        if (ftdi_usb_close((ftdi_context*)handle) < 0)
            return USB_INVALID_HANDLE;
        return USB_OK;
    #endif
}


/*==============================
    device_usb_write
    Writes data to a USB device
    @param  The USB handle to use
    @param  The buffer to use
    @param  The size of the data
    @param  A pointer to store the number of bytes written
    @return The USB status
==============================*/

USBStatus device_usb_write(USBHandle handle, void* buffer, uint32_t size, uint32_t* written)
{
    #ifndef LINUX
        return FT_Write(handle, buffer, size, (LPDWORD)written);
    #else
        int ret;
        USBStatus status = USB_OK;

        // Perform the write
        ret = ftdi_write_data((ftdi_context*)handle, (unsigned char*)buffer, size);

        // Handle errors
        if (ret == -666)
            status = USB_DEVICE_NOT_FOUND;
        else if (ret < 0)
            status = USB_IO_ERROR;
        else
            (*written) = ret;
        return status;
    #endif
}


/*==============================
    device_usb_read
    Reads data from a USB device
    @param  The USB handle to use
    @param  The buffer to read into
    @param  The size of the data to read
    @param  A pointer to store the number of bytes read
    @return The USB status
==============================*/

USBStatus device_usb_read(USBHandle handle, void* buffer, uint32_t size, uint32_t* read)
{
    #ifndef LINUX
        return FT_Read(handle, buffer, size, (LPDWORD)read);
    #else
        uint32_t min;

        // Because there's no way to poll for data in libftdi, the polling function actually performs the read into a buffer
        // This means that we must first check if our buffer has data, and if not, "poll" it
        if (readbuffer_left == 0)
        {
            uint32_t bytesleft = 0;
            int attempts = 4;

            // Make 4 attempts at a read. This is needed because libftdi is bad.
            while (bytesleft == 0 && attempts != 0)
            {
                USBStatus status = device_usb_getqueuestatus(handle, &bytesleft);
                if (status != USB_OK)
                    return status;
                attempts--;
            }
        }

        // Now that we have data in our buffer, memcpy it
        min = readbuffer_left;
        if (min > size)
            min = size;
        memcpy(buffer, readbuffer + readbuffer_offset, min);
        (*read) = min;

        // Increment the helper variables
        readbuffer_left -= min;
        readbuffer_offset += min;
        return USB_OK;
    #endif
}


/*==============================
    device_usb_getqueuestatus
    Checks how many bytes are in the rx buffer
    @param  The USB handle to use
    @param  A pointer to store the number of bytes in the queue
    @return The USB status
==============================*/

USBStatus device_usb_getqueuestatus(USBHandle handle, uint32_t* bytesleft)
{
    #ifndef LINUX
        return FT_GetQueueStatus(handle, (DWORD*)bytesleft);
    #else
        // If we have no bytes in our buffer, try to read some and update the helper variables
        // Otherwise, just return the number of bytes left to be processed by the user
        if (readbuffer_left == 0)
        {
            int ret = ftdi_read_data((ftdi_context*)handle, readbuffer, BUFFER_SIZE);
            if (ret == -666)
                return USB_DEVICE_NOT_FOUND;
            else if (ret < 0)
                return USB_IO_ERROR;
            readbuffer_left = ret;
            readbuffer_offset = 0;
            (*bytesleft) = ret;
        }
        else
            (*bytesleft) = readbuffer_left;
        return USB_OK;
    #endif
}


/*==============================
    device_usb_resetdevice
    Resets a USB device
    @param  The USB handle to use
    @return The USB status
==============================*/

USBStatus device_usb_resetdevice(USBHandle handle)
{
    #ifndef LINUX
        return FT_ResetDevice(handle);
    #else
        if (ftdi_usb_reset((ftdi_context*)handle) < 0)
            return USB_OTHER_ERROR;
        return USB_OK;
    #endif
}


/*==============================
    device_usb_settimeouts
    Sets the timeouts on a USB device
    @param  The USB handle to use
    @param  The read timeout
    @param  The write timeout
    @return The USB status
==============================*/

USBStatus device_usb_settimeouts(USBHandle handle, uint32_t readtimout, uint32_t writetimout)
{
    #ifndef LINUX
        return FT_SetTimeouts(handle, readtimout, writetimout);
    #else
        ((ftdi_context*)handle)->usb_read_timeout = readtimout;
        ((ftdi_context*)handle)->usb_write_timeout = writetimout;
        return USB_OK;
    #endif
}


/*==============================
    device_usb_setbitmode
    Sets bitmodes for the USB device
    @param  The USB handle to use
    @param  The bit mask
    @param  The bits to enable
    @return The USB status
==============================*/

USBStatus device_usb_setbitmode(USBHandle handle, uint8_t mask, uint8_t enable)
{
    #ifndef LINUX
        return FT_SetBitMode(handle, mask, enable);
    #else
        if (ftdi_set_bitmode((ftdi_context*)handle, mask, enable) < 0)
            return USB_OTHER_ERROR;
        return USB_OK;
    #endif
}


/*==============================
    device_usb_purge
    Purges the USB buffers
    @param  The USB handle to use
    @param  The bit mask
    @return The USB status
==============================*/

USBStatus device_usb_purge(USBHandle handle, uint32_t mask)
{
    #ifndef LINUX
        return FT_Purge(handle, mask);
    #else
        if (mask & USB_PURGE_RX)
            if (ftdi_tciflush((ftdi_context*)handle) < 0)
                return USB_OTHER_ERROR;
        if (mask & USB_PURGE_TX)
            if (ftdi_tcoflush((ftdi_context*)handle) < 0)
                return USB_OTHER_ERROR;
        return USB_OK;
    #endif
}


/*==============================
    device_usb_getmodemstatus
    Gets the USB modem status the USB buffers
    @param  The USB handle to use
    @param  A pointer to store the modem status into
    @return The USB status
==============================*/

USBStatus device_usb_getmodemstatus(USBHandle handle, uint32_t* modemstatus)
{
    #ifndef LINUX
        return FT_GetModemStatus(handle, (ULONG*)modemstatus);
    #else
        unsigned short tempstatus;
        if (ftdi_poll_modem_status((ftdi_context*)handle, &tempstatus) < 0)
            return USB_OTHER_ERROR;
        (*modemstatus) = tempstatus & 0x00FF;
        return USB_OK;
    #endif
}


/*==============================
    device_usb_setdtr
    Enables DTR on a USB device
    @param  The USB handle to use
    @return The USB status
==============================*/

USBStatus device_usb_setdtr(USBHandle handle)
{
    #ifndef LINUX
        return FT_SetDtr(handle);
    #else
        if (ftdi_setdtr((ftdi_context*)handle, 1) < 0)
            return USB_OTHER_ERROR;
        return USB_OK;
    #endif
}


/*==============================
    device_usb_cleardtr
    Disables DTR on a USB device
    @param  The USB handle to use
    @return The USB status
==============================*/

USBStatus device_usb_cleardtr(USBHandle handle)
{
    #ifndef LINUX
        return FT_ClrDtr(handle);
    #else
        if (ftdi_setdtr((ftdi_context*)handle, 0) < 0)
            return USB_OTHER_ERROR;
        return USB_OK;
    #endif
}
