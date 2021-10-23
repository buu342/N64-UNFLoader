/***************************************************************
                       device_sc64.cpp

Handles SummerCart64 USB communication.
https://github.com/Polprzewodnikowy/SummerCollection

***************************************************************/

#include "main.h"
#include "helper.h"
#include "device_sc64.h"


/*********************************
        Function Prototypes
*********************************/

static void device_send_cmd_sc64(ftdi_context_t* cart, u8 cmd, u32 arg1, u32 arg2, bool reply);
static void device_check_reply_sc64(ftdi_context_t* cart, u8 cmd);


/*==============================
    device_send_cmd_sc64
    Prepares and sends command and parameters to SC64
    @param A pointer to the cart context
    @param Command value
    @param First argument value
    @param Second argument value
==============================*/

static void device_send_cmd_sc64(ftdi_context_t* cart, u8 cmd, u32 arg1, u32 arg2, bool reply) {
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
    testcommand(FT_Write(cart->handle, buff, sizeof(buff), &bytes_processed), "Error: Unable to write command to SummerCart64.\n");
    if (bytes_processed != sizeof(buff)) {
        terminate("Error: Actual bytes written amount is different than desired.\n");
    }

    // Check reply if command doesn't require any data
    if (reply) {
        device_check_reply_sc64(cart, cmd);
    }
}


/*==============================
    device_check_reply_sc64
    Checks if last command was successful
    @param A pointer to the cart context
    @param Command value to be checked
==============================*/

static void device_check_reply_sc64(ftdi_context_t* cart, u8 cmd) {
    u8 buff[4];
    DWORD bytes_processed;

    testcommand(FT_Read(cart->handle, buff, sizeof(buff), &bytes_processed), "Error: Unable to read completion signal.\n");
    if (bytes_processed != sizeof(buff) || memcmp(buff, "CMP", 3) != 0 || buff[3] != cmd) {
        terminate("Error: Did not receive completion signal.\n");
    } 
}


/*==============================
    device_test_sc64
    Checks whether the device passed as an argument is SummerCart64
    @param A pointer to the cart context
    @param The index of the cart
==============================*/

bool device_test_sc64(ftdi_context_t* cart, int index)
{
    return (strcmp(cart->dev_info[index].Description, "SummerCart64") == 0 && cart->dev_info[index].ID == 0x04036014);
}


/*==============================
    device_open_sc64
    Opens the USB pipe
    @param A pointer to the cart context
==============================*/

void device_open_sc64(ftdi_context_t* cart)
{
    // Open the cart
    cart->status = FT_Open(cart->device_index, &cart->handle);
    if (cart->status != FT_OK || !cart->handle)
        terminate("Error: Unable to open flashcart.\n");

    // Reset the cart and set its timeouts and latency timer
    testcommand(FT_ResetDevice(cart->handle), "Error: Unable to reset flashcart.\n");
    testcommand(FT_SetTimeouts(cart->handle, 5000, 5000), "Error: Unable to set flashcart timeouts.\n");

    // Purge USB contents
    testcommand(FT_Purge(cart->handle, FT_PURGE_RX | FT_PURGE_TX), "Error: Unable to purge USB contents.\n");
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
    size_t bytes_left;
    time_t upload_time_start;
    s32 cic;
    s32 tv;
    s32 skip;
    const char* save_names[] = {
        "EEPROM 4k",
        "EEPROM 16k",
        "SRAM 32k",
        "FlashRAM 128k",
        "SRAM banked 3x32k",
        "FlashRAM 128k (Pokemon Stadium 2 special case)",
    };

    // Align rom size to 2 bytes
    size = size - (size % 2);

    // 256 kB chunk size
    chunk = 256 * 1024;

    // Allocate ROM buffer
    rom_buffer = (u8 *)malloc(chunk * sizeof(u8));
    if (rom_buffer == NULL) {
        terminate("Error: Unable to allocate memory for buffer.\n");
    }

    // Set unknown CIC and TV type as default
    cic = -1;
    tv = -1;
    skip = 0;

    // Set CIC and TV type if provided
    if (global_cictype != -1 && cart->cictype == 0) {
        switch (global_cictype) {
            case 0:
            case 6101: cic = 0x13F; tv = 0; break;
            case 1:
            case 6102: cic = 0x3F; tv = 0; break;
            case 2:
            case 7101: cic = 0x3F; tv = 1; break;
            case 3:
            case 7102: cic = 0x13F; tv = 1; break;
            case 4:
            case 103: cic = 0x78; tv = -1; break;
            case 6103: cic = 0x78; tv = 0; break;
            case 7103: cic = 0x78; tv = 1; break;
            case 5:
            case 105: cic = 0x91; tv = -1; break;
            case 6105: cic = 0x91; tv = 0; break;
            case 7105: cic = 0x91; tv = 1; break;
            case 6:
            case 106: cic = 0x85; tv = -1; break;
            case 6106: cic = 0x85; tv = 0; break;
            case 7106: cic = 0x85; tv = 1; break;
            case 7: cic = 0xAC; tv = -1; break;
            case 5101: cic = 0xAC; tv = 0; break;
            case 8303: cic = 0xDD; tv = 0; break;
            case 1234: skip = 1; break;
            default: terminate("Unknown or unsupported CIC type '%d'.", global_cictype);
        }
        cart->cictype = global_cictype;
        pdprint("CIC set to %d (cic_seed = %d, tv_type = %d, skip = %s).\n", CRDEF_PROGRAM, global_cictype, cic, tv, skip ? "yes" : "no");
    }

    // Commit CIC and TV settings
    device_send_cmd_sc64(cart, DEV_CMD_CONFIG, DEV_CONFIG_CIC_SEED, (u32) cic, true);
    device_send_cmd_sc64(cart, DEV_CMD_CONFIG, DEV_CONFIG_TV_TYPE, (u32) tv, true);
    device_send_cmd_sc64(cart, DEV_CMD_CONFIG, DEV_CONFIG_SKIP_BOOTLOADER, (u32) skip, true);

    // Set savetype if provided
    if (global_savetype > 0 && global_savetype <= 6) {
        pdprint("Save type set to %d, %s.\n", CRDEF_PROGRAM, global_savetype, save_names[global_savetype - 1]);
    } else {
        global_savetype = 0;
    }

    // Commit save setting

    device_send_cmd_sc64(cart, DEV_CMD_CONFIG, DEV_CONFIG_SAVE_TYPE, global_savetype, true);

    // Init progressbar
    pdprint("\n", CRDEF_PROGRAM);
    progressbar_draw("Uploading ROM", CRDEF_PROGRAM, 0);

    // Prepare variables
    bytes_left = size;

    // Get start time
    upload_time_start = clock();

    // Prepare cart for write
    device_send_cmd_sc64(cart, DEV_CMD_WRITE, 0, size, false);

    // Loop until ROM has been fully written
    do {
        // Calculate current chunk size
        if (bytes_left < chunk) {
            chunk = bytes_left;
        }

        // Read ROM from file to buffer
        fread(rom_buffer, sizeof(u8), chunk, file);
        if (global_z64) {
            for (size_t i = 0; i < chunk; i += 2) {
                SWAP(rom_buffer[i], rom_buffer[i + 1]);
            }
        }

        // Push data
        testcommand(FT_Write(cart->handle, rom_buffer, chunk, &cart->bytes_written), "Error: Unable to write data to SummerCart64.\n");

        // Break from loop if not all bytes has been sent
        if (cart->bytes_written != chunk) {
            break;
        }

        // Update bytes left
        bytes_left -= cart->bytes_written;

        // Update progressbar
        progressbar_draw("Uploading ROM", CRDEF_PROGRAM, (size - bytes_left) / (float)size);
    } while (bytes_left > 0);

    // Free ROM buffer
    free(rom_buffer);

    if (bytes_left > 0) {
        // Throw error if upload was unsuccessful
        terminate("Error: SummerCart64 timed out");
    }

    // Check if write was successful
    device_check_reply_sc64(cart, DEV_CMD_WRITE);

    // Print that we've finished
    double upload_time = (double)(clock() - upload_time_start) / CLOCKS_PER_SEC;
    pdprint_replace("ROM successfully uploaded in %.2f seconds!\n", CRDEF_PROGRAM, upload_time);
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
    u32 transfer_size;

    // Sanitize and align size
    size &= 0x00FFFFFF;
    transfer_size = size + size % 2;

    // Prepare cart for transfer
    device_send_cmd_sc64(cart, DEV_CMD_DEBUG_WRITE, ((datatype & 0xFF) << 24) | size, transfer_size, false);

    // Push data
    testcommand(FT_Write(cart->handle, data, transfer_size, &cart->bytes_written), "Error: Unable to write data to SummerCart64.\n");

    if (cart->bytes_written != transfer_size) {
        // Throw error if transfer was unsuccessful
        terminate("Error: SummerCart64 timed out");
    }
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
