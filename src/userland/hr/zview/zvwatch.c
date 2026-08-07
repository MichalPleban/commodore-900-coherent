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
 */
#include <signal.h>
#include <errno.h>

main()
{
	int w, st, pid, fd;
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
