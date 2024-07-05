#include <stdlib.h>
#include <string.h>
#include "device_usb.h"
#include "Include/ftd2xx.h"

#ifdef _WIN64
    #pragma comment(lib, "Include/FTD2XX_x64.lib")
#else
    
#endif

USBStatus device_usb_createdeviceinfolist(uint32_t* num_devices)
{
    return FT_CreateDeviceInfoList((LPDWORD)num_devices);
}

USBStatus device_usb_getdeviceinfolist(USB_DeviceInfoListNode* list, uint32_t* num_devices)
{
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
}

USBStatus device_usb_open(int32_t devnumber, USBHandle* handle)
{
    return FT_Open(devnumber, handle); 
}

USBStatus device_usb_close(USBHandle handle)
{
    return FT_Close(handle); 
}

USBStatus device_usb_write(USBHandle handle, void* buffer, uint32_t size, uint32_t* written)
{
    return FT_Write(handle, buffer, size, (LPDWORD)written);
}

USBStatus device_usb_read(USBHandle handle, void* buffer, uint32_t size, uint32_t* read)
{
    return FT_Read(handle, buffer, size, (LPDWORD)read);
}

USBStatus device_usb_getqueuestatus(USBHandle handle, uint32_t* bytesleft)
{
    return FT_GetQueueStatus(handle, (DWORD*)bytesleft);
}

USBStatus device_usb_resetdevice(USBHandle handle)
{
    return FT_ResetDevice(handle);
}

USBStatus device_usb_settimeouts(USBHandle handle, uint32_t readtimout, uint32_t writetimout)
{
    return FT_SetTimeouts(handle, readtimout, writetimout);
}

USBStatus device_usb_setbitmode(USBHandle handle, uint8_t mask, uint8_t enable)
{
    return FT_SetBitMode(handle, mask, enable);
}

USBStatus device_usb_purge(USBHandle handle, uint32_t mask)
{
    return FT_Purge(handle, mask);
}

USBStatus device_usb_getmodemstatus(USBHandle handle, uint32_t* modemstatus)
{
    return FT_GetModemStatus(handle, (ULONG*)modemstatus);
}

USBStatus device_usb_setdtr(USBHandle handle)
{
    return FT_SetDtr(handle);
}

USBStatus device_usb_cleardtr(USBHandle handle)
{
    return FT_ClrDtr(handle);
}