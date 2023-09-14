/***************************************************************
                            gdbstub.cpp
                               
Handles basic GDB communication
***************************************************************/ 

#ifndef LINUX
    #include <winsock2.h>
#else
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <netinet/tcp.h>
    #include <unistd.h>
    #include <signal.h>
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
#define LOG_ERRORS FALSE

#ifdef LINUX
    #define SOCKET       int
#endif


/*********************************
             Globals
*********************************/

SOCKET local_socket = -1;


/*==============================
    socket_connect
    TODO
==============================*/

static int socket_connect(char* address, char* port) 
{
    short sock;
    int optval;
    socklen_t socklen;
    struct sockaddr_in remote;

    // Create a socket for GDB
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == -1)
    {
        #if LOG_ERRORS
            log_colored("Unable to create socket for GDB\n", CRDEF_ERROR);
        #endif
        return -1;
    }

    // Setup the socket struct
    remote.sin_port = htons(atoi(port));
    remote.sin_family = PF_INET;
    remote.sin_addr.s_addr = inet_addr(address);
    if (bind(sock, (struct sockaddr *)&remote, sizeof(remote)) != 0)
    {
        #if LOG_ERRORS
            log_colored("Unable to bind socket for GDB\n", CRDEF_ERROR);
        #endif
        return -1;
    }

    // Listen for (at most one) client
    if (listen(sock, 1))
    {
        #if LOG_ERRORS
            log_colored("Unable to listen to socket for GDB\n", CRDEF_ERROR);
        #endif
        return -1;
    }

    // Accept a client which connects
    socklen = sizeof(remote);
    local_socket = accept(sock, (struct sockaddr *)&remote, &socklen);
    if (local_socket == -1)
    {
        #if LOG_ERRORS
            log_colored("Unable to accept socket for GDB\n", CRDEF_ERROR);
        #endif
        return -1;
    }

    // Enable TCP keep alive process
    optval = 1;
    setsockopt(local_socket, SOL_SOCKET, SO_KEEPALIVE, (char *)&optval, sizeof(optval));

    // Don't delay small packets, for better interactive response
    optval = 1;
    setsockopt(local_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&optval, sizeof(optval));

    // Cleanup
    close(sock);
    signal(SIGPIPE, SIG_IGN);  // So we don't exit if client dies
    return 0;
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
    char* addr;
    char* port;
    char* fulladdrcopy = (char*)malloc((strlen(fulladdr)+1)*sizeof(char));
    strcpy(fulladdrcopy, fulladdr);

    // Grab the address and port
    addr = strtok(fulladdrcopy, ":");
    port = strtok(NULL, ":");

    // Connect to the socket
    if (socket_connect(addr, port) != 0)
    {
        #if LOG_ERRORS
            log_colored("Unable to connect to socket: %d\n", CRDEF_ERROR, errno);
        #endif
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
        char buff[512];
        memset(buff, 0, 512*sizeof(char));
        socket_receive(local_socket, buff, 512);
        if (buff[0] != 0)
            log_simple("received: %s", buff);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}