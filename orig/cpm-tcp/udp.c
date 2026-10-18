/* Send and receive User Datagram Protocol packets */
#include "machdep.h"
#include "mbuf.h"
#include "netuser.h"
#include "udp.h"
#include "internet.h"

struct udp_cb *udps[NUDP];	/* Hash table for UDP structures */

/* Create a UDP control block for lsocket, so that we can queue
 * incoming datagrams.
 */
int
open_udp(lsocket,r_upcall)
struct socket *lsocket;
void (*r_upcall)();
{
	char *malloc();
	register struct udp_cb *up;
	struct udp_cb *lookup_udp();
	int16 hval,hash_udp();

	if((up = lookup_udp(lsocket)) != NULL)
		return 0;	/* Already exists */
	if((up = (struct udp_cb *)malloc(sizeof (struct udp_cb))) == NULL){
		net_error = NO_SPACE;
		return -1;
	}
	up->rcvq = NULL;
	up->rcvcnt = 0;
	up->socket.address = lsocket->address;
	up->socket.port = lsocket->port;
	up->r_upcall = r_upcall;

	hval = hash_udp(lsocket);
	up->next = udps[hval];
	up->prev = NULL;
	up->next->prev = up;
	udps[hval] = up;
	return 0;
}

/* Send a UDP datagram */
int
send_udp(lsocket,fsocket,tos,ttl,bp,length,id,df)
struct socket *lsocket;		/* Source socket */
struct socket *fsocket;		/* Destination socket */
char tos;					/* Type-of-service for IP */
char ttl;					/* Time-to-live for IP */
struct mbuf *bp;			/* Data field, if any */
int16 length;				/* Length of data field */
int16 id;					/* Optional ID field for IP */
char df;					/* Don't Fragment flag for IP */
{
	struct mbuf *hbp;
	int16 hdr_len;
	struct pseudo_header ph;
	struct udp_header *uhdr;

	if(length == 0 && bp != NULL)
		length = len_mbuf(bp);
	hdr_len = sizeof(struct udp_header);
	length += hdr_len;

	/* Allocate UDP protocol header and fill it in */
	if((hbp = alloc_mbuf(hdr_len)) == NULL){
		net_error = NO_SPACE;
		return -1;
	}
	hbp->cnt = hdr_len;

	uhdr = (struct udp_header *)hbp->data;
	uhdr->source = htons(lsocket->port);
	uhdr->dest = htons(fsocket->port);
	uhdr->length = htons(length);

	/* Link in the user data */
	hbp->next = bp;

	/* Create IP pseudo-header, compute checksum and send it */
	ph.length = length;
	ph.source = lsocket->address;
	ph.dest = fsocket->address;
	ph.protocol = UDP_PTCL;
	uhdr->checksum = htons(cksum(&ph,hbp,length));

	ip_send(lsocket->address,fsocket->address,UDP_PTCL,tos,ttl,hbp,length,id,df);
	return length;
}

/* Accept a waiting datagram, if available. Returns length of datagram */
int
recv_udp(lsocket,fsocket,bp)
struct socket *lsocket;		/* Local socket to receive on */
struct socket *fsocket;		/* Place to stash incoming socket */
struct mbuf **bp;			/* Place to stash data packet */
{
	struct udp_cb *lookup_udp();
	register struct udp_cb *up;
	struct socket *sp;
	struct mbuf *buf;
	int16 length;

	up = lookup_udp(lsocket);
	if(up == NULL){
		net_error = NO_CONN;
		return -1;
	}
	if(up->rcvcnt == 0){
		net_error = WOULDBLK;
		return -1;
	}
	buf = dequeue(&up->rcvq);
	up->rcvcnt--;

	sp = (struct socket *)buf->data;
	/* Fill in the user's foreign socket structure, if given */
	if(fsocket != NULL){
		fsocket->address = sp->address;
		fsocket->port = sp->port;
	}
	/* Strip socket header and hand data to user */
	pullup(&buf,(char *)NULL,sizeof(struct socket));
	length = len_mbuf(buf);
	if(bp != NULL)
		*bp = buf;
	else
		free_p(buf);
	return length;
}
/* Delete a UDP control block */
int
del_udp(lsocket)
struct socket *lsocket;
{
	register struct udp_cb *up;
	struct udp_cb *lookup_udp();
	struct mbuf *bp;
	int16 hval;

	if((up = lookup_udp(lsocket)) == NULL){
		net_error = INVALID;
		return -1;
	}		
	/* Get rid of any pending packets */
	while(up->rcvcnt != 0){
		bp = up->rcvq;
		up->rcvq = up->rcvq->anext;
		free_p(bp);
		up->rcvcnt--;
	}
	hval = hash_udp(&up->socket);
	if(udps[hval] == up){
		/* First on list */
		udps[hval] = up->next;
		up->next->prev = NULL;
	} else {
		up->prev->next = up->next;
		up->next->prev = up->prev;
	}
	free((char *)up);
	return 0;
}
/* Process an incoming UDP datagram */
void
udp_input(bp,protocol,source,dest,tos,length)
struct mbuf *bp;
char protocol;
int32 source;
int32 dest;
char tos;
int16 length;
{
	struct pseudo_header ph;
	struct udp_header udp;
	struct udp_cb *up,*lookup_udp();
	struct socket lsocket;
	struct socket *fsocket;
	struct mbuf *sp;

	if(bp == NULL)
		return;

	/* Create pseudo-header and verify checksum */
	ph.source = source;
	ph.dest = dest;
	ph.protocol = protocol;
	ph.length = length;
	if(cksum(&ph,bp,length) != 0){
		/* Checksum failed, ignore completely */
		free_p(bp);
		return;
	}
	/* Extract UDP header in host order */
	pullup(&bp,(char *)&udp,sizeof(struct udp_header));
	udp.dest = ntohs(udp.dest);
	udp.source = ntohs(udp.source);

	/* See if there's somebody around to read it */
	lsocket.address = dest;
	lsocket.port = udp.dest;
	if((up = lookup_udp(&lsocket)) == NULL){
		/* Nope, toss it on the floor */
		free_p(bp);
		return;
	}
	/* Create a buffer which will contain the foreign socket info */
	if((sp = alloc_mbuf(sizeof(struct socket))) == NULL){
		/* No space, drop whole packet */
		free_p(bp);
		return;
	}
	sp->cnt = sizeof(struct socket);

	fsocket = (struct socket *)sp->data;
	fsocket->address = source;
	fsocket->port = udp.source;

	/* Yes, remove the now redundant UDP header, chain the foreign socket
	 * info in front of it and queue it
	 */

	sp->next = bp;
	enqueue(&up->rcvq,sp);
	up->rcvcnt++;
	if(up->r_upcall)
		(*up->r_upcall)(&lsocket,up->rcvcnt);
}
/* Look up UDP socket, return control block pointer or NULL if nonexistant */
static
struct udp_cb *
lookup_udp(socket)
struct socket *socket;
{
	register struct udp_cb *up;
	int16 hash_udp();

	up = udps[hash_udp(socket)];
	while(up != NULL){
		if(bcmp((char *)socket,(char *)&up->socket,sizeof(struct socket)) == 0)
			break;
		up = up->next;
	}
	return up;
}

/* Hash a UDP socket (address and port) structure */
static
int16
hash_udp(socket)
struct socket *socket;
{
	int16 hval;

	/* Compute hash function on socket structure */
	hval = hiword(socket->address);
	hval ^= loword(socket->address);
	hval ^= socket->port;
	hval %= NUDP;
	return hval;
}
