# Use compiler provided by build system (Yocto/Buildroot)
CC ?= gcc

CFLAGS ?= -Wall -Werror -pthread
LDFLAGS ?=

TARGET = aesdsocket
SRC = aesdsocket.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

