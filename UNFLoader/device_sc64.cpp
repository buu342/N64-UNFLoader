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

static FT_STATUS device_read_manufacturer_sc64(ftdi_context_t* cart, char* manufacturer);
static void device_sendcmd_sc64(ftdi_context_t* cart, u8 command, u8* write_buffer, u32 write_length, u8* read_buffer, u32 read_length, bool reply, u8 numparams, ...);


/*==============================
    device_read_manufacturer_sc64
    Reads manufacturer string from EEPROM
    @param A pointer to the cart context
    @param A pointer to manufacturer string buffer
    @returns Last status from D2XX function call
==============================*/

static FT_STATUS device_read_manufacturer_sc64(ftdi_context_t* cart, char* manufacturer)
{
    FT_STATUS status;
    FT_PROGRAM_DATA ft_program_data;

    // Open device
    status = FT_Open(cart->device_index, &cart->handle);
    if (status != FT_OK) {
        return status;
    }

    // Clear FT_PROGRAM_DATA struct
    memset(&ft_program_data, 0, sizeof(FT_PROGRAM_DATA));

    // Prepare FT_PROGRAM_DATA struct
    ft_program_data.Signature1 = 0x00000000;
    ft_program_data.Signature2 = 0xFFFFFFFF;
    ft_program_data.Version = 3;    // FT2232 struct version
    ft_program_data.Manufacturer = manufacturer;

    // Read EEPROM
    status = FT_EE_Read(cart->handle, &ft_program_data);

    // Close device
    FT_Close(cart->handle);
    cart->handle = 0;

    return status;
}


/*==============================
    device_sendcmd_sc64
    Sends command with data to SummerCart64
    @param A pointer to the cart context
    @param The command to send
    @param A pointer to write data buffer
    @param Length of data to be written
    @param A pointer to read data buffer
    @param Length of data to be read
    @param A bool stating whether a reply should be expected
    @param The number of extra params to send
    @param The extra variadic commands to send
==============================*/

static void device_sendcmd_sc64(ftdi_context_t* cart, u8 command, u8* write_buffer, u32 write_length, u8* read_buffer, u32 read_length, bool reply, u8 numparams, ...)
{
    va_list params;
    u8 cmd_params_buff[12];
    u8 reply_buff[4];
    u32 bytes_to_process;
    DWORD bytes_processed;

    // Clear buffers
    memset(cmd_params_buff, 0, sizeof(cmd_params_buff));
    memset(reply_buff, 0, sizeof(reply_buff));

    // Prepare command
    cmd_params_buff[0] = (u8)'C';
    cmd_params_buff[1] = (u8)'M';
    cmd_params_buff[2] = (u8)'D';
    cmd_params_buff[3] = command;
    
    // Prepare parameters
    va_start(params, numparams);
    if (numparams > 2) {
        numparams = 2;
    }
    if (numparams > 0) {
        *(u32 *)(&cmd_params_buff[4]) = swap_endian(va_arg(params, u32));
    }
    if (numparams > 1) {
        *(u32 *)(&cmd_params_buff[8]) = swap_endian(va_arg(params, u32));
    }
    va_end(params);

    // Send command and parameters
    bytes_to_process = 4 + (numparams * 4);
    testcommand(FT_Write(cart->handle, cmd_params_buff, bytes_to_process, &bytes_processed), "Error: Unable to write command to SummerCart64.\n");
    if (bytes_processed != bytes_to_process) {
        terminate("Error: Actual bytes written amount is different than desired.\n");
    }

    // Write data if provided
    if (write_buffer != NULL && write_length > 0) {
        testcommand(FT_Write(cart->handle, write_buffer, write_length, &cart->bytes_written), "Error: Unable to write data to SummerCart64.\n");
        if (cart->bytes_written != write_length) {
            terminate("Error: Actual bytes written amount is different than desired.\n");
        }
    }

    // Read data if provided
    if (read_buffer != NULL && read_length > 0) {
        testcommand(FT_Read(cart->handle, read_buffer, read_length, &cart->bytes_read), "Error: Unable to read data from SummerCart64.\n");
        if (cart->bytes_read != read_length) {
            terminate("Error: Actual bytes read amount is different than desired.\n");
        }
    }

    // Check reply
    if (reply) {
        testcommand(FT_Read(cart->handle, reply_buff, 4, &bytes_processed), "Error: Unable to read completion signal.\n");
        if (bytes_processed != 4 || memcmp(reply_buff, "CMP", 3) != 0 || reply_buff[3] != command) {
            terminate("Error: Did not receive completion signal.\n");
        }
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
    char manufacturer[32];

    // Read manufacturer string
    cart->status = device_read_manufacturer_sc64(cart, manufacturer);

    return (
        cart->status == FT_OK &&
        strcmp(manufacturer, "Polprzewodnikowy") == 0 &&
        strcmp(cart->dev_info[index].Description, "Arrow USB Blaster B") == 0 &&
        cart->dev_info[index].ID == 0x04036010
    );
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

    // Reset the cart and set its timeouts
    testcommand(FT_ResetDevice(cart->handle), "Error: Unable to reset flashcart.\n");
    testcommand(FT_SetTimeouts(cart->handle, 5000, 5000), "Error: Unable to set flashcart timeouts.\n");

    // Reset bit mode for fast serial interface
    testcommand(FT_SetBitMode(cart->handle, 0xFF, FT_BITMODE_RESET), "Error: Unable to set bitmode %d.\n", FT_BITMODE_RESET);

    // Purge USB contents
    testcommand(FT_Purge(cart->handle, FT_PURGE_RX | FT_PURGE_TX), "Error: Unable to purge USB contents.\n");

    // Check cart identifier
    u8 identifier[4];
    device_sendcmd_sc64(cart, DEV_CMD_IDENTIFY, NULL, 0, identifier, 4, true, 0);
    if (memcmp(identifier, DEV_IDENTIFIER, 4) != 0) {
        terminate("Error: Identify response is different than expected, probably not a SummerCart64 device.\n");
    }
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
    size_t chunk_size;
    u8* rom_buffer;
    u32 offset;
    size_t bytes_left;
    time_t upload_time_start;

    // Calculate suitable chunk size
    chunk_size = DEV_MAX_RW_BYTES / 8;

    // Allocate ROM buffer
    rom_buffer = (u8 *)malloc(chunk_size * sizeof(u8));
    if (rom_buffer == NULL) {
        terminate("Error: Unable to allocate memory for buffer.\n");
    }

    // Init progressbar
    pdprint("\n", CRDEF_PROGRAM);
    progressbar_draw("Uploading ROM", 0);

    // Prepare variables
    offset = 0;
    bytes_left = size - (size % 4);

    // Get start time
    upload_time_start = clock();

    // Loop until ROM has been fully written
    do {
        // Calculate current chunk size
        if (bytes_left < chunk_size) {
            chunk_size = bytes_left;
        }

        // Read ROM from file to buffer
        fread(rom_buffer, sizeof(u8), chunk_size, file);
        if (global_z64) {
            for (int i = 0; i < chunk_size; i += 2) {
                SWAP(rom_buffer[i], rom_buffer[i + 1]);
            }
        }

        // Send data to SummerCart64
        device_sendcmd_sc64(cart, DEV_CMD_WRITE, rom_buffer, chunk_size, NULL, 0, true, 2, DEV_RW_PARAM_1(offset), DEV_RW_PARAM_2(DEV_BANK_ROM, chunk_size));     

        // Break from loop if not all bytes has been sent
        if (cart->bytes_written != chunk_size) {
            break;
        }

        // Update offset and bytes left
        offset += cart-> bytes_written;
        bytes_left -= cart->bytes_written;

        // Update progressbar
        progressbar_draw("Uploading ROM", (size - bytes_left) / (float)size);
    } while (bytes_left > 0);

    // Free ROM buffer
    free(rom_buffer);

    if (bytes_left > 0) {
        // Throw error if upload was unsuccessful
        terminate("Error: SummerCart64 timed out");
    } else {
        // Print that we've finished
        double upload_time = (double)(clock() - upload_time_start) / CLOCKS_PER_SEC;
        pdprint_replace("ROM successfully uploaded in %.2f seconds!\n", CRDEF_PROGRAM, upload_time);
    }
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
    // TODO: Currently no support in hardware
}


/*==============================
    device_close_sc64
    Closes the USB pipe
    @param A pointer to the cart context
==============================*/

void device_close_sc64(ftdi_context_t* cart)
{
    testcommand(FT_Close(cart->handle), "Error: Unable to close flashcart.\n");
}
