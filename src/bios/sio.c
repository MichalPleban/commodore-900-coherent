/*
 * Copyright (c) 2026 Kevin Dedon.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Commodore M-series boot ROM.
 * Console I/O primitives.  con_alt/con_hires route output to the
 * alternate or hi-res screen; otherwise the Z8030 SCC serial port.
 */

#include "romconf.h"
#include "ddtvars.h"
#define con_alt    romconf.has_hires          /* alternate-screen select */
#define con_hires  ddtvars.d_conhires          /* hi-res console select */
int  get_init = 0;              /* getchar initialized */
char put_init = 0;              /* putchar initialized */

extern        altput();
extern        hrput();
extern        coninit();
extern int    splhi();
extern        splx();
extern        chipinit();
extern int    hrkey();
extern int    hrdecode();
extern int    inb();
extern        outb();

putchar(c)
register char c;
{
        register int s;

        if (con_alt)   { altput(c); return; }
        if (con_hires) { hrput(c); return; }
        if (put_init == 0)
                coninit();
        if (c == '\n')
                putchar('\r');
        s = splhi();
        while ((inb(0x0101) & 0x04) == 0)       /* wait for transmitter */
                ;
        outb(0x0111, c);
        splx(s);
}

int
getchar()
{
        register int c;

        if (con_alt || con_hires) {
                if (get_init == 0) {
                        chipinit();
                        get_init++;
                }
                do {
                        while ((c = hrkey()) == 0)
                                ;
                        c = hrdecode(c);
                } while (c == 0);
        } else {
                while ((inb(0x0101) & 0x01) == 0)       /* wait for receiver */
                        ;
                c = inb(0x0111) & 0x7f;                 /* strip parity */
        }
        if (c == '\r')
                c = '\n';
        putchar(c);                                     /* echo */
        return c;
}
