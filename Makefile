# Folders
#BINDIR  = bin
SRCDIR  = src
OBJDIR  = obj

# Files
MAIN.C = main.c
MAIN.O = main.o

SERVER_PATH=server
CLIENT_PATH=client
UTILITY_PATH=utility

# Generals
CC     = gcc
TARGET_CLIENT = mytftp_client
TARGET_SERVER = mytftp_server

# Flags (disable check to ensure success)
# CFLAGS   = -std=gnu11 -Wall -pedantic -O3
# CFLAGS  += -g
# CFLAGS  += -fsanitize=address -fno-omit-frame-pointer
# LDFLAGS += -fsanitize=address

CFLAGS = -DRANDOM=0

# Targets
.PHONY: default
.DEFAULT_GOAL : default

default: clean target

clean:
	rm -rf $(OBJDIR)
	rm -f mytftp_client
	rm -f mytftp_server


$(OBJDIR)/utils.o: $(SRCDIR)/$(UTILITY_PATH)/utils.c
	mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/$(SERVER_PATH)/$(MAIN.O): $(SRCDIR)/$(SERVER_PATH)/$(MAIN.C)
	mkdir -p $(OBJDIR)/$(SERVER_PATH)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR)/$(CLIENT_PATH)/$(MAIN.O): $(SRCDIR)/$(CLIENT_PATH)/$(MAIN.C)
	mkdir -p $(OBJDIR)/$(CLIENT_PATH)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET_SERVER): $(OBJDIR)/$(SERVER_PATH)/$(MAIN.O) $(OBJDIR)/utils.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

$(TARGET_CLIENT): $(OBJDIR)/$(CLIENT_PATH)/$(MAIN.O) $(OBJDIR)/utils.o
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o $@

target: $(TARGET_SERVER) $(TARGET_CLIENT)

test: target


run: target
	$(TARGET_SERVER); $(TARGET_CLIENT)
