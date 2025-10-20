/* $Id: tokdefs.h,v 1.2 2007/08/15 22:18:49 tmizan Exp $
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


enum TOKEN_type {
UNDEF,
ICON, RCON, CCON, MON, TUE, WED, THU, FRI, SAT, SUN, JAN, FEB,
MAR,  APR,  MAY,  JUN, JUL, AUG, SEP, OCT, NOV, DEC, YY,  FY, 
WEEK, MONTH,QUARTER,    DAYS,    OR,  AND, LE,  GE,  EQ,  DOTS, 
HH, MM, ESTRING,  NAME, RANGE, DATES, SZZZZ
};


