/***************************************************************
                       device_64drive.cpp

Handles 64Drive HW1 and HW2 USB communication. A lot of the code
here is courtesy of MarshallH's 64Drive USB tool:
http://64drive.retroactive.be/support.php
***************************************************************/

#include "device_64drive.h"
#include "device_usb.h"
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <thread>
#include <chrono>
#include <vector>

typedef struct 
{
    uint32_t  device_index;
    USBHandle handle;
    bool      synchronous;
    uint32_t  bytes_written;
    uint32_t  bytes_read;
} N64DriveHandle;

typedef struct
{
    byte* data;
    uint32_t header;
    uint32_t size;
} USBData;

static std::vector<int> global_ackswanted;
static std::vector<USBData> global_gotdata;


/*********************************
        Function Prototypes
*********************************/

static DeviceError device_sendcmd_64drive(N64DriveHandle* cart, uint8_t command, bool reply, uint32_t* result, uint32_t numparams, ...);


/*==============================
    device_test_64drive1
    Checks whether the device passed as an argument is 64Drive HW1
    @param  A pointer to the cart context
    @return DEVICEERR_OK if the cart is a 64Drive HW1, 
            DEVICEERR_NOTCART if it isn't,
            Any other device error if problems ocurred
==============================*/

DeviceError device_test_64drive1(CartDevice* cart)
{
    uint32_t device_count;
    USB_DeviceInfoListNode* device_info;

    // Initialize FTD
    if (device_usb_createdeviceinfolist(&device_count) != USB_OK)
        return DEVICEERR_USBBUSY;

    // Check if the device exists
    if (device_count == 0)
        return DEVICEERR_NODEVICES;

    // Allocate storage and get device info list
    device_info = (USB_DeviceInfoListNode*) malloc(sizeof(USB_DeviceInfoListNode)*device_count);
    device_usb_getdeviceinfolist(device_info, &device_count);

    // Search the devices
    for (uint32_t i=0; i<device_count; i++)
    {
        // Look for 64drive HW1 (FT2232H Asynchronous FIFO mode)
        if ((strcmp(device_info[i].description, "64drive USB device A") == 0 || strcmp(device_info[i].description, "64drive USB device") == 0) && device_info[i].id == 0x4036010)
        {
            N64DriveHandle* fthandle = (N64DriveHandle*) malloc(sizeof(N64DriveHandle));
            free(device_info);
            fthandle->device_index = i;
            fthandle->synchronous = false;
            cart->structure = fthandle;
            return DEVICEERR_OK;
        }
    }

    // Could not find the flashcart
    free(device_info);
    return DEVICEERR_NOTCART;
}


/*==============================
    device_test_64drive2
    Checks whether the device passed as an argument is 64Drive HW2
    @param  A pointer to the cart context
    @return DEVICEERR_OK if the cart is a 64Drive HW2, 
            DEVICEERR_NOTCART if it isn't,
            Any other device error if problems ocurred
==============================*/

DeviceError device_test_64drive2(CartDevice* cart)
{
    uint32_t device_count;
    USB_DeviceInfoListNode* device_info;

    // Initialize FTD
    if (device_usb_createdeviceinfolist(&device_count) != USB_OK)
        return DEVICEERR_USBBUSY;

    // Check if the device exists
    if (device_count == 0)
        return DEVICEERR_NODEVICES;

    // Allocate storage and get device info list
    device_info = (USB_DeviceInfoListNode*) malloc(sizeof(USB_DeviceInfoListNode)*device_count);
    device_usb_getdeviceinfolist(device_info, &device_count);

    // Search the devices
    for (uint32_t i=0; i<device_count; i++)
    {
        // Look for 64drive HW1 (FT2232H Asynchronous FIFO mode)
        if (strcmp(device_info[i].description, "64drive USB device") == 0 && device_info[i].id == 0x4036014)
        {
            N64DriveHandle* fthandle = (N64DriveHandle*) malloc(sizeof(N64DriveHandle));
            free(device_info);
            fthandle->device_index = i;
            fthandle->synchronous = true;
            cart->structure = fthandle;
            return DEVICEERR_OK;
        }
    }

    // Could not find the flashcart
    free(device_info);
    return DEVICEERR_NOTCART;
}


/*==============================
    device_maxromsize_64drive
    Gets the max ROM size that 
    the 64Drive supports
    @return The max ROM size
==============================*/

uint32_t device_maxromsize_64drive()
{
    // TODO: Extended address mode for 256 MB ROMs
    return 64*1024*1024;
}


/*==============================
    device_rompadding_64drive
    Calculates the correct ROM size 
    for uploading on the 64Drive
    @param  The current ROM size
    @return The correct ROM size 
            for uploading.
==============================*/

uint32_t device_rompadding_64drive(uint32_t romsize)
{
    // While the 64Drive does not need ROMs to be padded,
    // there is a firmware bug which causes the last few
    // bytes to get corrupted. This was not fun to debug...
    // So we need to append 512 bytes as a safety buffer.
    // Since the 64Drive is super fast at uploading, it 
    // doesn't hurt to pad the ROM.
    uint32_t newsize = ALIGN(romsize, 512) + 512;
    return newsize > device_maxromsize_64drive() ? ALIGN(romsize, 512) : newsize;
}


/*==============================
    device_explicitcic_64drive1
    Checks if the 64Drive requires
    explicitly stating the CIC, and
    auto sets it based on the IPL if
    so
    @param  The 4KB bootcode
    @return Whether the CIC was changed
==============================*/

bool device_explicitcic_64drive1(byte* bootcode)
{
    (void)bootcode; // Workaround unreferenced parameter
    return false;
}


/*==============================
    device_explicitcic_64drive2
    Checks if the 64Drive requires
    explicitly stating the CIC, and
    auto sets it based on the IPL if
    so
    @param  The 4KB bootcode
    @return Whether the CIC was changed
==============================*/

bool device_explicitcic_64drive2(byte* bootcode)
{
    device_setcic(cic_from_bootcode(bootcode));
    return true;
}


/*==============================
    device_testdebug_64drive
    Checks whether the 64Drive can use debug mode
    @param A pointer to the cart context
    @returns DEVICEERR_OK if the firmware version is higher than 2.04, 
             DEVICEERR_64D_CANTDEBUG if the firmware isn't,
             or any other device error
==============================*/

DeviceError device_testdebug_64drive(CartDevice* cart)
{
    uint32_t result;
    N64DriveHandle* fthandle = (N64DriveHandle*) cart->structure;
    DeviceError err;
    
    // If we're not loading a ROM, then don't run the version command as it can cause problems if receiving data first
    if (device_getrom() == NULL)
        return DEVICEERR_OK;

    // Send the version command
    err = device_sendcmd_64drive(fthandle, DEV_CMD_GETVER, true, &result, 0);
    if (err != DEVICEERR_OK)
        return err;

    // Firmware must be 2.05 or higher for USB stuff
    if ((result & 0x0000FFFF) < 205)
        return DEVICEERR_64D_CANTDEBUG;

    // Otherwise, we can use debug mode.
    return DEVICEERR_OK;
}


/*==============================
    device_sendcmd_64drive
    Opens the USB pipe
    @param  A pointer to the cart handle
    @param  The command to send
    @param  A bool stating whether a reply should be expected
    @param  A pointer to store the result of the reply
    @param  The number of extra params to send
    @param  The extra variadic commands to send
    @return The device error, or OK
==============================*/

static DeviceError device_sendcmd_64drive(N64DriveHandle* fthandle, uint8_t command, bool reply, uint32_t* result, uint32_t numparams, ...)
{
    byte     send_buff[32];
    uint32_t recv_buff[32];
    va_list  params;

    // Clear the buffers
    memset(send_buff, 0, 32*sizeof(byte));
    memset(recv_buff, 0, 32*sizeof(uint32_t));

    // Setup the command to send
    send_buff[0] = command;
    send_buff[1] = 0x43;    // C
    send_buff[2] = 0x4D;    // M
    send_buff[3] = 0x44;    // D

    // Append extra arguments to the command if needed
    va_start(params, numparams);
    if (numparams > 0)
        *(uint32_t*)&send_buff[4] = swap_endian(va_arg(params, uint32_t));
    if (numparams > 1)
        *(uint32_t*)&send_buff[8] = swap_endian(va_arg(params, uint32_t));
    va_end(params);

    // Write to the cart
    if (device_usb_write(fthandle->handle, send_buff, 4+(numparams*4), &fthandle->bytes_written) != USB_OK)
        return DEVICEERR_WRITEFAIL;
    if (fthandle->bytes_written == 0)
        return DEVICEERR_WRITEZERO;

    // If the command expects a response
    if (reply)
    {
        uint32_t expected = command << 24 | 0x00504D43;

        // These two instructions do not return a success, so ignore them
        if (command == DEV_CMD_PI_WR_BL || command == DEV_CMD_PI_WR_BL_LONG)
            return DEVICEERR_OK;

        // Read the reply from the 64Drive
        if (device_usb_read(fthandle->handle, recv_buff, 4, &fthandle->bytes_read) != USB_OK)
            return DEVICEERR_NOCOMPSIG;

        // Store the result if requested
        if (result != NULL)
        {
            (*result) = swap_endian(recv_buff[0]);

            // Read the rest of the stuff and the CMP as well
            if (device_usb_read(fthandle->handle, recv_buff, 4, &fthandle->bytes_read) != USB_OK)
                return DEVICEERR_NOCOMPSIG;
            if (device_usb_read(fthandle->handle, recv_buff, 4, &fthandle->bytes_read) != USB_OK)
                return DEVICEERR_NOCOMPSIG;
        }

        // Check that we received the signal that the operation completed
        if (memcmp(recv_buff, &expected, 4) != 0)
            if (command != DEV_CMD_GETVER)
                return DEVICEERR_NOCOMPSIG;
    }

    // Success
    return DEVICEERR_OK;
}


/*==============================
    device_open_64drive
    Opens the USB pipe
    @param  A pointer to the cart context
    @return The device error, or OK
==============================*/

DeviceError device_open_64drive(CartDevice* cart)
{
    uint32_t size;
    N64DriveHandle* fthandle = (N64DriveHandle*) cart->structure;

    // Open the cart
    if (device_usb_open(fthandle->device_index, &fthandle->handle) != USB_OK || fthandle->handle == NULL)
        return DEVICEERR_CANTOPEN;

    // First, check if the USB port has crap in it, and if so, unclog it
    // Won't deal with data sent from PC to the N64 not being read 
    while (device_usb_getqueuestatus(fthandle->handle, &size) && size > 0)
        device_usb_read(fthandle->handle, &size, sizeof(uint32_t), &fthandle->bytes_read);

    // Reset the cart and set its timeouts
    if (device_usb_resetdevice(fthandle->handle) != USB_OK)
        return DEVICEERR_RESETFAIL;
    if (device_usb_resetdevice(fthandle->handle) != USB_OK)
        return DEVICEERR_RESETFAIL;
    if (device_usb_settimeouts(fthandle->handle, 5000, 5000) != USB_OK)
        return DEVICEERR_TIMEOUTSETFAIL;

    // If the cart is in synchronous mode, enable the bits
    if (fthandle->synchronous)
    {
        if (device_usb_setbitmode(fthandle->handle, 0xff, USB_BITMODE_RESET) != USB_OK)
            return DEVICEERR_BITMODEFAIL_RESET;
        if (device_usb_setbitmode(fthandle->handle, 0xff, USB_BITMODE_SYNC_FIFO) != USB_OK)
            return DEVICEERR_BITMODEFAIL_SYNCFIFO;
    }

    // Purge USB contents
    if (device_usb_purge(fthandle->handle, USB_PURGE_RX | USB_PURGE_TX) != USB_OK)
        return DEVICEERR_PURGEFAIL;

    // Ok
    return DEVICEERR_OK;
}


/*==============================
    device_sendrom_64drive
    Sends the ROM to the flashcart
    @param A pointer to the cart context
    @param A pointer to the ROM to send
    @param The size of the ROM
    @return The device error, or OK
==============================*/

DeviceError device_sendrom_64drive(CartDevice* cart, byte* rom, uint32_t size)
{
    N64DriveHandle* fthandle = (N64DriveHandle*) cart->structure;
    uint32_t ram_addr = 0x0;
    uint32_t bytes_done = 0;
    uint32_t bytes_left = size;
    uint32_t chunk;
    byte     cmpbuff[4];

    // Start by setting the CIC if we're running HW2
    if (cart->carttype != CART_64DRIVE1 && cart->cictype != CIC_NONE)
    {
        DeviceError err = device_sendcmd_64drive(fthandle, DEV_CMD_SETCIC, false, NULL, 1, (1 << 31) | ((uint32_t)cart->cictype), 0);
        if (err != DEVICEERR_OK)
            return err;
        device_usb_read(fthandle->handle, cmpbuff, 4, &fthandle->bytes_read);
        if (cmpbuff[0] != 'C' || cmpbuff[1] != 'M' || cmpbuff[2] != 'P' || cmpbuff[3] != DEV_CMD_SETCIC)
            return DEVICEERR_64D_BADCMP;
    }
    
    // Then, set the save type
    if (cart->savetype != SAVE_NONE)
    {
        DeviceError err = device_sendcmd_64drive(fthandle, DEV_CMD_SETSAVE, false, NULL, 1, (uint32_t)cart->savetype, 0);
        if (err != DEVICEERR_OK)
            return err;
        device_usb_read(fthandle->handle, cmpbuff, 4, &fthandle->bytes_read);
        if (cmpbuff[0] != 'C' || cmpbuff[1] != 'M' || cmpbuff[2] != 'P' || cmpbuff[3] != DEV_CMD_SETSAVE)
            return DEVICEERR_64D_BADCMP;
    }

    // Decide a better, more optimized chunk size for sending
    // Chunks must be under 8MB
    if (size > 16*1024*1024)
        chunk = 32;
    else if (size > 2*1024*1024)
        chunk = 16;
    else
        chunk = 4;
    chunk *= 128*1024; // Convert to megabytes

    // Upload the ROM in a loop
    while (bytes_left > 0)
    {
        uint32_t bytes_do = chunk;
        if (bytes_left < chunk)
            bytes_do = bytes_left;

        // Check if the upload was cancelled
        if (device_uploadcancelled())
           break;

        // Send the data to the 64Drive
        device_sendcmd_64drive(fthandle, DEV_CMD_LOADRAM, false, NULL, 2, ram_addr, (bytes_do & 0xffffff) | 0 << 24);
        if (device_usb_write(fthandle->handle, rom + bytes_done, bytes_do, &fthandle->bytes_written)  != USB_OK)
            return DEVICEERR_WRITEFAIL;

        // Read the success response
        if (device_usb_read(fthandle->handle, cmpbuff, 4, &fthandle->bytes_read)  != USB_OK)
            return DEVICEERR_READFAIL;
        if (cmpbuff[0] != 'C' || cmpbuff[1] != 'M' || cmpbuff[2] != 'P' || cmpbuff[3] != DEV_CMD_LOADRAM)
            return DEVICEERR_64D_BADCMP;

        // Update the upload progress
        bytes_left -= bytes_do;
        bytes_done += bytes_do;
        ram_addr += bytes_do;
        device_setuploadprogress((((float)bytes_done)/((float)size))*100.0f);
    }

    // Wait for the CMP signal
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Return an error if the upload was cancelled
    if (device_uploadcancelled())
        return DEVICEERR_UPLOADCANCELLED;

    // Success
    device_setuploadprogress(100.0f);
    return DEVICEERR_OK;
}


static int chartoint(const char* str)
{
    return ((((int)str[0])<<24)&0xFF000000) | ((((int)str[1])<<16)&0x00FF0000) | ((((int)str[2])<<8)&0x0000FF00) | ((((int)str[3])<<0)&0x000000FF);
}


static void device_queueack(const char* ack)
{
    //global_ackswanted.push_back(chartoint(ack));
}


//  0 -> Nothing left
// >0 -> Device error
// DEVICEERR_64D_BADDMA -> DMA@ in memory
static DeviceError device_checkacks(N64DriveHandle* fthandle)
{
    uint32_t size;

    // First, check if we have data to read
    if (device_usb_getqueuestatus(fthandle->handle, &size) != USB_OK)
        return DEVICEERR_POLLFAIL;

    // If we do, then check if it's incoming data
    while (size > 0)
    {
        int dataasint;
        char temp[4];
        bool found = false;
        std::vector<int>::iterator it;

        // Read the first 4 bytes, because it might be a CMP or a DMA header
        if (device_usb_read(fthandle->handle, (byte*)temp, 4, &fthandle->bytes_read) != USB_OK)
            return DEVICEERR_READFAIL;

        // If we have a DMA header, it's data that needs to be further handled
        if (strncmp(temp, "DMA@", 4) == 0)
        {
            uint32_t read = 0;
            byte     temp[4];
            USBData  dat;

            // Get information about the incoming data and store it
            if (device_usb_read(fthandle->handle, temp, 4, &fthandle->bytes_read) != USB_OK)
                return DEVICEERR_READFAIL;
            dat.header = swap_endian(temp[3] << 24 | temp[2] << 16 | temp[1] << 8 | temp[0]);
            dat.size = dat.header & 0xFFFFFF;

            // Allocate memory for the buffer to store incoming data
            dat.data = (byte*)malloc(dat.size);
            if (dat.data == NULL)
                return DEVICEERR_MALLOCFAIL;

            // Read the data into the buffer, in 512 byte chunks
            // Do in 512 byte chunks so we have a progress bar (and because the N64 does it in 512 byte chunks anyway)
            device_setuploadprogress(0.0f);
            while (read < dat.size)
            {
                uint32_t readamount = dat.size-read;
                if (readamount > 512)
                    readamount = 512;
                if (device_usb_read(fthandle->handle, dat.data+read, readamount, &fthandle->bytes_read) != USB_OK)
                    return DEVICEERR_READFAIL;
                read += fthandle->bytes_read;
                device_setuploadprogress((((float)read)/((float)dat.size))*100.0f);
            }

            // Queue a CMP signal for later
            device_queueack("CMPH");
            device_setuploadprogress(100.0f);
            global_gotdata.push_back(dat);
        }
        else
        {
            /*
            // Otherwise, we want to go through the list of acks we want and see if the data we read corresponded to something
            dataasint = chartoint(temp);
            for (it = global_ackswanted.begin(); it != global_ackswanted.end(); ++it)
            {
                if (*it == dataasint)
                {
                    found = true;
                    global_ackswanted.erase(it);
                    break;
                }
            }

            // The ack we got is unexpected. This should not happen!!!
            if (!found)
                return DEVICEERR_64D_BADCMP;
            */
        }

        // Check for data again
        if (device_usb_getqueuestatus(fthandle->handle, &size) != USB_OK)
            return DEVICEERR_POLLFAIL;
    }
    return DEVICEERR_OK;
}


/*==============================
    device_senddata_64drive
    Sends data to the 64Drive
    @param  A pointer to the cart context
    @param  The datatype that is being sent
    @param  A buffer containing said data
    @param  The size of the data
    @return The device error, or OK
==============================*/

DeviceError device_senddata_64drive(CartDevice* cart, USBDataType datatype, byte* data, uint32_t size)
{
    N64DriveHandle* fthandle = (N64DriveHandle*) cart->structure;
    uint32_t newsize = 0;
    byte*    datacopy = NULL;
    DeviceError err;
    
    // Pad the data to be 512 byte aligned if it is large, if not then to 4 bytes
    if (size > 512)
        newsize = ALIGN(size, 512) + 512; // The extra 512 is to workaround a 64Drive bug
    else
        newsize = ALIGN(size, 4);

    // Files must be smaller than 8MB
    if (newsize > 8*1024*1024)
        return DEVICEERR_64D_DATATOOBIG;

    // Copy the data onto a temp variable
    datacopy = (byte*) calloc(newsize, 1);
    if (datacopy == NULL)
        return DEVICEERR_MALLOCFAIL;
    memcpy(datacopy, data, size);

    // Send this block of data
    device_setuploadprogress(0.0f);
    err = device_sendcmd_64drive(fthandle, DEV_CMD_USBRECV, false, NULL, 1, (newsize & 0x00FFFFFF) | datatype << 24, 0);
    if (err != DEVICEERR_OK)
        return err;
    if (device_usb_write(fthandle->handle, datacopy, newsize, &fthandle->bytes_written) != USB_OK)
        return DEVICEERR_WRITEFAIL;

    // Queue a CMP signal for later
    device_queueack("CMP@");

    // Free used up resources
    device_setuploadprogress(100.0f);
    free(datacopy);
    return DEVICEERR_OK;
}


/*==============================
    device_receivedata_64drive
    Receives data from the 64Drive
    @param  A pointer to the cart context
    @param  A pointer to an 32-bit value where
            the received data header will be
            stored.
    @param  A pointer to a byte buffer pointer
            where the data will be malloc'ed into.
    @return The device error, or OK
==============================*/

DeviceError device_receivedata_64drive(CartDevice* cart, uint32_t* dataheader, byte** buff)
{
    N64DriveHandle* fthandle = (N64DriveHandle*) cart->structure;
    DeviceError err;

    // Before doing anything, we need to check for any incoming acks
    err = device_checkacks(fthandle);
    if (err != DEVICEERR_OK)
        return err;

    // If we have data, pop it out
    if (!global_gotdata.empty())
    {
        USBData dat = global_gotdata.back();
        (*dataheader) = dat.header;
        (*buff) = dat.data;
        global_gotdata.pop_back();
    }
    else
    {
        (*dataheader) = 0;
        (*buff) = NULL;
    }

    // All's good
    return DEVICEERR_OK;
}


/*==============================
    device_close_64drive
    Closes the USB pipe
    @param  A pointer to the cart context
    @return The device error, or OK
==============================*/

DeviceError device_close_64drive(CartDevice* cart)
{
    N64DriveHandle* fthandle = (N64DriveHandle*) cart->structure;
    if (device_usb_close(fthandle->handle) != USB_OK)
        return DEVICEERR_CLOSEFAIL;
    free(fthandle);
    cart->structure = NULL;
    return DEVICEERR_OK;
}