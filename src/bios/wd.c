/*
 * Copyright (c) 2026 Kevin Dedon.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Commodore M-series boot ROM.
 * WD/SASI hard disk driver.
 *
 * The WD controller's command block is fixed at the base of RAM
 * (RAMBASE, physical 0x08000000; a second block for the other pair
 * of units at +0x10) and the sector buffer at RAMBASE+0x400.  The
 * caller builds a class-0 command in the block:
 *      cmd[0]          opcode (01 restore, 04 format, 08 read,
 *                      0b seek, 0c set drive parameters)
 *      cmd[1]          unit<<5 | block number bits 20-16
 *      cmd[2..3]       block number bits 15-0
 *      cmd[4]          sector count / interleave
 *      cmd[6..8]       controller control bytes
 *      cmd[0x0c]       completion status, preset to 0xff
 * then wdgo() starts the controller, which executes the block by
 * DMA and writes the completion status: 0x80 = done, 0x76 = busy
 * (retry the command), anything else = error code.
 */

extern        outw();
extern        puts();
extern        puthex();
extern        ldirb();
extern        hdload();
#include "ddtvars.h"
#define nerr  ddtvars.d_nerr      /* global error counter */
extern int    hr_y;             /* controller busy flag */
extern char   quiet;            /* suppress non-fatal prints */

struct dpar {                   /* per-drivetype controller param block */
        unsigned char b0;
        unsigned char hc;       /* heads<<4 | cylinder-high-nibble */
        unsigned char cyl_lo;   /* cylinder low byte */
        unsigned char b3;
        unsigned char b4;
        unsigned char secs;     /* sectors per track */
};
struct dpar dparam[] = {        /* drive geometry, indexed by drivetype */
        { 0x0f, 0x22, 0x64, 0x08, 0x08, 0x11 },  /* 0: 2-head 10MB  cyl 612 */
        { 0x0f, 0x41, 0x32, 0x08, 0x08, 0x11 },  /* 1: 4-head 10MB  cyl 306 */
        { 0x0f, 0x42, 0x8c, 0x08, 0x08, 0x11 },  /* 2: 4-head 20MB  cyl 652 */
        { 0x0f, 0x72, 0xc0, 0x16, 0x16, 0x11 },  /* 3: 7-head 42MB  cyl 704 */
};
int drivetype = 2;              /* current drive type index */

/*
 * Start the controller on the given command block and spin on the
 * completion byte, counting down a timeout (a much longer one while
 * a format is in progress).
 */
wdgo(cmd)
register unsigned char *cmd;
{
        register long n;
        register int c;

        n = 0x000f4240;
        outw(0x0500, 0x0001);           /* controller go */
        if (hr_y)
                n = 0x01c9c380;         /* formatting: allow 30x longer */
        do {
                c = cmd[0x0c];
                --n;
        } while ((c & 0xff) == 0xff && n != 0);
        if (n == 0) {
                if (quiet == 0)
                        puts("\nTIMEOUT ERROR\n");
                ++nerr;
        }
        outw(0x0500, 0x0000);
}

/*
 * Read one 512-byte block from the given unit into buf.
 * Units 0/1 (hard disks) use the first command block, units 2/3
 * (floppies) the second.  Retry as long as the controller reports
 * busy; on an autoboot probe (quiet) suppress the expected
 * not-ready errors.
 */
wdread(unit, block, buf)
int  unit;
long block;
char *buf;
{
        register unsigned char *cmd, *p;
        register int i;

        if (unit < 0 || unit > 3) {
                puts("bad parameters in wdread\n");
                return ++nerr;
        }
        if (unit == 2 || unit == 3) {
                cmd = (unsigned char *)0x08000010;
                unit -= 2;
        } else
                cmd = (unsigned char *)0x08000000;
        for (;;) {
                p = cmd;
                for (i = 0; i < 16; i++)
                        p[i] = 0;
                cmd[0] = 0x08;                  /* read */
                cmd[4] = 0x01;                  /* one sector */
                cmd[6] = 0x08;
                cmd[7] = 0x04;
                cmd[8] = 0x00;
                cmd[1] = (unit << 5) | (block >> 16);
                cmd[2] = block >> 8;
                cmd[3] = block;
                cmd[0x0c] = 0xff;
                wdgo(cmd);
                i = cmd[0x0c] & 0xff;
                if (i == 0x80)                  /* done */
                        break;
                if (i == 0x76)                  /* busy: retry */
                        continue;
                if (quiet == 0 || (i != 0x84 && i != 0x92 && i != 0x94)) {
                        puts("read error: status=0x");
                        puthex((long)i, 4);
                        puts(" block=0x");
                        puthex(block, 0x1c);
                        puts("\n");
                }
                ++nerr;
                break;
        }
        ldirb((char *)0x08000400, buf, 0x200);  /* sector buffer -> caller */
        return nerr == 0;
}

/*
 * Initialize the controller and drive: copy the current drive
 * type's geometry into the sector buffer and issue a set-drive-
 * parameters command.
 */
wdinit()
{
        register unsigned char *cmd, *p;
        register struct dpar *q;
        register int i;

        cmd = (unsigned char *)0x08000000;
        p = cmd;
        for (i = 0; i < 0x20; i++)
                p[i] = 0;
        cmd[0] = 0x0c;                  /* set drive parameters */
        cmd[4] = 0;
        cmd[6] = 8;
        cmd[7] = 4;
        cmd[8] = 0;
        cmd[1] = 0;
        cmd[2] = 0;
        cmd[3] = 0;
        cmd[0x0c] = 0xff;
        q = (struct dpar *)0x08000400;
        *q = dparam[drivetype];         /* geometry as command data */
        wdgo(cmd);
        if (cmd[0x0c] & 0xff) {
                puts("controller/drive initialization failed\n");
                ++nerr;
        }
}

/*
 * Format the given floppy unit (the floppies sit behind the second
 * command block).  The busy flag stretches wdgo's timeout for the
 * duration of the format.
 */
wdformat(unit)
register int unit;
{
        register unsigned char *cmd, *p;
        register int i;

        if (unit == 0 || unit == 1)
                cmd = (unsigned char *)0x08000010;
        else {
                puts("illegal parameters for format!\n");
                return ++nerr;
        }
        for (;;) {
                p = cmd;
                for (i = 0; i < 16; i++)
                        p[i] = 0;
                cmd[0] = 0x04;                  /* format */
                cmd[4] = 0x01;
                cmd[6] = 0x08;
                cmd[7] = 0x04;
                cmd[8] = 0x00;
                cmd[1] = unit << 5;
                cmd[0x0c] = 0xff;
                hr_y = 1;
                wdgo(cmd);
                hr_y = 0;
                i = cmd[0x0c] & 0xff;
                if (i == 0x80)
                        break;
                if (i == 0x76)
                        continue;
                puts("error 0x");
                puthex((long)i, 4);
                puts(" on format.\n");
                puts("format failed: discard floppy!\n");
                ++nerr;
                break;
        }
        return nerr == 0;
}

/*
 * Park the given hard disk unit's heads over the shipping zone:
 * restore to cylinder 0, then seek to the drive's last block
 * (computed from the current drive type's geometry).
 */
wdpark(unit)
register int unit;
{
        register unsigned char *cmd, *p;
        register int i;
        long nblk, secs, heads;

        if (unit == 0 || unit == 1)
                cmd = (unsigned char *)0x08000000;
        else {
                puts("illegal parameters for park!\n");
                return ++nerr;
        }
        hdload();
        p = cmd;
        for (i = 0; i < 16; i++)
                p[i] = 0;
        cmd[0] = 0x01;                  /* restore to cylinder 0 */
        cmd[4] = 0x01;
        cmd[6] = 0x08;
        cmd[7] = 0x04;
        cmd[8] = 0x00;
        cmd[1] = unit << 5;
        cmd[0x0c] = 0xff;
        puts("Restore drive\n");
        wdgo(cmd);
        cmd[0] = 0x0b;                  /* seek to the last block */
        cmd[0x0c] = 0xff;
        heads = dparam[drivetype].hc >> 4;
        secs = (unsigned)dparam[drivetype].secs;
        nblk = (((dparam[drivetype].hc & 0x0f) << 8) | (long)dparam[drivetype].cyl_lo) * secs * heads - 1;
        cmd[1] = ((unsigned)unit << 5) | (nblk >> 16);
        cmd[2] = (nblk >> 8) & 0xff;
        cmd[3] = nblk & 0xff;
        puts("Seek\n");
        wdgo(cmd);
        i = cmd[0x0c] & 0xff;
        if (i != 0x80) {
                puts("error 0x");
                puthex((long)i, 4);
                puts(" on park.\n");
                ++nerr;
        } else
                puts("\nParked\n");
        return nerr == 0;
}
