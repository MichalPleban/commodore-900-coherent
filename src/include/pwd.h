struct	passwd
{
	char	*pw_name;	/* User name */
	char	*pw_passwd;	/* Encrypted password */
	int	pw_uid;		/* User id */
	int	pw_gid;		/* Group id */
	int	pw_quota;	/* FIle space quota */
	char	*pw_comment;	/* Comments */
	char	*pw_gecos;	/* Gecos box number */
	char	*pw_dir;	/* Working directory */
	char	*pw_shell;	/* Shell */
};

struct	passwd	*getpwent();
struct	passwd	*getpwuid();
struct	passwd	*getpwnam();
