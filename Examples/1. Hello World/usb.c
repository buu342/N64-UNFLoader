/***************************************************************
                            usb.c
                               
Allows USB communication between an N64 flashcart and the PC
using UNFLoader.
https://github.com/buu342/N64-UNFLoader
***************************************************************/

#include <ultra64.h>
#include <string.h>
#include "usb.h"


/*********************************
       Standard Definitions
*********************************/

// Input/Output buffer size. Keep at 512
#define BUFFER_SIZE       512

// Cart definitions
#define CART_NONE       0
#define CART_64DRIVE    2
#define CART_EVERDRIVE3 3
#define CART_EVERDRIVE7 4


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
#define D64_DEBUG_ADDRESS  0x03F00000 // Put the debug area at the 63MB area in SDRAM
#define D64_DEBUG_ADDRESS_SIZE 1*1024*1024

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
#define D64_USB_DATA        0x02
#define D64_USB_BUSY        0x0F
#define D64_USB_DISARM      0x0F
#define D64_USB_ARM         0x0A
#define D64_USB_ARMED       0x01
#define D64_USB_ARMING      0x0F
#define D64_USB_IDLEUNARMED 0x00

#define D64_CI_IDLE  0x00
#define D64_CI_BUSY  0x10
#define D64_CI_WRITE 0x20


/*********************************
       EverDrive 3.0 macros
*********************************/

#define ED3_BASE_ADDRESS  0xA8040000
#define ED3_WRITE_ADDRESS 0xB3FFF800
#define ED3_ROM_ADDRESS   0xB0000000
#define ED3_ROM_BUFFER    0x7FFF

#define ED3_REGISTER_CFG     0x00
#define ED3_REGISTER_STATUS  0x04
#define ED3_REGISTER_DMALEN  0x08
#define ED3_REGISTER_DMAADD  0x0C
#define ED3_REGISTER_MSG     0x10
#define ED3_REGISTER_DMACFG  0x14
#define ED3_REGISTER_KEY     0x20
#define ED3_REGISTER_VERSION 0x2C

#define ED3_DMA_BUSY  1
#define ED3_DMA_WRITE 4

#define ED3_DMACFG_USB2RAM 3
#define ED3_DMACFG_RAM2USB 4

#define ED3_VERSION 0xA3F0A3F0
#define ED3_REGKEY 0x1234


/*********************************
       EverDrive X7 macros
*********************************/

#define ED7_BASE_ADDRESS   0x1F800000

#define ED7_GET_REGADD(reg)   (0xA0000000 | ED7_BASE_ADDRESS | (reg))

#define ED7_REGISTER_VERSION 0x14

#define ED7_REG_USBCFG 0x0004
#define ED7_REG_USBDAT 0x0400
#define ED7_REG_SYSCFG 0x8000
#define ED7_REG_KEY    0x8004

#define ED7_USBMODE_RDNOP  0xC400
#define ED7_USBMODE_RD     0xC600
#define ED7_USBMODE_WRNOP  0xC000
#define ED7_USBMODE_WR     0xC200

#define ED7_USBSTAT_ACT     0x0200
#define ED7_USBSTAT_BUSY    0x2000

#define ED7_REGKEY  0xAA55
#define ED7_VERSION 0x00000000


/*********************************
        Function Prototypes
*********************************/

static void usb_findcart();
static void usb_64drive_write(int datatype, const void* data, int size);
static u8   usb_64drive_read();
static void usb_everdrive3_write(int datatype, const void* data, int size);
static u8   usb_everdrive3_read();
static void usb_everdrive7_write(int datatype, const void* data, int size);
static u8   usb_everdrive7_read();
static void usb_everdrive7_writereg(u64 reg, u32 value);


/*********************************
             Globals
*********************************/

// Function pointers
void (*funcPointer_write)(int datatype, const void* data, int size);
u8   (*funcPointer_read)();

// USB globals
static s8 usb_cart = CART_NONE;
static u8 usb_bufferout[BUFFER_SIZE] __attribute__((aligned(16)));
static u8 usb_bufferin[BUFFER_SIZE] __attribute__((aligned(16)));

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


/*********************************
           USB functions
*********************************/

/*==============================
    usb_initialize
    Initializes the USB buffers and pointers
==============================*/

static void usb_initialize()
{
    // Initialize the debug related globals
    memset(usb_bufferout, 0, BUFFER_SIZE);
    memset(usb_bufferin, 0, BUFFER_SIZE);
        
    // Create the message queue
    #if !USE_OSRAW
        osCreateMesgQueue(&dmaMessageQ, &dmaMessageBuf, 1);
    #endif
    
    // Find the flashcart
    usb_findcart();

    // Set the function pointers based on the flashcart
    switch(usb_cart)
    {
        case CART_64DRIVE:
            funcPointer_write = usb_64drive_write;
            funcPointer_read  = usb_64drive_read;
            break;
        case CART_EVERDRIVE3:
            funcPointer_write = usb_everdrive3_write;
            funcPointer_read  = usb_everdrive3_read;
            break;
        case CART_EVERDRIVE7:
            funcPointer_write = usb_everdrive7_write;
            funcPointer_read  = usb_everdrive7_read;
            break;
        default:
            return;
    }
}


/*==============================
    usb_findcart
    Checks if the game is running on a 64Drive or an EverDrive.
==============================*/

static void usb_findcart()
{
    u32 buff;
    
    // Read the cartridge and check if we have a 64Drive.
    #if USE_OSRAW
        osPiRawReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_MAGIC, &buff);
    #else
        osPiReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_MAGIC, &buff);
    #endif
    if (buff == D64_MAGIC)
    {
        usb_cart = CART_64DRIVE;
        return;
    }
    
    // Read the cartridge and check if we have an EverDrive 3.0
    #if USE_OSRAW
        osPiRawReadIo(ED3_BASE_ADDRESS + ED3_REGISTER_VERSION, &buff);
    #else
        osPiReadIo(ED3_BASE_ADDRESS + ED3_REGISTER_VERSION, &buff);
    #endif
    if (buff == ED3_VERSION)
    {
        // Write the key to unlock the registers and set the debug cart to ED3
        IO_WRITE(ED3_BASE_ADDRESS + ED3_REGISTER_KEY, ED3_REGKEY);
        usb_cart = CART_EVERDRIVE3;
        return;
    }
    
    // Read the cartridge and check if we have an EverDrive X7
    #if USE_OSRAW
        osPiRawReadIo(ED7_BASE_ADDRESS + ED7_REGISTER_VERSION, &buff);
    #else
        osPiReadIo(ED7_BASE_ADDRESS + ED7_REGISTER_VERSION, &buff);
    #endif
    if (buff == ED7_VERSION)
    {
        // Initialize the PI
        IO_WRITE(PI_BSD_DOM1_LAT_REG, 0x04);
        IO_WRITE(PI_BSD_DOM1_PWD_REG, 0x0C);
        
        // Write the key to unlock the registers and set the USB mode
        usb_everdrive7_writereg(ED7_REG_KEY, ED7_REGKEY);
        usb_everdrive7_writereg(ED7_REG_SYSCFG, 0);
        usb_everdrive7_writereg(ED7_REG_USBCFG, ED7_USBMODE_RDNOP);
        
        // Set the cart to EverDrive X7
        usb_cart = CART_EVERDRIVE7;
        return;
    }
}


/*==============================
    usb_write
    Writes data to the USB.
    @param The DATATYPE that is being sent
    @param A buffer with the data to send
    @param The size of the data being sent
==============================*/

void usb_write(int datatype, const void* data, int size)
{
    // If the cartridge hasn't been initialized, do so.
    if (usb_cart == CART_NONE)
        usb_initialize();
        
    // If no debug cart exists, stop
    if (usb_cart == CART_NONE)
        return;
        
    // Call the correct write function
    funcPointer_write(datatype, data, size);
}


/*==============================
    usb_read
    TODO: Implement this properly
==============================*/

u8 usb_read()
{
    // If the cartridge hasn't been initialized, do so.
    if (usb_cart == CART_NONE)
        usb_initialize();
        
    // If no debug cart exists, stop
    if (usb_cart == CART_NONE)
        return 0;
        
    // Call the correct read function
    return funcPointer_read();
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
    u32 ret;
    u32 timeout = 0; // I wanted to use osGetTime() but that requires the VI manager
    
    // Wait until the cartridge interface is ready
    do
    {
        #if USE_OSRAW
            osPiRawReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_STATUS, &ret);
        #else
            osPiReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_STATUS, &ret);
        #endif
        
        // Took too long, abort
        if((timeout++) > 1000000)
            return -1;
    }
    while((ret >> 8) & D64_CI_BUSY);
    
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
    #if USE_OSRAW
        osPiRawWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_COMMAND, enable ? D64_ENABLE_ROMWR : D64_DISABLE_ROMWR);
    #else
        osPiWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_COMMAND, enable ? D64_ENABLE_ROMWR : D64_DISABLE_ROMWR);
    #endif
    usb_64drive_wait();
}


/*==============================
    usb_64drive_waitidle
    Waits for the 64Drive's USB to be idle
==============================*/

static void usb_64drive_waitidle()
{
    u32 status;
    do 
    {
        #if USE_OSRAW
            osPiRawReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, &status);
        #else
            osPiReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, &status);
        #endif
        status = (status >> 4) & D64_USB_BUSY;
    }
    while(status != D64_USB_IDLE);
}


/*==============================
    usb_64drive_waitdata
    Waits for the 64Drive's USB be ablt to receive data
==============================*/

static void usb_64drive_waitdata()
{
    u32 status;
    do
    {
        #if USE_OSRAW
            osPiRawReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, &status);
        #else
            osPiReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, &status);
        #endif
        status &= 0x0F;
    }
    while (status == D64_USB_IDLEUNARMED || status == D64_USB_ARMED);
}


/*==============================
    usb_64drive_waitdisarmed
    Waits for the 64Drive's USB to be disarmed
==============================*/

static void usb_64drive_waitdisarmed()
{
    u32 status;
    do
    {
        #if USE_OSRAW
            osPiRawReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, &status);
        #else
            osPiReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, &status);
        #endif
        status &= 0x0F;
    }
    while (status != D64_USB_IDLEUNARMED);
}


/*==============================
    usb_64drive_write
    Sends data through USB from the 64Drive
    @param The DATATYPE that is being sent
    @param A buffer with the data to send
    @param The size of the data being sent
==============================*/

static void usb_64drive_write(int datatype, const void* data, int size)
{
    int left = size;
    int read = 0;
    
    // Spin until the write buffer is free and then set the cartridge to write mode
    usb_64drive_waitidle();
    usb_64drive_setwritable(TRUE);
    
    // Write data to SDRAM until we've finished
    while (left > 0)
    {
        int block = left;
        if (block > BUFFER_SIZE)
            block = BUFFER_SIZE;
            
        // Copy the data to the global buffer
        memcpy(usb_bufferout, (void*)((char*)data+read), block);

        // If the data was not 32-bit aligned, pad the buffer
        if(block < BUFFER_SIZE && size%4 != 0)
        {
            u32 i;
            u32 size_new = (size & ~3)+4;
            block += size_new-size;
            for(i=size; i<size_new; i++) 
                usb_bufferout[i] = 0;
            size = size_new;
        }
        
        // Spin until the write buffer is free
        usb_64drive_waitidle();
        
        // Set up DMA transfer between RDRAM and the PI
        osWritebackDCache(usb_bufferout, block);
        #if USE_OSRAW
            osPiRawStartDma(OS_WRITE, 
                         D64_BASE_ADDRESS + D64_DEBUG_ADDRESS + read, 
                         usb_bufferout, block);
        #else
            osPiStartDma(&dmaIOMessageBuf, OS_MESG_PRI_NORMAL, OS_WRITE, 
                         D64_BASE_ADDRESS + D64_DEBUG_ADDRESS + read, 
                         usb_bufferout, block, &dmaMessageQ);
            (void)osRecvMesg(&dmaMessageQ, NULL, OS_MESG_BLOCK);
        #endif
        
        // Keep track of what we've read so far
        left -= block;
        read += block;
    }
    
    // Send the data through USB
    #if USE_OSRAW
        osPiRawWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBP0R0, D64_DEBUG_ADDRESS >> 1);
        osPiRawWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBP1R1, (size & 0xFFFFFF) | (datatype << 24));
        osPiRawWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, D64_COMMAND_WRITE);
    #else
        osPiWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBP0R0, D64_DEBUG_ADDRESS >> 1);
        osPiWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBP1R1, (size & 0xFFFFFF) | (datatype << 24));
        osPiWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, D64_COMMAND_WRITE);
    #endif
        
    // Spin until the write buffer is free and then disable write mode
    usb_64drive_waitidle();
    usb_64drive_setwritable(FALSE);
}


/*==============================
    usb_64drive_read
    Polls the 64Drive for commands to run
    @return 1 if success, 0 otherwise
==============================*/

static u8 usb_64drive_read()
{
    u32 ret;
    u32 length=0;
    u32 remaining=0;
    u8 type=0; // Unused for now, will later be used to detect what was inputted (text, file, etc...)
    
    // Check if we've received data
    #if USE_OSRAW
        osPiRawReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, &ret);
    #else
        osPiReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, &ret);
    #endif
    if (ret != D64_USB_DATA)
        return 0;
        
    // Check if the USB is armed, and arm it if it isn't
    #if USE_OSRAW
        osPiRawReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, &ret); 
    #else
        osPiReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, &ret); 
    #endif
    if (ret != D64_USB_ARMING || ret != D64_USB_ARMED)
    {
        // Ensure the 64Drive is idle
        usb_64drive_waitidle();
        
        // Arm the USB FIFO DMA
        #if USE_OSRAW
            osPiRawWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBP0R0, D64_DEBUG_ADDRESS >> 1);
            osPiRawWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBP1R1, BUFFER_SIZE & 0xFFFFFF);
            osPiRawWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, D64_USB_ARM);
        #else
            osPiWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBP0R0, D64_DEBUG_ADDRESS >> 1);
            osPiWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBP1R1, BUFFER_SIZE & 0xFFFFFF);
            osPiWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, D64_USB_ARM);
        #endif
    }  
    
    // Read data
    do
    {
        // Wait for data
        usb_64drive_waitdata();
        
        // See how much data is left to read
        #if USE_OSRAW
            osPiRawReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBP0R0, &ret); 
        #else
            osPiReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBP0R0, &ret); 
        #endif
        length = ret & 0xFFFFFF;
        type = (ret >> 24) & 0xFF;
        #if USE_OSRAW
            osPiRawReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBP1R1, &ret); 
        #else
            osPiReadIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBP1R1, &ret); 
        #endif
        remaining = ret & 0xFFFFFF;
        
        // Set up DMA transfer between RDRAM and the PI
        osWritebackDCache(usb_bufferin, length);
        #if USE_OSRAW
            osPiRawStartDma(OS_READ, 
                         D64_BASE_ADDRESS + D64_DEBUG_ADDRESS, usb_bufferin, 
                         length);
        #else
            osPiStartDma(&dmaIOMessageBuf, OS_MESG_PRI_NORMAL, OS_READ, 
                         D64_BASE_ADDRESS + D64_DEBUG_ADDRESS, usb_bufferin, 
                         length, &dmaMessageQ);
            (void)osRecvMesg(&dmaMessageQ, NULL, OS_MESG_BLOCK);
        #endif
    }
    while (remaining > 0);
    
    // Disarm the USB and return success
    #if USE_OSRAW
        osPiRawWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, D64_USB_DISARM); 
    #else
        osPiWriteIo(D64_CIBASE_ADDRESS + D64_REGISTER_USBCOMSTAT, D64_USB_DISARM); 
    #endif
    usb_64drive_waitdisarmed();
    return 1;
}


/*********************************
      EverDrive 3.0 functions
*********************************/

/*==============================
    usb_everdrive3_regread
    Writes data to the specified register on the EverDrive 3.0
    @param The register to write to
    @param The data to write
==============================*/

static void usb_everdrive3_regwrite(unsigned long reg, unsigned long data)
{
    *(volatile unsigned long *) (ED3_BASE_ADDRESS);
    *(volatile unsigned long *) (ED3_BASE_ADDRESS + reg) = data;
}


/*==============================
    usb_everdrive3_regread
    Reads data from the specified register on the EverDrive 3.0
    @param The register to read from
==============================*/

static unsigned long usb_everdrive3_regread(unsigned long reg)
{
    *(volatile unsigned long *) (ED3_BASE_ADDRESS);
    return *(volatile unsigned long *) (ED3_BASE_ADDRESS + reg);
}


/*==============================
    usb_everdrive3_wait_pidma
    Spins until the EverDrive 3.0's PI is ready
==============================*/

static void usb_everdrive3_wait_pidma() 
{
    u32 status;
    do
    {
        status = *(volatile unsigned long *)(N64_PI_ADDRESS + N64_PI_STATUS);
        status &= (PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY);
    }
    while (status);
}


/*==============================
    usb_everdrive3_wait_write
    Spins until the EverDrive 3.0's can write
==============================*/

static void usb_everdrive3_wait_write()
{
    u32 status;
    do
    {
        status = usb_everdrive3_regread(ED3_REGISTER_STATUS);
        status &= ED3_DMA_WRITE;
    }
    while (status != 0);
}


/*==============================
    usb_everdrive3_wait_dma
    Spins until the EverDrive 3.0's DMA is ready
==============================*/

static void usb_everdrive3_wait_dma()
{
    u32 status;
    do
    {
        status = usb_everdrive3_regread(ED3_REGISTER_STATUS);
        status &= ED3_DMA_BUSY;
    }
    while (status != 0);
}


/*==============================
    usb_everdrive3_write
    Sends data through USB from the EverDrive 3.0
    @param The DATATYPE that is being sent
    @param A buffer with the data to send
    @param The size of the data being sent
==============================*/

static void usb_everdrive3_write(int datatype, const void* data, int size)
{
    char wrotecmp = 0;
    char cmp[] = {'C', 'M', 'P', 'H'};
    int read = 0;
    int left = size;
    int offset = 8;
    u32 header = (size & 0xFFFFFF) | (datatype << 24);
    
    // Put in the DMA header along with size and type information in the global buffer
    usb_bufferout[0] = 'D';
    usb_bufferout[1] = 'M';
    usb_bufferout[2] = 'A';
    usb_bufferout[3] = '@';
    usb_bufferout[4] = (header >> 24) & 0xFF;
    usb_bufferout[5] = (header >> 16) & 0xFF;
    usb_bufferout[6] = (header >> 8)  & 0xFF;
    usb_bufferout[7] = header & 0xFF;
    
    // Write data to USB until we've finished
    while (left > 0)
    {
        int block = left;
        if (block+offset > BUFFER_SIZE)
            block = BUFFER_SIZE-offset;
            
        // Copy the data to the next available spots in the global buffer
        memcpy(usb_bufferout+offset, (void*)((char*)data+read), block);
        
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

        // Set up DMA transfer between RDRAM and the PI
        osWritebackDCache(usb_bufferout, BUFFER_SIZE);
        #if USE_OSRAW
            osPiRawStartDma(OS_WRITE, 
                         ED3_WRITE_ADDRESS, usb_bufferout, 
                         BUFFER_SIZE);
        #else
            osPiStartDma(&dmaIOMessageBuf, OS_MESG_PRI_NORMAL, OS_WRITE, 
                         ED3_WRITE_ADDRESS, usb_bufferout, 
                         BUFFER_SIZE, &dmaMessageQ);
            (void)osRecvMesg(&dmaMessageQ, NULL, OS_MESG_BLOCK);
        #endif
        
        // Write the data to the PI
        usb_everdrive3_wait_pidma();
        IO_WRITE(PI_STATUS_REG, 3);
        *(volatile unsigned long *)(N64_PI_ADDRESS + N64_PI_RAMADDRESS) = (u32)usb_bufferout;
        *(volatile unsigned long *)(N64_PI_ADDRESS + N64_PI_PIADDRESS) = ED3_WRITE_ADDRESS & 0x1FFFFFFF;
        *(volatile unsigned long *)(N64_PI_ADDRESS + N64_PI_READLENGTH) = BUFFER_SIZE-1;
        usb_everdrive3_wait_pidma();
        
        // Write the data to the EverDrive 3.0's registers
        usb_everdrive3_wait_write();
        usb_everdrive3_regwrite(ED3_REGISTER_DMALEN, 0);
        usb_everdrive3_regwrite(ED3_REGISTER_DMAADD, ED3_ROM_BUFFER);
        usb_everdrive3_regwrite(ED3_REGISTER_DMACFG, ED3_DMACFG_RAM2USB);
        usb_everdrive3_wait_dma();
        
        // Keep track of what we've read so far
        left -= block;
        read += block;
        offset = 0;
    }
}


/*==============================
    usb_everdrive3_read
    TODO: Write this header
==============================*/

static u8 usb_everdrive3_read()
{
    // TODO: Implement this function
    return 0;
}


/*********************************
      EverDrive X7 functions
*********************************/

/*==============================
    usb_everdrive7_wait_pidma
    Spins until the EverDrive X7's DMA is ready
==============================*/

static void usb_everdrive7_wait_pidma() 
{
    u32 status;
    do
    {
        status = *(volatile unsigned long *)(N64_PI_ADDRESS + N64_PI_STATUS);
        status &= (PI_STATUS_DMA_BUSY | PI_STATUS_IO_BUSY);
    }
    while (status);
}


/*==============================
    usb_everdrive7_readdata
    Reads data from a specific address on the EverDrive X7
    @param The buffer with the data
    @param The register address to write to the PI
    @param The size of the data
==============================*/

static void usb_everdrive7_readdata(void* buff, u32 pi_address, u32 len) 
{
    // Correct the PI address
    pi_address = ED7_GET_REGADD(pi_address);

    // Set up DMA transfer between RDRAM and the PI
    osWritebackDCache(buff, len);
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

    // Write the data to the PI
    usb_everdrive7_wait_pidma();
    IO_WRITE(PI_STATUS_REG, 3);
    *(volatile unsigned long *)(N64_PI_ADDRESS + N64_PI_RAMADDRESS) = (u32)buff;
    *(volatile unsigned long *)(N64_PI_ADDRESS + N64_PI_PIADDRESS) = pi_address & 0x1FFFFFFF;
    *(volatile unsigned long *)(N64_PI_ADDRESS + N64_PI_READLENGTH) = len-1;
    usb_everdrive7_wait_pidma();
}


/*==============================
    usb_everdrive7_readreg
    Reads data from a specific register on the EverDrive X7
    @param The register to read from
    @param A pointer to write the read value to
==============================*/

static void usb_everdrive7_readreg(u32 reg, u32* result) 
{
    usb_everdrive7_readdata(result, reg, sizeof(u32));
}


/*==============================
    usb_everdrive7_writedata
    Writes data to a specific address on the EverDrive X7
    @param A buffer with the data to write
    @param The register address to write to the PI
    @param The length of the data
==============================*/

static void usb_everdrive7_writedata(void* buff, u32 pi_address, u32 len) 
{
    // Correct the PI address
    pi_address = ED7_GET_REGADD(pi_address);
    
    // Set up DMA transfer between RDRAM and the PI
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
    
    // Write the data to the PI
    usb_everdrive7_wait_pidma();
    IO_WRITE(PI_STATUS_REG, 3);
    *(volatile unsigned long *)(N64_PI_ADDRESS + N64_PI_RAMADDRESS) = (u32)buff;
    *(volatile unsigned long *)(N64_PI_ADDRESS + N64_PI_PIADDRESS) = pi_address & 0x1FFFFFFF;
    *(volatile unsigned long *)(N64_PI_ADDRESS + N64_PI_WRITELENGTH) = len-1;
    usb_everdrive7_wait_pidma();
}


/*==============================
    usb_everdrive7_writereg
    Writes data to a specific register on the EverDrive X7
    @param The register to write to
    @param The value to write to the register
==============================*/

static void usb_everdrive7_writereg(u64 reg, u32 value) 
{
    usb_everdrive7_writedata(&value, reg, sizeof(u32));
}


/*==============================
    usb_everdrive7_usbbusy
    Spins until the USB is no longer busy
==============================*/

static void usb_everdrive7_usbbusy() 
{
    u32 val;
    do 
    {
        usb_everdrive7_readreg(ED7_REG_USBCFG, &val);
    } 
    while ((val & ED7_USBSTAT_ACT) != 0);
}


/*==============================
    usb_everdrive7_write
    Sends data through USB from the EverDrive X7
    @param The DATATYPE that is being sent
    @param A buffer with the data to send
    @param The size of the data being sent
==============================*/

static void usb_everdrive7_write(int datatype, const void* data, int size)
{
    char wrotecmp = 0;
    char cmp[] = {'C', 'M', 'P', 'H'};
    int read = 0;
    int left = size;
    int offset = 8;
    u32 header = (size & 0xFFFFFF) | (datatype << 24);
    
    // Put in the DMA header along with length and type information in the global buffer
    usb_bufferout[0] = 'D';
    usb_bufferout[1] = 'M';
    usb_bufferout[2] = 'A';
    usb_bufferout[3] = '@';
    usb_bufferout[4] = (header >> 24) & 0xFF;
    usb_bufferout[5] = (header >> 16) & 0xFF;
    usb_bufferout[6] = (header >> 8)  & 0xFF;
    usb_bufferout[7] = header & 0xFF;
    
    // Write data to USB until we've finished
    while (left > 0)
    {
        int block = left;
        int blocksend, baddr;
        if (block+offset > BUFFER_SIZE)
            block = BUFFER_SIZE-offset;
            
        // Copy the data to the next available spots in the global buffer
        memcpy(usb_bufferout+offset, (void*)((char*)data+read), block);
        
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
        usb_everdrive7_writereg(ED7_REG_USBCFG, ED7_USBMODE_WRNOP);
        usb_everdrive7_writedata(usb_bufferout, ED7_REG_USBDAT + baddr, blocksend);
        
        // Set USB to write mode with the new address and wait for USB to end
        usb_everdrive7_writereg(ED7_REG_USBCFG, ED7_USBMODE_WR | baddr);
        usb_everdrive7_usbbusy();
        
        // Keep track of what we've read so far
        left -= block;
        read += block;
        offset = 0;
    }
}


/*==============================
    usb_everdrive7_read
    TODO: Write this header
==============================*/

static u8 usb_everdrive7_read()
{        
    // TODO: Implement this function
    return 0;
}