/* $Id: bkill.c,v 1.3 2007/08/15 22:18:43 tmizan Exp $
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

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "../../lsf/lib/lsi18n.h"

#define NL_SETN 8 	

extern int bsignal (int, char **);
extern int _lsb_recvtimeout;

int 
main (int argc, char **argv)
{
    int rc;
    
    _lsb_recvtimeout = 30;
    rc = _i18n_init ( I18N_CAT_MIN );	

    if (bsignal(argc, argv) == 0) {
	exit(-1);
    }
    _i18n_end ( ls_catd );			
    exit(0);
}
