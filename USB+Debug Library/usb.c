/***************************************************************
                            usb.c
                               
Allows USB communication between an N64 flashcart and the PC
using UNFLoader.
https://github.com/buu342/N64-UNFLoader
***************************************************************/

#include "usb.h"
#ifndef LIBDRAGON
    #include <ultra64.h>
#else
    #include <libdragon.h>
#endif
#include <string.h>


/*********************************
           Data macros
*********************************/

// Input/Output buffer size. Always keep it at 512
#define BUFFER_SIZE 512

// USB Memory location
#define DEBUG_ADDRESS  0x04000000-DEBUG_ADDRESS_SIZE // Put the debug area at the 63MB area in ROM space

// Data header related
#define USBHEADER_CREATE(type, left) (((type<<24) | (left & 0x00FFFFFF)))

// Size alignment helper
#define	ALIGN(value, align) (((value) + ((typeof(value))(align) - 1)) & ~((typeof(value))(align) - 1))


/*********************************
   Libultra macros for libdragon
*********************************/

#ifdef LIBDRAGON
    // Useful
    #define MIN(a, b) ((a) < (b) ? (a) : (b))
    #ifndef TRUE
        #define TRUE 1
    #endif
    #ifndef FALSE
        #define FALSE 0
    #endif
    #ifndef NULL
        #define NULL 0
    #endif
    
    // MIPS addresses
    #define KSEG0 0x80000000
    #define KSEG1 0xA0000000
    
    // Memory translation stuff
    #define	PHYS_TO_K1(x)       ((u32)(x)|KSEG1)
    #define	IO_WRITE(addr,data) (*(vu32 *)PHYS_TO_K1(addr)=(u32)(data))
    #define	IO_READ(addr)       (*(vu32 *)PHYS_TO_K1(addr))
    
    // PI registers
    #define PI_BASE_REG   0x04600000
    #define PI_STATUS_REG (PI_BASE_REG+0x10)
    #define	PI_STATUS_ERROR		0x04
    #define	PI_STATUS_IO_BUSY	0x02
    #define	PI_STATUS_DMA_BUSY	0x01
    
    #define PI_BSD_DOM1_LAT_REG	(PI_BASE_REG+0x14)
    #define PI_BSD_DOM1_PWD_REG	(PI_BASE_REG+0x18)
    #define PI_BSD_DOM1_PGS_REG	(PI_BASE_REG+0x1C)
    #define PI_BSD_DOM1_RLS_REG	(PI_BASE_REG+0x20)
    #define PI_BSD_DOM2_LAT_REG	(PI_BASE_REG+0x24)
    #define PI_BSD_DOM2_PWD_REG	(PI_BASE_REG+0x28)
    #define PI_BSD_DOM2_PGS_REG	(PI_BASE_REG+0x2C)
    #define PI_BSD_DOM2_RLS_REG	(PI_BASE_REG+0x30)
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

// How many cycles for the 64Drive to wait for data. 
// Lowering this might improve performance slightly faster at the expense of USB reading accuracy
#define D64_DEFAULTPOLLTIME 2000

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

#define ED25_VERSION 0xED640007
#define ED3_VERSION  0xED640008
#define ED7_VERSION  0xED640013


/*********************************
            SC64 macros
*********************************/

#define SC64_SDRAM_BASE                 0x10000000
#define SC64_REGS_BASE                  0x1FFF0000

#define SC64_REG_SR_CMD                 (SC64_REGS_BASE + 0x00)
#define SC64_REG_DATA_0                 (SC64_REGS_BASE + 0x04)
#define SC64_REG_DATA_1                 (SC64_REGS_BASE + 0x08)
#define SC64_REG_IDENTIFIER             (SC64_REGS_BASE + 0x0C)
#define SC64_REG_KEY                    (SC64_REGS_BASE + 0x10)

#define SC64_SR_CMD_ERROR               (1 << 30)
#define SC64_SR_CMD_BUSY                (1 << 31)

#define SC64_V2_IDENTIFIER              0x53437632

#define SC64_KEY_RESET                  0x00000000
#define SC64_KEY_UNLOCK_1               0x5F554E4C
#define SC64_KEY_UNLOCK_2               0x4F434B5F

#define SC64_CMD_CONFIG_SET             'C'
#define SC64_CMD_USB_WRITE_STATUS       'U'
#define SC64_CMD_USB_WRITE              'M'
#define SC64_CMD_USB_READ_STATUS        'u'
#define SC64_CMD_USB_READ               'm'

#define SC64_CFG_ID_ROM_WRITE_ENABLE    1

#define SC64_USB_WRITE_STATUS_BUSY      (1 << 31)
#define SC64_USB_READ_STATUS_BUSY       (1 << 31)


/*********************************
  Libultra types (for libdragon)
*********************************/

#ifdef LIBDRAGON
    typedef uint8_t  u8;	
    typedef uint16_t u16;
    typedef uint32_t u32;
    typedef uint64_t u64;
    
    typedef int8_t  s8;	
    typedef int16_t s16;
    typedef int32_t s32;
    typedef int64_t s64;
    
    typedef volatile uint8_t  vu8;
    typedef volatile uint16_t vu16;
    typedef volatile uint32_t vu32;
    typedef volatile uint64_t vu64;
    
    typedef volatile int8_t  vs8;
    typedef volatile int16_t vs16;
    typedef volatile int32_t vs32;
    typedef volatile int64_t vs64;
    
    typedef float  f32;
    typedef double f64;
#endif


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
static u32  usb_sc64_poll();
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
static u8 __attribute__((aligned(16))) usb_buffer[BUFFER_SIZE];
int usb_datatype = 0;
int usb_datasize = 0;
int usb_dataleft = 0;
int usb_readblock = -1;

#ifndef LIBDRAGON
// Message globals
    #if !USE_OSRAW
        OSMesg      dmaMessageBuf;
        OSIoMesg    dmaIOMessageBuf;
        OSMesgQueue dmaMessageQ;
    #endif
    
    // osPiRaw
    #if USE_OSRAW
        extern s32 __osPiRawWriteIo(u32, u32);
        extern s32 __osPiRawReadIo(u32, u32 *);
        extern s32 __osPiRawStartDma(s32, u32, void *, u32);
        
        #define osPiRawWriteIo(a, b) __osPiRawWriteIo(a, b)
        #define osPiRawReadIo(a, b) __osPiRawReadIo(a, b)
        #define osPiRawStartDma(a, b, c, d) __osPiRawStartDma(a, b, c, d)
    #endif
#endif

// Flashcart specific stuff
static u32 d64_polltime = D64_DEFAULTPOLLTIME;


/*********************************
          USB functions
*********************************/

/*==============================
    usb_io/dma_read/write
    PI I/O wrapper functions for libdragon and libultra API
==============================*/

#ifdef LIBDRAGON
    static inline u32 usb_io_read(u32 pi_address)
    {
        return io_read(pi_address);
    }

    static inline void usb_io_write(u32 pi_address, u32 value)
    {
        io_write(pi_address, value);
    }

    static inline void usb_dma_read(void *ram_address, u32 pi_address, size_t size)
    {
        data_cache_hit_writeback_invalidate(ram_address, size);
        dma_read(ram_address, pi_address, size);
    }

    static inline void usb_dma_write(void *ram_address, u32 pi_address, size_t size)
    {
        data_cache_hit_writeback(ram_address, size);
        dma_write(ram_address, pi_address, size);
    }
#else
    static inline u32 usb_io_read(u32 pi_address)
    {
        u32 value;
        #if USE_OSRAW
            osPiRawReadIo(pi_address, &value);
        #else
            osPiReadIo(pi_address, &value);
        #endif
        return value;
    }

    static inline void usb_io_write(u32 pi_address, u32 value)
    {
        #if USE_OSRAW
            osPiRawWriteIo(pi_address, value);
        #else
            osPiWriteIo(pi_address, value);
        #endif
    }

    static inline void usb_dma_read(void *ram_address, u32 pi_address, size_t size)
    {
        osWritebackDCache(ram_address, size);
        osInvalDCache(ram_address, size);
        #if USE_OSRAW
            osPiRawStartDma(OS_READ, pi_address, ram_address, size);
        #else
            osPiStartDma(&dmaIOMessageBuf, OS_MESG_PRI_NORMAL, OS_READ, pi_address, ram_address, size, &dmaMessageQ);
            osRecvMesg(&dmaMessageQ, NULL, OS_MESG_BLOCK);
        #endif
    }

    static inline void usb_dma_write(void *ram_address, u32 pi_address, size_t size)
    {
        osWritebackDCache(ram_address, size);
        #if USE_OSRAW
            osPiRawStartDma(OS_WRITE, pi_address, ram_address, size);
        #else
            osPiStartDma(&dmaIOMessageBuf, OS_MESG_PRI_NORMAL, OS_WRITE, pi_address, ram_address, size, &dmaMessageQ);
            osRecvMesg(&dmaMessageQ, NULL, OS_MESG_BLOCK);
        #endif
    }
#endif


/*==============================
    usb_initialize
    Initializes the USB buffers and pointers
    @returns 1 if the USB initialization was successful, 0 if not
==============================*/

char usb_initialize()
{
    // Initialize the debug related globals
    memset(usb_buffer, 0, BUFFER_SIZE);
        
    #ifndef LIBDRAGON
        // Create the message queue
        #if !USE_OSRAW
            osCreateMesgQueue(&dmaMessageQ, &dmaMessageBuf, 1);
        #endif
    #endif
    
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
    Checks if the game is running on a 64Drive, EverDrive or a SC64.
==============================*/

static void usb_findcart()
{
    u32 buff __attribute__((aligned(8)));
    
    // Read the cartridge and check if we have a 64Drive.
    #ifdef LIBDRAGON
        buff = io_read(D64_CIBASE_ADDRESS + D64_REGISTER_MAGIC);
    #else
        #if USE_OSRAW
            osPiRawReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_MAGIC, &buff);
        #else
            osPiReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_MAGIC, &buff);
        #endif
    #endif
    if (buff == D64_MAGIC)
    {
        usb_cart = CART_64DRIVE;
        return;
    }
    
    // Since we didn't find a 64Drive let's assume we have an EverDrive
    // Write the key to unlock the registers, then read the version register
    usb_everdrive_writereg(ED_REG_KEY, ED_REGKEY);
    usb_everdrive_readreg(ED_REG_VERSION, &buff);
    
    // EverDrive 2.5 not compatible
    if (buff == ED25_VERSION)
        return;
    
    // Check if we have an EverDrive
    if (buff == ED7_VERSION || buff == ED3_VERSION)
    {
        // Set the USB mode
        usb_everdrive_writereg(ED_REG_SYSCFG, 0);
        usb_everdrive_writereg(ED_REG_USBCFG, ED_USBMODE_RDNOP);
        
        // Set the cart to EverDrive
        usb_cart = CART_EVERDRIVE;
        return;
    }

    // Since we didn't find an EverDrive either let's assume we have a SC64
    // Write the key sequence to unlock the registers, then read the identifier register
    usb_io_write(SC64_REG_KEY, SC64_KEY_RESET);
    usb_io_write(SC64_REG_KEY, SC64_KEY_UNLOCK_1);
    usb_io_write(SC64_REG_KEY, SC64_KEY_UNLOCK_2);

    // Check if we have a SC64
    if (usb_io_read(SC64_REG_IDENTIFIER) == SC64_V2_IDENTIFIER)
    {
        // Set the cart to SC64
        usb_cart = CART_SC64;
        return;
    }
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
    u32 ret __attribute__((aligned(8)));
    u32 timeout = 0; // I wanted to use osGetTime() but that requires the VI manager
    
    // Wait until the cartridge interface is ready
    do
    {
        #ifdef LIBDRAGON
            ret = io_read(D64_CIBASE_ADDRESS + D64_REGISTER_STATUS);
        #else
            #if USE_OSRAW
                osPiRawReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_STATUS, &ret);
            #else
                osPiReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_STATUS, &ret);
            #endif
        #endif
        
        // Took too long, abort
        if((timeout++) > 10000)
            return -1;
    }
    while((ret >> 8) & D64_CI_BUSY);
    (void) timeout; // Needed to stop unused variable warning
    
    // Success
    return 0;
}


/*==============================
    usb_64drive_setwritable
    Set the write mode on the 64Drive
    @param A boolean with whether to enable or disable
==============================*/

static void usb_64drive_setwritable(u8 enable)
{
    usb_64drive_wait();
    #ifdef LIBDRAGON
        io_write(D64_CIBASE_ADDRESS + D64_REGISTER_COMMAND, enable ? D64_ENABLE_ROMWR : D64_DISABLE_ROMWR);
    #else
        #if USE_OSRAW
            osPiRawWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_COMMAND, enable ? D64_ENABLE_ROMWR : D64_DISABLE_ROMWR);
        #else
            osPiWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_COMMAND, enable ? D64_ENABLE_ROMWR : D64_DISABLE_ROMWR);
        #endif
    #endif
    usb_64drive_wait();
}


/*==============================
    usb_64drive_waitidle
    Waits for the 64Drive's USB to be idle
==============================*/

static int usb_64drive_waitidle()
{
    u32 status __attribute__((aligned(8)));
    u32 timeout = 0;
    do 
    {
        #ifdef LIBDRAGON
            status = io_read(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT);
        #else
            #if USE_OSRAW
                osPiRawReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, &status);
            #else
                osPiReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, &status);
            #endif
        #endif
        status = (status >> 4) & D64_USB_BUSY;
        if (timeout++ > 128)
            return 0;
    }
    while(status != D64_USB_IDLE);
    return 1;
}


/*==============================
    usb_64drive_armstatus
    Checks if the 64Drive is armed
    @return The arming status
==============================*/

static u32 usb_64drive_armstatus()
{
    u32 status __attribute__((aligned(8)));
    #ifdef LIBDRAGON
        status = io_read(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT);
    #else
        #if USE_OSRAW
            osPiRawReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, &status);
        #else
            osPiReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, &status);
        #endif
    #endif
    return status & 0xf;
}


/*==============================
    usb_64drive_waitdisarmed
    Waits for the 64Drive's USB to be disarmed
==============================*/

static void usb_64drive_waitdisarmed()
{
    u32 status __attribute__((aligned(8)));
    do
    {
        #ifdef LIBDRAGON
            status = io_read(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT);
        #else
            #if USE_OSRAW
                osPiRawReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, &status);
            #else
                osPiReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, &status);
            #endif
        #endif
        status &= 0x0F;
    }
    while (status != D64_USB_IDLEUNARMED);
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
    int left = size;
    int read = 0;
    
    // Spin until the write buffer is free and then set the cartridge to write mode
    if (!usb_64drive_waitidle())
        return;
    usb_64drive_setwritable(TRUE);
    
    // Write data to SDRAM until we've finished
    while (left > 0)
    {
        int block = left;
        if (block > BUFFER_SIZE)
            block = BUFFER_SIZE;
        
        // Copy the data to the global buffer
        memcpy(usb_buffer, (void*)((char*)data+read), block);
        
        // If the data was not 32-bit aligned, pad the buffer
        if (block < BUFFER_SIZE && size%4 != 0)
        {
            u32 i;
            u32 size_new = (size & ~3)+4;
            block += size_new-size;
            for (i=size; i<size_new; i++) 
                usb_buffer[i] = 0;
            size = size_new;
        }
        
        // Spin until the write buffer is free
        if (!usb_64drive_waitidle())
        {
            usb_64drive_setwritable(FALSE);
            return;
        }
        
        // Set up DMA transfer between RDRAM and the PI
        #ifdef LIBDRAGON
            data_cache_hit_writeback(usb_buffer, block);
            dma_write(usb_buffer, D64_BASE_ADDRESS + DEBUG_ADDRESS + read, block);
        #else
            osWritebackDCache(usb_buffer, block);
            #if USE_OSRAW
                osPiRawStartDma(OS_WRITE, 
                             D64_BASE_ADDRESS + DEBUG_ADDRESS + read, 
                             usb_buffer, block);
            #else
                osPiStartDma(&dmaIOMessageBuf, OS_MESG_PRI_NORMAL, OS_WRITE, 
                             D64_BASE_ADDRESS + DEBUG_ADDRESS + read, 
                             usb_buffer, block, &dmaMessageQ);
                (void)osRecvMesg(&dmaMessageQ, NULL, OS_MESG_BLOCK);
            #endif
        #endif
        // Keep track of what we've read so far
        left -= block;
        read += block;
    }
    
    // Send the data through USB
    #ifdef LIBDRAGON
        io_write(D64_CIBASE_ADDRESS + D64_REGISTER_USBP0R0, (DEBUG_ADDRESS) >> 1);
        io_write(D64_CIBASE_ADDRESS + D64_REGISTER_USBP1R1, (size & 0xFFFFFF) | (datatype << 24));
        io_write(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, D64_COMMAND_WRITE);
    #else
        #if USE_OSRAW
            osPiRawWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBP0R0, (DEBUG_ADDRESS) >> 1);
            osPiRawWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBP1R1, (size & 0xFFFFFF) | (datatype << 24));
            osPiRawWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, D64_COMMAND_WRITE);
        #else
            osPiWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBP0R0, (DEBUG_ADDRESS) >> 1);
            osPiWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBP1R1, (size & 0xFFFFFF) | (datatype << 24));
            osPiWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, D64_COMMAND_WRITE);
        #endif
    #endif
        
    // Spin until the write buffer is free and then disable write mode
    usb_64drive_waitidle();
    usb_64drive_setwritable(FALSE);
}


/*==============================
    usb_64drive_arm
    Arms the 64Drive's USB
    @param The ROM offset to arm
    @param The size of the data to transfer
==============================*/

static void usb_64drive_arm(u32 offset, u32 size)
{
    u32 ret __attribute__((aligned(8)));
    ret = usb_64drive_armstatus();
    
    if (ret != D64_USB_ARMING && ret != D64_USB_ARMED)
    {
        usb_64drive_waitidle();
        
        // Arm the 64Drive, using the ROM space as a buffer
        #ifdef LIBDRAGON
            io_write(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, D64_USB_ARM);
            io_write(D64_CIBASE_ADDRESS + D64_REGISTER_USBP0R0, (offset >> 1));
            io_write(D64_CIBASE_ADDRESS + D64_REGISTER_USBP1R1, (size & 0xFFFFFF));
        #else
            #if USE_OSRAW
                osPiRawWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, D64_USB_ARM);
                osPiRawWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBP0R0, (offset >> 1));
                osPiRawWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBP1R1, (size & 0xFFFFFF));
            #else
                osPiWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, D64_USB_ARM);
                osPiWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBP0R0, (offset >> 1));
                osPiWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBP1R1, (size & 0xFFFFFF));
            #endif
        #endif
    }
}


/*==============================
    usb_64drive_disarm
    Disarms the 64Drive's USB
==============================*/

static void usb_64drive_disarm()
{
    // Disarm the USB
    #ifdef LIBDRAGON
        io_write(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, D64_USB_DISARM);
    #else
        #if USE_OSRAW
            osPiRawWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, D64_USB_DISARM); 
        #else
            osPiWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, D64_USB_DISARM); 
        #endif
    #endif
    usb_64drive_waitdisarmed();
}


/*==============================
    usb_64drive_poll
    Returns the header of data being received via USB on the 64Drive
    The first byte contains the data type, the next 3 the number of bytes left to read
    @return The data header, or 0
==============================*/

static u32 usb_64drive_poll()
{
    int i;
    u32 ret __attribute__((aligned(8)));
    
    // Arm the USB buffer
    usb_64drive_waitidle();
    usb_64drive_setwritable(TRUE);
    usb_64drive_arm(DEBUG_ADDRESS, DEBUG_ADDRESS_SIZE);
    
    // Burn some time to see if any USB data comes in
    for (i=0; i<d64_polltime; i++)
        ;
    
    // If there's data to service
    if (usb_64drive_armstatus() == D64_USB_DATA)
    {
        // Read the data header from the Param0 register
        #ifdef LIBDRAGON
            ret = io_read(D64_CIBASE_ADDRESS + D64_REGISTER_USBP0R0);
        #else
            #if USE_OSRAW
                osPiRawReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBP0R0, &ret);
            #else
                osPiReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBP0R0, &ret);
            #endif
        #endif
    
        // Get the data header
        usb_datatype = USBHEADER_GETTYPE(ret);
        usb_dataleft = USBHEADER_GETSIZE(ret);
        usb_datasize = usb_dataleft;
        usb_readblock = -1;
        
        // Return the data header
        usb_64drive_waitidle();
        usb_64drive_setwritable(FALSE);
        return USBHEADER_CREATE(usb_datatype, usb_datasize);
    }

    // Disarm the USB if no data arrived
    usb_64drive_disarm();
    usb_64drive_waitidle();
    usb_64drive_setwritable(FALSE);
    return 0;
}


/*==============================
    usb_64drive_read
    Reads bytes from the 64Drive ROM into the global buffer with the block offset
==============================*/

static void usb_64drive_read()
{
    // Set up DMA transfer between RDRAM and the PI
    #ifdef LIBDRAGON
        data_cache_hit_writeback_invalidate(usb_buffer, BUFFER_SIZE);
        dma_read(usb_buffer, D64_BASE_ADDRESS + DEBUG_ADDRESS + usb_readblock, BUFFER_SIZE);
    #else
        osWritebackDCacheAll();
        #if USE_OSRAW
            osPiRawStartDma(OS_READ, 
                         D64_BASE_ADDRESS + DEBUG_ADDRESS + usb_readblock, usb_buffer, 
                         BUFFER_SIZE);
        #else
            osPiStartDma(&dmaIOMessageBuf, OS_MESG_PRI_NORMAL, OS_READ, 
                         D64_BASE_ADDRESS + DEBUG_ADDRESS + usb_readblock, usb_buffer, 
                         BUFFER_SIZE, &dmaMessageQ);
            (void)osRecvMesg(&dmaMessageQ, NULL, OS_MESG_BLOCK);
        #endif
    #endif
}


/*==============================
    usb_set_64drive_polltime
    Sets the time which the 64Drive should poll
    for incoming USB data. Default is 2000.
    Lowering this value improves game performance
    at the cost of polling accuracy.
    This is a hacky workaround for a 64Drive bug,
    but it is planned to be removed in the future
    when the polling protocol is changed.
    @param The time to stall for waiting for USB
           data
==============================*/

void usb_set_64drive_polltime(int polltime)
{
    d64_polltime = polltime;
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
    u32 status __attribute__((aligned(8)));
    do
    {
        status = *(volatile unsigned long *)(N64_PI_ADDRESS + N64_PI_STATUS);
        status &= (PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY);
    }
    while (status);
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
    // Correct the PI address
    pi_address &= 0x1FFFFFFF;

    // Set up DMA transfer between RDRAM and the PI
    #ifdef LIBDRAGON
        data_cache_hit_writeback_invalidate(buff, len);
        disable_interrupts();
        // Write the data to the PI
        usb_everdrive_wait_pidma();
        IO_WRITE(PI_STATUS_REG, 3);
        *(volatile unsigned long *)(N64_PI_ADDRESS + N64_PI_RAMADDRESS) = (u32)buff;
        *(volatile unsigned long *)(N64_PI_ADDRESS + N64_PI_PIADDRESS) = pi_address;
        *(volatile unsigned long *)(N64_PI_ADDRESS + N64_PI_WRITELENGTH) = len-1;
        usb_everdrive_wait_pidma();
        // Enable system interrupts
        enable_interrupts();
    #else
        osInvalDCache(buff, len);
        #if USE_OSRAW
            osPiRawStartDma(OS_READ, 
                         pi_address, buff, 
                         len);
        #else
            osPiStartDma(&dmaIOMessageBuf, OS_MESG_PRI_NORMAL, OS_READ, 
                         pi_address, buff, 
                         len, &dmaMessageQ);
            (void)osRecvMesg(&dmaMessageQ, NULL, OS_MESG_BLOCK);
        #endif
    #endif
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
    // Correct the PI address
    pi_address &= 0x1FFFFFFF;
    
    // Set up DMA transfer between RDRAM and the PI
    #ifdef LIBDRAGON
        data_cache_hit_writeback(buff, len);
        disable_interrupts();
        // Write the data to the PI
        usb_everdrive_wait_pidma();
        IO_WRITE(PI_STATUS_REG, 3);
        *(volatile unsigned long *)(N64_PI_ADDRESS + N64_PI_RAMADDRESS) = (u32)buff;
        *(volatile unsigned long *)(N64_PI_ADDRESS + N64_PI_PIADDRESS) = pi_address;
        *(volatile unsigned long *)(N64_PI_ADDRESS + N64_PI_READLENGTH) = len-1;
        usb_everdrive_wait_pidma();
        // Enable system interrupts
        enable_interrupts();
    #else
        osWritebackDCache(buff, len);
        #if USE_OSRAW
            osPiRawStartDma(OS_WRITE, 
                         pi_address, buff, 
                         len);
        #else
            osPiStartDma(&dmaIOMessageBuf, OS_MESG_PRI_NORMAL, OS_WRITE, 
                         pi_address, buff, 
                         len, &dmaMessageQ);
            (void)osRecvMesg(&dmaMessageQ, NULL, OS_MESG_BLOCK);
        #endif
    #endif
}


/*==============================
    usb_everdrive_writereg
    Writes data to a specific register on the EverDrive
    @param The register to write to
    @param The value to write to the register
==============================*/

static void usb_everdrive_writereg(u64 reg, u32 value) 
{
    u32 val __attribute__((aligned(8))) = value;
    usb_everdrive_writedata(&val, ED_GET_REGADD(reg), sizeof(u32));
}


/*==============================
    usb_everdrive_usbbusy
    Spins until the USB is no longer busy
    @return 1 on success, 0 on failure
==============================*/

static u8 usb_everdrive_usbbusy() 
{
    u32 timeout = 0;
    u32 val __attribute__((aligned(8))) = 0;
    while ((val & ED_USBSTAT_ACT) != 0)
    {
        usb_everdrive_readreg(ED_REG_USBCFG, &val);
        timeout++;
        if (timeout > 8192)
        {
            usb_everdrive_writereg(ED_REG_USBCFG, ED_USBMODE_RDNOP);
            return 0;
        }
    } 
    return 1;
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

        // Wait for the FPGA to transfer the data to its internal buffer, or stop on timeout
        if (!usb_everdrive_usbbusy())
            return;

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
        
        // Set USB to write mode with the new address and wait for USB to end (or stop if it times out)
        usb_everdrive_writereg(ED_REG_USBCFG, ED_USBMODE_WR | baddr);
        if (!usb_everdrive_usbbusy())
            return;
        
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
    if (!usb_everdrive_usbbusy())
        return;
    
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
    
    // Get the aligned data size. Must be 16 byte aligned
    len = usb_datasize + (16 - ((usb_datasize%16 == 0) ? 16 : usb_datasize%16));
    
    // While there's data to service
    while (len > 0) 
    {
        u32 bytes_do = BUFFER_SIZE;
        if (len < BUFFER_SIZE)
            bytes_do = len;
            
        // Read a chunk from USB and store it into our temp buffer
        usb_everdrive_readusb(usb_buffer, bytes_do);
        
        // Copy received block to ROM
        usb_everdrive_writedata(usb_buffer, ED_BASE + DEBUG_ADDRESS + offset, bytes_do);
        offset += bytes_do;
        len -= bytes_do;
    }
    
    // Read the CMP Signal
    if (!usb_everdrive_usbbusy())
        return;
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
    // Set up DMA transfer between RDRAM and the PI
    #ifdef LIBDRAGON
        data_cache_hit_writeback_invalidate(usb_buffer, BUFFER_SIZE);
        while (dma_busy());
        *(vu32*)0xA4600010 = 3;
        dma_read(usb_buffer, ED_BASE + DEBUG_ADDRESS + usb_readblock, BUFFER_SIZE);
        data_cache_hit_writeback_invalidate(usb_buffer, BUFFER_SIZE);
    #else
        osWritebackDCacheAll();
        #if USE_OSRAW
            osPiRawStartDma(OS_READ, 
                         ED_BASE + DEBUG_ADDRESS + usb_readblock, usb_buffer, 
                         BUFFER_SIZE);
        #else
            osPiStartDma(&dmaIOMessageBuf, OS_MESG_PRI_NORMAL, OS_READ, 
                         ED_BASE + DEBUG_ADDRESS + usb_readblock, usb_buffer, 
                         BUFFER_SIZE, &dmaMessageQ);
            (void)osRecvMesg(&dmaMessageQ, NULL, OS_MESG_BLOCK);
        #endif
    #endif
}


/*********************************
       SC64 functions
*********************************/

/*==============================
    usb_sc64_execute_cmd
    Executes specified command in SC64 controller
    @param Command ID to execute
    @param 2 element array of 32 bit arguments to pass with command, use NULL when argument values are not needed
    @param 2 element array of 32 bit values to read command result, use NULL when result values are not needed
    @return Error status, non-zero means there was error during command execution
==============================*/

static u32 usb_sc64_execute_cmd(u8 cmd, u32 *args, u32 *result)
{
    u32 sr;

    // Write arguments if provided
    if (args != NULL)
    {
        usb_io_write(SC64_REG_DATA_0, args[0]);
        usb_io_write(SC64_REG_DATA_1, args[1]);
    }

    // Start execution
    usb_io_write(SC64_REG_SR_CMD, cmd);

    // Wait for completion
    do {
        sr = usb_io_read(SC64_REG_SR_CMD);
    } while (sr & SC64_SR_CMD_BUSY);

    // Read result if provided
    if (result != NULL)
    {
        result[0] = usb_io_read(SC64_REG_DATA_0);
        result[1] = usb_io_read(SC64_REG_DATA_1);
    }

    // Return error status
    return sr & SC64_SR_CMD_ERROR;
}


/*==============================
    usb_sc64_set_writable
    Enable ROM (SDRAM) writes in SC64
    @param A boolean with whether to enable or disable
    @return Previous value of setting
==============================*/

static u32 usb_sc64_set_writable(u32 enable)
{
    u32 args[2];
    u32 result[2];

    args[0] = SC64_CFG_ID_ROM_WRITE_ENABLE;
    args[1] = enable;
    usb_sc64_execute_cmd(SC64_CMD_CONFIG_SET, args, result);

    return result[1];
}


/*==============================
    usb_sc64_write
    Sends data through USB from the SC64
    @param The DATATYPE that is being sent
    @param A buffer with the data to send
    @param The size of the data being sent
==============================*/

static void usb_sc64_write(int datatype, const void* data, int size)
{
    u32 sdram_address = SC64_SDRAM_BASE + DEBUG_ADDRESS;
    u32 left = size;
    u32 writable_restore;
    u32 args[2];
    u32 result[2];

    // Enable SDRAM writes and get previous setting
    writable_restore = usb_sc64_set_writable(TRUE);

    while (left > 0)
    {
        // Calculate transfer size
        u32 dma_length = ALIGN(MIN(left, BUFFER_SIZE), 2);

        // Copy data to PI DMA aligned buffer
        memcpy(usb_buffer, data, dma_length);

        // Copy block of data from RDRAM to SDRAM
        usb_dma_write(usb_buffer, sdram_address, dma_length);

        // Update pointers and variables
        data += dma_length;
        sdram_address += dma_length;
        left -= dma_length;
    }

    // Restore previous SDRAM writable setting
    usb_sc64_set_writable(writable_restore);

    // Start sending data from buffer in SDRAM
    args[0] = SC64_SDRAM_BASE + DEBUG_ADDRESS;
    args[1] = USBHEADER_CREATE(datatype, size);
    if (usb_sc64_execute_cmd(SC64_CMD_USB_WRITE, args, NULL))
    {
        // Return if USB write was unsuccessful
        return;
    }

    // Wait for transfer to end
    do {
        usb_sc64_execute_cmd(SC64_CMD_USB_WRITE_STATUS, NULL, result);
    } while (result[0] & SC64_USB_WRITE_STATUS_BUSY);
}


/*==============================
    usb_sc64_poll
    Returns the header of data being received via USB on the SC64
    The first byte contains the data type, the next 3 the number of bytes left to read
    @return The data header, or 0
==============================*/

static u32 usb_sc64_poll(void)
{
    u8 datatype;
    u32 length;
    u32 args[2];
    u32 result[2];

    // Get read status and extract packet info
    usb_sc64_execute_cmd(SC64_CMD_USB_READ_STATUS, NULL, result);
    datatype = result[0] & 0xFF;
    length = result[1] & 0xFFFFFF;

    // Return 0 if there's no data
    if (length == 0)
        return 0;
        
        // Fill USB read data variables
        usb_datatype = datatype;
        usb_dataleft = length;
        usb_datasize = usb_dataleft;
        usb_readblock = -1;

        // Start receiving data to buffer in SDRAM
        args[0] = SC64_SDRAM_BASE + DEBUG_ADDRESS;
        args[1] = length;
    if (usb_sc64_execute_cmd(SC64_CMD_USB_READ, args, NULL))
    {
        // Return 0 if USB read was unsuccessful
        return 0;
    }

        // Wait for completion
        do {
            usb_sc64_execute_cmd(SC64_CMD_USB_READ_STATUS, NULL, result);
        } while (result[0] & SC64_USB_READ_STATUS_BUSY);

        // Return USB header
        return USBHEADER_CREATE(datatype, length);
}


/*==============================
    usb_sc64_read
    Reads bytes from the SC64 SDRAM into the global buffer with the block offset
==============================*/

static void usb_sc64_read(void)
{
    // Set up DMA transfer between RDRAM and the PI
    usb_dma_read(usb_buffer, SC64_SDRAM_BASE + DEBUG_ADDRESS + usb_readblock, BUFFER_SIZE);
}