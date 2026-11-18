
CC = gcc

CFLAGS = -Wall -Wextra -O2

LIBS = $(shell pkg-config --libs libcurl libcjson)

CFLAGS += $(shell pkg-config --cflags libcurl libcjson)

TARGET = stock-briefing

all: $(TARGET)

$(TARGET): src/main.c
	$(CC) $(CFLAGS) -o $(TARGET) src/main.c $(LIBS)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all run clean
