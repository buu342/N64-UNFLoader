/***************************************************************
                       device_sc64.cpp

Handles SC64 USB communication.
https://github.com/Polprzewodnikowy/SummerCart64
***************************************************************/

#include <chrono>
#include <cstring>
#include <deque>
#include <thread>
#include "device_sc64.h"
#include "Include/ftd2xx.h"

/*********************************
              Macros
*********************************/

#define SUPPORTED_MAJOR_VERSION 2
#define SUPPORTED_MINOR_VERSION 14

#define CMD_IDENTIFIER_GET 'v'
#define CMD_VERSION_GET 'V'
#define CMD_STATE_RESET 'R'
#define CMD_CIC_PARAMS_SET 'B'
#define CMD_CONFIG_SET 'C'
#define CMD_MEMORY_WRITE 'M'
#define CMD_DEBUG_WRITE 'U'
#define CMD_FLASH_WAIT_BUSY 'p'
#define CMD_FLASH_ERASE_BLOCK 'P'
#define SC64_V2_IDENTIFIER "SCv2"

#define CFG_ID_ROM_SHADOW_ENABLE 2
#define CFG_ID_BOOT_MODE 5
#define CFG_ID_SAVE_TYPE 6
#define CFG_ID_ROM_EXTENDED_ENABLE 14

#define BOOT_MODE_ROM 1
#define BOOT_MODE_DIRECT_ROM 3

#define MEMORY_ADDRESS_SDRAM 0x00000000
#define MEMORY_ADDRESS_SHADOW 0x04FE0000
#define MEMORY_ADDRESS_EXTENDED 0x04000000

#define MEMORY_SIZE_SDRAM (64 * 1024 * 1024)
#define MEMORY_SIZE_SHADOW (128 * 1024)
#define MEMORY_SIZE_EXTENDED (14 * 1024 * 1024)

#define ROM_UPLOAD_CHUNK_SIZE (1 * 1024 * 1024)

#define USB_PACKET_DEBUG 'U'

#define U16(x) ((uint16_t)((x)[0] << 8 | (x)[1]))
#define U32(x) ((uint32_t)((x)[0] << 24 | (x)[1] << 16 | (x)[2] << 8 | (x)[3]))

// CIC params format:
// arg1: [31:25] unused
//          [24] disable CIC
//       [23:16] CIC seed
//        [15:0] IPL3 checksum (upper 16 bits)
// arg2:  [31:0] IPL3 checksum (lower 32 bits)
#define CIC_PARAMS(p, s, c)                              \
    {                                                    \
        (p)[0] = (((s) << 16) | (((c) >> 32) & 0xFFFF)); \
        (p)[1] = ((c) & (0xFFFFFFFF));                   \
    }

/*********************************
             Typedefs
*********************************/

typedef enum
{
    RESPONSE,
    CMDFAIL,
    PACKET,
    UNKNOWN,
} SC64DataType;

typedef struct
{
    uint8_t id;
    uint32_t size;
    std::unique_ptr<uint8_t[]> data;
} SC64Packet;

typedef struct
{
    DWORD device_number;
    FT_HANDLE handle;
    std::deque<SC64Packet> packets;
} SC64Device;

/*==============================
    device_reset_and_sync_sc64
    Resets and syncs SC64 communication
    @param  A pointer to the cart context
    @return The device error, or OK
==============================*/

static DeviceError device_reset_and_sync_sc64(SC64Device *device)
{
    ULONG modem_status;

    // Perform controller reset by setting DTR line and checking DSR line status
    if (FT_SetDtr(device->handle) != FT_OK)
        return DEVICEERR_SETDTRFAIL;
    for (int i = 0; i < 100; i++)
    {
        // Purge USB contents
        if (FT_Purge(device->handle, FT_PURGE_RX | FT_PURGE_TX) != FT_OK)
            return DEVICEERR_PURGEFAIL;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (FT_GetModemStatus(device->handle, &modem_status) != FT_OK)
            return DEVICEERR_GETMODEMSTATUSFAIL;
        if (modem_status & 0x20)
            break;
    }
    if (!(modem_status & 0x20))
        return DEVICEERR_SC64_CTRLRESETFAIL;

    // Purge USB contents again
    if (FT_Purge(device->handle, FT_PURGE_RX | FT_PURGE_TX) != FT_OK)
        return DEVICEERR_PURGEFAIL;

    // Release reset
    if (FT_ClrDtr(device->handle) != FT_OK)
        return DEVICEERR_CLEARDTRFAIL;
    for (int i = 0; i < 100; i++)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (FT_GetModemStatus(device->handle, &modem_status) != FT_OK)
            return DEVICEERR_GETMODEMSTATUSFAIL;
        if (!(modem_status & 0x20))
            break;
    }
    if (modem_status & 0x20)
        return DEVICEERR_SC64_CTRLRELEASEFAIL;

    // Flush packet queue
    device->packets.clear();

    return DEVICEERR_OK;
}

/*==============================
    device_send_command_sc64
    Prepares and sends command and parameters to the SC64
    @param  A pointer to the device handle
    @param  Command ID
    @param  First argument value
    @param  Second argument value
    @return The device error, or OK
==============================*/

static DeviceError device_send_command_sc64(SC64Device *device, uint8_t cmd, uint32_t arg1, uint32_t arg2)
{
    DWORD bytes;
    uint8_t header[12];

    // Prepare command header
    header[0] = (uint8_t)'C';
    header[1] = (uint8_t)'M';
    header[2] = (uint8_t)'D';
    header[3] = cmd;
    *(uint32_t *)(&header[4]) = swap_endian(arg1);
    *(uint32_t *)(&header[8]) = swap_endian(arg2);

    // Send command and parameters
    if (FT_Write(device->handle, header, sizeof(header), &bytes) != FT_OK)
        return DEVICEERR_WRITEFAIL;
    if (bytes != sizeof(header))
        return DEVICEERR_TXREPLYMISMATCH;

    return DEVICEERR_OK;
}

/*==============================
    device_process_incoming_data_sc64
    Tries to receive response/packet from the SC64
    Any received packet is stored in the packet queue
    @param  A pointer to the device handle
    @param  A pointer to the response packet structure
    @return Device error
==============================*/

static DeviceError device_process_incoming_data_sc64(SC64Device *device, SC64Packet *response)
{
    DWORD bytes;
    uint8_t buffer[4];

    // If processing data only for packets return if there's no header data yet
    if (response == NULL)
    {
        if (FT_GetQueueStatus(device->handle, &bytes) != FT_OK)
            return DEVICEERR_POLLFAIL;
        if (bytes < 4)
            return DEVICEERR_OK;
    }

    while (true)
    {
        // Read response/packet header
        if (FT_Read(device->handle, buffer, 4, &bytes) != FT_OK)
            return DEVICEERR_READFAIL;
        if (bytes != 4)
            return DEVICEERR_BADPACKSIZE;

        // Decode response/packet header
        auto datatype = SC64DataType::UNKNOWN;
        if (memcmp(buffer, "CMP", 3) == 0)
            datatype = SC64DataType::RESPONSE;
        else if (memcmp(buffer, "ERR", 3) == 0)
            datatype = SC64DataType::CMDFAIL;
        else if (memcmp(buffer, "PKT", 3) == 0)
            datatype = SC64DataType::PACKET;
        else
            return DEVICEERR_SC64_COMMFAIL;
        uint8_t id = buffer[3];

        // Read response/packet size
        if (FT_Read(device->handle, &buffer, 4, &bytes) != FT_OK)
            return DEVICEERR_READFAIL;
        if (bytes != 4)
            return DEVICEERR_BADPACKSIZE;
        uint32_t size = U32(buffer);

        // Read response/packet data
        std::unique_ptr<uint8_t[]> data(new uint8_t[size]);
        if (data.get() == NULL)
            return DEVICEERR_MALLOCFAIL;
        if (size > 0)
        {
            if (FT_Read(device->handle, data.get(), size, &bytes) != FT_OK)
                return DEVICEERR_READFAIL;
            if (bytes != size)
                return DEVICEERR_BADPACKSIZE;
        }

        // Save response/packet
        switch (datatype)
        {
        case SC64DataType::RESPONSE:
        case SC64DataType::CMDFAIL:
            if (response == NULL)
                return DEVICEERR_SC64_COMMFAIL;
            response->id = id;
            response->size = size;
            response->data = std::move(data);
            return datatype == SC64DataType::CMDFAIL ? DEVICEERR_SC64_CMDFAIL : DEVICEERR_OK;

        case SC64DataType::PACKET:
            device->packets.push_back(SC64Packet{id, size, std::move(data)});
            if (response == NULL)
                return DEVICEERR_OK;
            break;

        default:
            return DEVICEERR_SC64_COMMFAIL;
        }
    }
}

/*==============================
    device_execute_command_sc64
    Executes command on the SC64
    @param  A pointer to the device handle
    @param  Command ID
    @param  First argument value
    @param  Second argument value
    @param  A pointer to TX data or NULL
    @param  TX data size
    @param  A pointer to the response packet structure or NULL if command does not send response
    @return The device error, or OK
==============================*/

static DeviceError device_execute_command_sc64(SC64Device *device, uint8_t id, uint32_t arg1, uint32_t arg2, uint8_t *data, uint32_t size, SC64Packet *response)
{
    DeviceError err;

    // Send command header and parameters
    err = device_send_command_sc64(device, id, arg1, arg2);
    if (err != DEVICEERR_OK)
        return err;

    // Send command data if required
    if (data != NULL && size > 0)
    {
        DWORD bytes;
        if (FT_Write(device->handle, data, size, &bytes) != FT_OK)
            return DEVICEERR_WRITEFAIL;
        if (bytes != size)
            return DEVICEERR_TXREPLYMISMATCH;
    }

    // Read response if required
    if (response != NULL)
    {
        err = device_process_incoming_data_sc64(device, response);
        if (err != DEVICEERR_OK)
            return err;
        if (response->id != id)
            return DEVICEERR_SC64_COMMFAIL;
    }

    return DEVICEERR_OK;
}

/*==============================
    device_program_flash_sc64
    Programs flash memory on the SC64
    @param  A pointer to the device handle
    @param  Flash memory address
    @param  A pointer to data
    @param  Data size
    @param  A pointer to the progress lambda callback
    @return The device error, or OK
==============================*/

template <typename F>
static DeviceError device_program_flash_sc64(SC64Device *device, uint32_t address, uint8_t *data, uint32_t size, const F *progress)
{
    DeviceError err;
    SC64Packet response;

    // Get flash erase block size
    err = device_execute_command_sc64(device, CMD_FLASH_WAIT_BUSY, (uint32_t)(false), 0, NULL, 0, &response);
    if (err != DEVICEERR_OK)
        return err;
    uint32_t erase_block_size = U32(response.data.get());

    // Erase and program flash in loop
    for (uint32_t offset = 0; offset < size; offset += erase_block_size)
    {
        // Calculate block size
        uint32_t bytes_do = erase_block_size;
        if ((size - offset) < erase_block_size)
            bytes_do = size - offset;

        // Return an error if the upload was cancelled
        if (device_uploadcancelled())
            return DEVICEERR_UPLOADCANCELLED;

        // Erase flash block
        err = device_execute_command_sc64(device, CMD_FLASH_ERASE_BLOCK, address + offset, 0, NULL, 0, &response);
        if (err != DEVICEERR_OK)
            return err;

        // Program flash block
        err = device_execute_command_sc64(device, CMD_MEMORY_WRITE, address + offset, bytes_do, data + offset, bytes_do, &response);
        if (err != DEVICEERR_OK)
            return err;

        // Update progress
        if (progress != NULL)
            (*progress)(offset + bytes_do);
    }

    // Wait for flash to finish program operation
    err = device_execute_command_sc64(device, CMD_FLASH_WAIT_BUSY, (uint32_t)(true), 0, NULL, 0, &response);
    if (err != DEVICEERR_OK)
        return err;

    return DEVICEERR_OK;
}

/*==============================
    device_test_sc64
    Attempts to find SC64 device
    @param  A pointer to the cart context
    @return DEVICEERR_OK if the cart is an SC64,
            DEVICEERR_NOTCART if it isn't,
            Any other device error if problems ocurred
==============================*/

DeviceError device_test_sc64(CartDevice *cart)
{
    DWORD device_count;

    // Initialize FTDI
    if (FT_CreateDeviceInfoList(&device_count) != FT_OK)
        return DEVICEERR_USBBUSY;

    // Check if the device exists
    if (device_count == 0)
        return DEVICEERR_NODEVICES;

    // Allocate storage and get device info list
    std::unique_ptr<FT_DEVICE_LIST_INFO_NODE[]> device_info(new FT_DEVICE_LIST_INFO_NODE[device_count]);
    FT_GetDeviceInfoList(device_info.get(), &device_count);

    // Search the devices
    for (DWORD i = 0; i < device_count; i++)
    {
        // Look for SC64
        if (device_info[i].ID == 0x04036014 && memcmp(device_info[i].Description, "SC64", 4) == 0)
        {
            SC64Device *device = new SC64Device;
            if (device == NULL)
                return DEVICEERR_MALLOCFAIL;
            device->device_number = i;
            device->handle = NULL;
            device->packets = std::deque<SC64Packet>();
            cart->structure = device;
            return DEVICEERR_OK;
        }
    }

    // Could not find the flashcart
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
    return MEMORY_SIZE_SDRAM + MEMORY_SIZE_EXTENDED;
}

/*==============================
    device_rompadding_sc64
    Calculates the correct ROM size 
    for uploading on the SC64
    @param  The current ROM size
    @return The correct ROM size 
            for uploading.
==============================*/

uint32_t device_rompadding_sc64(uint32_t romsize)
{
    return romsize;
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

bool device_explicitcic_sc64(byte *bootcode)
{
    device_setcic(cic_from_bootcode(bootcode));
    return true;
}

/*==============================
    device_testdebug_sc64
    Checks whether the SC64 can use debug mode
    @param  A pointer to the cart context
    @return DEVICEERR_OK
==============================*/

DeviceError device_testdebug_sc64(CartDevice *cart)
{
    (void)(cart); // Ignore unused paramater warning

    // All SC64 firmware versions support debug mode
    return DEVICEERR_OK;
}

/*==============================
    device_open_sc64
    Opens the USB pipe
    @param  A pointer to the cart context
    @return The device error, or OK
==============================*/

DeviceError device_open_sc64(CartDevice *cart)
{
    DeviceError err;
    SC64Device *device = (SC64Device *)cart->structure;
    SC64Packet response;

    // Open the cart
    if (FT_Open(device->device_number, &device->handle) != FT_OK || device->handle == NULL)
        return DEVICEERR_CANTOPEN;

    // Reset the cart and set its timeouts and latency timer
    if (FT_ResetDevice(device->handle) != FT_OK)
        return DEVICEERR_RESETFAIL;
    if (FT_SetTimeouts(device->handle, 5000, 5000) != FT_OK)
        return DEVICEERR_TIMEOUTSETFAIL;

    // Reset and sync communication with SC64
    err = device_reset_and_sync_sc64(device);
    if (err != DEVICEERR_OK)
        return err;

    // Check SC64 identifier
    err = device_execute_command_sc64(device, CMD_IDENTIFIER_GET, 0, 0, NULL, 0, &response);
    if (err != DEVICEERR_OK)
        return err;
    if (response.size != 4 || memcmp(response.data.get(), SC64_V2_IDENTIFIER, 4) != 0)
        return DEVICEERR_SC64_FIRMWARECHECKFAIL;

    // Check firmware version
    err = device_execute_command_sc64(device, CMD_VERSION_GET, 0, 0, NULL, 0, &response);
    if (err != DEVICEERR_OK)
        return err;
    if (response.size != 8)
        return DEVICEERR_SC64_FIRMWARECHECKFAIL;
    auto major = U16(response.data.get() + 0);
    auto minor = U16(response.data.get() + 2);
    if (major != SUPPORTED_MAJOR_VERSION || minor < SUPPORTED_MINOR_VERSION)
        return DEVICEERR_SC64_FIRMWAREUNSUPPORTED;

    // Ok
    return DEVICEERR_OK;
}

/*==============================
    device_sendrom_sc64
    Sends the ROM to the flashcart
    @param  A pointer to the cart context
    @param  A pointer to the ROM to send
    @param  The size of the ROM
    @return The device error, or OK
==============================*/

DeviceError device_sendrom_sc64(CartDevice *cart, byte *rom, uint32_t size)
{
    DeviceError err;
    SC64Device *device = (SC64Device *)cart->structure;
    SC64Packet response;
    uint32_t bytes_done = 0;
    uint32_t sdram_size = size;

    // Reset SC64 state
    err = device_execute_command_sc64(device, CMD_STATE_RESET, 0, 0, NULL, 0, &response);
    if (err != DEVICEERR_OK)
        return err;

    // Set boot mode
    auto boot_mode = cart->cictype == CIC_NONE ? BOOT_MODE_ROM : BOOT_MODE_DIRECT_ROM;
    err = device_execute_command_sc64(device, CMD_CONFIG_SET, CFG_ID_BOOT_MODE, boot_mode, NULL, 0, &response);
    if (err != DEVICEERR_OK)
        return err;

    // Set CIC parameters if necessary
    if (cart->cictype != CIC_NONE)
    {
        uint32_t params[2] = {0, 0};
        switch (cart->cictype)
        {
            case CIC_6101:
                CIC_PARAMS(params, 0x3F, 0x45CC73EE317A);
                break;
            case CIC_7102:
                CIC_PARAMS(params, 0x3F, 0x44160EC5D9AF);
                break;
            case CIC_6102:
            case CIC_7101:
                CIC_PARAMS(params, 0x3F, 0xA536C0F1D859);
                break;
            case CIC_X103:
                CIC_PARAMS(params, 0x78, 0x586FD4709867);
                break;
            case CIC_X105:
                CIC_PARAMS(params, 0x91, 0x8618A45BC2D3);
                break;
            case CIC_X106:
                CIC_PARAMS(params, 0x85, 0x2BBAD4E6EB74);
                break;
            case CIC_5101:
                //CIC_PARAMS(params, 0xAC, 0x000000000000); // TODO: calculate checksum
                params[0] = 0;
                params[1] = 0;
                break;
            case CIC_8303:
                CIC_PARAMS(params, 0xDD, 0x32B294E2AB90);
                break;
            default:
                break;
        }
        err = device_execute_command_sc64(device, CMD_CIC_PARAMS_SET, params[0], params[1], NULL, 0, &response);
        if (err != DEVICEERR_OK)
            return err;
    }

    // Set save type
    auto save_type = (uint32_t)(cart->savetype == SAVE_FLASHRAMPKMN ? SAVE_FLASHRAM : cart->savetype);
    err = device_execute_command_sc64(device, CMD_CONFIG_SET, CFG_ID_SAVE_TYPE, save_type, NULL, 0, &response);
    if (err != DEVICEERR_OK)
        return err;

    // Decide if we need to use shadow and/or extended memory
    auto sdram_save = cart->savetype == SAVE_SRAM256 || cart->savetype == SAVE_SRAM768 || cart->savetype == SAVE_FLASHRAM || cart->savetype == SAVE_FLASHRAMPKMN;
    auto use_shadow_memory = sdram_save && size > (MEMORY_SIZE_SDRAM - MEMORY_SIZE_SHADOW);
    auto use_extended_memory = size > MEMORY_SIZE_SDRAM;

    // Truncate SDRAM size if shadow/extended memory is used
    if (use_extended_memory)
        sdram_size = MEMORY_SIZE_SDRAM;
    if (use_shadow_memory)
        sdram_size = (MEMORY_SIZE_SDRAM - MEMORY_SIZE_SHADOW);

    // Upload the ROM in a loop
    while (sdram_size > 0)
    {
        uint32_t bytes_do = ROM_UPLOAD_CHUNK_SIZE;
        if (sdram_size < bytes_do)
            bytes_do = sdram_size;

        if (device_uploadcancelled())
            break;

        err = device_execute_command_sc64(device, CMD_MEMORY_WRITE, MEMORY_ADDRESS_SDRAM + bytes_done, bytes_do, rom + bytes_done, bytes_do, &response);
        if (err != DEVICEERR_OK)
            return err;

        sdram_size -= bytes_do;
        bytes_done += bytes_do;
        device_setuploadprogress((((float)bytes_done) / ((float)size)) * 100.0f);
    }

    // Create progress callback for flash program operations
    auto progress = [&bytes_done, size](uint32_t bytes)
    {
        device_setuploadprogress((((float)bytes_done + bytes) / ((float)size)) * 100.0f);
    };

    // Program shadow memory if required
    if (use_shadow_memory && !device_uploadcancelled())
    {
        // Enable shadow memory
        err = device_execute_command_sc64(device, CMD_CONFIG_SET, CFG_ID_ROM_SHADOW_ENABLE, true, NULL, 0, &response);
        if (err != DEVICEERR_OK)
            return err;

        uint32_t shadow_size = size - (MEMORY_SIZE_SDRAM - MEMORY_SIZE_SHADOW);
        if (shadow_size > MEMORY_SIZE_SHADOW)
            shadow_size = MEMORY_SIZE_SHADOW;

        err = device_program_flash_sc64(device, MEMORY_ADDRESS_SHADOW, rom + bytes_done, shadow_size, &progress);
        if (err != DEVICEERR_OK)
            return err;

        bytes_done += shadow_size;
    }

    // Program extended memory if required
    if (use_extended_memory && !device_uploadcancelled())
    {
        // Enable extended memory
        err = device_execute_command_sc64(device, CMD_CONFIG_SET, CFG_ID_ROM_EXTENDED_ENABLE, true, NULL, 0, &response);
        if (err != DEVICEERR_OK)
            return err;

        uint32_t extended_size = size - MEMORY_SIZE_SDRAM;

        err = device_program_flash_sc64(device, MEMORY_ADDRESS_EXTENDED, rom + bytes_done, extended_size, &progress);
        if (err != DEVICEERR_OK)
            return err;

        bytes_done += extended_size;
    }

    // Return an error if the upload was cancelled
    if (device_uploadcancelled())
        return DEVICEERR_UPLOADCANCELLED;

    // Success
    device_setuploadprogress(100.0f);
    return DEVICEERR_OK;
}

/*==============================
    device_senddata_sc64
    Sends data to the SC64
    @param  A pointer to the cart context
    @param  The datatype that is being sent
    @param  A buffer containing said data
    @param  The size of the data
    @return The device error, or OK
==============================*/

DeviceError device_senddata_sc64(CartDevice *cart, USBDataType datatype, byte *data, uint32_t size)
{
    DeviceError err;
    SC64Device *device = (SC64Device *)cart->structure;

    device_setuploadprogress(0.0f);
    err = device_execute_command_sc64(device, CMD_DEBUG_WRITE, datatype, size, data, size, NULL);
    if (err != DEVICEERR_OK)
        return err;
    device_setuploadprogress(100.0f);

    return DEVICEERR_OK;
}

/*==============================
    device_receivedata_sc64
    Receives data from the SC64
    @param  A pointer to the cart context
    @param  A pointer to an 32-bit value where
            the received data header will be
            stored.
    @param  A pointer to a byte buffer pointer
            where the data will be malloc'ed into.
    @return The device error, or OK
==============================*/

DeviceError device_receivedata_sc64(CartDevice *cart, uint32_t *dataheader, byte **buff)
{
    DeviceError err;
    SC64Device *device = (SC64Device *)cart->structure;

    *dataheader = 0;
    *buff = NULL;

    // If packet queue is empty then try to fill it
    if (device->packets.empty())
    {
        err = device_process_incoming_data_sc64(device, NULL);
        if (err != DEVICEERR_OK)
            return err;
    }

    // If there are packets in the queue then try to read one
    if (!device->packets.empty())
    {
        // Get packet from the queue
        auto packet = std::move(device->packets.front());
        device->packets.pop_front();

        // We care only about debug packets
        if (packet.id == USB_PACKET_DEBUG)
        {
            device_setuploadprogress(0.0f);
            if (packet.size < 4)
                return DEVICEERR_SC64_COMMFAIL;
            *dataheader = U32(packet.data.get());
            auto size = *dataheader & 0xFFFFFF;
            if (size != (packet.size - 4))
                return DEVICEERR_SC64_COMMFAIL;
            *buff = (byte *)malloc(size);
            if (*buff == NULL)
                return DEVICEERR_MALLOCFAIL;
            memcpy(*buff, packet.data.get() + 4, size);
            device_setuploadprogress(100.0f);
        }
    }

    return DEVICEERR_OK;
}

/*==============================
    device_close_sc64
    Closes the USB pipe
    @param  A pointer to the cart context
    @return The device error, or OK
==============================*/

DeviceError device_close_sc64(CartDevice *cart)
{
    SC64Device *device = (SC64Device *)cart->structure;
    if (FT_Close(device->handle) != FT_OK)
        return DEVICEERR_CLOSEFAIL;
    free(device);
    cart->structure = NULL;
    return DEVICEERR_OK;
}