/* TCP implementation. Follows RFC 793 as closely as possible */

#define	DEF_WND		2048	/* Default receiver window */
#define	NTCB		19		/* # TCB hash table headers */
#define	RETRY		10		/* Retry limit */
#define	BACKOFF		10		/* Truncation point for backoff algorithm */
#define	DEF_MSS		512		/* Default maximum segment size */
#define	DEF_RTT		5		/* Initial guess at round trip time (5 sec) */
#define	MSL2		30		/* Guess at two maximum-segment lifetimes */

/* TCP segment header */
struct tcp_header {
	int16 source;		/* Source port */
	int16 dest;			/* Destination port */
	int32 seq;			/* Sequence number */
	int32 ack;			/* Acknowledgment number */
	char offset;		/* Data offset */
	char flags;			/* Flags, data offset */
#define	DSHIFT	4		/* Data offset field */
#define	DMASK	0x0f	/* Mask for normalized data offset field */
#define	URG		0x20	/* URGent flag */
#define	ACK		0x10	/* ACKnowledgment flag */
#define	PSH		0x08	/* PuSH flag */
#define	RST		0x04	/* ReSeT flag */
#define	SYN		0x02	/* SYNchronize flag */
#define	FIN		0x01	/* FINal flag */
	int16 wnd;			/* Receiver flow control window */
	int16 checksum;		/* Header + data checksum */
	int16 up;			/* Urgent pointer */
};

/* Format of the Maximum Segment Size option (the only one currently
 * defined in TCP)
 */
struct mss {
	char kind;		/* Must be 2 */
#define	MSS_KIND	2
	char length;	/* Must be 4 */
#define	MSS_LENGTH	4
	int16 mss;	/* The actual value */
};

/* Resequencing queue entry */
struct reseq {
	struct reseq *next;		/* Linked-list pointer */
	char tos;				/* Type of service */
	struct tcp_header seg;	/* TCP header */
	struct mbuf *bp;		/* data */
	int16 length;			/* data length */
};

/* TCP connection control block */
struct tcb {
	struct tcb *prev;	/* Linked list pointers for hash table */
	struct tcb *next;

	struct connection conn;

	char state;	/* Connection state */
#define	CLOSED			0
#define	LISTEN			1
#define	SYN_SENT		2
#define	SYN_RECEIVED	3
#define	ESTABLISHED		4
#define	FINWAIT1		5
#define	FINWAIT2		6
#define	CLOSE_WAIT		7
#define	CLOSING			8
#define	LAST_ACK		9
#define	TIME_WAIT		10

	char reason;			/* Reason for closing */
#define	NORMAL			0	/* Normal close */
#define	RESET			1	/* Reset by other end */
#define	TIMEOUT			2	/* Excessive retransmissions */
#define	NETWORK			3	/* Network problem (ICMP message) */

/* If reason == NETWORK, the ICMP type and code values are stored here */
	char type;
	char code;

	/* Send sequence variables */
	struct {
		int32 una;	/* First unacknowledged sequence number */
		int32 nxt;	/* Next sequence num to be sent for the first time */
		int32 ptr;	/* Working transmission pointer */
		int16 wnd;	/* Other end's offered receive window */
		int16 up;	/* Send urgent pointer */
		int32 wl1;	/* Sequence number used for last window update */
		int32 wl2;	/* Ack number used for last window update */
	} snd;
	int32 iss;		/* Initial send sequence number */

	/* Receive sequence variables */
	struct {
		int32 nxt;	/* Incoming sequence number expected next */
		int16 wnd;	/* Our offered receive window */
		int16 up;	/* Receive urgent pointer */
	} rcv;
	int32 irs;		/* Initial receive sequence number */
	int16 mss;		/* Maximum segment size */

	char retry;		/* Retransmission retry count */
	char backoff;	/* Retransmission timer backoff stage */
	void (*r_upcall)();	/* Call when "significant" amount of data arrives */
	void (*t_upcall)();	/* Call when ok to send more data */
	void (*s_upcall)();	/* Call when connection state changes */
	char force;			/* We owe the other end an ACK or window update */
	char tos;		/* Type of service (for IP) */

	int16 window;		/* Receiver window and send queue limit */
	struct mbuf *rcvq;	/* Receive queue */
	int16 rcvcnt;

	struct mbuf *sndq;	/* Send queue */
	int16 sndcnt;		/* Number of unacknowledged sequence numbers on
						 * send queue. NB: includes SYN and FIN, which don't
						 * actually appear on sndq!
						 */
	struct reseq *reseq;	/* Out-of-order segment queue */

	struct timer retrans;	/* Retransmission timer */
	struct timer rtt;		/* Round trip timer */
	int32 rttseq;			/* Sequence number being timed */
	int32 srtt;				/* Smoothed round trip time, milliseconds */
	struct timer msl;		/* 2MSL timer */
};

/* TCP statistics counters */
struct tcp_errors {
	int runt;		/* Smaller than minimum size */
	int checksum;	/* TCP header checksum errors */
};
extern struct tcp_errors tcp_errors;

#define	min(x,y)	((x)<(y)?(x):(y))
#define max(x,y)	((x)>(y)?(x):(y))
extern struct tcb *tcbs[];
extern int32 iss();
struct tcb *lookup_tcb();
struct tcb *create_tcb();
void rehash_tcb(),tcp_output(),tcp_input(),close_self(),dump_seg(),
	setstate();

/* TCP primitives */
struct tcb *open_tcp();
int send_tcp(),recv_tcp(),close_tcp(),del_tcp();
void state_tcp();
