/***************************************************************
                       device_gopher64.cpp

Handles Gopher64 TCP communication.
***************************************************************/
#include "device_gopher64.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#include <unistd.h>
#endif

#include <sys/types.h>
#include <string.h>
#include <fcntl.h>
#include <chrono>
#include <thread>
#include <vector>

typedef struct
{
    int sockfd;
    std::vector<byte> buffer;
    uint32_t data_type;
    uint32_t data_size;
} Gopher64Device;

/*==============================
    device_test_gopher64
    Attempts to find Gopher64 device
    @param  A pointer to the cart context
    @return DEVICEERR_OK if the cart is an Gopher64,
            DEVICEERR_NOTCART if it isn't,
            Any other device error if problems ocurred
==============================*/

DeviceError device_test_gopher64(CartDevice *cart)
{
    if (device_open_gopher64(cart) == DEVICEERR_OK)
    {
        byte data[3] = {'N', '6', '4'};
        if (device_senddata_gopher64(cart, DATATYPE_TCPTEST, data, 3) == DEVICEERR_OK)
        {
            byte *buff = NULL;
            uint32_t dataheader = 0;
            std::this_thread::sleep_for(std::chrono::milliseconds(500)); // give gopher64 time to respond
            if (device_receivedata_gopher64(cart, &dataheader, &buff) == DEVICEERR_OK)
            {
                USBDataType type = (USBDataType)(dataheader >> 24);
                uint32_t size = dataheader & 0xFFFFFF;
                if (type == DATATYPE_TCPTEST && size == 3 && memcmp(buff, "N64", 3) == 0)
                {
                    free(buff);
                    if (device_close_gopher64(cart) == DEVICEERR_OK)
                    {
                        return DEVICEERR_OK;
                    }
                }
                else
                {
                    free(buff);
                    device_close_gopher64(cart);
                    return DEVICEERR_NOTCART;
                }
            }
            else
            {
                if (buff != NULL)
                {
                    free(buff);
                }
                device_close_gopher64(cart);
                return DEVICEERR_NOTCART;
            }
        }
        else
        {
            device_close_gopher64(cart);
            return DEVICEERR_NOTCART;
        }
    }
    return DEVICEERR_NOTCART;
}

/*==============================
    device_maxromsize_gopher64
    Gets the max ROM size that
    the Gopher64 supports
    @return The max ROM size
==============================*/

uint32_t device_maxromsize_gopher64()
{
    return 0xFC00000;
}

/*==============================
    device_rompadding_gopher64
    Calculates the correct ROM size
    for uploading on the Gopher64
    @param  The current ROM size
    @return The correct ROM size
            for uploading.
==============================*/

uint32_t device_rompadding_gopher64(uint32_t romsize)
{
    return romsize;
}

/*==============================
    device_explicitcic_gopher64
    Checks if the Gopher64 requires
    explicitly stating the CIC, and
    auto sets it based on the IPL if
    so
    @param  The 4KB bootcode
    @return Whether the CIC was changed
==============================*/

bool device_explicitcic_gopher64(byte *bootcode)
{
    device_setcic(cic_from_bootcode(bootcode));
    return true;
}

/*==============================
    device_testdebug_gopher64
    Checks whether the Gopher64 can use debug mode
    @param  A pointer to the cart context
    @return DEVICEERR_OK
==============================*/

DeviceError device_testdebug_gopher64(CartDevice *cart)
{
    (void)(cart); // Ignore unused paramater warning

    // Gopher64 supports debug mode
    return DEVICEERR_OK;
}

/*==============================
    device_open_gopher64
    Opens the TCP connection
    @param  A pointer to the cart context
    @return The device error, or OK
==============================*/

DeviceError device_open_gopher64(CartDevice *cart)
{
    struct addrinfo hints, *res;
    Gopher64Device *device = new Gopher64Device;

    memset(&hints, 0, sizeof hints);
    hints.ai_socktype = SOCK_STREAM; // TCP

    // Resolve localhost and port
    if (getaddrinfo("localhost", "64000", &hints, &res) != 0)
    {
        delete device;
        return DEVICEERR_NOTCART;
    }

    // Create socket
    device->sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

    if (device->sockfd < 0)
    {
        freeaddrinfo(res);
        delete device;
        return DEVICEERR_NOTCART;
    }

    // Connect
    if (connect(device->sockfd, res->ai_addr, res->ai_addrlen) < 0)
    {
#ifdef _WIN32
        closesocket(device->sockfd);
#else
        close(device->sockfd);
#endif
        freeaddrinfo(res);
        delete device;
        return DEVICEERR_NOTCART;
    }

#ifdef _WIN32
    u_long non_blocking = 1;
    // Set the socket to non-blocking
    if (ioctlsocket(device->sockfd, FIONBIO, &non_blocking) != 0)
    {
        closesocket(device->sockfd);
        freeaddrinfo(res);
        delete device;
        return DEVICEERR_NOTCART;
    }
#else
    int flags = fcntl(device->sockfd, F_GETFL, 0);
    // Set the socket to non-blocking
    if (fcntl(device->sockfd, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        close(device->sockfd);
        freeaddrinfo(res);
        delete device;
        return DEVICEERR_NOTCART;
    }
#endif

    device->data_size = 0;
    device->data_type = 0;
    cart->structure = device;
    return DEVICEERR_OK;
}

/*==============================
    device_sendrom_gopher64
    Sends the ROM to the flashcart
    @param  A pointer to the cart context
    @param  A pointer to the ROM to send
    @param  The size of the ROM
    @return The device error, or OK
==============================*/

DeviceError device_sendrom_gopher64(CartDevice *cart, byte *rom, uint32_t size)
{
    return device_senddata_gopher64(cart, DATATYPE_ROMUPLOAD, rom, size);
}

/*==============================
    device_tcp_send_gopher64
    Sends TCP data to Gopher64
    @param  socket file descriptor
    @param  A pointer to the data to send
    @param  The size of the data
    @return The device error, or OK
==============================*/

static DeviceError device_tcp_send_gopher64(int sockfd, void *data, uint32_t size)
{
    size_t total_sent = 0;

    while (total_sent < size)
    {
        int sent = send(sockfd, (const char *)data + total_sent, size - total_sent, 0);
        if (sent > 0)
        {
            total_sent += sent;
        }
    }
    return DEVICEERR_OK;
}

/*==============================
    device_senddata_gopher64
    Sends data to Gopher64
    @param  A pointer to the cart context
    @param  The datatype that is being sent
    @param  A buffer containing said data
    @param  The size of the data
    @return The device error, or OK
==============================*/

DeviceError device_senddata_gopher64(CartDevice *cart, USBDataType datatype, byte *data, uint32_t size)
{
    Gopher64Device *device = (Gopher64Device *)cart->structure;

    uint32_t data_type = swap_endian((uint32_t)datatype);
    if (device_tcp_send_gopher64(device->sockfd, &data_type, sizeof(uint32_t)) != DEVICEERR_OK)
    {
        return DEVICEERR_WRITEFAIL;
    }
    uint32_t swapped_size = swap_endian(size);
    if (device_tcp_send_gopher64(device->sockfd, &swapped_size, sizeof(uint32_t)) != DEVICEERR_OK)
    {
        return DEVICEERR_WRITEFAIL;
    }
    if (device_tcp_send_gopher64(device->sockfd, data, size) != DEVICEERR_OK)
    {
        return DEVICEERR_WRITEFAIL;
    }
    return DEVICEERR_OK;
}

/*==============================
    device_receivedata_gopher64
    Receives data from Gopher64
    @param  A pointer to the cart context
    @param  A pointer to an 32-bit value where
            the received data header will be
            stored.
    @param  A pointer to a byte buffer pointer
            where the data will be malloc'ed into.
    @return The device error, or OK
==============================*/

DeviceError device_receivedata_gopher64(CartDevice *cart, uint32_t *dataheader, byte **buff)
{
    *dataheader = 0;
    *buff = NULL;

    Gopher64Device *device = (Gopher64Device *)cart->structure;
    char local_buffer[4096];

    while (true)
    {
        int n = recv(device->sockfd, &local_buffer[0], sizeof(local_buffer), 0);
        if (n > 0)
            device->buffer.insert(device->buffer.end(), &local_buffer[0], &local_buffer[n]);
        else
            break;
    }

    if (device->data_type == 0 && device->buffer.size() >= sizeof(uint32_t))
    {
        memcpy(&device->data_type, device->buffer.data(), sizeof(uint32_t));
        device->data_type = swap_endian(device->data_type);
        device->buffer.erase(device->buffer.begin(), device->buffer.begin() + sizeof(uint32_t));
    }
    if (device->data_type != 0 && device->data_size == 0 && device->buffer.size() >= sizeof(uint32_t))
    {
        memcpy(&device->data_size, device->buffer.data(), sizeof(uint32_t));
        device->data_size = swap_endian(device->data_size);
        device->buffer.erase(device->buffer.begin(), device->buffer.begin() + sizeof(uint32_t));
    }
    if (device->data_type != 0 && device->data_size != 0)
    {
        if (device->buffer.size() >= device->data_size)
        {
            *dataheader = ((device->data_type & 0xFF) << 24) | (device->data_size & 0xFFFFFF);
            *buff = (byte *)malloc(device->data_size);
            memcpy(*buff, device->buffer.data(), device->data_size);
            device->buffer.erase(device->buffer.begin(), device->buffer.begin() + device->data_size);
            device->data_type = 0;
            device->data_size = 0;
            return DEVICEERR_OK;
        }
    }
    return DEVICEERR_OK;
}

/*==============================
    device_close_gopher64
    Closes the TCP connection
    @param  A pointer to the cart context
    @return The device error, or OK
==============================*/

DeviceError device_close_gopher64(CartDevice *cart)
{
    Gopher64Device *device = (Gopher64Device *)cart->structure;
#ifdef _WIN32
    closesocket(device->sockfd);
#else
    close(device->sockfd);
#endif
    free(device);
    cart->structure = NULL;
    return DEVICEERR_OK;
}
