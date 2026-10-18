/* fad.h */
/* Copyright 1985 by Philip Karn, KA9Q
 * Permission granted for noncommercial copying
 * and use provided this notice is retained
 */

/* Hardware-dependent routines for the Xerox 820 with a FAD (8530)
 * add-on */

/* Interrupt vector handlers (assembler) */
int _ctcint(),_ltx0int(),_ltx1int(),
	_lrx0int(),_lrx1int(),
	_stxint(),_srxint(),
	_lsp0int(),_lsp1int(),
	_lex0int(),_lex1int(),
	_noint();

/* Assembler primitives */
char in(),out(),iff(),ireg(),im2(),ei(),di(),halt();

/* Other utility routines */
char read_sio();

/* 8530 clock speed */
#ifdef	OLDFAD	/* Uses 2.5 Mhz Xerox 820 clock */
#define	PCLK	((unsigned)39063)	/* 2.5 MHz / (2*32) */
#else		/* Uses FAD board crystal */
#define	PCLK	((unsigned)38400)	/* 2.4576 Mhz / (2*32) */
#endif

/* FAD (8530) ports */
#define	PORT_A	0x0a
#define	PORT_B	0x08
#define	CTL	0
#define	DATA	1

/* Xerox 820 Port definitions */
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

/* 8530 FAD board interrupt vectors - at very end */
#define	SCC_BTBE	0xffe0
#define	SCC_BEXT	0xffe2
#define	SCC_BRCA	0xffe4
#define	SCC_BSRC	0xffe6
#define	SCC_ATBE	0xffe8
#define	SCC_AEXT	0xffea
#define	SCC_ARCA	0xffec
#define	SCC_ASRC	0xffee

#define	FLAG	0x7e	/* SDLC/HDLC flag (01111110) */

/* 8530 Register definitions */

/* Write Register 0 */
#define	R0	0		/* Register selects */
#define	R1	1
#define	R2	2
#define	R3	3
#define	R4	4
#define	R5	5
#define	R6	6
#define	R7	7
#define	R8	8
#define	R9	9
#define	R10	10
#define	R11	11
#define	R12	12
#define	R13	13
#define	R14	14
#define	R15	15

#define	NULLCODE	0	/* Null Code */
#define	POINT_HIGH	0x8	/* Select upper half of registers */
#define	RES_EXT_INT	0x10	/* Reset Ext. Status Interrupts */
#define	SEND_ABORT	0x18	/* HDLC Abort */
#define	RES_RxINT_FC	0x20	/* Reset RxINT on First Character */
#define	RES_Tx_P	0x28	/* Reset TxINT Pending */
#define	ERR_RES		0x30	/* Error Reset */
#define	RET_INT		0x38	/* Reset highest IUS */

#define	RES_Rx_CRC	0x40	/* Reset Rx CRC Checker */
#define	RES_Tx_CRC	0x80	/* Reset Tx CRC Checker */
#define	RES_EOM_L	0xC0	/* Reset EOM latch */

/* Write Register 1 */

#define	EXT_INT_ENAB	0x1	/* Ext Int Enable */
#define	TxINT_ENAB	0x2	/* Tx Int Enable */
#define	PAR_SPEC	0x4	/* Parity is special condition */

#define	RxINT_DISAB	0	/* Rx Int Disable */
#define	RxINT_FCERR	0x8	/* Rx Int on First Character Only or Error */
#define	INT_ALL_Rx	0x10	/* Int on all Rx Characters or error */
#define	INT_ERR_Rx	0x18	/* Int on error only */

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

/* Write Register 8 (transmit buffer) */

/* Write Register 9 (Master interrupt control) */
#define	VIS	1	/* Vector Includes Status */
#define	NV	2	/* No Vector */
#define	DLC	4	/* Disable Lower Chain */
#define	MIE	8	/* Master Interrupt Enable */
#define	STATHI	0x10	/* Status high */
#define	NORESET	0	/* No reset on write to R9 */
#define	CHRB	0x40	/* Reset channel B */
#define	CHRA	0x80	/* Reset channel A */
#define	FHWRES	0xc0	/* Force hardware reset */

/* Write Register 10 (misc control bits) */
#define	BIT6	1	/* 6 bit/8bit sync */
#define	LOOPMODE 2	/* SDLC Loop mode */
#define	ABUNDER	4	/* Abort/flag on SDLC xmit underrun */
#define	MARKIDLE 8	/* Mark/flag on idle */
#define	GAOP	0x10	/* Go active on poll */
#define	NRZ	0	/* NRZ mode */
#define	NRZI	0x20	/* NRZI mode */
#define	FM1	0x40	/* FM1 (transition = 1) */
#define	FM0	0x60	/* FM0 (transition = 0) */
#define	CRCPS	0x80	/* CRC Preset I/O */

/* Write Register 11 (Clock Mode control) */
#define	TRxCXT	0	/* TRxC = Xtal output */
#define	TRxCTC	1	/* TRxC = Transmit clock */
#define	TRxCBR	2	/* TRxC = BR Generator Output */
#define	TRxCDP	3	/* TRxC = DPLL output */
#define	TRxCOI	4	/* TRxC O/I */
#define	TCRTxCP	0	/* Transmit clock = RTxC pin */
#define	TCTRxCP	8	/* Transmit clock = TRxC pin */
#define	TCBR	0x10	/* Transmit clock = BR Generator output */
#define	TCDPLL	0x18	/* Transmit clock = DPLL output */
#define	RCRTxCP	0	/* Receive clock = RTxC pin */
#define	RCTRxCP	0x20	/* Receive clock = TRxC pin */
#define	RCBR	0x40	/* Receive clock = BR Generator output */
#define	RCDPLL	0x60	/* Receive clock = DPLL output */
#define	RTxCX	0x80	/* RTxC Xtal/No Xtal */

/* Write Register 12 (lower byte of baud rate generator time constant) */

/* Write Register 13 (upper byte of baud rate generator time constant) */

/* Write Register 14 (Misc control bits) */
#define	BRENABL	1	/* Baud rate generator enable */
#define	BRSRC	2	/* Baud rate generator source */
#define	DTRREQ	4	/* DTR/Request function */
#define	AUTOECHO 8	/* Auto Echo */
#define	LOOPBAK	0x10	/* Local loopback */
#define	SEARCH	0x20	/* Enter search mode */
#define	RMC	0x40	/* Reset missing clock */
#define	DISDPLL	0x60	/* Disable DPLL */
#define	SSBR	0x80	/* Set source = BR generator */
#define	SSRTxC	0xa0	/* Set source = RTxC *
#define	SFMM	0xc0	/* Set FM mode */
#define	SNRZI	0xe0	/* Set NRZI mode */

/* Write Register 15 (external/status interrupt control) */
#define	ZCIE	2	/* Zero count IE */
#define	DCDIE	8	/* DCD IE */
#define	SYNCIE	0x10	/* Sync/hunt IE */
#define	CTSIE	0x20	/* CTS IE */
#define	TxUIE	0x40	/* Tx Underrun/EOM IE */
#define	BRKIE	0x80	/* Break/Abort IE */


/* Read Register 0 */
#define	Rx_CH_AV	0x1	/* Rx Character Available */
#define	ZCOUNT		0x2	/* Zero count */
#define	Tx_BUF_EMP	0x4	/* Tx Buffer empty */
#define	DCD		0x8	/* DCD */
#define	SYNC_HUNT	0x10	/* Sync/hunt */
#define	CTS		0x20	/* CTS */
#define	TxEOM		0x40	/* Tx underrun */
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

/* Read Register 3 (interrupt pending register) ch a only */
#define	CHBEXT	0x1		/* Channel B Ext/Stat IP */
#define	CHBTxIP	0x2		/* Channel B Tx IP */
#define	CHBRxIP	0x4		/* Channel B Rx IP */
#define	CHAEXT	0x8		/* Channel A Ext/Stat IP */
#define	CHATxIP	0x10		/* Channel A Tx IP */
#define	CHARxIP	0x20		/* Channel A Rx IP */

/* Read Register 8 (receive data register) */

/* Read Register 10  (misc status bits) */
#define	ONLOOP	2		/* On loop */
#define	LOOPSEND 0x10		/* Loop sending */
#define	CLK2MIS	0x40		/* Two clocks missing */
#define	CLK1MIS	0x80		/* One clock missing */

/* Read Register 12 (lower byte of baud rate generator constant) */

/* Read Register 13 (upper byte of baud rate generator constant) */

/* Read Register 15 (value of WR 15) */

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
#define	ltxgo2(l)	(out(sioctl[l],RES_EOM_L))
#define	ltxabort(l)	(out(sioctl[l],SEND_ABORT))
#define	lrxabort(l)	(write_sio(l,R3,ENT_HM|RxENABLE|RxCRC_ENAB|Rx8))
#define	ltxrdy(l)	(in(sioctl[l]) & Tx_BUF_EMP)
#define	lrxrdy(l)	(in(sioctl[l]) & Rx_CH_AV)
#define	ltx(l,c)	(out(siodata[l],c))
#define	ltxdone(l)	(out(sioctl[l],RES_Tx_P))
#define	lrx(l)		(in(siodata[l]))
#define	stx(c)		putchar(c)
#define	stxrdy()	(1)
#define	srx()		(~in(KBDAT) & 0x7f)
#define	srxrdy()	(~in(KBSTAT) & KBMASK)
