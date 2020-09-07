APP=UNFLoader

CODEFILES=main.cpp helper.cpp device.cpp debug.cpp \
	device_64drive.cpp \
	device_everdrive7.cpp \
	device_everdrive3.cpp
LIBFILES=Include/lodepng.c

CC=g++
DEPENDENCIES := -lncurses -lftd2xx -lpthread -lrt
LINKER_OPTIONS := -Wl,-rpath /usr/local/lib
CFLAGS=-D LINUX

default:
	$(CC) $(CFLAGS) -o $(APP) $(CODEFILES) $(LIBFILES) $(DEPENDENCIES) $(LINKER_OPTIONS) -L/usr/local/lib
