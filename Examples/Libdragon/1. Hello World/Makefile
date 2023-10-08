BUILD_DIR=build
include $(N64_INST)/include/n64.mk

################################################################
#                    Code files and ROM name                   #
################################################################

SRC = main.c

DEBUG_MODE = 1 # Remember to change debug.h as well to match!
DEBUGFILES = debug.c


################################################################
#     Don't touch below unless you know what you're doing!     #
################################################################


################################################################
#                         Make Commands                        #
################################################################

all: unflexa1.z64


################################################################
#                     ROM debug mode check                     #
################################################################

ifeq ($(DEBUG_MODE), 0)
N64_CFLAGS += -O2
else
SRC += $(DEBUGFILES)
N64_CFLAGS += -g -O0
N64_LDFLAGS += -g
endif


################################################################
#                          Compilation                         #
################################################################

$(BUILD_DIR)/unflexa1.elf: $(SRC:%.c=$(BUILD_DIR)/%.o)

unflexa1.z64: N64_ROM_TITLE="UNFLEXAM1"

clean:
	rm -rf $(BUILD_DIR) unflexa1.z64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean