CC=gcc
CFLAGS= -Wall -pedantic -std=gnu99
LFLAGS= -I/local/courses/csse2310/include -L/local/courses/csse2310/lib -lcsse2310a3 
.PHONY: all clean
.DEFAULT_GOAL := all
all: sigcat hq


sigcat: sigcat.c
	$(CC) $(CFLAGS) $(LFLAGS) $^ -o $@ -g

hq: hq.c
	$(CC) $(CFLAGS) $(LFLAGS) $^ -o $@ -g

clean:
	rm sigcat *.c
	rm hq *.c


