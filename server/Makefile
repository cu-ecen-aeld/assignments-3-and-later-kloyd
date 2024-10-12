CC = $(CROSS_COMPILE)gcc
CFLAGS = -g -Wall -Werror

aesdsocket: aesdsocket.o
	$(CC) $(CFLAGS) -o aesdsocket aesdsocket.o

aesdsocket.o: aesdsocket.c
	$(CC) $(CFLAGS) -o aesdsocket.o -c aesdsocket.c

clean:
	rm -f *.o aesdsocket

all: aesdsocket
