/*
 * ptytest.c - a minimal, non-GUI smoke test for the pty driver (GUI.md sec 4).
 *
 * Opens a master (/dev/ptyp0), forks a child that opens the matching slave
 * (/dev/ttyp0) as its stdin/stdout/stderr and execs an interactive shell, then
 * drives the shell from the master: writes a couple of commands and echoes
 * everything the shell produces to the console.  If the pty plumbing works you
 * see the shell's prompt, the echoed command, its output, then EOF when the
 * shell exits (master read returns 0 after the slave closes).
 *
 * Runnable from the single-user console with no windowing and no pipe(2), so it
 * validates the kernel driver in isolation (headless emulator).
 */
#include <stdio.h>

char	*master = "/dev/ptyp0";
char	*slave  = "/dev/ttyp0";

main()
{
	int m, s, pid, n;
	char buf[256];

	m = open(master, 2);
	if ( m < 0 )
	{
		printf("ptytest: cannot open master %s\n", master);
		exit(1);
	}
	printf("ptytest: master %s open on fd %d\n", master, m);

	pid = fork();
	if ( pid < 0 )
	{
		printf("ptytest: fork failed\n");
		exit(1);
	}
	if ( pid == 0 )
	{
		/* child: become the shell on the slave side */
		close(m);
		s = open(slave, 2);		/* claims slave as controlling tty */
		if ( s < 0 )
			_exit(1);
		dup2(s, 0);
		dup2(s, 1);
		dup2(s, 2);
		if ( s > 2 )
			close(s);
		execl("/bin/sh", "sh", "-i", (char *)0);
		_exit(127);
	}

	/* parent: feed the shell, relay its output to the console */
	sleep(1);
	write(m, "echo hello_from_pty\n", 20);
	write(m, "/bin/date\n", 10);
	write(m, "exit\n", 5);

	while ( (n = read(m, buf, sizeof buf)) > 0 )
		write(1, buf, n);

	printf("\nptytest: master read EOF (n=%d) -- shell exited, pty OK\n", n);
	exit(0);
}
