/*
 * Copyright (c) 2026 Kevin Dedon.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Commodore M-series boot ROM.
 * Monitor: startup, command interpreter, and helpers.
 */
#include "romconf.h"
#include "ddtvars.h"

typedef long addr_t;

struct romconf romconf;         /* machine configuration */
struct ddtvars ddtvars;         /* debugger working storage */
char    line[102];              /* console line buffer */

struct romconf *romp = &romconf;
char    abflags[2] = {0, 0};
#define burnin abflags[0]
#define abrt5  abflags[1]
char   *fdspec = "(fd,1)coherent"; /* floppy auto-boot spec */
char   *hdspec = "(hd)coherent";   /* hard disk auto-boot spec */
char    quiet = 0;              /* suppress boot errors */
char    *msgs[] = {
        "Command        Function\n",
        "---------------------------------------------------------\n",
        "(hd)file       boot <file> from the Commodore hard disk\n",
        "(fd)file       boot <file> from the Commodore floppy disk\n",
        "l              toggle load/boot switch\n",
        "m              display RAM configuration\n",
        "FF<unit>       format Commodore Floppy disk <unit>\n",
        "d              run debugger\n",
        "S <n>          set hard disk parameters to type <n>\n",
        "P <unit>       park hard disk <unit> heads over shipping zone\n",
        "\n",
};
int     manualdone = 0;

extern int    manualdone;
extern char   ldebug;           /* load/boot debug toggle */
extern int    drivetype;        /* current drive type index */

extern char  *boot();
extern        chipinit();
extern        puts();
extern        putchar();
extern        getchar();
extern char  *gets();
extern        memdiag();
extern        outstr();
extern        selftest();
extern        port_test();
extern        configure();
extern int    multmic();
extern        kill();
extern        ramtest();
extern        debugger();
extern        wdformat();
extern        wdpark();

/*
 * Called from the reset code with the extent of memory.
 * Print RAM size and run the self-tests (unless burning in),
 * configure the machine, then enter the monitor.
 */
main(low, hi)
unsigned low, hi;
{
        if (!burnin) {
                memdiag(hi - low);
                outstr("K OK\n");
        }
        if (!burnin)
                selftest();
        if (!burnin)
                port_test();
        configure(&romconf, low, hi);
        chipinit();
        if (multmic())
                command();
        if (burnin)
                kill();
        ramtest();
}

/*
 * Automatic boot (floppy once, then hard disk twice with a delay),
 * then the manual-boot recovery path, then the command prompt loop.
 */
command()
{
        short retry;
        char c1, c2;
        register char *r;
        register long n;
        register int f;

        if (fdspec != 0 && abrt5 == 0 && burnin == 0) {
                quiet = 0xff;
                puts("Automatic boot in progress\n");
                r = boot(fdspec);
                for (retry = 0; retry < 2; retry++) {
                        if (retry == 1)
                                quiet = 0;
                        r = boot(hdspec);
                        n = 0x004c4b40;
                        do
                                --n;
                        while (n != 0);
                }
                if (r != 0) {
                        chipinit();
                        puts(r);
                        putchar('\n');
                }
        }
top:
        if (manualdone)
                goto prompt;
        if (abrt5)
                goto prompt;
        puts(!burnin ? (puts("\ncannot boot!\n"), "Insert bootable floppy")
                     : "Manual boot required");
        puts(" - hit any key twice when ready.\n");
keys:
        c1 = getchar();
        c2 = getchar();
        putchar('\n');
        manualdone = (c1 == '1' && c2 == '9');
        if (!manualdone && c1 == c2) {
                quiet = 0xff;
                r = boot(fdspec);
                quiet = 0;
                r = boot(hdspec);
                if (r != 0) {
                        chipinit();
                        puts(r);
                        putchar('\n');
                }
        }
        if (manualdone)
                goto top;
        if (c1 == c2)
                goto top;
        goto keys;
prompt:
        puts("\nCommodore C900 monitor (type ? for commands)\n");
        for (;;) {
                f = 0;
                puts("? ");
                gets(line);
                switch (line[0]) {
                case '\0':
                case '\n':
                        continue;
                case '(':
                        quiet = 0;
                        r = boot(line);
                        if (r != 0) {
                                puts(r);
                                putchar('\n');
                        }
                        continue;
                case 'l':
                        ldebug = !ldebug;
                        puts(ldebug ? "Load file, don't boot\n" : "Boot file once loaded\n");

                        continue;
                case 'm':
                        memory();
                        continue;
                case 'F':
                        floppy(&line[1]);
                        continue;
                case 'S':
                        parameters(&line[1]);
                        continue;
                case 'P':
                        parkdisk(&line[1]);
                        continue;
                case '?':
                        usage();
                        continue;
                case 'd':
                        debugger();
                        continue;
                default:
                        putchar(line[0]);
                        puts(": bad command\n");
                        continue;
                }
        }
}

/*
 * Read a hex address; if none given, return the default.
 */
addr_t
getaddr(cp, def)
register char *cp;
addr_t def;
{
        register int n;

        while (*cp == ' ' || *cp == '\t')
                cp++;
        if (hexdigit(*cp) >= 0) {
                def = 0;
                while ((n = hexdigit(*cp++)) >= 0)
                        def = (def << 4) + n;
        }
        return (def);
}

/*
 * Set hard disk parameters ('S' command).
 */
struct hdpar {
        char    *hd_type;
        char    hd_param[10];
} hdpar[] = {
        {"2-head 10MB", 1, 1, 0, 1, 2, 98, 0, 0, 0, 0 },
        {"4-head 10MB", 1, 1, 0, 3, 1, 49, 0, 0, 0, 0 },
        {"4-head 20MB", 1, 1, 0, 3, 2, 98, 0, 0, 0, 0 },
        {"7-head 42MB", 1, 1, 0, 6, 2, 192, 0, 0, 0, 0 },
};

parameters(s)
register char *s;
{
        register struct hdpar *hp;
        register int n;

        n = getaddr(s, -1);
        if (n < 0 || n >= (sizeof(hdpar) / sizeof(hdpar[0]))) {
                puts("Bad parameter setting\n");
                return;
        }
        hp = &hdpar[n];
        puts("Selecting ");
        puts(hp->hd_type);
        puts(" as hard disc\n");
        drivetype = n;
}

/*
 * Print the RAM base and top as segment|offset ('m' command).
 */
memory()
{
        register unsigned l, h;

        l = romconf.rom_bram;
        h = romconf.rom_eram;

        puts("RAM base: ");
        puthex((long)(l >> 6), 4);
        puts("|");
        puthex((long)(l << 12), 0x0c);
        puts("\n");

        puts("RAM top:  ");
        puthex((long)(h >> 6), 4);
        puts("|");
        puthex((long)(h << 12), 0x0c);
        puts("\n");
}

/*
 * Hex value of a digit, -1 if none.
 */
hexdigit(c)
register int c;
{
        if (c >= '0' && c <= '9')
                return (c - '0');
        if (c >= 'A' && c <= 'Z')
                return (c - 'A' + 0xA);
        if (c >= 'a' && c <= 'z')
                return (c - 'a' + 0xa);
        return (-1);
}

/*
 * Output a string.
 */
puts(s)
register char *s;
{
        while (*s != '\0')
                putchar(*s++);
}

/*
 * Read a console line, with '\b' erase and '@' kill.
 */
char *
gets(as)
char *as;
{
        register char *s;
        register int c;

again:
        s = as;
        while ((c = getchar()) != '\n') {
                if (c == '\b') {
                        if (s <= as)
                                continue;
                        s--;
                        putchar(' ');
                        putchar(c);
                        continue;
                }
                if (c == '@') {
                        putchar('\n');
                        goto again;
                }
                *s++ = c;
        }
        *s = '\0';
        return (s);
}

/*
 * Print the command summary ('?' command).
 */

usage()
{
        register char **cp;

        puts("\nCommodore C900 Monitor V0.0\n");
        for (cp = msgs; cp < &msgs[(sizeof(msgs) / sizeof(msgs[0]))]; cp++)
                puts(*cp);
}

/*
 * Format a Commodore floppy disk ('F' command).
 */
floppy(args)
char *args;
{
        register char *p = args;
        int format_flag;
        register int d, arg1, arg2;

        format_flag = 0;
        arg1 = arg2 = 0xffff;
        if (*p == 'F') {
                p++;
                format_flag++;
        } else if (*p++ != 'S') {
                puts("Badly formed format command!\n");
                return;
        }
        while (*p == ' ' || *p == '\t')
                p++;
        if (hexdigit(*p) >= 0) {
                arg1 = 0;
                while ((d = hexdigit(*p++)) >= 0)
                        arg1 = (arg1 << 4) + d;
        }
        while (*p == ' ' || *p == '\t')
                p++;
        if (hexdigit(*p) >= 0) {
                arg2 = 0;
                while ((d = hexdigit(*p++)) >= 0)
                        arg2 = (arg2 << 4) + d;
        }
        wdformat(arg1);
}

/*
 * Park the hard disk heads ('P' command).
 */
parkdisk(args)
char *args;
{
        register char *p = args;
        register int d, unit;

        while (*p == ' ' || *p == '\t')
                p++;
        if (hexdigit(*p) < 0)
                goto issue;
        unit = 0;
        while ((d = hexdigit(*p++)) >= 0)
                unit = (unit << 4) + d;
issue:
        wdpark(unit);
}

/*
 * Print val in hex; n is the bit position of the high nibble.
 */
puthex(val, n)
register long val;
register int n;
{
        register int d;

        do {
                d = (val >> n) & 0x000f;
                d += '0';
                if (d > '9')
                        d += 7;
                putchar(d);
        } while ((n -= 4) >= 0);
}
