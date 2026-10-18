/* Hardware dependent routines for the Xerox 820 */

#include "ax25.h"
#include "820.h"

struct baudrate baudrate[] = {
	800,	0,
	1200,	1,
	1760,	2,
	2144,	3,
	2400,	4,
	4800,	5,
	9600,	6,
	19200,	7,
	28800,	8,
	32000,	9,
	38400,	10,
	57600,	11,
/*	76800,	12,
	115200,	13,
	153600,	14,
	307200,	15, */
	0,	0
};

char siodata[] = {PORT_A,PORT_B};
char sioctl[] = {PORT_A + CTL, PORT_B + CTL};
char baud[] = {BAUDA,BAUDB};

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
	char i_state;
	int i;

	i_state = disable();
	/* Set up SIO interrupt vectors */
	*((int *)siov4) = (int) _ltx0int;
	*((int *)siov5) = (int) _lex0int;
	*((int *)siov6) = (int) _lrx0int;
	*((int *)siov7) = (int) _lsp0int;
	*((int *)siov0) = (int) _ltx1int;
	*((int *)siov1) = (int) _lex1int;
	*((int *)siov2) = (int) _lrx1int;
	*((int *)siov3) = (int) _lsp1int;
	*((int *)KBVEC) = (int) _srxint;
	*((int *)CTC1_INT) = (int) _ctcint;
	*((int *)SYS_INT) = (int) _noint;
	*((int *)GENA_INT) = (int) _noint;
	*((int *)GENB_INT) = (int) _noint;

	/* Load CTC interrupt vector */
	out(CTC0,(CTC0_INT & 0xff) | CTC_VEC);
	/* Initialize CTC channel 0 as 100 hz timer */
	out(CTC0,CTC_RANGE|CTC_LOAD_TIME|CTC_RESET|CTC_SELMODE);
	out(CTC0,98);	 /* 98 * 256 * 400ns ~= .01 sec */
	/* Set up CTC 1 to count down the output of CTC 0 by 10 */
	out(CTC1,CTC_INT_ENAB|CTC_MODE|CTC_LOAD_TIME|CTC_RESET|CTC_SELMODE);
	out(CTC1,10);	/* Divide 100 hz by 10 = .1 sec */
	restore(i_state);
}

/* Set request-to-send on modem */
rts(lineno,x)
int lineno,x;
{
	char cmd;

	if(x)
		cmd = TxCRC_ENAB | RTS | TxENAB | Tx8 | DTR;
	else
		cmd = TxCRC_ENAB | TxENAB | Tx8 | DTR;
	write_sio(lineno,R5,cmd);
}

/* Handle SIO External/Status interrupts */
lexint(lineno)
register int lineno;
{
	register struct line *line;

	line = lines[lineno];
	line->status = in(sioctl[lineno]);	/* Fetch status */
	if((line->status & BRK_ABRT) && line->rfp != NULL){
		line->rfp->iocnt = 0;
		lrxdone(lineno);
	}
	out(sioctl[lineno],RES_EXT_INT);
	ltxint(lineno);	/* Kick transmitter for possible modem change */
}

/* Handle SIO Special Receive Condition interrupts */
lspint(lineno)
register int lineno;
{
	register char x;
	register struct frame *rfp;

	rfp = lines[lineno]->rfp;
	x = read_sio(lineno,R1);	/* Fetch latched bits */
	if((x & END_FR) && rfp != NULL){
		/* Handle end-of-frame */
		rfp->iocnt--;	/* Toss 1st crc byte */
		if(x & (CRC_ERR|Rx_OVR))	/* Bad recv frame */
			rfp->iocnt = 0;			
		lrxdone(lineno);
	}
	out(sioctl[lineno],ERR_RES);
	lrx(lineno);	/* Discard garbage in data register */
}

/* Set HDLC line parameters */
hdlcparam(lineno)
register int lineno;
{
	register struct baudrate *bp;
	register struct line *line;
	char i_state;

	line = lines[lineno];
	/* Initialize SIO channel for SDLC operation
	 * Taken largely from the Zilog Application Brief
	 * "Usng the Z80 SIO With SDLC"
	 */
	i_state = disable();
	write_sio(lineno,R0,CHAN_RESET);
	write_sio(1,R2,siov0 & 0xff);	/* Must be port B */
	write_sio(lineno,R4,SYNC_ENAB | SDLC | X1CLK);
	write_sio(lineno,R1,EXT_INT_ENAB | TxINT_ENAB | ST_AF_VEC | INT_ALL_Rx);
	write_sio(lineno,R7,FLAG);
	write_sio(lineno,R5,TxCRC_ENAB | TxENAB | Tx8 | DTR);
	write_sio(lineno,R3,RxENABLE | RxCRC_ENAB | Rx8);
	restore(i_state);

	for(bp = &baudrate[0];bp->speed != 0;bp++){
		if(bp->speed == line->speed)
			break;
	}
	if(bp->speed == 0){
		tprintf("Invalid speed: %u\n",line->speed);
		return 1;
	}
	out(baud[lineno],bp->val);
	if(line->mode == FULLDUP){
		line->keyup = ON;
		rts(lineno,ON);
	} else if(line->keyup && line->tstate == IDLE){
		line->keyup = OFF;
		rts(lineno,OFF);
	}
	return 0;
}

noint()
{
	tputs("stray interrupt\n");
}
/* Null routine for 820 */
asyncparam()
{
}
