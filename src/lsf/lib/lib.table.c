
/* $Id: lib.table.c,v 1.3 2007/08/15 22:18:51 tmizan Exp $
 * Copyright (C) 2007 Platform Computing Inc
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */
#include "lsf/lib/liblsf.h"



static hEnt     *h_findEnt(const char *key, hLinks *hList);
static unsigned int getAddr(hTab *tabPtr, const char *key);
static void     resetTab(hTab *tabPtr);

void
insList_(hLinks *elemPtr, hLinks *destPtr)
                        
                        
{

    if (elemPtr == (hLinks *) NULL || destPtr == (hLinks *) NULL
          || !elemPtr || !destPtr || (elemPtr == destPtr)) {
        return;
    }

    elemPtr->bwPtr = destPtr->bwPtr;
    elemPtr->fwPtr = destPtr;
    destPtr->bwPtr->fwPtr = elemPtr;
    destPtr->bwPtr = elemPtr;

} 

void
remList_(hLinks *elemPtr)
{

    if (elemPtr == (hLinks *) NULL || elemPtr == elemPtr->bwPtr
           || !elemPtr) {
        return;
    }

    elemPtr->fwPtr->bwPtr = elemPtr->bwPtr;
    elemPtr->bwPtr->fwPtr = elemPtr->fwPtr;

} 

void
initList_(hLinks *headPtr)
{

    if (headPtr == (hLinks *) NULL || !headPtr) {
        return;
    }

    headPtr->bwPtr = headPtr;
    headPtr->fwPtr = headPtr;

} 

void
h_initTab_(hTab *tabPtr, int numSlots)
                   
                  
{
    int i;
    hLinks *slotPtr;

    tabPtr->size = 2;
    tabPtr->numEnts = 0;

    if (numSlots <= 0)
        numSlots = DEFAULT_SLOTS;

    while (tabPtr->size < numSlots) 
        tabPtr->size <<= 1;

    tabPtr->slotPtr = (hLinks *) malloc(sizeof(hLinks) * tabPtr->size);

    for (i=0, slotPtr = tabPtr->slotPtr; i < tabPtr->size; i++, slotPtr++)
        initList_(slotPtr);
    
} 

void
h_freeTab_(hTab *tabPtr, void (*freeFunc)(void *))
{
    hLinks *hTabEnd, *slotPtr;
    hEnt    *hEntPtr;

    slotPtr = tabPtr->slotPtr;
    hTabEnd = &(slotPtr[tabPtr->size]);

    for ( ;slotPtr < hTabEnd; slotPtr++) {
        while ( slotPtr != slotPtr->bwPtr ) {
            hEntPtr = (hEnt *) slotPtr->bwPtr;
            remList_((hLinks *) hEntPtr);
            FREEUP(hEntPtr->keyname);
	    if (hEntPtr->hData != (int *)NULL) {
		if (freeFunc != NULL)
		    (*freeFunc)((void *)hEntPtr->hData);
		else {
	            free((char *) hEntPtr->hData);
		    hEntPtr->hData = (int *) NULL;
                }
            }
            free((char *) hEntPtr);
        }
    }

    free((char *) tabPtr->slotPtr);
    tabPtr->slotPtr = (hLinks *) NULL;
    tabPtr->numEnts = 0;
} 

int
h_TabEmpty_(hTab *tabPtr)
{
    return tabPtr->numEnts == 0;
}

void
h_delTab_(hTab *tabPtr)
{
    h_freeTab_(tabPtr, (HTAB_DATA_DESTROY_FUNC_T)NULL);
} 
            
hEnt *
h_getEnt_(hTab *tabPtr, const char *key)
                        
              
{
    if (tabPtr->numEnts == 0) return NULL;
    return(h_findEnt(key, &(tabPtr->slotPtr[getAddr(tabPtr, key)])));

} 

hEnt *
h_addEnt_(hTab *tabPtr, const char *key, int *newPtr)
                     
              
{
    hEnt *hEntPtr;
    int     *keyPtr;
    hLinks  *hList;

    keyPtr = (int *) key;
    hList = &(tabPtr->slotPtr[getAddr(tabPtr, (char *) keyPtr)]);
    hEntPtr = h_findEnt((char *) keyPtr, hList);

    if (hEntPtr != (hEnt *) NULL) {
        if (newPtr != NULL)
            *newPtr = false;
        return hEntPtr;
    }

    if (tabPtr->numEnts >= RESETLIMIT * tabPtr->size) {
        resetTab(tabPtr);
        hList = &(tabPtr->slotPtr[getAddr(tabPtr, (char *) keyPtr)]);
    }

    tabPtr->numEnts++;
    hEntPtr = (hEnt *) malloc(sizeof(hEnt));
    hEntPtr->keyname = putstr_((char *) keyPtr);
    hEntPtr->hData = (int *) NULL;
    insList_((hLinks *) hEntPtr, (hLinks *) hList);
    if (newPtr != NULL)
        *newPtr = true;

    return hEntPtr;

} 

hEnt *
lh_addEnt_(hTab *tabPtr, char *key, int *newPtr)
                     
              
{
    hEnt *hEntPtr;
    int     *keyPtr;
    hLinks  *hList;

    
    if (tabPtr->size > 1) tabPtr->size = 1;

    keyPtr = (int *) key;
    hList = &(tabPtr->slotPtr[getAddr(tabPtr, (char *) keyPtr)]);
    hEntPtr = h_findEnt((char *) keyPtr, hList);

    if (hEntPtr != (hEnt *) NULL) {
        if (newPtr != NULL)
            *newPtr = false;
        return hEntPtr;
    }

    tabPtr->numEnts++;
    hEntPtr = (hEnt *) malloc(sizeof(hEnt));
    hEntPtr->keyname = putstr_((char *) keyPtr);
    hEntPtr->hData = (int *) NULL;
    insList_((hLinks *) hEntPtr, (hLinks *) hList);
    
    if (newPtr != NULL)
        *newPtr = true;

    return hEntPtr;

} 

void
h_delEnt_(hTab *tabPtr, hEnt *hEntPtr)
{

    if (hEntPtr != (hEnt *) NULL) {
        remList_((hLinks *) hEntPtr);
        free(hEntPtr->keyname);
	if (hEntPtr->hData != (int *)NULL)
	    free((char *)hEntPtr->hData);
        free((char *) hEntPtr);
        tabPtr->numEnts--;
    }

} 

void
h_rmEnt_(hTab *tabPtr, hEnt *hEntPtr)
{

    if (hEntPtr != (hEnt *) NULL) {
        remList_((hLinks *) hEntPtr);
        free(hEntPtr->keyname);
        free((char *) hEntPtr);
        tabPtr->numEnts--;
    }

} 


hEnt *
h_firstEnt_(hTab *tabPtr, sTab *sPtr)
                        
                        
{

    sPtr->tabPtr = tabPtr;
    sPtr->nIndex = 0;
    sPtr->hEntPtr = (hEnt *) NULL;

    if (tabPtr->slotPtr) {
	return h_nextEnt_(sPtr);
    } else {
	
	return((hEnt *) NULL);
    }

} 

hEnt *
h_nextEnt_(sTab *sPtr)
                  
{
    hLinks *hList;
    hEnt *hEntPtr;

    hEntPtr = sPtr->hEntPtr;

    while (hEntPtr == (hEnt *) NULL || (hLinks *) hEntPtr == sPtr->hList) {
        if (sPtr->nIndex >= sPtr->tabPtr->size)
            return((hEnt *) NULL);
        hList = &(sPtr->tabPtr->slotPtr[sPtr->nIndex]);
        sPtr->nIndex++;
        if ( hList != hList->bwPtr ) {
            hEntPtr = (hEnt *) hList->bwPtr;
            sPtr->hList = hList;
            break;
        }
    }

    sPtr->hEntPtr = (hEnt *) ((hLinks *) hEntPtr)->bwPtr;
    
    return(hEntPtr);

} 

static unsigned int
getAddr(hTab *tabPtr, const char *key)
{
    unsigned int ha = 0;
   
    while (*key)
        ha = (ha * 128 + *key++) % tabPtr->size;

    return ha;

} 

static hEnt *
h_findEnt(const char *key, hLinks *hList)
{ 
    hEnt *hEntPtr;

    for ( hEntPtr = (hEnt *) hList->bwPtr;
          hEntPtr != (hEnt *) hList;
          hEntPtr = (hEnt *) ((hLinks *) hEntPtr)->bwPtr) {
        if (strcmp(hEntPtr->keyname, key) == 0)
            return(hEntPtr);
    }

    return ((hEnt *) NULL);

} 

static void
resetTab(hTab *tabPtr)
{
    int lastSize, slot;
    hLinks  *lastSlotPtr, *lastList;
    hEnt *hEntPtr;

    lastSlotPtr = tabPtr->slotPtr;
    lastSize = tabPtr->size;

    h_initTab_(tabPtr, tabPtr->size * RESETFACTOR);

    for (lastList = lastSlotPtr; lastSize > 0; lastSize--, lastList++) {
        while (lastList != lastList->bwPtr) {
            hEntPtr = (hEnt *) lastList->bwPtr;    
            remList_((hLinks *) hEntPtr);
            slot = getAddr(tabPtr, (char *) hEntPtr->keyname);
            insList_((hLinks *) hEntPtr, 
                    (hLinks *) (&(tabPtr->slotPtr[slot])));
            tabPtr->numEnts++;
        }
    }

    free((char *) lastSlotPtr);

} 

void
h_delRef_(hTab *tabPtr, hEnt *hEntPtr)
{
    if (hEntPtr != (hEnt *) NULL) {
        remList_((hLinks *) hEntPtr);
        free(hEntPtr->keyname);
        free((char *) hEntPtr);
        tabPtr->numEnts--;
    }

} 

void
h_freeRefTab_(hTab *tabPtr)
{
    hLinks *hTabEnd, *slotPtr;
    hEnt    *hEntPtr;

    slotPtr = tabPtr->slotPtr;
    hTabEnd = &(slotPtr[tabPtr->size]);

    for ( ;slotPtr < hTabEnd; slotPtr++) {
        while ( slotPtr != slotPtr->bwPtr ) {
            hEntPtr = (hEnt *) slotPtr->bwPtr;
            remList_((hLinks *) hEntPtr);
            FREEUP(hEntPtr->keyname);
            free((char *) hEntPtr);
        }
    }

    free((char *) tabPtr->slotPtr);
    tabPtr->slotPtr = (hLinks *) NULL;
    tabPtr->numEnts = 0;

} 
