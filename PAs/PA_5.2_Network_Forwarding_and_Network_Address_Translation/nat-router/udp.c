#include <arpa/inet.h>
#include <stdio.h>
#include "ip.h"
#include "udp.h"

#define MAXLENGTH 128
#define NBUFFERS    2

// The hdr must be in network byte order
char *udpHdrToString(udpheader *hdr) {
    static char buff[NBUFFERS][MAXLENGTH];
    static int next = 0;
    char *buf = &buff[next][0];
    next = (next + 1) % NBUFFERS;
    
    snprintf(buf, MAXLENGTH, "srcport: %5d dstport: %5d CkSum: 0x%04x Len: %4d",
	     ntohs(hdr->sport),
	     ntohs(hdr->dport),
             ntohs(hdr->sum),
             ntohs(hdr->len));
    return buf;
}
	     
void htonUdpHdr(udpheader *hdr) {
    hdr->sport = htons(hdr->sport);
    hdr->dport = htons(hdr->dport);
    hdr->sum = htons(hdr->sum);
    hdr->len = htons(hdr->len);
}
    
