/* 820.h */
/* Copyright 1986 by Philip Karn, KA9Q
 * Permission granted for noncommercial copying
 * and use provided this notice is retained
 */

#define	NSLIP	2			/* Number of SLIP lines */

/* Hardware-dependent interface structures for the Xerox 820 using a
 * Zilog Z80-SIO chip */

/* Output pseudo-dma control structure */
struct dma {
	char *data;	/* current output pointer */
	unsigned short cnt;	/* byte count remaining */
	char flags;	/* transmitter active */
};

/* Read fifo control structure */
struct fifo {
	char buf[256];	/* Ring buffer */
	char *wp;	/* Write pointer */
	char *rp;	/* Read pointer */
	unsigned short cnt;	/* count of chars in buffer */
};
extern char _clktick;
extern struct fifo cfifo,sfifo[];

