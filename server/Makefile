TARGET = aesdsocket
SRC = aesdsocket.c

CC ?= $(CC)
CFLAGS ?= -Wall -Werror -pthread
LDFLAGS ?= -pthread

all:
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

