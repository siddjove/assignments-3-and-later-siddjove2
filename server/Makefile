TARGET = aesdsocket
SRC = aesdsocket.c

CFLAGS += -Wall -Werror -pthread

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)

