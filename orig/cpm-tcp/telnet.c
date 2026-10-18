#include <stdio.h>
#include "machdep.h"
#include "mbuf.h"
#include "telnet.h"

#ifdef	DEBUG
char *t_options[] = {
	"Transmit Binary",
	"Echo",
	"",
	"Suppress Go Ahead",
	"",
	"Status",
	"Timing Mark"
};
#endif

/* Create and initialize a Telnet protocol descriptor */
struct telnet *
open_telnet(tcb)
struct tcb *tcb;
{
	struct telnet *tp;
	char *calloc();

	tp = (struct telnet *)calloc(1,sizeof(struct telnet));
	tp->tcb = tcb;
	tp->state = TS_DATA;
	return tp;
}
free_telnet(tp)
struct telnet *tp;
{
	if(tp != (struct telnet *)NULL)
		free((char *)tp);
}
/* Process typed characters */
void
send_tel(tp,buf,n)
register struct telnet *tp;
char *buf;
int16 n;
{
	struct mbuf *bp,*qdata();

	bp = qdata(buf,n);
	send_tcp(tp->tcb,bp);
}

/* Process incoming TELNET characters */
void
tel_input(tp,bp)
register struct telnet *tp;
struct mbuf *bp;
{
	char c;
	int ci;
	void doopt(),dontopt(),willopt(),wontopt(),respond();

	while(pullup(&bp,&c,1) == 1){
		ci = c & 0xff;
		switch(tp->state){
		case TS_DATA:
			if(ci == IAC){
				tp->state = TS_IAC;
			} else {
				if(!tp->remote[TN_TRANSMIT_BINARY])
					c &= 0x7f;
				putchar(c);
			}
			break;
		case TS_IAC:
			switch(ci){
			case WILL:
				tp->state = TS_WILL;
				break;
			case WONT:
				tp->state = TS_WONT;
				break;
			case DO:
				tp->state = TS_DO;
				break;
			case DONT:
				tp->state = TS_DONT;
				break;
			case IAC:
				putchar(IAC);
				tp->state = TS_DATA;
				break;
			default:
				tp->state = TS_DATA;
				break;
			}
			break;
		case TS_WILL:
			willopt(tp,ci);
			tp->state = TS_DATA;
			break;
		case TS_WONT:
			wontopt(tp,ci);
			tp->state = TS_DATA;
			break;
		case TS_DO:
			doopt(tp,ci);
			tp->state = TS_DATA;
			break;
		case TS_DONT:
			dontopt(tp,cis);
			tp->state = TS_DATA;
			break;
		}
	}
}

static void
respond(tp,r1,r2)
struct telnet *tp;
int r1,r2;
{
	struct mbuf *bp,*qdata();
	char s[3];

#ifdef	DEBUG
	switch(r1){
	case WILL:
		printf("sent: will ");
		break;
	case WONT:
		printf("sent: wont ");
		break;
	case DO:
		printf("sent: do ");
		break;
	case DONT:
		printf("sent: dont ");
		break;
	}
	if(r2 <= 6)
		printf("%s\r\n",t_options[r2]);
	else
		printf("%d\r\n",r2);
#endif

	s[0] = IAC;
	s[1] = r1;
	s[2] = r2;
	bp = qdata(s,(int16)3);
	send_tcp(tp->tcb,bp);
}

static void
willopt(tp,opt)
struct telnet *tp;
int opt;
{
	int ack;

#ifdef	DEBUG
	printf("recv: will ");
	if(opt <= NOPTIONS)
		printf("%s\r\n",t_options[opt]);
	else
		printf("%d\r\n",opt);
#endif
	
	switch(opt){
	case TN_TRANSMIT_BINARY:
	case TN_ECHO:
	case TN_SUPPRESS_GA:
		if(tp->remote[opt] == 1)
			return;		/* Already set, ignore to prevent loop */
		tp->remote[opt] = 1;
		if(opt == TN_ECHO)
			raw();		/* Put tty into raw mode */
		ack = DO;
		break;
	default:
		ack = DONT;		/* We don't know what he's offering; refuse */
	}
	respond(tp,ack,opt);
}
static void
wontopt(tp,opt)
struct telnet *tp;
int opt;
{
#ifdef	DEBUG
	printf("recv: wont ");
	if(opt <= NOPTIONS)
		printf("%s\r\n",t_options[opt]);
	else
		printf("%d\r\n",opt);
#endif
	if(opt <= NOPTIONS){
		if(tp->remote[opt] == 0)
			return;		/* Already clear, ignore to prevent loop */
		tp->remote[opt] = 0;
		if(opt == TN_ECHO)
			cooked();	/* Put tty into cooked mode */
	}
	respond(tp,DONT,opt);	/* Must always accept */
}
static void
doopt(tp,opt)
struct telnet *tp;
int opt;
{
	int ack;

#ifdef	DEBUG
	printf("recv: do ");
	if(opt <= NOPTIONS)
		printf("%s\r\n",t_options[opt]);
	else
		printf("%d\r\n",opt);
#endif
	switch(opt){
#ifdef	FUTURE	/* Use when local options are implemented */
		if(tp->local[opt] == 1)
			return;		/* Already set, ignore to prevent loop */
		tp->local[opt] = 1;
		ack = WILL;
		break;
#endif
	default:
		ack = WONT;	/* Don't know what it is */
	}
	respond(tp,ack,opt);
}
static void
dontopt(tp,opt)
struct telnet *tp;
int opt;
{
#ifdef	DEBUG
	printf("recv: dont ");
	if(opt <= NOPTIONS)
		printf("%s\r\n",t_options[opt]);
	else
		printf("%d\r\n",opt);
#endif
	if(opt <= NOPTIONS){
		if(tp->local[opt] == 0){
			/* Already clear, ignore to prevent loop */
			return;
		}
		tp->local[opt] = 0;
	}
	respond(tp,WONT,opt);
}

