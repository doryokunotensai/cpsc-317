#include <arpa/inet.h>
#include <stdio.h>
#include "ip.h"
#include "tcp.h"

#define MAXLENGTH 128
#define NBUFFERS    2

// The hdr must be in network byte order
char *tcpHdrToString(tcpheader *hdr) {
    static char buff[NBUFFERS][MAXLENGTH];
    static int next = 0;
    char *buf = &buff[next][0];
    next = (next + 1) % NBUFFERS;
    
    snprintf(buf, MAXLENGTH, "srcport: %5d dstport: %5d CkSum: 0x%04x",
	     ntohs(hdr->sport),
	     ntohs(hdr->dport),
             ntohs(hdr->sum));
    return buf;
}
	     
void htonTcpHdr(tcpheader *hdr) {
    hdr->sport = htons(hdr->sport);
    hdr->dport = htons(hdr->dport);
    hdr->seq = htonl(hdr->seq);
    hdr->ack = htonl(hdr->ack);
    hdr->win = htons(hdr->win);
    hdr->sum = htons(hdr->sum);
    hdr->urp = htons(hdr->urp);
}
    
typedef struct ippseudoheader {
    uint32_t srcip;
    uint32_t dstip;
    uint8_t zero;
    uint8_t protocol;
    uint16_t len;
} ippseudoheader;

void htonTcpPHdr(ippseudoheader *phdr) {
    phdr->len = htons(phdr->len);
}

// The srcip and dstip fields are used in the construction of the IP pseudo header.
// data should be a pointer to the TCP header with the payload following it.
// len should be the length of the TCP segment (including the length of the TCP header).
uint16_t tcpchecksum(uint32_t srcip, uint32_t dstip, void *data, int len) {
    ippseudoheader phdr;
    phdr.srcip = srcip;
    phdr.dstip = dstip;
    phdr.zero = 0;
    phdr.protocol = 6;
    phdr.len = len;
    htonTcpPHdr(&phdr);
    return ipchecksum(&phdr, sizeof(phdr), data, len);
}
