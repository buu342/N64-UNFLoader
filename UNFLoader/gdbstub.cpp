/***************************************************************
                            gdbstub.cpp
                               
Handles basic GDB communication
***************************************************************/ 

#ifndef LINUX
    #include <winsock2.h>
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
#endif
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <thread>
#include <chrono>
#include "gdbstub.h"
#include "helper.h"
#include "term.h"


/*********************************
              Macros
*********************************/

#define TIMEOUT 3

#ifdef LINUX
    #define SOCKET       short
#endif


/*********************************
             Globals
*********************************/

SOCKET local_socket = -1;


/*==============================
    socket_create
    TODO
==============================*/

static SOCKET socket_create()
{
    SOCKET sock;
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    return sock;
}


/*==============================
    socket_connect
    TODO
==============================*/

static int socket_connect(SOCKET sock, char* address, char* port) 
{
    int ret = -1;
    struct sockaddr_in remote = {0};
    remote.sin_addr.s_addr = inet_addr(address);
    remote.sin_family = AF_INET;
    remote.sin_port = htons(atoi(port));

    ret = connect(sock, (struct sockaddr *)&remote, sizeof(struct sockaddr_in));
    return ret;
}


/*==============================
    socket_send
    TODO
==============================*/

static int socket_send(SOCKET sock, char* request, size_t size)
{
    int ret = -1;
    struct timeval tv;
    tv.tv_sec = TIMEOUT;
    tv.tv_usec = 0;

    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv,sizeof(tv)) < 0)
        return -1;

    ret = send(sock, request, size, 0);
    return ret;
}


/*==============================
    socket_receive
    TODO
==============================*/

static int socket_receive(SOCKET sock, char* response, short size)
{
    int ret = -1;
    struct timeval tv;
    tv.tv_sec = TIMEOUT;
    tv.tv_usec = 0;

    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(tv)) < 0)
        return -1;

    ret = recv(sock, response, size, 0);
    return ret;
}


/*==============================
    gdb_connect
    TODO
==============================*/

void gdb_connect(char* fulladdr)
{
    int ret;
    char* addr;
    char* port;
    char* fulladdrcopy = (char*)malloc((strlen(fulladdr)+1)*sizeof(char));
    strcpy(fulladdrcopy, fulladdr);

    // Grab the address and port
    addr = strtok(fulladdrcopy, ":");
    port = strtok(NULL, ":");

    // Create a socket for GDB and connect to it
    local_socket = socket_create();
    if (local_socket == -1)
    {
        log_colored("Unable to create socket for GDB\n", CRDEF_ERROR);
        free(fulladdrcopy);
        return;
    }
    if (socket_connect(local_socket, addr, port) != 0)
    {
        log_colored("Unable to connect to socket, %d\n", CRDEF_ERROR, errno);
        local_socket = -1;
        free(fulladdrcopy);
        return;
    }

    // Cleanup
    free(fulladdrcopy);
}


/*==============================
    gdb_isconnected
    TODO
==============================*/

bool gdb_isconnected()
{
    return local_socket != -1;
}


/*==============================
    gdb_disconnect
    TODO
==============================*/

void gdb_disconnect()
{
    // TODO: Tell GDB we're disconnecting
    shutdown(local_socket, SHUT_RDWR);
    local_socket = -1;
}


/*==============================
    gdb_thread
    TODO
==============================*/

void gdb_thread(char* addr)
{
    gdb_connect(addr);
    while (gdb_isconnected())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}