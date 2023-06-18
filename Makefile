CC=gcc
CCOPTS= -Wall 

HEADERS=fat.h

BINS=fat_test

.phony: clean all

%.o:	%.c $(HEADERS)
	$(CC) $(CCOPTS) -c -o $@  $<

all:	 $(BINS)

fat_test:		main.o fat.o
	$(CC) $(CCOPTS) -o $@ $^

clean:
	rm -rf *.o *~ $(BINS)
