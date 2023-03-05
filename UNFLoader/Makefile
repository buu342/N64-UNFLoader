APP=UNFLoader
OS_NAME := $(shell uname -s)

CODEFILES = main.cpp helper.cpp
CODEOBJECTS =	$(CODEFILES:.cpp=.o)
LIBFILES = Include/lodepng.cpp
LIBOBJECTS =	$(LIBFILES:.cpp=.o)

CXX=g++

ifeq ($(OS_NAME),Darwin)
	DEPENDENCIES := -lncurses -lftd2xx -lpthread
else
	DEPENDENCIES := -lncursesw -lftd2xx -lpthread -lrt 
endif

LINKER_OPTIONS := -Wl,-rpath /usr/local/lib
CFLAGS=-D LINUX -D_XOPEN_SOURCE_EXTENDED

default: $(APP)

$(APP): $(CODEOBJECTS) $(LIBOBJECTS)
	@echo "Linking $@"
	@$(CXX) $(CFLAGS) -o $(APP) $(CODEOBJECTS) $(LIBOBJECTS) $(DEPENDENCIES) $(LINKER_OPTIONS) -L/usr/local/lib

%.o: %.cpp
	@echo "Compiling $<"
	@$(CXX) -c $(CFLAGS) -o $@ $<

clean:
	@echo "Cleaning built artifacts.."
	@rm -f $(APP) $(CODEOBJECTS) $(LIBOBJECTS)

install: $(APP)
	@echo "Installing $(APP) to /usr/local/bin"
	@mkdir -p /usr/local/bin
	@cp $(APP) /usr/local/bin/$(APP)

uninstall: $(APP)
	@echo "Removing $(APP) from /usr/local/bin"
	@rm -f /usr/local/bin/$(APP)