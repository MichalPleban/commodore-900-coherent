/*
 * Structure for the /etc/group file.
 */

struct group {
	char	*gr_name;
	char	*gr_passwd;
	int	gr_gid;
	char	**gr_mem;
};

struct	group	*getgrent();
struct	group	*getgrgid();
struct	group	*getgrnam();
