/*
 * Copyright (c) 2026 Kevin Dedon.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Commodore M-series Z8001 ROM bootstrap.
 * Boot a Coherent load module from an external
 * disc (or other) device:
 *      (xx,n,m)pathname
 * where:
 *      xx      is a disc name such as 'hd'
 *      n       unit number (default 0)
 *      m       offset in blocks into disc (default 0)
 *      pathname is an arbitrary filename on that file system.
 */
#define LADDR   1                       /* true for 32 bit l.out */

#include <ino.h>
#include <filsys.h>
#include <dir.h>
#include <canon.h>
#if      LADDR
#include "n.out.h"
#else
#include <l.out.h>
#endif
#include "rom.h"

#define bread(b,bn)     (*dread)(unit, (bn)+doffset, b)
#define min(a,b)        ((a)<(b)?(a):(b))

char    bootsyn[] = "boot command syntax error";
char    baddev[] = "bad boot device";
char    badlout[] = "l.out header error";
char    badtype[] = "bad file type";

static  unsigned lbn;                   /* Logical block number for vread */
static  long    doffset;                /* Disc block offset */
static  int     unit;                   /* Current logical unit # */
static  int     (*dread)();             /* Read routine */
char    *fsboot();
char    *fsload();
char    *dirlook();
struct dinode *iread();
static  char    buf[512];               /* sector buffer */
long    vblocks[NBN+ND];

extern  unsigned        seg;
extern  int     hdf;                    /* booting from hard disk */
#include "ddtvars.h"
#define fdf     ddtvars.d_fdf              /* booting from floppy */
extern  char    dname[2];               /* parsed device name */
extern  char    namebuf[DIRSIZ];        /* directory name scan buffer */
extern        outstr();

/*
 * Configuration of devices known to
 * the boot ROM.
 */
int     hdload();
int     wdread();
int     fdload();

char    ldebug = 0;             /* load file, don't boot it */
struct devs {
        char    dv_name[2];
        int     (*dv_load)();
        int     (*dv_read)();
        int     dv_uoff;        /* unit-number offset */
} devs[] = {
        "hd", hdload, wdread, 0,
        "fd", fdload, wdread, 2,
};

#define NDEV    (sizeof devs/sizeof devs[0])

/*
 * Parse boot string and read the filesystem to find
 * the driver.
 */
char *
boot(s)
register char *s;
{
        register struct devs *dp;

        if (*s++ != '(')
                return (bootsyn);
        if (*s=='\0' || *s==',')
                return (bootsyn);
        dname[0] = *s++;
        if (*s=='\0' || *s==',')
                return (bootsyn);
        dname[1] = *s++;
        for (dp = devs; dp < &devs[NDEV]; dp++)
                if (dp->dv_name[0]==dname[0] && dp->dv_name[1]==dname[1])
                        break;
        if (dp == &devs[NDEV])
                return (baddev);
        hdf = 0;
        fdf = 0;
        if (dname[0] == 'h' && dname[1] == 'd')
                hdf = 1;
        if (dname[0] == 'f' && dname[1] == 'd')
                fdf = 1;
        dread = dp->dv_read;
        /*
         * Parse unit number
         * and disc block offset.
         */
        doffset = unit = 0;
        if (*s == ',') {
                s++;
                while (*s>='0' && *s<='9')
                        unit = unit*10 + *s++-'0';
        }
        if (*s == ',') {
                s++;
                while (*s>='0' && *s<='9')
                        doffset = doffset*10 + *s++-'0';
        }
        unit += dp->dv_uoff;
        if (*s++ != ')')
                return (bootsyn);
        (*dp->dv_load)();
        return (fsboot(s));
}

/*
 * Boot a name from the file system.
 */
char *
fsboot(s)
register char *s;
{
        register struct dinode *dip;
        register char *cp;
        ino_t cino = ROOTIN;

        for (;;) {
                if (*s == '/')
                        s++;
                if ((dip = iread(cino)) == NULL)
                        return ("inode read error");
                switch (dip->di_mode & IFMT) {
                case IFDIR:
                        for (cp = namebuf; cp < &namebuf[DIRSIZ]; cp++)
                                *cp++ = 0;
                        for (cp = namebuf; *s!='/' && *s!='\0'; s++)
                                if (cp < &namebuf[DIRSIZ])
                                        *cp++ = *s;
                        if (cp == namebuf)
                                return (badtype);
                        if ((cp = dirlook(dip, namebuf, &cino)) != NULL)
                                return (cp);
                        break;

                case IFREG:
                        return (fsload(dip));

                default:
                        return (badtype);
                }
        }
}

/*
 * Look up the given DIRSIZ padded with nulls
 * name in the directory that vread currently
 * understands.
 */
char *
dirlook(ip, name, inop)
register struct dinode *ip;
register char *name;
ino_t *inop;
{
        register char *cp1, *cp2;
        register unsigned n;
        register struct direct *dp;

        while (ip->di_size > 0) {
                if (vread() == 0)
                        return ("directory read error");
                for (dp = (struct direct *)&buf[0];
                     dp < (struct direct *)&buf[512]; dp++) {
                        canino(dp->d_ino);
                        if (dp->d_ino == 0)
                                continue;
                        n = DIRSIZ;
                        cp1 = dp->d_name;
                        cp2 = name;
                        do {
                                if (*cp1++ != *cp2++)
                                        break;
                        } while (--n);
                        if (cp2 >= &name[DIRSIZ]) {
                                *inop = dp->d_ino;
                                return (NULL);
                        }
                }
                ip->di_size -= 512;
        }
        return ("file not found");
}

/*
 * Load the load module file into memory.
 * Set up segmentation in segments LDSEG and LDSEG+1.
 * Branch to its entry point to run it.
 */
char *
fsload(dip)
struct dinode *dip;
{
        unsigned s, o;
        unsigned bis, bds, is, ds;
        unsigned start;
        unsigned code, data;
        register struct ldheader *lp;
        register int i;

        if (vread() == 0)
                return (badlout);
        lp = (struct ldheader *)buf;
#if      LADDR
        canshort(lp->l_magic);
        canshort(lp->l_flag);
        canshort(lp->l_machine);
        canshort(lp->l_tbase);
        canlong(lp->l_entry);
        for (i=0; i<NXSEG; i++)
                canlong(lp->l_ssize[i]);
#else
        canint(lp->l_magic);
        canint(lp->l_flag);
        canint(lp->l_machine);
        canvaddr(lp->l_entry);
        for (i=0; i<NXSEG; i++)
                cansize(lp->l_ssize[i]);
#endif
        if (lp->l_magic != L_MAGIC
#if      LADDR
            || (lp->l_flag & (LF_SEP|LF_32)) != (LF_SEP|LF_32)
            || lp->l_machine != M_Z8001)
#else
            || (lp->l_flag & LF_SEP) == 0
            || lp->l_machine != M_Z8002)
#endif
                return (badlout);
        start = lp->l_entry;
        code = romconf.rom_bram<<2;
        s = code & ~0xFF;
        if (hdf || fdf)
                s += 0x100;
        o = 0;
        bds = lp->l_ssize[L_BSSD];
        bis = lp->l_ssize[L_BSSI];
        is = lp->l_ssize[L_SHRI] + lp->l_ssize[L_PRVI];
        ds = lp->l_ssize[L_SHRD] + lp->l_ssize[L_PRVD];
        /* BSSI + roundup to 512-byte click boundary */
        bis += 1024 - (bis+is)%1024;
#if      LADDR
        i = fload(&s, &o, lp->l_tbase, is, bis);
#else
        i = fload(&s, &o, sizeof *lp, is, bis);
#endif
        fload(&s, &o, i, ds, bds);
        /*
         * this really ugly code handles the case where the command
         * block for the Western Digital controller is fixed at the
         * base of RAM (RAMBASE). Since this is where we want to load
         * Coherent, we have to kludge a bit by loading it one physical
         * segment higher in memory and then block moving it down into
         * place after all disk I/O is completed. Then we have to clear
         * the memory that we stepped on.
         */
        if (hdf || fdf) {
                s = romconf.rom_bram << 2;
                s &= ~0xFF;
                ldirb(s+0x100, 0, s, 0, 0xFFFF);
                pclear(s+0x100, 0, 0xFFFF);
        }
        data = code;
        data += ((bis+is+1023)&~1023)>>8;
        segload(LDSEG, code, data);
        if (ldebug) {
                outstr("\nStart address : ");
                puthex((long)LDSEG, 4);
                putchar('|');
                puthex((long)start, 0xc);
                putchar('\n');
                return (0);
        }
        jmp(LDSEG<<8, start, 1);
        /* NOTREACHED */
}

/*
 * Load data from a file.
 * The 'segp' and 'offp' are call by reference.
 * The offset is into the block already read.
 * And offset of 0, means do another read.
 * Return a new offset.
 */
fload(segp, offp, offset, size, bsize)
unsigned *segp, *offp;
unsigned offset;
unsigned size;
unsigned bsize;
{
        register char *bp;
        register unsigned s, o;
        register unsigned count;

        s = *segp;
        o = *offp;
        for ( ; size!=0; size -= count) {
                if (offset == 0)
                        vread();
                count = 512-offset;
                bp = &buf[offset];
                count = min(count, size);
                ldirb(bp, s, o, count);
                if (o + count < o)
                        s++;
                o += count;
                offset = 0;
        }
        offset = count;
        if (bsize != 0) {
                pclear(s, o, bsize);
                if (o + bsize < o)
                        s++;
                o += bsize;
        }
        *segp = s;
        *offp = o;
        return (offset);
}

/*
 * Read in an inode.
 */
struct dinode *
iread(ino)
register ino_t ino;
{
        register struct dinode *dip;
        register int i;

        if (bread(buf, (long)iblockn(ino)) == 0)
                return (NULL);
        dip = ((struct dinode *)buf) + iblocko(ino);
        canshort(dip->di_mode);
        cansize(dip->di_size);
        l3tol(vblocks, dip->di_addr, ND+1);
        if (vblocks[ND] != 0) {
                if (bread((char *)&vblocks[ND], vblocks[ND]) == 0)
                        return (NULL);
                for (i=ND; i<ND+NBN; i++)
                        cansize(vblocks[i]);
        } else
                for (i=ND; i<ND+NBN; i++)
                        vblocks[i] = 0;
        lbn = 0;
        return (dip);
}

/*
 * Read virtual blocks sequentially from a file.
 * The block number is only an integer as it
 * can handle only 138 blocks of a file.
 * However, this handles sparse files.
 */
vread()
{
        long pb;

        if (lbn >= ND+NBN || (pb = vblocks[lbn])==0) {
                register char *cp;

                for (cp = buf; cp < &buf[512]; cp++)
                        *cp = 0;
                lbn++;
                return (1);
        } else {
                lbn++;
                return (bread(buf, pb));
        }
}
