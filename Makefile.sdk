# ps5-shellui-lock - Native PS5 ELF Daemon Makefile

# Tools
PYTHON := python3
HOST_CC ?= clang
CC     := /opt/ps5-payload-sdk/bin/prospero-clang
STRIP  := /opt/ps5-payload-sdk/bin/prospero-strip

# Paths
SDK      := /opt/ps5-payload-sdk
TARGET   := $(SDK)/target
INCLUDES := -Iinclude -I$(TARGET)/include
LIBS     := -L$(TARGET)/lib -lpthread \
            -lSceUserService -lSceSystemService

# Source Files
SRCS := src/main.c $(TARGET)/src/sqlite3.c
OBJS := $(SRCS:.c=.o)
ELF  := ps5-shellui-lock.elf

# Compiler Flags
CFLAGS := -Os -Wall -ffunction-sections -fdata-sections $(INCLUDES)
LDFLAGS := -Wl,--gc-sections

all: $(ELF)

$(ELF): $(SRCS)
	@echo "Building $(ELF)..."
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(ELF) $(SRCS) $(LIBS)
	@echo "Stripping $(ELF)..."
	$(STRIP) $(ELF)

clean:
	rm -f $(ELF) src/*.o $(TARGET)/src/*.o

.PHONY: all clean
