/* ax25.h */
/* Copyright 1984 by Philip Karn, KA9Q
 * Permission granted for noncommercial copying
 * and use provided this notice is retained
 */

#ifdef	SPACE
#define	EXTERN
#else
#define	EXTERN	extern
#endif

/* Memory allocation constants */
#define	MAXPSIZE	1024	/* Maximum I-field + header length */
#define	MAXRPT	8		/* Maximum number of repeaters */
#define	FIFOSIZE	256	/* Size of terminal fifos */
#define	NLINES	2		/* Max number of HDLC lines */
#define	NHASH	29		/* Number of hash chains. Best if prime */
#define	LBUFSIZE	256	/* Size of edited terminal input buffer */

/* Constants of the protocol */
#define	ALEN	6		/* Number of chars in callsign */
#define	MODULO	8		/* Sequence number modulo */
#define	MMASK	0x7		/* Mask for modulo arithmetic */

#define	NULL	0
#ifndef	EOF
#define	EOF	(-1)
#endif
#define	YES	1
#define	ON	1
#define	NO	0
#define	OFF	0
typedef	char	bool;

/* Control field templates */
#define	I	0x00	/* Information frames */
#define	S	0x01	/* Supervisory frames */
#define	RR	0x01	/* Receiver ready */
#define	RNR	0x05	/* Receiver not ready */
#define	REJ	0x09	/* Reject */
#define	U	0x03	/* Unnumbered frames */
#define	SABM	0x2f	/* Set Asynchronous Balanced Mode */
#define	DISC	0x43	/* Disconnect */
#define	DM	0x0f	/* Disconnected mode */
#define	UA	0x63	/* Unnumbered acknowledge */
#define	FRMR	0x87	/* Frame reject */
#define	UI	0x03	/* Unnumbered information */
#define	PF	0x10	/* Poll/final bit */
#define	NONE	0xff	/* None of the above */

/* FRMR reason bits */
#define	W	1	/* Invalid control field */
#define	X	2	/* Unallowed I-field */
#define	Y	4	/* Too-long I-field */
#define	Z	8	/* Invalid sequence number */

/* Format of an AX.25 address - left-shifted callsign plus sub station ID */
struct addr {
	char call[ALEN];	
	char ssid;
};
/* SSID address byte definitions */
#define	SSID		0x1e	/* Sub station ID */
#define	REPEATED	0x80	/* Has-been-repeated bit in repeater field */
#define	E		0x01	/* Address extension bit */
#define	C		0x80	/* Command/response designation */

/* Software timers
 * There is one of these structures for each simulated timer.
 * Whenever the timer is running, it is on a doubly-linked list
 * pointed to by "timers" so that the (hardware) timer interrupt
 * can quickly run through the list and change counts and states.
 * Stopping a timer or letting it expire causes it to be removed
 * from the list; starting a timer puts it on the list.
 */
struct timer {
	struct timer *next;	/* Doubly-linked-list pointers */
	struct timer *prev;
	unsigned start;	/* Period of counter (load value) */
	unsigned count;	/* Ticks to go until expiration */
	char state;	/* See below */
};
/* Timer states */
#define	TIMER_STOP	0
#define	TIMER_RUN	1
#define	TIMER_EXPIRE	2

/* Head of running timer chain */
EXTERN struct timer timers;

EXTERN bool timerflg;	/* Clock interrupt flag */

/* Frame descriptor queues */
struct q {
	unsigned count;
	struct frame *first;
	struct frame *last;
};

struct fifo {
	char buf[FIFOSIZE];	/* storage buffer */
	char *inp;	/* write pointer */
	char *outp;	/* read pointer */
	unsigned count;	/* count of chars in buffer */
};

/* Serial terminal control information */
struct term {
	struct fifo rxfifo;	/* Receive fifo */
	struct fifo txfifo;	/* Transmit fifo */
	struct q txq;		/* Raw transmit queue */
	bool inflow;		/* Input flow controlled flag */
	bool outflow;		/* Output flow controlled flag */
	struct timer timer_cin;	/* Transparent mode input timer */
	char lbuf[LBUFSIZE];	/* Input line buffer */

	/* Terminal-related command parameters */
	unsigned speed;		/* Baud rate */
	char abit;		/* Number of stop bits */
	char awlen;		/* Length of byte */
	char canline;		/* Line cancel character */
	char canpac;		/* Packet cancel character */
	char command;		/* Command recall character */
	char conmode;		/* Connect mode */
	bool cr;		/* Append end-of-line to packet */
	char delete;		/* Erase character */
	bool echo;		/* Control echoing */
	bool flow;		/* Flow control transmit while typing */
	unsigned hiwat;		/* Transmitter high water limit */
	unsigned lowat;		/* Transmitter low water limit */
	struct link *tlink;	/* Link terminal is connected to */
	char mode;		/* Transparent/conversational/command mode */
#define	COMMAND_MODE	0
#define	CONVERS_MODE	1
#define	TRANS_MODE	2
	char parity;		/* Parity on/off even/odd */
	char pass;		/* Transparency prefix character */
	char sendpac;		/* End-of-line character */
	char start;		/* Manual flow restart */
	char stop;		/* Manual flow stop */
	char xoff;		/* Char sent to stop input */
	char xon;		/* Char sent to restart input */
};
EXTERN struct term term;

/* Frame buffer structure.  This is a general-purpose way to
 * handle individual HDLC frames.  These are kept on various
 * linked lists, e.g., receive, transmit, etc.
 * The pointer "mem" points to an allocated buffer
 * in which the actual frame resides.  "iop"
 * can be set up to point to various places within this buffer.
 *
 * The elements next, mem and size must never be touched by user code.
 * The other elements are initialized when allocated and
 * may be used by user code.
 */
struct frame {
	struct frame *next;	/* Forward linked list pointer */

	char *mem;		/* Pointer to associated data buffer */
	unsigned size;		/* Allocated length of data buffer */

	char *iop;		/* Scratch pointer for I/O */
	unsigned iocnt;		/* and count */
};

/* Line state control block
 * There is one of these for each physical
 * HDLC controller channel and modem
 */
struct line {
	char lineno;		/* Line number */
	struct q txq;	/* transmit queue */
	struct frame *tfp;	/* Frame being transmitted */
	char tstate;		/* Transmitter state */
	struct timer ktimer;	/* Keyup timer */
	bool keyup;		/* Transmitter keyed */
	char status;		/* Hardware status bits */
	bool digipeat;		/* Digipeat enabled */
	char mode;		/* Channel access protocol */
	unsigned speed;		/* Hardware baud rate */
	unsigned tcnt;		/* Count of chars transmitted */
	unsigned tframes;	/* Count of frames transmitted */

	struct q rxq;		/* Receive queue */
	struct frame *rfp;	/* Frame being received */
	char rstate;		/* Receiver state */
	unsigned psize;		/* Maximum receive buffer size */
	unsigned rcnt;		/* Count of chars received */
	unsigned rframes;	/* Count of good frames received */
	unsigned rerrors;	/* Count of bad frames received */
	char tin;		/* Trace all received frames */
	char tused;		/* Trace used frames */
	char tout;		/* Trace sent frames */
};
/* HDLC transmitter line states */
#define	IDLE	0	/* No active frame */
#define	DEFER	1	/* Transmit defer (CSMA only) */
#define	KEYUP	2	/* Waiting for transmitter to come up */
#define	START	3	/* Sending the first byte of a frame */
#define	ACTIVE	4	/* Sending data */
#define	FIN1	5	/* Sending first flush character */
#define	FIN2	6	/* Sending second flush character */

/* Line modes */
#define	CSMA	0	/* Transmit on demand, defer on DCD */
#define	ALOHA	1	/* Transmit on demand, ignore DCD */
#define	FULLDUP	2	/* Transmit continuously */

EXTERN struct line *lines[NLINES];
EXTERN int nlines;	/* Actual number */

/* Per-connection link control block
 * These are created and destroyed dynamically,
 * and are indexed through a hash table.
 * One exists for each logical AX.25 Level 2 connection
 */
struct link {
	struct link *next;		/* Doubly linked list pointers */
	struct link *prev;

	struct q txq;			/* I-frames pending transmission */
	struct q rxq;			/* Received I-frames */

	struct addr address[MAXRPT + 2];/* Connected address template */
	unsigned addrlen;		/* Length of same */
	struct line *line;		/* Pointer to HDLC line to use */
	struct frame *i_frames[MODULO];	/* Sent but unacknowledged I-frames */
	bool localbusy;			/* We can't accept any more */
	bool rejsent;			/* REJ frame has been sent */
	bool remotebusy;		/* Remote sent RNR */
	bool waitack;			/* Wait for F-bit ack */
	char vs;			/* Our send state variable */
	char vr;			/* Our receive state variable */
	char remvr;			/* Last reported remote v(r) */
	char unack;			/* Unacked frame count */
	int maxframe;			/* Flow control cnt on output queue */
	bool nproto;			/* Send RR with P to recover from T1
					 * timeouts; otherwise resend I-frames
					 */
	unsigned retries;		/* Retry counter */
	unsigned n2;			/* Retry limit */
	char state;			/* Link state */

	struct timer t1;		/* Retry timer */
	struct timer t2;		/* Acknowledgement delay timer */
	struct timer t3;		/* Keep-alive poll timer */
	char frmrinfo[3];		/* I-field of FRMR frames */
	char tfrmr;			/* Trace frame rejects */
	char tstate;			/* Trace state transitions */
	char ttimer;			/* Trace timer events */
};
/* Link state values */
#define	DISCONNECTED	0
#define	SETUP		1
#define	DISCPENDING	2
#define	CONNECTED	3
#define	FRAMEREJECT	4
#ifndef	SPACE
EXTERN char *pstate[];	/* Ascii descriptions of link state */
EXTERN char *timstate[];	/* Ascii descriptions of timer state */
EXTERN char *lstate[];	/* Ascii descriptions of line states */
EXTERN char *lmodes[];	/* Ascii descriptions of line modes */
#endif

/* Hash table entry for address -> link pointer conversion */
EXTERN struct link *hashtab[NHASH];
EXTERN struct link linkdefault;

/* Special link entry for UI frames */
EXTERN struct link unproto;

/* Function declarations */
char ftype(),*pcall(),*parse(),upper(),disable(),lrx(),srx(),
	*tputs(),*malloc(),*index(),*visible(),*cbyte();
struct frame *getframe(),*allocframe();
int putframe(),recover(),addreq(),getf(),putf(),tputc(),
	restore(),ttyproc(),tflush(),tprintf(),asynparam(),hdlcparam(),
	del_link(),d_line(),d_link();
struct link *find_link(),*cr_link();

/* Command/response address code flag between rawproc() and process() */
#define	UNKNOWN 0
#define	COMMAND	1
#define	RESPONSE	2

/* Global variables set by user command */
EXTERN struct addr mycall;	/* Addr of this station (shifted ascii) */
EXTERN char btext[128];

/* Flags to dump() */
#define	DUMPUSED	1
#define	DUMPFRMR	2
#define	DUMPORIG	3
#define	DUMPIN	4

#define	TDATA	2

/* Level 3 protocols */
#define	NOLEVEL3	0xf0	/* Dump to terminal */
#define	IP		0xcc	/* DoD Internet Protocol */
#define	ARP		0xcd	/* Plummer's Address Resolution Protocol */

EXTERN unsigned memused;	/* Count of allocated frame space */
EXTERN unsigned memlimit;	/* Point at which flow control begins */
EXTERN unsigned aframes;	/* Count of allocated frames */
