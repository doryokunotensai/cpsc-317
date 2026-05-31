#ifndef __PACKET_LIST_H__
#define __PACKET_LIST_H__

/*
 * This structure is used to manage a list of packets.  It can be used to
 * manage a retransmission list.  The packets are copied into the list (so
 * the original can be deleted if desired).
 */
typedef struct pktlist {
    struct pktlist *next;
    unsigned int seqNo;
    packet packet;
} pktlist;

/* Declarations for packet_list.c */
extern char *pktlistToString(pktlist *l);
extern void addPacket(pktlist **listptr, unsigned int seqNo, packet *pptr);
extern pktlist *findPacket(struct pktlist **listptr, unsigned int seqNo, int remove);
extern void freePacket(pktlist *list);

#endif
