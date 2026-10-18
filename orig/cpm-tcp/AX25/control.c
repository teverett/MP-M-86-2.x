/* control.c */
/* Copyright 1984 by Philip Karn, KA9Q
 * Permission granted for noncommercial copying
 * and use provided this notice is retained
 */

/* Level 2 interface primitives provided to level 3 */

#include "ax25.h"

/* Initiate connection:
 *
 * connect linenumber destination [digipeater ...]
 *
 * 'dest' is an ascii string of callsigns, separated by spaces 
 * the first callsign is the destination; optional additional
 * callsigns are the intervening repeaters
 */
connect(argc,argv)
int argc;
char *argv[];
{
	struct addr *ap;
	struct link *link;
	struct addr dest;
	unsigned lineno;
	int i;

	if(argc < 3){
		tprintf("usage: %s lineno call [digipeater] ...\n",
		argv[0]);
		return;
	}
	lineno = atoi(argv[1]);	
	if(lineno >= nlines){
		tprintf("Line %d out of range\n",lineno);
		return;
	}
	if(setcall(&dest,argv[2]) == -1){
		tprintf("Invalid address %s\n",argv[2]);
		return;
	}
	link = find_link(&dest);
	if(link == NULL){
		link = cr_link(&dest);
	} else if(link->state == SETUP){
		tprintf("Connect already in progress to %s\n",pcall(&dest));
		return;	/* No action if connection already pending */
	}
	link->line = lines[lineno];

	/* Initialize digipeater string, if any */
	ap = &link->address[2];
	for(i=3; i < argc; i++){
		if(ap >= &link->address[MAXRPT+2]){
			tprintf("Extra digipeaters ignored\n");
			break;
		}
		if(setcall(ap++,argv[i]) == -1){
			tprintf("Invalid digipeater %s\n",argv[i]);
			return;
		}
	}
	link->addrlen = ap - &link->address[0];
	link->address[link->addrlen-1].ssid |= E;
	setstate(link,SETUP);
	sendctl(link,COMMAND,SABM|PF);
	start_timer(&link->t1);
	return;
}

/* Initiate disconnect with current station
 * If disconnect is already in progress, abort wait for acknowledgement
 * This is a slight change from the protocol spec, which specifies
 * that a 'local stop' command is to be ignored while a disconnect
 * is pending.
 */
disconnect(argc,argv)
int argc;
char *argv[];
{
	struct link *link;
	struct addr addr;

	if(argc < 2){
		/* Default is current link */
		link = term.tlink;
		term.tlink = NULL;
	} else {
		setcall(&addr,argv[1]);
		link = find_link(&addr);
	}
	if(link == NULL || link->state == DISCONNECTED){
		tprintf("No current connection\n");
		return;
	}
	if(link->state == DISCPENDING){
		setstate(link,DISCONNECTED);
		return;
	}
	setstate(link,DISCPENDING);
	sendctl(link,COMMAND,DISC|PF);
	start_timer(&link->t1);
	return;
}
#ifdef	FOO
/*
 * Read data from receive buffer.  This involves a copy, so it
 * is provided for convenience only.  It is much more efficient
 * to take buffers off the receive queue and access the data directly
 */
int
recv(link,buf,n)
struct link *link;
register char *buf;
unsigned n;
{
	struct frame *fp;
	register unsigned cnt,i;

	if(link == NULL)
		return -1;
	i = 0;
	cnt = 0;
	while(n != 0) {
		if((fp = getframe(&link->rxq)) == NULL)
			break;
		i = n > fp->iocnt ? fp->iocnt : n;
		movmem(fp->iop,buf,i);
		buf += i;
		n -= i;
		cnt += i;
	}
	return cnt;
}
#endif
/*
 * Send data from user buffer.
 */
int
send(link,level3,buf,n)
struct link *link;
char level3;
register char *buf;
unsigned n;
{
	register unsigned cnt,i;
	struct frame *fp;

	if(link == NULL)
		return -1;
	i = 0;
	cnt = 0;
	while(n != 0) {
		i = n > link->line->psize ? link->line->psize : n;
		fp = allocframe(i+1);
		if(level3 != 0){
			*fp->iop = level3;
			fp->iocnt++;
		}
		movmem(buf,fp->iop+1,i);
		fp->iocnt += i;
		putframe(&link->txq,fp);
		buf += i;
		n -= i;
		cnt += i;
	}
	ltxstart(link);
	return cnt;
}

/* Level 2 processing - first empty HDLC receiver queues into
 * the level 2 protocol, then route the received level 2 I-frames
 * to the appropriate level 3 protocol.
 */
level2()
{
	register int i;
	register struct frame *fp;
	register struct link *link;

	/* Check for incoming link frames and process them */
	for(i=0;i<nlines;i++){
		while((fp = getframe(&lines[i]->rxq)) != NULL){
			addrproc(i,fp);
		}
	}
	/* Do per-link processing.
	/* Process incoming level 2 frames; also un-flow-control link
	 * if it was previously flow controlled and memory has become
	 * available.
	 */
	for(i=0;i<NHASH;i++){
		for(link=hashtab[i];link != NULL;link = link->next){
			while((fp = getframe(&link->rxq)) != NULL){
				protosw(link,fp);
			}
			if(memused < memlimit && link->localbusy){
				link->localbusy = NO;
				sendctl(link,RESPONSE,RR);
			}
		}
	}
}
/* Route an incoming I-frame to the appropriate level 3 protocol */
protosw(link,fp)
struct link *link;
register struct frame *fp;
{
	register char proto;

	proto = *fp->iop++;
	fp->iocnt--;

	switch(proto){
	case NOLEVEL3:
		putframe(&term.txq,fp);
		break;
	default:	/* Ignore unknown protocols */
		if(link->tstate){
			tprintf("%s: Unknown level 3 protocol %x\n",
			 pcall(&link->address[0]),proto);
		}
		freeframe(fp);
		break;
	}
}
