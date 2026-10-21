# ps5-shellui-lock - PC test build

EXT_DIR = ./ext

# Sqlite3
SQLITE_ZIP_URL = https://www.sqlite.org/2026/sqlite-amalgamation-3540100.zip
SQLITE_ZIP_FILE = $(EXT_DIR)/sqlite-amalgamation.zip
SQLITE_TARGETS = $(EXT_DIR)/sqlite3.c $(EXT_DIR)/sqlite3.h

# kqueue
KQUEUE_URL = https://github.com/mheily/libkqueue/archive/refs/tags/v2.7.0.zip
KQUEUE_ZIP_FILE = $(EXT_DIR)/kqueue-v2.7.0.zip
KQUEUE_SRC_DIR = $(EXT_DIR)/libkqueue-2.7.0
KQUEUE_BUILD_DIR = $(KQUEUE_SRC_DIR)/build

# Tools
CC     := /usr/bin/clang-18

# Paths
INCLUDES := -Iinclude -I$(EXT_DIR) -I$(KQUEUE_SRC_DIR)/include
LIBS     := $(KQUEUE_BUILD_DIR)/libkqueue.a  -lpthread

# Source Files
SRCS := src/main.c \
				src/util_linux.c \
			  $(EXT_DIR)/sqlite3.c
OBJS := $(SRCS:.c=.o)
ELF  := ps5-shellui-lock

TEST_SRCS := src/test_linux.c \
					   $(EXT_DIR)/sqlite3.c
TEST_OBJS := $(TEST_SRCS:.c=.o)
TEST  		:= test

# Compiler Flags
#CFLAGS := -Os -Wall -ffunction-sections -fdata-sections $(INCLUDES)
CFLAGS := -g $(INCLUDES)
LDFLAGS := -Wl,--gc-sections

$(SQLITE_ZIP_FILE):
	@echo "Creating directory $(EXT_DIR)..."
	mkdir -p $(EXT_DIR)
	@echo "Downloading SQLite amalgamation zip..."
	curl -L $(SQLITE_ZIP_URL) -o $(SQLITE_ZIP_FILE)

$(KQUEUE_ZIP_FILE):
	@echo "Creating directory $(EXT_DIR)..."
	mkdir -p $(EXT_DIR)
	@echo "Downloading $(KQUEUE_ZIP_FILE)..."
	curl -L $(KQUEUE_URL) -o $(KQUEUE_ZIP_FILE)

# Rule to extract files: only runs if sqlite3 files are missing or the zip is newer
$(SQLITE_TARGETS): $(SQLITE_ZIP_FILE)
	@echo "Extracting sqlite3.* files from $(SQLITE_ZIP_FILE) to $(EXT_DIR)..."
	unzip -j -o $(SQLITE_ZIP_FILE) "*/sqlite3.*" -d $(EXT_DIR)
	@touch $(SQLITE_TARGETS)
	@echo "Sqlite3 extraction complete."

$(KQUEUE_SRC_DIR)/CMakeLists.txt: $(KQUEUE_ZIP_FILE)
	@echo "Extracting $(KQUEUE_ZIP_FILE)..."
	unzip -q -o $(KQUEUE_ZIP_FILE) -d $(EXT_DIR)
	touch $(KQUEUE_SRC_DIR)/CMakeLists.txt
	@echo "libkqueue extraction complete."

.PHONY: download-sqlite
download-sqlite: $(SQLITE_TARGETS)

build-libkqueue: $(KQUEUE_SRC_DIR)/CMakeLists.txt
	@echo "Building libkqueue..."
	cmake -B $(KQUEUE_BUILD_DIR) -S $(KQUEUE_SRC_DIR)
	cmake --build $(KQUEUE_BUILD_DIR)

$(ELF): download-sqlite build-libkqueue $(SRCS)
	@echo "Building $(ELF)..."
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(ELF) $(SRCS) $(LIBS)

$(TEST): download-sqlite $(TEST_SRCS)
	@echo "Building $(TEST)..."
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TEST) $(TEST_SRCS) $(LIBS)

all: $(ELF) $(TEST)

clean:
	rm -rf $(EXT_DIR) $(ELF) $(TEST) src/*.o

.PHONY: all clean
