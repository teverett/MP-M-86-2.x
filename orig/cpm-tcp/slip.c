/* Send and receive IP datagrams on serial lines. Compatible with SLIP
 * under Berkeley Unix.
 */
#define	SLIP_MTU	1024	/* Maximum datagram size */
#define	SLIP_ALLOC	40	/* Receiver allocation increment */

#include "machdep.h"
#include "mbuf.h"
#include "interface.h"
#include "820.h"

#define	FR_END		0300	/* Frame End */
#define	FR_ESC		0333	/* Frame Escape */
#define	T_FR_END	0334	/* Transposed frame end */
#define	T_FR_ESC	0335	/* Transposed frame escape */

/* IP Interface control structure */
struct interface if_sl[NSLIP];

/* Slip protocol control structure */
struct slip {
	char escaped;		/* Receiver State control flag */
	struct mbuf *rbp;	/* Head of mbuf chain being filled */
	struct mbuf *rbp1;
	char *rcp;		/* Write pointer */
	int16 rcnt;		/* Length of mbuf chain */
	struct mbuf *tbp;	/* Transmit mbuf being sent */
	int16 errors;		/* Receiver input errors */
	void (*send)();		/* Hardware transmit function */
};
struct slip slip[NSLIP];

/* Set the output and end-of-packet functions for slip link n */
void
set_slip(dev,speed)
int16 dev;
int speed;
{
	struct interface *interface;
	struct slip *sp;
	int send_slip();
	char *calloc();
	extern void asend();
	
	if(dev >= NSLIP)
		return;
 	sp = &slip[dev];
	interface = &if_sl[dev];

	interface->name = calloc(1,10);
	strcpy(interface->name,"sl");
	interface->name[2] = dev + '0';
	interface->sndq = NULL;
	interface->sndcnt = 0;
	interface->mtu = SLIP_MTU;
	interface->send = send_slip;
	interface->dev = dev;
	interface->flags = 0;

	sp->send = asend;
	aparam(dev,speed);	/* Init device */
}
/* Encode a datagram in SLIP, put on interface queue and kick transmit */
int
send_slip(bp,interface,gateway,precedence,delay,throughput,reliability)
struct mbuf *bp;				/* Buffer to send */
struct interface *interface;	/* Pointer to interface control block */
int32 gateway;					/* Ignored (SLIP is point-to-point) */
char precedence;
char delay;
char throughput;
char reliability;
{
	char c;
	struct slip *sp;
	struct mbuf *lbp;		/* Mbuf containing line-ready packet */
	register char *cp;
	register int cnt;

	sp = &slip[interface->dev];
	if(sp->send == NULL){
		free_p(bp);
		return;
	}
	/* Allocate output mbuf that's twice as long as the packet.
	 * This is a worst-case guess (consider a packet full of FR_ENDs!)
	 */
	lbp = alloc_mbuf(2*len_mbuf(bp));	
	if(lbp == NULL){
		/* No space; drop */
		free_p(bp);
		return;
	}
	cp = lbp->data;
	cnt = 0;

	/* Flush out any line garbage */
	*cp++ = FR_END;
	cnt++;

	/* Copy input to output, escaping special characters */
	while(pullup(&bp,&c,1) == 1){
		switch(c & 0xff){
		case FR_ESC:
			*cp++ = FR_ESC;
			*cp++ = T_FR_ESC;
			cnt += 2;
			break;
		case FR_END:
			*cp++ = FR_ESC;
			*cp++ = T_FR_END;
			cnt += 2;
			break;
		default:
			*cp++ = c;
			cnt++;
		}
	}
	*cp++ = FR_END;
	cnt++;
	lbp->cnt = cnt;

	enqueue(&interface->sndq,lbp);
	interface->sndcnt++;
	if(sp->tbp == NULL)
		slip_start(interface->dev);
}
/* Start output, if possible, on slip device dev */
slip_start(dev)
int16 dev;
{
	struct slip *sp;
	struct interface *interface;

	if(!stxrdy(dev))
		return;		/* Transmitter not ready */

	sp = &slip[dev];
	interface = &if_sl[dev];
	if(sp->tbp != NULL){
		/* transmission just completed */
		free_p(sp->tbp);
		sp->tbp = NULL;
	}
	if(interface->sndq == NULL)
		return;	/* No work */
	sp->tbp = dequeue(&interface->sndq);
	interface->sndcnt--;
	(*sp->send)(dev,sp->tbp->data,sp->tbp->cnt);
}
/* Process incoming bytes on a SLIP link */
void
recv_slip(dev,buf,cnt)
int dev;	/* Slip unit number */
char *buf;	/* Incoming buffer */
int16 cnt;	/* Input buffer length */
{
	register struct slip *sp;
	register char c;

	sp = &slip[dev];
	while(cnt != 0){
		c = *buf++;
		cnt--;
		if(sp->escaped){
			sp->escaped = 0;
			switch(c & 0xff){
			case T_FR_ESC:
				c = FR_ESC;
				break;
			case T_FR_END:
				c = FR_END;
				break;
			default:
				sp->errors++;
			}
		} else {
			switch(c & 0xff){
			case FR_END:
				if(sp->rbp == NULL)
					continue;	/* Null frame, ignore */
				/* Kick upstairs */
				ip_route(sp->rbp);
				sp->rbp = NULL;
				sp->rcnt = 0;
				continue;
			case FR_ESC:
				sp->escaped = 1;
				continue;
			}
		}
		if(sp->rcnt == SLIP_MTU){
			/* Packet is too large, drop it and start over */
			free_p(sp->rbp);
			sp->rbp = NULL;
			sp->rcnt = 0;
			continue;
		}
		/* We reach here with a character for the buffer;
		 * make sure there's space for it
		 */
		if(sp->rbp == NULL){
			/* Allocate first mbuf for new packet */
			if((sp->rbp1 = sp->rbp = alloc_mbuf(SLIP_ALLOC)) == NULL)
				continue; /* No memory, drop */
			sp->rcp = sp->rbp->data;
		} else if(sp->rbp1->cnt == SLIP_ALLOC){
			/* Current mbuf is full; link in another */
			if((sp->rbp1->next = alloc_mbuf(SLIP_ALLOC)) == NULL){
				/* No memory, drop whole thing */
				free_p(sp->rbp);
				sp->rbp = NULL;
				sp->rcnt = 0;
				continue;
			}
			sp->rbp1 = sp->rbp1->next;
			sp->rcp = sp->rbp1->data;
		}
		/* Store the character, increment fragment and total
		 * byte counts
		 */
		*sp->rcp++ = c;
		sp->rbp1->cnt++;
		sp->rcnt++;
	}
}
