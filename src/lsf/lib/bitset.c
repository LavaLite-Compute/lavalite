/* $Id: bitset.c,v 1.4 2007/08/15 22:18:49 tmizan Exp $
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

#include "lsf/lib/ll_sys.h"
#include "lsf/lib/list.h"
#include "lsf/lib/bitset.h"
__thread int bitseterrno;
static void setObserverDestroy(LS_BITSET_T *);
static void observerDestroy(LS_BITSET_OBSERVER_T *);

LS_BITSET_T *setCreate(const int size, int (*directFunction)(void *),
                       void *(*inverseFunction)(int), char *caller)
{
    static char *fname = "setCreate";
    int sz;
    LS_BITSET_T *this;

    this = (LS_BITSET_T *) calloc(1, sizeof(LS_BITSET_T));
    if (!this) {
        bitseterrno = LS_BITSET_ERR_NOMEM;
        ls_syslog(LOG_ERR, "%s %s", fname, setPerror(bitseterrno));
        return NULL;
    }

    this->setDescription = putstr_(caller);

    sz = (size > 0) ? size : SET_DEFAULT_SIZE;
    this->setWidth = (sz + WORDLENGTH - 1) / WORDLENGTH;
    this->setSize = (this->setWidth) * WORDLENGTH;

    this->setNumElements = 0;

    this->bitmask =
        (unsigned int *) calloc(this->setWidth, sizeof(unsigned int));
    if (!this->bitmask) {
        bitseterrno = LS_BITSET_ERR_NOMEM;
        return NULL;
    }

    if (directFunction)
        this->getIndexByObject = directFunction;
    else
        this->getIndexByObject = NULL;

    if (inverseFunction)
        this->getObjectByIndex = inverseFunction;
    else
        this->getObjectByIndex = NULL;

    return this;
}

LS_BITSET_T *simpleSetCreate(const int size, char *caller)
{
    return (setCreate(size, NULL, NULL, caller));
}

int setDestroy(LS_BITSET_T *this)
{
    static char fname[] = "setDestroy";

    if (!SET_IS_VALID(this)) {
        ls_syslog(LOG_ERR,
                  "%s: expected a non-NULL set pointer but got a NULL one",
                  fname);
        bitseterrno = LS_BITSET_ERR_BADARG;
        return -1;
    }

    if (this->allowObservers == true) {
        setObserverDestroy(this);
    }

    free(this->setDescription);
    free(this->bitmask);
    free(this);
    this = NULL;

    return 0;
}

static void setObserverDestroy(LS_BITSET_T *set)
{
    if (set->allowObservers == false) {
        return;
    }

    listDestroy(set->observers, (LIST_ENTRY_DESTROY_FUNC_T) &observerDestroy);
}

static void observerDestroy(LS_BITSET_OBSERVER_T *observer)
{
    free(observer->name);
    free(observer);
    observer = NULL;
}

LS_BITSET_T *setDup(LS_BITSET_T *this)
{
    static char *fname = "setCreate";
    LS_BITSET_T *set;

    set = (LS_BITSET_T *) calloc(1, sizeof(LS_BITSET_T));
    if (!set) {
        bitseterrno = LS_BITSET_ERR_NOMEM;
        ls_syslog(LOG_ERR, "%s %s", fname, setPerror(bitseterrno));
        return NULL;
    }

    memcpy((void *) set, (void *) this, sizeof(LS_BITSET_T));
    set->setDescription = putstr_(this->setDescription);
    set->bitmask =
        (unsigned int *) calloc(this->setWidth, sizeof(unsigned int));

    memcpy((void *) set->bitmask, this->bitmask,
           this->setWidth * sizeof(unsigned int));

    return set;
}

bool_t setTestValue(LS_BITSET_T *this, const int value)
{
    int word;
    int offset;
    bool_t trueORfalse;

    word = SET_GET_WORD(value);

    offset = SET_GET_BIT_IN_WORD(value);

    trueORfalse = ((*(this->bitmask + word) & (SET_BIT_ON << offset)) != 0);

    return trueORfalse;
}

bool_t setIsMember(LS_BITSET_T *this, void *obj)
{
    static char fname[] = "setIsMember()";
    int value;

    if (!SET_IS_VALID(this)) {
        bitseterrno = LS_BITSET_ERR_BADARG;
        return false;
    }

    if (SET_IS_EMPTY(this)) {
        bitseterrno = LS_BITSET_ERR_SETEMPTY;
        return false;
    }

    if (this->getIndexByObject == NULL) {
        value = *(int *) obj;
    } else {
        value = (*this->getIndexByObject)(obj);
        if (value < 0) {
            bitseterrno = LS_BITSET_ERR_FUNC;
            ls_syslog(LOG_ERR, "%s %s", fname, setPerror(bitseterrno));
            return -1;
        }
    }

    if (value >= this->setSize)
        return false;

    return (setTestValue(this, value));
}

int setAddElement(LS_BITSET_T *this, void *obj)
{
    static char fname[] = "setAddElement";
    intptr_t value;
    int word;
    int offset;

    if (!SET_IS_VALID(this)) {
        bitseterrno = LS_BITSET_ERR_BADARG;
        return -1;
    }

    if (this->getIndexByObject == NULL) {
        value = *(int *) obj;
    } else {
        value = (*this->getIndexByObject)(obj);
        if (value < 0) {
            bitseterrno = LS_BITSET_ERR_FUNC;
            return -1;
        }
    }

    if (value >= this->setSize) {
        ls_syslog(LOG_DEBUG3, "%s: realloc set when size=%d", fname, value);
        setEnlarge(this, value);

        if (this == NULL)
            return -1;
    } else {
        if (setTestValue(this, value) == true) {
            bitseterrno = LS_BITSET_ERR_ISALREDY;
            return -1;
        }
    }

    word = SET_GET_WORD(value);
    offset = SET_GET_BIT_IN_WORD(value);

    *(this->bitmask + word) |= (SET_BIT_ON << offset);

    this->setNumElements++;

    if (this->allowObservers) {
        LS_BITSET_EVENT_T event;

        event.type = LS_BITSET_EVENT_ENTER;
        event.entry = (void *) obj;

        setNotifyObservers(this, &event);
    }

    return 0;
}

int setRemoveElement(LS_BITSET_T *this, void *obj)
{
    static char fname[] = "setRemoveElement";
    int value;
    int word;
    int offset;

    if (!SET_IS_VALID(this)) {
        ls_syslog(LOG_ERR,
                  "%s: expected a non-NULL set pointer but got a NULL one",
                  fname);
        bitseterrno = LS_BITSET_ERR_BADARG;
        return -1;
    }

    if (SET_IS_EMPTY(this)) {
        bitseterrno = LS_BITSET_ERR_SETEMPTY;
        return -1;
    }

    if (this->getIndexByObject == NULL) {
        value = *(int *) obj;
    } else {
        value = this->getIndexByObject(obj);
        if (value < 0) {
            bitseterrno = LS_BITSET_ERR_FUNC;
            return -1;
        }
    }

    if (value >= this->setSize)
        return false;

    word = SET_GET_WORD(value);
    offset = SET_GET_BIT_IN_WORD(value);

    *(this->bitmask + word) &= ~(SET_BIT_ON << offset);

    this->setNumElements--;

    return 0;
}

int setClear(LS_BITSET_T *this)
{
    if (!SET_IS_VALID(this)) {
        ls_syslog(
            LOG_ERR,
            "setClear: expected a non-NULL set pointer but got a NULL one");
        bitseterrno = LS_BITSET_ERR_BADARG;
        return -1;
    }

    memset(this->bitmask, 0, this->setWidth * sizeof(unsigned int));

    this->setNumElements = 0;

    return 0;
}

int getNum1BitsInWord(unsigned int *word)
{
    unsigned char *p;
    int numElements = 0;
    int i;
    static unsigned char nbits[] = {
        0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 1, 2, 2, 3, 2, 3, 3, 4,
        2, 3, 3, 4, 3, 4, 4, 5, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 1, 2, 2, 3, 2, 3, 3, 4,
        2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6,
        4, 5, 5, 6, 5, 6, 6, 7, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5,
        3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6,
        4, 5, 5, 6, 5, 6, 6, 7, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8};

    p = (unsigned char *) word;
    numElements = 0;

    for (i = sizeof(unsigned int); --i >= 0;)
        numElements += nbits[*p++];

    return numElements;
}

int setGetNumElements(LS_BITSET_T *this)
{
    static char fname[] = "setGetNumElements";
    int i;
    unsigned char *p;
    int numElements = 0;
    static unsigned char nbits[] = {
        0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 1, 2, 2, 3, 2, 3, 3, 4,
        2, 3, 3, 4, 3, 4, 4, 5, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 1, 2, 2, 3, 2, 3, 3, 4,
        2, 3, 3, 4, 3, 4, 4, 5, 2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6,
        4, 5, 5, 6, 5, 6, 6, 7, 1, 2, 2, 3, 2, 3, 3, 4, 2, 3, 3, 4, 3, 4, 4, 5,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 2, 3, 3, 4, 3, 4, 4, 5,
        3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        2, 3, 3, 4, 3, 4, 4, 5, 3, 4, 4, 5, 4, 5, 5, 6, 3, 4, 4, 5, 4, 5, 5, 6,
        4, 5, 5, 6, 5, 6, 6, 7, 3, 4, 4, 5, 4, 5, 5, 6, 4, 5, 5, 6, 5, 6, 6, 7,
        4, 5, 5, 6, 5, 6, 6, 7, 5, 6, 6, 7, 6, 7, 7, 8};

    if (!SET_IS_VALID(this)) {
        ls_syslog(LOG_ERR,
                  "%s: expected a non-NULL set pointer but got a NULL one",
                  fname);
        bitseterrno = LS_BITSET_ERR_BADARG;
        return -1;
    }

    p = (unsigned char *) this->bitmask;

    for (i = BYTES_IN_MASK(this->setWidth); --i >= 0;)
        numElements += nbits[*p++];

    return numElements;
}

LS_BITSET_T *setEnlarge(LS_BITSET_T *this, unsigned int newSize)
{
    unsigned int oldWidth;

    oldWidth = this->setWidth;
    this->setWidth += newSize + SET_WORD_DEFAULT_EXTENT;

    this->bitmask = (unsigned int *) realloc(
        this->bitmask, sizeof(unsigned int) * this->setWidth);
    if (this->bitmask == NULL) {
        bitseterrno = LS_BITSET_ERR_NOMEM;
        return NULL;
    }

    memset(this->bitmask + oldWidth, 0,
           sizeof(unsigned int) * (this->setWidth - oldWidth));

    this->setSize = (this->setWidth) * WORDLENGTH;

    return this;
}
void setOperate(LS_BITSET_T *dest, LS_BITSET_T *src, int op)
{
    unsigned int *d;
    unsigned int *s;
    int nwords;
    int tail;

    if (op != LS_SET_UNION && op != LS_SET_INTERSECT &&
        op != LS_SET_DIFFERENCE && op != LS_SET_ASSIGN) {
        bitseterrno = LS_BITSET_ERR_EINVAL;
        return;
    }

    if (dest->setWidth < src->setWidth)
        setEnlarge(dest, src->setWidth);
    if (dest == NULL)
        return;

    nwords = src->setWidth;
    tail = dest->setWidth - nwords;

    d = dest->bitmask;
    s = src->bitmask;

    switch (op) {
    case LS_SET_UNION:

        while (--nwords >= 0)
            *d++ |= *s++;
        break;

    case LS_SET_INTERSECT:

        while (--nwords >= 0)
            *d++ &= *s++;
        while (--tail >= 0)
            *d++ = 0;
        break;

    case LS_SET_DIFFERENCE:

        while (--nwords >= 0)
            *d++ ^= *s++;
        break;

    case LS_SET_ASSIGN:

        while (--nwords >= 0)
            *d++ = *s++;
        while (--tail >= 0)
            *d++ = 0;
        break;
    }

    return;
}

void setCat(LS_BITSET_T *set, char *buffer, int bufferSize,
            char *(*catFunc)(void *, void *), void *hint)
{
    static char fname[] = "setCat";
    LS_BITSET_ITERATOR_T iter;
    void *entry;
    int curSize;

    if (!set || !catFunc) {
        ls_syslog(LOG_ERR, "%s: expected a non-NULL set but get a NULL one",
                  fname);
        return;
    }

    buffer[0] = '\000';

    BITSET_ITERATOR_ZERO_OUT(&iter);
    setIteratorAttach(&iter, set, fname);

    curSize = 0;
    for (entry = setIteratorBegin(&iter);
         entry != NULL && (setIteratorIsEndOfSet(&iter) == false);
         entry = setIteratorGetNextElement(&iter)) {
        char *str;

        str = (*catFunc)(entry, hint);
        if (str == NULL) {
            ls_syslog(LOG_ERR, "%s: catFunc returned NULL string", fname);
            continue;
        }

        if (curSize + strlen(str) > bufferSize - 1) {
            ls_syslog(LOG_ERR, "%s: the provided buffer is not big enough",
                      fname);
            break;
        }

        strcat(buffer, str);
        curSize += strlen(str);
    }
}

LS_BITSET_ITERATOR_T *setIteratorCreate(LS_BITSET_T *this)
{
    static char fname[] = "setIteratorcreate";
    LS_BITSET_ITERATOR_T *iter = NULL;

    if (!SET_IS_VALID(this)) {
        ls_syslog(LOG_ERR,
                  "%s: expected a non-NULL set pointer but got a NULL one",
                  fname);
        bitseterrno = LS_BITSET_ERR_BADARG;
        return NULL;
    }

    iter = (LS_BITSET_ITERATOR_T *) calloc(1, sizeof(LS_BITSET_ITERATOR_T));
    if (!iter) {
        bitseterrno = LS_BITSET_ERR_NOMEM;
        return NULL;
    }
    iter->this = this;
    iter->setCurrentBit = 0;
    iter->setSize = this->setSize;

    return iter;
}

int setIteratorAttach(LS_BITSET_ITERATOR_T *iter, LS_BITSET_T *set, char *func)
{
    static char fname[] = "setIteratorAttach()";

    if (!iter || !set) {
        ls_syslog(LOG_ERR,
                  "%s: expected a non-NULL set pointer but got a NULL one ",
                  fname);
        bitseterrno = LS_BITSET_ERR_BADARG;
        return -1;
    }

    iter->this = set;
    iter->setCurrentBit = 0;
    iter->setSize = set->setSize;

    return 0;
}
void setIteratorDetach(LS_BITSET_ITERATOR_T *iter)
{
    if (!SET_IS_VALID(iter->this)) {
        bitseterrno = LS_BITSET_ERR_BADARG;
        return;
    }

    iter->this = NULL;
    iter->setCurrentBit = 0;
    iter->setSize = 0;
}
void *setIteratorBegin(LS_BITSET_ITERATOR_T *iter)
{
    void *object;

    object = (void *) setIteratorGetNextElement(iter);

    return object;
}

/* Bug. The whole iterator is garbage anyway, legacy fairshare data structure
 * are not in use lava.
 */
void *setIteratorGetNextElement(LS_BITSET_ITERATOR_T *iter)
{
    static char fname[] = "setIteratorGetNextElement";
    void *object;

    if (!SET_IS_VALID(iter->this)) {
        ls_syslog(LOG_ERR,
                  "%s: expected a non-NULL set pointer but got a NULL one",
                  fname);
        bitseterrno = LS_BITSET_ERR_BADARG;
        return NULL;
    }

    if (SET_IS_EMPTY(iter->this)) {
        bitseterrno = LS_BITSET_ERR_SETEMPTY;
        return NULL;
    }

    while (iter->setCurrentBit < iter->this->setSize) {
        if (setTestValue(iter->this, iter->setCurrentBit) == true) {
            if (iter->this->getObjectByIndex == NULL)
                object = (void *) (uintptr_t) iter->setCurrentBit;
            else {
                object = (*iter->this->getObjectByIndex)(iter->setCurrentBit);
                if (!object) {
                    bitseterrno = LS_BITSET_ERR_FUNC;
                    return NULL;
                }
            }

            ++iter->setCurrentBit;
            return object;
        }
        ++iter->setCurrentBit;
    }

    return NULL;
}

bool_t setIteratorIsEndOfSet(LS_BITSET_ITERATOR_T *iter)
{
    static char fname[] = "setIteratorIsEndOfSet";

    if (!SET_IS_VALID(iter->this)) {
        ls_syslog(LOG_ERR,
                  "%s: expected a non-NULL set pointer but got a NULL one",
                  fname);
        bitseterrno = LS_BITSET_ERR_BADARG;
        return false;
    }

    if (iter->setCurrentBit >= iter->setSize)
        return true;
    else
        return false;
}
void setIteratorDestroy(LS_BITSET_ITERATOR_T *iter)
{
    static char fname[] = "setIteratorDestroy";

    if (!SET_IS_VALID(iter->this)) {
        ls_syslog(LOG_ERR,
                  "%s: expected a non-NULL set pointer but got a NULL one",
                  fname);
        bitseterrno = LS_BITSET_ERR_BADARG;
        return;
    }

    setDestroy(iter->this);
    free(iter);
    iter = NULL;
}

static char *bitSetErrList[] = {"No Error",
                                "Bad Arguments",
                                "Set Is Empty",
                                "Memory allocation failed",
                                "User function failed",
                                "Object alredy in set",
                                "Invalid set operation",
                                "Observer permission denied",
                                "Last Error (no error)"};

char *setPerror(int errorNumber)
{
    static char buf[216];

    if (errorNumber < 0 || errorNumber > (int) LS_BITSET_ERR_LAST) {
        sprintf(buf, " Unknown error number %d", errorNumber);
        return buf;
    }

    return (bitSetErrList[errorNumber]);
}

int setAllowObservers(LS_BITSET_T *set)
{
    static char fname[] = "setAllowObservers";

    if (!set) {
        ls_syslog(LOG_ERR, "%s: expected a non null set but got a null one",
                  fname);
        bitseterrno = LS_BITSET_ERR_BADARG;
        return -1;
    }

    if (set->allowObservers)

        return 0;

    set->allowObservers = true;
    set->observers = listCreate("Observer list");

    if (!set->observers)

        return -1;

    return 0;
}

LS_BITSET_OBSERVER_T *setObserverCreate(char *name, void *extra,
                                        LS_BITSET_ENTRY_SELECT_OP_T select, ...)
{
    LS_BITSET_OBSERVER_T *observer;
    LS_BITSET_EVENT_TYPE_T etype;
    LS_BITSET_EVENT_CALLBACK_FUNC_T callback;
    va_list ap;

    observer = (LS_BITSET_OBSERVER_T *) calloc(1, sizeof(LS_BITSET_OBSERVER_T));
    if (observer == NULL) {
        bitseterrno = LS_BITSET_ERR_NOMEM;
        goto Fail;
    }

    observer->name = putstr_(name);
    observer->select = select;
    observer->extra = extra;

    va_start(ap, select);

    while (true) {
        etype = va_arg(ap, LS_BITSET_EVENT_TYPE_T);

        if (etype == LS_BITSET_EVENT_NULL)
            break;

        callback = va_arg(ap, LS_BITSET_EVENT_CALLBACK_FUNC_T);

        switch (etype) {
        case (int) LS_BITSET_EVENT_ENTER:
            observer->enter = callback;
            break;

        case (int) LS_BITSET_EVENT_LEAVE:
            observer->leave_ = callback;
            break;

        default:
            ls_syslog(LOG_ERR, "%s: invalid event ID %d from the invoker",
                      __func__, (int) etype);
            bitseterrno = LS_BITSET_ERR_BADARG;
            FREEUP(observer->name);
            goto Fail;
        }
    }

    return observer;

Fail:
    FREEUP(observer);
    return (LS_BITSET_OBSERVER_T *) NULL;
}

int setObserverAttach(LS_BITSET_OBSERVER_T *observer, LS_BITSET_T *set)
{
    static char fname[] = "setObserverAttach";
    int rc;

    if (!observer || !set) {
        ls_syslog(LOG_ERR, "%s: expected a non-null set but got a null one",
                  fname);
        bitseterrno = LS_BITSET_ERR_BADARG;
        return -1;
    }

    if (!set->allowObservers) {
        ls_syslog(LOG_ERR, "%s: list \"%s\" does not accept observers", fname,
                  set->setDescription);
        bitseterrno = (int) LS_BITSET_ERR_NOOBSVR;
        return -1;
    }

    rc = listInsertEntryBefore(set->observers, (LIST_ENTRY_T *) set->observers,
                               (LIST_ENTRY_T *) observer);
    if (rc < 0)
        return rc;

    observer->set = set;
    return 0;
}

int setNotifyObservers(LS_BITSET_T *set, LS_BITSET_EVENT_T *event)
{
    static char fname[] = "setNotifyObservers";
    LS_BITSET_OBSERVER_T *observer;
    LIST_ITERATOR_T iter;

    if (!set || !event) {
        ls_syslog(LOG_ERR, "%s: expected a non-null set but got a null one",
                  fname);
        bitseterrno = LS_BITSET_ERR_BADARG;
        return -1;
    }

    listIteratorAttach(&iter, set->observers);

    for (observer = (LS_BITSET_OBSERVER_T *) listIteratorGetCurEntry(&iter);
         !listIteratorIsEndOfList(&iter);
         listIteratorNext(&iter, (LIST_ENTRY_T **) &observer)) {
        ls_syslog(LOG_DEBUG3, "\
%s: Notifying observer \"%s\" of the event <%d>",
                  fname, observer->name, (int) event->type);

        if (observer->select != NULL) {
            if (!(*observer->select)(observer->extra, event))
                continue;
        }

        switch (event->type) {
        case (int) LS_BITSET_EVENT_ENTER:
            if (observer->enter)
                (*observer->enter)(set, observer->extra, event);
            break;
        case (int) LS_BITSET_EVENT_LEAVE:
            if (observer->leave_)
                (*observer->leave_)(set, observer->extra, event);
            break;
        default:
            ls_syslog(LOG_ERR, "%s: invalide event type %d", fname,
                      event->type);
            bitseterrno = LS_BITSET_ERR_BADARG;
            return -1;
        }
    }

    return 0;
}

int setDumpSet(LS_BITSET_T *set, char *caller)
{
    static char fname[] = "setDumpSet";
    char buf[3072];
    int i;
    int j;

    if (!SET_IS_VALID(set)) {
        bitseterrno = LS_BITSET_ERR_BADARG;
        ls_syslog(LOG_ERR, "%s caller %s %s", fname, caller,
                  setPerror(bitseterrno));
        return -1;
    }

    ls_syslog(LOG_ERR, "%s Dumping set <%lx> <%s>", fname, (long) set,
              set->setDescription);

    ls_syslog(LOG_ERR, "%s Begin DumpMask", fname);
    memset(buf, 0, sizeof(buf));

    for (i = 0; i < set->setWidth; i++) {
        sprintf(buf + strlen(buf), "\
word <%d> decimal <%d> bits: ",
                i, set->bitmask[i]);

        for (j = 0; j < 8; j++) {
            (*(set->bitmask + i) & (SET_BIT_ON << j))
                ? sprintf(buf + strlen(buf), "1")
                : sprintf(buf + strlen(buf), "0");
        }

        sprintf(buf + strlen(buf), "\n");
    }

    ls_syslog(LOG_ERR, "%s", buf);
    ls_syslog(LOG_ERR, "%s End DumpMask", fname);

    return 0;
}
