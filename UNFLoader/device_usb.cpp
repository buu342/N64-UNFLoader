#include <stdlib.h>
#include <string.h>
#include "device_usb.h"

#ifndef LINUX
    #include "Include/ftd2xx.h"
#else
    #include <libusb-1.0/libusb.h>
    #include <libftdi1/ftdi.h>
    #include <thread>
    #include <chrono>

    #define BUFFER_SIZE 8*1024*1024

    ftdi_context* context = NULL;
    ftdi_device_list* devlist = NULL;
    uint8_t* readbuffer = NULL;
    uint32_t readbuffer_left = 0;
    uint32_t readbuffer_offset = 0;
#endif

USBStatus device_usb_createdeviceinfolist(uint32_t* num_devices)
{
    #ifndef LINUX
        return FT_CreateDeviceInfoList((LPDWORD)num_devices);
    #else
        // TODO: Handle status
        int count;
        int status = USB_OK;
        if (context == NULL)
            context = ftdi_new();
        if (readbuffer == NULL)
        {
            readbuffer = (uint8_t*)malloc(BUFFER_SIZE);
            if (readbuffer == NULL)
                return USB_INSUFFICIENT_RESOURCES;
        }
        if (devlist != NULL)
            ftdi_list_free(&devlist);
        count = ftdi_usb_find_all(context, &devlist, 0, 0);
        if (count < 0)
            status = USB_DEVICE_LIST_NOT_READY;
        (*num_devices) = count;
        return status;
    #endif
}

USBStatus device_usb_getdeviceinfolist(USB_DeviceInfoListNode* list, uint32_t* num_devices)
{
    #ifndef LINUX
        uint32_t i;
        USBStatus stat;
        FT_DEVICE_LIST_INFO_NODE* ftdidevs = (FT_DEVICE_LIST_INFO_NODE*)malloc(sizeof(FT_DEVICE_LIST_INFO_NODE)*(*num_devices));
        stat = FT_GetDeviceInfoList(ftdidevs, (LPDWORD)num_devices);
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
        free(ftdidevs);
        return stat;
    #else
        // TODO: Handle status
        int count = 0;
        ftdi_device_list* curdev;
        for (curdev = devlist; curdev != NULL; curdev = curdev->next)
        {
            libusb_device* dev = curdev->dev;
            struct libusb_device_descriptor desc;
            char manufacturer[16], description[64], id[16];

            if (ftdi_usb_get_strings(context, curdev->dev, manufacturer, 16, description, 64, id, 16) < 0)
                return USB_DEVICE_NOT_OPENED;
            if (libusb_get_device_descriptor(dev, &desc) < 0)
                return USB_DEVICE_NOT_OPENED;

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

USBStatus device_usb_open(int32_t devnumber, USBHandle* handle)
{
    #ifndef LINUX
        return FT_Open(devnumber, handle);
    #else
        // TODO: Handle status
        int curdev_index = 0;
        ftdi_device_list* curdev = devlist;
        while (curdev_index < devnumber)
            curdev = curdev->next;
        ftdi_usb_open_dev(context, devlist[0].dev);
        (*handle) = (void*)context;
        return USB_OK;
    #endif 
}

USBStatus device_usb_close(USBHandle handle)
{
    #ifndef LINUX
        return FT_Close(handle);
    #else
        // TODO: Handle status
        ftdi_usb_close((ftdi_context*)handle);
        return USB_OK;
    #endif
}

USBStatus device_usb_write(USBHandle handle, void* buffer, uint32_t size, uint32_t* written)
{
    #ifndef LINUX
        return FT_Write(handle, buffer, size, (LPDWORD)written);
    #else
        // TODO: Handle status
        int ret;
        USBStatus status = USB_OK;
        ret = ftdi_write_data((ftdi_context*)handle, (unsigned char*)buffer, size);
        if (ret == -666)
            status = USB_DEVICE_NOT_FOUND;
        else if (ret < 0)
            status = USB_IO_ERROR;
        (*written) = ret;
        return status;
    #endif
}

USBStatus device_usb_read(USBHandle handle, void* buffer, uint32_t size, uint32_t* read)
{
    #ifndef LINUX
        return FT_Read(handle, buffer, size, (LPDWORD)read);
    #else
        // TODO: Handle status
        uint32_t min;
        if (readbuffer_left == 0)
        {
            uint32_t bytesleft = 0;
            USBStatus status = USB_OK;
            int attempts = 4;
            while (bytesleft == 0 && attempts != 0)
            {
                attempts--;
                status = device_usb_getqueuestatus(handle, &bytesleft);
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            if (status != USB_OK)
                return status;
        }

        min = readbuffer_left;
        if (min > size)
            min = size;
        memcpy(buffer, readbuffer + readbuffer_offset, min);
        (*read) = min;
        readbuffer_left -= min;
        readbuffer_offset += min;
        return USB_OK;
    #endif
}

USBStatus device_usb_getqueuestatus(USBHandle handle, uint32_t* bytesleft)
{
    #ifndef LINUX
        return FT_GetQueueStatus(handle, (DWORD*)bytesleft);
    #else
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

USBStatus device_usb_resetdevice(USBHandle handle)
{
    #ifndef LINUX
        return FT_ResetDevice(handle);
    #else
        // TODO: Handle status
        ftdi_usb_reset((ftdi_context*)handle);
        return USB_OK;
    #endif
}

USBStatus device_usb_settimeouts(USBHandle handle, uint32_t readtimout, uint32_t writetimout)
{
    #ifndef LINUX
        return FT_SetTimeouts(handle, readtimout, writetimout);
    #else
        // TODO: Am I doing this right??? There's no timeout setting functions
        ((ftdi_context*)handle)->usb_read_timeout = readtimout;
        ((ftdi_context*)handle)->usb_write_timeout = writetimout;
        return USB_OK;
    #endif
}

USBStatus device_usb_setbitmode(USBHandle handle, uint8_t mask, uint8_t enable)
{
    #ifndef LINUX
        return FT_SetBitMode(handle, mask, enable);
    #else
        // TODO: Handle status
        ftdi_set_bitmode((ftdi_context*)handle, mask, enable);
        return USB_OK;
    #endif
}

USBStatus device_usb_purge(USBHandle handle, uint32_t mask)
{
    #ifndef LINUX
        return FT_Purge(handle, mask);
    #else
        // TODO: Handle status
        if (mask & USB_PURGE_RX)
            ftdi_tciflush((ftdi_context*)handle);
        if (mask & USB_PURGE_TX)
            ftdi_tcoflush((ftdi_context*)handle);
        return USB_OK;
    #endif
}

USBStatus device_usb_getmodemstatus(USBHandle handle, uint32_t* modemstatus)
{
    #ifndef LINUX
        return FT_GetModemStatus(handle, (ULONG*)modemstatus);
    #else
        return USB_OK;
    #endif
}

USBStatus device_usb_setdtr(USBHandle handle)
{
    #ifndef LINUX
        return FT_SetDtr(handle);
    #else
        // TODO: Handle status
        ftdi_setdtr((ftdi_context*)handle, 1);
        return USB_OK;
    #endif
}

USBStatus device_usb_cleardtr(USBHandle handle)
{
    #ifndef LINUX
        return FT_ClrDtr(handle);
    #else
        // TODO: Handle status
        ftdi_setdtr((ftdi_context*)handle, 0);
        return USB_OK;
    #endif
}
