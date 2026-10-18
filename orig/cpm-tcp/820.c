/* Xerox 820 SIO functions */
#include "820.h"
#define	min(x,y)	((x)<(y)?(x):(y))

/* Interrupt vector handlers (assembler) */
int _ctcint(),_stx0int(),_stx1int(),
	_srx0int(),_srx1int(),
	_ctxint(),_crxint(),
	_ssp0int(),_ssp1int(),
	_sex0int(),_sex1int(),
	_noint();

/* Assembler primitives */
char in(),out(),iff(),ireg(),im2(),ei(),di(),halt();

/* Other utility routines */
char read_sio();
int srx();

#define	NMIVEC	0x0066
#define	RET	0xc9

/* Xerox 820 SIO Port definitions */
#define	PORT_A	0x4
#define	PORT_B	0x5
#define	CTL	0x2
#define	DATA	0
#define	CTC0	0x18	/* Counter/timer channel 0 port */
#define	CTC1	0x19	/* Counter/timer channel 1 port */
#define	CTC2	0x1a	/* Counter/timer channel 2 port */
#define	CTC3	0x1b	/* Counter/timer channel 3 port */
#define	KBSTAT	0x1c	/* Keyboard status register */
#define	KBMASK	0x8	/* Keyboard data available flag */
#define	KBDAT	0x1e	/* Parallel Keyboard data input */

/* Xerox 820 Interrupt vector linkage */
#define	siov0	0xff00	/* SIO Port b xmit buffer empty */
#define	siov1	0xff02	/* SIO Port b external/status change */
#define	siov2	0xff04	/* SIO Port b receive data available */
#define	siov3	0xff06	/* SIO Port b special receive condition */
#define	siov4	0xff08	/* SIO Port a xmit buffer empty */
#define	siov5	0xff0a	/* SIO Port a external/status change */
#define	siov6	0xff0c	/* SIO Port a receive data available */
#define	siov7	0xff0e  /* SIO Port a special receive condition */
#define	CTC0_INT	0xff10	/* CTC0 interrupt vector */
#define	CTC1_INT	0xff12
#define	CTC2_INT	0xff14
#define	CTC3_INT	0xff16
#define	SYS_INT	0xff18
#define	KBVEC	0xff1a	/* Parallel Keyboard interrupt vector */
#define	GENA_INT	0xff1c
#define	GENB_INT	0xff1e

#define	FLAG	0x7e	/* SDLC/HDLC flag (01111110) */

/* SIO Register definitions */

/* Write Register 0 */
#define	R0	0		/* Register selects */
#define	R1	1
#define	R2	2
#define	R3	3
#define	R4	4
#define	R5	5
#define	R6	6
#define	R7	7

#define	NULLCODE	0	/* Null Code */
#define	SEND_ABORT	0x8	/* Send abort (SDLC) */
#define	RES_EXT_INT	0x10	/* Reset Ext. Status Interrupts */
#define	CHAN_RESET	0x18	/* Channel Reset */
#define	RES_RxINT_FC	0x20	/* Reset RxINT on First Character */
#define	RES_Tx_P	0x28	/* Reset TxINT Pending */
#define	ERR_RES		0x30	/* Error Reset */
#define	RET_INT		0x38	/* Return From Int (Chan A Only) */

#define	RES_Rx_CRC	0x40	/* Reset Rx CRC Checker */
#define	RES_Tx_CRC	0x80	/* Reset Tx CRC Checker */
#define	RES_CRC_L	0xC0	/* Reset CRC/SYNCS Sent/Sending Latch */

/* Write Register #1 */

#define	EXT_INT_ENAB	0x1	/* Ext Int Enable */
#define	TxINT_ENAB	0x2	/* Tx Int Enable */
#define	ST_AF_VEC	0x4	/* Status affects vector (CH b only) */

#define	RxINT_DISAB	0	/* Rx Int Disable */
#define	RxINT_FCERR	0x8	/* Rx Int on First Character Only or Error */
#define	PV_INT_ALL_Rx	0x10	/* Int on all Rx Characters (parity affects
				   vector) */
#define	INT_ALL_Rx	0x18	/* Int on all Rx Characters (parity does not
				   affect vector) */

#define	WT_RDY_RT	0x20	/* Wait/Ready on R/T */
#define	WT_FN_RDYFN	0x40	/* Wait/FN/Ready FN */
#define	WT_RDY_ENAB	0x80	/* Wait/Ready Enable */

/* Write Register #2 (Interrupt Vector) */

/* Write Register 3 */

#define	RxENABLE	0x1	/* Rx Enable */
#define	SYNC_L_INH	0x2	/* Sync Character Load Inhibit */
#define	ADD_SM		0x4	/* Address Search Mode (SDLC) */
#define	RxCRC_ENAB	0x8	/* Rx CRC Enable */
#define	ENT_HM		0x10	/* Enter Hunt Mode */
#define	AUTO_ENAB	0x20	/* Auto Enables */
#define	Rx5		0x0	/* Rx 5 Bits/Character */
#define	Rx7		0x40	/* Rx 7 Bits/Character */
#define	Rx6		0x80	/* Rx 6 Bits/Character */
#define	Rx8		0xc0	/* Rx 8 Bits/Character */

/* Write Register 4 */

#define	PAR_ENA		0x1	/* Parity Enable */
#define	PAR_EVEN	0x2	/* Parity Even/Odd* */

#define	SYNC_ENAB	0	/* Sync Modes Enable */
#define	SB1		0x4	/* 1 stop bit/char */
#define	SB15		0x8	/* 1.5 stop bits/char */
#define	SB2		0xc	/* 2 stop bits/char */

#define	MONSYNC		0	/* 8 Bit Sync character */
#define	BISYNC		0x10	/* 16 bit sync character */
#define	SDLC		0x20	/* SDLC Mode (01111110 Sync Flag) */
#define	EXTSYNC		0x30	/* External Sync Mode */

#define	X1CLK		0x0	/* x1 clock mode */
#define	X16CLK		0x40	/* x16 clock mode */
#define	X32CLK		0x80	/* x32 clock mode */
#define	X64CLK		0xC0	/* x64 clock mode */

/* Write Register 5 */

#define	TxCRC_ENAB	0x1	/* Tx CRC Enable */
#define	RTS		0x2	/* RTS */
#define	SDLC_CRC	0x4	/* SDLC/CRC-16 */
#define	TxENAB		0x8	/* Tx Enable */
#define	SND_BRK		0x10	/* Send Break */
#define	Tx5		0x0	/* Tx 5 bits (or less)/character */
#define	Tx7		0x20	/* Tx 7 bits/character */
#define	Tx6		0x40	/* Tx 6 bits/character */
#define	Tx8		0x60	/* Tx 8 bits/character */
#define	DTR		0x80	/* DTR */

/* Write Register 6 (Sync bits 0-7/SDLC Address Field) */

/* Write Register 7 (Sync bits 8-15/SDLC 01111110) */

/* Read Register 0 */
#define	Rx_CH_AV	0x1	/* Rx Character Available */
#define	INT_PEND	0x2	/* Int pending (ch a only) */
#define	Tx_BUF_EMP	0x4	/* Tx Buffer empty */
#define	DCD		0x8	/* DCD */
#define	SYNC_HUNT	0x10	/* Sync/hunt */
#define	CTS		0x20	/* CTS */
#define	SND_CRC		0x40	/* Sending CRC/SYNCS */
#define	BRK_ABRT	0x80	/* Break/Abort */

/* Read Register 1 */
#define	ALL_SNT		0x1	/* All sent */
/* Residue Data for 8 Rx bits/char programmed */
#define	RES3		0x8	/* 0/3 */
#define	RES4		0x4	/* 0/4 */
#define	RES5		0xc	/* 0/5 */
#define	RES6		0x2	/* 0/6 */
#define	RES7		0xa	/* 0/7 */
#define	RES8		0x6	/* 0/8 */
#define	RES18		0xe	/* 1/8 */
#define	RES28		0x0	/* 2/8 */
/* Special Rx Condition Interrupts */
#define	PAR_ERR		0x10	/* Parity error */
#define	Rx_OVR		0x20	/* Rx Overrun Error */
#define	CRC_ERR		0x40	/* CRC/Framing Error */
#define	END_FR		0x80	/* End of Frame (SDLC) */

/* Read Register 2 (channel b only) - Interrupt vector */

/* CTC register definitions */

#define	CTC_INT_ENAB	0x80	/* Interrupt enable */
#define	CTC_MODE	0x40	/* Counter/timer* mode */
#define	CTC_RANGE	0x20	/* Timer divided by 256/16* */
#define	CTC_SLOPE	0x10	/* Positive edge triggers/decrements */
#define	CTC_TRIGGER	0x8	/* Enable external trigger on timer */
#define	CTC_LOAD_TIME	0x4	/* Load time constant on next write */
#define	CTC_RESET	0x2	/* Stop operation */
#define	CTC_SELMODE	0x1	/* Select mode */
#define	CTC_VEC		0	/* Load interrupt vector */

/* Baud rate generator definitions */
struct baudrate {
	unsigned speed;
	char val;
};
#define	BAUDA	0
#define	BAUDB	0xc

extern char sioctl[],siodata[],baud[];

/* User macros */
#define	dcd(l)		(lines[l]->status & DCD)
#define	cts(l)		(lines[l]->status & CTS)
#define	ltxgo1(l)	(out(sioctl[l],RES_Tx_CRC))
#define	ltxgo2(l)	(out(sioctl[l],RES_CRC_L))
#define	ltxabort(l)	(out(sioctl[l],SEND_ABORT))
#define	lrxabort(l)	(write_sio(l,R3,ENT_HM|RxENABLE|RxCRC_ENAB|Rx8))
#define	ltxrdy(l)	(in(sioctl[l]) & Tx_BUF_EMP)
#define	lrxrdy(l)	(in(sioctl[l]) & Rx_CH_AV)
#define	ltx(l,c)	(out(siodata[l],c))
#define	ltxdone(l)	(out(sioctl[l],RES_Tx_P))
#define	lrx(l)		(in(siodata[l]))
#define	ctx(c)		putchar(c)
#define	ctxrdy()	(1)

/* Baud rate table for x16 clock */
struct baudrate baudrate[] = {
	50,	0,
	75,	1,
	110,	2,
	134,	3,
	150,	4,
	300,	5,
	600,	6,
	1200,	7,
	1800,	8,
	2000,	9,
	2400,	10,
	3600,	11,
	4800,	12,
	7200,	13,
	9600,	14,
	19200,	15,
	0,	0
};
char siodata[] = {PORT_A,PORT_B};
char sioctl[] = {PORT_A + CTL, PORT_B + CTL};
char baud[] = {BAUDA,BAUDB};

struct dma sdma[2];
struct fifo sfifo[2];
struct fifo cfifo;

/* Set line speed */
aparam(dev,speed)
int dev;
int speed;
{
	register struct baudrate *bp;

	if(dev >= 2)
		return;
	for(bp = &baudrate[0];bp->speed != 0;bp++){
		if(bp->speed == speed){
			out(baud[dev],bp->val);
			break;
		}
	}
}
/* Send a buffer to serial transmitter */
asend(dev,buf,cnt)
int dev;
char *buf;
unsigned short cnt;
{
	register struct dma *dp;

	if(dev >= 2)
		return;
	dp = &sdma[dev];
	if(dp->flags)
		return;	/* Already busy */
	dp->data = buf + 1;
	dp->cnt = cnt - 1;
	dp->flags = 1;
	/* Prime transmitter to get things started */
	out(siodata[dev],*buf);
}

/* Write SIO register */
write_sio(lineno,reg,val)
char lineno,reg,val;
{
	char i_state;
	register char port;

	port = sioctl[lineno];
	i_state = disable();
	out(port,NULLCODE|reg);
	out(port,val);
	restore(i_state);
}
/* Read SIO register */
char
read_sio(lineno,reg)
char lineno,reg;
{
	char c,i_state;
	register char port;

	port = sioctl[lineno];
	i_state = disable();
	out(port,NULLCODE|reg);
	c = in(port);
	restore(i_state);
	return c;
}
/* Startup initialization */
ioinit()
{
	int dev;

	di();
	ireg(0xff);
	/* The 820 gates the floppy INTREQ and DREQ lines to the
	 * Z80's NMI, enabling them in hardware whenever
	 * the CPU is halted.
	 * Since we halt in the main loop, we have to stash a return
	 * in the NMI vector location, or risk much grief!
	 */
	*((char *)NMIVEC) = RET;

	/* Set up SIO interrupt vectors */
	*((int *)siov4) = (int) _stx0int;
	*((int *)siov5) = (int) _sex0int;
	*((int *)siov6) = (int) _srx0int;
	*((int *)siov7) = (int) _ssp0int;
	*((int *)siov0) = (int) _stx1int;
	*((int *)siov1) = (int) _sex1int;
	*((int *)siov2) = (int) _srx1int;
	*((int *)siov3) = (int) _ssp1int;
	*((int *)KBVEC) = (int) _crxint;

	/* The manual lies; the 820 monitor uses ALL CTC channels.
	 * Since the monitor already has a 1-second tick on CTC-3,
	 * we divert it to our timer routine, which in due course
	 * calls the disk turnoff timer routine. NB: this obviously assumes
	 * the existence of the 820 monitor!
	 */
#ifdef	DOTIMER
	*((int *)CTC1_INT) = (int) _ctcint;
#else	/* steal 820 monitor's disk timer */
	*((int *)CTC3_INT) = (int) _ctcint;
#endif
	*((int *)SYS_INT) = (int) _noint;
	*((int *)GENA_INT) = (int) _noint;
	*((int *)GENB_INT) = (int) _noint;

#ifdef	DOTIMER
	/* Load CTC interrupt vector */
	out(CTC0,(CTC0_INT & 0xff) | CTC_VEC);
	/* Initialize CTC channel 0 as 100 hz timer */
	out(CTC0,CTC_RANGE|CTC_LOAD_TIME|CTC_RESET|CTC_SELMODE);
	out(CTC0,98);	 /* 98 * 256 * 400ns ~= .01 sec */
	/* Set up CTC 1 to count down the output of CTC 0 by 100 */
	out(CTC1,CTC_INT_ENAB|CTC_MODE|CTC_LOAD_TIME|CTC_RESET|CTC_SELMODE);
	out(CTC1,100);	/* Divide 100 hz by 100 = 1.0 sec */
#endif

	/* Initialize keyboard buffer */
	cfifo.rp = cfifo.wp = cfifo.buf;
	cfifo.cnt = 0;

	/* Initialize SIO ports and buffers */
	for(dev=0;dev<2;dev++){
		/* Initialize receiver FIFO pointers */
		sfifo[dev].rp = sfifo[dev].wp = sfifo[dev].buf;
		sfifo[dev].cnt = 0;

		/* Clear transmit pseudo-DMA */
		sdma[dev].data = 0;
		sdma[dev].cnt = 0;
		sdma[dev].flags = 0;

		write_sio(dev,R0,CHAN_RESET);
		write_sio(1,R2,siov0 & 0xff);	/* must be port B */
		write_sio(dev,R4,X16CLK|SB1);
		write_sio(dev,R1,EXT_INT_ENAB|TxINT_ENAB|ST_AF_VEC|INT_ALL_Rx);
		write_sio(dev,R5,RTS|TxENAB|Tx8|DTR);
		write_sio(dev,R3,RxENABLE|Rx8);
	}
	im2();
	ei();
}
/* Read input fifo for serial line; return number of bytes read */
int
srx(fp,buf,cnt)
register struct fifo *fp;
char *buf;
unsigned cnt;
{
	int n,tot;
	char i_state;	

	tot = 0;
	i_state = disable();
	for(;;){
		n = min(cnt,fp->cnt);
		if(n == 0)
			break;
		n = min(n,&fp->buf[256] - fp->rp);
		movmem(fp->rp,buf,n);
		fp->rp += n;
		if(fp->rp >= &fp->buf[256])
			fp->rp = fp->buf;
		fp->cnt -= n;
		buf += n;
		tot += n;
		cnt -= n;
	}
	restore(i_state);
	return tot;
}
/* Handle completion "interrupt" from pseudo-dma transmitter */
stxint(dev)
int dev;
{
}
stxrdy(dev)
int dev;
{
	return (sdma[dev].flags == 0);
}
sexint(dev)
int dev;
{
	write_sio(dev,R0,RES_EXT_INT);
}
sspint(dev)
int dev;
{
	write_sio(dev,R0,ERR_RES);
}
noint()
{
}
