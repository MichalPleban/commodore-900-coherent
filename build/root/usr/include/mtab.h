/*
 * Structure for the mount table maintained by
 * `/etc/mount' and `/etc/umount'.
 * The file `/etc/mtab' is an array of these structures.
 */

#define	MNAMSIZ	32		/* Size of a mount filename */

struct	mtab {
	char	mt_name[MNAMSIZ];
	char	mt_special[MNAMSIZ];
	int	mt_flags;
};
