/***************************************************************
                       device_sc64.cpp

Handles SC64 USB communication.
https://github.com/Polprzewodnikowy/SummerCollection

***************************************************************/

#include "main.h"
#include "helper.h"
#include "device_sc64.h"


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
        Function Prototypes
*********************************/

static void device_send_cmd_sc64(ftdi_context_t* cart, u8 cmd, u32 arg1, u32 arg2);
static u32 device_check_reply_sc64(ftdi_context_t* cart, u8 cmd);


/*==============================
    device_send_cmd_sc64
    Prepares and sends command and parameters to SC64
    @param A pointer to the cart context
    @param Command value
    @param First argument value
    @param Second argument value
==============================*/

static void device_send_cmd_sc64(ftdi_context_t* cart, u8 cmd, u32 arg1, u32 arg2)
{
    u8 buff[12];
    DWORD bytes_processed;

    // Prepare command
    buff[0] = (u8)'C';
    buff[1] = (u8)'M';
    buff[2] = (u8)'D';
    buff[3] = cmd;
    *(u32 *)(&buff[4]) = swap_endian(arg1);
    *(u32 *)(&buff[8]) = swap_endian(arg2);

    // Send command and parameters
    testcommand(FT_Write(cart->handle, buff, sizeof(buff), &bytes_processed), "Error: Unable to write command to SC64.\n");
    if (bytes_processed != sizeof(buff))
        terminate("Actual bytes written amount is different than desired.\n");
}


/*==============================
    device_check_reply_sc64
    Checks if last command was successful
    @param A pointer to the cart context
    @param Command value to be checked
    @returns Packet data size
==============================*/

static u32 device_check_reply_sc64(ftdi_context_t* cart, u8 cmd)
{
    u8 buff[4];
    DWORD read;
    u32 packet_size;

    // Check completion signal
    testcommand(FT_Read(cart->handle, buff, sizeof(buff), &read), "Error: Unable to read completion signal.\n");
    if (read != sizeof(buff) || memcmp(buff, "CMP", 3) != 0 || buff[3] != cmd)
        terminate("Did not receive completion signal.\n");

    // Get packet size
    testcommand(FT_Read(cart->handle, &packet_size, 4, &read), "Error: Unable to read packet size.\n");
    if (read != 4)
        terminate("Couldn't read packet size.\n");

    return swap_endian(packet_size);
}


/*==============================
    device_test_sc64
    Checks whether the device passed as an argument is SC64
    @param A pointer to the cart context
    @param The index of the cart
==============================*/

bool device_test_sc64(ftdi_context_t* cart, int index)
{
    return (strcmp(cart->dev_info[index].Description, "SC64") == 0 && cart->dev_info[index].ID == 0x04036014);
}


/*==============================
    device_open_sc64
    Opens the USB pipe
    @param A pointer to the cart context
==============================*/

void device_open_sc64(ftdi_context_t* cart)
{
    ULONG modem_status;
    u32 packet_size;
    u32 version;

    // Open the cart
    cart->status = FT_Open(cart->device_index, &cart->handle);
    if (cart->status != FT_OK || !cart->handle)
        terminate("Unable to open flashcart.\n");

    // Reset the cart and set its timeouts and latency timer
    testcommand(FT_ResetDevice(cart->handle), "Error: Unable to reset flashcart.\n");
    testcommand(FT_SetTimeouts(cart->handle, 5000, 5000), "Error: Unable to set flashcart timeouts.\n");

    // Perform controller reset by setting DTR line and checking DSR line status
    testcommand(FT_SetDtr(cart->handle), "Error: Unable to set DTR line");
    for (int i = 0; i < 10; i++)
    {
        testcommand(FT_GetModemStatus(cart->handle, &modem_status), "Error: Unable to get modem status");
        if (modem_status & 0x20)
            break;
        #ifndef LINUX
            Sleep(10);
        #else
            usleep(10);
        #endif
    }
    if (!(modem_status & 0x20))
        terminate("Couldn't perform SC64 controller reset");

    // Purge USB contents
    testcommand(FT_Purge(cart->handle, FT_PURGE_RX | FT_PURGE_TX), "Error: Unable to purge USB contents.\n");

    // Release reset
    testcommand(FT_ClrDtr(cart->handle), "Error: Unable to clear DTR line");
    for (int i = 0; i < 10; i++)
    {
        testcommand(FT_GetModemStatus(cart->handle, &modem_status), "Error: Unable to get modem status");
        if (!(modem_status & 0x20))
            break;
        #ifndef LINUX
            Sleep(10);
        #else
            usleep(10);
        #endif
    }
    if (modem_status & 0x20)
        terminate("Couldn't release SC64 controller reset");

    // Check SC64 firmware version
    device_send_cmd_sc64(cart, CMD_VERSION_GET, 0, 0);
    packet_size = device_check_reply_sc64(cart, CMD_VERSION_GET);
    testcommand(FT_Read(cart->handle, &version, 4, &cart->bytes_read), "Error: Couldn't get SC64 firmware version");
    if (packet_size != 4 || cart->bytes_read != 4 || version != VERSION_V2)
        terminate("Unknown SC64 firmware version");
}


/*==============================
    device_sendrom_sc64
    Sends the ROM to the flashcart
    @param A pointer to the cart context
    @param A pointer to the ROM to send
    @param The size of the ROM
==============================*/

void device_sendrom_sc64(ftdi_context_t* cart, FILE* file, u32 size)
{
    size_t chunk;
    u8* rom_buffer;
    size_t left;
    time_t upload_time_start;
    const char* save_names[] = {
        "EEPROM 4k",
        "EEPROM 16k",
        "SRAM 32k",
        "FlashRAM 128k",
        "SRAM banked 3x32k",
    };

    // 256 kB chunk size
    chunk = 256 * 1024;

    // Allocate ROM buffer
    rom_buffer = (u8 *)malloc(chunk * sizeof(u8));
    if (rom_buffer == NULL)
        terminate("Unable to allocate memory for buffer.\n");

    // Set boot mode
    device_send_cmd_sc64(cart, CMD_CONFIG_SET, CFG_ID_BOOT_MODE, BOOT_MODE_ROM);
    if (device_check_reply_sc64(cart, CMD_CONFIG_SET) != 0)
        terminate("SC64 config set command returned with unexpected reply");

    // Set savetype if provided
    if (global_savetype > 0 && global_savetype <= 5)
        pdprint("Save type set to %d, %s.\n", CRDEF_PROGRAM, global_savetype, save_names[global_savetype - 1]);
    else
        global_savetype = 0;

    // Commit save setting
    device_send_cmd_sc64(cart, CMD_CONFIG_SET, CFG_ID_SAVE_TYPE, global_savetype);
    if (device_check_reply_sc64(cart, CMD_CONFIG_SET) != 0)
        terminate("SC64 config set command returned with unexpected reply");

    // Init progressbar
    pdprint("\n", CRDEF_PROGRAM);
    progressbar_draw("Uploading ROM (ESC to cancel)", CRDEF_PROGRAM, 0);

    // Prepare variables
    left = size;

    // Get start time
    upload_time_start = clock();

    // Prepare cart for write
    device_send_cmd_sc64(cart, CMD_MEMORY_WRITE, MEMORY_ADDRESS_SDRAM, size);

    // Loop until ROM has been fully written
    while (left > 0)
    {
        // Calculate current chunk size
        if (left < chunk)
            chunk = left;

        // Check if ESC was pressed
        int ch = getch();
        if (ch == CH_ESCAPE)
        {
            pdprint_replace("ROM upload canceled by the user\n", CRDEF_PROGRAM);
            free(rom_buffer);
            return;
        }

        // Read ROM from file to buffer
        fread(rom_buffer, sizeof(u8), chunk, file);
        if (global_z64)
            for (size_t i=0; i<chunk; i+=2)
                SWAP(rom_buffer[i], rom_buffer[i+1]);

        // Push data
        testcommand(FT_Write(cart->handle, rom_buffer, chunk, &cart->bytes_written), "Error: Unable to write data to SC64.\n");

        // Break from loop if not all bytes has been sent
        if (cart->bytes_written != chunk)
            break;

        // Update bytes left
        left -= cart->bytes_written;

        // Update progressbar
        progressbar_draw("Uploading ROM (ESC to cancel)", CRDEF_PROGRAM, (size - left) / (float)size);
    }

    // Free ROM buffer
    free(rom_buffer);

    // Throw error if upload was unsuccessful
    if (left > 0)
        terminate("SC64 timed out");

    // Check if write was successful
    if (device_check_reply_sc64(cart, CMD_MEMORY_WRITE) != 0)
        terminate("SC64 memory write command returned with unexpected reply");

    // Print that we've finished
    double upload_time = (double)(clock() - upload_time_start) / CLOCKS_PER_SEC;
    pdprint_replace("ROM successfully uploaded in %.2f seconds!\n", CRDEF_PROGRAM, upload_time);
}


/*==============================
    device_testdebug_sc64
    Checks whether this cart can use debug mode
    @param A pointer to the cart context
    @returns Always returns true.
==============================*/

bool device_testdebug_sc64(ftdi_context_t* cart)
{
    (void)cart; // Prevents error about unused parameter
    return true;
}


/*==============================
    device_senddata_sc64
    Sends data to the flashcart
    @param A pointer to the cart context
    @param A pointer to the data to send
    @param The size of the data
==============================*/

void device_senddata_sc64(ftdi_context_t* cart, int datatype, char* data, u32 size)
{
    // Sanitize datatype and size
    datatype &= 0xFF;
    size &= 0xFFFFFF;

    // Prepare cart for transfer
    device_send_cmd_sc64(cart, CMD_DEBUG_WRITE, datatype, size);

    // Transmit data
    testcommand(FT_Write(cart->handle, data, size, &cart->bytes_written), "Error: Unable to write data to SC64.\n");

    // Throw error if transfer was unsuccessful
    if (cart->bytes_written != size)
        terminate("SC64 timed out");
}


/*==============================
    device_close_sc64
    Closes the USB pipe
    @param A pointer to the cart context
==============================*/

void device_close_sc64(ftdi_context_t* cart)
{
    testcommand(FT_Close(cart->handle), "Error: Unable to close flashcart.\n");
    cart->handle = 0;
}
