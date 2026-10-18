/* Test IP-level fragmentation with a big UDP packet */
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include "machdep.h"
#include "mbuf.h"
#include "netuser.h"
#include "timer.h"
#include "internet.h"
#include "tcp.h"
#include "interface.h"

int trace;

main(argc,argv)
int argc;
char *argv[];
{
	extern struct interface if_test,if_lo;
	struct socket lsocket,fsocket;
	int32 aton();
	int16 length;
	struct mbuf *bp;
	int i;
	void r_upcall();

	my_addr = aton("192.4.8.60");
	if_lo.mtu = 512;				/* Cause fragmentation to happen */
	rt_add(my_addr,0L,0,&if_lo);	/* Route our packets to ourself */
	rt_add(0L,0L,0,&if_test);		/* Default entry to test dumper */

	lsocket.address = my_addr;
	lsocket.port = 7;
	fsocket.address = aton("1.2.3.4");
	fsocket.port = 7;

	open_udp(&lsocket,r_upcall);
	length = 2000;
	bp = alloc_mbuf(length);
	bp->cnt = length;
	for(i=0;i<length;i++)
		bp->data[i] = i & 0xff;
	send_udp(&lsocket,&fsocket,0,MAXTTL,bp,length,0,0);
}
r_upcall(lsocket,cnt)
struct socket *lsocket;
int cnt;
{
	struct mbuf *bp;
	struct socket fsocket;

	recv_udp(lsocket,&fsocket,&bp);
	printf("Got UDP datagram:\r\n");
	hexdump(bp);
	free_p(bp);
}

int32
aton(s)
char *s;
{
	int32 n;
	int a,b,c,d;

	sscanf(s,"%d.%d.%d.%d",&a,&b,&c,&d);
	n = ((long)a << 24) | ((long) b << 16) |( (long)c << 8) | d;
	return n;
}
testout(bp,interface,gateway)
struct mbuf *bp;
struct interface *interface;
int32 gateway;
{
	printf("testout: got buffer 0x%x\r\n",bp);
	dump_ip(bp);
	free_p(bp);
}

struct interface if_test = {
	NULL,
	0,
	512,
	testout,
	0,
	0
};
