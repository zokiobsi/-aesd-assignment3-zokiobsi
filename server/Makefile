#TODO check CC is defined to overwrite, add CROSS_COMPILE arguement 

COMPILER = $(CC)

all: aesdsocket.o
	$(COMPILER) aesdsocket.o -o aesdsocket
	chmod +x aesdsocket

aesdsocket.o: aesdsocket.c
	$(COMPILER) -c aesdsocket.c

clean:
	rm -f aesdsocket.o aesdsocket
