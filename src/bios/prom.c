/*
 * Copyright (c) 2026 Kevin Dedon.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Split the flat ROM image into the two EPROM images for the
 * programmer: rom.H = even bytes (D15:D8), rom.L = odd bytes (D7:D0).
 */

#define CHUNK   8192
#define NCHUNK  4               /* 4 x 8K = the 32K image */

char    chunk[CHUNK];
char    hi[CHUNK / 2];
char    lo[CHUNK / 2];
char    name[64];

main(argc, argv)
int argc;
char *argv[];
{
        register int i, n;
        int fd, fdh, fdl;

        if (argc != 2) {
                write(2, "usage: prom rom\n", 16);
                exit(1);
        }
        fd = open(argv[1], 0);
        if (fd < 0) {
                write(2, "prom: cannot open\n", 18);
                exit(1);
        }
        for (n = 0; argv[1][n] != '\0' && n < 60; n++)
                name[n] = argv[1][n];
        name[n] = '.'; name[n + 1] = 'H'; name[n + 2] = '\0';
        fdh = creat(name, 0644);
        name[n + 1] = 'L';
        fdl = creat(name, 0644);
        if (fdh < 0 || fdl < 0) {
                write(2, "prom: cannot create\n", 20);
                exit(1);
        }
        for (n = 0; n < NCHUNK; n++) {
                if (read(fd, chunk, CHUNK) != CHUNK) {
                        write(2, "prom: short read\n", 17);
                        exit(1);
                }
                for (i = 0; i < CHUNK / 2; i++) {
                        hi[i] = chunk[2 * i];
                        lo[i] = chunk[2 * i + 1];
                }
                if (write(fdh, hi, CHUNK / 2) != CHUNK / 2
                 || write(fdl, lo, CHUNK / 2) != CHUNK / 2) {
                        write(2, "prom: write failed\n", 19);
                        exit(1);
                }
        }
        close(fd);
        close(fdh);
        close(fdl);
        exit(0);
}
