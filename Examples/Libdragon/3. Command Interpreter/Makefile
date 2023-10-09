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

ASSETS_PNG = $(wildcard assets/*.png)

ASSETS_CONV = $(addprefix filesystem/,$(notdir $(ASSETS_PNG:%.png=%.sprite)))

MKSPRITE_FLAGS ?= --compress 0


################################################################
#                         Make Commands                        #
################################################################

all: unflexa3.z64

filesystem/%.sprite: assets/%.png
	@mkdir -p $(dir $@)
	@echo "    [SPRITE] $@"
	@$(N64_MKSPRITE) $(MKSPRITE_FLAGS) -o filesystem "$<"


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

$(BUILD_DIR)/unflexa3.dfs: $(ASSETS_CONV)
$(BUILD_DIR)/unflexa3.elf: $(SRC:%.c=$(BUILD_DIR)/%.o)

unflexa3.z64: N64_ROM_TITLE="UNFLEXAM3"
unflexa3.z64: $(BUILD_DIR)/unflexa3.dfs 

clean:
	rm -rf $(BUILD_DIR) unflexa3.n64

-include $(wildcard $(BUILD_DIR)/*.d)

.PHONY: all clean