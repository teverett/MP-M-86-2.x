#define	NULL 0

#include "machdep.h"
#include "mbuf.h"
#include "internet.h"

#ifdef	LITTLE_ENDIAN
/* Machine-dependent byteorder routines for Little-Endian machines.
/* Should be rewritten in assembler for speed.
 * The Z-80 and 8088 are consistent Little Endians, while the network standard
 * is Big Endian. Therefore all bytes get swapped.
 */

#ifndef	Z80	/* ASM routines available for z-80 */
#ifndef	pdp11
/* Network to host int32 */
int32
ntohl(x)
int32 x;
{
	register int32 ret;

	ret = ntohs(loword(x));
	ret <<= 16;
	ret |= ntohs(hiword(x));
	return ret;

}
#else	/* The PDP-11 is a special case; longs are 2 3 0 1 */
int32
ntohl(x)
int32 x;
{
	register int32 ret;

	ret = ntohs(hiword(x));
	ret <<= 16;
	ret |= ntohs(loword(x));
	return ret;
}
#endif pdp11
/* Network to host short */
int16
ntohs(x)
int16 x;
{
	return (lobyte(x) << 8) | hibyte(x);
}	
#endif Z80
#endif LITTLE_ENDIAN

/* Internet Checksum routine -- should work on either little or big-endian
 * machines as long as their bytes are 8 bits long
 */
#ifndef	Z80
int16
cksum(ph,bp,length)
struct pseudo_header *ph;	/* Optional IP pseudo-header, MACHINE ORDER */
struct mbuf *bp;
int16 length;
{
	register char *buf;
	register int16 cnt,tot;
	register int32 sum;

	sum = 0;
	if(ph != NULL){
		sum = hiword(ph->source);
		sum += loword(ph->source);
		sum += hiword(ph->dest);
		sum += loword(ph->dest);
		sum += ph->protocol & 0xff;
		sum += ph->length;
	}
	tot = 0;
	while(bp != NULL){
		cnt = bp->cnt;
		buf = bp->data;
		while(cnt != 0 && tot < length){
			if(tot & 1){
				/* Low order byte */
				sum += (int16)(*buf++ & 0xff);
			} else {
				/* High order byte */
				sum += (int16)((*buf++ << 8) & 0xff00);
			}
			tot++;		
			cnt--;
		}
		if(tot == length)
			break;
		bp = bp->next;
	}
	/* All done */
	sum = (int16)(sum & 0xffff) + (int16)(sum >> 16);
	sum = (int16)(sum & 0xffff) + (int16)(sum >> 16);
	return(((int16)(~sum & 0xffff)));
}
#else
/* Z-80 version with assembler assist (it needs it!) */
int16
cksum(ph,bp,length)
struct pseudo_header *ph;	/* Optional IP pseudo-header, MACHINE ORDER */
struct mbuf *bp;
int16 length;
{
	register char *buf;
	register int16 cnt,tot;
	register int32 sum;
	int16 csum();

	sum = 0;
	if(ph != NULL){
		sum = hiword(ph->source);
		sum += loword(ph->source);
		sum += hiword(ph->dest);
		sum += loword(ph->dest);
		sum += ph->protocol & 0xff;
		sum += ph->length;
	}
	tot = 0;
	while(bp != NULL){
		cnt = min(bp->cnt,length-tot);
		buf = bp->data;
		/* check for leading odd boundary */
		if(tot & 1){
			sum += (int16)(*buf++ & 0xff);
			cnt--;
			tot++;
		}
		sum += csum((int16 *)buf,cnt>>1);
		/* Check for trailing odd boundary */
		if(cnt & 1){
			sum += (int16)((buf[cnt-1] << 8) & 0xff00);
		}
		tot += cnt;
		if(tot == length)
			break;
		bp = bp->next;
	}
	/* All done */
	sum = (int16)(sum & 0xffff) + (int16)(sum >> 16);
	sum = (int16)(sum & 0xffff) + (int16)(sum >> 16);
	return(((int16)(~sum & 0xffff)));
}
#endif	Z80

bcmp(a,b,n)
register char *a,*b;
register int16 n;
{
	while(n-- != 0){
		if(*a++ != *b++)
			return 1;
	}
	return 0;
}
#ifndef	Z80	/* AZTEC has the movmem() routine */
bcopy(a,b,n)
register char *a,*b;
register int16 n;
{
	while(n-- != 0)
		*b++ = *a++;
}
#endif Z80
