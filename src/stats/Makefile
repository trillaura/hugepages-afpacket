CC=gcc
CFLAGS=-O2 -Wall -Wextra
CFILES:=$(shell ls *.c 2>/dev/null)
TARGET=stats
LIBS=-lm

all: $(CFILES)
	$(CC) -o $(TARGET) $^ $(CFLAGS) $(LIBS)


.PHONY: clean
clean:
	rm -f *.o *~ $(TARGET)
