/
/ C interface for suload system call.
/ int suload(major) int major;

.globl	suload_
suload_:
	sys	0x41		/ 65
	ret
