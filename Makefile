CC = gcc

CFLAGS = -Wall -Wextra -Werror -std=c11 -Iinclude

TARGET = steg

SRC = src/main.c

OBJ = $(SRC:.c=.o)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET)

clean:
	rm -f $(OBJ) $(TARGET)
