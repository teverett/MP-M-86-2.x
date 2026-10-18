/* line.c */
/* Copyright 1984 by Philip Karn, KA9Q
 * Permission granted for noncommercial copying
 * and use provided this notice is retained
 */
/* Line level I/O - send and receive HDLC frames
 * This code tries to be generic, but it was written for
 * the Zilog SIO.
 */

#include "ax25.h"
#include "820.h"

/* lrxint - line receiver interrupt
 * (HDLC Receiver interrupt handler)
 * This function takes buffers off the freelist,
 * fills them with receive data, and puts them on the receive queue.
 * It assumes that the data buffer for the entire frame (address,
 * control & data) has been pre-allocated and is pointed to by the
 * "mem" entry.  Incoming bytes are stuffed
 * directly into this array with no interpretation. Upper levels
 * will later scan the raw data of "iocnt" bytes pointed to by "iop".
 */
lrxint(lineno)
register int lineno;
{
	register struct line *line;
	register struct frame *fp;

	line = lines[lineno];
	while(lrxrdy(lineno)){
		if((fp = line->rfp) == NULL
		 && (fp = line->rfp = allocframe(line->psize)) == NULL){
			lrxabort(lineno);
			lrx(lineno);
			return;	/* No buffers; we'll lose the frame */
		}
		/* Normal save */
		*fp->iop++ = lrx(lineno);
		if(++fp->iocnt >= fp->size){
			/* Buffer overflow; toss the frame */
			fp->iocnt = 0;
			lrxabort(lineno);
			lrxdone(lineno);
		}
	}
}
/* ltxint - line transmit interrupt
 * (HDLC transmit interrupt service routine)
 *
 * The state variable tstate, along with some static pointers,
 * represents the state of the transmit "process".
 *
 * The queued frames for transmission must have the following fields set:
 * 		iop - point to frame buffer for transmission
 *		iocnt - length of frame
 * The data pointed to by "iop" will be sent as-is.
 */
ltxint(lineno)
register int lineno;
{
	register struct line *line;
	register struct frame *fp;
	char i_state;

	line = lines[lineno];
	i_state = disable();
	while(ltxrdy(lineno)){
		switch(line->tstate){
		/* First here for efficiency */
		case ACTIVE:		/* Sending frame */
			fp = line->tfp;
			if(fp->iocnt-- != 0){
				ltx(lineno,*fp->iop++);
			} else {
				/* Do this after sending the last byte */
				line->tstate = IDLE;
				freeframe(fp);
				line->tfp = NULL;
				ltxdone(lineno);
				goto ret;
			}
			break;
		case IDLE:
			/* Transmitter idle. Find a frame for transmission */
			while((fp = getframe(&line->txq)) != NULL){
				/* Paranoid check for empty frames */
				if(fp->iop != NULL && fp->iocnt > 2){
					break;
				} else {
					freeframe(fp);
				}
			}
			if(fp == NULL){
				/* Queue empty, turn off interrupt */
				if(line->mode != FULLDUP && line->keyup){
					/* Start shutdown sequence */
					ltx(lineno,0);
					line->tstate = FIN1;
				} else
					ltxdone(lineno);
				goto ret;	/* Wait for work */
			}
			line->tcnt += fp->iocnt;
			line->tframes++;
			line->tfp = fp;
			if(line->keyup){
				/* Bypass access procedure if already going */
				line->tstate = START;	/* And collect $200 */
				break;
			}
			line->tstate = DEFER;	/* note fall-thru */
		case DEFER:
			if(line->mode == CSMA && dcd(lineno) && !line->keyup){
				/* Wait for receiver carrier to drop */
				ltxdone(lineno);
				goto ret;
			}
			rts(lineno,ON);	/* Transmitter on */
			line->keyup = YES;
			line->tstate = KEYUP;
			if(line->ktimer.start != 0){
				start_timer(&line->ktimer);
			}
		case KEYUP:	/* note fall-thru */
			if(line->ktimer.state == TIMER_RUN || !cts(lineno)){
				/* Wait for timer to expire */
				ltxdone(lineno);
				goto ret;
			}
		case START:	/* Note fall-thru */
			line->tfp->iocnt--;
			ltxgo1(lineno);
			line->tstate = ACTIVE;
			ltx(lineno,*line->tfp->iop++);
			ltxgo2(lineno);
			break;
		case FIN1:	/* Sending flush character */
			ltx(lineno,0);
			line->tstate = FIN2;
			break;
		case FIN2:
			ltxabort(lineno);
			line->keyup = NO;
			line->tstate = IDLE;
			rts(lineno,OFF);
			ltxdone(lineno);
			break;
		}
	}
ret:	restore(i_state);
}

/* Line receiver end-of-frame interrupt */
lrxdone(lineno)
int lineno;
{
	register struct line *line;
	register struct frame *fp;

	line = lines[lineno];
	if((fp = line->rfp) == NULL)
		return;	/* Abort with no active frame */
	/* Receiver has finished frame */
	fp->iop = fp->mem;
	if(fp->iocnt == 0){	/* Toss bad frames back */
		line->rerrors++;
	} else {
		/* End of normal frame, no errors */
		line->rframes++;
		line->rcnt += fp->iocnt;
		putframe(&line->rxq,fp);
		line->rfp = allocframe(line->psize);
	}
}
