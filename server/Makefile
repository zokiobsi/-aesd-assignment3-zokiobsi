CC ?= gcc
CFLAGS ?=
LDFLAGS ?=

all: aesdsocket.o
	$(CC) $(CFLAGS) aesdsocket.o -o aesdsocket $(LDFLAGS)
	chmod +x aesdsocket

aesdsocket.o: aesdsocket.c
	$(CC) $(CFLAGS) -c aesdsocket.c

clean:
	rm -f aesdsocket.o aesdsocket
