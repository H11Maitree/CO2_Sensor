CC=gcc
LD=$(CC)

CFLAGS=-Wall -g
LDFLAGS= -lusb

PREFIX=/usr/local

PROG=dusbrelays_ctl

OBJS=$(patsubst %.c,%.o,$(wildcard *.c))

.PHONY : clean install

all: $(OBJS)
	$(LD) $(LDFLAGS) $(OBJS) -o $(PROG)

%.o: %.c %.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o $(PROG)

install:
	@echo "Install not done yet. Sorry"

