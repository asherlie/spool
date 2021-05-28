CC=gcc

CFLAGS=-Wall -Wextra -Wpedantic -Werror -O3 -pthread

all: ex

sp.o: sp.c sp.h
ex: test.c sp.o
	$(CC) $(CFLAGS) test.c sp.o -o ex

.PHONY:
clean:
	rm -f ex *.o
