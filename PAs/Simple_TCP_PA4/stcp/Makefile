#
# This code is adapted from a programming course at 
# Boston University for use in CPSC 317 at UBC
#
#
# Version 1.0
#


CC     = gcc
CFLAGS = -g -Wall

all:	sender testpacketlist testtcp testwraparound waitForPorts 
	bash ./runnoerrors.sh

test:	testpacketlist testtcp testwraparound
	./testpacketlist
	./testpacketlist tcp.h
	./testtcp
	./testwraparound

sender: sender.o stcp.o wraparound.o tcp.o log.o packet_list.o
	$(CC) -o $@ $(CFLAGS) $^

wraparound.o: stcp.h wraparound.c
	$(CC) -c -o $@ $(CFLAGS) wraparound.c

stcp.o: stcp.h stcp.c
	$(CC) -c -o $@ $(CFLAGS) stcp.c

waitForPorts:	waitForPorts.o
	$(CC) -o $@ $(CFLAGS) $^

testwraparound: testwraparound.o wraparound.o
	$(CC) -o $@ $(CFLAGS) $^

testtcp: testtcp.o tcp.o
	$(CC) -o $@ $(CFLAGS) $^

testpacketlist: testpacketlist.o packet_list.o log.o wraparound.o stcp.o tcp.o
	$(CC) -o $@ $(CFLAGS) $^

clean:
	-rm -f *.o sender testpacketlist testtcp testwraparound waitForPorts OutputFile
