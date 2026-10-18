extern int32 my_addr;	/* Our IP address for ICMP and source routing */

struct ip_header {
	char v_ihl;		/* Version + IP header length */
#define	IPVERSION	4
	char tos;		/* Type of service */
	int16 length;	/* Total length */
	int16 id;		/* Identification */
	int16 fl_offs;	/* Flags + fragment offset */

#define	F_OFFSET	0x1fff	/* Offset field */
#define	DF	0x4000			/* Don't fragment flag */
#define	MF	0x2000			/* More Fragments flag */	

	char ttl;		/* Time to live */
	char protocol;	/* Protocol */
	int16 checksum;	/* Header checksum */
	int32 source;	/* Source address */
	int32 dest;		/* Destination address */
};
/* Fields in option type byte */
#define	OPT_COPIED		0x80	/* Copied-on-fragmentation flag */
#define	OPT_CLASS		0x60	/* Option class */
#define	OPT_NUMBER		0x1f	/* Option number */

/* IP option numbers */
#define	IP_EOL			0		/* End of options list */
#define	IP_NOOP			1		/* No Operation */
#define	IP_SECURITY		2		/* Security parameters */
#define	IP_LSROUTE		3		/* Loose Source Routing */
#define	IP_TIMESTAMP	4		/* Internet Timestamp */
#define	IP_RROUTE		7		/* Record Route */
#define	IP_STREAMID		8		/* Stream ID */
#define	IP_SSROUTE		9		/* Strict Source Routing */

/* Timestamp option flags */
#define	TS_ONLY			0		/* Time stamps only */
#define	TS_ADDRESS		1		/* Addresses + Time stamps */
#define	TS_PRESPEC		3		/* Prespecified addresses only */

/* IP error logging counters */
struct ip_errors {
	int runt;		/* Smaller than minimum size */
	int length;		/* IP header length field too small */
	int version;	/* Wrong IP version */
	int checksum;	/* IP header checksum errors */
	int protocol;	/* Unsupported protocols */
	int ttl;		/* Time-to-live exceeded */
	int badroute;	/* Unreachable destination */
	int routefail;	/* Source route failures */
};
extern struct ip_errors ip_errors;
