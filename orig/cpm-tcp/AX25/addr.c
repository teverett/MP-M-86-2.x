/* addr.c */
/* Copyright 1984 by Philip Karn, KA9Q
 * Permission granted for noncommercial copying
 * and use provided this notice is retained
 */

/* Code specific to the addressing conventions of AX.25 */

#include "ax25.h"

/* Return a pointer to the control byte in the given frame */
char *
cbyte(fp)
register struct frame *fp;
{
	register char *cp;
	register unsigned cnt;

	if(fp == NULL || fp->iop == NULL)
		return NULL;

	cnt = fp->iocnt;
	cp = fp->iop;
	while(cnt != 0 && (*cp & E) == 0){
		cnt--;
		cp++;
	}
	/* If the address field never ended, cnt = 0; if it ended
	 * on the last byte of a frame, cnt = 1. In either case,
	 * there is no control field
	 */
	if(cnt <= 1)
		return NULL;
	else
		return cp + 1;
}

/* Filter the address fields of incoming frames, moving the
 * frame buffer pointer to the X.25 control field.
 * Call process() for further work only on interesting frames.
 *
 * This function "compartmentalizes" the addressing changes made for
 * AX.25 from vanilla X.25, allowing the process() function to think
 * it's talking (almost) real X.25.
 */
addrproc(lineno,fp)
int lineno;
register struct frame *fp;
{
	register struct addr *ap,*ap1;
	unsigned naddr,addrsize;
	char cmdrsp,*control;
	struct link *link;
	struct line *line;
	int i;

	if(fp == NULL)
		return;
	line = lines[lineno];
	/* Trace all received frames? */
	if(line->tin)
		dump(lineno,fp,DUMPIN,line->tin);

	control = cbyte(fp);		/* control -> control byte */

	ap = (struct addr *)fp->iop;	/* -> address field */
	addrsize = control - (char *)ap;	/* # bytes in address field */
	/* Check for either a missing control byte or a residual length
	 * address field
	 */
	if(control == NULL || addrsize % sizeof(struct addr) != 0){
		freeframe(fp);
		return;
	}
	naddr = addrsize/sizeof(struct addr);	/* # addresses in address field */
	/* Check for invalid address field (too short or odd length) */	
	if(naddr < 2) {
		freeframe(fp);
		return;
	}
	/* Rescan, looking for our call in the repeater fields, if any.
	 * Repeat appropriate packets
	 */
	for(i=2,ap1 = &ap[2];i < naddr;i++,ap1++){
		if((ap1->ssid & REPEATED) == 0){
			/* Check if packet is directed to us as a digipeater */
			if(line->digipeat && addreq(ap1,&mycall)){
				ap1->ssid |= REPEATED;
				if(line->tout)
					dump(lineno,fp,DUMPORIG,line->tout);
				/* This needs to be made smarter, to
				 * pick a line for repeating depending
				 * on routing tables, etc
				 */
				putframe(&line->txq,fp);
				ltxint(lineno);
			} else {
				freeframe(fp);
			}
			return;
		}
	}
	/* Packet has passed all repeaters, now look at destination */
	ap1 = &ap[0];
	if(!addreq(ap1,&mycall)){
		freeframe(fp);
		return;
	}
	/* The frame is addressed to us as a destination.
	 * Find the source address in hash table
	 */
	link = find_link(++ap1);
	if(link == NULL){
		/* Creat a new link entry for this guy,
		 * insert into hash table keyed on his address,
		 * and initialize link entries
		 */
		link = cr_link(ap1);
		link->line = line;
		if(naddr > 2){
			/* Construct reverse digipeater path */
			revrpt(++ap1,&link->address[2],naddr-2);
			/* Scale timers to account for extra delay */
			link->t1.start *= (naddr-1);
			link->t2.start *= (naddr-1);
			link->t3.start *= (naddr-1);
		}
		link->addrlen = naddr;
		link->address[naddr-1].ssid |= E;
	}
	/*
	 * Determine command/response frame type from new address rules
	 */
	if((ap[1].ssid & C) != (ap[0].ssid & C)){
		if(ap[0].ssid & C)
			cmdrsp = COMMAND;
		else
			cmdrsp = RESPONSE;
		link->nproto = YES;
	} else {
		/* Old protocol in use; frame type is unknown */
		cmdrsp = UNKNOWN;
		link->nproto = NO;
	}
	if(line->tused)
		dump(lineno,fp,DUMPUSED,line->tused);
	fp->iop = control;		/* -> control byte */
	fp->iocnt -= addrsize;		/* # bytes in control, data fields */
	process(fp,link,cmdrsp);
}

/*
 * Queue a frame to be sent and kick HDLC transmitter.
 */
int
sendframe(link,cmdrsp,control,data,datalen)
register struct link *link;
char cmdrsp;	/* Command/response flag */
char control;	/* Control field */
char *data;		/* I-field */
unsigned datalen;	/* I-field length */
{
	register struct frame *fp;
	register struct addr *ap;
	char type;
	unsigned fsize,addrcnt;
	int lineno;	/* Index into hardware lines */

	lineno = link->line->lineno;
	type = ftype(control);

	/* Don't let out any unallowed I-fields */
	if(type != I && type != UI && type != FRMR && (data != NULL
	 || datalen != 0))
		return -1;
	addrcnt = link->addrlen * sizeof(struct addr);
	fsize = addrcnt + 1 + datalen;
	fp = allocframe(fsize);
	if(fp == NULL)
		return -1;
	ap = (struct addr *)fp->iop;
	/* Construct frame in buffer associated with the header */
	movmem((char *)link->address,fp->iop,addrcnt);
	/* points to ctl byte */
	fp->iop += addrcnt;
	fp->iocnt += addrcnt;

	/* Set command/response indication, if any (new protocol only) */
	if(link->nproto){
		switch(cmdrsp){
		case COMMAND:
			ap[0].ssid |= C;
			break;
		case RESPONSE:
			ap[1].ssid |= C;
			break;
		}
	} else {
		/* When talking the old protocol, clear the C bits and the PF bit
		 * to avoid confusing certain AX.25 implementations that don't
		 * ignore them like they should
		 */
		ap[0].ssid &= ~C;
		ap[1].ssid &= ~C;
		control &= ~PF;
	}
	*fp->iop++ = control;	/* fp->iop points to I field */
	fp->iocnt++;
	/* Assemble data field, if any */
	if(data != NULL && datalen != 0){
		movmem(data,fp->iop,datalen);
		fp->iocnt += datalen;	
	}
	/* Reset i/o pointer to beginning of buffer and get rid of it */
	fp->iop = fp->mem;
	if(type == FRMR && link->tfrmr)
		dump(lineno,fp,DUMPFRMR,link->tfrmr);
	else if(link->line->tout)
		dump(lineno,fp,DUMPORIG,link->line->tout);

	putframe(&link->line->txq,fp);
	ltxint(lineno);	/* Kick transmitter */
	return 0;
}
