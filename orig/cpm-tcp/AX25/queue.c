/* queue.c */
/* Copyright 1984 by Philip Karn, KA9Q
 * Permission granted for noncommercial copying
 * and use provided this notice is retained
 */

/*
 * Frame queue manipulation routines
 */
#include "ax25.h"

/* Put a frame on a queue */
putframe(q,fp)
register struct q *q;
register struct frame *fp;
{
	char i_state;

	if(fp == NULL || q == NULL)
		return;
	fp->next = NULL;
	i_state = disable();	
	if(q->count != 0)	/* Non-empty queue */
		q->last->next = fp;
	else
		q->first = fp;
	q->last = fp;
	q->count++;
	restore(i_state);
}

/* Fetch a frame off a queue */
struct frame *
getframe(q)
register struct q *q;
{
	register struct frame *fp;
	char i_state;

	if(q == NULL || q->count == 0)			/* queue empty */
		return NULL;
	i_state = disable();
	if(q->count == 1)		/* only one on queue */
		q->last = NULL;
	fp = q->first;
	q->first = fp->next;
	q->count--;
	restore(i_state);
	return fp;
}
initq(q)
struct q *q;
{
	if(q == NULL)
		return;
	q->first = q->last = NULL;
	q->count = 0;
}
purgeq(q)
struct q *q;
{
	struct frame *fp;

	if(q == NULL)
		return;
	while((fp = getframe(q)) != NULL)
		freeframe(fp);
}

/* put a character on a fifo */
int
putf(f,c)
struct fifo *f;
char c;
{
	char i_state;

	if(f == NULL || f->count == FIFOSIZE)
		return -1;
	i_state = disable();
	*f->inp++ = c;
	f->count++;
	if(f->inp >= &f->buf[FIFOSIZE])
		f->inp = f->buf;
	restore(i_state);
	return c & 0xff;
}
/* get a character from a fifo */
int
getf(f)
struct fifo *f;
{
	char c,i_state;

	if(f == NULL || f->count == 0)
		return -1;
	i_state = disable();
	c = *f->outp++;
	f->count--;
	if(f->outp >= &f->buf[FIFOSIZE])
		f->outp = f->buf;
	restore(i_state);
	return c & 0xff;
}
initfifo(f)
struct fifo *f;
{
	if(f == NULL)
		return;
	f->inp = f->outp = f->buf;
	f->count = 0;
}

/* Allocate a frame descriptor */
struct frame *
allocframe(size)
unsigned size;
{
	register struct frame *fp;
	register char *mem;
	unsigned tsize;
	char i_state;
	unsigned asize();

	i_state = disable();
	tsize = sizeof(struct frame) + size;
	mem = malloc(tsize);
	if(mem == NULL){
		restore(i_state);
		return NULL;
	}
	aframes++;
	memused += asize(mem);
	fp = (struct frame *)mem;
	fp->mem = mem + sizeof(struct frame);
	fp->size = size;
	fp->next = NULL;
	fp->iop = fp->mem;
	fp->iocnt = 0;
	restore(i_state);
	return fp;
}

/* Free all resources allocated to the given frame */
freeframe(fp)
register struct frame *fp;
{
	char i_state;
	unsigned asize();

	if(fp == NULL)
		return;
	i_state = disable();
	aframes--;
	memused -= asize((char *)fp);
	free((char *)fp);
	restore(i_state);
}

/* Return size of memory block that has been allocated with malloc()
 * This assumes use of the AZTEC library routine, which is very
 * similar to the Unix routine.
 */

typedef struct freelist {
	unsigned f_size;
	struct freelist *f_chain;
} FREE;

unsigned
asize(area)
char *area;
{
	return (((FREE *)area-1)->f_size - 1) * sizeof(FREE);
}
