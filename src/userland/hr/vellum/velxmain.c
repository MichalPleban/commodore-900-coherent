/*
 * velxmain.c - entry point of the velxport HELPER binary
 * (/usr/vellum/lib/velxport): vellum -print/-pic/-net exec this.  The
 * helper is velbase + velfile + velport + velwalk + velv5 + velv6 with
 * NO gfx library, so every export runs headless -- from make(1), from a
 * script, from a serial terminal.
 *
 * On a machine with no hi-res card, run THIS BINARY: the editor is a
 * client of the shared graphics library, which is not loaded where
 * there is no card, so `vellum -print' cannot even exec there to hand
 * its arguments over.  The shipped Makefile skeletons name this path
 * for exactly that reason.
 */
#include <stdio.h>
#include "vellum.h"

main(argc, argv)
char **argv;
{
	exit(velxport(argc, argv));
}
