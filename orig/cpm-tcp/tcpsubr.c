#include "machdep.h"
#include "timer.h"
#include "mbuf.h"
#include "netuser.h"
#include "internet.h"
#include "tcp.h"

struct tcb *tcbs[NTCB];

/* Lookup connection, return TCB pointer or NULL if nonexistant */
struct tcb *
lookup_tcb(conn)
struct connection *conn;
{
	register struct tcb *tcb;
	int16 hash_tcb();	

	tcb = tcbs[hash_tcb(conn)];
	while(tcb != NULL){
		/* Yet another structure compatibility hack */
		if(conn->local.address == tcb->conn.local.address
		 && conn->remote.address == tcb->conn.remote.address
		 && conn->local.port == tcb->conn.local.port
		 && conn->remote.port == tcb->conn.remote.port)
			break;
		tcb = tcb->next;
	}
	return tcb;
}

/* Create a TCB, return pointer. Return pointer if TCB already exists. */
struct tcb *
create_tcb(conn)
struct connection *conn;
{
	char *calloc();
	register struct tcb *tcb;
	void tcp_timeout(),tcp_msl();
	void link_tcb();

	if((tcb = lookup_tcb(conn)) != NULL)
		return tcb;
	if((tcb = (struct tcb *)calloc(1,sizeof (struct tcb))) == NULL)
		return NULL;
	bcopy((char *)conn,(char *)&tcb->conn,sizeof(struct connection));

	tcb->mss = DEF_MSS;
	tcb->srtt = DEF_RTT * MSPTICK;

	tcb->msl.start = MSL2;
	tcb->msl.func = tcp_msl;
	tcb->msl.arg = (int *)tcb;

	tcb->retrans.func = tcp_timeout;
	tcb->retrans.arg = (int *)tcb;

	link_tcb(tcb);
	return tcb;
}

/* Close our TCB */
void
close_self(tcb,reason)
register struct tcb *tcb;
char reason;
{
	struct reseq *rp,*rp1;

	stop_timer(&tcb->retrans);
	stop_timer(&tcb->msl);
	stop_timer(&tcb->rtt);
	tcb->reason = reason;

	/* Flush reassembly queue; nothing more can arrive */
	for(rp = tcb->reseq;rp != NULL;rp = rp1){
		rp1 = rp->next;
		free_p(rp->bp);
		free((char *)rp);
	}
	tcb->reseq = NULL;
	/* If this is an abnormal close that occured after receiving
	 * a SYN but before seeing a FIN, notify the user so he can empty
	 * any unpushed stuff that might be in the receive queue.
	 */
	if(tcb->r_upcall){
		switch(tcb->state){
		case SYN_RECEIVED:
		case ESTABLISHED:
		case FINWAIT1:
		case FINWAIT2:
			(*tcb->r_upcall)(tcb,tcb->rcvcnt);
		}
	}
	setstate(tcb,CLOSED);
}

/* Determine initial sequence number */
int32
iss()
{
	static int32 seq;

	seq += 250000;
	return seq;
}

/* Sequence number comparisons
 * Return true if x is between low and high inclusive,
 * false otherwise
 */
int
seq_within(x,low,high)
register int32 x,low,high;
{
	if(low <= high){
		if(low <= x && x <= high)
			return 1;
	} else {
		if(low >= x && x >= high)
			return 1;
	}
	return 0;
}
int
seq_lt(x,y)
register int32 x,y;
{
	return (long)(x-y) < 0;
}
int
seq_le(x,y)
register int32 x,y;
{
	return (long)(x-y) <= 0;
}
int
seq_gt(x,y)
register int32 x,y;
{
	return (long)(x-y) > 0;
}
int
seq_ge(x,y)
register int32 x,y;
{
	return (long)(x-y) >= 0;
}

/* Hash a connect structure into the hash chain header array */
static int16
hash_tcb(conn)
struct connection *conn;
{
	register int16 hval;

	/* Compute hash function on connection structure */
	hval = hiword(conn->remote.address);
	hval ^= loword(conn->remote.address);
	hval ^= hiword(conn->local.address);
	hval ^= loword(conn->local.address);
	hval ^= conn->remote.port;
	hval ^= conn->local.port;
	hval %= NTCB;
	return hval;
}
/* Insert TCB at head of proper hash chain */
void
link_tcb(tcb)
register struct tcb *tcb;
{
	register struct tcb **tcbhead;
	int16 hash_tcb();

	tcb->prev = NULL;
	tcbhead = &tcbs[hash_tcb(&tcb->conn)];
	tcb->next = *tcbhead;
	if(tcb->next != NULL){
		tcb->next->prev = tcb;
	}
	*tcbhead = tcb;
}
/* Remove TCB from whatever hash chain it may be on */
void
unlink_tcb(tcb)
register struct tcb *tcb;
{
	register struct tcb **tcbhead;
	int16 hash_tcb();

	tcbhead = &tcbs[hash_tcb(&tcb->conn)];
	if(*tcbhead == tcb)
		*tcbhead = tcb->next;	/* We're the first one on the chain */
	if(tcb->prev != NULL)
		tcb->prev->next = tcb->next;
	if(tcb->next != NULL)
		tcb->next->prev = tcb->prev;
}
void
setstate(tcb,newstate)
register struct tcb *tcb;
register char newstate;
{
	register char oldstate;

	oldstate = tcb->state;
	tcb->state = newstate;
	if(tcb->s_upcall){
		(*tcb->s_upcall)(tcb,oldstate,newstate);
	}
	/* Notify the user that he can begin sending data */
	if(tcb->t_upcall && newstate == ESTABLISHED){
		(*tcb->t_upcall)(tcb,tcb->window - tcb->sndcnt);
	}
}
