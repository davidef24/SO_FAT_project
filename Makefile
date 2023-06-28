CC=gcc
CCOPTS= -Wall 

HEADERS=fat.h

BINS=fat_test

.phony: clean all

all:	 $(BINS)

fat_test:		main.c fat.c
	$(CC) $(CCOPTS) -o $@ $^

clean:
	rm -rf *.o *~ $(BINS)
