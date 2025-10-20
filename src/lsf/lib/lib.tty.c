/* $Id: lib.tty.c,v 1.3 2007/08/15 22:18:51 tmizan Exp $
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
#include "lsf/lib/lib.common.h"

#define NL_SETN   23


static void ttymode_(int mode, int ind, int enableIntSus);

void
ls_remtty(int ind, int enableIntSus)
{
    ttymode_(1, ind, enableIntSus);
}

void
ls_loctty(int ind)
{
    ttymode_(0, ind, 0);
}

static void
ttymode_(int mode, int ind, int enableIntSus)
{
    static int lastmode;
    static int first = 1;
    static struct termios  loxio;
    struct termios  xio;
    tcflag_t  tmpflag;
    int i;

    if (getpgrp() != tcgetpgrp(ind)) {
        return;
    }

    if (first && ! mode)
        return;
    first = 0;
    switch (mode) {
    case 0:
        xio = loxio;
        break;
    case 1:
        if (! lastmode) {
            if (tcgetattr(ind, &loxio) == -1) {
                perror("ttymode_");
                fprintf(stderr, I18N_ERROR_LD, "tcgetattr", (long)ind);
                fprintf(stderr, "\n");
            }
        }
        xio = loxio;

        xio.c_iflag &= (IXOFF | IXON | IXANY);

        tmpflag  = xio.c_oflag & (NLDLY|CRDLY|TABDLY|BSDLY|VTDLY|FFDLY);
        xio.c_oflag &= (OPOST | OFILL| OFDEL | tmpflag);


        if (enableIntSus)
            xio.c_lflag &= (XCASE | ISIG);
        else
            xio.c_lflag &= (XCASE);

        for (i=0; i < NCCS; i++)
            xio.c_cc[i] = _POSIX_VDISABLE;


        if (enableIntSus) {
            xio.c_cc[VINTR] = loxio.c_cc[VINTR];
            xio.c_cc[VSUSP] = loxio.c_cc[VSUSP];
        }

        xio.c_cc[VMIN] = 0;
        xio.c_cc[VTIME] = 0;

        break;
    }
    if (tcsetattr(ind, TCSANOW, &xio) == -1) {
        if (errno != EINTR) {
            perror("ttymode_");
            fprintf(stderr, I18N_ERROR_LD, "tcsetattr", (long)ind);
            fprintf(stderr, "\n");
        }
    }

    lastmode = mode;

}
