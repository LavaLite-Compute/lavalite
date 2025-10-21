/* $Id: lib.term.c,v 1.4 2007/08/15 22:18:51 tmizan Exp $
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
#include "lsf/lib/lib.h"

#ifndef VREPRINT
#define VREPRINT VRPRNT
#endif

#ifndef VDISCARD
#define VDISCARD VFLUSHO
#endif

#define LOBLK 0040000
#define VDSUSP  11
#define VSWTCH  7

#define IN_TABLE_SIZE 13
static tcflag_t in_table[IN_TABLE_SIZE] = {IGNBRK
                                           ,BRKINT
                                           ,IGNPAR
                                           ,PARMRK
                                           ,INPCK
                                           ,ISTRIP
                                           ,INLCR
                                           ,IGNCR
                                           ,ICRNL
                                           ,IUCLC
                                           ,IXON
                                           ,IXANY
                                           ,IXOFF
};

#define OUT_TABLE_SIZE 30
static tcflag_t out_table[OUT_TABLE_SIZE] = {OPOST
                                             ,OLCUC
                                             ,ONLCR
                                             ,OCRNL
                                             ,ONOCR
                                             ,ONLRET
                                             ,OFILL
                                             ,OFDEL
                                             ,NLDLY
                                             ,NL0
                                             ,NL1
                                             ,CRDLY
                                             ,CR0
                                             ,CR1
                                             ,CR2
                                             ,CR3
                                             ,TABDLY
                                             ,TAB0
                                             ,TAB1
                                             ,TAB2
                                             ,TAB3
                                             ,BSDLY
                                             ,BS0
                                             ,BS1
                                             ,VTDLY
                                             ,VT0
                                             ,VT1
                                             ,FFDLY
                                             ,FF0
                                             ,FF1
};

#define CTRL_TABLE_SIZE 12
static tcflag_t ctrl_table[CTRL_TABLE_SIZE] = {CLOCAL
                                               ,CREAD
                                               ,CSIZE
                                               ,CS5
                                               ,CS6
                                               ,CS7
                                               ,CS8
                                               ,CSTOPB
                                               ,HUPCL
                                               ,PARENB
                                               ,PARODD
                                               ,LOBLK
};

#define LOC_TABLE_SIZE 10
static tcflag_t loc_table[LOC_TABLE_SIZE] = {ISIG
                                             ,ICANON
                                             ,XCASE
                                             ,ECHO
                                             ,ECHOE
                                             ,ECHOK
                                             ,ECHONL
                                             ,NOFLSH
                                             ,TOSTOP
                                             ,IEXTEN
};

#define CHR_TABLE_SIZE 18
static int chr_table[CHR_TABLE_SIZE] = {VINTR
                                        ,VQUIT
                                        ,VERASE
                                        ,VKILL
                                        ,VEOF
                                        ,VEOL
                                        ,VEOL2
                                        ,VSWTCH
                                        ,VSTART
                                        ,VSTOP
                                        ,VMIN
                                        ,VTIME

#                                     define CHR_TABLE_SPLIT    12

                                        ,VSUSP
                                        ,VDSUSP
                                        ,VREPRINT
                                        ,VDISCARD
                                        ,VWERASE
                                        ,VLNEXT
};

#define BAUD_TABLE_SIZE 16
static speed_t baud_table[BAUD_TABLE_SIZE] = {B0
                                              ,B50
                                              ,B75
                                              ,B110
                                              ,B134
                                              ,B150
                                              ,B200
                                              ,B300
                                              ,B600
                                              ,B1200
                                              ,B1800
                                              ,B2400
                                              ,B4800
                                              ,B9600
                                              ,B19200
                                              ,B38400
};

static int encode_mode(XDR *xdrs, tcflag_t mode_set, tcflag_t *attr_table,
                       int table_count);
static int decode_mode(XDR *xdrs, tcflag_t *attr_table, int table_count,
                       tcflag_t *mode_set);

int
encodeTermios_(XDR *xdrs, struct termios *ptr_termios)
{
    speed_t speed_value;
    int i;

    if (!encode_mode(xdrs, ptr_termios->c_iflag, in_table, IN_TABLE_SIZE))
        return false;

    if (!encode_mode(xdrs, ptr_termios->c_oflag, out_table, OUT_TABLE_SIZE))
        return false;

    if (! (encode_mode(xdrs, ptr_termios->c_cflag, ctrl_table,
                       CTRL_TABLE_SIZE) &&
           encode_mode(xdrs, ptr_termios->c_lflag, loc_table, LOC_TABLE_SIZE)))
        return false;

    speed_value = cfgetospeed(ptr_termios);
    for (i = 0; i < BAUD_TABLE_SIZE; i++) {
        if (speed_value == baud_table[i]) {
            break;
        }
    }
    if (i == BAUD_TABLE_SIZE) {
        i = 0;
    }
    if (!xdr_int(xdrs, &i))
        return false;

    for (i = 0; i < CHR_TABLE_SIZE; i++) {
        if (i < CHR_TABLE_SPLIT) {
            if (!xdr_char(xdrs, (char *)&ptr_termios->c_cc[chr_table[i]]))
                return false;
        } else {
            if (!xdr_char(xdrs, (char *)&ptr_termios->c_cc[chr_table[i]]))
                return false;
        }
    }
    return true;
}

static int
encode_mode(XDR *xdrs, tcflag_t mode_set, tcflag_t *attr_table,
            int table_count)
{

    int encode_set;
    int i;

    for (encode_set = 0, i = 0; i < table_count; i++) {
        if (attr_table[i] != (tcflag_t)0 &&
            (mode_set & attr_table[i]) == attr_table[i]) {
            encode_set |= 1<<i;
        }
    }

    return (xdr_int(xdrs, &encode_set));
}

int
decodeTermios_(XDR *xdrs, struct termios *ptr_termios)
{
    speed_t speed_value;
    int i;

    if (!decode_mode(xdrs, in_table,   IN_TABLE_SIZE,   &ptr_termios->c_iflag))
        return false;

    if (!decode_mode(xdrs, out_table,  OUT_TABLE_SIZE,  &ptr_termios->c_oflag))
        return false;

    if (! (decode_mode(xdrs, ctrl_table, CTRL_TABLE_SIZE,
                       &ptr_termios->c_cflag) &&
           decode_mode(xdrs, loc_table,  LOC_TABLE_SIZE,
                       &ptr_termios->c_lflag)))
        return false;

    if (!xdr_int(xdrs, &i))
        return false;

    speed_value = baud_table[i];
    (void)cfsetospeed(ptr_termios, speed_value);

    for (i = 0; i < NCCS; i++) {
        ptr_termios->c_cc[i] = _POSIX_VDISABLE;
    }

    for (i = 0; i < CHR_TABLE_SIZE; i++) {
        if (i < CHR_TABLE_SPLIT) {
            if (!xdr_char(xdrs, (char *)&ptr_termios->c_cc[chr_table[i]]))
                return false;
        } else {
            if (!xdr_char(xdrs, (char *)&ptr_termios->c_cc[chr_table[i]]))
                return false;
        }
    }

    return true;
}

static int
decode_mode(XDR *xdrs, tcflag_t *attr_table, int table_count,
            tcflag_t *mode_set)
{
    int encode_set;
    int i;

    if (!xdr_int(xdrs, &encode_set))
        return false;

    for (*mode_set = 0, i = 0; i < table_count; i++) {
        if (encode_set & (1<<i)) {
            *mode_set |= attr_table[i];
        }
    }

    return true;
}
