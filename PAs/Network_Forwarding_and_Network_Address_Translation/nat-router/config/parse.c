#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "scan.h"
#include "parse.h"

#define MAXLENGTH 128
#define NBUFFERS    8

static char *nettostring(cparsenet *ip) {
    static char buff[NBUFFERS][MAXLENGTH];
    static int next = 0;
    char *buf = &buff[next][0];
    next = (next + 1) % NBUFFERS;
    
    snprintf(buf, MAXLENGTH, "%d.%d.%d.%d/%d",
	     ip->part[0], ip->part[1], ip->part[2], ip->part[3], ip->netlength);
    return buf;
}
	     
static char *iptostring(cparseip *ip) {
    static char buff[NBUFFERS][MAXLENGTH];
    static int next = 0;
    char *buf = &buff[next][0];
    next = (next + 1) % NBUFFERS;
    
    snprintf(buf, MAXLENGTH, "%d.%d.%d.%d",
	     ip->part[0], ip->part[1], ip->part[2], ip->part[3]);
    return buf;
}
	     

static void configParseNext(cparsestate *s) {
    s->next = configScanNext(s->scan);
}

static void configParseError(cparsestate *s, ctoken t) {
    snprintf(s->errormsg, sizeof(s->errormsg), "Syntax error: expected %s found %s", configTokenName(t), configTokenName(s->next));
    longjmp(s->jmp, 1);
}

static void configParseError2(cparsestate *s, ctoken t1, ctoken t2) {
    snprintf(s->errormsg, sizeof(s->errormsg), "Syntax error: expected %s or %s found %s", configTokenName(t1), configTokenName(t2), configTokenName(s->next));
    longjmp(s->jmp, 1);
}

static void configParseError4(cparsestate *s, ctoken t1, ctoken t2, ctoken t3, ctoken t4) {
    snprintf(s->errormsg, sizeof(s->errormsg), "Syntax error: expected %s, %s, %s, or %s found %s", configTokenName(t1), configTokenName(t2), configTokenName(t3), configTokenName(t4), configTokenName(s->next));
    longjmp(s->jmp, 1);
}

static void advance(cparsestate *s, ctoken t) {
    if (s->next == t) {
	configParseNext(s);
    } else {
	configParseError(s, t);
    }
}

static cparseport configParsePort(cparsestate *s);
static cparseip configParseIP(cparsestate *s);
static cparsenet configParseNet(cparsestate *s);
static cparserule configParseRule(cparsestate *s);
static cparseexternal configParseExternal(cparsestate *s);
static cparseportforward configParsePortForward(cparsestate *s);
static cparseline *configParseS(cparsestate *s);
static cparseline *configParseLine(cparsestate *s);

cparsenet defaultparsenet = { .part[0] = 0, .part[1] = 0, .part[2] = 0, .part[3] = 0, .netlength = 0 };
cparseip defaultparseip = { .part[0] = 0, .part[1] = 0, .part[2] = 0, .part[3] = 0 };
cparseport defaultparseport = { .port = 0 };
cparserule defaultparserule = { .ip = { .part[0] = 0, .part[1] = 0, .part[2] = 0, .part[3] = 0, .netlength = 0 }, .port = 0 };
cparseexternal defaultparseexternal = { .ip = { .part[0] = 0, .part[1] = 0, .part[2] = 0, .part[3] = 0 }, .port = 0 };
cparseportforward defaultparseportforward = { .protocol = 0, .externalport = 0, .internalip = { .part[0] = 0, .part[1] = 0, .part[2] = 0, .part[3] = 0 }, .internalport = 0 };

void configParseInit(cparsestate *s, cscanstate *scan) {
    s->scan = scan;
    configParseNext(s);
}

static cparsenet configParseNet(cparsestate *s) {
    cparsenet ans = {.netlength = 0 };
    if (s->next != TCNUM) { 
	configParseError(s, TCNUM);
	return ans;
    }
    ans.part[0] = strtol(configScanLexeme(s->scan), NULL, 10);
    configParseNext(s);
    for (int i = 1; i <= 3; i++) {
	if (s->next != TCDOT) { 
	    configParseError(s, TCDOT);
	    return ans;
	}
	configParseNext(s);
	if (s->next != TCNUM) { 
	    configParseError(s, TCNUM);
	    return ans;
	} else {
	    ans.part[i] = strtol(configScanLexeme(s->scan), NULL, 10);
	}
	configParseNext(s);
    }
    if (s->next != TCSLASH) { 
	configParseError(s, TCSLASH);
	return ans;
    }
    configParseNext(s);
    if (s->next != TCNUM) { 
	configParseError(s, TCNUM);
	return ans;
    } else {
	ans.netlength = strtol(configScanLexeme(s->scan), NULL, 10);
    }
    
    configParseNext(s);
    return ans;
}
    
static cparseip configParseIP(cparsestate *s) {
    cparseip ans = { };
    if (s->next != TCNUM) { 
	configParseError(s, TCNUM);
	return ans;
    }
    ans.part[0] = strtol(configScanLexeme(s->scan), NULL, 10);
    configParseNext(s);
    for (int i = 1; i <= 3; i++) {
	if (s->next != TCDOT) { 
	    configParseError(s, TCDOT);
	    return ans;
	}
	configParseNext(s);
	if (s->next != TCNUM) { 
	    configParseError(s, TCNUM);
	    return ans;
	} else {
	    ans.part[i] = strtol(configScanLexeme(s->scan), NULL, 10);
	}
	configParseNext(s);
    }
    return ans;
}
    
static cparseport configParsePort(cparsestate *s) {
    cparseport ans = { .port = 0 };
    advance(s, TCPORT);
    if (s->next != TCNUM) {
	configParseError(s, TCNUM);
	return ans;
    }
    ans.port = strtol(configScanLexeme(s->scan), NULL, 10);    
    configParseNext(s);
    return ans;
}

static cparserule configParseRule(cparsestate *s) {
    cparserule ans = { .port = 0 };
    advance(s, TCFORWARD);
    ans.ip = configParseNet(s);
    advance(s, TCPORT);
    if (s->next == TCNUM) {
	ans.port = strtol(configScanLexeme(s->scan), NULL, 10);
	advance(s, TCNUM);
    } else {
	configParseError(s, TCNUM);
    }
    return ans;
}

static cparseexternal configParseExternal(cparsestate *s) {
    cparseexternal ans = { .port = 0 };
    advance(s, TCEXTERNAL);
    if (s->next == TCNUM) {
	ans.port = strtol(configScanLexeme(s->scan), NULL, 10);
	advance(s, TCNUM);
    } else {
	configParseError(s, TCNUM);
    }
    ans.ip = configParseIP(s);
    return ans;
}

static cparseportforward configParsePortForward(cparsestate *s) {
    cparseportforward ans = {  };
    advance(s, TCPORTFORWARD);
    if (s->next == TCTCP) {
        advance(s, TCTCP);
        ans.protocol = 6;
    } else if (s->next == TCUDP) {
        advance(s, TCUDP);
        ans.protocol = 17;
    } else {
        configParseError2(s, TCUDP, TCTCP);
    }
    if (s->next == TCNUM) {
	ans.externalport = strtol(configScanLexeme(s->scan), NULL, 10);
	advance(s, TCNUM);
    } else {
	configParseError(s, TCNUM);
    }
    ans.internalip = configParseIP(s);
    if (s->next == TCNUM) {
	ans.internalport = strtol(configScanLexeme(s->scan), NULL, 10);
	advance(s, TCNUM);
    } else {
	configParseError(s, TCNUM);
    }
    return ans;
}

static cparseline *configParseLine(cparsestate *s) {
    cparseline *ans = NULL;
    cparseport p = defaultparseport;
    cparserule r = defaultparserule;
    cparseexternal e = defaultparseexternal;
    cparseportforward pf = defaultparseportforward;
    ctoken key = s->next;
    if (s->next == TCPORT) {
	p = configParsePort(s);
    } else if (s->next == TCFORWARD) {
	r = configParseRule(s);
    } else if (s->next == TCEXTERNAL) {
        e = configParseExternal(s);
    } else if (s->next == TCPORTFORWARD) {
	pf = configParsePortForward(s);
    } else if (s->next == TCNL) {
	advance(s, TCNL);
	return NULL;
    } else {
	configParseError4(s, TCPORT, TCFORWARD, TCEXTERNAL, TCPORTFORWARD);
    }
    ans = malloc(sizeof(cparseline));
    ans->next = NULL;
    ans->key = key;
    ans->port = p;
    ans->rule = r;
    ans->external = e;
    ans->portforward = pf;
    advance(s, TCNL);
    return ans;
}

static cparseline *configParseS(cparsestate *s) {
    cparseline *ans = NULL, **ansp = &ans;
    while (s->next != TCEOF) {
	cparseline *l = configParseLine(s);
	if (l != NULL) {
	    *ansp = l;
	    ansp = &l->next;
	}
    }
    return ans;
}

cparseline *configParse(cparsestate *s) {
    int x = setjmp(s->jmp);
    if (x == 0) {
	return configParseS(s);
    } else {
	fprintf(stderr, "%s\n", s->errormsg);
	return NULL;
    }
}

static void configPrintRule(cparserule r) {
    printf("forward %s port %d", nettostring(&r.ip), r.port);
}

static void configPrintExternal(cparseexternal e) {
    printf("external %d %s", e.port, iptostring(&e.ip));
}

static void configPrintPortForward(cparseportforward pf) {
    printf("portforward %s %d %s %d", pf.protocol == 6 ? "tcp" : pf.protocol == 17 ? "udp" : "bogus", pf.externalport, iptostring(&pf.internalip), pf.internalport);
}

static void configPrintPort(cparseport p) {
    printf("port %d", p.port);    
}


static void configPrintLine(cparseline *l) {
    if (l->key == TCPORT) {
	configPrintPort(l->port);
    } else if (l->key == TCFORWARD) {
	configPrintRule(l->rule);
    } else if (l->key == TCEXTERNAL) {
	configPrintExternal(l->external);
    } else if (l->key == TCPORTFORWARD) {
	configPrintPortForward(l->portforward);
    } else {
	assert(0);
    }
    putchar('\n');
}

void configParsePrint(cparseline *l) {
    while (l) {
	configPrintLine(l);
	l = l->next;
    }
}

uint32_t cparseip_to_uint32_t(cparseip ip) {
    uint32_t ip32 = (ip.part[0] << 24) +
        (ip.part[1] << 16) +
        (ip.part[2] << 8) +
        (ip.part[3] << 0);
    return ip32;
}
