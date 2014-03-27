TARGET=my_dns_client
SRC=my_dns_client.c
CC=gcc
CFLAGS=-g -Wall

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -o $(TARGET) $(SRC) $(CFLAGS)
	
clean:
	rm -rf $(TARGET)