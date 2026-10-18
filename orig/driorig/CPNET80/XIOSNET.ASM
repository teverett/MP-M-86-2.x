	title 'ULCNET Data Link Layer MP/M XIOS Module'
	page	54

;*****************************************************************************
;* This module must be linked into the server's XIOS.  It is designed to     *
;* run under MP/M for the Xerox 820, but should be easily customized.  It    *
;* contains the ULCnet interface modules DLIF and NIOD.  The DLIF is an      *
;* interface between the transport software contained in ULCIF.RSP and the   *
;* data-link software contained in IPBMAIN.REL.  The NIOD contains the actual*
;* hardware drivers required to run ULCnet.  The module IPBMAIN.REL must also*
;* be linked into the XIOS.                                                  *
;*****************************************************************************

;	This software is the result of a joint effort between
;
;		Digital Research, Inc.
;		P.O. Box 579
;		Pacific Grove, CA 93950
;	and
;		Keybrook Business Systems, Inc.
;		2035 National Avenue
;		Hayward, CA 94545	

; Conditional assembly control

true		equ	0ffffh
false		equ	not true

interrupts	equ	true		; false=polled, true=interrupt-driven
netstats	equ	true		; switch to gather network statistics
slfclkd		equ	true		; supports self-clocked operation

; Linkage information

	public	nconst,nconin,nconout	; XIOS console jump table entries
	public	polldevice		; XIOS polling routine
	public	setbaud,xmit,recv,initu	; NIOD routines called by IPBMAIN
	public	inituart,pgmuart
	public	chkstat,netidle,initrecv
	public	wait,restuart,csniod
	public	dsblxmit
	public	dllbau,netadr

	if	interrupts
	public	enblrecv,dsblrecv
	endif

	extrn	transmit,receive	; IPBMAIN routines and objects
	extrn	gettcode,getrcode
	extrn	csdll,dllon,regshrt
	extrn	terrcnt,parcntr,ovrcntr
	extrn	frmcntr,inccntr
	extrn	xdos,const,conin,conout	; linkage back to the rest of XIOS
	extrn	poldev

	if	interrupts
	extrn	rtmochk			; IPBMAIN interrupt routines
	extrn	dlisr,reisr,niisr
	endif


; Hardware definitions for the Z80-SIO channel A - For the Xerox 820.

baudsl	equ	03h		; Usable baud rates: 9600, 19.2K asynch.,
baudsh	equ	2ah		; 76.8K, 153.6K, 307.2K self-clocked

				; baud rate capability mask
bauds	equ	(baudsh*100h)+baudsl

baudgen	equ	0		; External baud rate generator register
siocmd	equ	6		; Command/Mode register
siostat	equ	6		; Status register
sioxmit	equ	4		; Transmit register
siorecv	equ	4		; Receive register

xrdybit	equ	2		; Transmit buffer empty status bit
xrdymsk	equ	4		; transmit buffer empty status mask
rrdybit	equ	0		; Receive buffer full status bit
rrdymsk	equ	1		; receive buffer full status mask
carbit	equ	3		; Net Idle detect bit position
carmsk	equ	8		; Net Idle detect mask
errst	equ	030h		; Error flag reset
errbits	equ	070h		; Error bit position mask
pbit	equ	4		; Parity error bit position
pmsk	equ	10h		; parity error mask
obit	equ	5		; Overrun error bit position
omsk	equ	20h		; overrun error mask
fbit	equ	6		; Framing error bit position
fmsk	equ	40h		; framing error mask
selfbit	equ	3		; Self clock bit position
selfmsk	equ	8		; slef clock bit mask
dtron	equ	0eah		; Turn on DTR
dtroff	equ	06ah		; Turn off DTR
enarcv	equ	0c1h		; Enable receive-clock
disrcv	equ	0c0h		; Disable receive clock
enaslf	equ	00fh		; Enable Self-clock mode
disslf	equ	04fh		; Disable Self-clock mode 

; SIO Mode 2 interrupts vector table

siov4	equ	0ff08h		; SIO port A xmit buffer empty
siov5	equ	0ff0ah		; SIO port A external status change
siov6	equ	0ff0ch		; SIO port A receive
siov7	equ	0ff0eh		; SIO port A special receive condition

netcon	equ	20h		; fake console number called by ULCIF for
				; network operations

; polling equates

ulctx	equ	20h		; transmission poll number
ulcrx	equ	21h		; receive poll number
	page


; ULCnet Data Definitions

netadr:	ds	3		;ULCnet network address
dllbau:	ds	2		;baud rate mask

timeval	equ	22		; WAIT routine time constant
				; 12 for 2.5 megahertz Z80
				; 22 for 4.0 megahertz Z80 	

dev$table:			;polling device table

	dw	twait		;receive poll wait
	dw	rwait		;transmit poll wait
num$devices	equ	($-dev$table)/2	

tcode:	ds	1			; Transmit Return code
rcode:	ds	1			; Receive Return code

curbaud db	0ffh			; Current baud rate

				
btbl:	db	1,2,4,8,16,32,64,128	; table to convert baud number codes
					;   into a bit mask

baudtbl:				; async baud rate table

	db	0eh			; 9600 Baud
	db	0fh			; 19200

scbaudt:				; self-clock baud rate table

	db	0			;  62500 Baud - Not implemented
	db	0dh			;  76800 Baud
	db	0			; 125000 Baud - Not implemented
	db	0eh			; 153600 Baud
	db	0			; 250000 Baud - Not implemented
	db	0fh			; 307200 Baud

	if	interrupts
sioiblk	db	030h,14h,4fh,15h,06ah,13h,0c1h,11h,01h,10h,10h,30h
	else
sioiblk	db	030h,14h,4fh,15h,06ah,13h,0c1h,11h,00h,10h,10h,30h
	endif

sioilen	equ	$-sioiblk

	page


; ULCnet data-link interface code


; POLLDEVICE:  Device polling routine.
;	Input:
;		C = device number to poll
;	Output:
;		A = 0    if not ready
;		    0ffh if ready

polldevice:			

	mov	a,c		; if not a network poll, go to the real
	sui	ulctx		;   routine
	jc	poldev

	cpi	num$devices	; check for poll number in bounds
	jc	devok

	mvi	a,0		; out-of-bounds-->don't do anything
	ret

devok:

	mov	l,a
	mvi	h,0
	dad	h		; multiply index by 2

	lxi	d,dev$table	; index into the poll routine table
	dad	d

	mov	e,m
	inx	h
	mov	d,m		; get the routine address

	xchg
	pchl			; dispatch



;
; NCONST: Console status entry point.  If register D = fake network
;	  console ID, do network initialization.  Otherwise, go back to
;	  the real console routines.

nconst:

	mvi	a,netcon	; Check if network call
	cmp	d
	jnz	const		; Jump to normal CONST if not network

	call	csdll		; Cold start the data link
	call	dllon		; Initialize the SIO Drivers
	xra	a		; Initialize all the short addresses

nxtadd:

	inr	a
	cpi	5		; Check for last address
	rz
	push	psw
	call	regshrt
	pop	psw
	jmp	nxtadd		; Jump to process next address


; NCONIN:  Console In entry point.  If register D = the fake network ID
;	   then receive a network message, using polled status checks of
;	   an interrupt-driven data-link.  Otherwise, go back to the real
;	   CONIN routine.

nconin:

	mvi	a,netcon	; Check for network call
	cmp	d
	jnz	conin		; Jump to normal CONIN if not network

	mov	d,b		; Setup for PSRECEIVE
	mov	e,c

rretry:

	xra	a		; Packet mode
	lxi	b,257		; Buffer size
	lxi	h,0		; Infinite wait	
	push	d		; Save buffer address for retry
	call	psrecv
	pop	d		; Restore buffer address
	ora	a
	rz			; Return if no error

	jmp	rretry		; Jump to try again if error 


; NCONOUT:  Console out entry point.  If D = fake console ID, send a network
;	    message.  Otherwise, just head for the real CONOUT routine.


nconout:

	mvi	a,netcon	; Check for network call
	cmp	d
	jnz	conout		; Jump to normal CONOUT if not network

	mov	d,b		; Setup for PSXMIT
	mov	e,c

tretry:

	xra	a		; Packet mode, wait for Net Idle
	push	d		; Save buffer address for retry
	call	psxmit
	pop	d		; Restore buffer address
	ora	a
	rz			; Return if no error

	jmp	tretry		; Jump to retry if error


; PSXMIT:  Transmit the packet pointed at by DE.  If carry flag is set
;	   then don't wait for the Net to become idle.
;
; Returns the completion code in A:
;
;	0	- Transmission ok and Data Link Ack Received
;		  (In the case of multicast, no Ack required)
;	2	- Transmission OK but no Data Link Ack received.
;
;	4	- Other error.

psxmit:

	call	transmit		; TRETCODE := TRANSMIT(TBUFPTR,<C>)

	mvi	c,83h			; Poll the transmitter for completion
 	mvi	e,ulctx
	call	xdos			

	lda	tcode			; Fetch return code
	jmp	exitdl
	
; TWAIT:  Transmission completion poll routine.
;
;	  Output:
;		A = 0    if not complete
;		    0ffh if complete

twait:

	call	gettcode		; A := GETTCODE - Xmit return code

	mov	e,a			; get return code processing vectore
	mvi	d,0
	lxi	h,trtbl
	dad	d

	mov	e,m			; dispatch on return code
	inx	h
	mov	h,m
	mov	l,e
	pchl

; Return code dispatch table

trtbl:	dw	psxret			; Good transmission
	dw	psxret			; No Data Link Ack
	dw	psxret			; Too many collisions
	dw	psxret			; Transmitter is disabled
	dw	tsleep			; Transmitter is idle
	dw	tsleep			; Transmitter is in progress
	dw	tsleep			; Transmitter is waiting for ack

tsleep:

	xra	a			; Code for continue to sleep
	ret

psxret:					; Enter here if something happened

	jnc	twakeup			; Jump if no transmit error
	cma				; Else-->Indicate error

twakeup:

	sta	tcode			; Store return code
	mvi	a,0ffh			; Signal poll successful
	ret



; PSRECV:  Receive a packet into buffer pointed at by DE.  Length of
; 	   packet must be less than length of buffer in BC. HL is the receive
; 	   timeout count. 
;
;	   Upon return clear the carry bit if a packet received and ACKed.
;   	   Set the carry flag if any error occured.
;


psrecv:

	call	receive			; <C> := RECEIVE(HL,DE,BC)

	mvi	c,83h			; Poll until receive complete
	mvi	e,ulcrx
	call	xdos			

	lda	rcode			; Fetch return code

; Common exit routine for returning to the pseudo-console handler

exitdl:
	
	ora	a			; Assume no error
	rp				; Return if no error

	cma
	stc				; Indicate error
	ret		

; RWAIT:  Poll routine to detect receive status.
;
;	Output:
;		A = 0    if receive not complete
;		    0ffh if receive complete

rwait:

	call	getrcode		; A := GETRCODE

	mov	e,a			; form dispatch vector
	mvi	d,0
	lxi	h,rrtbl
	dad	d

	mov	e,m			; dispatch on receive completion code
	inx	h
	mov	h,m
	mov	l,e
	pchl

; Receive completion code dispatch table

rrtbl:	dw	rgood			; Good receive
	dw	rbad			; Bad receive
	dw	rbad			; Disabled

    	if	not interrupts
	dw	rbad			; Still idle after timeout
	else
	dw	ridle			; Idle
        endif

	dw	rsleep			; Inprogress
	dw	rsleep			; In progress and for us.

rsleep:	

	xra	a			; Code for continue to sleep
	ret

rgood:
rwakeup:

	sta	rcode			; Store return code
	mvi	a,0ffh			; Wake up code
	ret

rbad:

	cma				; Code for error
	jmp	rwakeup			; Jump to wake up receive process

    	if interrupts

ridle:

	call	rtmochk			; Check for timeout
	jc	rbad			; if timeout, signal error
	jmp	rsleep			; Continue to wait if no timeout

	ret

      	endif
	page

; NIOD routines



; SETBAUD:  Set the baud rate based on the baud rate code in A.  Do special 
; 	    logic for self-clocked mode.
;
;	 	0 = 9600 baud
;		1 = 19200 baud
;		9 = 76800 baud self-clock
;		11= 153600 baud self-clock
;		13= 307200 baud self-clock
; 
; If this station cannot handle the requested baud rate, then set
; the carry flag.

setbaud:

	ani	0fh		; mask all but the baud bits
	lxi	h,curbaud	; are we at the current baud rate?
	cmp	m
	rz			; yes-->all done

	mov	b,a		; else-->get baud rate generator value
	ani	7
	mov	e,a
	mvi	d,0

	lxi	h,btbl		; point to vertical-to-horizontal decode
	dad	d		;   table

	if	slfclkd
	mov	a,b
	ani	selfmsk		; is this a self-clocked value?
	jnz	selfclkd
	endif

	mvi	a,baudsl	; get legal baud rate mask
	ana	m
	stc
	rz			; return with error if its an illegal rate

	if	slfclkd
	mvi	a,5		; else-->switch off possible self-clock mode
	out	siocmd
	mvi	a,dtroff	; disable DTR in SIO register 5
	out	siocmd

	mvi	a,4		; disable sync mode in register 4
	out	siocmd
	mvi	a,disslf
	out	siocmd
	endif

	lxi	h,baudtbl	; point to async baud rate table

outbau:

	dad	d		; get async baud rate value
	mov	a,m
	out	baudgen		; load it into the baud rate generator
				; NOTE: This is not a CTC

	lxi	h,curbaud
	mov	m,b		; set current baud byte

	call	wait		; allow the system to reach equilibrium

	ana	a		; return success
	ret

	if	slfclkd
; Throw SIO into self-clocked mode

selfclkd:

	mvi	a,baudsh	; Is this a legal rate?
	ana	m
	stc
	rz			; return an error if not

	mvi	a,4		; enable sync mode in register 4
	out	siocmd
	mvi	a,enaslf
	out	siocmd

	mvi	a,5		; enable DTR in register 5
	out	siocmd
	mvi	a,dtron
	out	siocmd

	lxi	h,scbaudt	; point to baud rate table for self-clock mode
	jmp	outbau		; program the baud rate generator
	endif


; DSBLXMIT:  Disable the transmitter if in self clocked mode

dsblxmit:

	if	slfclkd
	lda	curbaud		; are we in self-clocked mode?
	ani	selfmsk
	rz			; no-->don't bother

	mvi	a,5		; disable SIO from transmitting by disabling 
	out	siocmd		; DTR in register 5
	mvi	a,dtroff
	out	siocmd

	mvi	a,5		; Enable receive by re-enabling DTR
	out	siocmd
	mvi	a,dtron
	out	siocmd
	endif

	ret


; XMIT:  Transmit the byte in A on network A.


xmit:

	if	not interrupts
	push	psw

xmit1:

	in	siostat		; don't overrun the transmitter if we're
	ani	xrdymsk		;  interrupt-driven; wait for TxReady
	jz	xmit1

	pop	psw
	endif

	out	sioxmit		; blast that byte
	ret


; RECV:  Receive a byte from Network A. Set the carry flag if there was
; 	 a receive error.
;
;	 For Z80-SIO receive errors are handled by the special receive
; 	 condition interrupts.

recv:

	if	not interrupts
	call	netidle	
	jc	rto		; set error condition if the net went idle

	in	siostat		; else-->wait until a character is in the
	ani	rrdymsk		;    buffer
	jz	recv

	call	chkstat		; check for receive errors

	else
	ana	a		; clear carry flag
	endif

	in	siorecv		; input the character
	ret

rto:				; set an error

	xra	a
	stc
	ret
	

; CHKSTAT:  Check error status bits of a receive error.  If not error then
; 	    clear the carry flag and return.  Otherwise figure out which
; 	    error occured and increment its counter and set the carry flag.
; 	    Issue an error reset command to the UART.


chkstat:

	mvi	a,1		; get error status from SIO read register 1
	out	siocmd
	in	siostat

	ani	errbits
	rz			; no error occurred-->all done

	if	netstats	; gather statistics on the type of error
	mov	b,a
	ani	pmsk
	jz	np		; not a parity error

	lxi	h,parcntr	; else-->
	call	inccntr		; increment parity error counter

np:

	mov	a,b
	ani	obit
	jz	no		; not an overrun

	lxi	h,ovrcntr	; else-->
	call	inccntr		; increment overrun counter

no:

	mov	a,b
	ani	fbit
	jz	nf		; not a framing error

	lxi	h,frmcntr	; else-->
	call	inccntr		; increment framing error counter

nf:
	endif

	mvi	a,errst		; reset error condition
	out	siocmd
	stc			; signal an error
	ret
		


; NETIDLE:  See if network A is idle. If idle then set the carry flag.

netidle:

	mvi	a,10h		; reset interrupts
	out	siocmd
	out	siocmd		; do it twice to reject glitches on DCD

	in	siostat		; is there a data-carrier detect?
	ani	carmsk
	rz			; yes-->net is in use-->carry flag cleared

	xra	a
	call	setbaud		; net is idle-->reset to hailing rate (9600)
	stc			; set net idle to true
	ret


	if	interrupts

; ENBLRECV:  Enable the channel A receiver interrupts.

enblrecv:

	mvi	a,1		; enable interrupts on all characters
	out	siocmd
	mvi	a,011h		; NOTE: This mask would have to be 015h on
	out	siocmd		;  channel B
	ret

; DSBLRECV:  Disable the channel A receiver interrupts.

dsblrecv:

	mvi	a,1		; Disable interrupts on received characters
	out	siocmd		;   (Keep status interrupts enabled)
	out	siocmd		; NOTE:  Channel B mask is 05h
	ret

	endif


; PGMUART:  Program the Network UART channel

pgmuart:

	if	interrupts
				; The 820 already has the SIO vector address
				; programmed from channel B.  Other 
				; implementations will have to provide linkage
				; to the vector area in the main XIOS, and 
				; load the vector offset into SIO write 
				; register 2

	lxi	h,niisr		; load status interrupt service routine vector
	shld	siov5
	lxi	h,dlisr		; load transmit ISR vector
	shld	siov6
	lxi	h,reisr		; load receiv ISR vector
	shld	siov7
	endif

	lxi	h,sioiblk	; point to SIO initialization block
	mvi	b,sioilen	; length of block
	di

pgm1:

	mov	a,m		; output the block to the SIO
	out	siocmd
	inx	h
	dcr	b
	jnz	pgm1

	ei
	xra	a		; set up hailing baud rate = 9600
	call	setbaud
	ret


; INITUART:  Initialize the uart for network A by issuing a reset command
; 	     and clearing out the receive buffer.

inituart:

	mvi	a,3		; disable the receiver through register 3
	out	siocmd
	mvi	a,disrcv
	out	siocmd

	in	siostat		; is there a garbage byte?
	ani	rrdymsk
	jz	initu		; no-->continue initialization

	in	siorecv		; else-->eat the character
	jmp	inituart	; try again

initu:

	mvi	a,errst		; reset error conditions
	out	siocmd

	mvi	a,3		; re-enable the receiver
	out	siocmd
	mvi	a,enarcv
	out	siocmd

	ret

; INITRECV:  Initialize a receive operation

initrecv:

	call	inituart

	if	interrupts
	call	enblrecv	; enable receiver interrupts
	endif

	ret


; WAIT - Wait 100 micro seconds

wait:

	mvi	a,timeval

w:

	dcr	a		; 04
	ana	a		; 04
	jnz	w		; 12
				; ---
	ret			; 30 T-States total


; RESTUART:  Reinitialize the UART to the way it was in the
;	     original BIOS after completing the network operations


restuart:
	ret			; UART not used except by network


; CSNIOD:  Do any cold start initialization which is necessary.
;	   Must at least return the value of BAUDS
; 	   If the network uses the printer port then set theh carry flag
;	   otherwise clear it.

csniod: 
	
	lxi	b,bauds		; return the legal baud rates
	ora	a		; not using a printer port
	ret

	end
