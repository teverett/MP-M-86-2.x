/* cmdtab.c */
/* Copyright 1984 by Philip Karn, KA9Q
 * Permission granted for noncommercial copying
 * and use provided this notice is retained
 */

/* Command table, including defaults for each parameter */

#include "ax25.h"
#include "command.h"

/* Immediate commands */
struct cmdtab cmdtab[] = {

/* name		function */

"btext",	setbt,
"connect",	connect,
"convers",	convers,
"disconnect",	disconnect,
"lineparam",	lineparam,
"linkparam",	linkparam,
"load",		load,
"memory",	memory,
"mycall",	setmyc,
"purge",	purge,
"reset",	reset,
"save",		save,
"term",		termparam,
"trans",	trans,
"unproto",	setunp,
NULL,		NULL,
};

/* Address related parameters */


/* Terminal related paramters
 * I play some liberties with type casting here and in cmdrout.c,
 * but it should be portable since C guarantees that casting any pointer
 * into a char * and back again is OK. If only unions accepted
 * initializers properly...
 */

struct vartab termtab[] = {

/* name		variable		type	min	max */
"blen",		(char *)&term.awlen,	BYTE,	7,	8,
"bits",		(char *)&term.abit,	BYTE,	1,	2,
"canline",	(char *)&term.canline,	CHAR,	0,	0x7f,
"canpac",	(char *)&term.canpac,	CHAR,	0,	0x7f,
"command",	(char *)&term.command,	CHAR,	0,	0x7f,
"cr",		(char *)&term.cr,	BOOL,	OFF,	ON,
"delete",	(char *)&term.delete,	CHAR,	0,	0x7f,
"echo",		(char *)&term.echo,	BOOL,	OFF,	ON,
"flow",		(char *)&term.flow,	BOOL,	OFF,	ON,
"hiwat",	(char *)&term.hiwat,	NUM,	1,	FIFOSIZE,
"lowat",	(char *)&term.lowat,	NUM,	0,	FIFOSIZE-1,
"parity",	(char *)&term.parity,	BYTE,	0,	4,
"pass",		(char *)&term.pass,	CHAR,	0,	0x7f,
"sendpac",	(char *)&term.sendpac,	CHAR,	0,	0x7f,
"speed",	(char *)&term.speed,	NUM,	50,	19200,
"start",	(char *)&term.start,	CHAR,	0,	0x7f,
"stop",		(char *)&term.stop,	CHAR,	0,	0x7f,
"xoff",		(char *)&term.xoff,	CHAR,	0,	0x7f,
"xon",		(char *)&term.xon,	CHAR,	0,	0x7f,
NULL,
};

struct line lt;

struct vartab linetab[] = {

/* name		address			type	min	max */
"digipeat",	(char *)&lt.digipeat,	BOOL,	OFF,	ON,
"mode",		(char *)&lt.mode,	BYTE,	0,	2,
"psize",	(char *)&lt.psize,	NUM,	64,	MAXPSIZE,
"speed",	(char *)&lt.speed,	NUM,	0,	65535,
"tin",		(char *)&lt.tin,	BYTE,	OFF,	TDATA,
"tout",		(char *)&lt.tout,	BYTE,	OFF,	TDATA,
"tused",	(char *)&lt.tused,	BYTE,	OFF,	TDATA,
"txdelay",	(char *)&lt.ktimer.start,NUM,	0,	255,
NULL
};

struct link ln;
struct vartab linktab[] = {

/* name		address			type	min	max */

"ackdel",	(char *)&ln.t2.start,	NUM,	0,	65535,
"frack",	(char *)&ln.t1.start,	NUM,	0,	65535,
"idlepoll",	(char *)&ln.t3.start,	NUM,	0,	65535,
"maxframe",	(char *)&ln.maxframe,	NUM,	1,	7,
"newproto",	(char *)&ln.nproto,	BOOL,	OFF,	ON,
"retry",	(char *)&ln.n2,		NUM,	0,	65535,
"tfrmr",	(char *)&ln.tfrmr,	BYTE,	OFF,	TDATA,
"tstate",	(char *)&ln.tstate,	BYTE,	OFF,	TDATA,
"ttimer",	(char *)&ln.ttimer,	BYTE,	OFF,	TDATA,
NULL
};
