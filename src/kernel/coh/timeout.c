/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
/*
 * Coherent.
 * Timeout management.
 */
#include <coherent.h>
#include <timeout.h>

/*
 * Given a pointer to a timeout structure, `tp', call the function `f'
 * with integer argument `a' in `n' ticks of the clock. The list is
 * searched to see if the specified timeout structure is already in the
 * list, and it is removed if already there.
 *
 * On entry, the driver invoking timeout is mapped.
 * The timl structure is in the kernel space.
 * While scanning the queue, only one driver space can be mapped at a time
 * hence the many dsave's, drest's, and dcopy's.
 */
timeout(tp, n, f, a)
register TIM *tp;
int n;
int (*f)();
char *a;
{
	register TIM *tp1;
	register TIM *tp2;
	register int s;
	dold_t dold;
	TIM *tnext;
	dmap_t tdmap;
	int ttinc;

	dsave(dold);
	s = sphi();
	if (tp->t_func != NULL) {
		/* Save linkages */
		tnext = tp->t_next;
		dcopy(tdmap, tp->t_dmap);
		ttinc = tp->t_tinc;
		/* Excise from queue */
		for (tp1=&timl; (tp2=tp1->t_next) != NULL; tp1=tp2) {
			if (tp2 == tp) {
				tp1->t_next = tnext;
				dcopy(tp1->t_dmap, tdmap);
				tp1->t_tinc += ttinc;
				break;
			}
			drest(tp1->t_dmap);
		}
	}
	drest(dold);
	if (f != NULL) {
		/* Find insertion point */
		for (tp1=&timl; (tp2=tp1->t_next) != NULL; tp1=tp2) {
			if ((n -= tp1->t_tinc) < 0) {
				n += tp1->t_tinc;
				break;
			}
			drest(tp1->t_dmap);
		}
		/* Insert into queue, observing map restrictions */
		tp1->t_next = tp;
		dcopy(tdmap, tp1->t_dmap);
		dcopy(tp1->t_dmap, dold);
		ttinc = tp1->t_tinc - n;
		tp1->t_tinc = n;
		drest(dold);
		tp->t_next = tp2;
		dcopy(tp->t_dmap, tdmap);
		tp->t_tinc = ttinc;
		tp->t_func = f;
		tp->t_farg = a;
	}
	spl(s);
}
