/* Lower half of IP, consisting of gateway routines
 * Includes routing and options processing code
 */
#define	NROUTE	13	/* Number of hash chains in routing table */

#include "machdep.h"
#include "mbuf.h"
#include "internet.h"
#include "timer.h"
#include "ip.h"
#include "icmp.h"
#include "interface.h"

/* IP routing table entry */
struct route {
	struct route *prev;	/* Linked list pointers */
	struct route *next;
	int32 target;		/* Target IP address */
	int32 gateway;		/* IP address of local gateway for this target */
	int metric;			/* Hop count, whatever */
	struct interface *interface;	/* Device interface structure */
};

struct route *routes[NROUTE];	/* Routing table */
struct route r_default;			/* Default route entry */

int32 my_addr;
struct ip_errors ip_errors;

/* Route an IP datagram. This is the "hopper" through which all IP datagrams,
 * coming or going, must pass.
 *
 * This router is a temporary hack, since it only does host-specific or
 * default routing (no hierarchical routing yet).
 */
void
ip_route(bp)
struct mbuf *bp;
{
	register struct ip_header *ip;	/* IP header being processed */
	int16 ip_len;					/* IP header length */
	int16 buflen;					/* Length of mbuf */
	int16 length;					/* Total datagram length */
	int32 target;					/* Target IP address */
	register struct route *rp;		/* Route table entry */
	struct route *rt_lookup();
	int opi;						/* Index into options field */
	int opt_len;					/* Length of current option */
	int strict;						/* Strict source routing flag */
	struct mbuf *sbp;				/* IP header for fragmenting */
	int16 fl_offs;					/* fl_offs field of datagram */
	int16 offset;					/* Offset of fragment */
	char precedence;				/* Extracted from tos field */
	char delay;
	char throughput;
	char reliability;
	extern int trace;

	if(trace)
		dump_ip(bp);
	buflen = len_mbuf(bp);
	if(buflen < sizeof(struct ip_header)){
		/* The packet is shorter than a legal IP header */
		ip_errors.runt++;
		free_p(bp);
		return;
	}
	ip = (struct ip_header *)bp->data;
	length = ntohs(ip->length);
	if(buflen > length){
		/* Packet has excess garbage (e.g., Ethernet padding); trim */
		if(bp->next == NULL){
			/* One mbuf, just adjust count */
			bp->cnt = length;
		} else {
			struct mbuf *nbp;

			/* Copy to a new one */
			nbp = copy_p(bp,length);
			free_p(bp);
			bp = nbp;
			ip = (struct ip_header *)bp->data;
		}
	}
	ip_len = lonibble(ip->v_ihl) * sizeof(int32);
	if(ip_len < sizeof(struct ip_header)){
		/* The IP header length field is too small */
		ip_errors.length++;
		free_p(bp);
		return;
	}
	if(cksum((struct pseudo_header *)NULL,bp,ip_len) != 0){
		/* Bad IP header checksum; discard */
		ip_errors.checksum++;
		free_p(bp);
		return;
	}
	if(hinibble(ip->v_ihl) != IPVERSION){
		/* We can't handle this version of IP */
		ip_errors.version++;
		free_p(bp);
		return;
	}
	/* Decrement TTL and discard if zero */
	if(--ip->ttl == 0){
		/* Send ICMP "Time Exceeded" message */
		ip_errors.ttl++;
		icmp_output(bp,TIME_EXCEED,0,(union icmp_args *)NULL);
		free_p(bp);
		return;
	}
	/* Process options, if any. Also compute length of secondary IP
	 * header in case fragmentation is needed later
	 */
	strict = 0;
	for(opi = sizeof(struct ip_header);opi < ip_len; opi += opt_len){
		char *opt;		/* Points to current option */
		int opt_type;	/* Type of current option */
		int pointer;	/* Pointer field of current option */
		int32 *addr;	/* Pointer to an IP address field in option */

		opt = (char *)ip + opi;
		opt_type = opt[0] & OPT_NUMBER;

		/* Handle special 1-byte do-nothing options */
		if(opt_type == IP_EOL)
			break;			/* End of options list, we're done */
		if(opt_type == IP_NOOP){
			opt_len = 1;	/* No operation, skip to next option */
			continue;
		}
		/* Other options have a length field */
		opt_len = opt[1] & 0xff;

		/* Process options */
		switch(opt_type){
		case IP_SSROUTE:	/* Strict source route & record route */
			strict = 1;
		case IP_LSROUTE:	/* Loose source route & record route */
			/* Source routes are ignored unless the datagram appears to
			 * be for us
			 */
			if(ntohl(ip->dest) != my_addr)
				continue;
		case IP_RROUTE:		/* Record route */
			pointer = (opt[2] & 0xff) - 1;
			if(pointer + sizeof(int32) <= opt_len){
				/* Insert our address in the list */
				addr = (int32 *)&opt[pointer];
				if(opt_type != IP_RROUTE)
					/* Old value is next dest only for source routing */
					ip->dest = *addr;
				*addr = htonl(my_addr);
				opt[2] += 4;
			} else {
				/* Out of space; return a parameter problem and drop */
				union icmp_args icmp_args;

				icmp_args.unused = 0;
				icmp_args.pointer = sizeof(struct ip_header) + opi;
				icmp_output(bp,PARAM_PROB,0,&icmp_args);
				free_p(bp);
				return;
			}
			break;
		}
	}

	/* Note this address may have been modified by source routing */
	target = ntohl(ip->dest);

	/* Look up target address in routing table */
	rp = rt_lookup(target);
	if(strict){
		/* Strict source routing requires a direct entry */
		if(rp == NULL || rp->interface == NULL || rp->gateway != target){
			ip_errors.routefail++;
			icmp_output(bp,DEST_UNREACH,ROUTE_FAIL,(union icmp_args *)NULL);
			free_p(bp);
			return;
		}
	} else if(rp == NULL || rp->interface == NULL){
		/* Use default routing entry, if it exists */
		rp = &r_default;
		if(rp->interface == NULL){
			ip_errors.badroute++;
			icmp_output(bp,DEST_UNREACH,HOST_UNREACH,(union icmp_args *)NULL);
			free_p(bp);
			return;
		}
	}
	precedence = PREC(ip->tos);
	delay = ip->tos & DELAY;
	throughput = ip->tos & THRUPUT;
	reliability = ip->tos & RELIABILITY;

	length = ntohs(ip->length);
	if(length <= rp->interface->mtu){
		/* Datagram smaller than interface MTU; send normally */
		/* Recompute header checksum */
		ip->checksum = 0;
		ip->checksum = htons(cksum((struct pseudo_header *)NULL,bp,ip_len));
		(*rp->interface->send)(bp,rp->interface,rp->gateway,
			precedence,delay,throughput,reliability);
		return;
	}
	/* Fragmentation needed */
	fl_offs = ntohs(ip->fl_offs);
	if(fl_offs & DF){
		/* Don't Fragment set; return ICMP message and drop */
		icmp_output(bp,DEST_UNREACH,FRAG_NEEDED,(union icmp_args *)NULL);
		free_p(bp);
		return;
	}
	/* Create copy of IP header for each fragment */
	sbp = copy_p(bp,ip_len);
	pullup(&bp,(char *)NULL,ip_len);
	length -= ip_len;

	/* Create fragments */
	offset = (fl_offs & F_OFFSET) << 3;
	while(length != 0){
		int16 fragsize;			/* Size of this fragment's data */
		struct mbuf *f_header;	/* Header portion of fragment */
		struct ip_header *fip;	/* IP header */
		struct mbuf *f_data;	/* Data portion of fragment */

		f_header = copy_p(sbp,ip_len);
		fip = (struct ip_header *)f_header->data;
		fip->fl_offs = htons(offset >> 3);
		if(length + ip_len <= rp->interface->mtu){
			/* Last fragment; send all that remains */
			fragsize = length;
		} else {
			/* More to come, so send multiple of 8 bytes */
			fragsize = (rp->interface->mtu - ip_len) & 0xfff8;
			fip->fl_offs |= htons(MF);
		}
		fip->length = htons(fragsize + ip_len);
		/* Recompute header checksum */
		fip->checksum = 0;
		fip->checksum = htons(cksum((struct pseudo_header *)NULL,f_header,ip_len));

		/* Extract portion of data and link in */
		f_data = copy_p(bp,fragsize);
		pullup(&bp,(char *)NULL,fragsize);
		f_header->next = f_data;

		(*rp->interface->send)(f_header,rp->interface,rp->gateway,
			precedence,delay,throughput,reliability);		
		offset += fragsize;
		length -= fragsize;
	}
	free_p(sbp);
}

/* Add an entry to the IP routing table. Returns 0 on success, -1 on failure */
int
rt_add(target,gateway,metric,interface)
int32 target;
int32 gateway;
int metric;
struct interface *interface;
{
	struct route *rp,**hp,*rt_lookup();
	int16 hash_ip();
	char *malloc();

	/* A target address of 0.0.0.0 refers to the default entry */
	if(target == 0){
		rp = &r_default;
	} else if((rp = rt_lookup(target)) == NULL){
		/* If the target is not already in the table, create a new
		 * entry and put it in. Otherwise just overwrite the old one.
		 */
		if((rp = (struct route *)malloc(sizeof(struct route))) == NULL)
			return -1;	/* No space */
		/* Insert at head of table */
		rp->prev = NULL;
		hp = &routes[hash_ip(target)];
		rp->next = *hp;
		if(rp->next != NULL)
			rp->next->prev = rp;
		*hp = rp;
	}
	rp->target = target;
	rp->gateway = gateway;
	rp->metric = metric;
	rp->interface = interface;
	return 0;
}

/* Remove an entry from the IP routing table. Returns 0 on success, -1
 * if entry was not in table.
 */
int
rt_drop(target)
int32 target;
{
	register struct route *rp;
	struct route *rt_lookup();

	if(target == 0){
		/* Nail the default entry */
		r_default.interface = NULL;
		return 0;
	}
	if((rp = rt_lookup(target)) == NULL)
		return -1;	/* Not in table */

	if(rp->next != NULL)
		rp->next->prev = rp->prev;
	if(rp->prev != NULL)
		rp->prev->next = rp->next;
	else
		routes[hash_ip(target)] = rp->next;

	free((char *)rp);
	return 0;
}

/* Compute hash function on IP address */
static int16
hash_ip(addr)
register int32 addr;
{
	register int16 ret;

	ret = hiword(addr);
	ret ^= loword(addr);
	ret %= NROUTE;
	return ret;
}

/* Look up target in hash table. Return NULL if not found */
static struct route *
rt_lookup(target)
int32 target;
{
	register struct route *rp;
	int16 hash_ip();

	for(rp = routes[hash_ip(target)];rp != NULL;rp = rp->next){
		if(rp->target == target)
			break;
	}
	return rp;
}
