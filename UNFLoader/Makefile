APP=UNFLoader
OS_NAME := $(shell uname -s)

PREFIX ?= /usr/local

CODEFILES = main.cpp helper.cpp term.cpp debug.cpp gdbstub.cpp
LIBFILES = Include/lodepng.cpp

CARTLIBNAME 	= flashcart
CARTLIBFILES	= device.cpp \
		  		  device_usb.cpp \
            	  device_64drive.cpp \
                  device_everdrive.cpp \
                  device_sc64.cpp

ifeq ($(DEBUG),1)
	CARTLIBNAME := $(CARTLIBNAME)_d
	CODEOBJECTS =	$(CODEFILES:.cpp=.od)
	LIBOBJECTS =	$(LIBFILES:.cpp=.od)
	CARTLIBOBJECTS 	= $(CARTLIBFILES:.cpp=.od)
else
	CODEOBJECTS =	$(CODEFILES:.cpp=.o)
	LIBOBJECTS =	$(LIBFILES:.cpp=.o)
	CARTLIBOBJECTS 	= $(CARTLIBFILES:.cpp=.o)
endif
CARTLIBNAME_STATIC	= $(CARTLIBNAME).a
CARTLIBNAME_SHARED	= $(CARTLIBNAME).so

CXX = g++

ifeq ($(OS_NAME),Darwin)
	DEPENDENCIES := -lncurses -lpthread $(DEPENDENCIESLIB)
else
	DEPENDENCIESLIB += -lftdi1 -lusb-1.0 -ludev
	DEPENDENCIESLIB_SHARED = -lftdi1 -lusb-1.0
	DEPENDENCIES := -lncursesw -lpthread -lrt $(DEPENDENCIESLIB)
endif

CFLAGS = -D LINUX -D_XOPEN_SOURCE_EXTENDED -Wall -Wno-unknown-pragmas -std=c++11 -I "$(PREFIX)/include"
ifeq ($(DEBUG),1)
	CFLAGS += -ggdb -O0 -g
else
	CFLAGS += -O3
endif


ifeq ($(OS_NAME),Darwin)
	slash:= /
	empty:=
	space:= $(empty) $(empty)
	HOMEBREW_PREFIX=$(shell brew --prefix)
	HOMEBREW_CELLAR=$(shell brew --cellar)
	LIBUSB_VER=$(subst $(space),$(slash),$(shell brew list libusb --versions| awk  '{print $2}'))
	LIBFTDI_VER=$(subst $(space),$(slash),$(shell brew list libftdi --versions| awk  '{print $2}'))
	CFLAGS += -I$(HOMEBREW_PREFIX)/include
	LINKER_OPTIONS += $(shell pkg-config --libs-only-other --static libftdi1) $(HOMEBREW_CELLAR)/$(LIBUSB_VER)/lib/libusb-1.0.a $(HOMEBREW_CELLAR)/$(LIBFTDI_VER)/lib/libftdi1.a
endif

default: $(APP)

$(APP): static $(CODEOBJECTS) $(LIBOBJECTS) 
	@echo "Linking $@"
	@$(CXX) $(CFLAGS) -o $(APP) $(CODEOBJECTS) $(CARTLIBOBJECTS) $(LIBOBJECTS) $(CARTLIBNAME_STATIC) $(LINKER_OPTIONS) -LInclude $(DEPENDENCIES) 

%.o: %.cpp
	@echo "Compiling $<"
	@$(CXX) -c $(CFLAGS) -o $@ $<

%.od: %.cpp
	@echo "Compiling (Debug) $<"
	@$(CXX) -c $(CFLAGS) -o $@ $<

static: $(CARTLIBOBJECTS) 
	@echo "Creating static library $(CARTLIBNAME_STATIC)"
	ar rcs $(CARTLIBNAME_STATIC) $^

shared: $(CARTLIBOBJECTS)
	@echo "Creating shared library $(CARTLIBNAME_SHARED)"
	@$(CXX) $(CFLAGS) $(CARTLIBFILES) -o $(CARTLIBNAME_SHARED) $(DEPENDENCIESLIB_SHARED) $(LINKER_OPTIONS) -fPIC -shared

clean:
	@echo "Cleaning built artifacts.."
	@rm -f $(APP) $(CODEOBJECTS) $(CARTLIBOBJECTS) $(LIBOBJECTS) $(CARTLIBNAME_STATIC) $(CARTLIBNAME_SHARED)

install: $(APP)
	@echo "Installing $(APP) to $(PREFIX)/bin"
	@mkdir -p "$(PREFIX)/bin"
	@cp "$(APP)" "$(PREFIX)/bin/$(APP)"

uninstall: $(APP)
	@echo "Removing $(APP) from $(PREFIX)/bin"
	@rm -f "$(PREFIX)/bin/$(APP)"
