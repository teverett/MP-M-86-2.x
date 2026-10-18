/* misc.c */
/* Copyright 1984 by Philip Karn, KA9Q
 * Permission granted for noncommercial copying
 * and use provided this notice is retained
 */
/* Miscellaneous AX.25 functions */
#include "ax25.h"
#include <ctype.h>

/*
 * setcall - convert callsign plus substation ID of the form
 * "KA9Q-0" to AX.25 (shifted) address format
 *   Address extension bit is left unset.
 *   Return -1 on error, 0 if OK
 */

int
setcall(out,call)
struct addr *out;
char *call;
{
	int csize;
	unsigned ssid;
	register int i;
	register char *cp,*dp;
	char c;

	if(out == NULL || call == NULL || *call == '\0'){
		return -1;
	}
	/* Find dash, if any, separating callsign from ssid
	 * Then compute length of callsign field and make sure
	 * it isn't excessive
	 */
	dp = index(call,'-');
	if(dp == NULL)
		csize = strlen(call);
	else
		csize = dp - call;
	if(csize > ALEN)
		return -1;
	/* Now find and convert ssid, if any */
	if(dp != NULL){
		dp++;	/* skip dash */
		ssid = atoi(dp);
		if(ssid > 15)
			return -1;
	} else
		ssid = 0;
	/* Copy upper-case callsign, left shifted one bit */
	cp = out->call;
	for(i=0;i<csize;i++){
		c = *call++;
		if(islower(c))
			c = toupper(c);
		*cp++ = c << 1;
	}
	/* Pad with shifted spaces if necessary */
	for(;i<ALEN;i++)
		*cp++ = ' ' << 1;
	
	/* Insert substation ID field and set reserved bits */
	out->ssid = 0x60 | (ssid << 1);
	return 0;
}

/* Convert AX.25 address back to ASCII string, e.g., KA9Q-0 */
char *
pcall(addr)
register struct addr *addr;
{
	static char buf[10];
	register char *cp,*cpa;
	register int i;
	int ssid;

	if(addr == NULL || addr->call[0] == '\0'){
		buf[0] = '\0';
		return buf;
	}
	for(cp=buf,cpa=addr->call,i=0 ; i<ALEN && *cpa != ' ' << 1 ; i++){
		*cp++ = (*cpa++ >> 1) & 0x7f;
	}
	*cp++ = '-';
	ssid = (addr->ssid >> 1) & 0xf;
	if(ssid > 9)
		*cp++ = '0' + ssid / 10;
	*cp++ = '0' + ssid % 10;
	*cp = '\0';
	return buf;
}

/* Compare addresses
 * Return 1 if match, 0 if not, -1 if invalid
 */
int
addreq(a,b)
struct addr *a,*b;
{
	register int i;
	register char *cpa,*cpb;

	if(a == NULL || b == NULL)
		return -1;
	cpa = a->call;
	cpb = b->call;
	for(i=0;i<ALEN;i++)
		if(*cpa++ != *cpb++)
			return 0;
	if((a->ssid & SSID) != (b->ssid & SSID))
		return 0;
	return 1;
}

/* Figure out the frame type from the control field
 * This is done by masking out any sequence numbers and the
 * poll/final bit after determining the general class (I/S/U) of the frame
 */
char
ftype(control)
register char control;
{
	if((control & 1) == 0)	/* An I-frame is an I-frame... */
		return I;
	if(control & 2)			/* U-frames use all except P/F bit for type */
		return(control & ~PF);
	else					/* S-frames use low order 4 bits for type */
		return(control & 0xf);
}

/* Reverse an digipeater string from 'in' to 'out'.
 * 'in' and 'out' may be the same, in which case revaddr
 * automatically uses temporary storage.
 */
int
revrpt(in,out,nrpt)
register struct addr in[];
struct addr out[];
unsigned nrpt;
{
	register struct addr *ap;
	unsigned length,i;
	char i_state;

	if(in == NULL || out == NULL || nrpt == 0)
		return -1;
	if(in == out){
		length = sizeof(struct addr) * nrpt;
		i_state = disable();
		ap = (struct addr *)malloc(length);
		restore(i_state);
	} else {
		ap = out;
	}
	/* Build reverse digipeater string */
	for(i=0;i<nrpt;i++){
		movmem((char *)&in[i],(char *)&ap[nrpt-i-1],sizeof(struct addr));
		ap[nrpt-i-1].ssid &= ~(E | REPEATED);
	}
	if(in == out){
		movmem((char *)ap,(char *)out,length);
		i_state = disable();
		free((char *)ap);
		restore(i_state);
	}
	return 0;
}

/* convert possible control character to printable string */
char *
visible(c)
register char c;
{
	static char ret[3];
	register char *cp;

	cp = ret;
	if(isprint(c)){
		*cp++ = c;
	} else {
		*cp++ = '^';
		*cp++ = ((c + 0x40) & 0x7f);
	}
	*cp++ = '\0';
	return ret;
}
