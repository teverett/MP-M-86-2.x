/* fad.c */
/* Copyright 1984 by Philip Karn, KA9Q
 * Permission granted for noncommercial copying
 * and use provided this notice is retained
 */
/* Low level I/O drivers for the FADCA 8530 board add-on for the 820
 */

#include "ax25.h"
#include "fad.h"

/* lrxint - line receiver interrupt
 * (HDLC Receiver interrupt handler)
 * This function takes buffers off the freelist,
 * fills them with receive data, and puts them on the receive queue.
 * It assumes that the data buffer for the entire frame (address,
 * control & data) has been pre-allocated and is pointed to by the
 * "mem" entry.  Incoming bytes are stuffed
 * directly into this array with no interpretation. Upper levels
 * will later scan the raw data of "iocnt" bytes pointed to by "iop".
 */
lrxint(lineno)
register int lineno;
{
	register struct line *line;
	register struct frame *fp;

	line = lines[lineno];
	while(lrxrdy(lineno)){
		if((fp = line->rfp) == NULL
		 && (fp = line->rfp = allocframe(line->psize)) == NULL){
			lrxabort(lineno);
			lrx(lineno);
			write_sio(lineno,R0,RET_INT);
			return;	/* No buffers; we'll lose the frame */
		}
		/* Normal save */
		*fp->iop++ = lrx(lineno);
		if(++fp->iocnt >= fp->size){
			/* Buffer overflow; toss the frame */
			fp->iocnt = 0;
			lrxabort(lineno);
			lrxdone(lineno);
		}
	}
	write_sio(lineno,R0,RET_INT);
}
/* ltxint - line transmit interrupt
 * (HDLC transmit interrupt service routine)
 *
 * The state variable tstate, along with some static pointers,
 * represents the state of the transmit "process".
 *
 * The queued frames for transmission must have the following fields set:
 * 		iop - point to frame buffer for transmission
 *		iocnt - length of frame
 * The data pointed to by "iop" will be sent as-is.
 */
ltxint(lineno)
register int lineno;
{
	register struct line *line;
	register struct frame *fp;
	char i_state;

	line = lines[lineno];
	i_state = disable();
	while(ltxrdy(lineno)){
		switch(line->tstate){
		/* First here for efficiency */
		case ACTIVE:		/* Sending frame */
			fp = line->tfp;
			if(fp->iocnt-- != 0){
				ltx(lineno,*fp->iop++);
			} else {
				/* Do this after sending the last byte */
				line->tstate = IDLE;
				freeframe(fp);
				line->tfp = NULL;
				ltxdone(lineno);
				goto ret;
			}
			break;
		case IDLE:
			/* Transmitter idle. Find a frame for transmission */
			while((fp = getframe(&line->txq)) != NULL){
				/* Paranoid check for empty frames */
				if(fp->iop != NULL && fp->iocnt > 2){
					break;
				} else {
					freeframe(fp);
				}
			}
			if(fp == NULL){
				/* Queue empty, turn off interrupt */
				if(line->mode != FULLDUP && line->keyup){
					/* Start shutdown sequence */
					ltx(lineno,0);
					line->tstate = FIN1;
				} else
					ltxdone(lineno);
				goto ret;	/* Wait for work */
			}
			line->tcnt += fp->iocnt;
			line->tframes++;
			line->tfp = fp;
			if(line->keyup){
				/* Bypass access procedure if already going */
				line->tstate = START;	/* And collect $200 */
				break;
			}
			line->tstate = DEFER;	/* note fall-thru */
		case DEFER:
			if(line->mode == CSMA && dcd(lineno) && !line->keyup){
				/* Wait for receiver carrier to drop */
				ltxdone(lineno);
				goto ret;
			}
			rts(lineno,ON);	/* Transmitter on */
			line->keyup = YES;
			line->tstate = KEYUP;
			if(line->ktimer.start != 0){
				start_timer(&line->ktimer);
			}
		case KEYUP:	/* note fall-thru */
			if(line->ktimer.state == TIMER_RUN || !cts(lineno)){
				/* Wait for timer to expire */
				ltxdone(lineno);
				goto ret;
			}
		case START:	/* Note fall-thru */
			line->tfp->iocnt--;
			ltxgo1(lineno);
			line->tstate = ACTIVE;
			ltx(lineno,*line->tfp->iop++);
			ltxgo2(lineno);
			break;
		case FIN1:	/* Sending flush character */
			ltx(lineno,0);
			line->tstate = FIN2;
			break;
		case FIN2:
			ltxabort(lineno);
			line->keyup = NO;
			line->tstate = IDLE;
			rts(lineno,OFF);
			ltxdone(lineno);
			break;
		}
	}
ret:	write_sio(lineno,R0,RET_INT);
	restore(i_state);
}

/* Line receiver end-of-frame interrupt */
lrxdone(lineno)
int lineno;
{
	register struct line *line;
	register struct frame *fp;

	line = lines[lineno];
	if((fp = line->rfp) == NULL)
		return;	/* Abort with no active frame */
	/* Receiver has finished frame */
	fp->iop = fp->mem;
	if(fp->iocnt == 0){	/* Toss bad frames back */
		line->rerrors++;
	} else {
		/* End of normal frame, no errors */
		line->rframes++;
		line->rcnt += fp->iocnt;
		putframe(&line->rxq,fp);
		line->rfp = allocframe(line->psize);
	}
	write_sio(lineno,R0,RET_INT);
}

char siodata[] = {PORT_A + DATA,PORT_B + DATA};
char sioctl[] = {PORT_A + CTL, PORT_B + CTL};

/* Write SIO register */
write_sio(lineno,reg,val)
char lineno,reg,val;
{
	char i_state;
	register char port;

	port = sioctl[lineno];
	i_state = disable();
	out(port,NULLCODE|reg);	/* Note that point high is also written */
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
	out(port,NULLCODE|reg);	/* Note that point high is also written */
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
	/* Set up 8530 interrupt vectors */
	*((int *)SCC_ATBE) = (int) _ltx0int;
	*((int *)SCC_AEXT) = (int) _lex0int;
	*((int *)SCC_ARCA) = (int) _lrx0int;
	*((int *)SCC_ASRC) = (int) _lsp0int;
	*((int *)SCC_BTBE) = (int) _ltx1int;
	*((int *)SCC_BEXT) = (int) _lex1int;
	*((int *)SCC_BRCA) = (int) _lrx1int;
	*((int *)SCC_BSRC) = (int) _lsp1int;
	*((int *)KBVEC) = (int) _srxint;
	*((int *)CTC1_INT) = (int) _ctcint;
	*((int *)SYS_INT) = (int) _noint;

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
	write_sio(lineno,R0,RET_INT);
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
	write_sio(lineno,R0,RET_INT);
}

/* Set HDLC line parameters */
hdlcparam(lineno)
register int lineno;
{
	unsigned int tc;
	register struct line *line;
	char i_state;

	line = lines[lineno];
	/* Initialize 8530 channel for SDLC operation */
	i_state = disable();
	switch(lineno){
	case 0:
		write_sio(lineno,R9,CHRA);	/* Reset channel A */
		break;
	case 1:
		write_sio(lineno,R9,CHRB);	/* Reset channel B */
		break;
	}
	/* Wait/DMA disable, Int on all Rx chars + spec condition,
	 * parity NOT spec condition, TxINT enable, Ext Int enable */
	write_sio(lineno,R1, INT_ALL_Rx | TxINT_ENAB | EXT_INT_ENAB);

	/* Write interrupt vector (either port) */
	write_sio(1,R2,SCC_BTBE & 0xff);

	/* 8 bit RX chars, auto enables off, no hunt mode, RxCRC enable,
	 * no address search, no inhibit sync chars, enable RX
	 */
	write_sio(lineno,R3,Rx8|RxCRC_ENAB|RxENABLE);

	/* X1 clock, SDLC mode, Sync modes enable, parity disable */
	write_sio(lineno,R4,X1CLK | SDLC | SYNC_ENAB);

	/* DTR On, 8 bit TX chars, no break, TX enable, SDLC CRC,
	 * RTS off, TxCRC enable */
	write_sio(lineno,R5,DTR|Tx8|TxENAB|TxCRC_ENAB);

	/* SDLC flag */
	write_sio(lineno,R7,FLAG);

	/* No reset, status low, master int enable, enable lower chain,
	 * return vector, vector includes status */
	write_sio(lineno,R9,MIE|VIS);

	/* CRC preset 1, NRZI encoding, no active on poll, flag idle,
	 * flag on underrun, no loop mode, 8 bit sync
	 */
	write_sio(lineno,R10,CRCPS|NRZI);

#ifdef	OLDFAD
	/* RTxC NO xtal, receive clock = DPLL output,
	 * transmit clock = DPLL output, TRxC = output, TRxCout = DPLL
	 */
	write_sio(lineno,R11,RCDPLL|TCDPLL|TRxCOI|TRxCDP);
#else
	switch(lineno){
	case 0:	/* Channel A: RTxC NO xtal, receive clock = DPLL output,
		 * transmit clock = DPLL output, TRxC = output, TRxCout = DPLL
		 */
		write_sio(lineno,R11,RCDPLL|TCDPLL|TRxCOI|TRxCDP);
		break;
	case 1:	/* Channel B: RTxC XTAL, receive clock = DPLL output,
		 * transmit clock = DPLL output, TRxC = output, TRxCout = XTAL
		 * (TRxCB is cross-connected to RxTCA)
		 */
		write_sio(lineno,R11,RTxCX|RCDPLL|TCDPLL|TRxCOI|TRxCXT);
		break;
	}
#endif
	/* Compute and load baud rate generator time constant
	 * DPLL needs x32 clock
	 * PCLK is defined in fad.h to be the processor clock (for OLDFAD)
	 * or the FAD XTAL clock / (2 * 32)
	 */
	tc = PCLK/(line->speed) - 2;
	write_sio(lineno,R12,tc & 0xff);
	write_sio(lineno,R13,(tc >> 8) & 0xff);

	/* Set NRZI mode */
	write_sio(lineno,R14,SNRZI);
	/* Set source = BR generator */
	write_sio(lineno,R14,SSBR);
	/* Enter search mode */
	write_sio(lineno,R14,SEARCH);
#ifdef	OLDFAD
	/* NULL, baud rate gen source = PCLK, enable baud rate gen */
	write_sio(lineno,R14,BRSRC|BRENABL);
#else
	/* NULL, baud rate gen source = RTxC, enable baud rate gen
	 * RTxC on channel B is the crystal oscillator; on channel A
	 * it is strapped to the TxRC output of B, which is programmed
	 * to give the crystal oscillator.
	 */
	write_sio(lineno,R14,BRENABL);
#endif
	/* Break/abort IE, TX EOM IE, CTS IE, no SYNC/HUNT IE, DCD IE,
	 * no Zero Count IE
	 */
	write_sio(lineno,R15,BRKIE|TxUIE|CTSIE|DCDIE);
	restore(i_state);
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
