#include "machdep.h"
#include "mbuf.h"

/* Primitive mbuf allocate/free routines */

/* Allocate mbuf with associated buffer of 'size' bytes */
struct mbuf *
alloc_mbuf(size)
register int16 size;
{
	register struct mbuf *bp;
	char *malloc();

	if((bp = (struct mbuf *)malloc(size + sizeof(struct mbuf))) == NULL)
		return NULL;
	bp->next = bp->anext = NULL;
	if(size != 0){
		bp->data = bp->_buf = (char *)(bp + 1);
	} else {
		bp->data = bp->_buf = NULL;
	}
	bp->cnt = 0;
	return bp;
}

/* Free all resources associated with mbuf */
free_mbuf(bp)
register struct mbuf *bp;
{
	if(bp != NULL){
		free((char *)bp);
	}
}

/* Free packet (a chain of mbufs). Return pointer to next packet on queue,
 * if any
 */
struct mbuf *
free_p(bp)
register struct mbuf *bp;
{
	struct mbuf *abp,*nbp;

	if(bp == NULL)
		return NULL;
	abp = bp->anext;
	while(bp != NULL){
		nbp = bp->next;
		free_mbuf(bp);
		bp = nbp;
	}
	return abp;
}		
/* Free entire queue of packets (of mbufs) */
free_q(q)
struct mbuf *q;
{
	register struct mbuf *bp;

	while((bp = dequeue(&q)) != NULL)
		free_p(bp);
}

/* Count up the total number of bytes in an mbuf */
int16
len_mbuf(bp)
register struct mbuf *bp;
{
	int cnt;

	cnt = 0;
	while(bp != NULL){
		cnt += bp->cnt;
		bp = bp->next;
	}
	return cnt;
}
/* Duplicate/enqueue/dequeue operations based on mbufs */

/* Duplicate first 'cnt' bytes of packet starting at 'offset'.
 * This is done without copying data; only the headers are duplicated,
 * but without data segments of their own. The pointers are set up to
 * share the data segments of the original copy. The return pointer is
 * passed back through the first argument, and the return value is the
 * number of bytes actually duplicated.
 */
int16
dup_p(hp,bp,offset,cnt)
struct mbuf **hp;
register struct mbuf *bp;
register int16 offset;
register int16 cnt;
{
	register struct mbuf *cp;
	int16 tot;

	if(bp == NULL || cnt == 0 || (*hp = cp = alloc_mbuf(0)) == NULL){
		return 0;
	}
	/* Skip over leading mbufs that are smaller than the offset */
	while(bp->cnt <= offset){
		offset -= bp->cnt;
		bp = bp->next;
	}
	tot = 0;
	for(;;){
		cp->data = bp->data + offset;
		cp->cnt = min(cnt,bp->cnt - offset);
		offset = 0;
		cnt -= cp->cnt;
		tot += cp->cnt;
		bp = bp->next;
		if(cnt == 0 || bp == NULL || (cp->next = alloc_mbuf(0)) == NULL)
			break;
		cp = cp->next;
	}
	return tot;
}
/* Copy first 'cnt' bytes of packet into a new, single mbuf */
struct mbuf *
copy_p(bp,cnt)
register struct mbuf *bp;
register int16 cnt;
{
	register struct mbuf *cp;
	register char *wp;
	register int16 n;

	if(bp == NULL || cnt == 0 || (cp = alloc_mbuf(cnt)) == NULL)
		return NULL;
	wp = cp->data;
	while(cnt != 0 && bp != NULL){
		n = min(cnt,bp->cnt);
		bcopy(bp->data,wp,n);
		wp += n;
		cp->cnt += n;
		cnt -= n;
		bp = bp->next;
	}
	return cp;
}
/* Copy and delete "cnt" bytes from beginning of packet. Return number of
 * bytes actually pulled off
 */
int16
pullup(bph,buf,cnt)
struct mbuf **bph;
char *buf;
int16 cnt;
{
	register struct mbuf *bp;
	int16 n,tot;

	tot = 0;
	if(bph == NULL)
		return 0;
	while(*bph != NULL && cnt != 0){
		bp = *bph;
		n = min(cnt,bp->cnt);
		if(buf != NULL){
			bcopy(bp->data,buf,n);
			buf += n;
		}
		tot += n;
		cnt -= n;
		bp->data += n;
		bp->cnt -= n;		
		if(bp->cnt == 0){
			*bph = bp->next;
			free_mbuf(bp);
			bp = *bph;
		}
	}
	return tot;
}
/* Append mbuf to end of mbuf chain */
int16
append(bph,bp)
struct mbuf **bph;
struct mbuf *bp;
{
	register struct mbuf *p;

	if(bph == NULL || bp == NULL)
		return;
	if(*bph == NULL){
		/* First one on chain */
		*bph = bp;
	} else {
		for(p = *bph ; p->next != NULL ; p = p->next)
			;
		p->next = bp;
	}
}
/* Append packet to end of packet queue */
void
enqueue(q,bp)
struct mbuf **q;
struct mbuf *bp;
{
	register struct mbuf *p;

	if(q == NULL || bp == NULL)
		return;
	if(*q == NULL){
		/* List is empty, stick at front */
		*q = bp;
	} else {
		for(p = *q ; p->anext != NULL ; p = p->anext)
			;
		p->anext = bp;
	}
}
/* Unlink a packet from the head of the queue */
struct mbuf *
dequeue(q)
register struct mbuf **q;
{
	register struct mbuf *bp;

	if(q == NULL)
		return NULL;
	if((bp = *q) != NULL){
		(*q) = bp->anext;
		bp->anext = NULL;
	}
	return bp;
}	

/* Copy user data into an mbuf */
struct mbuf *
qdata(data,cnt)
char *data;
int16 cnt;
{
	register struct mbuf *bp;

	if((bp = alloc_mbuf(cnt)) == NULL)
		return NULL;
	bcopy(data,bp->data,cnt);
	bp->cnt = cnt;
	return bp;
}
/* Copy mbuf data into user buffer */
int16
dqdata(bp,buf,cnt)
struct mbuf *bp;
char *buf;
unsigned cnt;
{
	unsigned n,tot;
	struct mbuf *bp1;

	if(buf == NULL)
		return 0;
	
	tot = 0;
	for(bp1 = bp;bp1 != NULL; bp1 = bp1->next){
		n = min(bp1->cnt,cnt);
		bcopy(bp1->data,buf,n);
		cnt -= n;
		buf += n;
		tot += n;
	}
	free_p(bp);
	return tot;
}
