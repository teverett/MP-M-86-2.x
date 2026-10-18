#include "machdep.h"
#include "timer.h"
#include "mbuf.h"
#include "netuser.h"
#include "internet.h"
#include "tcp.h"

/* Send a segment on the specified connection. One gets sent only
 * if there is data to be sent or if "force" is non zero
 */
void
tcp_output(tcb)
register struct tcb *tcb;
{
	struct pseudo_header ph;
	struct mbuf *hbp;
	struct tcp_header *tcph;
	int16 ssize;		/* Size of current segment being sent,
						 * including SYN and FIN flags */
	int16 dsize;		/* Size of segment less SYN and FIN */
	int16 usable;		/* Usable window */
	int16 sent;			/* Sequence count (incl SYN/FIN) already in the pipe */

	if(tcb == NULL)
		return;

	switch(tcb->state){
	case LISTEN:
	case CLOSED:
		return;	/* Don't send anything */
	}
	for(;;){
		sent = tcb->snd.ptr - tcb->snd.una;

		/* There can only be one outstanding segment in this state
		 * since the other end would reject any segment without SYN
		 */
		if(tcb->state == SYN_SENT && sent != 0)
			return;
		if(tcb->snd.wnd == 0){
			/* Force a closed-window probe */
			usable = 1;
		} else {
			/* usable window = offered window - unacked bytes in transit */
			usable = tcb->snd.wnd - sent;
		}
		/* John Nagle's "single outstanding segment" rule.
		 * Allow only one segment in the pipeline unless there is enough
		 * unsent data to form at least one maximum-sized segment.
		 *
		 * This also allows only one closed-window probe at a time.
		 */
		if(sent != 0 && tcb->sndcnt - sent < tcb->mss){
			usable = 0;
		}
		/* Silly window avoidance. Don't send anything if the usable window
		 * is less than a quarter of the offered window.
		 * This test comes into play only when the offered window is at
		 * least 4 times the MSS; otherwise Nagle's test is sufficient
		 * to prevent SWS.
	 	 */
		else if(usable < tcb->snd.wnd/4){
			usable = 0;
		}
		/* If this is a retransmission, send only the oldest unacknowledged
		 * segment ("First Only Retransmission")
		 */
		if(tcb->backoff != 0)
			usable = min(usable,tcb->mss);

		/* Compute size of segment to send. This is either the usable
		 * window, the mss, or the amount we have on hand, whichever is less.
		 * (I don't like optimistic windows)
		 */
		ssize = min(tcb->sndcnt - sent,usable);
		ssize = min(ssize,tcb->mss);
		dsize = ssize;

		if(ssize == 0 && tcb->force == 0)
			return;		/* No need to send anything */

		/* Allocate mbuf for TCP header and fill it in */
		if((hbp = alloc_mbuf(sizeof(struct tcp_header))) == NULL)
			return;	/* No room to form a packet */

		tcb->force = 0;	/* Only one forced segment! */
		hbp->cnt = sizeof(struct tcp_header);

		tcph = (struct tcp_header *)hbp->data;
		tcph->source = htons(tcb->conn.local.port);
		tcph->dest = htons(tcb->conn.remote.port);
		tcph->offset = (sizeof(struct tcp_header)/sizeof(int32)) << DSHIFT;
		tcph->flags = 0;

		/* Set the SYN and ACK flags according to the state we're in. It is
		 * assumed that if this segment is associated with a state transition,
		 * then the state change will already have been made. This allows
		 * this routine to be called from a retransmission timeout with
		 * force=1.
		 * If SYN is being sent, adjust the dsize counter so we'll
		 * try to get the right amount of data off the send queue.
		 */
		switch(tcb->state){
		case SYN_SENT:
		case SYN_RECEIVED:
			if(tcb->snd.ptr == tcb->iss){
				tcph->flags = SYN;
				dsize--;
			}
		}
		switch(tcb->state){
		case SYN_RECEIVED:
		case ESTABLISHED:
		case CLOSE_WAIT:
		case FINWAIT2:
		case TIME_WAIT:
		case CLOSING:
		case FINWAIT1:
		case LAST_ACK:
			tcph->flags |= ACK;
			break;
		}
		tcph->seq = htonl(tcb->snd.ptr);
		tcph->ack = htonl(tcb->rcv.nxt);
		tcph->wnd = htons(tcb->rcv.wnd);
		tcph->checksum = 0;
		tcph->up = 0;

		/* Now try to extract some data from the send queue.
		 * Since SYN and FIN occupy sequence space and are reflected
		 * in sndcnt but don't actually sit in the send queue,
		 * dup_p will return one less than dsize if a FIN needs to be sent.
		 */
		if(dsize != 0){
			if(dup_p(&hbp->next,tcb->sndq,sent,dsize) != dsize){
				/* We ran past the end of the send queue; send a FIN */
				tcph->flags |= FIN;
				dsize--;
			}
		}
		/* If the entire send queue will now be in the pipe, set the
		 * push flag
		 */
		if(dsize != 0 && sent + ssize == tcb->sndcnt)
			tcph->flags |= PSH;

		tcb->snd.ptr += ssize;
		/* If this is the first transmission of a range of sequence
		 * numbers, record it so we'll accept acknowledgments
		 * for it later
		 */
		if(seq_gt(tcb->snd.ptr,tcb->snd.nxt))
			tcb->snd.nxt = tcb->snd.ptr;
		/* Fill in fields of pseudo IP header */
		ph.source = tcb->conn.local.address;
		ph.dest = tcb->conn.remote.address;
		ph.protocol = TCP_PTCL;
		ph.length = sizeof(struct tcp_header) + dsize;

		/* Compute checksum over pseudo-header, TCP header and data,
		 * and pass it off to IP
		 */
		tcph->checksum = htons(cksum(&ph,hbp,ph.length));

		/* If we're sending some data or flags, start retransmission
		 * timer if it isn't already running.
		 * Use a binary-exponential backoff for retries.
		 */
		if(ssize != 0){
			if(tcb->retrans.state != TIMER_RUN){
				tcb->retrans.start = (2 * tcb->srtt * (1 << tcb->backoff))/MSPTICK;
				/* Never initialize the timer with zero; it won't run! */
				tcb->retrans.start = max(tcb->retrans.start,1);
				start_timer(&tcb->retrans);
			}
			/* If round trip timer isn't already running, record the
			 * sequence number of this segment and start it too.
			 */
			if(tcb->rtt.state != TIMER_RUN){
				tcb->rttseq = ntohl(tcph->seq);
				tcb->rtt.start = MAX_TIME;
				start_timer(&tcb->rtt);
			}
		}
		ip_send(tcb->conn.local.address,tcb->conn.remote.address,
		 TCP_PTCL,tcb->tos,MAXTTL,hbp,ph.length,0,0);
	}
}
