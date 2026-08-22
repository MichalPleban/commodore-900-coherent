/*
 * velxmain.c - entry point of the velxport HELPER binary
 * (/usr/vellum/lib/velxport): vellum -print/-pic/-net exec this.  The
 * helper is velbase + velfile + velport with NO gfx library, so every
 * export runs headless -- from make(1), from a script, on a machine
 * with no hi-res card at all.
 */
#include <stdio.h>
#include "vellum.h"

main(argc, argv)
char **argv;
{
	exit(velxport(argc, argv));
}
