/***************************************************************
                            gdbstub.cpp
                               
Handles basic GDB communication
***************************************************************/ 

#ifdef LINUX
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <netinet/tcp.h>
    #include <unistd.h>
    #include <signal.h>
#endif
#include "main.h"
#include "gdbstub.h"
#include "helper.h"
#include "term.h"
#include "debug.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <thread>
#include <chrono>
#include <string>
#include <queue>


/*********************************
              Macros
*********************************/

#define TIMEOUT 3
#define VERBOSE 1

#ifdef LINUX
    #define SOCKET          int
    #define INVALID_SOCKET  -1
#endif


/*********************************
              Enums
*********************************/

typedef enum {
    STATE_SEARCHING,
    STATE_HEADER,
    STATE_PACKETDATA,
    STATE_CHECKSUM,
} ParseState;


/*********************************
             Globals
*********************************/

static std::string local_packetdata = "";
static std::string local_packetchecksum = "";
static std::string local_lastreply = "";
static ParseState  local_parserstate = STATE_SEARCHING;
static SOCKET      local_socket = INVALID_SOCKET;


/*==============================
    socket_connect
    TODO
==============================*/

static int socket_connect(char* address, char* port) 
{
    int sock;
    int optval;
    #ifndef LINUX
        int socklen;
    #else
        socklen_t socklen;
    #endif
    struct sockaddr_in remote;

    // Create a socket for GDB
    sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
    {
        #if VERBOSE
            log_colored("Unable to create socket for GDB\n", CRDEF_ERROR);
        #endif
        return -1;
    }

    // Allow us to reuse this socket if it is killed
    optval = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));
    optval = 1;
    #ifndef LINUX
        setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof(optval));
        optval = 1;
        setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char*)&optval, sizeof(optval));
    #else
        setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, (char*)&optval, sizeof(optval));
    #endif
    // Setup the socket struct
    remote.sin_port = htons((short)atoi(port));
    remote.sin_family = PF_INET;
    remote.sin_addr.s_addr = inet_addr(address);
    if (bind(sock, (struct sockaddr *)&remote, sizeof(remote)) != 0)
    {
        #if VERBOSE
            log_colored("Unable to bind socket for GDB\n", CRDEF_ERROR);
        #endif
        return -1;
    }

    // Listen for (at most one) client
    if (listen(sock, 1))
    {
        #if VERBOSE
            log_colored("Unable to listen to socket for GDB\n", CRDEF_ERROR);
        #endif
        return -1;
    }

    // Accept a client which connects
    socklen = sizeof(remote);
    local_socket = accept(sock, (struct sockaddr *)&remote, &socklen);
    if (local_socket == INVALID_SOCKET)
    {
        #if VERBOSE
            log_colored("Unable to accept socket for GDB\n", CRDEF_ERROR);
        #endif
        return -1;
    }

    // Enable TCP keep alive process
    optval = 1;
    setsockopt(local_socket, SOL_SOCKET, SO_KEEPALIVE, (char*)&optval, sizeof(optval));

    // Don't delay small packets, for better interactive response
    optval = 1;
    setsockopt(local_socket, IPPROTO_TCP, TCP_NODELAY, (char*)&optval, sizeof(optval));

    // Cleanup
    #ifndef LINUX
        closesocket(sock);
    #else
        close(sock);
        signal(SIGPIPE, SIG_IGN);  // So we don't exit if client dies
    #endif
    return 0;
}


/*==============================
    socket_send
    TODO
==============================*/

static int socket_send(SOCKET sock, char* data, size_t size)
{
    int ret = -1;
    struct timeval tv;
    tv.tv_sec = TIMEOUT;
    tv.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&tv,sizeof(tv)) < 0)
        return -1;
    ret = send(sock, data, size, 0);
    return ret;
}


/*==============================
    socket_receive
    TODO
==============================*/

static int socket_receive(SOCKET sock, char* data, size_t size)
{
    int ret = -1;
    struct timeval tv;
    tv.tv_sec = TIMEOUT;
    tv.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&tv,sizeof(tv)) < 0)
        return -1;
    ret = recv(sock, data, size, 0);
    return ret;
}


/*==============================
    packet_getchecksum
    TODO
==============================*/

static uint8_t packet_getchecksum(std::string packet)
{
    uint32_t checksum = 0;
    uint32_t strln = packet.size();
    for (uint32_t i=0; i<strln; i++)
        checksum += packet[i];
    return (uint8_t)(checksum%256);
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
        #if VERBOSE
            #ifndef LINUX
                log_colored("Unable to connect to socket: %d\n", CRDEF_ERROR, WSAGetLastError());
            #else
                log_colored("Unable to connect to socket: %d\n", CRDEF_ERROR, errno);
            #endif
        #endif
        local_socket = INVALID_SOCKET;
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
    return local_socket != INVALID_SOCKET;
}


/*==============================
    gdb_disconnect
    TODO
==============================*/

void gdb_disconnect()
{
    #ifndef LINUX
        shutdown(local_socket, SD_BOTH);
    #else
        shutdown(local_socket, SHUT_RDWR);
    #endif
    local_socket = INVALID_SOCKET;
}


/*==============================
    gdb_parsepacket
    TODO
==============================*/

static void gdb_parsepacket(char* buff, int buffsize)
{
    int left = buffsize, read = 0;
    while (left > 0)
    {
        switch (local_parserstate)
        {
            case STATE_SEARCHING:
                while (left > 0 && buff[read] != '$')
                {
                    if (buff[read] == '-') // Resend last packet in case of failure
                        socket_send(local_socket, (char*)local_lastreply.c_str(), local_lastreply.size()+1);
                    if (buff[read] == '\x03') // CTRL+C from GDB
                        debug_send(DATATYPE_RDBPACKET, (char*)"\x03", 1+1);
                    read++;
                    left--;
                }
                if (buff[read] == '$')
                    local_parserstate = STATE_HEADER;
                break;
            case STATE_HEADER:
                if (left > 0 && buff[read] == '$')
                {
                    read++;
                    left--;
                    local_parserstate = STATE_PACKETDATA;
                }
                break;
            case STATE_PACKETDATA:
                while (left > 0 && buff[read] != '#') // Read bytes until we hit a checksum marker, or we run out of bytes
                {
                    local_packetdata += buff[read];
                    read++;
                    left--;
                }
                if (buff[read] == '#')
                    local_parserstate = STATE_CHECKSUM;
                break;
            case STATE_CHECKSUM:
                if (left > 0)
                {
                    if (buff[read] != '#')
                    {
                        local_packetchecksum += buff[read];
                        if (local_packetchecksum.size() == 2)
                        {
                            int checksum = packet_getchecksum(local_packetdata);

                            // Check if the checksum failed, if it didn't then send the packet
                            if (checksum == strtol(local_packetchecksum.c_str(), NULL, 16L))
                            {
                                debug_send(DATATYPE_RDBPACKET, (char*)local_packetdata.c_str(), local_packetdata.size()+1);
                            }
                            else
                            {
                                #if VERBOSE
                                    log_simple("GDB Packet checksum failed. Expected %x, got %x\n", checksum, strtol(local_packetchecksum.c_str(), NULL, 16L));
                                #endif
                                socket_send(local_socket, (char*)"-", 2);
                            }

                            // Finish
                            local_packetdata = "";
                            local_packetchecksum = "";
                            local_parserstate = STATE_SEARCHING;
                        }
                    }
                    read++;
                    left--;
                }
                break;
        }
    }
}


/*==============================
    gdb_replypacket
    TODO
==============================*/
/*
static void gdb_replypacket(std::string packet)
{
    else if (packet == "g")
    {
        // TODO: This properly
        //std::string registers = "";
        //for (int i=0; i<32; i++)
        //    registers += "00000000";
        //registers += "00000001"; // sr
        //registers += "00000002"; // Lo
        //registers += "00000003"; // Hi
        //registers += "00000004"; // Bad
        //registers += "00000005"; // Cause
        //registers += "00000400"; // PC
        //registers += "00000007"; // fsr
        //registers += "00000008"; // fir
        //reply += packet_appendchecksum(registers);
    }
    else if (packet == "?")
    {
        reply += packet_appendchecksum("S05");
    }
    else
        reply += "#00";
}
*/


/*==============================
    gdb_reply
    TODO
==============================*/

void gdb_reply(char* reply)
{
    if (!gdb_isconnected())
        return;

    char chk[3];
    std::string str = "+$";

    // Build the reply packet
    sprintf(chk, "%02x", packet_getchecksum(reply));
    str += reply;
    str += "#";
    str += chk;
    local_lastreply = str;

    // Send the packet to GDB and pop the message from our queue
    #if VERBOSE
        log_simple("Sending to GDB: %s\n", str.c_str());
    #endif
    socket_send(local_socket, (char*)str.c_str(), str.size()+1);
}


/*==============================
    gdb_thread
    TODO
==============================*/

void gdb_thread(char* addr)
{
    // Connect to GDB
    while (!gdb_isconnected())
    {
        gdb_connect(addr);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Communicate with GDB
    while (gdb_isconnected())
    {
        int readsize;
        char buff[512];
        memset(buff, 0, 512*sizeof(char));

        // Read packets from GDB
        readsize = socket_receive(local_socket, buff, 512);
        if (readsize > 0)
        {
            #if VERBOSE
                log_simple("Received from GDB: %s\n", buff);
            #endif
            gdb_parsepacket(buff, readsize);
        }

        // Sleep for a bit
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}