CC=c99
CFLAGS=-g3 -Wall

all: disk

disk: disk.o
	$(CC) $(CFLAGS) -o $@ $^

test: disk
	./disk

disk.o: disk.c disk.h

clean:
	$(RM) -f disk *.o