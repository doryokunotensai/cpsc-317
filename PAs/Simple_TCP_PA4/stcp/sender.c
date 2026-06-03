/************************************************************************
 * Adapted from a course at Boston University for use in CPSC 317 at UBC
 *
 *
 * The interfaces for the STCP sender (you get to implement them), and a
 * simple application-level routine to drive the sender.
 *
 * This routine reads the data to be transferred over the connection
 * from a file specified and invokes the STCP send functionality to
 * deliver the packets as an ordered sequence of datagrams.
 *
 * Version 2.0
 *
 *
 *************************************************************************/


// Implemented by Dikpaal Patel 37647864 in May/June 2026


#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/file.h>

#include "stcp.h"
#include "packet_list.h"

#define STCP_SUCCESS 1
#define STCP_ERROR -1
#define FAST_RETRANSMIT_THRESHOLD 3

typedef struct {
    int fd;
    int state;

    unsigned int nextSeqNo;    /* next seq# to assign to outgoing data */
    unsigned int sendBase;     /* oldest unacked seq# */

    unsigned int recvNextSeqNo;/* receiver's next expected seq# (their ISN+1) */
    unsigned short recvWindow; /* receiver's latest advertised window */

    pktlist *unacked;          /* sent but not yet acked packets */
    long oldestSendTime;       /* when oldest unacked packet was (last) sent */
    int retransmitTimeout;     /* current timeout for oldest unacked */

    unsigned int lastAckNo;    /* ackNo of last received ACK (for dup detection) */
    int dupAckCount;           /* consecutive duplicate ACK count */
} stcp_send_ctrl_blk;


static void sendPacket(stcp_send_ctrl_blk *cb, packet *pkt) {
    htonHdr(pkt->hdr);
    pkt->hdr->checksum = 0;
    pkt->hdr->checksum = ipchecksum(pkt->data, pkt->len);
    dump('s', pkt->data, pkt->len);
    send(cb->fd, pkt->data, pkt->len, 0);
    ntohHdr(pkt->hdr);
}

static int checksumOk(packet *pkt) {
    unsigned short saved = pkt->hdr->checksum;
    pkt->hdr->checksum = 0;
    unsigned short computed = ipchecksum(pkt->data, pkt->len);
    pkt->hdr->checksum = saved;
    return saved == computed;
}

/* Returns bytes read, or STCP_READ_TIMED_OUT / STCP_READ_PERMANENT_FAILURE */
static int recvPacket(stcp_send_ctrl_blk *cb, packet *pkt, int timeoutMs) {
    int n = readWithTimeout(cb->fd, pkt->data, timeoutMs);
    if (n <= 0) return n;
    pkt->len = n;
    pkt->hdr = (tcpheader *)pkt->data;
    /* checksum must be verified while header is still in network byte order */
    if (!checksumOk(pkt)) {
        logLog("error", "bad checksum, dropping");
        return STCP_READ_TIMED_OUT;
    }
    ntohHdr(pkt->hdr);
    return n;
}

/* Free all unacked entries whose last byte falls at or before ackNo, advance sendBase */
static void processAck(stcp_send_ctrl_blk *cb, unsigned int ackNo) {
    if (!greater32(ackNo, cb->sendBase)) return;

    int advanced = 0;
    while (cb->unacked != NULL) {
        pktlist *oldest = cb->unacked;
        unsigned int endSeq = plus32(oldest->seqNo, (unsigned)payloadSize(&oldest->packet));
        if (greater32(endSeq, ackNo)) break;
        cb->unacked = oldest->next;
        oldest->next = NULL;
        freePacket(oldest);
        advanced = 1;
    }

    cb->sendBase = ackNo;

    if (advanced && cb->unacked != NULL) {
        cb->oldestSendTime    = now();
        cb->retransmitTimeout = STCP_INITIAL_TIMEOUT;
    }
}

/*
 * Track duplicate ACKs. Returns the current consecutive dup ACK count.
 * Resets count when ackNo advances, increments when same ackNo seen again.
 */
static int updateDupAck(stcp_send_ctrl_blk *cb, unsigned int ackNo) {
    if (ackNo == cb->lastAckNo) {
        cb->dupAckCount++;
    } else if (greater32(ackNo, cb->lastAckNo)) {
        cb->dupAckCount = 0;
        cb->lastAckNo   = ackNo;
    }
    /* stale ACK (ackNo < lastAckNo): leave counts unchanged */
    return cb->dupAckCount;
}

/*
 * Drain all buffered ACKs without blocking.
 * Updates dup ACK count and sendBase. Does NOT trigger fast retransmit.
 * Returns STCP_ERROR on RST or permanent failure.
 */
static int drainAcks(stcp_send_ctrl_blk *cb) {
    packet resp;
    while (1) {
        int n = recvPacket(cb, &resp, 0);
        if (n == STCP_READ_TIMED_OUT)       return STCP_SUCCESS;
        if (n == STCP_READ_PERMANENT_FAILURE) return STCP_ERROR;
        if (getRst(resp.hdr))               return STCP_ERROR;
        if (getAck(resp.hdr)) {
            cb->recvWindow = resp.hdr->windowSize;
            updateDupAck(cb, resp.hdr->ackNo);
            processAck(cb, resp.hdr->ackNo);
        }
    }
}

/*
 * Fast retransmit: drain buffered ACKs, resend oldest unacked packet,
 * then wait (blocking) until an ACK advances past sendBase.
 * Duplicate ACKs at the same ackNo are ignored (no new fast retransmit triggered).
 * Timeout doubles like a normal timeout, starting at 2s.
 */
static int fastRetransmit(stcp_send_ctrl_blk *cb) {
    if (cb->unacked == NULL) return STCP_SUCCESS;

    logLog("segment", "fast retransmit at seqNo %u", cb->unacked->seqNo);

    if (drainAcks(cb) == STCP_ERROR) return STCP_ERROR;

    sendPacket(cb, &cb->unacked->packet);
    /* treated as if a timeout occurred → next timeout is 2s */
    cb->retransmitTimeout = stcpNextTimeout(STCP_INITIAL_TIMEOUT);
    cb->oldestSendTime    = now();
    cb->dupAckCount       = 0;

    unsigned int stalledAt = cb->sendBase;   /* the seqNo we're waiting to get past */

    while (cb->unacked != NULL) {
        long elapsed = now() - cb->oldestSendTime;
        int timeLeft = max(0, cb->retransmitTimeout - (int)elapsed);

        packet resp;
        int n = recvPacket(cb, &resp, timeLeft);
        if (n == STCP_READ_PERMANENT_FAILURE) return STCP_ERROR;
        if (n == STCP_READ_TIMED_OUT) {
            sendPacket(cb, &cb->unacked->packet);
            cb->retransmitTimeout = stcpNextTimeout(cb->retransmitTimeout);
            cb->oldestSendTime    = now();
            continue;
        }
        if (getRst(resp.hdr)) return STCP_ERROR;
        if (!getAck(resp.hdr)) continue;

        unsigned int ackNo = resp.hdr->ackNo;

        /* ignore ACKs that don't advance past the stalled point */
        if (!greater32(ackNo, stalledAt)) continue;

        cb->recvWindow  = resp.hdr->windowSize;
        cb->lastAckNo   = ackNo;
        cb->dupAckCount = 0;
        processAck(cb, ackNo);
        break;
    }
    return STCP_SUCCESS;
}

/*
 * Send STCP. This routine is to send all the data (len bytes).  If more
 * than MSS bytes are to be sent, the routine breaks the data into multiple
 * packets. It will keep sending data until the send window is full or all
 * the data has been sent. At which point it reads data from the network to,
 * hopefully, get the ACKs that open the window. You will need to be careful
 * about timing your packets and dealing with the last piece of data.
 *
 * Your sender program will spend almost all of its time in either this
 * function or in tcp_close().  All input processing (you can use the
 * function readWithTimeout() defined in stcp.c to receive segments) is done
 * as a side effect of the work of this function (and stcp_close()).
 *
 * The function returns STCP_SUCCESS on success, or STCP_ERROR on error.
 */
int stcp_send(stcp_send_ctrl_blk *cb, unsigned char *data, int length) {
    int offset = 0;

    while (offset < length) {
        /* Process any already-arrived ACKs without blocking */
        if (drainAcks(cb) == STCP_ERROR) return STCP_ERROR;

        if (cb->dupAckCount >= FAST_RETRANSMIT_THRESHOLD && cb->unacked != NULL) {
            if (fastRetransmit(cb) == STCP_ERROR) return STCP_ERROR;
            continue;
        }

        /* Retransmit oldest if it has timed out */
        if (cb->unacked != NULL) {
            long elapsed = now() - cb->oldestSendTime;
            if (elapsed >= cb->retransmitTimeout) {
                sendPacket(cb, &cb->unacked->packet);
                cb->retransmitTimeout = stcpNextTimeout(cb->retransmitTimeout);
                cb->oldestSendTime    = now();
            }
        }

        unsigned int inFlight = minus32(cb->nextSeqNo, cb->sendBase);
        int windowAvail = (int)cb->recvWindow - (int)inFlight;

        if (windowAvail > 0) {
            int chunkLen = min(min(STCP_MSS, windowAvail), length - offset);
            int wasEmpty = (cb->unacked == NULL);

            packet pkt;
            createSegment(&pkt, ACK, 0, cb->nextSeqNo, cb->recvNextSeqNo,
                          data + offset, chunkLen);
            sendPacket(cb, &pkt);
            addPacket(&cb->unacked, cb->nextSeqNo, &pkt);
            cb->nextSeqNo = plus32(cb->nextSeqNo, (unsigned)chunkLen);
            offset += chunkLen;

            if (wasEmpty) {
                cb->oldestSendTime    = now();
                cb->retransmitTimeout = STCP_INITIAL_TIMEOUT;
            }
        } else if (cb->unacked != NULL) {
            /* Window full with packets in flight — block until ACK or timeout */
            long elapsed = now() - cb->oldestSendTime;
            int timeLeft = max(1, cb->retransmitTimeout - (int)elapsed);

            packet resp;
            int n = recvPacket(cb, &resp, timeLeft);
            if (n == STCP_READ_PERMANENT_FAILURE) return STCP_ERROR;
            if (n == STCP_READ_TIMED_OUT) {
                sendPacket(cb, &cb->unacked->packet);
                cb->retransmitTimeout = stcpNextTimeout(cb->retransmitTimeout);
                cb->oldestSendTime    = now();
            } else {
                if (getRst(resp.hdr)) return STCP_ERROR;
                if (getAck(resp.hdr)) {
                    cb->recvWindow = resp.hdr->windowSize;
                    int dups = updateDupAck(cb, resp.hdr->ackNo);
                    processAck(cb, resp.hdr->ackNo);
                    if (dups >= FAST_RETRANSMIT_THRESHOLD && cb->unacked != NULL) {
                        if (fastRetransmit(cb) == STCP_ERROR) return STCP_ERROR;
                    }
                }
            }
        } else {
            /* Zero window with nothing in flight — wait for receiver window update */
            packet resp;
            int n = recvPacket(cb, &resp, STCP_INITIAL_TIMEOUT);
            if (n == STCP_READ_PERMANENT_FAILURE) return STCP_ERROR;
            if (n != STCP_READ_TIMED_OUT && getAck(resp.hdr)) {
                if (getRst(resp.hdr)) return STCP_ERROR;
                cb->recvWindow = resp.hdr->windowSize;
                processAck(cb, resp.hdr->ackNo);
            }
        }
    }
    return STCP_SUCCESS;
}


/*
 * Open the sender side of the STCP connection. Returns the pointer to
 * a newly allocated control block containing the basic information
 * about the connection. Returns NULL if an error happened.
 *
 * If you use udp_open() it will use connect() on the UDP socket
 * then all packets then sent and received on the given file
 * descriptor go to and are received from the specified host. Reads
 * and writes are still completed in a datagram unit size, but the
 * application does not have to do the multiplexing and
 * demultiplexing. This greatly simplifies things but restricts the
 * number of "connections" to the number of file descriptors and isn't
 * very good for a pure request response protocol like DNS where there
 * is no long term relationship between the client and server.
 */
stcp_send_ctrl_blk *stcp_open(char *destination, int sendersPort, int receiversPort) {
    logLog("init", "Sending from port %d to <%s, %d>", sendersPort, destination, receiversPort);

    int fd = udp_open(destination, receiversPort, sendersPort);
    if (fd < 0) return NULL;

    stcp_send_ctrl_blk *cb = calloc(1, sizeof(stcp_send_ctrl_blk));
    cb->fd      = fd;
    cb->state   = STCP_SENDER_SYN_SENT;
    cb->unacked = NULL;

    srand((unsigned)time(NULL));
    unsigned int isn  = (unsigned int)rand();
    cb->nextSeqNo = plus32(isn, 1);
    cb->sendBase  = plus32(isn, 1);
    cb->lastAckNo = plus32(isn, 1);

    packet syn;
    createSegment(&syn, SYN, 0, isn, 0, NULL, 0);

    int timeout = STCP_INITIAL_TIMEOUT;
    while (1) {
        sendPacket(cb, &syn);

        packet resp;
        int n = recvPacket(cb, &resp, timeout);
        if (n == STCP_READ_PERMANENT_FAILURE) { free(cb); return NULL; }
        if (n == STCP_READ_TIMED_OUT)         { timeout = stcpNextTimeout(timeout); continue; }
        if (getRst(resp.hdr))                 { logLog("error", "RST in handshake"); free(cb); return NULL; }
        if (!getSyn(resp.hdr) || !getAck(resp.hdr)) continue;
        if (resp.hdr->ackNo != plus32(isn, 1))      continue;

        cb->recvWindow    = resp.hdr->windowSize;
        cb->recvNextSeqNo = plus32(resp.hdr->seqNo, 1);
        cb->state         = STCP_SENDER_ESTABLISHED;
        logLog("init", "established, window=%d", cb->recvWindow);
        return cb;
    }
}


/*
 * Make sure all the outstanding data has been transmitted and
 * acknowledged, and then initiate closing the connection. This
 * function is also responsible for freeing and closing all necessary
 * structures that were not previously freed, including the control
 * block itself.
 *
 * Returns STCP_SUCCESS on success or STCP_ERROR on error.
 */
int stcp_close(stcp_send_ctrl_blk *cb) {
    cb->state = STCP_SENDER_CLOSING;

    /* Drain any unacked data before sending FIN */
    while (cb->unacked != NULL) {
        if (cb->dupAckCount >= FAST_RETRANSMIT_THRESHOLD) {
            if (fastRetransmit(cb) == STCP_ERROR) goto cleanup;
            continue;
        }

        long elapsed = now() - cb->oldestSendTime;
        int timeLeft = max(0, cb->retransmitTimeout - (int)elapsed);

        packet resp;
        int n = recvPacket(cb, &resp, timeLeft);
        if (n == STCP_READ_PERMANENT_FAILURE) goto cleanup;
        if (n == STCP_READ_TIMED_OUT) {
            sendPacket(cb, &cb->unacked->packet);
            cb->retransmitTimeout = stcpNextTimeout(cb->retransmitTimeout);
            cb->oldestSendTime    = now();
        } else {
            if (getRst(resp.hdr)) goto cleanup;
            if (getAck(resp.hdr)) {
                cb->recvWindow = resp.hdr->windowSize;
                updateDupAck(cb, resp.hdr->ackNo);
                processAck(cb, resp.hdr->ackNo);
            }
        }
    }

    {
        packet fin;
        createSegment(&fin, FIN | ACK, 0, cb->nextSeqNo, cb->recvNextSeqNo, NULL, 0);
        unsigned int finSeq = cb->nextSeqNo;
        cb->nextSeqNo = plus32(cb->nextSeqNo, 1);
        cb->state     = STCP_SENDER_FIN_WAIT;

        int timeout = STCP_INITIAL_TIMEOUT;
        while (1) {
            sendPacket(cb, &fin);

            packet resp;
            int n = recvPacket(cb, &resp, timeout);
            if (n == STCP_READ_PERMANENT_FAILURE) break;
            if (n == STCP_READ_TIMED_OUT)         { timeout = stcpNextTimeout(timeout); continue; }
            if (getRst(resp.hdr))                 break;
            if (!getAck(resp.hdr))                continue;
            if (greater32(resp.hdr->ackNo, finSeq) ||
                resp.hdr->ackNo == cb->nextSeqNo) break;
        }
    }

cleanup:
    cb->state = STCP_SENDER_CLOSED;
    while (cb->unacked) {
        pktlist *next = cb->unacked->next;
        cb->unacked->next = NULL;
        freePacket(cb->unacked);
        cb->unacked = next;
    }
    close(cb->fd);
    free(cb);
    return STCP_SUCCESS;
}

/*
 * Return a port number based on the uid of the caller.  This will
 * with reasonably high probability return a port number different from
 * that chosen for other uses on the undergraduate Linux systems.
 *
 * This port is used if ports are not specified on the command line.
 */
int getDefaultPort() {
    uid_t uid = getuid();
    int port = (uid % (32768 - 512) * 2) + 1024;
    assert(port >= 1024 && port <= 65535 - 1);
    return port;
}

/*
 * This application is to invoke the send-side functionality.
 */
int main(int argc, char **argv) {
    stcp_send_ctrl_blk *cb;

    char *destinationHost;
    int receiversPort, sendersPort;
    char *filename = NULL;
    int file;
    /* You might want to change the size of this buffer to test how your
     * code deals with different packet sizes.
     */
    unsigned char buffer[STCP_MSS];
    int num_read_bytes;

    logConfig("sender", "init,segment,error,failure");
    /* Verify that the arguments are right */
    if (argc > 5 || argc == 1) {
        fprintf(stderr, "usage: sender DestinationIPAddress/Name receiveDataOnPort sendDataToPort filename\n");
        fprintf(stderr, "or   : sender filename\n");
        exit(1);
    }
    if (argc == 2) {
        filename = argv[1];
        argc--;
    }

    // Extract the arguments
    destinationHost = argc > 1 ? argv[1] : "localhost";
    receiversPort = argc > 2 ? atoi(argv[2]) : getDefaultPort();
    sendersPort = argc > 3 ? atoi(argv[3]) : getDefaultPort() + 1;
    if (argc > 4) filename = argv[4];

    /* Open file for transfer */
    file = open(filename, O_RDONLY);
    if (file < 0) {
        logPerror(filename);
        exit(1);
    }

    /*
     * Open connection to destination.  If stcp_open succeeds the
     * control block should be correctly initialized.
     */
    cb = stcp_open(destinationHost, sendersPort, receiversPort);
    if (cb == NULL) {
        fprintf(stderr, "stcp_open failed\n");
        exit(1);
    }

    /* Start to send data in file via STCP to remote receiver. Chop up
     * the file into pieces as large as max packet size and transmit
     * those pieces.
     */
    while (1) {
        num_read_bytes = read(file, buffer, sizeof(buffer));

        /* Break when EOF is reached */
        if (num_read_bytes <= 0)
            break;

        if (stcp_send(cb, buffer, num_read_bytes) == STCP_ERROR) {
            fprintf(stderr, "stcp_send failed\n");
            break;
        }
    }

    /* Close the connection to remote receiver */
    if (stcp_close(cb) == STCP_ERROR)
        fprintf(stderr, "stcp_close failed\n");

    return 0;
}
