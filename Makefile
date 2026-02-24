CC = gcc
CFLAGS = -Wall -o

all:
	$(CC) $(CFLAGS) security_daemon server.c

clean:
	rm -f security_daemon

