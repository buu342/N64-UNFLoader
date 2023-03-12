/***************************************************************
                       device_sc64.cpp

Handles SC64 USB communication.
https://github.com/Polprzewodnikowy/SummerCollection
***************************************************************/

#include "device_sc64.h"
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


/*********************************
        Function Prototypes
*********************************/

static DeviceError device_send_cmd_sc64(FTDIDevice* cart, uint8_t cmd, uint32_t arg1, uint32_t arg2);
static DevTuple    device_check_reply_sc64(FTDIDevice* cart, uint8_t cmd);


/*==============================
    device_send_cmd_sc64
    Prepares and sends command and parameters to SC64
    @param  A pointer to the cart context
    @param  Command value
    @param  First argument value
    @param  Second argument value
    @return The device error, or OK
==============================*/

static DeviceError device_send_cmd_sc64(FTDIDevice* cart, uint8_t cmd, uint32_t arg1, uint32_t arg2)
{
    uint8_t buff[12];
    DWORD bytes_processed;

    // Prepare command
    buff[0] = (uint8_t)'C';
    buff[1] = (uint8_t)'M';
    buff[2] = (uint8_t)'D';
    buff[3] = cmd;
    *(uint32_t *)(&buff[4]) = swap_endian(arg1);
    *(uint32_t *)(&buff[8]) = swap_endian(arg2);

    // Send command and parameters
    if (FT_Write(cart->handle, buff, sizeof(buff), &bytes_processed) != FT_OK)
        return DEVICEERR_WRITEFAIL;
    if (bytes_processed != sizeof(buff))
        return DEVICEERR_TXREPLYMISMATCH;
    return DEVICEERR_OK;
}


/*==============================
    device_check_reply_sc64
    Checks if last command was successful
    @param A pointer to the cart context
    @param Command value to be checked
    @returns A tuple with the device error
             and packet data size
==============================*/

static DevTuple device_check_reply_sc64(FTDIDevice* cart, uint8_t cmd)
{
    uint8_t buff[4];
    DWORD read;
    uint32_t packet_size;

    // Check completion signal
    if (FT_Read(cart->handle, buff, sizeof(buff), &read) != FT_OK)
        return {DEVICEERR_READCOMPSIGFAIL, 0};
    if (read != sizeof(buff) || memcmp(buff, "CMP", 3) != 0 || buff[3] != cmd)
        return {DEVICEERR_NOCOMPSIG, 0};

    // Get packet size
    if (FT_Read(cart->handle, &packet_size, 4, &read) != FT_OK)
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
    @return True if the cart is an SC64, or false otherwise
==============================*/

bool device_test_sc64(FTDIDevice* cart, uint32_t index)
{
    return (strcmp(cart->device_info[index].Description, "SC64") == 0 && cart->device_info[index].ID == 0x04036014);
}


/*==============================
    device_open_sc64
    Opens the USB pipe
    @param  A pointer to the cart context
    @return The device error, or OK
==============================*/

DeviceError device_open_sc64(FTDIDevice* cart)
{
    ULONG modem_status;
    uint32_t version;
    DeviceError err;
    DevTuple retval;

    // Open the cart
    cart->status = FT_Open(cart->device_index, &cart->handle);
    if (cart->status != FT_OK || cart->handle == NULL)
        return DEVICEERR_CANTOPEN;

    // Reset the cart and set its timeouts and latency timer
    if (FT_ResetDevice(cart->handle) != FT_OK)
        return DEVICEERR_RESETFAIL;
    if (FT_SetTimeouts(cart->handle, 5000, 5000) != FT_OK)
        return DEVICEERR_TIMEOUTSETFAIL;

    // Perform controller reset by setting DTR line and checking DSR line status
    if (FT_SetDtr(cart->handle) != FT_OK)
        return DEVICEERR_SETDTRFAIL;
    for (int i = 0; i < 10; i++)
    {
        if (FT_GetModemStatus(cart->handle, &modem_status) != FT_OK)
            return DEVICEERR_GETMODEMSTATUSFAIL;
        if (modem_status & 0x20)
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!(modem_status & 0x20))
        return DEVICEERR_PURGEFAIL;

    // Purge USB contents
    if (FT_Purge(cart->handle, FT_PURGE_RX | FT_PURGE_TX) != FT_OK)
        return DEVICEERR_PURGEFAIL;

    // Release reset
    if (FT_ClrDtr(cart->handle) != FT_OK)
        return DEVICEERR_CLEARDTRFAIL;
    for (int i = 0; i < 10; i++)
    {
        if (FT_GetModemStatus(cart->handle, &modem_status) != FT_OK)
            return DEVICEERR_GETMODEMSTATUSFAIL;
        if (!(modem_status & 0x20))
            break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (modem_status & 0x20)
        return DEVICEERR_SC64_CTRLRELEASEFAIL;

    // Check SC64 firmware version
    err = device_send_cmd_sc64(cart, CMD_VERSION_GET, 0, 0);
    if (err != DEVICEERR_OK)
        return err;
    retval = device_check_reply_sc64(cart, CMD_VERSION_GET);
    if (retval.err != DEVICEERR_OK)
        return retval.err;
    if (FT_Read(cart->handle, &version, 4, &cart->bytes_read) != FT_OK)
        return DEVICEERR_SC64_FIRMWARECHECKFAIL;
    if (retval.val != 4 || cart->bytes_read != 4 || version != VERSION_V2)
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

DeviceError device_close_sc64(FTDIDevice* cart)
{
    if (FT_Close(cart->handle) != FT_OK)
        return DEVICEERR_CLOSEFAIL;
    return DEVICEERR_OK;
}
