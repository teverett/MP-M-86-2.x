; Interrupt vectors for Xerox 820

; WARNING WARNING DANGER Will Robinson!
; These routines do not save either the long or floating point
; accumulators. You will need to add code to this if either
; longs or floats are ever used in the interrupt level code

; size of interrupt stack in bytes
stksize		equ	100

; SIO I/O port assignments and flag bits
; must be the same as those in 820.c
PORT_A		equ	4
PORT_B		equ	5
CTL		equ	2
Rx_CH_AV	equ	1
Tx_BUF_EMP	equ	4
RES_Tx_P	equ	28h
KBDAT		equ	1eh
KBSTAT		equ	1ch
KBMASK		equ	8

; Interrupt register saving sequence
; Adapted from page 2-81, Zilog Microprocessor Applications Reference book
; A separate interrupt stack is needed because the CP/M BDOS has a very
; tiny one.

	public iff_
	dseg
	bss	spsave,2	; saved regular SP
	bss	tmpstk,stksize	; should be enough
sptmp	equ	tmpstk+stksize	; top of tmpstk
	cseg

save:	xthl			; save hl, hl -> start of int handler
	; ld	(spsave),sp	; save original sp
	db	0edh,73h
	dw	spsave
	lxi	sp,sptmp	; switch to temporary stack
	push	d		; save de
	push	b		;      bc
	push	psw		;      af
	; push ix
	db	0ddh,0e5h
	; push iy
	db	0fdh,0e5h
	xra	a
	sta	iff_
	call	go		; save pc
	mvi	a,1
	sta	iff_
	; pop iy
	db	0fdh,0e1h
	; pop ix
	db	0ddh,0e1h
	pop	psw		; restore af
	pop	b		;         bc
	pop	d		;         de
	; ld	sp,(spsave)	; restore original stack
	db	0edh,7bh
	dw	spsave
	pop	h		;         hl
	ei
	; reti
	db 0edh,4dh

go:	pchl


; CTC channel 0 interrupt handler
; Simply increments the C external variable "_clktick" on every tick
; Also calls the 820 monitor disk timer, whose vector we've stolen
	public	_ctcint_
	public	_clktick_
	dseg
	bss	_clktick_,1
	cseg
dsktmr	equ	0f3a2h
_ctcint_:
	call	save
	lxi	h,_clktick_
	inr	m
	jmp	dsktmr	; it returns through the restore routine

; definitions for pseudo-dma control
; appears to C as
; struct dma {
;	char *data;
;	unsigned short cnt;
;	char flags;
; } dma[2];	/* Both channels must be defined */

	public sdma_
data0	equ	sdma_+0
cnt0	equ	sdma_+2
flags0	equ	sdma_+4
; SIO transmit interrupt handler #0
	public	_stx0int_
	public	stxint_

_stx0int_:
	call	save
;check for false alarm; return if transmitter not ready
	in	PORT_A+CTL
	ani	Tx_BUF_EMP
	rz
;check if dma is active; return otherwise
	lda	flags0
	ora	a
	rz

	lhld	cnt0		; fetch count remaining
	mov	a,l
	ora	h
	jnz	more0
	mvi	a,RES_Tx_P	; clear device interrupt flag
	out	PORT_A+CTL
	xra	a
	sta	flags0		; clear busy flag
	lxi	h,0
	push	h
	call	stxint_		; notify user
	pop	h
	ret

more0:	dcx	h
	shld	cnt0
	lhld	data0		; points to next byte
	mov	a,m
	out	PORT_A
	inx	h
	shld	data0
	ret

; SIO transmit interrupt handler #1
	public	_stx1int_
	public	stxint_
data1	equ	sdma_+5
cnt1	equ	sdma_+7
flags1	equ	sdma_+9

_stx1int_:
	call	save
;check for false alarm; return if transmitter not ready
	in	PORT_B+CTL
	ani	Tx_BUF_EMP
	rz
;check if dma is active; return otherwise
	lda	flags1
	ora	a
	rz

	lhld	cnt1	; fetch count remaining
	mov	a,l
	ora	h
	jnz	more1
	mvi	a,RES_Tx_P	; clear device interrupt flag
	out	PORT_B+CTL
	xra	a
	sta	flags1	; clear busy flag
	lxi	h,1
	push	h
	call	stxint_
	pop	h
	ret

more1:	dcx	h
	shld	cnt1
	lhld	data1		; points to next byte
	mov	a,m
	out	PORT_B
	inx	h
	shld	data1
	ret

; receiver silo control pointers
; Appears to C as
; struct fifo {
;	char buf[256];	/* Ring buffer */
;	char *wp;	/* Write pointer */
;	char *rp;	/* Read pointer */
;	unsigned short cnt;	/* count of chars in buffer */
;};

; keyboard interrupt handler
	public cfifo_
cbuf	equ	cfifo_+0
cwp	equ	cfifo_+256
crp	equ	cfifo_+258
ccnt	equ	cfifo_+260
	public	_crxint_

_crxint_:
	call	save
cloop:
	in	KBSTAT		; check for char avail
	cma
	ani	KBMASK
	rz			; no more
	lhld	ccnt		; load and increment count
	inx	h
	shld	ccnt
	lhld	cwp		; get write pointer
	in	KBDAT		; read input byte
	cma
	ani	7fh		; strip parity
	mov	m,a		; store it
	inx	h		; bump write pointer
	shld	cwp		; save
	lxi	d,-cbuf		; check for wraparound
	dad	d
	mov	a,h		; buffer is 256 bytes long
	ora	a
	jz	cloop
	lxi	h,cbuf		; restart at beginning
	shld	cwp
	jmp	cloop

	public sfifo_
rbuf0	equ	sfifo_+0
rwp0	equ	sfifo_+256
rrp0	equ	sfifo_+258
rcnt0	equ	sfifo_+260

; SIO receiver interrupt handler #0
	public	_srx0int_
_srx0int_:
	call	save
r0loop:
	in	PORT_A+CTL	; check for char avail
	ani	Rx_CH_AV
	rz
	lhld	rcnt0		; load and increment count
	inx	h
	shld	rcnt0
	lhld	rwp0		; get write pointer
	in	PORT_A		; read input byte
	mov	m,a		; store it
	inx	h		; bump write pointer
	shld	rwp0		; save
	lxi	d,-rbuf0	; check for wraparound
	dad	d
	mov	a,h		; buffer is 256 bytes long
	ora	a
	jz	r0loop
	lxi	h,rbuf0		; restart at beginning
	shld	rwp0
	jmp	r0loop

; SIO receiver interrupt handler #1
	public	_srx1int_
rbuf1	equ	sfifo_+262
rwp1	equ	sfifo_+518
rrp1	equ	sfifo_+520
rcnt1	equ	sfifo_+522

_srx1int_:
	call	save
r1loop:
	in	PORT_B+CTL	; check for char avail
	ani	Rx_CH_AV
	rz			; no more
	lhld	rcnt1		; load and increment count
	inx	h
	shld	rcnt1
	lhld	rwp1		; get write pointer
	in	PORT_B		; read input byte
	mov	m,a		; store it
	inx	h		; bump write pointer
	shld	rwp1		; save
	lxi	d,-rbuf1	; check for wraparound
	dad	d
	mov	a,h		; buffer is 256 bytes long
	ora	a
	jz	r1loop
	lxi	h,rbuf1		; restart at beginning
	shld	rwp1
	jmp	r1loop

; external/Status interrupt handler #0
	public	_sex0int_
	public	sexint_
_sex0int_:
	call	save
	lxi	h,0		; line number 0
	push	h
	call	sexint_
	pop	h
	ret

; external/Status interrupt handler #1
	public	_sex1int_
	public	sexint_
_sex1int_:
	call	save
	lxi	h,1		; line number 1
	push	h
	call	sexint_
	pop	h
	ret

; Special status interrupt handler #0
	public	_ssp0int_
	public	sspint_
_ssp0int_:
	call	save
	lxi	h,0		; line number 0
	push	h
	call	sspint_
	pop	h
	ret

; Special status interrupt handler #1
	public	_ssp1int_
	public	sspint_
_ssp1int_:
	call	save
	lxi	h,1		; line number 1
	push	h
	call	sspint_
	pop	h
	ret

	public	_noint_
	public	noint_
_noint_:
	call	save
	call	noint_
	ret

	public putchar_
crtout equ	0f00ch		; 820 monitor display routine
putchar_:
	lxi	h,2
	dad	sp	; hl -> character
	mov	a,m
	jmp	crtout

; Byte swapping routines
	public	ntohs_
; network-to-host short -- 0 1 becomes 1 0
ntohs_:
	lxi	h,2
	dad	sp	; hl -> arg
	mov	d,m	; low byte -> d
	inx	h
	mov	e,m	; high byte -> e
	xchg		; result -> hl
	mov	a,l	; set flags
	ora	h
	ret
	
; Network-to-host long -- 0 1 2 3 becomes 3 2 1 0
	public	ntohl_
	extrn	lnprm
ntohl_:
	lxi	h,5
	dad	sp	; hl -> last byte of arg
	lxi	d,lnprm	; de -> primary long accumulator (ret val)
	mov	a,m
	stax	d
	dcx	h
	inx	d
	mov	a,m
	stax	d
	dcx	h
	inx	d
	mov	a,m
	stax	d
	dcx	h
	inx	d
	mov	a,m
	stax	d
	ret

; Word and byte extraction - done here for speed; also available as
; C macros in machdep.h for less archaic machines than the Z-80

; extract low nibble of a byte
	public lonibble_
lonibble_:
	lxi	h,2
	dad	sp	; hl -> byte
	mov	a,m	; a = byte
	ani	0fh	; strip to low nibble, set flags
	mov	l,a
	mvi	h,0
	ret

; extract high nibble of a byte
	public hinibble_
hinibble_:
	lxi	h,2
	dad	sp	; hl -> byte
	mov	a,m	; a = byte
	rrc
	rrc
	rrc
	rrc		; shift high nibble to low end
	ani	0fh	; strip, set flags
	mov	l,a
	mvi	h,0
	ret

; extract low byte of a 16-bit word
	public lobyte_
lobyte_:
	lxi	h,2
	dad	sp	; hl -> low byte
	mov	l,m
	mvi	h,0	; hl = low byte
	mov	a,l
	ora	h	; set flags 
	ret

; Get high byte of 16-bit word
	public	hibyte_
hibyte_:
	lxi	h,3
	dad	sp	; hl -> high byte
	mov	l,m
	mvi	h,0	; hl = low byte
	mov	a,l
	ora	h	; set flags
	ret

; extract low-order 16 bits from 32-bit long
	public	loword_
loword_:
	lxi	h,2
	dad	sp	; hl -> low byte of low word
	mov	e,m
	inx	h
	mov	d,m	; de = loword
	xchg		; hl = loword
	mov	a,l
	ora	h	; set flags
	ret

; extract high-order 16 bits from 32-bit long
	public	hiword_
hiword_:
	lxi	h,4
	dad	sp	; hl -> low byte of high word
	mov	e,m
	inx	h
	mov	d,m	; de = loword
	xchg		; hl = loword
	mov	a,l
	ora	h	; set flags
	ret

	end
