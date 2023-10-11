################################################################
#                    Code files and ROM name                   #
################################################################

CODECFILES  = main.c graphic.c stage00.c spr_chuck.c spr_arena.c
CODEHFILES  =

DEBUGFILES = debug.c usb.c

TARGETS = unflexa2.n64

REGISTRATION = "UNFLEXAM2" B U2 I


################################################################
#                         Nusys Config                         #
################################################################

N64KITDIR	= c:/nintendo/n64kit
NUSYSDIR	= $(N64KITDIR)/nusys
NUSYSINC	= $(NUSYSDIR)/include
NUSYSLIB	= $(NUSYSDIR)/lib
NUOBJ		= $(NUSYSLIB)/nusys.o


################################################################
#                       Compiler Settings                      #
################################################################

CC  = gcc
LD  = ld
MAKEROM = mild


################################################################
#                         Make Commands                        #
################################################################

default: $(TARGETS)


################################################################
#     Don't touch below unless you know what you're doing!     #
################################################################

include $(ROOT)/usr/include/make/PRdefs

CODESEGMENT = codesegment.o
OBJECTS		= $(CODESEGMENT)
APP         = $(TARGETS:.n64=.out)


################################################################
#                     ROM debug mode check                     #
################################################################

ifeq ($(DEBUG_MODE), 0)
    CODEOBJECTS     = $(CODECFILES:.c=.o) $(NUOBJ)
    OPTIMIZER       = -O2
    LCDEFS          = -D_FINALROM -DNDEBUG -DF3DEX_GBI_2 -DNOT_SPEC
    N64LIB          = -lnusys -lgultra_rom
    MAKEROMFLAGS    = 
else
    CODEOBJECTS     = $(CODECFILES:.c=.o) $(NUOBJ) $(DEBUGFILES:.c=.o)
    OPTIMIZER       = -g -O0
    LCDEFS          = -DDEBUG  -DF3DEX_GBI_2 -DNOT_SPEC
    N64LIB          = -lnusys_d -lgultra_d 
    MAKEROMFLAGS    = -d
endif


################################################################
#                        Linker Settings                       #
################################################################

LCINCS =	-I. -I$(ROOT)/usr/include/PR -I $(ROOT)/usr/include -I$(NUSYSINC)
LCOPTS =	-G 0
LDIRT  =	$(APP)
LDFLAGS=	$(MKDEPOPT) -L$(ROOT)/usr/lib -L$(ROOT)/usr/lib/PR -L$(NUSYSLIB) $(N64LIB) -L$(GCCDIR)/mipse/lib -lkmc -g


################################################################
#                          Compilation                         #
################################################################

include $(COMMONRULES)

$(CODESEGMENT): $(CODEOBJECTS)
    $(LD) -o $(CODESEGMENT) -r $(CODEOBJECTS) $(LDFLAGS)

$(TARGETS): $(OBJECTS)
    $(MAKEROM) spec $(MAKEROMFLAGS) -I$(NUSYSINC) -r $(TARGETS) -e $(APP) 
    @makemask $(TARGETS)
ifeq ($(ALLOWREGISTER), 1)
    @nrdc $(TARGETS) $(REGISTRATION)
endif
