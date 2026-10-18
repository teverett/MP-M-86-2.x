/* Interface control structure */
struct interface {
	char *name;			/* Ascii string with interface name */
	struct mbuf *sndq;	/* Encapsulated packets awaiting transmission */
	int16 sndcnt;		/* Number of datagrams on queue */
	int16 mtu;			/* Maximum transmission unit size */
	int (*send)();		/* Routine to call to send datagram */
	int16 dev;			/* Subdevice number to pass to send */
	int16 flags;		/* State of interface */
#define	IF_ACTIVE	0x01

};
