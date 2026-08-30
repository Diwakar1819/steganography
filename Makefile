CC = gcc

CFLAGS = -Wall -Wextra -Werror -std=c11 -Iinclude

TARGET = steg

SRC = src/main.c src/bmp.c src/lsb.c src/payload.c src/crc32.c src/encoder.c src/decoder.c

OBJ = $(SRC:.c=.o)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)
