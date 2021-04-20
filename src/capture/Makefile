CC=gcc
CFLAGS=-O2 -Wall -Wextra
CFILES:=$(shell ls *.c 2>/dev/null)
CCFILES:=$(shell find ../utils -name '*.c')
TARGET=capture
LIBS=

all: $(CFILES) $(CCFILES)
	$(CC) -I../utils -I. -o $(TARGET) $^ $(CFLAGS) $(LIBS)

.PHONY: clean
clean:
	rm -f *.o *~ $(TARGET)
