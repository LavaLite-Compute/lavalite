
#ifndef _LLCORE_TCL_STUB_
#define _LLCORE_TCL_STUB_
struct tclHostData {
    char *hostName;
    int maxCpus;
    int maxMem;
    int maxSwap;
    int maxTmp;
    int nDisks;
    short hostInactivityCount;
    int *status;
    float *loadIndex;
    int rexPriority;
    char *hostType;
    char *hostModel;
    char *fromHostType;
    char *fromHostModel;
    float cpuFactor;
    int ignDedicatedResource;
    int *resBitMaps;
    int *DResBitMaps;
    int numResPairs;
    struct resPair *resPairs;
    int flag;
    int overRideFromType;
};

struct tclLsInfo {
    int numIndx;
    char **indexNames;
    int nRes;
    char **resName;
    int *stringResBitMaps;
    int *numericResBitMaps;
};

#define TCL_CHECK_SYNTAX 0
#define TCL_CHECK_EXPRESSION 1

extern int initTcl(struct tclLsInfo *);
extern void freeTclLsInfo(struct tclLsInfo *, int);
extern int evalResReq(char *, struct tclHostData *, char);

#endif
