
#include "lsf/lib/llsys.h"
#include "lsf/lib/tcl_stub.h"

int initTcl(struct tclLsInfo *t)
{
    return 1;
}
extern void freeTclLsInfo(struct tclLsInfo *t, int n)
{
}

int evalResReq(char *s, struct tclHostData *t, char c)
{
    return 1;
}
