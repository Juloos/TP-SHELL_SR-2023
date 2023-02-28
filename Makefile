.PHONY: all, clean, test

# Disable implicit rules
.SUFFIXES:

CC=gcc
CFLAGS=-Wall -g
VPATH=src/

# Note: -lnsl does not seem to work on Mac OS but will
# probably be necessary on Solaris for linking network-related functions 
#LIBS += -lsocket -lnsl -lrt
LIBS+=-lpthread

INCLUDE = readcmd.h csapp.h shell_commands.h jobs.h
OBJS = readcmd.o csapp.o shell_commands.o jobs.o
INCLDIR = -I.

all: shell

%.o: %.c $(INCLUDE)
	$(CC) $(CFLAGS) $(INCLDIR) -c -o $@ $<

%: %.o $(OBJS)
	$(CC) -o $@ $(LDFLAGS) $^ $(LIBS)

clean:
	rm -f shell *.o tests/*.log


test: all
	bash tests/run_tests.sh
