#ifndef __UDP_H__
#define __UDP_H__

typedef struct udpheader {
    unsigned short sport;    /* source port */
    unsigned short dport;    /* destination port */
    unsigned short len;      /* segment length, including the header length */
    unsigned short sum;      /* checksum */
} udpheader;

extern char *udpHdrToString(udpheader *hdr);
extern void htonUdpHdr(udpheader *hdr);
#endif
