/* $Id: mbd.grp.c,v 1.7 2007/08/15 22:18:45 tmizan Exp $
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

#include <stdlib.h>
#include "mbd.h"

#define NL_SETN		10	

struct uStackEntry {
    struct gData*  myGData;
    struct uData*  myUData;
    struct gData*  parentGData;
    struct uData*  parentUData;
};

static void                    catMembers (struct gData*, char*, char);
static void                    traverseGroupTree(struct gData*);
static void                    checkuDataSet(struct uData*);
static struct groupInfoEnt*    getGroupInfoEnt(char**, int*, char);
static struct groupInfoEnt*    getGroupInfoEntFromUdata(struct uData*, char);
static void 	    	       getAllHostGroup(struct gData **, int, int,
		    	    	    	       struct groupInfoEnt *, int *);
static void    	       	       getSpecifiedHostGroup(char **, int, 
						     struct gData **, int, int,
						     struct groupInfoEnt *, 
						     int *);
static void    	       	       copyHostGroup(struct gData *, int, 
					     struct groupInfoEnt *);

LS_BITSET_T                    *allUsersSet = NULL;

LS_BITSET_T                    *uGrpAllSet;

LS_BITSET_T                    *uGrpAllAncestorSet;

int 
checkGroups (struct infoReq *groupInfoReq, 
	     struct groupInfoReply *groupInfoReply)
{
    int                   recursive;
    struct gData **        gplist;
    int                    ngrp;    

    if (groupInfoReq->options & HOST_GRP) {
        gplist = hostgroups;
        ngrp = numofhgroups;
    } else if (groupInfoReq->options & USER_GRP) {
        gplist = usergroups;
        ngrp = numofugroups;
    } else {
        return LSBE_LSBLIB;
    }

    if (groupInfoReq->options & GRP_RECURSIVE)
        recursive = TRUE;
    else
        recursive = FALSE;

    
    if (groupInfoReq->options & HOST_GRP) {
	if (groupInfoReq->options & GRP_ALL) {
	    getAllHostGroup(gplist, ngrp, recursive, groupInfoReply->groups, 
			    &groupInfoReply->numGroups);
	} else {
	    getSpecifiedHostGroup(groupInfoReq->names, groupInfoReq->numNames, 
				  gplist, ngrp, recursive, 
				  groupInfoReply->groups, 
				  &groupInfoReply->numGroups);

	    if (groupInfoReply->numGroups == 0 || 
		groupInfoReply->numGroups != groupInfoReq->numNames) {
		return (LSBE_BAD_GROUP);
	    }
	}
    }

    
    if (groupInfoReq->options & USER_GRP) {

	if (groupInfoReq->options & GRP_ALL) {
	    int num = 0;

	    groupInfoReply->groups = getGroupInfoEnt(NULL, &num, recursive);
	    groupInfoReply->numGroups = num;
		
	} else {
	    int num = 0;
	    
	    num = groupInfoReq->numNames;
	    groupInfoReply->groups = getGroupInfoEnt(groupInfoReq->names, 
						     &num, 
						     recursive);
	    groupInfoReply->numGroups = num;
	    if ( num == 0 || num != groupInfoReq->numNames )
		 
		return (LSBE_BAD_GROUP);
	} 
    }

    return (LSBE_NO_ERROR);

} 
static struct groupInfoEnt *
getGroupInfoEnt(char **groups, int *num, char recursive)
{
    static char              fname[] = "getGroupInfoEnt()"; 
    struct groupInfoEnt *    groupInfoEnt;

    if (groups == NULL) {
	struct uData *u;
	int i;
	
	
	groupInfoEnt = (struct groupInfoEnt *)
	    my_calloc(numofugroups,
		      sizeof(struct groupInfoEnt),
			     fname);

	i = 0;
	
	for (i = 0; i < numofugroups; i++) {
	    struct groupInfoEnt *g;

	    u = getUserData(usergroups[i]->group);
	    
	    g = getGroupInfoEntFromUdata(u, recursive);
	    if (g == NULL) {
		FREEUP(groupInfoEnt);
		lsberrno = LSBE_NO_MEM;
		return (NULL);
	    }
	    memcpy(groupInfoEnt + i, g, sizeof(struct groupInfoEnt));
	}
	  
	*num = numofugroups;        
    } else {             
	struct uData * u;
	int            j = 0;
	int            k;
	int            i;

	groupInfoEnt = (struct groupInfoEnt *)
	    my_calloc(*num, sizeof(struct groupInfoEnt), fname);

	for (i = 0; i < *num; i++) {
	    struct groupInfoEnt *g;
	    bool_t  validUGrp = FALSE;

	    
	    for (k = 0; k < numofugroups; k++) {
		if (strcmp(usergroups[k]->group, groups[i]) == 0) {
		    validUGrp = TRUE;
		    break;
		}
	    }

	    if (validUGrp == FALSE) {
	        struct groupInfoEnt *  tmpgroupInfoEnt;
		lsberrno = LSBE_BAD_USER;
		*num = i;   

		if ( i == 0 )  {  
		    
		    FREEUP(groupInfoEnt);
		    return NULL;
		}

		tmpgroupInfoEnt = (struct groupInfoEnt *)
			my_calloc(i, sizeof(struct groupInfoEnt), fname);

		for(k=0; k < i; k++)
		    tmpgroupInfoEnt[k] = groupInfoEnt[k];
		FREEUP(groupInfoEnt);
		groupInfoEnt = tmpgroupInfoEnt;

		return (groupInfoEnt);
	    }

	    u = getUserData(groups[i]);
	    if (u == NULL) {
		ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL, 
		    fname, "getUserData", groups[i]);
		continue;
	    }
	    
	    g = getGroupInfoEntFromUdata(u, recursive);
	    if (g == NULL){
		lsberrno = LSBE_NO_MEM;
		FREEUP(groupInfoEnt);
		return (NULL);
	    }
	    
	    memcpy(groupInfoEnt + j, g, sizeof(struct groupInfoEnt));
	    
	    j++;           
	}
	*num = j;          
    }
	    
    return (groupInfoEnt);

} 
static struct groupInfoEnt *
getGroupInfoEntFromUdata(struct uData *u, char recursive)
{
    static struct groupInfoEnt group;

    memset((struct groupInfoEnt *)&group, 0, sizeof(struct groupInfoEnt));

    group.group = u->user;
    group.memberList = getGroupMembers(u->gData, 
				      recursive);
    return (&group);

} 
char *
getGroupMembers (struct gData *gp, char r)
{
    char *members;
    int numMembers;

    numMembers = sumMembers(gp, r, 1);
    if (numMembers == 0) {
	members = safeSave("all");        
	return (members);
    }

    members = my_calloc(numMembers, MAX_LSB_NAME_LEN, "getGroupMembers");
    members[0] = '\0';
    catMembers(gp, members, r);
    return members;

} 

int 
sumMembers (struct gData *gp, char r, int first)
{
    int i;
    static int num;
   
    if (first)
	num = 0;

    if (gp->numGroups == 0 && gp->memberTab.numEnts == 0 && r)
        return (0);          

    num += gp->memberTab.numEnts;

    if (!r) 
	num += gp->numGroups;
    else {
	for (i=0; i<gp->numGroups; i++) 
            if (sumMembers (gp->gPtr[i], r, 0) == 0)
                return (0);
    }

    return (num);
} 

static void 
catMembers(struct gData *gp, char *cbuf, char r)
{
    sTab hashSearchPtr;
    hEnt *hashEntryPtr;
    int i;

    hashEntryPtr = h_firstEnt_(&gp->memberTab, &hashSearchPtr);

    while (hashEntryPtr) {
        strcat(cbuf, hashEntryPtr->keyname);
        strcat(cbuf, " ");
        hashEntryPtr = h_nextEnt_(&hashSearchPtr);
    }

    for (i=0;i<gp->numGroups;i++) {
        if (!r) {
            int lastChar = strlen(gp->gPtr[i]->group)-1;
            strcat(cbuf, gp->gPtr[i]->group);
            if ( gp->gPtr[i]->group[lastChar] != '/' ) 
                strcat(cbuf, "/ ");
            else 
                strcat(cbuf, " ");
        } else {
            catMembers(gp->gPtr[i], cbuf, r);
        }
    }
} 

char *
catGnames (struct gData *gp)
{
    int i;
    char *buf;

    if (gp->numGroups <= 0) {
        buf = safeSave(" ");
        return buf;
    }

    buf = my_calloc(gp->numGroups, MAX_LSB_NAME_LEN, "catGnames");
    for (i=0;i<gp->numGroups;i++) {
	strcat (buf, gp->gPtr[i]->group);
	strcat (buf, "/ ");
    }
    return buf;
} 

char **
expandGrp (struct gData *gp, char *gName, int *num)
{
    char **memberTab;

    *num = countEntries(gp, TRUE);
    if (! *num) {
        memberTab = (char **) my_calloc(1, sizeof (char *), "expandGrp");
        memberTab[0] = gName;
        *num = 1;
        return memberTab;
    }
    memberTab = (char **) my_calloc(*num, sizeof (char *), "expandGrp");

    fillMembers(gp, memberTab, TRUE);
    return memberTab;

} 
void 
fillMembers (struct gData *gp, char **memberTab, char first)
{
    sTab hashSearchPtr;
    hEnt *hashEntryPtr;
    static int mcnt;
    int i;

    if (first) {
        first = FALSE;
        mcnt = 0;
    }

    hashEntryPtr = h_firstEnt_(&gp->memberTab, &hashSearchPtr);

    while (hashEntryPtr) {                   
        memberTab[mcnt] = hashEntryPtr->keyname;
        mcnt++;
        hashEntryPtr = h_nextEnt_(&hashSearchPtr);
    }

    for (i=0; i<gp->numGroups; i++)          
        fillMembers(gp->gPtr[i], memberTab, FALSE);

}  
struct gData *
getGroup (int groupType, char *member)
{
    struct gData **gplist;
    int i, ngrp;

    if (groupType == HOST_GRP) {
        gplist = hostgroups;
        ngrp = numofhgroups;
    } else if (groupType == USER_GRP) {
        gplist = usergroups;
        ngrp = numofugroups;
    } else 
        return NULL;

    for (i = 0; i < ngrp; i++) 
        if (gMember(member, gplist[i]))
            return (gplist[i]);

    return NULL;

} 

char
gDirectMember (char *word, struct gData *gp)
{

    if (word == NULL || gp == NULL)
        return FALSE;

    if (gp->numGroups == 0 && gp->memberTab.numEnts == 0)
        return TRUE;    
                        
    if (h_getEnt_(&gp->memberTab, word))
        return TRUE;              
    return FALSE;                           

}                   

 
char
gMember (char *word, struct gData *gp)
{
    int i;

    INC_CNT(PROF_CNT_gMember);

    if (word == NULL || gp == NULL)
        return FALSE;

    if (gDirectMember(word, gp))
        return TRUE;

    for (i = 0; i < gp->numGroups; i++) {
        if (gMember(word, gp->gPtr[i]))
            return TRUE;
    }
    return FALSE;

} 


int
countEntries (struct gData *gp, char first)
{
    static int num;
    int i;

    if (first)
        num = 0;

    num += gp->memberTab.numEnts;   

    for (i=0;i<gp->numGroups;i++)
        countEntries(gp->gPtr[i], FALSE);

    return num;

} 

struct gData *
getUGrpData (char *gname)
{
    return (getGrpData (usergroups, gname, numofugroups));
} 

struct gData *
getHGrpData (char *gname)
{
    return (getGrpData (hostgroups, gname, numofhgroups));
} 

struct gData *
getGrpData (struct gData *groups[], char *name, int num)
{
    int i;

    if (name == NULL)
        return NULL;

    for (i = 0; i < num; i++) {
        if (strcmp (name, groups[i]->group) == 0)
            return (groups[i]);
    }

    return NULL;

} 

void
uDataGroupCreate()
{
    static char fname[] = "uDataGroupCreate()";
    int i;

    for (i = 0; i < numofugroups; i++) {
	struct uData *u;
	
	
	if ((u = getUserData(usergroups[i]->group)) == NULL) {
	    ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL_M, 
		fname, "getUserData", usergroups[i]->group);
	    mbdDie(MASTER_FATAL);
	}
	u->flags |= USER_GROUP;
	
	if (logclass & (LC_TRACE))
	    ls_syslog(LOG_DEBUG2, "\
%s: removing user group <%s> from all user set",
		      fname, u->user);
	
	setRemoveElement(allUsersSet, (void *)u);
	
	if (USER_GROUP_IS_ALL_USERS(usergroups[i]) == TRUE) {
	    u->flags |= USER_ALL;
	    
            if(u->children){
	    setDestroy(u->children);
            }
            if(u->descendants){
	    setDestroy(u->descendants);
            }

	    u->children = allUsersSet;
	    u->descendants = allUsersSet;
	    setAddElement(uGrpAllSet, (void *)u);
	}

	
	u->gData = usergroups[i];
	
    }
} 
void
uDataPtrTbInitialize(void)
{
    sTab        hashSearchPtr;
    hEnt *      hashEntryPtr;
    int         currentIndex;

    uDataPtrTb = uDataTableCreate();

    currentIndex = 0;
    hashEntryPtr = h_firstEnt_(&uDataList, &hashSearchPtr);
    
    while (hashEntryPtr) {
	struct uData *uData;

	uData = (struct uData *)hashEntryPtr->hData;

	uData->uDataIndex = currentIndex;
	++currentIndex;
        
	
	uDataTableAddEntry(uDataPtrTb, uData);

	
	hashEntryPtr = h_nextEnt_(&hashSearchPtr);	
    }

}
int
getIndexByuData(void *userData)
{
    struct uData *u;

    u = (struct uData *)userData;

    return (u->uDataIndex);
}

void *
getuDataByIndex(int index)
{
    if (index > uDataPtrTb->_size_)
	return (NULL);

    return ((struct uData *)uDataPtrTb->_base_[index]); 
}
void
setuDataCreate()
{
    static char fname[] = "setuDataCreate";
    struct uData *u;

    while ((u = uDataTableGetNextEntry(uDataPtrTb))) {
	if ((u->flags & USER_GROUP) 
	    && (strncmp(u->user, "others", 6) != 0)) {
	    
	    traverseGroupTree(u->gData);
        }
    }

    
    while ((u = uDataTableGetNextEntry(uDataPtrTb)) != NULL)
    {
	
	if (! u->flags & USER_GROUP) {
	    struct uData              *allUserGrp;
	    LS_BITSET_ITERATOR_T      iter;

	    BITSET_ITERATOR_ZERO_OUT(&iter);
	    setIteratorAttach(&iter, uGrpAllSet, fname);
	    for (allUserGrp = (struct uData *)setIteratorBegin(&iter);
		 allUserGrp != NULL;
		 allUserGrp = (struct uData *)setIteratorGetNextElement(&iter))
	    {
		setAddElement(u->parents, (void *)allUserGrp);
		setAddElement(u->ancestors, (void *)allUserGrp);
	    }
	}
	
	if (u->flags & USER_ALL) 
	    FOR_EACH_USER_ANCESTOR_UGRP(u, ancestor) {
	        if (setIsMember(uGrpAllAncestorSet, (void *)ancestor)==FALSE)
		{
		    char                      strBuf[512];
		    LS_BITSET_OBSERVER_T      *observer;

		    
		    sprintf(strBuf,
			    "User <%s>'s all user set observer",
			    ancestor->user);
		    
		    observer = setObserverCreate(strBuf,
						 (void *)ancestor->descendants,
						 (LS_BITSET_ENTRY_SELECT_OP_T)
						 NULL,
						 LS_BITSET_EVENT_ENTER,
						 userSetOnNewUser,
						 LS_BITSET_EVENT_NULL);
		    if (observer == NULL) {
			ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5902,
			    "%s: failed to create observer for all users set: %s"), /* catgets 5902 */
			    fname, setPerror(bitseterrno));
			mbdDie(MASTER_FATAL);
		    }

		    setObserverAttach(observer, allUsersSet);
		    setAddElement(uGrpAllAncestorSet, ancestor);
		}
            } END_FOR_EACH_USER_ANCESTOR_UGRP;
    }    
}
static void
traverseGroupTree(struct gData *grp)
{
    static char         fname[] = "traverseGroupTree";
    struct uStackEntry  uStack[MAX_GROUPS]; 
    int                 uStackTop;
    int                 uStackCur;

    uStackTop = 0;
    uStackCur = -1;

    
    uStack[uStackTop].myGData = grp;
    uStack[uStackTop].myUData = getUserData(grp->group);
    uStack[uStackTop].parentGData = NULL;
    uStack[uStackTop].parentUData = NULL;

    ++uStackTop;
    uStackCur = 0;

    while (uStackCur < uStackTop) {
        int i;
        struct gData *curGData;
        struct uData *curUData;
	sTab hashSearchPtr;
	hEnt *hashEntryPtr;

        
        curGData = uStack[uStackCur].myGData;
        curUData = uStack[uStackCur].myUData;

	
	checkuDataSet(curUData);

	
	hashEntryPtr = h_firstEnt_(&curGData->memberTab, &hashSearchPtr);
	
	while (hashEntryPtr) {
	    char *user;
	    struct uData *u;
	    
	    user =  hashEntryPtr->keyname;

	    u = getUserData(user);
	    if (u == NULL) {
		ls_syslog(LOG_ERR, I18N_FUNC_S_FAIL, 
		    fname, "getUserData", user);
		continue;
	    }
	    
	    
	    checkuDataSet(u);
	    
	    
	    setAddElement(u->parents, curUData);
	    setAddElement(u->ancestors, curUData);
	    
	    
	    setOperate(u->ancestors, curUData->ancestors,
		       LS_SET_UNION);
		
	    
	    setAddElement(curUData->children, u);
	    setAddElement(curUData->descendants, u);
	    
	    
	    hashEntryPtr = h_nextEnt_(&hashSearchPtr);
	}
	
	
	for (i = 0; i < curGData->numGroups; i++) {
	    struct gData *subGData;  
	    struct uData *subUData;
	    
	    subGData = curGData->gPtr[i];
	    subUData = getUserData(subGData->group);
	    
	    uStack[uStackTop].myGData = subGData;
	    uStack[uStackTop].myUData = subUData;
	    uStack[uStackTop].parentGData = curGData;
	    uStack[uStackTop].parentUData = curUData;
	    
	    
	    checkuDataSet(subUData);
	    
	    
	    setAddElement(subUData->parents, curUData);
	    setAddElement(subUData->ancestors, curUData);
	    setOperate(subUData->ancestors, curUData->ancestors, 
		       LS_SET_UNION);

	    
	    setAddElement(curUData->children, subUData);
	    setAddElement(curUData->descendants, subUData);
	    
	    ++uStackTop;
	}

	++uStackCur;
    }
    
    --uStackTop;
    while (uStackTop > 0 ) {
        struct uData *curUData;
	struct uData *parentUData;
	
        curUData = uStack[uStackTop].myUData;
	parentUData = uStack[uStackTop].parentUData;

 	
	checkuDataSet(parentUData);

	setOperate(parentUData->descendants, curUData->descendants, 
		   LS_SET_UNION);

        --uStackTop;
    }
}
static void 
checkuDataSet(struct uData *u)
{
    static char fname[] = "checkuDataSet()";
    static char strBuf[128];

    if (u->children == NULL) {
	memset(strBuf, 0, 128);
	sprintf(strBuf, "%s children set", u->user);
	u->children = setCreate(MAX_GROUPS, 
				getIndexByuData, 
				getuDataByIndex ,
				strBuf);
    }

    if (u->descendants == NULL) {
	memset(strBuf, 0, 128);
	sprintf(strBuf, "%s descendants set", u->user);
	u->descendants = setCreate(MAX_GROUPS, 
				 getIndexByuData, 
				 getuDataByIndex ,
				 strBuf);
    }

    if (u->parents == NULL) {
	memset(strBuf, 0, 128);
	sprintf(strBuf, "%s parents set", u->user);
	u->parents = setCreate(MAX_GROUPS, 
			       getIndexByuData, 
			       getuDataByIndex ,
			       strBuf);
    }

    if (u->ancestors == NULL) {
       	memset(strBuf, 0, 128);
	sprintf(strBuf, "%s ancestors set", u->user);
	u->ancestors = setCreate(MAX_GROUPS, 
				getIndexByuData, 
				getuDataByIndex ,
				strBuf);
    }
    if (u->children == NULL   ||
	u->descendants == NULL||
	u->parents == NULL    ||
	u->ancestors == NULL) {
	ls_syslog(LOG_ERR, _i18n_msg_get(ls_catd , NL_SETN, 5904,
	    "%s: Failed in creating user set %s"), /* catgets 5904 */
	    fname, 
	    setPerror(bitseterrno));
	mbdDie(MASTER_FATAL);
    }
}
UDATA_TABLE_T *
uDataTableCreate()
{
    static char fname[] = "uDataTableCreate()";
    UDATA_TABLE_T *this;

    this = (UDATA_TABLE_T *)my_calloc(1, sizeof(UDATA_TABLE_T), fname);

    this->_base_ = (struct uData **)my_calloc(MAX_GROUPS,
						sizeof(struct uData *),
						fname);
    this->_cur_  =   0;
    this->_size_ =   MAX_GROUPS;

    return (this);

} 

void 
uDataTableFree(UDATA_TABLE_T *uTab)
{

    if (!uTab)
       return;
    FREEUP(uTab->_base_)
    FREEUP(uTab);
    return;

} 

void
uDataTableAddEntry(UDATA_TABLE_T *this, struct uData *new)
{
    static char fname[] = "uDataTableAddEntry()";

    if (! this) {
	this = uDataPtrTb = uDataTableCreate();
    }

    if (this->_cur_  >= this->_size_) {

	this->_base_ = (struct uData **)realloc(this->_base_,
						2*(this->_size_)
						*(sizeof(struct uData *)));
	if (this->_base_ == NULL) {
	    ls_syslog(LOG_ERR, I18N_FUNC_FAIL_M, fname, "realloc");
	    mbdDie(MASTER_FATAL);
	}
			
	memset(this->_base_ + this->_size_, 0, 
	       (this->_size_)*(sizeof(struct uData *)));
	this->_size_ = 2*(this->_size_);
    }

    this->_base_[this->_cur_++] = new ;

} 
int
uDataTableGetNumEntries(UDATA_TABLE_T *this)
{
    return(this->_size_);
}
struct uData *
uDataTableGetNextEntry(UDATA_TABLE_T *this)
{
    static int i = 0;

    if (i == this->_cur_) {
	i = 0;
	return (NULL);
    } else {
	return (this->_base_[i++]);
    }
}

int
userSetOnNewUser(LS_BITSET_T *subjectSet, 
		 void *extra, 
		 LS_BITSET_EVENT_T *event)
{
    static char        fname[] = "userSetOnNewUser";
    LS_BITSET_T        *set;
    struct uData       *newUser;

    set = (LS_BITSET_T *) extra;
    newUser = (struct uData *)event->entry;

    if (logclass & (LC_TRACE)) 
	ls_syslog(LOG_DEBUG, "\
%s: added new user <%s> to <%s>", 
		  fname, newUser->user,  set->setDescription);
	
    setAddElement(set, (void *)newUser);

    return (0);

} 

#define SKIP_HOST_GROUP(group) (strstr(group, "others") != NULL)

static void
getAllHostGroup(struct gData **gplist, int ngrp, int recursive,
		struct groupInfoEnt *groupInfoEnt, int *numHostGroup)
{
    int i, count;
    
    for (i = 0, count = 0; i < ngrp; i++) {
	if (SKIP_HOST_GROUP(gplist[i]->group)) {
	    continue;
	}
	copyHostGroup(gplist[i], recursive,
		      &(groupInfoEnt[count]));
	count++;
    }
    *numHostGroup = count;
}

static void 
getSpecifiedHostGroup(char **grpName, int numGrpName, struct gData **gplist, 
		      int ngrp, int recursive, 
		      struct groupInfoEnt *groupInfoEnt, int *numHostGroup)
{
    int i, count;
    
    for (count = 0; count < numGrpName; count++) {
	int groupFound = FALSE;
	
	for (i = 0; i < ngrp; i++) {
	    if (SKIP_HOST_GROUP(gplist[i]->group)) {
		continue;
	    }
	    if (strcmp(gplist[i]->group, grpName[count]) == 0) {
		groupFound = TRUE;
		break;
	    }
	}
	if (groupFound) {
	    copyHostGroup(gplist[i], recursive,
			  &(groupInfoEnt[count]));
	} else {
	    
	    break;
	}
    }
    *numHostGroup = count;
}
	    
static void
copyHostGroup(struct gData *grp, int recursive, 
	      struct groupInfoEnt *groupInfoEnt)
{
    groupInfoEnt->group = grp->group;
    groupInfoEnt->memberList = getGroupMembers(grp, recursive);
}

int
sizeofGroupInfoReply(struct groupInfoReply *ugroups) 
{
    int              len;
    int              i;

    len = 0;

    
    len += ALIGNWORD_(sizeof(struct groupInfoReply) 
		      + ugroups->numGroups * sizeof(struct groupInfoEnt)
		      + ugroups->numGroups * NET_INTSIZE_);
    
    for (i = 0; i < ugroups->numGroups; i++) {
	struct groupInfoEnt      *ent;
	
	ent = &(ugroups->groups[i]);
	
	
	len += ALIGNWORD_(strlen(ent->group) + 1);
	len += ALIGNWORD_(strlen(ent->memberList) + 1);
    }

    return(len);

} 
