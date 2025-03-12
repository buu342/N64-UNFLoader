################################################################
#	THIS MAKEFILE IS MEANT FOR THE N64 SDK FOR LINUX       #
#	BY CRASHOVERIDE. IF USING Partner-N64 SDK PLEASE       #
#	USE THE OTHER makefile!!!!!!!!!!!!!!!!!!!!!!!!!!       #
################################################################

################################################################
#                    Code files and ROM name                   #
################################################################

ASMFILES    = asm/entry.s asm/rom_header.s
ASMOBJECTS  =	$(ASMFILES:.s=.o)
CODECFILES  = main.c stacks.c 
CODEHFILES  =

DEBUGFILES = usb.c

ELF		= unflexa3.elf
TARGETS	= unflexa3.z64
MAP		= unflexa3.map

LD_SCRIPT	= unflexa3.ld
CP_LD_SCRIPT	= unflexa3_cp.ld


################################################################
#                         Make Commands                        #
################################################################

default: $(TARGETS)
    



################################################################
#     Don't touch below unless you know what you're doing!     #
################################################################

include $(ROOT)/usr/include/make/PRdefs

BOOT		= /usr/lib/n64/PR/bootcode/boot.6102
BOOT_OBJ	= boot.6102.o
CODESEGMENT = codesegment.o
OBJECTS     = $(ASMOBJECTS) $(BOOT_OBJ) $(CODESEGMENT)


################################################################
#                     ROM debug mode check                     #
################################################################

ifeq ($(DEBUG_MODE), 0)
    CODEOBJECTS     = $(CODECFILES:.c=.o)
    OPTIMIZER       = -O2
    LCDEFS          = -D_FINALROM -DNDEBUG -DF3DEX_GBI_2 -DNOT_SPEC
    N64LIB          = -lultra_rom
    MAKEROMFLAGS    = 
else
    CODEOBJECTS     = $(CODECFILES:.c=.o) $(DEBUGFILES:.c=.o)
    OPTIMIZER       = -g
    LCDEFS          = -DDEBUG  -DF3DEX_GBI_2 -DNOT_SPEC
    N64LIB          = -lultra_d
    MAKEROMFLAGS    = 
endif


################################################################
#                        Linker Settings                       #
################################################################

LCINCS =	-I. -I$(ROOT)/usr/include/PR -I $(ROOT)/usr/include -I $(ROOT)/usr/include/nustd
LCOPTS =	-G 0
LDIRT  =	$(ELF) $(CP_LD_SCRIPT) $(TARGETS) $(MAP) $(ASMOBJECTS)
LDFLAGS=	$(MKDEPOPT)  -L$(ROOT)/usr/lib -L$(ROOT)/usr/lib/PR $(N64LIB) -L$(N64_LIBGCCDIR) -lgcc -lnustd


################################################################
#                          Compilation                         #
################################################################

include $(COMMONRULES)

.s.o:
	$(AS) -Wa,-Iasm -o $@ $<

$(CODESEGMENT):	$(CODEOBJECTS)
	$(LD) -o $(CODESEGMENT) -r $(CODEOBJECTS) $(LDFLAGS)

$(BOOT_OBJ): $(BOOT)
	$(OBJCOPY) -I binary -B mips -O elf32-bigmips $< $@

$(CP_LD_SCRIPT): $(LD_SCRIPT)
	cpp -P -Wno-trigraphs -o $@ $<

$(TARGETS): $(OBJECTS) $(CP_LD_SCRIPT)
	$(LD) -L. -T $(CP_LD_SCRIPT) -Map $(MAP) -o $(ELF) 
	$(OBJCOPY) --pad-to=0x100000 --gap-fill=0xFF $(ELF) $(TARGETS) -O binary
	makemask $(TARGETS)
