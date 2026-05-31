/*
 * A list of packets.
 * 
 * Version 1.0
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "stcp.h"
#include "packet_list.h"

#define NBUFFERS      7
#define BUFFERLEN  4096

char *pktlistToString(pktlist *list) {
    static char buffer[NBUFFERS][BUFFERLEN];
    static int next = 0;
    char *ans = &buffer[next][0], *writeto = ans;
    next = (next + 1) % NBUFFERS;
    
    while (list != NULL) {
	int wrote = snprintf(writeto, BUFFERLEN - (writeto - ans), "[%08x-%08x] %.*s\n", list->seqNo, plus32(list->seqNo, list->packet.len - sizeof(tcpheader) - 1), (int)(list->packet.len - sizeof(tcpheader)), list->packet.data + sizeof(tcpheader));
        writeto += wrote;
	list = list->next;
    }
    return ans;
}

/* 
 * Adds a packet to the list in sequence number order.
 * If a packet with the given sequence number is already in the list, then
 * overwrite that list element.
 */
void addPacket(pktlist **listptr, unsigned int seqNo, packet *pptr) {
    pktlist *new;
  
    new = (pktlist *)malloc(sizeof(pktlist));
    logLog("alloc", "New is %p", new);
    new->seqNo = seqNo;
    new->packet = *pptr;
    new->packet.hdr = (tcpheader *)new->packet.data;
  
    while (*listptr) {
	if (greater32((*listptr)->seqNo, new->seqNo)) {
	    // It goes before *listptr
	    new->next = *listptr;
	    *listptr = new;
	    return;
	} else if ((*listptr)-> seqNo == new->seqNo) {
	    // It is an update, replace the old one
	    pktlist *old = *listptr;
	    new->next = old->next;
	    *listptr = new;
	    logLog("alloc", "Add packet freeing %p", old);
	    free(old);
	    return;
	}
	listptr = &(*listptr)->next;
    }
    // If we get here, then the packet goes at the end of the list
    new->next = NULL;
    *listptr = new;
}

void freePacket(pktlist *pbuf) {
    if (pbuf->next != NULL) {
        fprintf(stderr, "Attempt to free a pktlist entry with non null next pointer %p\n", pbuf);
        abort();
    }
    free(pbuf);
}

/*
 * Returns a packet whose sequence number equals to seqNo.
 * If no such packet, returns NULL.
 *
 */
pktlist *findPacket(pktlist **listptr, unsigned int seqNo, int remove) {
    while (*listptr) {
	pktlist *curr = *listptr;
	if (curr->seqNo == seqNo) {
	    if (remove) {
		// Unlink the node from the list 
		*listptr = curr->next;
                curr->next = NULL;
	    }
	    return curr;
	}
	listptr = &curr->next;
    }
    return NULL;
}
