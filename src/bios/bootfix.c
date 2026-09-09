/*
 * Copyright (c) 2026 Kevin Dedon.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Flatten the linked boot ROM (l.out) to the 32K EPROM image, in
 * place: text at 0, data at 0x6800, zero fill.  The header is parsed
 * byte by byte (shorts low-first, longs high word first).  Two half
 * buffers -- a single 32K array is too big for an object.
 */

#define TEXTMAX 0x6800          /* image 0x0000-0x67ff: text + zero fill  */
#define DATAMAX 0x1800          /* image 0x6800-0x7fff: seg-1 data + fill */

char    tbuf[TEXTMAX];
char    dbuf[DATAMAX];
char    hdr[44];

unsigned gw(p)
register char *p;
{
        return ((p[1] & 0xff) << 8) | (p[0] & 0xff);
}

long gl(p)
register char *p;
{
        return ((long)gw(p) << 16) | (long)gw(p + 2);
}

main(argc, argv)
int argc;
char *argv[];
{
        register int fd, i;
        int tbase;
        long ssize[9];
        long text, data;
        long lseek();

        if (argc != 2) {
                write(2, "usage: bootfix l.out\n", 21);
                exit(1);
        }
        fd = open(argv[1], 0);
        if (fd < 0) {
                write(2, "bootfix: cannot open\n", 21);
                exit(1);
        }
        if (read(fd, hdr, 44) != 44) {
                write(2, "bootfix: short header\n", 22);
                exit(1);
        }
        tbase = gw(hdr + 6);
        for (i = 0; i < 9; i++)
                ssize[i] = gl(hdr + 8 + 4 * i);
        text = ssize[0];                        /* SHRI */
        data = ssize[3] + ssize[4];             /* SHRD + PRVD */
        if (text > (long)TEXTMAX || data > (long)DATAMAX) {
                write(2, "bootfix: image too big\n", 23);
                exit(1);
        }
        for (i = 0; i < TEXTMAX; i++)
                tbuf[i] = 0;
        for (i = 0; i < DATAMAX; i++)
                dbuf[i] = 0;
        lseek(fd, (long)tbase, 0);
        if (read(fd, tbuf, (int)text) != (int)text) {
                write(2, "bootfix: short text\n", 20);
                exit(1);
        }
        lseek(fd, (long)tbase + ssize[1] + text, 0);
        if (read(fd, dbuf, (int)data) != (int)data) {
                write(2, "bootfix: short data\n", 20);
                exit(1);
        }
        close(fd);
        fd = creat(argv[1], 0644);
        if (fd < 0 || write(fd, tbuf, TEXTMAX) != TEXTMAX
                   || write(fd, dbuf, DATAMAX) != DATAMAX) {
                write(2, "bootfix: rewrite failed\n", 24);
                exit(1);
        }
        close(fd);
        exit(0);
}
