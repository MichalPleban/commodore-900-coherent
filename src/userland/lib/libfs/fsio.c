/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * libfs -- block I/O over an open file system volume.
 *
 * fsopen/fsclose manage the FS handle; fsbread/fsbwrite are the single copy
 * of the range-checked lseek+read/write the check tools each used to carry.
 * On a hard I/O error they print a diagnostic and OR the caller's status bit
 * (fsp->hardbit) into *fsp->pexstat, then return -1; a bad read zero-fills the
 * buffer so callers that ignore the return value see zeroes, matching the old
 * per-tool behaviour.
 */
#include <stdio.h>
#include <fs.h>

char	*malloc();

FS *
fsopen(name, writable, pexstat, hardbit)
char *name;
int writable;
int *pexstat;
int hardbit;
{
	register FS *fsp;

	if ((fsp = (FS *)malloc(sizeof (FS))) == NULL)
		return (NULL);
	fsbclear((char *)fsp, sizeof (FS));
	if ((fsp->fd = open(name, writable ? 2 : 0)) < 0) {
		free((char *)fsp);
		return (NULL);
	}
	if ((fsp->dbuf = malloc(BSIZE)) == NULL) {
		close(fsp->fd);
		free((char *)fsp);
		return (NULL);
	}
	fsp->writable = writable;
	fsp->fsize = SUPERI + 1;	/* enough to let the super block be read */
	fsp->isize = 0;
	fsp->maxino = 0;
	fsp->pexstat = pexstat;
	fsp->hardbit = hardbit;
	fsp->deflist = NULL;
	fsp->entries = NULL;
	fsp->dbmap = NULL;
	fsp->sbmap = NULL;
	return (fsp);
}

void
fsclose(fsp)
register FS *fsp;
{
	if (fsp == NULL)
		return;
	fsnfree(fsp);
	fsfreedefective(fsp);
	if (fsp->dbuf != NULL)
		free(fsp->dbuf);
	close(fsp->fd);
	free((char *)fsp);
}

/*
 * Read block `bn' into `buf'.  Returns 0 on success, -1 on a range or I/O
 * error (buf is zeroed in that case).
 */
int
fsbread(fsp, bn, buf)
register FS *fsp;
daddr_t bn;
char *buf;
{
	if (bn < 0 || bn >= fsp->fsize) {
		fsbclear(buf, BSIZE);
		fprintf(stderr, "libfs: bad block #%D\n", (long)bn);
		if (fsp->pexstat != NULL)
			*fsp->pexstat |= fsp->hardbit;
		return (-1);
	}
	lseek(fsp->fd, (size_t)BSIZE * bn, 0);
	if (read(fsp->fd, buf, BSIZE) != BSIZE) {
		fsbclear(buf, BSIZE);
		fprintf(stderr, "libfs: read error %D\n", (long)bn);
		if (fsp->pexstat != NULL)
			*fsp->pexstat |= fsp->hardbit;
		return (-1);
	}
	return (0);
}

/*
 * Write `buf' to block `bn'.  Returns 0 on success, -1 on a range or I/O
 * error (or if the volume was not opened writable).
 */
int
fsbwrite(fsp, bn, buf)
register FS *fsp;
daddr_t bn;
char *buf;
{
	if (!fsp->writable || bn < 0 || bn >= fsp->fsize) {
		fprintf(stderr, "libfs: bad block #%D\n", (long)bn);
		if (fsp->pexstat != NULL)
			*fsp->pexstat |= fsp->hardbit;
		return (-1);
	}
	lseek(fsp->fd, (size_t)BSIZE * bn, 0);
	if (write(fsp->fd, buf, BSIZE) != BSIZE) {
		fprintf(stderr, "libfs: write error %D\n", (long)bn);
		if (fsp->pexstat != NULL)
			*fsp->pexstat |= fsp->hardbit;
		return (-1);
	}
	return (0);
}

/*
 * Clear `nb' bytes at `bp'.
 */
void
fsbclear(bp, nb)
register char *bp;
register unsigned nb;
{
	if (nb)
		do {
			*bp++ = 0;
		} while (--nb);
}

/*
 * Copy `nb' bytes from `in' to `out'.
 */
void
fsbcopy(in, out, nb)
register char *in, *out;
register unsigned nb;
{
	if (nb)
		do {
			*out++ = *in++;
		} while (--nb);
}
