# Makefile for relay_handler

CC = gcc
CFLAGS = -Wall -O2
LDFLAGS = -lmosquitto -lhiredis

TARGET = persis_session
SRC = persis_session.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)