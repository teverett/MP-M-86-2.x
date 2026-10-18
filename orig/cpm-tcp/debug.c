#include "machdep.h"
#include "timer.h"
#include "mbuf.h"
#include "netuser.h"
#include "internet.h"
#include "ip.h"
#include "tcp.h"
#include "udp.h"

/* Comment this out if your library already has this function */
#define	isprint(c)	((c) >= ' ' && (c) < 0x7f)		/* Assumes ASCII */

/* TCP connection states */
char *tcpstates[] = {
	"Closed",
	"Listen",
	"SYN sent",
	"SYN received",
	"Established",
	"FIN wait 1",
	"FIN wait 2",
	"Close wait",
	"Closing",
	"Last ACK",
	"Time wait"
};

/* TCP closing reasons */
char *reasons[] = {
	"Normal",
	"Reset",
	"Timeout",
	"ICMP"
};
/* ICMP unreachable messages */
char *unreach[] = {
	"Network",
	"Host",
	"Protocol",
	"Port",
	"Fragmentation",
	"Source route"
};
/* ICMP Time exceeded messages */
char *exceed[] = {
	"Time-to-live",
	"Fragment reassembly"
};

/* TCP segment header flags */
char *tcpflags[] = {
	"FIN",	/* 0x01 */
	"SYN",	/* 0x02 */
	"RST",	/* 0x04 */
	"PSH",	/* 0x08 */
	"ACK",	/* 0x10 */
	"URG"	/* 0x20 */
};

/* Convert an internet address (in host byte order) to a dotted decimal ascii
 * string, e.g., 255.255.255.255\0
 */
char *
inet_ntoa(a)
int32 a;
{
	static char buf[16];

	sprintf(buf,"%u.%u.%u.%u",
		hibyte(hiword(a)),
		lobyte(hiword(a)),
		hibyte(loword(a)),
		lobyte(loword(a)) );
	return buf;
}

/* Dump an mbuf in hex */
void
hexdump(bp)
struct mbuf *bp;
{
	register struct mbuf *tbp;
	int n;
	int16 address;
	void fmtline();

	printf("Hex dump:\r\n");
	tbp = copy_p(bp,len_mbuf(bp));
	address = 0;
	while(tbp->cnt != 0){
		n = min(tbp->cnt,16);
		fmtline(address,tbp->data,n);
		address += n;
		tbp->data += n;
		tbp->cnt -= n;
	}
	free_p(tbp);
}
/* Print a buffer up to 16 bytes long in formatted hex with ascii
 * translation, e.g.,
 * 0000: 30 31 32 33 34 35 36 37 38 39 3a 3b 3c 3d 3e 3f  0123456789:;<=>?
 */
static void
fmtline(addr,buf,len)
int16 addr;
char *buf;
int len;
{
	char line[80];
	char *aptr,*cptr;
	int c;
	void ctohex();

	for(cptr = line;cptr < &line[80];cptr++)
		*cptr = ' ';
	ctohex(line,hibyte(addr));
	ctohex(line+2,lobyte(addr));
	aptr = &line[6];
	cptr = &line[55];
	while(len-- != 0){
		c = *buf++ & 0xff;
		ctohex(aptr,c);
		aptr += 3;
		if(isprint(c)){
			*cptr++ = c;
		} else {
			*cptr++ = '.';
		}
	}
	*cptr++ = '\r';
	*cptr++ = '\n';
	write(1,line,cptr-line);
}
/* Dump a TCP control block */
void
state_tcp(tcb)
struct tcb *tcb;
{
	void ptimer();

	if(tcb == NULL)
		return;
	printf("TCP Control Block at %x:\r\n",(int)tcb);
	printf("Local: %s:%u",
	 inet_ntoa(tcb->conn.local.address),tcb->conn.local.port);
	printf(" Remote: %s:%u",
	 inet_ntoa(tcb->conn.remote.address),tcb->conn.remote.port);
	printf(" State: %s\r\n",tcpstates[tcb->state]);
	printf("        Init seq      Unack       Next  Wind   Urg        WL1        WL2 Queue\r\n");
	printf("Send:");
	printf("%11lu",tcb->iss);
	printf("%11lu",tcb->snd.una);
	printf("%11lu",tcb->snd.nxt);
	printf("%6u",tcb->snd.wnd);
	printf("%6u",tcb->snd.up);
	printf("%11lu",tcb->snd.wl1);
	printf("%11lu",tcb->snd.wl2);
	printf("%6u\r\n",tcb->sndcnt);

	printf("Recv:");
	printf("%11lu",tcb->irs);
	printf("           ");
	printf("%11lu",tcb->rcv.nxt);
	printf("%6u",tcb->rcv.wnd);
	printf("%6u",tcb->rcv.up);
	printf("           ");
	printf("           ");
	printf("%6u\r\n",tcb->rcvcnt);

	printf("Retry: %u Backoff: %u\r\n",tcb->retry,tcb->backoff);
	if(tcb->reseq != NULL){
		register struct reseq *rp;

		printf("Reassembly queue:\r\n");
		for(rp = tcb->reseq;rp != NULL; rp = rp->next){
			printf("  seq %lu %u bytes\r\n",rp->seg.seq,rp->length);
		}
	}
	ptimer("Retransmission timer: ",&tcb->retrans);
	ptimer("Round trip timer: ",&tcb->rtt);
	printf("Smoothed round trip time: %d mS\r\n",tcb->srtt);
	ptimer("2MSL timer: ",&tcb->msl);
}
/* Print state of a timer */
static void
ptimer(label,t)
char *label;
register struct timer *t;
{
	printf(label);

	switch(t->state){
	case TIMER_STOP:
		printf("stopped\r\n");
		break;
	case TIMER_RUN:
		printf("running (%d mS)\r\n",MSPTICK * (t->start - t->count));
		break;
	case TIMER_EXPIRE:
		printf("expired\r\n");
	}
}

/* Dump a TCP segment header. Assumed to be in network byte order */
void
dump_tcp(bp)
struct mbuf *bp;
{
	int hdr_len,i;
	register struct tcp_header *tcph;

	if(bp == NULL)
		return;
	tcph = (struct tcp_header *)bp->data;
	hdr_len = hinibble(tcph->offset) * sizeof(int32);
	printf("TCP header:\r\n");

	printf("Source port: %u; ",ntohs(tcph->source));
	printf("Dest port: %u\r\n",ntohs(tcph->dest));
	printf("Seq: %lu",ntohl(tcph->seq));
	printf(" Ack: %lu\r\n",ntohl(tcph->ack));
	printf("Header length: %u bytes; ",hdr_len);
	printf("Flags:");
	for(i=0;i<6;i++){
		if(tcph->flags & 1 << i){
			printf(" %s",tcpflags[i]);
		}
	}
	printf("; Window: %u\r\n",ntohs(tcph->wnd));
	printf("Checksum: %u; ",ntohs(tcph->checksum));
/*	if(cksum(ph,bp,len) == 0)
		printf(" Valid\r\n");
	else
		printf(" Invalid\r\n");*/
	printf("Urgent pointer: %u\r\n",ntohs(tcph->up));
}

/* Dump a UDP header */
void
dump_udp(bp)
struct mbuf *bp;
{
	register struct udp_header *udph;

	if(bp == NULL)
		return;
	udph = (struct udp_header *)bp->data;
	printf("UDP header:\r\n");
	printf("Source port: %u Dest port: %u\r\n",ntohs(udph->source),
		ntohs(udph->dest));
	printf("Length: %u Checksum: %u\r\n",ntohs(udph->length),
		ntohs(udph->checksum));
}

/* Dump an IP datagram header. If it's the first fragment, also dump
 * the next layer header (if known)
 */
void
dump_ip(bp)
struct mbuf *bp;
{
	register struct ip_header *ip;
	int16 ip_len;
	int16 length;
	struct mbuf *tbp;

	if(bp == NULL)
		return;	
	ip = (struct ip_header *)bp->data;
	ip_len = lonibble(ip->v_ihl) * sizeof(int32);
	length = ntohs(ip->length);
	printf("IP Datagram:\r\n");
	printf("IP Header length: %u bytes; ",ip_len);
	printf("TOS: %u; ",ip->tos);
	printf("Total Length: %u\r\n",length);

	printf("ID: %u; ",ntohs(ip->id));
	printf("Offset: %u bytes",(ntohs(ip->fl_offs) & F_OFFSET) << 3) ;
	if(ntohs(ip->fl_offs) & DF)
		printf(" DF");
	if(ntohs(ip->fl_offs) & MF)
		printf(" MF");
	printf("\r\n");
	printf("TTL: %u; Protocol: %u; Header checksum: %u\r\n",
		ip->ttl & 0xff,ip->protocol & 0xff,ip->checksum);
	printf("Source address: %s\r\n",inet_ntoa(ntohl(ip->source)));
	printf("Dest address: %s\r\n",inet_ntoa(ntohl(ip->dest)));

	if(ip->fl_offs == 0){
		dup_p(&tbp,bp,ip_len,length - ip_len);
		switch(ip->protocol & 0xff){
		case TCP_PTCL:
			dump_tcp(tbp);
			break;
		case UDP_PTCL:
			dump_udp(tbp);
			break;
		}
		free_p(tbp);
	}
	hexdump(bp);
}

/* Convert byte to two ascii-hex characters */
static void
ctohex(buf,c)
char *buf;
unsigned int c;
{
	static char hex[] = "0123456789abcdef";

	buf[0] = hex[hinibble(c)];
	buf[1] = hex[lonibble(c)];
}
