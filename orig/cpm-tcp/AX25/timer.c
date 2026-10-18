/* timer.c */
/* Copyright 1984 by Philip Karn, KA9Q
 * Permission granted for noncommercial copying
 * and use provided this notice is retained
 */
#include "ax25.h"
/* Timer routines */
/* Handle clock tick interrupt - should be called at regular intervals
 * (e.g., 100 ms) from clock interrupt
 */
ctcint()
{
	timerflg = 1;
}

/* Called from main loop to process clock ticks.
 * This is done here rather than at interrupt time to avoid
 * starving more important interrupts.
 */
ctcsrv()
{
	register struct timer *tp,*tptmp;
	
	if(!timerflg)
		return;
	timerflg = 0;
	tp = timers.next;
	while(tp != &timers){
		tptmp = tp->next;
		tick(tp);
		tp = tptmp;
	}
}
/* Called on each active timer to register each clock tick */
tick(t)
register struct timer *t;
{
	char i_state;

	if(t == NULL)
		return;
	
	i_state = disable();
	if(t->state == TIMER_RUN && --(t->count) == 0){
		t->state = TIMER_EXPIRE;
		/* Delete from active timer list */
		t->next->prev = t->prev;
		t->prev->next = t->next;
	}		
	restore(i_state);
}
/* Start a timer */
start_timer(t)
register struct timer *t;
{
	char i_state;

	if(t == NULL || t->start == 0)
		return;
	i_state = disable();
	t->count = t->start;
	if(t->state != TIMER_RUN){
		t->state = TIMER_RUN;
		/* Put on head of active timer list */
		t->prev = &timers;
		t->next = timers.next;
		t->prev->next = t;
		t->next->prev = t;
	}
	restore(i_state);
}
/* Stop a timer */
stop_timer(t)
register struct timer *t;
{
	char i_state;

	if(t == NULL)
		return;
	i_state = disable();
	if(t->state == TIMER_RUN){
		/* Delete from active timer list */
		t->next->prev = t->prev;
		t->prev->next = t->next;
	}
	t->state = TIMER_STOP;
	restore(i_state);
}

/* Scan all of the link timers and take any appropriate actions */
timer_srv()
{
	register int i;
	register struct link *lp;

	ctcsrv();
	for(i=0;i<NHASH;i++){
		for(lp = hashtab[i];lp != NULL;lp = lp->next){
			check_timer(lp);
		}
	}
	for(i=0;i<nlines;i++){
		if(lines[i]->ktimer.state == TIMER_EXPIRE){
			stop_timer(&lines[i]->ktimer);
			ltxint(i);
		}
	}
}
check_timer(link)
register struct link *link;
{
	if(link->t1.state == TIMER_EXPIRE){
		if(link->ttimer)
			tprintf("%s: T1 Timeout %u/%u\n",
			 pcall(&link->address[0]),link->retries,link->n2);
		stop_timer(&link->t1);
		recover(link);
	}
	if(link->t2.state == TIMER_EXPIRE){
		stop_timer(&link->t2);
		send_ack(link);
	}
	if(link->t3.state == TIMER_EXPIRE){
		if(link->ttimer)
			tprintf("%s: T3 Timeout\n",
			 pcall(&link->address[0]));
		stop_timer(&link->t3);
		pollthem(link);
	}
}
/* This function gets called whenever timer T1 expires on a link.
 * By definition, the timer is not running when we enter.
 */
recover(link)
register struct link *link;
{
	link->retries++;
	if(link->n2 != 0 && link->retries > link->n2){
		if(link->ttimer)
			tprintf("%s: Retry count exceeded\n",
			 pcall(&link->address[0]));
		/* Retry count exceeded; reset the link, unless
		 * a connect or disconnect was pending, in which case just give up.
		 */
		switch(link->state){
		case DISCONNECTED:
			return;					/* The user issued a double disc */
		case SETUP:
		case DISCPENDING:
			setstate(link,DISCONNECTED);	/* Just give up */
			return;
		case CONNECTED:
		case FRAMEREJECT:
			setstate(link,SETUP);
	 		sendctl(link,COMMAND,SABM|PF);		/* Attempt link reset */
			start_timer(&link->t1);
			return;
		}
	}		
	switch(link->state){
	case SETUP:
		sendctl(link,COMMAND,SABM|PF);
		start_timer(&link->t1);
		break;
	case FRAMEREJECT:
		frmr(link,0,0);
		break;
	case DISCPENDING:
		sendctl(link,COMMAND,DISC|PF);
		start_timer(&link->t1);
		break;
	case CONNECTED:
		/* This takes some explanation.  Two ways to recover are
		 * allowed in LAPB. If nproto is set, send a supervisory
		 * frame with the poll bit set.  Otherwise, retransmit the oldest
		 * unacknowledged I-frame. Unfortunately, only the second option
		 * is compatible with the old AX.25 implementations.
		 */
		if(link->nproto){
			pollthem(link);
		} else {
			char sendv;
			struct frame *fp;
			char control;

			sendv = (link->remvr + link->unack) & MMASK;
			fp = link->i_frames[sendv];
			if(fp == NULL)
				break;
			control = I | (sendv << 1) | (link->vr << 5);
			sendframe(link,COMMAND,control,fp->iop,fp->iocnt);
		}
		break;
	}
}

/* T2 has expired, we can't delay an acknowledgement any further */
send_ack(link)
register struct link *link;
{
	if(link->localbusy)
		sendctl(link,RESPONSE,RNR);
	else
		sendctl(link,RESPONSE,RR);
	stop_timer(&link->t2);
}

/* Send a poll (S-frame command with the poll bit set) */
pollthem(link)
register struct link *link;
{
	char poll;

	link->waitack = YES;
	poll = link->localbusy ? RNR|PF : RR|PF;
	sendctl(link,COMMAND,poll);
	stop_timer(&link->t3);
	start_timer(&link->t1);
}
