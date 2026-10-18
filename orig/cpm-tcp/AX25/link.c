/* link.c */
/* Copyright 1984 by Philip Karn, KA9Q
 * Permission granted for noncommercial copying
 * and use provided this notice is retained
 */

#include "ax25.h"

/* Process received frames once the AX.25 addressing has been
 * filtered out.  The logic here should be exactly the same
 * as the X.25 level 2 spec, except that "cmdrsp" is set by
 * the addressing layer to indicate a command or response frame.
 */
process(fp,link,cmdrsp)
register struct frame *fp;
register struct link *link;
char cmdrsp;
{
	char control;
	char rej;				/* FRMR reject reason code */
	char class;				/* General class (I/S/U) of frame */
	char type;				/* Specific type (I/RR/RNR/etc) of frame */
	char pf;				/* extracted poll/final bit */
	char disposed;			/* Indicate need for disposal of frame */
	char response;			/* Response we need to give */
	char polled;			/* We're being polled */

	response = 0;
	polled = NO;
	rej = 0;
	disposed = NO;

	control = *fp->iop++;		/* fp->iop -> data field */
	fp->iocnt--;			/* # bytes in data field */
	type = ftype(control);
	class = type & 0x3;
	pf = control & PF;

	if(type == UI){
		putframe(&link->rxq,fp);
		disposed = YES;
		goto reply;
	}
	/* Check for polls */
	if(cmdrsp == COMMAND && pf)
		polled = YES;
	/* While we're disconnected, reject all except SABM */
	if(link->state == DISCONNECTED && type != SABM){
		if(polled)
			response = DM|pf;
		goto reply;
	}		
	/* Process implicit acknowledgements in all but U-frames */
	if(class != U && ackours(link,(control >> 5) & 7) == -1)
		rej = Z;	/* Out of range sequence number */

	if(type != I && type != UI && type != FRMR && fp->iocnt)
		rej |= X|W;	/* I-field not allowed */

	/* Process final bit, if set */
	if(class != U && pf && cmdrsp == RESPONSE && link->waitack){
		link->waitack = NO;
		link->retries = 0;
		stop_timer(&link->t1);
		/* Pick up retransmission at proper point */
		link->unack = 0;
		ltxstart(link);	/* Try to restart transmitter */
	}
	/* Resend FRMR for all except certain U while in error state */
	if(link->state == FRAMEREJECT && class != U){
		response = FRMR;
		goto reply;
	}
	switch(type){
	case SABM:	/* Initialize or reset link */
		switch(link->state){
		case DISCONNECTED:
		case FRAMEREJECT:
		case CONNECTED:		/* note fall-through */
			setstate(link,CONNECTED);	/* Resets state counters */
			response = UA;
			break;
		case DISCPENDING:
			response = DM;
			break;
		case SETUP:
			response = UA;
			break;
		}
		break;
	case UA:
		link->retries = 0;
		switch(link->state){
		case CONNECTED:
			setstate(link,SETUP);
			sendctl(link,COMMAND,SABM|PF);
			break;
		case SETUP:
			stop_timer(&link->t1);
			setstate(link,CONNECTED);
			break;
		case DISCPENDING:
			setstate(link,DISCONNECTED);
			break;
		/* note - ignored if DISCONNECTED or in FRAMEREJECT state */
		}
		break;
	case DISC:
		switch(link->state){
		case SETUP:
			setstate(link,DISCONNECTED);
			response = DM;
			break;
		case DISCPENDING:
			response = UA;
			break;
		default:
			response = UA;
			setstate(link,DISCONNECTED);
			break;
		}
		break;
	case RR:
		if(link->remotebusy){
			link->remotebusy = NO;
			ltxstart(link);
		}	
		break;
	case RNR:
		link->remotebusy = YES;
		link->retries = 0;
		start_timer(&link->t1);	/* Probe as long as necessary */
		break;
	case REJ:
		link->unack = 0;
		ltxstart(link);
		break;
	case I:
		if(link->localbusy){
			/* Too bad he didn't listen to us; he'll
			 * have to resend the frame later. This
			 * drastic action is necessary to avoid
			 * deadlock.
			 */
			response = RNR;
			freeframe(fp);
			disposed = YES;
			break;
		}
		/* Reject or ignore I-frames with receive sequence number errors */
		if(((control>>1) & 7) != link->vr){
			if(!link->nproto || !link->rejsent){
				link->rejsent = YES;
				response = REJ;
			}
			break;
		}
		link->rejsent = NO;
		putframe(&link->rxq,fp);
		if(memused > memlimit){
			link->localbusy = YES;
			response = RNR;
		} else {
			link->localbusy = NO;
			response = RR;
		}
		disposed = YES;
		link->vr = (link->vr+1) & MMASK;
		break;
	case FRMR:
		if(link->state == FRAMEREJECT || link->state == CONNECTED){
			setstate(link,SETUP);
			sendctl(link,COMMAND,SABM|PF);
		}
		break;
	case DM:
		switch(link->state){
		case SETUP:
			if(link->tstate)
				tprintf("%s: Connection refused\n",
				 pcall(&link->address[0]));
		case DISCPENDING:
			setstate(link,DISCONNECTED);
			break;
		default:
			setstate(link,SETUP);
			sendctl(link,COMMAND,SABM|PF);
		}
		break;
	default:
		rej |= W;
		break;
	}
/* Check if we have to make some sort of response to this frame */
reply:
	if(response == FRMR || rej){
		frmr(link,control,rej);
	} else {
		/* Normal RRs may be delayed to avoid single-frame acks */
		if(!polled && response == RR && link->t2.start != 0){
			if(link->t2.state != TIMER_RUN)
				start_timer(&link->t2);
		} else if(polled || response != 0){
			/* Responses to polls must be sent right away */
			stop_timer(&link->t2);
			if(response == 0){
				if(link->localbusy)
					response = RNR;
				else
					response = RR;
			}
			response |= polled ? PF : 0;
			sendctl(link,RESPONSE,response);
		}
	}
	/* Restart timer T1 if there are still unacknowledged I-frames or
	 * a poll outstanding.
	 */
	if(link->state != DISCONNECTED && (link->unack !=0 || link->waitack))
		start_timer(&link->t1);
	/* Start timer T3 (the idle poll timer) under AX.25 V2.0
	 * whenever T1 is stopped, unless we are disconnected
	 */
	if(link->state != DISCONNECTED && link->t1.state != TIMER_RUN
	 && link->t3.start != 0 && link->nproto)
		start_timer(&link->t3);
	else
		stop_timer(&link->t3);
	if(!disposed)
		freeframe(fp);
}
/* Handle incoming acknowledgements for frames we've sent.
 * Free frames being acknowledged.
 * Return -1 to cause a frame reject if number is bad, 0 otherwise
 * Also attempt to queue more I-frames for transmission
 */
ackours(link,n)
struct link *link;
char n;
{	
	struct frame *fp;

	/* Free up acknowledged frames by purging frames from the I-frame
	 * transmit queue. Start at the remote end's last reported V(r)
	 * and keep going until we reach the new sequence number.
	 * If we try to free a null pointer,
	 * then we have a frame reject condition.
	 * Stop the T1 timer if at least one frame is being acknowledged;
	 * it will be restarted again if not all frames were acknowledged.
	 */
	while(link->remvr != n){
		if((fp = link->i_frames[link->remvr]) == NULL)
			/* Acking unsent frame */
			return -1;
		freeframe(fp);
		link->i_frames[link->remvr] = NULL;
		link->unack--;
		stop_timer(&link->t1);
		link->retries = 0;
		link->remvr++;
		link->remvr &= MMASK;
	}
	if(link->unack < link->maxframe)
		ltxstart(link);
	return 0;
}

/* Generate Frame Reject (FRMR) response
 * If reason != 0, this is the initial error frame
 * If reason == 0, resend the last error frame
 */
frmr(link,control,reason)
register struct link *link;
char control;
char reason;
{
	if(reason != 0){
		link->frmrinfo[0] = control;
		link->frmrinfo[1] = link->vr << 5 || link->vs << 1;
		link->frmrinfo[2] = reason;
	}
	if(link->tstate)
		tprintf("%s: FRMR, reason %x\n",pcall(&link->address[0]),reason);
	setstate(link,FRAMEREJECT);
	sendframe(link,RESPONSE,FRMR|(control&PF),link->frmrinfo,3);
}

/* Send S or U frame to currently connected station */
int
sendctl(link,cmdrsp,cmd)
struct link *link;
char cmdrsp,cmd;
{
	if((ftype(cmd) & 0x3) == S)	/* Insert V(R) if S frame */
		cmd |= (link->vr << 5);
	return sendframe(link,cmdrsp,cmd,(char *)NULL,0);
}
/* Start transmission on link */
ltxstart(link)
register struct link *link;
{
	register struct frame *fp;
	char control;
	char sendv;

	if(link->state == DISCONNECTED){
		while((fp = getframe(&link->txq)) != NULL){
			sendframe(link,COMMAND,UI,fp->iop,fp->iocnt);
			freeframe(fp);
		}
		return;
	}
	/* Copy frames from the transmit pending queue to the active
	 * i_frame queue
	 */
	while((fp = getframe(&link->txq)) != NULL){
		if(link->i_frames[link->vs] != NULL)
			break;	/* Active queue is full */
		link->i_frames[link->vs++] = fp;
		link->vs &= MMASK;
	}
	if(link->remotebusy)
		return;

	/* Now actually transmit frames from the i_frame queue, if possible */
	/* Start at first unsent I-frame, stop when either the
	 * number of unacknowledged frames reaches the maxframe limit,
	 * or when there are no more frames to send
	 */
	sendv = (link->remvr + link->unack) & MMASK;
	while(link->unack < link->maxframe){
		fp = link->i_frames[sendv];
		if(fp == NULL)
			break;
		control = I | (sendv << 1) | (link->vr << 5);
		sendv = (sendv+1) & MMASK;
		sendframe(link,COMMAND,control,fp->iop,fp->iocnt);
		/* Cancel delayed acknowledgment, start T1 */
		stop_timer(&link->t2);
		link->unack++;
		start_timer(&link->t1);
	}
}
/* Set new link state
 * This is a major upheaval, so reset the world,
 * flush the I-queue. If the new state is disconnected,
 * also free the link control block
 */
setstate(link,s)
struct link *link;
char s;
{
	int i;

	if(link->tstate && link->state != s){
		tprintf("%s: state change: %s -> %s\n",
		 pcall(&link->address[0]),
		 pstate[link->state],pstate[s]);
	}
	link->state = s;
	link->localbusy = NO;
	link->rejsent = NO;
	link->remotebusy = NO;
	link->waitack = NO;
	link->unack = link->remvr = link->vr = link->vs = 0;
	link->retries = 0;
	for(i=0;i<MODULO;i++)
		if(link->i_frames[i] != NULL)
			freeframe(link->i_frames[i]);
	if(s == DISCONNECTED){
		stop_timer(&link->t1);
		stop_timer(&link->t2);
		stop_timer(&link->t3);
	}
}
