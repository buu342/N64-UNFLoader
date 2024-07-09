APP=UNFLoader
OS_NAME := $(shell uname -s)

CODEFILES = main.cpp helper.cpp term.cpp debug.cpp gdbstub.cpp
CODEOBJECTS =	$(CODEFILES:.cpp=.o)
LIBFILES = Include/lodepng.cpp
LIBOBJECTS =	$(LIBFILES:.cpp=.o)

CARTLIBNAME 	= flashcart
CARTLIBFILES	= device.cpp \
		  device_usb.cpp \
            	  device_64drive.cpp \
                  device_everdrive.cpp \
                  device_sc64.cpp
CARTLIBOBJECTS 	= $(CARTLIBFILES:.cpp=.o)
CARTLIBNAME_STATIC	= $(CARTLIBNAME).a
CARTLIBNAME_SHARED	= $(CARTLIBNAME).so

CXX = g++

ifeq ($(OS_NAME),Darwin)
	DEPENDENCIESLIB := -l:libftdi1.a -l:libusb-1.0.a -ludev
else
	DEPENDENCIESLIB_SHARED = -lftdi1 -lusb-1.0
	DEPENDENCIESLIB += -l:libftdi1.a -l:libusb-1.0.a -ludev
	DEPENDENCIES := -lncursesw -lpthread -lrt $(DEPENDENCIESLIB)
endif

LINKER_OPTIONS := -Wl,-rpath /usr/local/lib -L/usr/local/lib
CFLAGS = -D LINUX -D_XOPEN_SOURCE_EXTENDED -Wall -Wno-unknown-pragmas -O3 -std=c++11 -I /usr/local/include

ifeq ($(OS_NAME),Darwin)
	CFLAGS += -I/opt/homebrew/include
	LINKER_OPTIONS += $(shell pkg-config --libs-only-other --static libftdi1) /opt/homebrew/Cellar/libusb/1.0.27/lib/libusb-1.0.a /opt/homebrew/Cellar/libftdi/1.5_2/lib/libftdi1.a
endif

default: $(APP)

$(APP): static $(CODEOBJECTS) $(LIBOBJECTS) 
	@echo "Linking $@"
	@$(CXX) $(CFLAGS) -o $(APP) $(CODEOBJECTS) $(CARTLIBOBJECTS) $(LIBOBJECTS) $(CARTLIBNAME_STATIC) $(LINKER_OPTIONS) -LInclude $(DEPENDENCIES) 

%.o: %.cpp
	@echo "Compiling $<"
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
	@echo "Installing $(APP) to /usr/local/bin"
	@mkdir -p /usr/local/bin
	@cp $(APP) /usr/local/bin/$(APP)

uninstall: $(APP)
	@echo "Removing $(APP) from /usr/local/bin"
	@rm -f /usr/local/bin/$(APP)
