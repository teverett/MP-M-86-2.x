/* User calls to TCP */
#include "machdep.h"
#include "timer.h"
#include "mbuf.h"
#include "netuser.h"
#include "internet.h"
#include "ip.h"
#include "tcp.h"

struct tcb *
open_tcp(lsocket,fsocket,active,window,r_upcall,t_upcall,s_upcall,tos)
struct socket *lsocket;	/* Local socket */
struct socket *fsocket;	/* Remote socket */
int active;		/* Active/passive */
int16 window;		/* Receive window (and send buffer) sizes */
void (*r_upcall)();	/* Function to call when data arrives */
void (*t_upcall)();	/* Function to call when ok to send more data */
void (*s_upcall)();	/* Function to call when connection state changes */
char tos;
{
	struct connection conn;
	register struct tcb *tcb;
	void send_syn();

	if(lsocket == NULL){
		net_error = INVALID;
		return NULL;
	}
	conn.local.address = lsocket->address;
	conn.local.port = lsocket->port;
	if(fsocket != NULL){
		conn.remote.address = fsocket->address;
		conn.remote.port = fsocket->port;
	} else {
		conn.remote.address = 0;
		conn.remote.port = 0;
	}
	if(lookup_tcb(&conn) != NULL){
		net_error = CON_EXISTS;
		return NULL;
	}
	if((tcb = create_tcb(&conn)) == NULL){
		net_error = NO_SPACE;
		return NULL;
	}
	if(window != 0)
		tcb->window = tcb->rcv.wnd = window;
	else
		tcb->window = tcb->rcv.wnd = DEF_WND;
	tcb->r_upcall = r_upcall;
	tcb->t_upcall = t_upcall;
	tcb->s_upcall = s_upcall;
	tcb->tos = tos;
	if(!active){
		setstate(tcb,LISTEN);
		return tcb;
	}
	/* Send SYN, go into SYN_SENT state */
	send_syn(tcb);
	setstate(tcb,SYN_SENT);
	tcp_output(tcb);
	return tcb;
}
/* User send routine */
int
send_tcp(tcb,bp)
register struct tcb *tcb;
struct mbuf *bp;
{
	int16 cnt;

	if(tcb == NULL || bp == NULL){
		free_p(bp);
		net_error = INVALID;
		return -1;
	}
	cnt = len_mbuf(bp);
	/* If this would overfill our send queue, reject it entirely */
	if(tcb->sndcnt + cnt > tcb->window){
		free_p(bp);
		net_error = WOULDBLK;
		return -1;
	}
	switch(tcb->state){
	case CLOSED:
		free_p(bp);
		net_error = NO_CONN;
		return -1;
	case LISTEN:	/* Change state from passive to active */
		send_syn(tcb);
		setstate(tcb,SYN_SENT);	/* Note fall-thru */
	case SYN_SENT:
	case SYN_RECEIVED:
	case ESTABLISHED:
	case CLOSE_WAIT:
		append(&tcb->sndq,bp);
		tcb->sndcnt += cnt;
		tcp_output(tcb);
		break;
	case FINWAIT1:
	case FINWAIT2:
	case CLOSING:
	case LAST_ACK:
	case TIME_WAIT:
		free_p(bp);
		net_error = CON_CLOS;
		return -1;
	}
	return cnt;
}
/* User receive routine */
int
recv_tcp(tcb,bp,cnt)
register struct tcb *tcb;
struct mbuf **bp;
int16 cnt;
{
	if(tcb == NULL || bp == NULL){
		net_error = INVALID;
		return -1;
	}
	/* cnt == 0 means "I want it all" */
	if(cnt == 0)
		cnt = tcb->rcvcnt;
	/* If there's something on the queue, just return it regardless
	 * of the state we're in.
	 */
	if(tcb->rcvcnt != 0){
		/* See if the user can take all of it */
		if(tcb->rcvcnt <= cnt){
			cnt = tcb->rcvcnt;
			*bp = tcb->rcvq;
			tcb->rcvq = NULL;
		} else {
			if((*bp = alloc_mbuf(cnt)) == NULL){
				net_error = NO_SPACE;
				return -1;
			}
			pullup(&tcb->rcvq,(*bp)->data,cnt);
			(*bp)->cnt = cnt;
		}
		tcb->rcvcnt -= cnt;
		tcb->rcv.wnd += cnt;
		/* Do a window update if it was closed */
		if(cnt == tcb->rcv.wnd){
			tcb->force = 1;
			tcp_output(tcb);
		}
		return cnt;
	} else {
		/* If there's nothing on the queue, our action depends on what state
		 * we're in (i.e., whether or not we're expecting any more data).
		 * If no more data is expected, then simply return 0; this is
		 * interpreted as "end of file".
		 */
		switch(tcb->state){
		case LISTEN:
		case SYN_SENT:
		case SYN_RECEIVED:
		case ESTABLISHED:
		case FINWAIT1:
		case FINWAIT2:
			*bp = NULL;
			net_error = WOULDBLK;
			return -1;
		case CLOSED:
		case CLOSE_WAIT:
		case CLOSING:
		case LAST_ACK:
		case TIME_WAIT:
			*bp = NULL;
			return 0;
		}
	}
	return 0;	/* Not reached, but lint doesn't know that */
}
/* This really means "I have no more data to send". It only closes the
 * connection in one direction, and we can continue to receive data
 * indefinitely.
 */
int
close_tcp(tcb)
register struct tcb *tcb;
{
	if(tcb == NULL){
		net_error = INVALID;
		return -1;
	}
	switch(tcb->state){
	case LISTEN:
	case SYN_SENT:
		close_self(tcb,NORMAL);
		return 0;
	case SYN_RECEIVED:
	case ESTABLISHED:
		tcb->sndcnt++;
		setstate(tcb,FINWAIT1);
		tcp_output(tcb);
		return 0;
	case CLOSE_WAIT:
		tcb->sndcnt++;
		setstate(tcb,LAST_ACK);
		tcp_output(tcb);
		return 0;
	case FINWAIT1:
	case FINWAIT2:
	case CLOSING:
	case LAST_ACK:
	case TIME_WAIT:
		net_error = CON_CLOS;
		return -1;
	}
	return -1;	/* "Can't happen" */
}
/* Delete TCB, free resources */
int
del_tcp(tcb)
register struct tcb *tcb;
{
	void unlink_tcb();

	if(tcb == NULL){
		net_error = INVALID;
		return -1;
	}
	if(tcb->state != CLOSED)
		close_self(tcb,NORMAL);
	unlink_tcb(tcb);
	free_p(tcb->rcvq);
	free_p(tcb->sndq);
	free((char *)tcb);
	return 0;
}
