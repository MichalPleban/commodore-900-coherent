/*
 * Copyright (c) 1977-1995 Robert Swartz.
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <time.h>
struct tm *localtime();
long time();
char *getwd();

char mon[12][4] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

main()
{
	long clock;
	register struct tm *tp;
	register char *pwd;

	time(&clock);
	tp = localtime(&clock);
	pwd = getwd();
	printf("%s/%d/%s/%d\n",
		pwd, tp->tm_mday, mon[tp->tm_mon], tp->tm_year%100);
}
