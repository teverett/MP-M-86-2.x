/* Upper half of IP, consisting of send/receive primitives, including
 * fragment reassembly, for higher level protocols.
 * Not needed when running as a standalone gateway.
 */
#define	TLB		30	/* Reassembly limit time */

#include "machdep.h"
#include "mbuf.h"
#include "timer.h"
#include "internet.h"
#include "ip.h"
#include "icmp.h"
#include "interface.h"

int ip_recv();	/* Should be void, but C complains */

/* Local recipient interface structure */
struct interface if_lo = {
	"local",
	NULL,
	0,
	65535,	/* We take them as big as they come */
	ip_recv,
	0,
	0
};

/* Reassembly descriptor */
struct reasm {
	struct reasm *next;	/* Linked list pointers */
	struct reasm *prev;
	int32 source;		/* These four fields uniquely describe a datagram */
	int32 dest;
	int16 id;
	char protocol;
	struct timer timer;	/* Reassembly timeout timer */
	struct mbuf *buf;	/* Data buffer, also holds hole descriptors */
	int16 length;		/* Total length of datagram so far */
	struct hole *hole;	/* Head of hole list, points into data buffer */
};

/* Format of a hole in a reassembly buffer */
struct hole {
	struct hole *next;
	int16 first;
	int16 last;
};

static struct reasm *reasmq;

/* Send an IP datagram. Modeled after the example interface on p 32 of
 * RFC 791
 */
void
ip_send(source,dest,protocol,tos,ttl,bp,length,id,df)
int32 source;			/* source address */
int32 dest;				/* Destination address */
char protocol;			/* Protocol */
char tos;				/* Type of service */
char ttl;				/* Time-to-live */
struct mbuf *bp;		/* Data portion of datagram */
int16 length;			/* Optional length of data portion */
int16 id;				/* Optional identification */
char df;				/* Don't-fragment flag */
{
	struct mbuf *hbp;		/* mbuf containing IP header */
	struct ip_header *iph;	/* Pointer to IP header */
	static int16 id_cntr;	/* Datagram serial number */
	int16 hdr_len;			/* IP header length, bytes */
	void ip_route();		/* Datagram router */

	if(length == 0 && bp != NULL)
		length = len_mbuf(bp);
	if(id == 0)
		id = id_cntr++;		
	if(ttl = 0)
		ttl = MAXTTL;

	/* Allocate an mbuf for the IP header */
	hdr_len = sizeof(struct ip_header);
	if((hbp = alloc_mbuf(hdr_len)) == NULL){
		/* We really ought to source-quench the sender, but that would
		 * probably fail too.
		 */
		free_p(bp);
		return;
	}
	hbp->cnt = hdr_len;

	/* and fill it in */
	iph = (struct ip_header *)hbp->data;

	iph->v_ihl = (IPVERSION << 4) | (hdr_len/sizeof(int32));
	iph->tos = tos;
	iph->length = htons(hdr_len + length);
	iph->id = htons(id);
	if(df)
		iph->fl_offs = htons(DF);
	else
		iph->fl_offs = 0;
	iph->ttl = ttl;
	iph->protocol = protocol;
	iph->checksum = 0;
	iph->source = htonl(source);
	iph->dest = htonl(dest);
	iph->checksum = htons(cksum((struct pseudo_header *)NULL,hbp,hdr_len));
	hbp->next = bp;
	ip_route(hbp);		/* Toss it to the router */
}

/* Reassemble incoming IP fragments and dispatch completed datagrams
 * to the proper transport module
 */
int	/* Should really be void */
ip_recv(bp,dev,gateway)
struct mbuf *bp;
int16 dev;		/* Ignored */
int32 gateway;	/* Ignored */
{
	register struct ip_header *ip;
	int16 ip_len;		/* Length of IP header */
	int16 id;			/* Datagram ID, used for reassembly */
	int16 offset;		/* Index of first byte in fragment */
	int16 last;			/* Index of last byte in fragment */
	char mf;			/* 1 if not last fragment, 0 otherwise */
	char protocol;
	int32 source;		/* Remote IP address */
	int32 dest;			/* Our IP address */
	char tos;
	int16 length;		/* Length of data field of this fragment */
	struct reasm *rp;
	void (*protrecv)();	/* Function to call with completed datagram */
	void tcp_input(),udp_input(),icmp_input();
	void free_reasm();
	struct reasm *lookup_reasm();

	ip = (struct ip_header *)bp->data;
	ip_len = lonibble(ip->v_ihl) * sizeof(int32);
	tos = ip->tos;
	length = ntohs(ip->length) - ip_len;
	id = ntohs(ip->id);
	offset = (ntohs(ip->fl_offs) & F_OFFSET) << 3;	/* Convert to bytes */
	last = offset + length - 1;
	mf = (ntohs(ip->fl_offs) & MF) ? 1:0;
	protocol = ip->protocol;
	source = ntohl(ip->source);
	dest = ntohl(ip->dest);

	/* Initial check for protocols we can't handle */
	switch(protocol & 0xff){
	case TCP_PTCL:
		protrecv = tcp_input;
		break;
	case UDP_PTCL:
		protrecv = udp_input;
		break;
	case ICMP_PTCL:
		protrecv = icmp_input;
		break;
	default:
		/* Send an ICMP Protocol Unknown response */
		ip_errors.protocol++;
		icmp_output(bp,DEST_UNREACH,PROT_UNREACH,(union icmp_args *)NULL);
		free_p(bp);
		return;
	}
	/* Strip IP header, now that we've gotten all the goodies out */
	pullup(&bp,(char *)NULL,ip_len);

	/* Reassemble datagrams. Basic algorithm from RFC-791, page 28 */
	rp = lookup_reasm(source,dest,protocol,id);
	if(offset == 0 && !mf){
		/* Complete datagram received. Discard any earlier fragments */
		if(rp != NULL){
			free_reasm(rp);
		}
	} else {
		char *realloc();
		struct hole *hp,*hpp;
		void ip_timeout();

		/* Fragment received. Run through the reassembly algorithm */
		if(rp == NULL){
			struct reasm *creat_reasm();

			/* First fragment; create new reassembly descriptor,
			 * place at head of reassembly list, initialize hole list
			 */
			if((rp = creat_reasm(offset + length + sizeof(struct hole))) == NULL){
				/* No space for descriptor, drop fragment */
				free_p(bp);
				return;
			}
			rp->source = source;
			rp->dest = dest;
			rp->id = id;
			rp->protocol = protocol;
			rp->timer.start = TLB;
			rp->timer.func = ip_timeout;
			rp->timer.arg = (int *)rp;
			rp->length = offset + length;

			hp = (struct hole *)rp->buf->data;
			hp->first = 0;
			hp->last = 65535;		/* We don't know how long yet */
			hp->next = NULL;

			rp->hole = hp;
		}
		/* Keep restarting timer as long as we keep getting fragments */
		stop_timer(&rp->timer);
		start_timer(&rp->timer);

		/* Expand data area if necessary */
		if(offset + length > rp->length){
			char *realloc();

			rp->length = offset + length;
			rp->buf->_buf = realloc(rp->buf->_buf,rp->length + sizeof(struct hole));
			rp->buf->data = rp->buf->_buf;
			if(rp->buf->data == NULL){
				/* Can't expand reassembly area; give up on whole datagram */
				free_p(bp);
				free_reasm(rp);
				return;
			}
		}
		/* Hole management routine straight from RFC-814, pages 2-3.
		 * The hole descriptor list is threaded right through the gaps
		 * in the data buffer; this works because an IP fragment (and
		 * reassembly hole) is always at least 8 bytes.
		 */
		for(hpp = NULL,hp = rp->hole;hp != NULL; hpp = hp,hp = hp->next){
			if(offset > hp->last || last < hp->first){
				/* New fragment does not interact with this hole; skip */
				continue;
			}
			/* Remove this hole from list */
			if(hpp == NULL){
				rp->hole = hp->next;	/* First on list */
			} else {
				hpp->next = hp->next;
			}
			if(offset > hp->first){
				/* The first part of original hole is not filled;
				 * create new descriptor for smaller hole. It starts
				 * in the same place as before, only the end is different.
				 */
				hp->last = offset - 1;
				/* Put hole descriptor back on list */
				if(hpp == NULL){
					hp->next = rp->hole;
					rp->hole = hp;
				} else {
					hp->next = hpp->next;
					hpp->next = hp;
				}				
			}
			if(last < hp->last && mf){
				/* Mirror of last step, with special test for last fragment */
				int16 tmp;

				tmp = hp->last;
				hp = (struct hole *)&rp->buf->data[last + 1];
				hp->first = last + 1;
				hp->last = tmp;

				/* Put hole descriptor back on list */
				if(hpp == NULL){
					hp->next = rp->hole;
					rp->hole = hp;
				} else {
					hp->next = hpp->next;
					hpp->next = hp;
				}				
			}
			/* It's now safe to copy the new data into the data buffer */
			pullup(&bp,rp->buf->data+offset,length);
			free_p(bp);	/* Shouldn't be necessary */
		}
		if(rp->hole != NULL)
			return;		/* Fragments still missing, return */
		/* We've gotten a complete datagram, so extract it from the
		 * reassembly buffer and pass it on.
		 */
		rp->buf->cnt = rp->length;
		length = rp->length;
		bp = rp->buf;
		rp->buf = NULL;
		free_reasm(rp);
	}
	/* Call the next layer to handle the result */
	(*protrecv)(bp,protocol,source,dest,tos,length);
}

static struct reasm *
lookup_reasm(source,dest,protocol,id)
int32 source;
int32 dest;
char protocol;
int16 id;
{
	struct reasm *rp;

	for(rp = reasmq;rp != NULL;rp = rp->next){
		if(source == rp->source && dest == rp->dest && protocol == rp->protocol
		 && id == rp->id)
			return rp;
	}
	return NULL;
}
#ifdef	FOO
static int16
hash_reasm(source,dest,protocol,id)
int32 source;
int32 dest,
char protocol;
int16 id;
{
	int16 hval;

	hval = loword(source);
	hval ^= hiword(source);
	hval ^= loword(dest);
	hval ^= hiword(dest);
	hval ^= protocol & 0xff;
	hval ^= id;
	hval %= RHASH;
	return hval;
}
#endif
/* Create a reassembly descriptor with associated mbuf of size,
 * put at head of reassembly list
 */
static struct reasm *
creat_reasm(size)
int16 size;
{
	char *calloc();
	register struct reasm *rp;

	if((rp = (struct reasm *)calloc(1,sizeof(struct reasm))) == NULL)
		return NULL;	/* No space for descriptor */
	if((rp->buf = alloc_mbuf(size)) == NULL){
		free((char *)rp);	/* No space for associated data buffer */
		return NULL;
	}
	rp->next = reasmq;
	if(rp->next != NULL)
		rp->next->prev = rp;
	reasmq = rp;
	return rp;
}

/* Free all resources associated with a reassembly descriptor */
static void
free_reasm(rp)
register struct reasm *rp;
{
	stop_timer(&rp->timer);
	/* Remove from list of reassembly descriptors */
	if(rp->prev != NULL)
		rp->prev->next = rp->next;
	else
		reasmq = rp->next;
	if(rp->next != NULL)
		rp->next->prev = rp->prev;
	if(rp->buf != NULL)
		free_p(rp->buf);
	free((char *)rp);
}

/* Handle reassembly timeouts by deleting all reassembly resources */
static void
ip_timeout(arg)
int *arg;
{
	register struct reasm *rp;

	rp = (struct reasm *)arg;
	free_reasm(rp);
}
