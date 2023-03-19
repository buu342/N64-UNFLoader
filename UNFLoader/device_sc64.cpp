/***************************************************************
                       device_sc64.cpp

Handles SC64 USB communication.
https://github.com/Polprzewodnikowy/SummerCollection
***************************************************************/

#include "device_sc64.h"
#include "Include/ftd2xx.h"
#include <string.h>
#include <thread>
#include <chrono>


/*********************************
              Macros
*********************************/

#define CMD_VERSION_GET             'v'
#define CMD_CONFIG_SET              'C'
#define CMD_MEMORY_WRITE            'M'
#define CMD_DEBUG_WRITE             'U'

#define VERSION_V2                  0x32764353

#define CFG_ID_BOOT_MODE            5
#define CFG_ID_SAVE_TYPE            6

#define BOOT_MODE_ROM               1

#define MEMORY_ADDRESS_SDRAM        0x00000000


/*********************************
             Typedefs
*********************************/

typedef struct {
    DeviceError err;
    uint32_t    val;
} DevTuple;

typedef struct 
{
    uint32_t  device_index;
    FT_HANDLE handle;
    DWORD     bytes_written;
    DWORD     bytes_read;
} SC64Handle;


/*********************************
        Function Prototypes
*********************************/

static DeviceError device_send_cmd_sc64(SC64Handle* fthandle, uint8_t cmd, uint32_t arg1, uint32_t arg2);
static DevTuple    device_check_reply_sc64(SC64Handle* fthandle, uint8_t cmd);


/*==============================
    device_send_cmd_sc64
    Prepares and sends command and parameters to SC64
    @param  A pointer to the cart handle
    @param  Command value
    @param  First argument value
    @param  Second argument value
    @return The device error, or OK
==============================*/

static DeviceError device_send_cmd_sc64(SC64Handle* fthandle, uint8_t cmd, uint32_t arg1, uint32_t arg2)
{
    byte buff[12];
    DWORD bytes_processed;

    // Prepare command
    buff[0] = (byte)'C';
    buff[1] = (byte)'M';
    buff[2] = (byte)'D';
    buff[3] = cmd;
    *(uint32_t *)(&buff[4]) = swap_endian(arg1);
    *(uint32_t *)(&buff[8]) = swap_endian(arg2);

    // Send command and parameters
    if (FT_Write(fthandle->handle, buff, sizeof(buff), &bytes_processed) != FT_OK)
        return DEVICEERR_WRITEFAIL;
    if (bytes_processed != sizeof(buff))
        return DEVICEERR_TXREPLYMISMATCH;
    return DEVICEERR_OK;
}


/*==============================
    device_check_reply_sc64
    Checks if last command was successful
    @param A pointer to the cart handle
    @param Command value to be checked
    @returns A tuple with the device error
             and packet data size
==============================*/

static DevTuple device_check_reply_sc64(SC64Handle* fthandle, uint8_t cmd)
{
    byte buff[4];
    DWORD read;
    uint32_t packet_size;

    // Check completion signal
    if (FT_Read(fthandle->handle, buff, sizeof(buff), &read) != FT_OK)
        return {DEVICEERR_READCOMPSIGFAIL, 0};
    if (read != sizeof(buff) || memcmp(buff, "CMP", 3) != 0 || buff[3] != cmd)
        return {DEVICEERR_NOCOMPSIG, 0};

    // Get packet size
    if (FT_Read(fthandle->handle, &packet_size, 4, &read) != FT_OK)
        return {DEVICEERR_READPACKSIZEFAIL, 0};
    if (read != 4)
        return {DEVICEERR_BADPACKSIZE, 0};

    return {DEVICEERR_OK, swap_endian(packet_size)};
}


/*==============================
    device_test_sc64
    Checks whether the device passed as an argument is SC64
    @param  A pointer to the cart context
    @param  The index of the cart
    @return DEVICEERR_OK if the cart is an SC64, 
            DEVICEERR_NOTCART if it isn't,
            Any other device error if problems ocurred
==============================*/

DeviceError device_test_sc64(CartDevice* cart)
{
    DWORD device_count;
    FT_DEVICE_LIST_INFO_NODE* device_info;

    // Initialize FTD
    if (FT_CreateDeviceInfoList(&device_count) != FT_OK)
        return DEVICEERR_USBBUSY;

    // Check if the device exists
    if (device_count == 0)
        return DEVICEERR_NODEVICES;

    // Allocate storage and get device info list
    device_info = (FT_DEVICE_LIST_INFO_NODE*) malloc(sizeof(FT_DEVICE_LIST_INFO_NODE)*device_count);
    FT_GetDeviceInfoList(device_info, &device_count);

    // Search the devices
    for (uint32_t i=0; i<device_count; i++)
    {
        // Look for 64drive HW1 (FT2232H Asynchronous FIFO mode)
        if (strcmp(device_info[i].Description, "SC64") == 0 && device_info[i].ID == 0x04036014)
        {
            SC64Handle* fthandle = (SC64Handle*) malloc(sizeof(SC64Handle));
            free(device_info);
            fthandle->device_index = i;
            cart->structure = fthandle;
            return DEVICEERR_OK;
        }
    }

    // Could not find the flashcart
    free(device_info);
    return DEVICEERR_NOTCART;
}


/*==============================
    device_maxromsize_sc64
    Gets the max ROM size that 
    the SC64 supports
    @return The max ROM size
==============================*/

uint32_t device_maxromsize_sc64()
{
    return 64*1024*1024;
}


/*==============================
    device_shouldpadrom_sc64
    Checks if the ROM should be
    padded before uploading on the
    SC64.
    @return Whether or not to pad
            the ROM.
==============================*/

bool device_shouldpadrom_sc64()
{
    return true;
}


/*==============================
    device_explicitcic_sc64
    Checks if the SC64 requires
    explicitly stating the CIC, and
    auto sets it based on the IPL if
    so
    @param  The 4KB bootcode
    @return Whether the CIC was changed
==============================*/

bool device_explicitcic_sc64(byte* bootcode)
{
    (void)(bootcode); // Ignore unused paramater warning
    return false;
}


/*==============================
    device_open_sc64
    Opens the USB pipe
    @param  A pointer to the cart context
    @return The device error, or OK
==============================*/

DeviceError device_open_sc64(CartDevice* cart)
{
    SC64Handle* fthandle = (SC64Handle*) cart->structure;
    ULONG modem_status;
    uint32_t version;
    DeviceError err;
    DevTuple retval;

    // Open the cart
    if (FT_Open(fthandle->device_index, &fthandle->handle) != FT_OK || fthandle->handle == NULL)
        return DEVICEERR_CANTOPEN;

    // Reset the cart and set its timeouts and latency timer
    if (FT_ResetDevice(fthandle->handle) != FT_OK)
        return DEVICEERR_RESETFAIL;
    if (FT_SetTimeouts(fthandle->handle, 5000, 5000) != FT_OK)
        return DEVICEERR_TIMEOUTSETFAIL;

    // Perform controller reset by setting DTR line and checking DSR line status
    if (FT_SetDtr(fthandle->handle) != FT_OK)
        return DEVICEERR_SETDTRFAIL;
    for (int i = 0; i < 10; i++)
    {
        if (FT_GetModemStatus(fthandle->handle, &modem_status) != FT_OK)
            return DEVICEERR_GETMODEMSTATUSFAIL;
        if (modem_status & 0x20)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!(modem_status & 0x20))
        return DEVICEERR_PURGEFAIL;

    // Purge USB contents
    if (FT_Purge(fthandle->handle, FT_PURGE_RX | FT_PURGE_TX) != FT_OK)
        return DEVICEERR_PURGEFAIL;

    // Release reset
    if (FT_ClrDtr(fthandle->handle) != FT_OK)
        return DEVICEERR_CLEARDTRFAIL;
    for (int i = 0; i < 10; i++)
    {
        if (FT_GetModemStatus(fthandle->handle, &modem_status) != FT_OK)
            return DEVICEERR_GETMODEMSTATUSFAIL;
        if (!(modem_status & 0x20))
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (modem_status & 0x20)
        return DEVICEERR_SC64_CTRLRELEASEFAIL;

    // Check SC64 firmware version
    err = device_send_cmd_sc64(fthandle, CMD_VERSION_GET, 0, 0);
    if (err != DEVICEERR_OK)
        return err;
    retval = device_check_reply_sc64(fthandle, CMD_VERSION_GET);
    if (retval.err != DEVICEERR_OK)
        return retval.err;
    if (FT_Read(fthandle->handle, &version, 4, &fthandle->bytes_read) != FT_OK)
        return DEVICEERR_SC64_FIRMWARECHECKFAIL;
    if (retval.val != 4 || fthandle->bytes_read != 4 || version != VERSION_V2)
        return DEVICEERR_SC64_FIRMWAREUNKNOWN;

    // Ok
    return DEVICEERR_OK;
}


/*==============================
    device_close_sc64
    Closes the USB pipe
    @param  A pointer to the cart context
    @return The device error, or OK
==============================*/

DeviceError device_close_sc64(CartDevice* cart)
{
    SC64Handle* fthandle = (SC64Handle*) cart->structure;
    if (FT_Close(fthandle->handle) != FT_OK)
        return DEVICEERR_CLOSEFAIL;
    free(fthandle);
    cart->structure = NULL;
    return DEVICEERR_OK;
}