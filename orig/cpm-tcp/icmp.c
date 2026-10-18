/* Internet Control Message Protocol */

#include "machdep.h"
#include "internet.h"
#include "timer.h"
#include "ip.h"
#include "icmp.h"
#include "mbuf.h"

/* Process an incoming ICMP packet */
void
icmp_input(bp,protocol,source,dest,tos,length)
struct mbuf *bp;	/* Pointer to ICMP message */
char protocol;		/* Should always be ICMP_PTCL */
int32 source;		/* Sender of ICMP message */
int32 dest;			/* Us */
char tos;			/* Type of Service */
int16 length;		/* Length of ICMP message */
{

	struct icmp *icmph;	/* Pointer to ICMP message */
	struct ip_header *iph;	/* Offending datagram header */
	int16 ip_len;

	if(cksum((struct pseudo_header *)NULL,bp,length) != 0){
		/* Bad ICMP checksum; discard */
		free_p(bp);
		return;
	}
	/* If the message is fragmented, copy to a contiguous mbuf */
	if(bp->next != NULL){
		struct mbuf *nbp;

		nbp = copy_p(bp,length);
		free_p(bp);
		if(nbp == NULL)
			return;
		bp = nbp;
	}
	icmph = (struct icmp *)bp->data;

	/* Process the message. Some messages are passed up to the protocol
	 * module for handling, others are handled here.
	 */
	switch(icmph->type & 0xff){
	case TIME_EXCEED:	/* Time-to-live Exceeded */
	case DEST_UNREACH:	/* Destination Unreachable */
	case QUENCH:		/* Source Quench */
		iph = (struct ip_header *)(icmph + 1);
		ip_len = (iph->v_ihl & 0xf) * sizeof(int32);

		switch(iph->protocol){
		case TCP_PTCL:
			tcp_icmp(ntohl(iph->source),ntohl(iph->dest),
				icmph->type,icmph->code,(char *)iph + ip_len);
			break;
		}
		break;
	case ECHO:			/* Echo Request */
		/* Change type to ECHO_REPLY, recompute checksum and return datagram.
		 */
		icmph->type = ECHO_REPLY;
		icmph->checksum = 0;
		icmph->checksum = htons(cksum((struct pseudo_header *)NULL,bp,length));
		ip_send(dest,source,ICMP_PTCL,tos,MAXTTL,bp,length,0,0);
		return;
	case REDIRECT:		/* Redirect */
	case PARAM_PROB:	/* Parameter Problem */
	case ECHO_REPLY:	/* Echo Reply */
	case TIMESTAMP:		/* Timestamp */
	case TIME_REPLY:	/* Timestamp Reply */
	case INFO_RQST:		/* Information Request */
	case INFO_REPLY:	/* Information Reply */
		break;
	}
	free_p(bp);
}
/* Return an ICMP response to the sender of a datagram */
icmp_output(bp,type,code,args)
struct mbuf *bp;			/* Pointer to offending IP header + data */
char type,code;				/* Codes to send */
union icmp_args *args;
{
	struct ip_header *iph;	/* Offending IP header */
	int16 ip_len;			/* Length of offending IP header */

	struct mbuf *reply;		/* Buffer with ICMP reply */
	struct icmp *icmph;		/* ICMP protocol header */
	struct mbuf *data;		/* Returned portion of offending packet */
	int16 dlen;				/* Length of data portion of offending pkt */
	int16 length;			/* Total length of reply */
	extern int32 my_addr;	/* Our IP address */

	iph = (struct ip_header *)bp->data;

	if(iph->protocol == ICMP_PTCL)
		return;	/* Never send an ICMP message about another ICMP message */

	/* Compute amount of original datagram to return.
	 * We return the original IP header, and up to 8 bytes past that.
	 */
	ip_len = (iph->v_ihl & 0xf) * sizeof(int32);
	dlen = ntohs(iph->length);
	if(dlen > ip_len + 8)
		dlen = ip_len + 8;
	length = sizeof(struct icmp) + dlen;

	/* Allocate ICMP header and fill in */
	if((reply = alloc_mbuf(sizeof(struct icmp))) == NULL){
		/* No space; don't bother */
		return;
	}
	reply->cnt = sizeof(struct icmp);
	icmph = (struct icmp *)reply->data;
	icmph->type = type;
	icmph->code = code;
	if(args != NULL)
		icmph->args.unused = args->unused;	/* copies whole union */
	else
		icmph->args.unused = 0;

	/* Link in a copy of the beginning of the original datagram */
	data = copy_p(bp,dlen);
	reply->next = data;	/* Could be NULL if copy fails */

	/* Compute ICMP checksum and send */
	icmph->checksum = 0;
	icmph->checksum = htons(cksum((struct pseudo_header *)NULL,reply,length));

	ip_send(my_addr,iph->source,ICMP_PTCL,iph->tos,MAXTTL,reply,length,0,0);
}
