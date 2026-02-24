COMPILER = $(CC)

all: writer.o
	$(COMPILER) writer.o -o writer
	chmod +x writer

writer.o: writer.c
	$(COMPILER) -c writer.c

clean:
	rm -f writer.o writer
