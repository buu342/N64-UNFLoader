################################################################
#                    Code files and ROM name                   #
################################################################

ROM_NAME = unflexa3.n64

CODECFILES = main.c texture.c
CODEHFILES = 

DEBUG_MODE = 1 # Remember to change debug.h as well to match!
DEBUGFILES = debug.c usb.c

REGISTRATION = "UNFLEXAM3" B U3 I


################################################################
#     Don't touch below unless you know what you're doing!     #
################################################################

ROOTDIR = $(N64_INST)
GCCN64PREFIX = $(ROOTDIR)/bin/mips64-elf-
CHKSUM64PATH = $(ROOTDIR)/bin/chksum64
MKDFSPATH = $(ROOTDIR)/bin/mkdfs
HEADERPATH = $(ROOTDIR)/mips64-elf/lib
N64TOOL = $(ROOTDIR)/bin/n64tool


################################################################
#                       Compiler Settings                      #
################################################################

CC = $(GCCN64PREFIX)gcc
AS = $(GCCN64PREFIX)as
LD = $(GCCN64PREFIX)ld
OBJCOPY = $(GCCN64PREFIX)objcopy


################################################################
#                         Make Commands                        #
################################################################

default: $(ROM_NAME)


################################################################
#                     ROM debug mode check                     #
################################################################

ifeq ($(DEBUG_MODE), 0)
OBJ_FILES = $(CODECFILES:.c=.o)
CFLAGS = -std=gnu99 -march=vr4300 -mtune=vr4300 -O2 -Wall -Wno-unused-function -I$(ROOTDIR)/mips64-elf/include
else
OBJ_FILES = $(CODECFILES:.c=.o) $(DEBUGFILES:.c=.o)
CFLAGS = -std=gnu99 -march=vr4300 -mtune=vr4300 -g -Wall -Wno-unused-function -I$(ROOTDIR)/mips64-elf/include
endif


################################################################
#                        Linker Settings                       #
################################################################

HEADERNAME = header
LINK_FLAGS = -L$(ROOTDIR)/mips64-elf/lib -ldragon -lc -lm -ldragonsys -Tn64.ld
ASFLAGS = -mtune=vr4300 -march=vr4300

N64_FLAGS = -l 2M -h $(HEADERPATH)/$(HEADERNAME) -o $(ROM_NAME)


################################################################
#                          Compilation                         #
################################################################

$(ROM_NAME): $(ROM_NAME:.n64=.elf)
	$(OBJCOPY) $(ROM_NAME:.n64=.elf) $(ROM_NAME:.n64=.bin) -O binary
	rm -f $(ROM_NAME)
	$(N64TOOL) $(N64_FLAGS) $(ROM_NAME:.n64=.bin)
	$(CHKSUM64PATH) $(ROM_NAME)
ifeq ($(ALLOWREGISTER), 1)
    nrdc $(TARGETS) $(REGISTRATION)
endif

$(ROM_NAME:.n64=.elf) : $(OBJ_FILES)
	$(LD) -o $(ROM_NAME:.n64=.elf) $(OBJ_FILES) $(LINK_FLAGS)

clean:
	rm -f *.n64 *.elf *.o *.bin
