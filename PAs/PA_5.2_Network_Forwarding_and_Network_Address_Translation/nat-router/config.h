#ifndef __CONFIG_H__
#define __CONFIG_H__
extern void configLoad(char *fileName, void (*addInterface)(int interface), void (*markInterfaceExternal)(int interface, uint32_t externalip), void (*addForwardEntry)(uint32_t network, int netlength, int interface), void (*addPortForwardingRule)(int protocol, int externalport, uint32_t internalip, int internalport));

#endif
