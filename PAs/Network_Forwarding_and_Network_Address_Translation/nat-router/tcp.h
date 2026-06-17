#ifndef __TCP_H__
#define __TCP_H__
typedef struct tcpheader {
    unsigned short sport;    /* source port */
    unsigned short dport;    /* destination port */
    unsigned int   seq;      /* sequence number */
    unsigned int   ack;      /* acknowledgement number */
    unsigned char  off;      /* data offset - must be 5 */
    unsigned char  flags;    /* flags */
    unsigned short win;      /* window size - not used */
    unsigned short sum;      /* checksum */
    unsigned short urp;      /* urgent pointer - not used */
} tcpheader;

typedef enum tcpflags {
    FIN = 0b000000001,
    SYN = 0b000000010,
    RST = 0b000000100,
    ACK = 0b000010000
} tcpflags;
    
static inline void setFin(tcpheader *hdr) { hdr->flags |= FIN; }
static inline void setSyn(tcpheader *hdr) { hdr->flags |= SYN; }
static inline void setRst(tcpheader *hdr) { hdr->flags |= RST; }
static inline void setAck(tcpheader *hdr) { hdr->flags |= ACK; }
static inline int getFin(tcpheader *hdr) { return (hdr->flags & FIN) >> 0; }
static inline int getSyn(tcpheader *hdr) { return (hdr->flags & SYN) >> 1; }
static inline int getRst(tcpheader *hdr) { return (hdr->flags & RST) >> 2; }
static inline int getAck(tcpheader *hdr) { return (hdr->flags & ACK) >> 4; }

extern char *tcpHdrToString(tcpheader *hdr);
extern void htonTcpHdr(tcpheader *hdr);

// The srcip and dstip fields are used in the construction of the IP pseudo header.
// data should be a pointer to the TCP header with the payload following it.
// len should be the length of the TCP segment (including the length of the TCP header).
extern uint16_t tcpchecksum(uint32_t srcip, uint32_t dstip, void *data, int len);
#endif
