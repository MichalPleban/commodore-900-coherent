/*
 * locktest.c - isolated unit test for the hrgui drawing-lock primitives.
 *
 * Runs standalone on the plain serial-console emulator (../Emulator) -- NO GUI,
 * NO framebuffer, NO driver: the lock word is just an ordinary short in this
 * process's memory, so hr_lock/hr_unlock/TSET are exercised in isolation.  This
 * can only test the SINGLE-process semantics (acquire sets the word, a second
 * acquire on a held lock burns the whole spin budget, release clears it) and,
 * usefully, MEASURE how long the baseline's contended spin actually costs --
 * the number behind the "flood makes drawing batchy" report.  (Cross-process
 * contention needs the shared VRAM tail and the hi-res card; that is a separate
 * test.)
 */
#include <stdio.h>

struct tbuffer { long tb_utime, tb_stime, tb_cutime, tb_cstime; };

extern int	hr_lock();
extern int	hr_unlock();

main()
{
	short		lock;
	struct tbuffer	tb;
	long		u0, u1;

	lock = 0;
	printf("init:        lock=0x%x (want 0x0)\n", lock & 0xffff);

	hr_lock(&lock);
	printf("acquire:     lock=0x%x (want 0xffff)\n", lock & 0xffff);

	/* Second acquire on a lock we already hold: nobody will release it, so
	 * hr_lock spins its full ~4M deadlock-breaker budget and then proceeds.
	 * tb_utime is user-mode CPU in HZ=100 ticks, so this is the cost of one
	 * fully-contended spin. */
	times(&tb);  u0 = tb.tb_utime;
	hr_lock(&lock);
	times(&tb);  u1 = tb.tb_utime;
	printf("spin cost:   %ld ticks = ~%ld ms (full 4M-spin breaker)\n",
	       u1 - u0, (u1 - u0) * 10);

	hr_unlock(&lock);
	printf("release:     lock=0x%x (want 0x0)\n", lock & 0xffff);

	hr_lock(&lock);
	printf("re-acquire:  lock=0x%x (want 0xffff)\n", lock & 0xffff);
	hr_unlock(&lock);

	printf("LOCKTEST DONE\n");
	exit(0);
}
