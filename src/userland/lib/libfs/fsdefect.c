/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * Copyright (c) 2026 Michal Pleban.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * libfs -- the bad-block / defect-run list.
 *
 * A Coherent volume records its defective blocks in the bad-block file
 * (i-number BADFIN).  fsfinddefective reads that file and builds a sorted list
 * of defective runs (fsp->deflist); the i-list scanners walk the list to skip
 * defective inode blocks.  fssavedefective inserts one block into the sorted
 * list, coalescing with an adjacent run; fsfreedefective releases it.  This is
 * the copy icheck and dcheck each used to carry.
 */
#include <stdio.h>
#include <fs.h>
#include <canon.h>

char	*malloc();

/*
 * Read the bad-block inode (BADFIN) and record every block it names as
 * defective.  Returns 0 (including when there is no bad-block file), -1 on a
 * hard error reading or interpreting the inode.
 */
int
fsfinddefective(fsp)
register FS *fsp;
{
	struct dinode ino;
	daddr_t pb, lb;

	if (fsbread(fsp, (daddr_t)iblockn(BADFIN), fsp->dbuf) < 0)
		return (-1);
	ino = ((struct dinode *)fsp->dbuf)[iblocko(BADFIN)];
	canshort(ino.di_mode);
	if (ino.di_mode == 0)
		return (0);
	if ((ino.di_mode & IFMT) != IFREG) {
		fprintf(stderr, "libfs: bad-block file has bad mode\n");
		if (fsp->pexstat != NULL)
			*fsp->pexstat |= fsp->hardbit;
		return (-1);
	}
	lb = 0;
	while ((pb = fsimap(fsp, &ino, lb++)) != 0)
		fssavedefective(fsp, pb);
	return (0);
}

/*
 * Free every run in the defective-block list.
 */
void
fsfreedefective(fsp)
register FS *fsp;
{
	register struct defect *cdsp1, *cdsp2;

	cdsp1 = fsp->deflist;
	fsp->deflist = NULL;
	while (cdsp1 != NULL) {
		cdsp2 = cdsp1->d_next;
		free((char *)cdsp1);
		cdsp1 = cdsp2;
	}
}

/*
 * Insert block `bn' into the sorted defective-block chain, merging it onto the
 * end of an adjacent run where possible.  Bad blocks are generally scooped up
 * in order and are sparsely placed, so no attempt is made to fuse two runs.
 */
void
fssavedefective(fsp, bn)
register FS *fsp;
daddr_t bn;
{
	register struct defect *cdsp1, *cdsp2, *cdsp3;

	cdsp1 = NULL;
	cdsp2 = fsp->deflist;
	while (cdsp2 != NULL && bn > cdsp2->d_start) {
		cdsp1 = cdsp2;
		cdsp2 = cdsp2->d_next;
	}
	if (cdsp1 != NULL && bn == cdsp1->d_start + cdsp1->d_length) {
		++cdsp1->d_length;
		return;
	}
	if (cdsp2 != NULL && bn == cdsp2->d_start - 1) {
		--cdsp2->d_start;
		++cdsp2->d_length;
		return;
	}
	if ((cdsp3 = (struct defect *)malloc(sizeof (struct defect))) == NULL) {
		fprintf(stderr, "libfs: out of space for bad blocks\n");
		if (fsp->pexstat != NULL)
			*fsp->pexstat |= fsp->hardbit;
		return;
	}
	if (cdsp1 == NULL)
		fsp->deflist = cdsp3;
	else
		cdsp1->d_next = cdsp3;
	cdsp3->d_next = cdsp2;
	cdsp3->d_start = bn;
	cdsp3->d_length = 1;
}
