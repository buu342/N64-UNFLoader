/***************************************************************
                       device_sc64.cpp

Handles SummerCart64 USB communication.
https://github.com/Polprzewodnikowy/SummerCollection

***************************************************************/

#include "main.h"
#include "helper.h"
#include "device_sc64.h"


/*********************************
           Data macros
*********************************/

#ifndef ALIGN
#define ALIGN(x, a) (((x) % (a)) ? ((x) + ((a) - ((x) % (a)))) : (x))
#endif

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif


/*********************************
        Function Prototypes
*********************************/

static FT_STATUS device_read_manufacturer_sc64(ftdi_context_t* cart, int index, char* manufacturer);
static void device_sendcmd_sc64(ftdi_context_t* cart, u8 command, u8* write_buffer, u32 write_length, u8* read_buffer, u32 read_length, bool reply, u32 numparams, ...);
static void device_write_bank_sc64(ftdi_context_t* cart, u8 bank, u32 offset, u8* write_buffer, u32 write_length);


/*==============================
    device_read_manufacturer_sc64
    Reads manufacturer string from EEPROM
    @param A pointer to the cart context
    @param A pointer to manufacturer string buffer
    @returns Last status from D2XX function call
==============================*/

static FT_STATUS device_read_manufacturer_sc64(ftdi_context_t* cart, int index, char* manufacturer)
{
    FT_STATUS status;
    FT_PROGRAM_DATA ft_program_data;

    // Open device
    status = FT_Open(index, &cart->handle);
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

static void device_sendcmd_sc64(ftdi_context_t* cart, u8 command, u8* write_buffer, u32 write_length, u8* read_buffer, u32 read_length, bool reply, u32 numparams, ...)
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
    device_write_bank_sc64
    Writes data to specified bank at offset
    @param A pointer to the cart context
    @param Bank to write to
    @param Bank offset
    @param A pointer to write data buffer
    @param Length of data to be written
==============================*/

static void device_write_bank_sc64(ftdi_context_t* cart, u8 bank, u32 offset, u8* write_buffer, u32 write_length)
{
    device_sendcmd_sc64(cart, DEV_CMD_WRITE, write_buffer, write_length, NULL, 0, true, DEV_RW_PARAMS(offset, bank, write_length));
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

    bool candidate = (
        strcmp(cart->dev_info[index].Description, "Arrow USB Blaster B") == 0 &&
        cart->dev_info[index].ID == 0x04036010
    );

    if (candidate) {
        cart->status = device_read_manufacturer_sc64(cart, index, manufacturer);

        return (cart->status == FT_OK && strcmp(manufacturer, "Polprzewodnikowy") == 0);
    }

    return false;
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
    testcommand(FT_SetLatencyTimer(cart->handle, 0), "Error: Unable to set flashcart latency timer.\n");

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
    u32 gpio;
    u32 boot_config;
    u32 cart_config;
    u32 sram_address;

    // Calculate suitable chunk size
    chunk_size = DEV_MAX_RW_BYTES / 4;

    // Allocate ROM buffer
    rom_buffer = (u8 *)malloc(chunk_size * sizeof(u8));
    if (rom_buffer == NULL) {
        terminate("Error: Unable to allocate memory for buffer.\n");
    }

    // Reset console and wait 500 ms for PRE_NMI event
    gpio = swap_endian(DEV_GPIO_RESET);
    device_write_bank_sc64(cart, DEV_BANK_CART, DEV_OFFSET_GPIO, (u8*)(&gpio), 4);
    pdprint("Resetting console... ", CRDEF_PROGRAM);
    #ifndef LINUX
        Sleep(500);
    #else
        usleep(500);
    #endif
    pdprint("done.\n", CRDEF_PROGRAM);

    // Set unknown CIC and TV type as default
    boot_config = DEV_BOOT_SKIP_MENU;

    // Set CIC and TV type if provided
     if (global_cictype != -1 && cart->cictype == 0) {
        switch (global_cictype) {
            case 0:
            case 6101: boot_config |= DEV_BOOT(DEV_TV_TYPE_NTSC, 0x13F); break;
            case 1:
            case 6102: boot_config |= DEV_BOOT(DEV_TV_TYPE_NTSC, 0x3F); break;
            case 2:
            case 7101: boot_config |= DEV_BOOT(DEV_TV_TYPE_PAL, 0x3F); break;
            case 3:
            case 7102: boot_config |= DEV_BOOT(DEV_TV_TYPE_PAL, 0x13F); break;
            case 4:
            case 103:
            case 6103: boot_config |= DEV_BOOT(DEV_TV_TYPE_NTSC, 0x78); break;
            case 7103: boot_config |= DEV_BOOT(DEV_TV_TYPE_PAL, 0x78); break;
            case 5:
            case 105:
            case 6105: boot_config |= DEV_BOOT(DEV_TV_TYPE_NTSC, 0x91); break;
            case 7105: boot_config |= DEV_BOOT(DEV_TV_TYPE_PAL, 0x91); break;
            case 6:
            case 106:
            case 6106: boot_config |= DEV_BOOT(DEV_TV_TYPE_NTSC, 0x85); break;
            case 7106: boot_config |= DEV_BOOT(DEV_TV_TYPE_PAL, 0x85); break;
            case 7:
            case 5101: boot_config |= DEV_BOOT(DEV_TV_TYPE_NTSC, 0xAC); break;
            case 8303: boot_config |= DEV_BOOT(DEV_TV_TYPE_NTSC, 0xDD); break;
            default: terminate("Unknown or unsupported CIC type '%d'.", global_cictype);
        }
        cart->cictype = global_cictype;
        pdprint("CIC set to %d.\n", CRDEF_PROGRAM, global_cictype);
    }

    // Write boot override register
    boot_config = swap_endian(boot_config);
    device_write_bank_sc64(cart, DEV_BANK_CART, DEV_OFFSET_BOOT, (u8 *)(&boot_config), 4);

    // Set no save type as default
    cart_config = 0;

    // Set savetype if provided
    if (global_savetype != 0) {
        switch (global_savetype) {
            case 1: cart_config = DEV_SCR_EEPROM_ENABLE; break;
            case 2: cart_config = DEV_SCR_EEPROM_16K_MODE | DEV_SCR_EEPROM_ENABLE; break;
            case 3: cart_config = DEV_SCR_SRAM_ENABLE; break;
            case 5: cart_config = DEV_SCR_SRAM_768K_MODE | DEV_SCR_SRAM_ENABLE; break;
            case 4:
            case 6: cart_config = DEV_SCR_FLASHRAM_ENABLE; break;
            default: terminate("Unknown or unsupported save type '%d'.", global_savetype);
        }
        pdprint("Save type set to %d.\n", CRDEF_PROGRAM, global_savetype);
    }

    // Write save type
    cart_config = swap_endian(cart_config);
    device_write_bank_sc64(cart, DEV_BANK_CART, DEV_OFFSET_SCR, (u8 *)(&cart_config), 4);

    // Set starting SRAM location in SDRAM
    if (global_savetype == 3 || global_savetype == 5) {
        // Locate at the end of SDRAM
        sram_address = DEV_SDRAM_SIZE - (32 * 1024 * ((global_savetype == 5) ? 3 : 1));
        sram_address = swap_endian(sram_address);
        device_write_bank_sc64(cart, DEV_BANK_CART, DEV_OFFSET_SRAM_ADDR, (u8 *)(&sram_address), 4);
    }

    // Init progressbar
    pdprint("\n", CRDEF_PROGRAM);
    progressbar_draw("Uploading ROM", CRDEF_PROGRAM, 0);

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
            for (size_t i = 0; i < chunk_size; i += 2) {
                SWAP(rom_buffer[i], rom_buffer[i + 1]);
            }
        }

        // Send data to SummerCart64
        device_write_bank_sc64(cart, DEV_BANK_SDRAM, offset, rom_buffer, chunk_size);

        // Break from loop if not all bytes has been sent
        if (cart->bytes_written != chunk_size) {
            break;
        }

        // Update offset and bytes left
        offset += cart->bytes_written;
        bytes_left -= cart->bytes_written;

        // Update progressbar
        progressbar_draw("Uploading ROM", CRDEF_PROGRAM, (size - bytes_left) / (float)size);
    } while (bytes_left > 0);

    // Free ROM buffer
    free(rom_buffer);

    // Release reset
    gpio = swap_endian(~DEV_GPIO_RESET);
    device_write_bank_sc64(cart, DEV_BANK_CART, DEV_OFFSET_GPIO, (u8*)(&gpio), 4);

    if (bytes_left > 0) {
        // Throw error if upload was unsuccessful
        terminate("Error: SummerCart64 timed out");
    }

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
    // Calculate transfer length
    u32 transfer_length = ALIGN(8 + size + 4, 4);

    // Allocate data buffer
    u8 *buff = (u8 *) malloc(transfer_length);

    // Create header
    u32 header = swap_endian((size & 0x00FFFFFF) | datatype << 24);

    // Copy data to buffer
    memcpy(buff, "DMA@", 4);
    memcpy(buff + 4, &header, 4);
    memcpy(buff + 8, data, size);
    memcpy(buff + 8 + size, "CMPH", 4);

    // Calculate number of transfers
    int num_transfers = (transfer_length / DEV_MAX_RW_BYTES) + 1;

    // Set buffer pointer
    u8 *buff_ptr = buff;

    for (int i = 0; i < num_transfers; i++) {
        // Calculate block length
        u32 block_length = MIN(transfer_length, DEV_MAX_RW_BYTES);
        
        // Send block
        device_sendcmd_sc64(cart, DEV_CMD_DEBUG_WRITE, buff_ptr, block_length, NULL, 0, false, 1, DEV_DEBUG_WRITE_PARAM_1(block_length));
        
        // Update tracking variables
        transfer_length -= block_length;
        buff_ptr += block_length;
    }

    // Free data buffer
    free(buff);
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
