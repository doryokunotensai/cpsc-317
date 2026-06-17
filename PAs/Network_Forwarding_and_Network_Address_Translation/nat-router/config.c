#include <assert.h>
#include <stdint.h>
#include "ip.h"
#include "config/scan.h"
#include "config/parse.h"

void configLoad(char *filename, void (*addInterface)(int interface), void (*markInterfaceExternal)(int interface, uint32_t externalip), void (*addForwardEntry)(uint32_t network, int netlength, int interface), void (*addPortForwardingRule)(int protocol, int externalport, uint32_t internalip, int internalport)) {
    cscanstate sstate, *ss = &sstate;
    cparsestate pstate, *s = &pstate;
    cparseline *l;

    configScanInit(ss);
    configScanTargetFile(ss, filename);
    configParseInit(s, ss);
    l = configParse(s);
    assert(l != NULL);
    while (l) {
	if (l->key == TCFORWARD) {
	    uint32_t ip = (l->rule.ip.part[0] << 24) +
		(l->rule.ip.part[1] << 16) +
		(l->rule.ip.part[2] << 8) +
		(l->rule.ip.part[3] << 0);
	    addForwardEntry(ip, l->rule.ip.netlength, l->rule.port);
	} else if (l->key == TCPORT) {
	    addInterface(l->port.port);
	} else if (l->key == TCEXTERNAL) {
	    markInterfaceExternal(l->external.port, cparseip_to_uint32_t(l->external.ip));
	} else if (l->key == TCPORTFORWARD) {
	    addPortForwardingRule(l->portforward.protocol, l->portforward.externalport, cparseip_to_uint32_t(l->portforward.internalip), l->portforward.internalport);
	}
	l = l->next;
    }
    configParsePrint(l);
    configScanClose(ss);
}
