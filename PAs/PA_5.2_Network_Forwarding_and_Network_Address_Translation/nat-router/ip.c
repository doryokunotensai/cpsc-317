#include <arpa/inet.h>
#include <stdio.h>
#include "ip.h"

#define MAXLENGTH 128
#define NBUFFERS    2

// The header is assumed to be in network byte order
char *ipHdrToString(ipheader *hdr) {
    static char buff[NBUFFERS][MAXLENGTH];
    static int next = 0;
    char *buf = &buff[next][0];
    next = (next + 1) % NBUFFERS;
    uint32_t srcipaddr = hdr->srcipaddr, dstipaddr = hdr->dstipaddr;
    
    snprintf(buf, MAXLENGTH, "src: %3d.%3d.%3d.%3d dst: %3d.%3d.%3d.%3d ID: %4d CkSum: 0x%04x Len: %4d TTL: %3d Prot: 0x%02x",
	     (srcipaddr & 0xFF000000) >> 24,
	     (srcipaddr & 0x00FF0000) >> 16,
	     (srcipaddr & 0x0000FF00) >> 8,
	     (srcipaddr & 0x000000FF),
	     (dstipaddr & 0xFF000000) >> 24,
	     (dstipaddr & 0x00FF0000) >> 16,
	     (dstipaddr & 0x0000FF00) >> 8,
	     (dstipaddr & 0x000000FF),
             ntohs(hdr->id),
	     ntohs(hdr->checksum), ntohs(hdr->length), hdr->ttl, hdr->protocol);
    return buf;
}
	     
// ip is assumed to be in network byte order
char *ipToString(uint32_t ip) {
    static char buff[NBUFFERS][MAXLENGTH];
    static int next = 0;
    char *buf = &buff[next][0];
    next = (next + 1) % NBUFFERS;
    
    snprintf(buf, MAXLENGTH, "%3d.%3d.%3d.%3d",
	     (ip & 0xFF000000) >> 24,
             (ip & 0x00FF0000) >> 16,
             (ip & 0x0000FF00) >> 8,
             (ip & 0x000000FF));

    return buf;
}
	     
char *protocolToString(uint8_t protocol) {
    static char buff[NBUFFERS][MAXLENGTH];
    static int next = 0;
    char *buf = &buff[next][0];
    next = (next + 1) % NBUFFERS;
    if (protocol == PROT_UDP) {
        return "udp";
    } else if (protocol == PROT_TCP) {
        return "tcp";
    } else {
        snprintf(buf, MAXLENGTH, "%3d", protocol);
        return buf;
    }
}

void htonHdr(ipheader *hdr) {
    hdr->id = htons(hdr->id);
    hdr->length = htons(hdr->length);
}
    
// Compute Internet Checksum for "len" bytes beginning at location "data".
unsigned short ipchecksum(void *data, int len, void *data2, int len2) {
    register int sum = 0;

    while (len > 1)  {
	sum += * (unsigned short *) data;
	data += 2;
	len -= 2;
    }

    /*  Add left-over byte, if any */
    if (len > 0) {
	sum += * (unsigned char *) data;
    }
    while (len2 > 1)  {
	sum += * (unsigned short *) data2;
	data2 += 2;
	len2 -= 2;
    }

    /*  Add left-over byte, if any */
    if (len2 > 0) {
	sum += * (unsigned char *) data2;
    }
    /*  Fold 32-bit sum to 16 bits */
    while (sum >> 16) {
	sum = (sum & 0xffff) + (sum >> 16);
    }
    return ~sum;
}

