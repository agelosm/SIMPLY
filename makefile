CC = gcc
CFLAGS = -g -O2

all: executable simplyrun pi matmul

simplyrun: simplyrun.c simply.o
	$(CC) $(CFLAGS) -o simplyrun simplyrun.c simply.o -lpthread

executable: executable.c simply.o
	$(CC) $(CFLAGS)  -o executable executable.c simply.o -lpthread

pi: simply_pi.c simply.o
	$(CC) $(CFLAGS)  -o pi simply_pi.c simply.o -lpthread

matmul: matmul_concurrent.c simply.o
	$(CC) $(CFLAGS) -o matmul matmul_concurrent.c simply.o -lpthread

%.o : %.c simply.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -rf *.o *~ simplyrun executable
