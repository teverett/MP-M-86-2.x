#include "stdio.h"
#include "machdep.h"
#include "mbuf.h"
#include "netuser.h"
#include "timer.h"
#include "internet.h"
#include "ip.h"
#include "interface.h"
#include "tcp.h"

int trace;
FILE *logfile;
main()
{
	struct tcb *tcb1,*tcb2;
	int x;
	struct socket fsock;
	struct socket lsock;
	struct mbuf *bp;
	char buf[80];
	void ip_recv();
	void t_state(),r_int(),t_int();
	extern struct interface if_lo;

	printf("Sizeof(struct tcb) = %d\n",sizeof(struct tcb));
	logfile = fopen("tcp.log","a+");
	my_addr = 0x1;
	x = rt_add(my_addr,0L,0,&if_lo);	/* Route our packets to ourself */
	printf("rt_add returns %d\n",x);

	lsock.address = my_addr;
	lsock.port = 44;
	fsock.address = my_addr;
	fsock.port = 55;
	/* Passive TCP open (listen) */
	printf("Passive open of FD1:\n");
	tcb1 = open_tcp(&lsock,&fsock,0,0,r_int,t_int,t_state,0);

	/* Active TCP open*/
	printf("Active open of FD2:\n");
	tcb2 = open_tcp(&fsock,&lsock,1,0,r_int,t_int,t_state,0);

	/* Send some data from tcb1 to tcb2 */
	bp = qdata("hello",6);
	send_tcp(tcb1,bp);
	printf("state of tcb1 after sending data\n");
	state_tcp(tcb1);
	printf("state of tcb2 after receiving data\n");
	state_tcp(tcb2);

	x = recv_tcp(tcb2,&bp,80);
	printf("recv_tcp on tcb2 returns %d bytes\n",x);
	dqdata(bp,buf,80);
	printf("read returns: %s\n",buf);

	/* Initiate close from tcb1 */
	printf("Initiating close on FD1:\n");
	close_tcp(tcb1);

	/* Respond with close from tcb2 */
	printf("Responding with close on FD2:\n");
	close_tcp(tcb2);
	
	exit(0);
}
void
t_state(tcb,old,new)
struct tcb *tcb;
char old,new;
{
	extern char *tcpstates[];

	printf("TCB @ %x state change: %s -> %s\n",(int)tcb,tcpstates[old],
		tcpstates[new]);
	state_tcp(tcb);
}
void
r_int(tcb,cnt)
struct tcb *tcb;
int16 cnt;
{
	printf("Receiver upcall on tcb %x, %d bytes available\n",(int)tcb,(int)cnt);
}
void
t_int(tcb,cnt)
struct tcb *tcb;
int16 cnt;
{
	printf("Transmitter upcall on tcb %x, %d bytes available\n",(int)tcb,(int)cnt);
}
