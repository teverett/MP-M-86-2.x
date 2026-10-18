#include "machdep.h"
#include "timer.h"

/* Head of running timer chain */
struct timer *timers;

tick()
{
	struct timer *t;

	for(t = timers;t != NULL; t = t->next){
		if(t->state == TIMER_RUN && --(t->count) == 0){
			stop_timer(t);
			t->state = TIMER_EXPIRE;
			if(t->func)
				(*t->func)(t->arg);
		}	
	}
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
		t->prev = NULL;
		t->next = timers;
		if(t->next != NULL)
			t->next->prev = t;
		timers = t;
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
		if(timers == t)
			timers = t->next;
		if(t->next != NULL)
			t->next->prev = t->prev;
		if(t->prev != NULL)
			t->prev->next = t->next;
	}
	t->state = TIMER_STOP;
	restore(i_state);
}
