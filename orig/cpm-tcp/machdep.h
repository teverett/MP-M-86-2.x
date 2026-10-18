/* This file defines certain low level operations and characteristics that
 * are likely to be machine dependent.
 */
#define	Z80
#define	CPM
#define	LITTLE_ENDIAN		/* Comment out for Big Endian machines */
/*#define void int*/		/* Uncomment if your compiler doesn't like void */

/* These two lines assume that your compiler's longs are 32 bits and
 * shorts are 16 bits. It is already assumed that chars are 8 bits,
 * but it doesn't matter if they're signed or unsigned.
 */
typedef long int32;				/* 32-bit signed integer */
typedef unsigned short int16;	/* 16-bit unsigned integer */

#ifdef	Z80	/* AZTEC definitions */
#define	bcopy(a,b,cnt)	movmem(a,b,cnt)
#endif

#ifdef	LITTLE_ENDIAN
int32 ntohl();
int16 ntohs();
#else	/* Degenerate case for Big Endian machines */
#define	ntohl(x)	(x)
#define ntohs(x)	(x)
#endif

int16 cksum();

/* Host-to-network and network-to-host are symmetrical */
#define htonl(x)	ntohl(x)
#define	htons(x)	ntohs(x)

#ifdef	Z80	/* Assembler routines are available */
int16 hinibble(),lonibble(),hibyte(),lobyte(),hiword(),loword();

#else

/* Extract a short from a long */
#define	hiword(x)	((int16)((x) >> 16))
#define	loword(x)	((int16)(x))

/* Extract a byte from a short */
#define	hibyte(x)	(((x) >> 8) & 0xff)
#define	lobyte(x)	((x) & 0xff)

/* Extract nibbles from a byte */
#define	hinibble(x)	(((x) >> 4) & 0xf)
#define	lonibble(x)	((x) & 0xf)

#endif

#define	NULL	0
