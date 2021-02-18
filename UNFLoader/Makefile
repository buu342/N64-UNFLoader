APP=UNFLoader
OS_NAME := $(shell uname -s)

CODEFILES=main.cpp helper.cpp device.cpp debug.cpp \
	device_64drive.cpp \
	device_everdrive.cpp \
	device_sc64.cpp
LIBFILES=Include/lodepng.cpp

CC=g++

ifeq ($(OS_NAME),Darwin)
	DEPENDENCIES := -lncurses -lftd2xx -lpthread
else
	DEPENDENCIES := -lncurses -lftd2xx -lpthread -lrt 
endif

LINKER_OPTIONS := -Wl,-rpath /usr/local/lib
CFLAGS=-D LINUX -D_XOPEN_SOURCE_EXTENDED

default:
	$(CC) $(CFLAGS) -o $(APP) $(CODEFILES) $(LIBFILES) $(DEPENDENCIES) $(LINKER_OPTIONS) -L/usr/local/lib
