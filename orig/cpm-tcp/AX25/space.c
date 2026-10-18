/* space.c */
/* Copyright 1984 by Philip Karn, KA9Q
 * Permission granted for noncommercial copying
 * and use provided this notice is retained
 */

/* Storage definitions
 * Definition of SPACE causes
 * actual storage to be allocated here
 */
#define	SPACE
#include "ax25.h"
char *pstate[] = {
	"discon",
	"con pend",
	"discpend",
	"connect",
	"FRMR",
};
char *timstate[] = {
	"stopped",
	"running",
	"expired"
};
char *lstate[] = {
	"idle",
	"defer",
	"keyup",
	"start",
	"active",
	"fin1",
	"fin2",
};
char *lmodes[] = {
	"csma",
	"aloha",
	"full duplex",
	NULL
};
