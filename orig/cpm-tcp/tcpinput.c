/* Process incoming TCP segments. Page number references are to ARPA RFC-793,
 * the TCP specification.
 */

#include "machdep.h"
#include "timer.h"
#include "mbuf.h"
#include "netuser.h"
#include "internet.h"
#include "tcp.h"
#include "icmp.h"

struct tcp_errors tcp_errors;

/* This function is called from IP with the IP header in machine byte order,
 * along with a mbuf chain pointing to the TCP header.
 */
void
tcp_input(bp,protocol,source,dest,tos,length)
struct mbuf *bp;		/* Data field, if any */
char protocol;			/* Should always be TCP_PTCL */
int32 source;			/* Remote IP address */
int32 dest;				/* Our IP address */
char tos;				/* Type of Service */
int16 length;			/* Length of data field */
{
	void reset(),update();
	void proc_syn(),send_syn(),add_reseq(),get_reseq();

	register struct tcb *tcb;	/* TCP Protocol control block */
	struct tcp_header seg;		/* Local copy of segment header */
	int16 mss;					/* Incoming MSS, if any */
	struct connection conn;		/* Local copy of addresses */
	struct pseudo_header ph;	/* Pseudo-header for checksumming */
	int16 optlen;				/* Length of TCP options */

	if(bp == NULL)
		return;

	ph.source = source;
	ph.dest = dest;
	ph.protocol = protocol;
	ph.length = length;
	if(cksum(&ph,bp,length) != 0){
		/* Checksum failed, ignore segment completely */
		tcp_errors.checksum++;
		free_p(bp);
		return;
	}
	/* Form local copy of TCP header in host byte order */
	if(pullup(&bp,(char *)&seg,sizeof(struct tcp_header))
		 != sizeof(struct tcp_header)){
		/* TCP header is too small */
		tcp_errors.runt++;
		free_p(bp);
		return;
	}
	length -= sizeof(struct tcp_header);
	seg.source = ntohs(seg.source);
	seg.dest = ntohs(seg.dest);
	seg.seq = ntohl(seg.seq);
	seg.ack = ntohl(seg.ack);
	seg.wnd = ntohs(seg.wnd);
	seg.up = ntohs(seg.up);	/* Unused as of yet */

	/* Examine options, if any (there can only be MSS). */
	optlen = hinibble(seg.offset) * sizeof(long) - sizeof(struct tcp_header);
	if(optlen != 0){
		struct mss *mssp;

		mssp = (struct mss *)bp->data;
		if(mssp->kind == MSS_KIND && mssp->length == MSS_LENGTH){
			mss = ntohs(mssp->mss);
		}
		pullup(&bp,(char *)NULL,optlen);
		length -= optlen;
	} else {
		mss = 0;
	}
	/* Fill in connection structure and find TCB */
	conn.local.address = dest;
	conn.local.port = seg.dest;
	conn.remote.address = source;
	conn.remote.port = seg.source;
	
	if((tcb = lookup_tcb(&conn)) == NULL){
		struct tcb *ntcb;
		char *malloc();
		void link_tcb();

		/* Check to see if there's a LISTEN on this socket with
		 * unspecified source address and port.
		 */
		conn.remote.address = 0;
		conn.remote.port = 0;
		if((tcb = lookup_tcb(&conn)) == NULL){
			/* No unspecified LISTEN either, so reject */
			free_p(bp);
			reset(source,dest,tos,length,&seg);
			return;
		}
		/* We've found an unspecified remote socket, so clone the TCB
		 * and stuff the foreign socket into the clone.
		 */
		if((ntcb = (struct tcb *)malloc(sizeof (struct tcb))) == NULL){
			free_p(bp);
			/* This may fail, but we should at least try */
			reset(source,dest,tos,length,&seg);
			return;
		}
		bcopy((char *)tcb,(char *)ntcb,sizeof(struct tcb));
		tcb = ntcb;
		tcb->conn.remote.address = source;
		tcb->conn.remote.port = seg.source;
		tcb->retrans.arg = (int *)tcb;
		tcb->msl.arg = (int *)tcb;
		link_tcb(tcb);
	}
	/* Do unsynchronized-state processing (p. 65-68) */
	switch(tcb->state){
	case CLOSED:
		free_p(bp);
		reset(source,dest,tos,length,&seg);
		return;
	case LISTEN:
		if(seg.flags & RST){
			free_p(bp);
			return;
		}
		if(seg.flags & ACK){
			free_p(bp);
			reset(source,dest,tos,length,&seg);
			return;
		}
		if(seg.flags & SYN){

			/* (Security check is bypassed) */
			/* page 66 */
			proc_syn(tcb,tos,&seg,mss);
			setstate(tcb,SYN_RECEIVED);		
			send_syn(tcb);
			if(length != 0 || seg.flags & FIN) {
				break;		/* Continue processing if there's more */
			}
			tcp_output(tcb);
		}
		free_p(bp);	/* Unlikely to get here directly */
		return;
	case SYN_SENT:
		if(seg.flags & ACK){
			if(!seq_within(seg.ack,tcb->iss+1,tcb->snd.nxt)){
				free_p(bp);
				reset(source,dest,tos,length,&seg);
				return;
			}
		}
		if(seg.flags & RST){	/* p 67 */
			if(seg.flags & ACK){
				/* The ack must be acceptable since we just checked it.
				 * This is how the remote side refuses connect requests.
				 */
				close_self(tcb,RESET);
			}
			free_p(bp);
			return;
		}
		/* (Security check skipped here) */
		/* Check incoming precedence; it must match if there's an ACK */
		if((seg.flags & ACK) && PREC(tos) != PREC(tcb->tos)){
			free_p(bp);
			reset(source,dest,tos,length,&seg);
			return;
		}
		if(seg.flags & SYN){
			proc_syn(tcb,tos,&seg,mss);
			if(seg.flags & ACK){
				/* Our SYN has been acked, otherwise the ACK
				 * wouldn't have been valid.
				 */
				update(tcb,&seg);
				setstate(tcb,ESTABLISHED);
			} else {
				setstate(tcb,SYN_RECEIVED);
			}
			if(length != 0 || (seg.flags & FIN)) {
				break;		/* Continue processing if there's more */
			}
			tcp_output(tcb);
		} else {
			free_p(bp);	/* Ignore if neither SYN or RST is set */
		}
		return;
	}
	/* We reach this point directly in any synchronized state. Note that
	 * if we fell through from LISTEN or SYN_SENT processing because of a
	 * data-bearing SYN, window trimming and sequence testing "cannot fail".
	 */

	/* Trim segment to fit receive window. */
	if(trim(tcb,&seg,&bp,&length) == -1){
		/* Segment is unacceptable */
		if(!(seg.flags & RST)){
			tcb->force = 1;
			tcp_output(tcb);
		}
		return;
	}
	/* If segment isn't the next one expected, and there's data
	 * or flags associated with it, put it on the resequencing
	 * queue and return. Don't send anything in reply.
	 *
	 * Processing the ACK in an out-of-sequence segment without
	 * flags or data should be safe, however.
	 */
	if(seg.seq != tcb->rcv.nxt
	 && (length != 0 || (seg.flags & (SYN|FIN)) )){
		add_reseq(tcb,tos,&seg,bp,length);
		return;
	}
	/* This loop first processes the current segment, and then
	 * repeats if it can process the resequencing queue.
	 */
	for(;;){
		/* We reach this point with an acceptable segment; all data and flags
		 * are in the window, and the starting sequence number equals rcv.nxt
		 * (p. 70)
		 */	
		if(seg.flags & RST){
			switch(tcb->state){
			case SYN_RECEIVED:
				setstate(tcb,LISTEN);
				free_p(bp);
				return;
			default:
				close_self(tcb,RESET);
				free_p(bp);
				return;
			}
		}
		/* (Security check skipped here) p. 71 */
		/* Check for precedence mismatch or erroneous extra SYN */
		if(PREC(tos) != PREC(tcb->tos) || (seg.flags & SYN)){
			free_p(bp);
			reset(source,dest,tos,length,&seg);
			return;
		}
		/* Check ack field p. 72 */
		if(!(seg.flags & ACK)){
			free_p(bp);	/* All segments after synchronization must have ACK */
			return;
		}
		/* Process ACK */
		switch(tcb->state){
		case SYN_RECEIVED:
			if(seq_within(seg.ack,tcb->snd.una+1,tcb->snd.nxt)){
				update(tcb,&seg);
				setstate(tcb,ESTABLISHED);
			} else {
				free_p(bp);
				reset(source,dest,tos,length,&seg);
				return;
			}
			break;
		case ESTABLISHED:
		case CLOSE_WAIT:
			update(tcb,&seg);
			break;
		case FINWAIT1:	/* p. 73 */
			update(tcb,&seg);
			if(tcb->snd.nxt == tcb->snd.una){
				/* Our FIN is acknowledged */
				setstate(tcb,FINWAIT2);
			}
			break;
		case FINWAIT2:
			update(tcb,&seg);
			break;
		case CLOSING:
			update(tcb,&seg);
			if(tcb->snd.nxt == tcb->snd.una){
				/* Our FIN is acknowledged */
				start_timer(&tcb->msl);
				setstate(tcb,TIME_WAIT);
			}
			break;
		case LAST_ACK:
			update(tcb,&seg);
			if(tcb->snd.nxt == tcb->snd.una){
				/* Our FIN is acknowledged, close connection */
				close_self(tcb,NORMAL);
				return;
			}			
		case TIME_WAIT:
			tcb->force = 1;
			start_timer(&tcb->msl);
		}

		/* (URGent bit processing skipped here) */

		/* Process the segment text, if any, beginning at rcv.nxt (p. 74) */
		if(length != 0){
			switch(tcb->state){
			case SYN_RECEIVED:
			case ESTABLISHED:
			case FINWAIT1:
			case FINWAIT2:
				/* Place on receive queue */
				append(&tcb->rcvq,bp);
				tcb->rcvcnt += length;
				tcb->rcv.nxt += length;
				tcb->rcv.wnd -= length;
				tcb->force = 1;
				break;
			default:
				/* Ignore segment text */
				free_p(bp);
				break;
			}
		}
		/* process FIN bit (p 75) */
		if(seg.flags & FIN){
			tcb->force = 1;	/* Always respond with an ACK */

			switch(tcb->state){
			case SYN_RECEIVED:
			case ESTABLISHED:
				tcb->rcv.nxt++;
				setstate(tcb,CLOSE_WAIT);
				break;
			case FINWAIT1:
				tcb->rcv.nxt++;
				if(tcb->snd.nxt == seg.ack){
					/* Our FIN has been acked; bypass CLOSING state */
					start_timer(&tcb->msl);
					stop_timer(&tcb->rtt);
					stop_timer(&tcb->retrans);
					setstate(tcb,TIME_WAIT);
				} else {
					setstate(tcb,CLOSING);
				}
				break;
			case FINWAIT2:
				tcb->rcv.nxt++;
				start_timer(&tcb->msl);
				stop_timer(&tcb->rtt);
				stop_timer(&tcb->retrans);
				setstate(tcb,TIME_WAIT);
				break;
			case CLOSE_WAIT:
			case CLOSING:
			case LAST_ACK:
				break;		/* Ignore */
			case TIME_WAIT:	/* p 76 */
				start_timer(&tcb->msl);
				break;
			}
		}
		/* Scan the resequencing queue, looking for a segment we can handle,
		 * and freeing all those that are now obsolete.
		 */
		while(tcb->reseq != NULL && seq_ge(tcb->rcv.nxt,tcb->reseq->seg.seq)){
			get_reseq(tcb,&tos,&seg,&bp,&length);
			if(trim(tcb,&seg,&bp,&length) == 0)
				goto gotone;
			/* Segment is an old one; trim has freed it */
		}
		break;
gotone:	;
	}
	/* If the user has set up a r_upcall function, AND
	 * the incoming segment either has the urg, push or fin flags set OR
	 * has filled the receive window, then notify the user.
	 * In case we get an empty segment with PSH or URG set,
	 * ignore to avoid calling him with 0 bytes, simulating FIN.
	 *
	 * All this is done before sending an acknowledgement,
	 * to give the user a chance to piggyback some reply data.
	 */
	if(tcb->r_upcall){
		if((seg.flags & FIN) || tcb->rcv.wnd == 0
		 || ((seg.flags & (URG|PSH)) && tcb->rcvcnt != 0)){
			(*tcb->r_upcall)(tcb,tcb->rcvcnt);
		}
	}
	tcp_output(tcb);	/* Send any necessary ack */
}

/* Process an incoming ICMP response */
tcp_icmp(source,dest,type,code,data)
int32 source;			/* Original IP datagram source (i.e. us) */
int32 dest;				/* Original IP datagram dest (i.e., them) */
char type,code;			/* ICMP error codes */
char *data;				/* First 8 bytes of TCP header */
{
	struct tcp_header *tcph;
	struct connection conn;
	register struct tcb *tcb;

	/* Extract the socket info from the returned TCP header fragment
	 * Note that since this is a datagram we sent, the source fields
	 * refer to the local side.
	 */
	tcph = (struct tcp_header *)data;
	conn.local.address = source;
	conn.local.port = ntohs(tcph->source);
	conn.remote.address = dest;
	conn.remote.port = ntohs(tcph->dest);
	if((tcb = lookup_tcb(&conn)) == NULL)
		return;	/* Unknown connection, ignore */

	/* Verify that the sequence number in the returned segment corresponds
	 * to something currently unacknowledged. If not, it can safely
	 * be ignored.
	 */
	if(!seq_within(ntohl(tcph->seq),tcb->snd.una,tcb->snd.nxt))
		return;

	/* The strategy here is that Destination Unreachable and Time Exceeded
	 * messages that occur after a connection has been established are likely
	 * to be transient events, and shouldn't kill our connection (at least
	 * until after we've tried a few more times). On the other hand, if
	 * they occur on our very first attempt to send a datagram on a new
	 * connection, they're probably "for real". In any event, the info
	 * is saved.
	 */
	switch(type){
	case DEST_UNREACH:
	case TIME_EXCEED:
		tcb->type = type;
		tcb->code = code;
		if(tcb->state == SYN_SENT || tcb->state == SYN_RECEIVED){
			close_self(tcb,NETWORK);
		}
		break;
	case QUENCH:
		break;		/* I really ought to implement this */
	}
}

/* Send an acceptable reset (RST) response for this segment */
static void
reset(source,dest,tos,length,seg)
int32 source;	/* Remote IP address */
int32 dest;		/* Our IP address */
char tos;		/* Type of Service */
int16 length;	/* Length of data portion */
register struct tcp_header *seg;	/* Offending TCP header */
{
	struct mbuf *hbp;
	struct pseudo_header ph;
	register struct tcp_header *tcph;

	if(seg->flags & RST)
		return;			/* Never send an RST in response to an RST */

	/* Compose the RST IP pseudo-header, swapping addresses */
	ph.source = dest;
	ph.dest = source;
	ph.protocol = TCP_PTCL;
	ph.length = sizeof(struct tcp_header);

	/* Allocate mbuf for the RST TCP header and fill it in */
	if((hbp = alloc_mbuf(ph.length)) == NULL)
		return;	/* Can't do nothin' */

	hbp->cnt = ph.length;
	tcph = (struct tcp_header *)hbp->data;

	tcph->source = htons(seg->dest);
	tcph->dest = htons(seg->source);
	tcph->flags = RST;

	if(seg->flags & ACK){
		/* This reset is being sent to clear a half-open connection.
		 * Set the sequence number of the RST to the incoming ACK
		 * so it will be acceptable.
		 */
		tcph->seq = htonl(seg->ack);
		tcph->ack = 0;
	} else {
		/* We're rejecting a connect request (SYN) from LISTEN state
		 * so we have to "acknowledge" their SYN.
		 */
		tcph->seq = 0;
		if(seg->flags & SYN)
			length++;
		if(seg->flags & FIN)
			length++;
		tcph->ack = htonl((int32)(seg->seq + length));
		tcph->flags |= ACK;
	}
	tcph->offset = (ph.length/sizeof(long)) << DSHIFT;
	tcph->wnd = 0;
	tcph->checksum = 0;
	tcph->up = 0;
	tcph->checksum = htons(cksum(&ph,hbp,ph.length));
	/* Ship it out (note swap of addresses) */
	ip_send(dest,source,TCP_PTCL,tos,MAXTTL,hbp,ph.length,0,0);
}

/* Process an incoming acknowledgement and window indication.
 * From page 72.
 */
static void
update(tcb,seg)
register struct tcb *tcb;
register struct tcp_header *seg;
{
	int16 acked;

	acked = 0;
	if(seq_gt(seg->ack,tcb->snd.nxt)){
		tcb->force = 1;	/* Acks something not yet sent */
		return;
	}
	/* As long as we keep hearing current ACKs from the other end,
	 * reset the retry counter even if it doesn't acknowledge anything
	 * new. This allows closed-window probes to go on indefinitely.
	 */
	if(seq_ge(seg->ack,tcb->snd.una))
		tcb->retry = 0;

	/* See if anything new is being acknowledged */
	if(seq_gt(seg->ack,tcb->snd.una)){
		acked = seg->ack - tcb->snd.una;
		/* If we're waiting for an ack of our SYN, process it */
		switch(tcb->state){
		case SYN_SENT:
		case SYN_RECEIVED:
			acked--;
			tcb->sndcnt--;
		}
		/* Remove acknowledged bytes from the send queue and update the
		 * unacknowledged pointer. If a FIN is being acked,
		 * pullup won't be able to remove it from the queue.
		 */
		pullup(&tcb->sndq,(char *)NULL,acked);

		/* This will include the FIN if there is one */
		tcb->sndcnt -= acked;
		tcb->snd.una = seg->ack;

		/* Reset the retransmission timer backoff level.
		 * Repeated transmissions of the same sequence number (e.g.,
		 * a closed-window probe) will back off until the receiver
		 * acknowledges it.
		 */
		tcb->backoff = 0;
		/* Stop retransmission timer, but restart it if there is still
		 * unacknowledged data
		 */	
		stop_timer(&tcb->retrans);
		if(tcb->snd.una != tcb->snd.nxt){
			start_timer(&tcb->retrans);
		}
	}
	/* If round trip timer was running and its sequence number has been
	 * acked, stop it and compute the new smoothed estimate.
	 *
	 * If the window is closed, stop the timer anyway in order to
	 * avoid an artificial spike in the RTT because of repeated
	 * closed window probing by the retransmission timer.
	 * If the next probe is accepted, we'll then get a good RTT.
	 */
	if(tcb->rtt.state == TIMER_RUN){
	 	if(seq_gt(tcb->snd.una,tcb->rttseq)){
			int32 rtt;

			stop_timer(&tcb->rtt);
			rtt = (tcb->rtt.start - tcb->rtt.count) * MSPTICK;
			tcb->srtt = (9*tcb->srtt + rtt)/10;
		} else if(seg->wnd == 0){
			stop_timer(&tcb->rtt);
		}
	}
	/* Decide if we need to do a window update.
	 * This is always checked whenever a legal ACK is received,
	 * because it might be a spontaneous window reopening.
	 */
	if(seq_gt(seg->seq,tcb->snd.wl1) || ((seg->seq == tcb->snd.wl1) 
	 && seq_ge(seg->ack,tcb->snd.wl2))){
		/* If the window had been closed, crank back the
		 * send pointer so we'll immediately resume transmission.
		 * Otherwise we'd have to wait until the next probe.
		 */
		if(tcb->snd.wnd == 0 && seg->wnd != 0)
			tcb->snd.ptr = tcb->snd.una;
		tcb->snd.wnd = seg->wnd;
		tcb->snd.wl1 = seg->seq;
		tcb->snd.wl2 = seg->ack;
	}
	/* If outgoing data was acked, notify the user so he can send more
	 * unless we've already sent a FIN.
	 */
	if(acked != 0 && tcb->t_upcall){
		switch(tcb->state){
	 	case ESTABLISHED:
		case CLOSE_WAIT:
			(*tcb->t_upcall)(tcb,tcb->window - tcb->sndcnt);
		}
	}
}

/* Determine if the given sequence number is in our receiver window.
 * NB: must not be used when window is closed!
 */
static
int
in_window(tcb,seq)
struct tcb *tcb;
int32 seq;
{
	return seq_within(seq,tcb->rcv.nxt,(int32)(tcb->rcv.nxt+tcb->rcv.wnd-1));
}

/* Process an incoming SYN */
static void
proc_syn(tcb,tos,seg,mss)
register struct tcb *tcb;
char tos;
struct tcp_header *seg;
int16 mss;
{
	tcb->force = 1;	/* Always send a response */

	/* Note: It's not specified in RFC 793, but SND.WL1 and
	 * SND.WND are initialized here since it's possible for the
	 * window update routine in update() to fail depending on the
	 * IRS if they are left unitialized.
	 */
	/* Check incoming precedence and increase if higher */
	if(PREC(tos) > PREC(tcb->tos))
		tcb->tos = tos;
	tcb->rcv.nxt = seg->seq + 1;	/* p 68 */
	tcb->snd.wl1 = tcb->irs = seg->seq;
	tcb->snd.wnd = seg->wnd;
	if(mss != 0)
		tcb->mss = mss;
}

/* Generate an initial sequence number and put a SYN on the send queue */
void
send_syn(tcb)
register struct tcb *tcb;
{
	tcb->iss = iss();
	tcb->snd.ptr = tcb->snd.wl2 = tcb->snd.nxt = tcb->snd.una = tcb->iss;
	tcb->sndcnt++;
	tcb->force = 1;
}

/* Add an entry to the resequencing queue in the proper place */
static void
add_reseq(tcb,tos,seg,bp,length)
struct tcb *tcb;
char tos;
struct tcp_header *seg;
struct mbuf *bp;
int16 length;
{
	register struct reseq *rp,*rp1;
	char *malloc();

	/* Allocate reassembly descriptor */
	if((rp = (struct reseq *)malloc(sizeof (struct reseq))) == NULL){
		/* No space, toss on floor */
		free_p(bp);
		return;
	}
	bcopy((char *)seg,(char *)&rp->seg,sizeof(struct tcp_header));
	rp->tos = tos;
	rp->bp = bp;
	rp->length = length;

	/* Place on reassembly list sorting by starting seq number */
	rp1 = tcb->reseq;
	if(rp1 == NULL || seq_lt(seg->seq,rp1->seg.seq)){
		/* Either the list is empty, or we're less than all other
		 * entries; insert at beginning.
		 */
		rp->next = rp1;
		tcb->reseq = rp;
	} else {
		/* Find the last entry less than us */
		for(;;){
			if(rp1->next == NULL || seq_lt(seg->seq,rp1->next->seg.seq)){
				/* We belong just after this one */
				rp->next = rp1->next;
				rp1->next = rp;
				break;
			}
			rp1 = rp1->next;
		}
	}
}

/* Fetch the first entry off the resequencing queue */
static void
get_reseq(tcb,tos,seg,bp,length)
register struct tcb *tcb;
char *tos;
struct tcp_header *seg;
struct mbuf **bp;
int16 *length;
{
	register struct reseq *rp;

	if((rp = tcb->reseq) == NULL)
		return;

	tcb->reseq = rp->next;

	*tos = rp->tos;
	bcopy((char *)&rp->seg,(char *)seg,sizeof(struct tcp_header));
	*bp = rp->bp;
	*length = rp->length;
	free((char *)rp);
}

/* Trim segment to fit window. Return 0 if OK, -1 if segment is
 * unacceptable.
 */
static int
trim(tcb,seg,bp,length)
register struct tcb *tcb;
register struct tcp_header *seg;
struct mbuf **bp;
int16 *length;
{
	struct mbuf *nbp;
	long dupcnt,excess;
	int16 len;		/* Segment length including flags */
	char accept;

	accept = 0;
	len = *length;
	if(seg->flags & SYN)
		len++;
	if(seg->flags & FIN)
		len++;

	/* Acceptability tests */
	if(tcb->rcv.wnd == 0){
		/* Only in-order, zero-length segments are acceptable when our window
		 * is closed.
		 */
		if(seg->seq == tcb->rcv.nxt && len == 0){
			return 0;	/* Acceptable, no trimming needed */
		}
	} else {
		/* Some part of the segment must be in the window */
		if(in_window(tcb,seg->seq)){
			accept++;	/* Beginning is */
		} else if(len != 0){
			if(in_window(tcb,seg->seq+len-1) || /* End is */
			 seq_within(tcb->rcv.nxt,seg->seq,seg->seq+len-1)){ /* Straddles */
				accept++;
			}
		}
	}
	if(!accept){
		free_p(*bp);
		return -1;
	}
	dupcnt = tcb->rcv.nxt - seg->seq;
	if(dupcnt > 0){
		/* Trim off SYN if present */
		if(seg->flags & SYN){
			/* SYN is before first data byte */
			seg->flags &= ~SYN;
			seg->seq++;
			dupcnt--;
		}
		if(dupcnt > 0){
			pullup(bp,(char *)NULL,(int16)dupcnt);
			seg->seq += dupcnt;
			*length -= dupcnt;
		}
	}
	excess = seg->seq + *length - (tcb->rcv.nxt + tcb->rcv.wnd);
	if(excess > 0){
		/* Trim right edge */
		*length -= excess;
		nbp = copy_p(*bp,*length);
		free_p(*bp);
		*bp = nbp;
		seg->flags &= ~FIN;	/* FIN follows last data byte */
	}
	return 0;
}
