CC=gcc
CCOPTS=--std=c89 -Wall 

HEADERS=fat.h

BINS=fat_test

.phony: clean all

%.o:	%.c $(HEADERS)
	$(CC) $(CCOPTS) -c -o $@  $<

all:	 $(BINS)

fat_test:		main.o 
	$(CC) $(CCOPTS) -o $@ $^

clean:
	rm -rf *.o *~ $(BINS)
