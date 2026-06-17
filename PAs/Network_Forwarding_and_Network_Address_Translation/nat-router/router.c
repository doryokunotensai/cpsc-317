#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ip.h"
#include "config.h"
#include "util.h"
#include "log.h"
#include "tcp.h"
#include "udp.h"

#define MAX_INTERFACES 8
#define MAX_FORWARD_ENTRIES 128
#define MAX_NAT_ENTRIES 2048
#define MAX_PORT_FORWARD_RULES 128
#define FIRST_NAT_PORT 1024

typedef struct forward_entry {
    uint32_t network;
    uint32_t mask;
    int netlength;
    int interface;
} forward_entry;

typedef struct nat_entry {
    uint32_t internalip;
    uint16_t internalport;
    uint32_t remoteip;
    uint16_t remoteport;
    uint8_t protocol;
    uint16_t natport;
} nat_entry;

typedef struct port_forward_rule {
    uint8_t protocol;
    uint16_t externalport;
    uint32_t internalip;
    uint16_t internalport;
} port_forward_rule;

typedef struct router_state {
    int interfaces[MAX_INTERFACES];
    forward_entry forwards[MAX_FORWARD_ENTRIES];
    int numForwardEntries;
    nat_entry natEntries[MAX_NAT_ENTRIES];
    int numNatEntries;
    port_forward_rule portForwardRules[MAX_PORT_FORWARD_RULES];
    int numPortForwardRules;
    int hasExternalInterface;
    int externalInterface;
    uint32_t externalip;
    int listenPort;
} router_state;

static router_state router = {
    .externalInterface = -1,
    .listenPort = -1
};

static void failConfig(const char *message) {
    fprintf(stderr, "%s\n", message);
    exit(1);
}

static uint32_t maskForLength(int netlength) {
    if (netlength == 0) {
        return 0;
    }
    return 0xffffffffU << (32 - netlength);
}

static int validInterface(int interface) {
    return interface >= 0 && interface < MAX_INTERFACES;
}

static int interfaceExists(int interface) {
    return validInterface(interface) && router.interfaces[interface];
}

static ipheader *packetHeader(packet *pkt) {
    pkt->hdr = (ipheader *)pkt->data;
    return pkt->hdr;
}

static int findForwardInterface(uint32_t dstipaddr) {
    int bestInterface = -1;
    int bestLength = -1;

    for (int i = 0; i < router.numForwardEntries; i++) {
        forward_entry *entry = &router.forwards[i];
        if ((dstipaddr & entry->mask) == entry->network &&
            entry->netlength > bestLength) {
            bestLength = entry->netlength;
            bestInterface = entry->interface;
        }
    }

    return bestInterface;
}

static int isTcpOrUdp(uint8_t protocol) {
    return protocol == PROT_TCP || protocol == PROT_UDP;
}

static void updateIpChecksum(ipheader *hdr) {
    hdr->checksum = 0;
    hdr->checksum = ipchecksum(hdr, sizeof(ipheader), NULL, 0);
}

static int validIpPacket(packet *pkt) {
    ipheader *hdr = packetHeader(pkt);
    int headerLength = (hdr->verslen & 0x0f) * 4;
    int datagramLength = ntohs(hdr->length);

    if (pkt->len < (int)sizeof(ipheader) || headerLength != (int)sizeof(ipheader)) {
        return 0;
    }
    if ((hdr->verslen >> 4) != 4 || hdr->typeOfService != 0 ||
        hdr->flagsoffset != 0) {
        return 0;
    }
    if (datagramLength < (int)sizeof(ipheader) || datagramLength > pkt->len) {
        return 0;
    }
    if (ipchecksum(hdr, sizeof(ipheader), NULL, 0) != 0) {
        return 0;
    }

    pkt->len = datagramLength;
    return 1;
}

static int transportLength(packet *pkt) {
    return ntohs(pkt->hdr->length) - (int)sizeof(ipheader);
}

static void *transportHeader(packet *pkt) {
    return pkt->data + sizeof(ipheader);
}

static uint16_t packetSourcePort(packet *pkt) {
    if (pkt->hdr->protocol == PROT_TCP) {
        tcpheader *tcp = (tcpheader *)transportHeader(pkt);
        return ntohs(tcp->sport);
    }

    udpheader *udp = (udpheader *)transportHeader(pkt);
    return ntohs(udp->sport);
}

static uint16_t packetDestinationPort(packet *pkt) {
    if (pkt->hdr->protocol == PROT_TCP) {
        tcpheader *tcp = (tcpheader *)transportHeader(pkt);
        return ntohs(tcp->dport);
    }

    udpheader *udp = (udpheader *)transportHeader(pkt);
    return ntohs(udp->dport);
}

static void setPacketSourcePort(packet *pkt, uint16_t port) {
    if (pkt->hdr->protocol == PROT_TCP) {
        tcpheader *tcp = (tcpheader *)transportHeader(pkt);
        tcp->sport = htons(port);
    } else {
        udpheader *udp = (udpheader *)transportHeader(pkt);
        udp->sport = htons(port);
    }
}

static void setPacketDestinationPort(packet *pkt, uint16_t port) {
    if (pkt->hdr->protocol == PROT_TCP) {
        tcpheader *tcp = (tcpheader *)transportHeader(pkt);
        tcp->dport = htons(port);
    } else {
        udpheader *udp = (udpheader *)transportHeader(pkt);
        udp->dport = htons(port);
    }
}

static int validTransportPacket(packet *pkt) {
    int len = transportLength(pkt);

    if (pkt->hdr->protocol == PROT_UDP) {
        return len >= (int)sizeof(udpheader);
    }
    if (pkt->hdr->protocol == PROT_TCP) {
        if (len < (int)sizeof(tcpheader)) {
            return 0;
        }
        if (tcpchecksum(pkt->hdr->srcipaddr, pkt->hdr->dstipaddr,
                        transportHeader(pkt), len) != 0) {
            return 0;
        }
        return 1;
    }

    return 0;
}

static void updateTransportChecksum(packet *pkt) {
    if (pkt->hdr->protocol == PROT_UDP) {
        udpheader *udp = (udpheader *)transportHeader(pkt);
        udp->sum = 0;
    } else if (pkt->hdr->protocol == PROT_TCP) {
        tcpheader *tcp = (tcpheader *)transportHeader(pkt);
        tcp->sum = 0;
        tcp->sum = tcpchecksum(pkt->hdr->srcipaddr, pkt->hdr->dstipaddr,
                               tcp, transportLength(pkt));
    }
}

static nat_entry *findOutboundNatEntry(packet *pkt) {
    uint32_t internalip = pkt->hdr->srcipaddr;
    uint32_t remoteip = pkt->hdr->dstipaddr;
    uint16_t internalport = packetSourcePort(pkt);
    uint16_t remoteport = packetDestinationPort(pkt);

    for (int i = 0; i < router.numNatEntries; i++) {
        nat_entry *entry = &router.natEntries[i];
        if (entry->protocol == pkt->hdr->protocol &&
            entry->internalip == internalip &&
            entry->internalport == internalport &&
            entry->remoteip == remoteip &&
            entry->remoteport == remoteport) {
            return entry;
        }
    }

    return NULL;
}

static nat_entry *findInboundNatEntry(packet *pkt) {
    uint32_t remoteip = pkt->hdr->srcipaddr;
    uint16_t remoteport = packetSourcePort(pkt);
    uint16_t natport = packetDestinationPort(pkt);

    for (int i = 0; i < router.numNatEntries; i++) {
        nat_entry *entry = &router.natEntries[i];
        if (entry->protocol == pkt->hdr->protocol &&
            entry->remoteip == remoteip &&
            entry->remoteport == remoteport &&
            entry->natport == natport) {
            return entry;
        }
    }

    return NULL;
}

static port_forward_rule *findPortForwardRule(packet *pkt) {
    uint16_t externalport = packetDestinationPort(pkt);

    for (int i = 0; i < router.numPortForwardRules; i++) {
        port_forward_rule *rule = &router.portForwardRules[i];
        if (rule->protocol == pkt->hdr->protocol &&
            rule->externalport == externalport) {
            return rule;
        }
    }

    return NULL;
}

static int natPortInUse(uint8_t protocol, uint16_t natport) {
    for (int i = 0; i < router.numNatEntries; i++) {
        nat_entry *entry = &router.natEntries[i];
        if (entry->protocol == protocol && entry->natport == natport) {
            return 1;
        }
    }

    return 0;
}

static uint16_t allocateNatPort(uint8_t protocol) {
    for (uint32_t port = FIRST_NAT_PORT; port <= 65535; port++) {
        if (!natPortInUse(protocol, (uint16_t)port)) {
            return (uint16_t)port;
        }
    }

    return 0;
}

static nat_entry *addNatEntry(uint32_t internalip, uint16_t internalport,
                              uint32_t remoteip, uint16_t remoteport,
                              uint8_t protocol, uint16_t natport) {
    if (router.numNatEntries >= MAX_NAT_ENTRIES) {
        return NULL;
    }

    nat_entry *entry = &router.natEntries[router.numNatEntries++];
    entry->internalip = internalip;
    entry->internalport = internalport;
    entry->remoteip = remoteip;
    entry->remoteport = remoteport;
    entry->protocol = protocol;
    entry->natport = natport;
    return entry;
}

static int translateOutbound(packet *pkt) {
    nat_entry *entry = findOutboundNatEntry(pkt);

    if (entry == NULL) {
        uint16_t natport = allocateNatPort(pkt->hdr->protocol);
        if (natport == 0) {
            return 0;
        }
        entry = addNatEntry(pkt->hdr->srcipaddr, packetSourcePort(pkt),
                            pkt->hdr->dstipaddr, packetDestinationPort(pkt),
                            pkt->hdr->protocol, natport);
        if (entry == NULL) {
            return 0;
        }
    }

    pkt->hdr->srcipaddr = router.externalip;
    setPacketSourcePort(pkt, entry->natport);
    updateTransportChecksum(pkt);
    return 1;
}

static int translateInbound(packet *pkt) {
    nat_entry *entry = findInboundNatEntry(pkt);

    if (entry == NULL) {
        port_forward_rule *rule = findPortForwardRule(pkt);
        if (rule == NULL) {
            return 0;
        }
        entry = addNatEntry(rule->internalip, rule->internalport,
                            pkt->hdr->srcipaddr, packetSourcePort(pkt),
                            pkt->hdr->protocol, rule->externalport);
        if (entry == NULL) {
            return 0;
        }
    }

    pkt->hdr->dstipaddr = entry->internalip;
    setPacketDestinationPort(pkt, entry->internalport);
    updateTransportChecksum(pkt);
    return 1;
}

static int applyNatIfNeeded(packet *pkt, int incomingInterface,
                            int *outgoingInterface) {
    if (!router.hasExternalInterface) {
        return 1;
    }

    if (incomingInterface == router.externalInterface) {
        if (pkt->hdr->dstipaddr != router.externalip ||
            !isTcpOrUdp(pkt->hdr->protocol) ||
            !validTransportPacket(pkt)) {
            return 0;
        }
        if (!translateInbound(pkt)) {
            return 0;
        }
        *outgoingInterface = findForwardInterface(pkt->hdr->dstipaddr);
        return *outgoingInterface != -1 &&
            *outgoingInterface != router.externalInterface;
    }

    if (*outgoingInterface == router.externalInterface) {
        if (!isTcpOrUdp(pkt->hdr->protocol) || !validTransportPacket(pkt)) {
            return 0;
        }
        return translateOutbound(pkt);
    }

    return 1;
}

/*
 * Add an interface to your router. 
 */
void addInterface(int interface) {
    if (!validInterface(interface)) {
        failConfig("Interface number must be between 0 and 7");
    }

    router.interfaces[interface] = 1;
}

/*
 * Mark the given interface (which must have been previously added) as the
 * external interface and assign it the given ip address.
 */
void markInterfaceExternal(int interface, uint32_t ip) {
    if (!interfaceExists(interface)) {
        failConfig("External interface must be added before it is marked");
    }
    if (router.hasExternalInterface) {
        failConfig("Only one external interface may be configured");
    }

    router.hasExternalInterface = 1;
    router.externalInterface = interface;
    router.externalip = ip;
}

/*
 * Add a forwarding entry to your forwarding table.  The network address is
 * given by the network and netlength arguments.  Datagrams destined for
 * this network are to be sent out on the indicated interface.
 */
void addForwardEntry(uint32_t network, int netlength, int interface) {
    if (!interfaceExists(interface)) {
        failConfig("Forwarding interface must be added before use");
    }
    if (netlength < 0 || netlength > 32) {
        failConfig("Forwarding prefix length must be between 0 and 32");
    }
    if (router.numForwardEntries >= MAX_FORWARD_ENTRIES) {
        failConfig("Too many forwarding entries");
    }

    forward_entry *entry = &router.forwards[router.numForwardEntries++];
    entry->mask = maskForLength(netlength);
    entry->network = network & entry->mask;
    entry->netlength = netlength;
    entry->interface = interface;
}

/*
 * Add a port-forwarding rule to your NAT table.  Incoming datagrams using
 * the given protocol that are destined for the given natport on the
 * external interface are to be forwarded to the given internalip and
 * internal port given by the network and netlength arguments.  
 */
void addPortForwardingRule(int protocol, int natport,
                                       uint32_t internalip, int internalport) {
    if (!isTcpOrUdp(protocol)) {
        failConfig("Port forwarding protocol must be TCP or UDP");
    }
    if (natport < 0 || natport > 65535 ||
        internalport < 0 || internalport > 65535) {
        failConfig("Port forwarding ports must fit in 16 bits");
    }
    if (router.numPortForwardRules >= MAX_PORT_FORWARD_RULES) {
        failConfig("Too many port forwarding rules");
    }

    port_forward_rule *rule =
        &router.portForwardRules[router.numPortForwardRules++];
    rule->protocol = protocol;
    rule->externalport = natport;
    rule->internalip = internalip;
    rule->internalport = internalport;
}
void run() {
    int port = router.listenPort == -1 ? getDefaultPort() : router.listenPort;
    int fd = udp_open(port);
    if (fd < 0) {
        exit(1);
    }

    while (1) {
        packet pkt;
        int incomingInterface;
        int bytesRead = readpkt(fd, &pkt, &incomingInterface);

        if (bytesRead == UTIL_READ_PERMANENT_FAILURE) {
            break;
        }
        if (bytesRead <= 0) {
            continue;
        }
        if (!validIpPacket(&pkt)) {
            continue;
        }
        if (pkt.hdr->ttl <= 1) {
            continue;
        }

        int outgoingInterface = -1;
        if (!(router.hasExternalInterface &&
              incomingInterface == router.externalInterface)) {
            outgoingInterface = findForwardInterface(pkt.hdr->dstipaddr);
        }
        if (outgoingInterface == -1 &&
            !(router.hasExternalInterface &&
              incomingInterface == router.externalInterface)) {
            continue;
        }
        if (outgoingInterface == incomingInterface) {
            continue;
        }

        if (!applyNatIfNeeded(&pkt, incomingInterface, &outgoingInterface)) {
            continue;
        }

        pkt.hdr->ttl--;
        updateIpChecksum(pkt.hdr);
        sendpkt(fd, outgoingInterface, &pkt);
    }
}

int main(int argc, char **argv) {
    logConfig("router", "packet,error,failure");

    char *configFileName = "router.config";
    if (argc == 2) {
        if (isdigit((unsigned char)argv[1][0])) {
            router.listenPort = atoi(argv[1]);
        } else {
            configFileName = argv[1];
        }
    } else if (argc > 2) {
        router.listenPort = atoi(argv[1]);
	configFileName = argv[2];
    }
    configLoad(configFileName, addInterface, markInterfaceExternal,
               addForwardEntry, addPortForwardingRule);
    run();
    return 0;
}
