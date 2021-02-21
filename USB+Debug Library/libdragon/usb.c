/***************************************************************
                            usb.c
                               
Allows USB communication between an N64 flashcart and the PC
using UNFLoader.
https://github.com/buu342/N64-UNFLoader
***************************************************************/

#include <string.h>
#include "usb.h"


/*********************************
           Data macros
*********************************/

// Input/Output buffer size. Always keep it at 512
#define BUFFER_SIZE 512

// USB Memory location
#define DEBUG_ADDRESS  0x04000000-DEBUG_ADDRESS_SIZE // Put the debug area at the 63MB area in ROM space

// Data header related
#define USBHEADER_CREATE(type, left) (((type<<24) | (left & 0x00FFFFFF)))

// Other libultra stuff
#ifndef ALIGN
    #define	ALIGN(s, align)	(((u32)(s) + ((align)-1)) & ~((align)-1))
#endif
#ifndef MIN
    #define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif


/*********************************
     Parallel Interface macros
*********************************/

#define N64_PI_ADDRESS    0xA4600000

#define N64_PI_RAMADDRESS  0x00
#define N64_PI_PIADDRESS   0x04
#define N64_PI_READLENGTH  0x08
#define N64_PI_WRITELENGTH 0x0C
#define N64_PI_STATUS      0x10


/*********************************
          64Drive macros
*********************************/

// Cartridge Interface definitions. Obtained from 64Drive's Spec Sheet
#define D64_BASE_ADDRESS   0xB0000000
#define D64_CIREG_ADDRESS  0x08000000
#define D64_CIBASE_ADDRESS 0xB8000000

#define D64_REGISTER_STATUS  0x00000200
#define D64_REGISTER_COMMAND 0x00000208
#define D64_REGISTER_LBA     0x00000210
#define D64_REGISTER_LENGTH  0x00000218
#define D64_REGISTER_RESULT  0x00000220

#define D64_REGISTER_MAGIC    0x000002EC
#define D64_REGISTER_VARIANT  0x000002F0
#define D64_REGISTER_BUTTON   0x000002F8
#define D64_REGISTER_REVISION 0x000002FC

#define D64_REGISTER_USBCOMSTAT 0x00000400
#define D64_REGISTER_USBP0R0    0x00000404
#define D64_REGISTER_USBP1R1    0x00000408

#define D64_ENABLE_ROMWR  0xF0
#define D64_DISABLE_ROMWR 0xF1
#define D64_COMMAND_WRITE 0x08

// Cartridge Interface return values
#define D64_MAGIC    0x55444556

#define D64_USB_IDLE        0x00
#define D64_USB_IDLEUNARMED 0x00
#define D64_USB_ARMED       0x01
#define D64_USB_DATA        0x02
#define D64_USB_ARM         0x0A
#define D64_USB_BUSY        0x0F
#define D64_USB_DISARM      0x0F
#define D64_USB_ARMING      0x0F

#define D64_CI_IDLE  0x00
#define D64_CI_BUSY  0x10
#define D64_CI_WRITE 0x20


/*********************************
       EverDrive macros
*********************************/

#define ED_BASE           0x10000000
#define ED_BASE_ADDRESS   0x1F800000
#define ED_GET_REGADD(reg)   (0xA0000000 | ED_BASE_ADDRESS | (reg))

#define ED_REG_USBCFG  0x0004
#define ED_REG_VERSION 0x0014
#define ED_REG_USBDAT  0x0400
#define ED_REG_SYSCFG  0x8000
#define ED_REG_KEY     0x8004

#define ED_USBMODE_RDNOP 0xC400
#define ED_USBMODE_RD    0xC600
#define ED_USBMODE_WRNOP 0xC000
#define ED_USBMODE_WR    0xC200

#define ED_USBSTAT_ACT   0x0200
#define ED_USBSTAT_RXF   0x0400
#define ED_USBSTAT_TXE   0x0800
#define ED_USBSTAT_POWER 0x1000
#define ED_USBSTAT_BUSY  0x2000

#define ED_REGKEY  0xAA55

#define ED3_VERSION 0xED640008
#define ED7_VERSION 0xED640013


/*********************************
       SummerCart64 macros
*********************************/

#define SC64_SDRAM_BASE             0x10000000

#define SC64_BANK_ROM               1

#define SC64_REGS_BASE              0x1E000000
#define SC64_REG_SCR                (SC64_REGS_BASE + 0x00)
#define SC64_REG_VERSION            (SC64_REGS_BASE + 0x08)
#define SC64_REG_USB_SCR            (SC64_REGS_BASE + 0x10)
#define SC64_REG_USB_DMA_ADDR       (SC64_REGS_BASE + 0x14)
#define SC64_REG_USB_DMA_LEN        (SC64_REGS_BASE + 0x18)

#define SC64_MEM_BASE               (SC64_REGS_BASE + 0x1000)
#define SC64_MEM_USB_FIFO_BASE      (SC64_MEM_BASE + 0x0000)
#define SC64_MEM_USB_FIFO_LEN       (4 * 1024)

#define SC64_SCR_SDRAM_WRITE_EN     (1 << 0)

#define SC64_VERSION_A              0x53363461

#define SC64_USB_STATUS_BUSY        (1 << 0)
#define SC64_USB_STATUS_READY       (1 << 1)
#define SC64_USB_CONTROL_START      (1 << 0)
#define SC64_USB_CONTROL_FIFO_FLUSH (1 << 2)

#define SC64_USB_BANK_ADDR(b, a)    ((((b) & 0xF) << 28) | ((a) & 0x3FFFFFF))
#define SC64_USB_LENGTH(l)          (ALIGN((l), 4) / 4)
#define SC64_USB_DMA_MAX_LEN        (2 * 1024 * 1024)
#define SC64_USB_FIFO_ITEMS(s)      (((s) >> 3) & 0x7FF)


/*********************************
        Function Prototypes
*********************************/

static void usb_findcart();
static void usb_64drive_write(int datatype, const void* data, int size);
static u32  usb_64drive_poll();
static void usb_64drive_read();
static void usb_everdrive_readreg(u32 reg, u32* result);
static void usb_everdrive_write(int datatype, const void* data, int size);
static u32  usb_everdrive_poll();
static void usb_everdrive_read();
static void usb_everdrive_writereg(u64 reg, u32 value);
static void usb_sc64_write(int datatype, const void* data, int size);
static u32 usb_sc64_poll();
static void usb_sc64_read();


/*********************************
             Globals
*********************************/

// Function pointers
void (*funcPointer_write)(int datatype, const void* data, int size);
u32  (*funcPointer_poll)();
void (*funcPointer_read)();

// USB globals
static s8 usb_cart = CART_NONE;
static u8 usb_buffer[BUFFER_SIZE*3] __attribute__((aligned(16)));
int usb_datatype = 0;
int usb_datasize = 0;
int usb_dataleft = 0;
int usb_readblock = -1;


/*********************************
           USB functions
*********************************/

/*==============================
    usb_initialize
    Initializes the USB buffers and pointers
    @returns 1 if the USB initialization was successful, 0 if not
==============================*/

char usb_initialize()
{
    // Initialize the debug related globals
    memset(usb_buffer, 0, BUFFER_SIZE);
            
    // Find the flashcart
    usb_findcart();

    // Set the function pointers based on the flashcart
    switch (usb_cart)
    {
        case CART_64DRIVE:
            funcPointer_write = usb_64drive_write;
            funcPointer_poll  = usb_64drive_poll;
            funcPointer_read  = usb_64drive_read;
            break;
        case CART_EVERDRIVE:
            funcPointer_write = usb_everdrive_write;
            funcPointer_poll  = usb_everdrive_poll;
            funcPointer_read  = usb_everdrive_read;
            break;
        case CART_SC64:
            funcPointer_write = usb_sc64_write;
            funcPointer_poll  = usb_sc64_poll;
            funcPointer_read  = usb_sc64_read;
            break;
        default:
            return 0;
    }
    return 1;
}


/*==============================
    usb_findcart
    Checks if the game is running on a 64Drive, EverDrive or a SummerCart64.
==============================*/

static void usb_findcart()
{
    usb_cart = CART_64DRIVE;
    return;
}


/*==============================
    usb_getcart
    Returns which flashcart is currently connected
    @return The CART macro that corresponds to the identified flashcart
==============================*/

char usb_getcart()
{
    return usb_cart;
}


/*==============================
    usb_write
    Writes data to the USB.
    Will not write if there is data to read from USB
    @param The DATATYPE that is being sent
    @param A buffer with the data to send
    @param The size of the data being sent
==============================*/

void usb_write(int datatype, const void* data, int size)
{
    // If no debug cart exists, stop
    if (usb_cart == CART_NONE)
        return;
        
    // If there's data to read first, stop
    if (usb_dataleft != 0)
        return;
        
    // Call the correct write function
    funcPointer_write(datatype, data, size);
}


/*==============================
    usb_poll
    Returns the header of data being received via USB
    The first byte contains the data type, the next 3 the number of bytes left to read
    @return The data header, or 0
==============================*/

u32 usb_poll()
{
    // If no debug cart exists, stop
    if (usb_cart == CART_NONE)
        return 0;
        
    // If we're out of USB data to read, we don't need the header info anymore
    if (usb_dataleft <= 0)
    {
        usb_dataleft = 0;
        usb_datatype = 0;
        usb_datasize = 0;
        usb_readblock = -1;
    }
        
    // If there's still data that needs to be read, return the header with the data left
    if (usb_dataleft != 0)
        return USBHEADER_CREATE(usb_datatype, usb_dataleft);
        
    // Call the correct read function
    return funcPointer_poll();
}


/*==============================
    usb_read
    Reads bytes from USB into the provided buffer
    @param The buffer to put the read data in
    @param The number of bytes to read
==============================*/

void usb_read(void* buffer, int nbytes)
{
    int read = 0;
    int left = nbytes;
    int offset = usb_datasize-usb_dataleft;
    int copystart = offset%BUFFER_SIZE;
    int block = BUFFER_SIZE-copystart;
    int blockoffset = (offset/BUFFER_SIZE)*BUFFER_SIZE;
    
    // If no debug cart exists, stop
    if (usb_cart == CART_NONE)
        return;
        
    // If there's no data to read, stop
    if (usb_dataleft == 0)
        return;

    // Read chunks from ROM
    while (left > 0)
    {
        // Ensure we don't read too much data
        if (left > usb_dataleft)
            left = usb_dataleft;
        if (block > left)
            block = left;
            
        // Call the read function if we're reading a new block
        if (usb_readblock != blockoffset)
        {
            usb_readblock = blockoffset;
            funcPointer_read();
        }
        
        // Copy from the USB buffer to the supplied buffer
        memcpy(buffer+read, usb_buffer+copystart, block);
        
        // Increment/decrement all our counters
        read += block;
        left -= block;
        usb_dataleft -= block;
        blockoffset += BUFFER_SIZE;
        block = BUFFER_SIZE;
        copystart = 0;
    }
}


/*==============================
    usb_skip
    Skips a USB read by the specified amount of bytes
    @param The number of bytes to skip
==============================*/

void usb_skip(int nbytes)
{
    // Subtract the amount of bytes to skip to the data pointers
    usb_dataleft -= nbytes;
    if (usb_dataleft < 0)
        usb_dataleft = 0;
}


/*==============================
    usb_rewind
    Rewinds a USB read by the specified amount of bytes
    @param The number of bytes to rewind
==============================*/

void usb_rewind(int nbytes)
{
    // Add the amount of bytes to rewind to the data pointers
    usb_dataleft += nbytes;
    if (usb_dataleft > usb_datasize)
        usb_dataleft = usb_datasize;
}


/*==============================
    usb_purge
    Purges the incoming USB data
==============================*/

void usb_purge()
{
    usb_dataleft = 0;
    usb_datatype = 0;
    usb_datasize = 0;
    usb_readblock = -1;
}


/*********************************
        64Drive functions
*********************************/

/*==============================
    usb_64drive_wait
    Wait until the 64Drive is ready
    @return 0 if success or -1 if failure
==============================*/

static s8 usb_64drive_wait()
{
    return 0;
}


/*==============================
    usb_64drive_setwritable
    Set the write mode on the 64Drive
    @param A boolean with whether to enable or disable
==============================*/

static void usb_64drive_setwritable(u8 enable)
{
    
}


/*==============================
    usb_64drive_waitidle
    Waits for the 64Drive's USB to be idle
==============================*/

static void usb_64drive_waitidle()
{

}


/*==============================
    usb_64drive_armstatus
    Checks if the 64Drive is armed
    @return The arming status
==============================*/

static u32 usb_64drive_armstatus()
{
    return 0;
}


/*==============================
    usb_64drive_waitdisarmed
    Waits for the 64Drive's USB to be disarmed
==============================*/

static void usb_64drive_waitdisarmed()
{

}


/*==============================
    usb_64drive_write
    Sends data through USB from the 64Drive
    Will not write if there is data to read from USB
    @param The DATATYPE that is being sent
    @param A buffer with the data to send
    @param The size of the data being sent
==============================*/

static void usb_64drive_write(int datatype, const void* data, int size)
{

}


/*==============================
    usb_64drive_arm
    Arms the 64Drive's USB
    @param The ROM offset to arm
    @param The size of the data to transfer
==============================*/

static void usb_64drive_arm(u32 offset, u32 size)
{

}


/*==============================
    usb_64drive_disarm
    Disarms the 64Drive's USB
==============================*/

static void usb_64drive_disarm()
{

}


/*==============================
    usb_64drive_poll
    Returns the header of data being received via USB on the 64Drive
    The first byte contains the data type, the next 3 the number of bytes left to read
    @return The data header, or 0
==============================*/

static u32 usb_64drive_poll()
{
    return 0;
}


/*==============================
    usb_64drive_read
    Reads bytes from the 64Drive ROM into the global buffer with the block offset
==============================*/

static void usb_64drive_read()
{

}


/*********************************
       EverDrive functions
*********************************/

/*==============================
    usb_everdrive_wait_pidma
    Spins until the EverDrive's DMA is ready
==============================*/

static void usb_everdrive_wait_pidma() 
{

}


/*==============================
    usb_everdrive_readdata
    Reads data from a specific address on the EverDrive
    @param The buffer with the data
    @param The register address to write to the PI
    @param The size of the data
==============================*/

static void usb_everdrive_readdata(void* buff, u32 pi_address, u32 len) 
{

}


/*==============================
    usb_everdrive_readreg
    Reads data from a specific register on the EverDrive
    @param The register to read from
    @param A pointer to write the read value to
==============================*/

static void usb_everdrive_readreg(u32 reg, u32* result) 
{
    usb_everdrive_readdata(result, ED_GET_REGADD(reg), sizeof(u32));
}


/*==============================
    usb_everdrive_writedata
    Writes data to a specific address on the EverDrive
    @param A buffer with the data to write
    @param The register address to write to the PI
    @param The length of the data
==============================*/

static void usb_everdrive_writedata(void* buff, u32 pi_address, u32 len) 
{
 
}


/*==============================
    usb_everdrive_writereg
    Writes data to a specific register on the EverDrive
    @param The register to write to
    @param The value to write to the register
==============================*/

static void usb_everdrive_writereg(u64 reg, u32 value) 
{
    usb_everdrive_writedata(&value, ED_GET_REGADD(reg), sizeof(u32));
}


/*==============================
    usb_everdrive_usbbusy
    Spins until the USB is no longer busy
==============================*/

static void usb_everdrive_usbbusy() 
{
    u32 val __attribute__((aligned(8)));
    do 
    {
        usb_everdrive_readreg(ED_REG_USBCFG, &val);
    } 
    while ((val & ED_USBSTAT_ACT) != 0);
}


/*==============================
    usb_everdrive_canread
    Checks if the EverDrive's USB can read
    @return 1 if it can read, 0 if not
==============================*/

static u8 usb_everdrive_canread() 
{
    u32 val __attribute__((aligned(8)));
    u32 status = ED_USBSTAT_POWER;
    
    // Read the USB register and check its status
    usb_everdrive_readreg(ED_REG_USBCFG, &val);
    status = val & (ED_USBSTAT_POWER | ED_USBSTAT_RXF);
    return status == ED_USBSTAT_POWER;
}


/*==============================
    usb_everdrive_readusb
    Reads from the EverDrive USB buffer
    @param The buffer to put the read data in
    @param The number of bytes to read
==============================*/

static void usb_everdrive_readusb(void* buffer, int size)
{
    u16 block, addr;
    
    while (size) 
    {
        // Get the block size
        block = BUFFER_SIZE;
        if (block > size)
            block = size;
        addr = BUFFER_SIZE - block;
        
        // Request to read from the USB
        usb_everdrive_writereg(ED_REG_USBCFG, ED_USBMODE_RD | addr); 

        // Wait for the FPGA to transfer the data to its internal buffer
        usb_everdrive_usbbusy(); 

        // Read from the internal buffer and store it in our buffer
        usb_everdrive_readdata(buffer, ED_GET_REGADD(ED_REG_USBDAT + addr), block); 
        buffer = (char*)buffer + block;
        size -= block;
    }
}


/*==============================
    usb_everdrive_write
    Sends data through USB from the EverDrive
    Will not write if there is data to read from USB
    @param The DATATYPE that is being sent
    @param A buffer with the data to send
    @param The size of the data being sent
==============================*/

static void usb_everdrive_write(int datatype, const void* data, int size)
{
    char wrotecmp = 0;
    char cmp[] = {'C', 'M', 'P', 'H'};
    int read = 0;
    int left = size;
    int offset = 8;
    u32 header = (size & 0x00FFFFFF) | (datatype << 24);
    
    // Put in the DMA header along with length and type information in the global buffer
    usb_buffer[0] = 'D';
    usb_buffer[1] = 'M';
    usb_buffer[2] = 'A';
    usb_buffer[3] = '@';
    usb_buffer[4] = (header >> 24) & 0xFF;
    usb_buffer[5] = (header >> 16) & 0xFF;
    usb_buffer[6] = (header >> 8)  & 0xFF;
    usb_buffer[7] = header & 0xFF;
    
    // Write data to USB until we've finished
    while (left > 0)
    {
        int block = left;
        int blocksend, baddr;
        if (block+offset > BUFFER_SIZE)
            block = BUFFER_SIZE-offset;
            
        // Copy the data to the next available spots in the global buffer
        memcpy(usb_buffer+offset, (void*)((char*)data+read), block);
        
        // Restart the loop to write the CMP signal if we've finished
        if (!wrotecmp && read+block >= size)
        {
            left = 4;
            offset = block+offset;
            data = cmp;
            wrotecmp = 1;
            read = 0;
            continue;
        }
        
        // Ensure the data is 16 byte aligned and the block address is correct
        blocksend = (block+offset)+15 - ((block+offset)+15)%16;
        baddr = BUFFER_SIZE - blocksend;

        // Set USB to write mode and send data through USB
        usb_everdrive_writereg(ED_REG_USBCFG, ED_USBMODE_WRNOP);
        usb_everdrive_writedata(usb_buffer, ED_GET_REGADD(ED_REG_USBDAT + baddr), blocksend);
        
        // Set USB to write mode with the new address and wait for USB to end
        usb_everdrive_writereg(ED_REG_USBCFG, ED_USBMODE_WR | baddr);
        usb_everdrive_usbbusy();
        
        // Keep track of what we've read so far
        left -= block;
        read += block;
        offset = 0;
    }
}


/*==============================
    usb_everdrive_poll
    Returns the header of data being received via USB on the EverDrive
    The first byte contains the data type, the next 3 the number of bytes left to read
    @return The data header, or 0
==============================*/

static u32 usb_everdrive_poll()
{
    char buff[16] __attribute__((aligned(8)));
    int len;
    int offset = 0;
    
    // Wait for the USB to be ready
    usb_everdrive_usbbusy();
    
    // Check if the USB is ready to be read
    if (!usb_everdrive_canread())
        return 0;
    
    // Read the first 8 bytes that are being received and check if they're valid
    usb_everdrive_readusb(buff, 16);
    if (buff[0] != 'D' || buff[1] != 'M' || buff[2] != 'A' || buff[3] != '@')
        return 0;
        
    // Store information about the incoming data
    usb_datatype = (int)buff[4];
    usb_datasize = (int)buff[5]<<16 | (int)buff[6]<<8 | (int)buff[7]<<0;
    usb_dataleft = usb_datasize;
    usb_readblock = -1;
    
    // Begin receiving data
    usb_everdrive_writereg(ED_REG_USBCFG, ED_USBMODE_RD | BUFFER_SIZE);
    len = (usb_datasize + BUFFER_SIZE-usb_datasize%BUFFER_SIZE)/BUFFER_SIZE;
    
    // While there's data to service
    while (len--) 
    {
        // Wait for the USB to be ready and then read data
        usb_everdrive_usbbusy();
        usb_everdrive_readdata(usb_buffer, ED_GET_REGADD(ED_REG_USBDAT), BUFFER_SIZE); // TODO: Replace with usb_everdrive_readusb?
        
        // Tell the FPGA we can receive more data
        if (len != 0)
            usb_everdrive_writereg(ED_REG_USBCFG, ED_USBMODE_RD | BUFFER_SIZE);
        
        // Copy received block to ROM
        usb_everdrive_writedata(usb_buffer, ED_BASE + DEBUG_ADDRESS + offset, BUFFER_SIZE);
        offset += BUFFER_SIZE;
    }
    
    // Read the CMP Signal
    usb_everdrive_usbbusy();
    usb_everdrive_readusb(buff, 16);
    if (buff[0] != 'C' || buff[1] != 'M' || buff[2] != 'P' || buff[3] != 'H')
    {
        // Something went wrong with the data
        usb_datatype = 0;
        usb_datasize = 0;
        usb_dataleft = 0;
        usb_readblock = -1;
        return 0;
    }

    // Return the data header
    return USBHEADER_CREATE(usb_datatype, usb_datasize);
}


/*==============================
    usb_everdrive_read
    Reads bytes from the EverDrive ROM into the global buffer with the block offset
==============================*/

static void usb_everdrive_read()
{

}


/*********************************
       SummerCart64 functions
*********************************/


/*==============================
    usb_sc64_read_usb_scr
    Reads SummerCart64 REG_USB_SCR register
    @return value of REG_USB_SCR register
==============================*/

static u32 usb_sc64_read_usb_scr(void)
{
    return 0;
}


/*==============================
    usb_sc64_read_usb_fifo
    Loads one element from USB FIFO
    @return value popped from USB FIFO
==============================*/

static u32 usb_sc64_read_usb_fifo(void)
{
    u32 data __attribute__((aligned(8)));

    return data;
}


/*==============================
    usb_sc64_waitidle
    Waits for the SummerCart64 USB interface to be idle
    @return 0 if interface is ready, -1 if USB cable is not connected
==============================*/

static s8 usb_sc64_waitidle(void)
{
    return 0;
}


/*==============================
    usb_sc64_waitdata
    Waits for the SummerCart64 USB FIFO to contain specified amount of data or for full FIFO
    @param length in bytes
    @return number of available bytes in FIFO, -1 if USB cable is not connected
==============================*/

static s32 usb_sc64_waitdata(u32 length)
{
    u32 usb_scr __attribute__((aligned(8)));
    u32 wait_length = ALIGN(MIN(length, SC64_MEM_USB_FIFO_LEN), 4);
    u32 bytes = 0;

    do
    {
        usb_scr = usb_sc64_read_usb_scr();
        if (!(usb_scr & SC64_USB_STATUS_READY)) {
            // Reset usb_cart type if USB cable is not connected
            usb_cart = CART_NONE;
            return -1;
        }
        bytes = SC64_USB_FIFO_ITEMS(usb_scr) * 4;
    } while (bytes < wait_length);

    return (s32) bytes;
}


/*==============================
    usb_sc64_setwritable
    Enable ROM (SDRAM) writes in SummerCart64
    @param A boolean with whether to enable or disable
==============================*/

static void usb_sc64_setwritable(u8 enable)
{

}


/*==============================
    usb_sc64_write
    Sends data through USB from the SummerCart64
    @param The DATATYPE that is being sent
    @param A buffer with the data to send
    @param The size of the data being sent
==============================*/

static void usb_sc64_write(int datatype, const void* data, int size)
{

}


/*==============================
    usb_sc64_poll
    Returns the header of data being received via USB on the SummerCart64
    The first byte contains the data type, the next 3 the number of bytes left to read
    @return The data header, or 0
==============================*/

static u32 usb_sc64_poll(void)
{

    // Return no USB header if FIFO is empty
    return 0;
}


/*==============================
    usb_sc64_read
    Reads bytes from the SummerCart64 ROM into the global buffer with the block offset
==============================*/

static void usb_sc64_read(void)
{

}
