#ifndef __IP_H__
#define __IP_H__

#ifndef __LOG_H__
#include "log.h"
#endif // __LOG_H__

#include <arpa/inet.h>

#define IP_MAX_DATAGRAM_LENGTH 512

typedef struct ipheader {
    unsigned char verslen;	// version in first 4 bits (== 4), len in second 4 bits (== 5) (header length in 32 bit words)
    unsigned char typeOfService;// Not used, must be 0
    unsigned short length;	// Length in bytes of the entire datagram
    unsigned short id;		// Not used except by the tester
    unsigned short flagsoffset;	// Not used, must be 0
    unsigned char ttl;		// Time to live in hops
    unsigned char protocol;	// Upper level protocol.  May be any value
    unsigned short checksum;	// Checksum of the IP header
    unsigned int srcipaddr;	// Src IP address
    unsigned int dstipaddr;	// Dst IP address
} ipheader;

typedef struct packet {
    char data[IP_MAX_DATAGRAM_LENGTH];
    ipheader *hdr;
    int len;
} packet;

#define PROT_UDP 17
#define PROT_TCP 6

extern unsigned short ipchecksum(void *data, int len, void *data2, int len2);
extern char *ipHdrToString(ipheader *hdr);
extern char *ipToString(uint32_t ip);
extern char *protocolToString(uint8_t protocol);
extern void htonHdr(ipheader *hdr);

#endif
