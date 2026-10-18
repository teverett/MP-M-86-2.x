#include "machdep.h"
#include "mbuf.h"
#include "netuser.h"
#include "timer.h"
#include "internet.h"
#include "ip.h"
#include "icmp.h"
#include "interface.h"
#include "tcp.h"
#include "telnet.h"
#include "820.h"

#define BUFSIZ 256
int trace;
static struct telnet *tn;
extern struct interface if_lo,if_sl[];
static struct tcb *sock;
int32 aton();
static int speed = 9600;

main(argc,argv)
int argc;
char *argv[];
{
	int cnt,i;
	extern char _clktick;
	extern struct fifo cfifo,sfifo[];
	void r_discard(),r_echo(),t_echo(),t_chargen(),rt_echo(),t_state();
	void r_mail(),t_mail(),s_mail();
	void r_sftp(),t_sftp(),s_sftp();
	struct socket lsocket;
	char inbuf[256];

	ioinit();
	set_slip(0,speed);

	my_addr = aton("128.96.32.61");
	rt_add(my_addr,0L,0,&if_lo);		/* Route our packets to ourself */
	rt_add(aton("128.96.0.0"),0L,0,&if_lo);		/* Also take broadcasts */
	rt_add(0L,0L,0,&if_sl[0]);	/* Default entry to slip line #0 */

	/* Post LISTENs on the various server ports */

	/* Discard */
	lsocket.address = my_addr;
	lsocket.port = 9;
	open_tcp(&lsocket,NULL,TCP_PASSIVE,200,r_discard,NULL,t_state,0);

	/* Echo */
	lsocket.port = 7;
	open_tcp(&lsocket,NULL,TCP_PASSIVE,200,r_echo,t_echo,t_state,0);

	/* Char gen */
	lsocket.port = 19;
	open_tcp(&lsocket,NULL,TCP_PASSIVE,200,r_discard,t_chargen,t_state,0);	

	/* SMTP */
	lsocket.port = 25;
	open_tcp(&lsocket,NULL,TCP_PASSIVE,200,r_mail,t_mail,s_mail,0);

	/* SFTP */
	lsocket.port = 115;
	open_tcp(&lsocket,NULL,TCP_PASSIVE,200,r_sftp,t_sftp,s_sftp,0);

	for(;;){
		if(srx(&cfifo,inbuf,BUFSIZ) > 0){
			iodone();
			exit();	/* Any key terminates */
		}
		/* Process SLIP line I/O */
		for(i=0;i<2;i++){
			if((cnt = srx(&sfifo[i],inbuf,BUFSIZ)) != 0)
				recv_slip(i,inbuf,cnt);
			if(stxrdy(i)){
				slip_start(i);
			}
		}
		/* Handle clock ticks */
		if(_clktick != 0){
			tick();
			(void)iss();
			_clktick = 0;
		}
		eihalt();		/* wait until interrupt */
	}
}
/* Discard server receiver upcall */
static void
r_discard(tcb,cnt)
struct tcb *tcb;
int cnt;
{
	struct mbuf *bp;

	if(recv_tcp(tcb,&bp,cnt) > 0)
		free_p(bp);			/* Discard */
}

/* Echo server receive
 * Copies only as much will fit on the transmit queue
 */
static void
r_echo(tcb,cnt)
struct tcb *tcb;
int cnt;
{
	struct mbuf *bp;
	int acnt;

	if(cnt == 0){
		close_tcp(tcb);
		return;
	}
	acnt = min(cnt,tcb->snd.wnd);
	if(acnt > 0){
		/* Get only as much will fit in the send window */
		recv_tcp(tcb,&bp,tcb->snd.wnd);
		send_tcp(tcb,bp);
	}
}
/* Echo server transmit
 * Copies anything that might have been left in the receiver queue
 */
static void
t_echo(tcb,cnt)
struct tcb *tcb;
int cnt;
{
	struct mbuf *bp;

	if(tcb->rcvcnt > 0){
		/* Get only as much will fit in the send window */
		recv_tcp(tcb,&bp,cnt);
		send_tcp(tcb,bp);
	}
}
/* Character generator, upcall routine */
t_chargen(tcb,cnt)
struct tcb *tcb;
int cnt;
{
	struct mbuf *bp;
	static char message[] =
"The quick brown fox jumped over the lazy dog's back 1,234,567,890 times\r\n\
Voyez le brick geant que j'examine pres du wharf 1,234,567,890\r\n";

	if(cnt < sizeof(message))
		return;	/* Not enough space; wait */
	bp = qdata(message,sizeof(message));
	send_tcp(tcb,bp);
}

/* Log connection state changes; also respond to remote closes */
t_state(tcb,old,new)
register struct tcb *tcb;
char old,new;
{
	switch(new){
	case ESTABLISHED:
		printf("%s:%d - open %d\r\n",
		 inet_ntoa(tcb->conn.remote.address),tcb->conn.remote.port,
		 tcb->conn.local.port);
		break;
	case CLOSE_WAIT:
		close_tcp(tcb);
		break;
	case CLOSED:
		printf("%s:%d - close %d\r\n",
		 inet_ntoa(tcb->conn.remote.address),tcb->conn.remote.port,
		 tcb->conn.local.port);
		del_tcp(tcb);
		break;
	}
}
/* Convert Intenet address in dotted-decimal to binary */
int32
aton(s)
char *s;
{
	int32 n;
	int atoi();
	char *index();

	n = 0;
	for(;;){
		n = atoi(s) | (n << 8);
		if((s = index(s,'.')) == NULL)
			break;
		s++;
	}
	return n;
}
