#include <stdlib.h>
#include <string.h>
#include "device_usb.h"
#ifdef D2XX
    #include "Include/ftd2xx.h"
#else
    #include <libusb-1.0/libusb.h>
    #include <libftdi1/ftdi.h>
#endif


/*********************************
              Macros
*********************************/

#ifndef D2XX
    #define BUFFER_SIZE 8*1024*1024
#endif


/*********************************
         Global Variables
*********************************/

#ifndef D2XX
    ftdi_context* context = NULL;
    ftdi_device_list* devlist = NULL;
    uint8_t* readbuffer = NULL;
    uint32_t readbuffer_left = 0;
    uint32_t readbuffer_readoffset = 0;
    uint32_t readbuffer_copyoffset = 0;
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
    #ifdef D2XX
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
    #ifdef D2XX
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
    #ifdef D2XX
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
        ftdi_set_latency_timer(context, 1);
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
    #ifdef D2XX
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
    #ifdef D2XX
        uint32_t totalwritten = 0;
        while (totalwritten < size)
        {
            uint32_t curwrite;
            FT_STATUS ret = FT_Write(handle, ((uint8_t*)buffer)+totalwritten, size-totalwritten, (LPDWORD)&curwrite);
            totalwritten += curwrite;
            if (ret != FT_OK)
            {
                (*written) = totalwritten;
                return ret;
            }
        }
        (*written) = totalwritten;
        return USB_OK;
    #else
        uint32_t totalwritten = 0;
        while (totalwritten < size)
        {
            int ret = ftdi_write_data((ftdi_context*)handle, ((unsigned char*)buffer)+totalwritten, size-totalwritten);
            if (ret == -666)
            {
                (*written) = totalwritten;
                return USB_DEVICE_NOT_FOUND;
            }
            else if (ret < 0)
            {
                (*written) = totalwritten;
                return USB_IO_ERROR;
            }
            totalwritten += ret;
        }
        (*written) = totalwritten;
        return USB_OK;
    #endif
}


/*==============================
    device_usb_read
    Reads data from a USB device, blocking until finished
    @param  The USB handle to use
    @param  The buffer to read into
    @param  The size of the data to read
    @param  A pointer to store the number of bytes read
    @return The USB status
==============================*/

USBStatus device_usb_read(USBHandle handle, void* buffer, uint32_t size, uint32_t* read)
{
    #ifdef D2XX
        uint32_t totalread = 0;
        while (totalread < size)
        {
            uint32_t curread;
            FT_STATUS ret = FT_Read(handle, ((uint8_t*)buffer)+totalread, size-totalread, (LPDWORD)&curread);
            totalread += curread;
            if (ret != FT_OK)
            {
                (*read) = totalread;
                return ret;
            }
        }
        (*read) = totalread;
        return USB_OK;
    #else
        uint32_t readcount = size;

        // If we're being asked to read more data than we have in our buffer, check if the USB can give us more
        while (readcount > readbuffer_left)
            device_usb_getqueuestatus(handle, NULL);

        // Copy the data
        if (readcount > readbuffer_left)
            readcount = readbuffer_left;
        memcpy(buffer, readbuffer+readbuffer_readoffset, readcount);
        readbuffer_left -= readcount;
        (*read) = readcount;

        // If we have no data left to read, we can safetly reset the buffer position
        if (readbuffer_left == 0)
        {
            readbuffer_readoffset = 0;
            readbuffer_copyoffset = 0;
        }
        else
            readbuffer_readoffset += readcount;
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
    #ifdef D2XX
        return FT_GetQueueStatus(handle, (DWORD*)bytesleft);
    #else
        // Perform a USB read to see how much data is in the actual USB buffer (since libftdi doesn't provide a way to poll)
        int ret = ftdi_read_data((ftdi_context*)handle, readbuffer+readbuffer_copyoffset, BUFFER_SIZE-readbuffer_copyoffset);
        if (ret == -666)
            return USB_DEVICE_NOT_FOUND;
        else if (ret < 0)
            return USB_IO_ERROR;

        // Add how much we have left to read
        readbuffer_left += ret;
        readbuffer_copyoffset += ret;

        // Done
        if (bytesleft != NULL)
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
    #ifdef D2XX
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
    #ifdef D2XX
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
    #ifdef D2XX
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
    #ifdef D2XX
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
    #ifdef D2XX
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
    #ifdef D2XX
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
    #ifdef D2XX
        return FT_ClrDtr(handle);
    #else
        if (ftdi_setdtr((ftdi_context*)handle, 0) < 0)
            return USB_OTHER_ERROR;
        return USB_OK;
    #endif
}
