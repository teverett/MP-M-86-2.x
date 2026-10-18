/* TCP timeout routines */
#include "machdep.h"
#include "timer.h"
#include "mbuf.h"
#include "netuser.h"
#include "internet.h"
#include "ip.h"
#include "tcp.h"

/* Retransmission timeout */
void
tcp_timeout(arg)
int *arg;
{
	register struct tcb *tcb;

	tcb = (struct tcb *)arg;
	if(tcb->retry < RETRY){
		tcb->retry++;
		tcb->backoff = min(tcb->backoff+1,BACKOFF);
		tcb->snd.ptr = tcb->snd.una;
		tcp_output(tcb);
	} else {
		/* Give up */
		close_self(tcb,TIMEOUT);
	}
}

/* The 2MSL timer has expired, presumably in TIME_WAIT state.
 * Close the connection.
 */
void
tcp_msl(arg)
int *arg;
{
	register struct tcb *tcb;

	tcb = (struct tcb *)arg;
	close_self(tcb,NORMAL);
}
