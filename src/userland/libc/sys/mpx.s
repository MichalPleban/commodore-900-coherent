/
/ C interface for mpx system call(s).
/ mpx(cmd, vec);

.globl	mpx_

mpx_:
	sys	067		/55
	ret
