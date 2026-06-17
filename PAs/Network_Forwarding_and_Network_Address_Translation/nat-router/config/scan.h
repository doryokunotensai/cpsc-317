#ifndef __CONFIG_SCAN_H__
#define __CONFIG_SCAN_H__

#include <regex.h>
#include <stdio.h>
#include <stdint.h>

#define MAXLINELENGTH 128

typedef enum ctoken {
    TCEXTERNAL,
    TCFORWARD,
    TCPORT,
    TCPORTFORWARD,
    TCTCP,
    TCUDP,
    TCDOT,
    TCSLASH,
    TCNL,
    TCNUM,
    TCEOF,
    TCERROR,
} ctoken;

typedef struct cscanstate {
    char line[MAXLINELENGTH];
    char lexeme[MAXLINELENGTH];
    FILE *infile;
    int pos;
    regex_t regex;
    regmatch_t matches[10];
} cscanstate;

extern void configScanInit(cscanstate *);
extern void configScanClose(cscanstate *);
extern void configScanTarget(cscanstate *, char *);
extern int configScanTargetFile(cscanstate *, char *);
extern ctoken configScanNext(cscanstate *);
extern char *configScanLexeme(cscanstate *);
extern char *configTokenName(ctoken t);
#endif
