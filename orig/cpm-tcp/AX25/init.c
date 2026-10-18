#include "ax25.h"

/* Initialize terminal queues */
terminit()
{
	initq(&term.txq);
	initfifo(&term.rxfifo);
	initfifo(&term.txfifo);
}

/* Initialize HDLC line queues and hardware parameters */
lineinit()
{
	int i;
	struct line *line;

	/* Clear all line states, initialize HDLC hardware */
	for(i=0;i<nlines;i++){
		if(lines[i] == NULL){
			lines[i] = (struct line *)calloc(1,sizeof(struct line));
		}
		line = lines[i];

		line->lineno = i;
		initq(&line->txq);
		line->tfp = NULL;
		line->tstate = IDLE;
		line->keyup = NO;
		line->tcnt = 0;
		line->tframes = 0;

		initq(&line->rxq);
		line->rfp = allocframe(line->psize);
		line->rstate = IDLE;
		line->rcnt = 0;
		line->rframes = 0;
		line->rerrors = 0;
		hdlcparam(i);
	}
}

/* Initialize active timer queue */
timerinit()
{
	timers.next = &timers;
	timers.prev = &timers;
}

/* Reset and free all link control blocks */
linkinit()
{
	register struct link **hp,*link,*tmplink;

	for(hp = &hashtab[0];hp < &hashtab[NHASH]; hp++){
		for(link = *hp;link != NULL;link = tmplink){
			setstate(link,DISCONNECTED);
			tmplink = link->next;
			del_link(link);
		}
	}
}
/* Initialize memory control constants */
meminit()
{
	char *sbot();
	extern char _Uend;

	memused = 0;
	aframes = 0;
	memlimit = sbot() - &_Uend - 1024;	/* Arbitrary grace margin */
}
