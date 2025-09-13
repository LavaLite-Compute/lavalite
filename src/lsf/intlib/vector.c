/* $Id: vector.c,v 1.2 2007/08/15 22:18:49 tmizan Exp $
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 *
 */

#include "lsf/intlib/intlib_internal.h"

extern char *my_calloc(int, int, char *);

LS_VECTOR_T *
vectorCreate(int size)
{
    LS_VECTOR_T *vector;

    vector = (LS_VECTOR_T *)my_calloc(1,
				      sizeof(LS_VECTOR_T),
				      "vectorCreate");
    vector->entry = (LS_VECTOR_ENTRY_T *)my_calloc(size,
						   sizeof(LS_VECTOR_ENTRY_T),
						   "vectorCreate");
    vector->size = size;
    vector->position = 0;

    return(vector);
}

int
vectorInsertEntry(LS_VECTOR_T *vector, LS_VECTOR_ENTRY_T *entry)
{
    vector->entry = entry;
    vector->position++;

    return(0);
}

LS_VECTOR_ENTRY_T *
vectorGetEntry(LS_VECTOR_T *vector, int index)
{
    LS_VECTOR_ENTRY_T *entry;

    entry = &(vector->entry[index]);

    return(entry);
}

void
vectorDestroy(LS_VECTOR_T *vector, void (*freeFun)(void *))
{
    int   i;
    int   size;

    size = vector->size;

    for (i = 0; i < size; i++) {
	LS_VECTOR_ENTRY_T *entry;

	entry = &(vector->entry[i]);
	(*freeFun)((void *)entry);
    }

    FREEUP(vector);

}
