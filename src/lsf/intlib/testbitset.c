/* $Id: testbitset.c,v 1.3 2007/08/15 22:18:49 tmizan Exp $
 * Copyright (C) 2007 Platform Computing Inc
 * Copyright (C) LavaLite Contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 USA
 *
 */
#if _BITSET_TEST_

#include <stdio.h>
#include "lsf.h"
#include "lsf/intlib/llsys.h"

static void test_1(void);
static void test_2(void);
static void test_3(void);
static void test_4(void);

extern int bitseterrno;

void main(int argc, char **argv)
{
    char fname[] = "main";

    if (ls_initdebug(argv[0]) < 0) {
        ls_perror("ls_initdebug");
        exit(-1);
    }
    test_4();

    exit(0);
}

#define VECT_SIZE 15
static void test_1()
{
    static char fname[] = "test_1";
    int vect[VECT_SIZE];
    LS_BITSET_T *set;
    int i;

    for (i = 0; i < VECT_SIZE; i++)
        vect[i] = i + 1;

    set = simpleSetCreate(VECT_SIZE, fname);
    if (!set) {
        ls_syslog(LOG_ERR, "%s failed in creating set", fname);
        exit(-1);
    }

    for (i = 0; i < VECT_SIZE; i++) {
        if (setAddElement(set, &vect[i]) < 0) {
            ls_syslog(LOG_ERR, "%s setAddElement failed for i=%d vect[i]=%d",
                      fname, i, vect[i]);
            setDumpSet(set, fname);
            exit(-1);
        }
    }

    setDumpSet(set, fname);

    ls_syslog(LOG_ERR, "%s setGetNumElements=%d", fname,
              setGetNumElements(set));

    for (i = 0; i < VECT_SIZE; i++) {
        if (setRemoveElement(set, &vect[i]) < 0) {
            ls_syslog(LOG_ERR, "%s setAddElement failed for i=%d vect[i]=%d",
                      fname, i, vect[i]);
            setDumpSet(set, fname);
            exit(-1);
        }
    }
    setDumpSet(set, fname);
}

static void test_2()
{
    static char fname[] = "test_2";
    int vect[VECT_SIZE];
    LS_BITSET_T *set;
    LS_BITSET_ITERATOR_T *iterator;
    int i;
    int *p;

    for (i = 0; i < VECT_SIZE; i++)
        vect[i] = i + 1;

    set = simpleSetCreate(VECT_SIZE, fname);
    if (!set) {
        ls_syslog(LOG_ERR, "%s failed in creating set", fname);
        exit(-1);
    }

    for (i = 0; i < VECT_SIZE; i++) {
        if (setAddElement(set, &vect[i]) < 0) {
            ls_syslog(LOG_ERR, "%s setAddElement failed for i=%d vect[i]=%d",
                      fname, i, vect[i]);
            setDumpSet(set, fname);
            exit(-1);
        }
    }
    iterator = setIteratorCreate(set);
    if (!iterator) {
        ls_syslog(LOG_ERR, "%s setIteratorCreate failed", fname);
        exit(-1);
    }

    while (p = setIteratorGetNextElement(iterator))
        ls_syslog(LOG_ERR, "%s next element in set %d", fname, *p);

    return;
}

static struct X {
    int index;
    char *blaBla;
};
#define OBJ_SIZE 2000
static struct X objs[OBJ_SIZE];

struct X **table;

static int fun(void *hux)
{
    struct X p;

    memcpy(&p, (struct X *) hux, sizeof(struct X));

    return (p.index);
}

static void *gun(int zug)
{
    return (table[zug]);
}

static void test_3()
{
    static char fname[] = "test_3";
    int (*directFun)(void *);
    void *(*inverseFun)(int);
    int i;
    LS_BITSET_T *set;
    LS_BITSET_ITERATOR_T *iterator;
    struct X *gimmeObject;

    directFun = fun;
    inverseFun = gun;

    table = (struct X **) malloc(sizeof(struct X) * OBJ_SIZE);
    if (!table) {
        ls_syslog(LOG_ERR, "%s failed malloc for %d bytes", fname, OBJ_SIZE);
        exit(-1);
    }

    for (i = 0; i < OBJ_SIZE; i++) {
        char buf[1024];
        memset(buf, 0, sizeof(buf));
        sprintf(buf, "You couldn't disagree %d", i);
        objs[i].index = i;
        objs[i].blaBla = strdup(buf);
        table[i] = &objs[i];
    }

    set = setCreate(OBJ_SIZE, directFun, inverseFun, fname);
    if (!set) {
        ls_syslog(LOG_ERR, "%s failed allocating %d bytes", fname, OBJ_SIZE);
        exit(-1);
    }

    for (i = 0; i < OBJ_SIZE; i++) {
        if (setAddElement(set, table[i]) < 0) {
            ls_syslog(LOG_ERR, "%s setAddElement failed, index=%d", fname, i);
            exit(-1);
        }
        ls_syslog(LOG_ERR, "%s adding element obj=%x  index=%d", fname,
                  table[i], i);
    }

    iterator = setIteratorCreate(set);
    if (!iterator) {
        ls_syslog(LOG_ERR, "%s setIteratorCreate() failed", fname);
        exit(-1);
    }

    bitseterrno = LS_BITSET_ERR_NOERR;
    while (gimmeObject = (struct X *) setIteratorGetNextElement(iterator)) {
        ls_syslog(LOG_ERR, "%s Gotta object with index %d = <%s>", fname,
                  gimmeObject->index, gimmeObject->blaBla);
    }
    if (bitseterrno != LS_BITSET_ERR_NOERR) {
        ls_syslog(LOG_ERR, "%s %s", fname, setPerror(bitseterrno));
        exit(-1);
    }

    return;
}

static void test_4()
{
    static char fname[] = "test_4()";
    LS_BITSET_T *set;
    LS_BITSET_T *set1;
    LS_BITSET_ITERATOR_T iter;
    int cc;
    int i;
    int *p;

    set = simpleSetCreate(1, "Just one");

    for (i = 0; i < 100; i++) {
        cc = setAddElement(set, (void *) i);
        if (cc < 0) {
            ls_syslog(LOG_ERR, "\
%s setAddElement() failed fuck item=%d",
                      fname, i);
            exit(-1);
        }
    }

    cc = setGetNumElements(set);
    printf("Suck element has %d members\n", cc);

    set1 = simpleSetCreate(3, "do od");
    for (i = 0; i < 30000; i++) {
        cc = setAddElement(set1, (void *) i);
        if (cc < 0) {
            ls_syslog(LOG_ERR, "\
%s setAddElement() failed fuck item=%d",
                      fname, i);
            exit(-1);
        }
    }
    cc = setGetNumElements(set1);
    printf("Fuck element has %d members\n", cc);

    setOperate(set, set1, LS_SET_UNION);

    cc = setIteratorAttach(&iter, set1);
    if (cc < 0) {
        ls_syslog(LOG_ERR, "%s %s", fname, setPerror(bitseterrno));
        exit(-1);
    }

    for (i = (int) setIteratorBegin(&iter);
         setIteratorIsEndOfSet(&iter) == false;
         i = (int) setIteratorGetNextElement(&iter))
        ;

    setIteratorDetach(&iter);
    setDestroy(set);
    setDestroy(set1);
}
#endif
