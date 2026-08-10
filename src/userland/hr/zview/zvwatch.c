/*
 * zvwatch.c - zview's crash watchdog as a TINY separate program.
 *
 * The watchdog is the process that outlives the server so a server death
 * never leaves the machine deaf (zview.c srvwatch has the full rationale:
 * /drv/hr owns the keyboard vector, so something must survive to unload
 * it).  It used to be the parent HALF OF A FORK of the server itself --
 * a full contiguous ~69 Kb copy of the zview image parked in RAM for the
 * whole session just to sit in wait().  srvwatch now execs this program
 * over that copy instead; it links libc only (a few Kb).
 *
 * Exec'd with exactly ONE child: the server (exec keeps children, and
 * srvwatch made SIGINT/SIGQUIT/SIGHUP ignored, which exec preserves --
 * they are re-ignored here only for belt and braces).  A clean quitwm()
 * exits 0 and has already unloaded the driver; anything else gets the
 * driver unloaded and the text console restored.
 *
 * A dead server is NOT a dead desktop: every client is a separate process
 * drawing straight into VRAM, and they all outlive the crash -- the old
 * symptom was a restored text console being painted over by the surviving
 * clock/terminals.  So before the driver goes, declare the session over
 * where every client looks: clear the tail magic (shmem.h HR_MAGIC) and
 * ring every event-ring doorbell.  A woken client finds the magic gone in
 * hr_evwait (hrlock.c) and exits; one parked in CIOEVWAIT is woken while
 * the driver still exists to wake it.  Only then unload.
 */
#include <signal.h>
#include <errno.h>
#include "shmem.h"

main()
{
	int w, st, pid, fd, i;
	static char msg[] =
	    "\033[E\007zview: server died -- screen and keyboard restored\r\n";

	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	while ( (w = wait(&st)) < 0 )
		if ( errno != EINTR )
		{
			/* No child at all: we were run by hand, not exec'd
			 * by srvwatch.  There is nothing to guard; tearing
			 * the driver down now would kill a healthy desktop. */
			exit(0);
		}
	if ( st == 0 )
		exit(0);			/* quitwm(): already cleaned up */
	/* Post-mortem breadcrumb: the wait() status of the dead server, in
	 * ASCII, on the root (low byte = the signal that killed it, e.g.
	 * 11 = SIGSEGV).  A crash on real hardware leaves no other trace. */
	{
		char nb[8];
		register int n, v;

		v = st & 0xffff;
		n = sizeof(nb);
		nb[--n] = '\n';
		do
			nb[--n] = '0' + v % 10;
		while ( (v /= 10) != 0 && n > 0 );
		if ( (fd = creat("/wscrash", 0644)) >= 0 )
		{
			write(fd, nb + n, sizeof(nb) - n);
			close(fd);
		}
	}
	/* End the session for the surviving clients: dead magic, then wake
	 * everyone so they see it (see the header comment).  The magic word
	 * is plain user-mapped VRAM, so this needs no driver; the wakes do,
	 * and on the Ctrl-Alt-HELP path the driver is already gone -- then
	 * the open fails and the timer-driven clients still exit off the
	 * magic alone. */
	hr_glob()->magic = 0;
	if ( (fd = open("/dev/dmgr", 2)) >= 0 )
	{
		for ( i = 0; i < EVQ_N; i++ )
			ioctl(fd, CIOEVWAKE, (char *)i);
		close(fd);
	}
	if ( (pid = fork()) == 0 )
	{
		execl("/etc/uload", "uload", "/drv/hr", (char *)0);
		_exit(1);
	}
	while ( pid > 0 && (w = wait(&st)) != pid && w >= 0 )
		;
	/* hrtty's clear-screen is ESC [ E (see zview.c wdconsole): wipe the
	 * dead desktop off the framebuffer and prove the console is back. */
	if ( (fd = open("/dev/console", 1)) >= 0 )
	{
		write(fd, msg, sizeof(msg) - 1);
		close(fd);
	}
	exit(0);
}
