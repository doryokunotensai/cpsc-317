#ifndef __CONFIG_PARSE_H__
#define __CONFIG_PARSE_H__
#include <setjmp.h>
#include "scan.h"

typedef struct cparsestate {
    cscanstate *scan;
    ctoken next;
    jmp_buf jmp;
    char errormsg[MAXLINELENGTH];
} cparsestate;

typedef struct cparseport {
    int port;
} cparseport, scriptport;

typedef struct cparseip {
    unsigned int part[4];
} cparseip, scriptip;

typedef struct cparsenet {
    unsigned int part[4];
    int netlength;
} cparsenet, scriptnet;

typedef struct cparserule {
    cparsenet ip;
    int port;
} cparserule, scriptrule;

typedef struct cparseexternal {
    cparseip ip;
    int port;
} cparseexternal, scriptexternal;

typedef struct cparseportforward {
    int protocol;
    int externalport;
    cparseip internalip;
    int internalport;
} cparseportforward, scriptportforward;

typedef struct cparseline {
    struct cparseline *next;
    ctoken key;
    cparserule rule;
    cparseport port;
    cparseexternal external;
    cparseportforward portforward;
} cparseline, cscript;
    
extern void configParseInit(cparsestate *s, cscanstate *scan);
extern cscript *configParse(cparsestate *s);
extern void configParsePrint(cparseline *);
extern uint32_t cparseip_to_uint32_t(cparseip ip);
#endif
