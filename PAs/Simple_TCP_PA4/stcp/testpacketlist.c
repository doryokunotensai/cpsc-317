#include <assert.h>
#include <stdio.h>
#include <stdint.h>

#include "stcp.h"
#include "packet_list.h"

#define PACKETS_TO_ADD 7
#define INITIAL_SEQ 0xFFFFFFFF - 3
#define LENGTH 17

void init_data(unsigned char *data, uint32_t seq, uint32_t len) {
    char *values = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    int nvalues = strlen(values);
    int start = seq % nvalues;
    int nleft = min(nvalues - start, len);
    memcpy(data, (unsigned char *)&values[start], nleft);
    memcpy(data + nleft, (unsigned char *)values, len - nleft);
}

int main(int argc, char **argv) {
    pktlist *list = NULL;
    packet p;
    uint32_t seq = INITIAL_SEQ;
    uint32_t inc = LENGTH;
    unsigned char data[STCP_MSS];
    // Add a bunch of packets to the pktlist
    for (int i = 0; i < PACKETS_TO_ADD; i++) {
        init_data(data, seq, inc);

        createSegment(&p, ACK, 0, seq, 0, data, inc);
        addPacket(&list, seq, &p);
        seq += inc;
    }

    // Capture the string result
    char *original = pktlistToString(list);

    // Check that the right sequence numbers can be found
    for (int i = 0; i < PACKETS_TO_ADD * LENGTH; i++) {
        pktlist *l = findPacket(&list, INITIAL_SEQ + i, 0);
        if (i % LENGTH == 0) {
            assert(l != NULL);
            assert(l->packet.hdr->seqNo == plus32(INITIAL_SEQ, i));
        } else {
            assert(l == NULL);
        }
    }

    // Replace one
    seq = plus32(INITIAL_SEQ, LENGTH);
    init_data(data, plus32(seq, 2), inc);
    createSegment(&p, ACK, 0, seq, 0, data, inc);
    addPacket(&list, seq, &p);
    // Make sure the result changed
    char *changed = pktlistToString(list);
    assert(strcmp(original, changed) != 0);

    // If a file name argument is passed, make a list of all the segments from the file
    pktlist *filelist = NULL;
    seq = INITIAL_SEQ;
    if (argc > 1) {
        char *filename = argv[1];
        FILE *infile = fopen(filename, "rb");

        int nread = fread(data, 1, LENGTH, infile);
        while (nread > 0) {
            createSegment(&p, ACK, 0, seq, 0, data, nread);
            addPacket(&filelist, seq, &p);
            seq += nread;
            nread = fread(data, 1, LENGTH, infile);
        }
        printf("%s", pktlistToString(filelist));
    }
}
