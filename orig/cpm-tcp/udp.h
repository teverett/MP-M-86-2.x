/* User Datagram Protocol definitions */

#define	NUDP	20

/* Structure of a UDP protocol header */
struct udp_header {
	int16 source;		/* Source port */
	int16 dest;			/* Destination port */
	int16 length;		/* Length of header and data */
	int16 checksum;		/* Checksum over pseudo-header, header and data */
};


/* User Datagram Protocol control block
 * Each entry on the receive queue consists of the
 * remote socket structure, followed by any data
 */
struct udp_cb {
	struct udp_cb *prev;	/* Linked list pointers */
	struct udp_cb *next;
	struct socket socket;	/* Local port accepting datagrams */
	void (*r_upcall)();		/* Function to call when one arrives */
	struct mbuf *rcvq;		/* Queue of pending datagrams */
	int rcvcnt;				/* Count of pending datagrams */
};

/* UDP primitives */
int open_udp(),recv_udp(),send_udp(),del_udp();
