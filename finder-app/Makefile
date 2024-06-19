CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall

writer: writer.o
	$(CC) $(CFLAGS) -o writer writer.o

writer.o: writer.c
	$(CC) $(CFLAGS) -o writer.o -c writer.c

clean:
	rm *.o writer

all: writer
