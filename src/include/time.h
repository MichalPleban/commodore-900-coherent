/*
 * Header file for localtime()
 */

struct	tm {
	int	tm_sec;
	int	tm_min;
	int	tm_hour;
	int	tm_mday;
	int	tm_mon;
	int	tm_year;
	int	tm_wday;
	int	tm_yday;
	int	tm_isdst;
};

struct	tm	*localtime();
struct	tm	*gmtime();
char	*asctime();
char	*ctime();
extern	long	timezone;
extern	char	tzname[2][16];
