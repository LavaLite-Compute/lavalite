/* $Id: btop.c,v 1.2 2007/08/15 22:18:44 tmizan Exp $
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

#include "cmd.h"

#define NL_SETN 8 	

extern void bmove (int, char **, int);

int 
main (int argc, char **argv)
{

    int rc;

    rc = _i18n_init ( I18N_CAT_MIN );	

    bmove(argc, argv, TO_TOP);

    _i18n_end ( ls_catd );			
    exit(0);
} 
