#include <stdlib.h>
#include <string.h>
#include "device_usb.h"

#ifdef _WIN64
    #include "Include/ftd2xx.h"
    #pragma comment(lib, "Include/FTD2XX_x64.lib")
#else
    #include <ftdi.h>
    ftdi_context* context = NULL;
    ftdi_device_list* devlist[5] = {NULL};
    uint32_t products_ftdi[5] = {0x6001,0x6010,0x6011,0x6014,0x6015};
#endif

USBStatus device_usb_createdeviceinfolist(uint32_t* num_devices)
{
    #ifdef _WIN64
        return FT_CreateDeviceInfoList((LPDWORD)num_devices);
    #else
        // TODO: Handle status
        int i;
        int status = USB_OK;
        int count = 0;
        struct ftdi_device_list *curdev;
        if (context == NULL)
            context = ftdi_new();
        if (devlist[0] == NULL)
            for (i=0; i<5; i++)
                ftdi_usb_find_all(context, &devlist[i], 0x0403, products_ftdi[i]); // 0:0 isn't working, so for loop it is...
        for (i=0; i<5; i++)
            for (curdev = devlist[i]; curdev != NULL; curdev = curdev->next)
                if (curdev->dev != NULL)
                    count++;
        if (count < 0)
            status = USB_DEVICE_LIST_NOT_READY;
        (*num_devices) = count;
        return status;
    #endif
}

USBStatus device_usb_getdeviceinfolist(USB_DeviceInfoListNode* list, uint32_t* num_devices)
{
    #ifdef _WIN64
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
        int i = 0;
        int count = 0;
        for (i=0; i<5; i++)
        {
            struct ftdi_device_list *curdev;
            for (curdev = devlist[i]; curdev != NULL; curdev = curdev->next)
            {
                char manufacturer[16], description[64], id[16];
                if (ftdi_usb_get_strings(context, curdev->dev, manufacturer, 16, description, 64, id, 16) < 0)
                    return USB_DEVICE_NOT_OPENED;
                list[count].flags = 0;
                list[count].type = 0;
                list[count].id = (0x0403 << 16) | products_ftdi[i];
                list[count].locid = 0;
                memcpy(&list[count].serial, manufacturer, sizeof(char)*16);
                memcpy(&list[count].description, description, sizeof(char)*64);
                count++;
            }
        }
        return USB_OK;
    #endif
}

USBStatus device_usb_open(int32_t devnumber, USBHandle* handle)
{
    #ifdef _WIN64
        return FT_Open(devnumber, handle);
    #else
        // TODO: Handle status
        int i = 0;
        int devcount = 0;
        for (i=0; i<5; i++)
        {
            int curprodnum = 0;
            struct ftdi_device_list *curdev;
            for (curdev = devlist[i]; curdev != NULL; curdev = curdev->next)
            {
                if (curdev->dev != NULL)
                {
                    if (devcount == devnumber)
                    {
                        ftdi_usb_open_desc_index(context, 0x0403, products_ftdi[i], NULL, NULL, curprodnum);
                    }
                    devcount++;
                    curprodnum++;
                }
            }
        }
        
        (*handle) = (void*)context;
        return USB_OK;
    #endif 
}

USBStatus device_usb_close(USBHandle handle)
{
    #ifdef _WIN64
        return FT_Close(handle);
    #else
        // TODO: Handle status
        ftdi_usb_close((ftdi_context*)handle);
        return USB_OK;
    #endif
}

USBStatus device_usb_write(USBHandle handle, void* buffer, uint32_t size, uint32_t* written)
{
    #ifdef _WIN64
        return FT_Write(handle, buffer, size, (LPDWORD)written);
    #else
        // TODO: Handle status
        (*written) = ftdi_write_data((ftdi_context*)handle, (unsigned char*)buffer, size);
        return USB_OK;
    #endif
}

USBStatus device_usb_read(USBHandle handle, void* buffer, uint32_t size, uint32_t* read)
{
    #ifdef _WIN64
        return FT_Read(handle, buffer, size, (LPDWORD)read);
    #else
        // TODO: Handle status
        (*read) = ftdi_read_data((ftdi_context*)handle, (unsigned char*)buffer, size);
        return USB_OK;
    #endif
}

USBStatus device_usb_getqueuestatus(USBHandle handle, uint32_t* bytesleft)
{
    #ifdef _WIN64
        return FT_GetQueueStatus(handle, (DWORD*)bytesleft);
    #else
        // TODO: Doesn't exist in libfdti?
        return USB_OK;
    #endif
}

USBStatus device_usb_resetdevice(USBHandle handle)
{
    #ifdef _WIN64
        return FT_ResetDevice(handle);
    #else
        // TODO: Handle status
        ftdi_usb_reset((ftdi_context*)handle);
        return USB_OK;
    #endif
}

USBStatus device_usb_settimeouts(USBHandle handle, uint32_t readtimout, uint32_t writetimout)
{
    #ifdef _WIN64
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
    #ifdef _WIN64
        return FT_SetBitMode(handle, mask, enable);
    #else
        // TODO: Handle status
        int i;
        uint8_t modes_d2xx[8] = {
            USB_BITMODE_RESET,
            USB_BITMODE_ASYNC_BITBANG,
            USB_BITMODE_MPSSE,
            USB_BITMODE_SYNC_BITBANG,
            USB_BITMODE_MCU_HOST,
            USB_BITMODE_FAST_SERIAL,
            USB_BITMODE_CBUS_BITBANG,
            USB_BITMODE_SYNC_FIFO,
        };
        uint8_t modes_libftdi[8] = {
            BITMODE_RESET,
            BITMODE_BITBANG,
            BITMODE_MPSSE,
            BITMODE_SYNCBB,
            BITMODE_MCU,
            BITMODE_OPTO,
            BITMODE_CBUS,
            BITMODE_SYNCFF,
        };
        uint8_t converted = 0;
        for (i=0; i<8; i++)
            if (enable & modes_d2xx[i])
                converted |= modes_libftdi[i];
        ftdi_set_bitmode((ftdi_context*)handle, mask, converted);
        return USB_OK;
    #endif
}

USBStatus device_usb_purge(USBHandle handle, uint32_t mask)
{
    #ifdef _WIN64
        return FT_Purge(handle, mask);
    #else
        // TODO: Handle status
        if (mask & USB_PURGE_RX)
            ftdi_usb_purge_rx_buffer((ftdi_context*)handle);
        if (mask & USB_PURGE_TX)
            ftdi_usb_purge_tx_buffer((ftdi_context*)handle);
        return USB_OK;
    #endif
}

USBStatus device_usb_getmodemstatus(USBHandle handle, uint32_t* modemstatus)
{
    #ifdef _WIN64
        return FT_GetModemStatus(handle, (ULONG*)modemstatus);
    #else
        return USB_OK;
    #endif
}

USBStatus device_usb_setdtr(USBHandle handle)
{
    #ifdef _WIN64
        return FT_SetDtr(handle);
    #else
        // TODO: Handle status
        ftdi_setdtr((ftdi_context*)handle, 1);
        return USB_OK;
    #endif
}

USBStatus device_usb_cleardtr(USBHandle handle)
{
    #ifdef _WIN64
        return FT_ClrDtr(handle);
    #else
        // TODO: Handle status
        ftdi_setdtr((ftdi_context*)handle, 0);
        return USB_OK;
    #endif
}