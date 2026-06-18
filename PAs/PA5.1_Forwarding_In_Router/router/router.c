#include <assert.h>
#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "ip.h"
#include "config.h"
#include "util.h"
#include "log.h"

#define MAX_FORWARD_ENTRIES 64
#define MAX_INTERFACES 8

typedef struct {
    uint32_t network;
    int netlength;
    int interface;
} ForwardEntry;

static ForwardEntry forwardTable[MAX_FORWARD_ENTRIES];
static int forwardCount = 0;

static int activeInterfaces[MAX_INTERFACES];
static int interfaceCount = 0;

static uint32_t prefixMask(int netlength) {
    if (netlength == 0) return 0;
    if (netlength == 32) return 0xFFFFFFFF;
    return ~((uint32_t)0xFFFFFFFF >> netlength);
}

static int lookupInterface(uint32_t dst) {
    int bestLen = -1;
    int bestInterface = -1;
    for (int i = 0; i < forwardCount; i++) {
        uint32_t mask = prefixMask(forwardTable[i].netlength);
        if ((dst & mask) == forwardTable[i].network && forwardTable[i].netlength > bestLen) {
            bestLen = forwardTable[i].netlength;
            bestInterface = forwardTable[i].interface;
        }
    }
    return bestInterface;
}

/*
 * Add a forwarding entry to your forwarding table.  The network address is
 * given by the network and netlength arguments.  Datagrams destined for
 * this network are to be sent out on the indicated interface.
 */
void addForwardEntry(uint32_t network, int netlength, int interface) {
    assert(forwardCount < MAX_FORWARD_ENTRIES);
    forwardTable[forwardCount++] = (ForwardEntry){network, netlength, interface};
}

/*
 * Add an interface to your router.
 */
void addInterface(int interface) {
    assert(interfaceCount < MAX_INTERFACES);
    activeInterfaces[interfaceCount++] = interface;
}

void run(int port) {
    int fd = udp_open(port);
    if (fd < 0) return;

    packet pkt;
    int interface;
    while (1) {
        int ret = readpkt(fd, &pkt, &interface);
        if (ret == UTIL_READ_PERMANENT_FAILURE) break;
        if (ret <= 0) continue;

        ipheader *hdr = (ipheader *)pkt.data;

        if (hdr->verslen != 0x45) {
            logLog("failure", "bad verslen 0x%02x, dropping", hdr->verslen);
            continue;
        }

        unsigned short savedChecksum = hdr->checksum;
        hdr->checksum = 0;
        unsigned short computed = ipchecksum(hdr, sizeof(ipheader));
        hdr->checksum = savedChecksum;
        if (computed != savedChecksum) {
            logLog("failure", "bad checksum (got 0x%04x expected 0x%04x), dropping", computed, savedChecksum);
            continue;
        }

        if (hdr->ttl == 0 || --hdr->ttl == 0) {
            logLog("failure", "TTL expired, dropping");
            continue;
        }

        int outInterface = lookupInterface(hdr->dstipaddr);
        if (outInterface == -1) {
            logLog("failure", "no route to dst, dropping");
            continue;
        }
        if (outInterface == interface) {
            logLog("failure", "incoming interface equals outgoing interface, dropping");
            continue;
        }

        hdr->checksum = 0;
        hdr->checksum = ipchecksum(hdr, sizeof(ipheader));

        sendpkt(fd, outInterface, &pkt);
    }
}

static int isNumber(char *str) {
    if (*str == '\0') {
        return 0;
    }
    while (*str != '\0') {
        if (!isdigit((unsigned char)*str)) {
            return 0;
        }
        str++;
    }
    return 1;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    logConfig("router", "packet,error,failure");

    int port = getDefaultPort();
    char *configFileName = "router.config";
    if (argc > 1 && isNumber(argv[1])) {
        port = atoi(argv[1]);
        if (argc > 2) {
            configFileName = argv[2];
        }
    } else if (argc > 1) {
        configFileName = argv[1];
    }
    configLoad(configFileName, addForwardEntry, addInterface);
    run(port);
    return 0;
}
