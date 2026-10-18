  title 'Sample Server Network I/F for CORVUS OMNINET 20-Oct-82'
	page 54

;________________________________________________________________________
;________________________________________________________________________
;									;
;	SAMPLE MASTER NETWORK IO SYSTEM FOR CP/NET 1.2			;
;	VERSION FOR CORVUS OMNINET "ENGINEERING" TRANSPORTER		;
;		(Requires RMAC for assembly)				;
;									;
;	COPYRIGHT (C) 1982 by	VANO ASSOCIATES, INC.			;
;				P.O. BOX 12730				;
;				New Brighton, MN  55112			;
;				U.S.A.					;
;				(612) 631-1245				;
;				ALL RIGHTS RESERVED			;
;									;
;	ANY USE OF THIS CODE without the imbedded copyright notice	;
;	is hereby strictly prohibited.					;
;									;
;	Permission is hereby granted to Digital Research Inc. to use	;
;	this source file for educational and illustrative purposes in 	;
;	conjunction with CP/Net 80 documentation.  Any other use of 	;
;	this code without the EXPRESS WRITTEN PERMISSION of VANO	;
;	ASSOCIATES INC. is hereby strictly prohibited.			;
;									;
;	This file is provided courtesy of:				;
;									;
;		R2E (Realisations Etude Electroniques)			;
;		Z.A.I. de Courtaboeuf					;
;		BP 73	91942 Les Ulis					;
;		FRANCE							;
;									;
;	who sponsored the development of one of its ancestors.		;
;									;
;	Note that this version requires that the CP/NET SLAVESP		;
;	process be properly patched to send all output traffic		;
;	to output queue 0.  For the current (1.2) beta release, the	;
;	following patch is enough:					;
;									;
;	Make this change in unrelocated server.rsp module.		;
;		-a543							;
;		0543  mvi a,30						;
;		0545  jmp 34f						;
;	Then resave the module and its bit map.				;
;									;
;________________________________________________________________________
;________________________________________________________________________

YES	equ	0ffffh
NO	equ	not YES

;	assembly mode switches
DEBUG		equ	NO		;assemble for debugging with rdt
RSP		equ	YES		;assemble as a resident process
INTERRUPT 	equ	NO		;transporter can interrupt (advisable)

;	Logical Configuration constants
NSLAVES		equ	2		;maximum number of slaves supported
SRVR$STK$SIZ	equ	150		;stack size  needed by SLVSPs
SRVR$PD$SIZ	equ	52		;PD size for SLVSPs
BUFFSIZE	equ	280		;maximum message buffer size
NMSG$BUFFS	equ	1+NSLAVES	;number of message buffers allocated
RX$PRIORITY	equ	64		;receive process priority
TX$PRIORITY	equ	63		;usually higher than rx

;	Physical configuration constants (FOR OUR INSTALLATION)
OMNI$BASE	equ	0F8h		;transporter base address
OMNI$SOCKET	equ	0a0h		;omninet transporter socket code (2)
OMNI$FLAG	equ	8		;XDOS flag for int. driven transporter
RST$NUM		equ	7		;interrupt level if interrupt driven
  INT$VCTR 	equ	RST$NUM * 8

;	transporter IO PORT constants for CORVUS "ENGINEERING" transporter
OMNI$DATA	equ	OMNI$BASE	;TCB pointer data port
OMNI$STAT	equ	OMNI$BASE + 1	;status port
OMNI$RDY	equ	10h		;ready bit (=1) in OMNI$STAT
;  the rest are not part of standard CORVUS "ENGINEERING" transporter
OMNI$ACK	equ	OMNI$BASE + 2	;int ack port (any data write)
OMNI$MASK	equ	OMNI$BASE + 3	;int mask port (b0, 1= enbl)
OMNI$PENDING	equ	1		;int pending (=1) in "   "
OMNI$ENABLE	equ	1		;int enable mask command
OMNI$DISABLE	equ	0		;int disable mask command

;	BDOS and XDOS Equates
PRINTF		equ	9		;message to console
FLAGWAITF	equ	132		;flag wait
FLAGSETF	equ	133		;flag set
MAKEQ		equ	134		;make queue
READQ		equ	137		;read queue
WRITEQ		equ	139		;write queue
DELAY		equ	141		;delay
DSPTCH		equ	142		;dispatch
CREATEP		equ	144		;create process
SET$PRIORITY	equ	145		;set caller's priority
DETACH		equ	147		;detach console
SYDATAD		equ	154		;get system data page address

;	MISC useful constants
CR		equ	0dh		;carriage return
LF		equ	0ah		;line feed


codeseg:
  if not RSP
;	.PRL Initialization entry point for whole module
	lxi	sp,ServerxSTKTOP	;switch to rx process stack
	mvi	c,SET$PRIORITY
	mvi	e,RX$PRIORITY
	call	bdos
    if not DEBUG
	  mvi	c,DETACH
	  call	bdos			;detach console
    endif ; DEBUG
	ret

bdosadr:
	dw	codeseg - 100h + 5	;bdos entry pointer
  else ; not RSP
;	in an rsp, this is filled in by GENSYS and the rx process is created
;	automatically
bdosadr:
	dw	0000h
  endif ; not RSP

page
;________________________________________________________________________
;________________________________________________________________________
;									;
;	This is the network receiver server process module		;
;									;
;	The receive server obtains a buffer from FreeBuff and gives it	;
;	to the transporter hardware for receive use.  It then waits	;
;	for a message completion by calling the wf$rx$done routine	;
;	Once a return from that routine occurs, the receiver server 	;
;	checks the slave number and sends a pointer to that message	;
;	buffer to the SLVSP support process corresponding to that	;
;	slave's server. Once the message pointer has been passed, the	;
;	process loops back for the next message and continues in this	;
;	fashion forever.						;
;									;
;	At present, receive errors are considered to be the Slave's	;
;	problem since normal error recovery is allegedly handled by the ;
;	transporter firmware.  Only error free messages are passed on, 	;
;	the rest are ignored unless the error is the absence of a free	;
;	support process in which case a "NOT LOGGED IN" error is sent	;
;	by the receiver process to the offending slave.			;
;									;
;	In order to prevent clobbering the transporter when it is busy	;
;	transmitting, the receiver must be synchronized with the	;
;	transmit server.  In this implementation, this is handled by	;
;	an MX Queue.							;
;									;
;________________________________________________________________________
;________________________________________________________________________

;	receiver server process descriptor (position dependent if RSP)
ServerxPD:
	dw	0			;link
	db	0,RX$PRIORITY		;status,priority
	dw	$ + 94			;stack pointer
	db	'ServeRX '		;name
	db	0,0ffh			;console, memseg
	ds	82			;reserved for MP/M use and stack
ServerxSTKTOP:
	dw	InitRX			;startup PC for process

;	User queue control block array used by this module for message queues. 
;	 Each element is 3 words long and is one UQCB followed by its message.
UQCBLEN	equ	6			;constant used to index array
XQCBMSG	equ	4			;subindex for  message word

INUQCB:					;array name
??xx	set	0
	rept	NSLAVES
	  dw	(inqcb$array + ??xx)	;;Q pointer, msg addr, message word
	  dw	$+2
	  dw	0
??xx	  set	??xx + INQCB$SIZE
	endm

;	UQCB used by ServeRX to get free buffers from Q
gbuf$uqcb:	dw	buffQCB,newbuff
newbuff:	dw	0		;message is a free buffer ptr from pool

;	UQCB used by ServeRX to get transporter from MX Q
omnirx$uqcb:	dw	omniQ,rx$mx$msg
rx$mx$msg:	dw	0

;	UQCB used by ServeRX to send error messages to outQ
err$out$uqcb:	dw	outQCB,err$out$msg ;pointer, msgadr
err$out$msg:	ds	2		   ;used to send error messages

;	receiver transporter control block
rxtcb:	db	0f0h			;post read command
	db	0			;result hi (always 0)
rxrsltp:
	db	0,0			;result middle and low (NOT 8080 order)
	db	OMNI$SOCKET		;transporter message socket code
	db	0			;data pointer high (always 0)
	db	0,0			;data pointer middle, low
	db	BUFFSIZE/256		;data max length hi
	db	BUFFSIZE and 255	;data max length lo
	db	0,0			;ctrl lgth (0 for now), host (not used)

rxrslt:	db	0,0,0,0,0,0,0,0		;result block for rx

;________________________________________________________________________
;									;
;	Receiver server process initialization entry point		;
;		(initializes all of module)				;
;________________________________________________________________________
InitRX:	call	omni$init	;init hardware & get ID code from its switches
	sta	configtbl+1	; store ID in config table as master ID
;
	mvi	c,MAKEQ		;create the free buffer Q
	lxi	d,buffQCB
	call	bdos
;
	lxi	d,inqcb$array
	mvi	c,NSLAVES	;create input Qs (1/slave supported)
make$inQs:
	push	d
	push	b
	mvi	c,MAKEQ
	call	bdos
	pop	b
	pop	d
	lxi	h,INQCB$SIZE
	dad	d
	xchg
	dcr	c
	jnz	make$inQs
;
	lxi	d,outQCB	;create the output Queue (only 1)
	mvi	c,MAKEQ
	call	bdos
;
	lxi	d,ServetxPD	;create the network output process
	mvi	c,CREATEP
	call	bdos
;
	mvi	c,SYDATAD	;get system data page address
	call	bdos
	lxi	d,9
	dad	d		;install config table address at sysdat(9)
	lxi	d,configtbl
	mov	m,e
	inx	h
	mov	m,d
;
	lxi	h,rxrslt	;initialize transporter command block result
	mov	d,l		;field to point to receive result block
	mov	e,h		; (done at run time because of reversed byte
	xchg			;  order used by CORVUS.)
	shld	rxrsltp


;	Receiver server process loop head
RXloop:	mvi	c,READQ
	lxi	d,gbuf$uqcb
	call	bdos		;get a free message buffer from Q
;
RXretry:
	lhld	newbuff
	mov	e,h
	mov	d,l
	xchg			;swap bytes for CORVUS command block
	shld	rxtcb+6		;put buffer address pointer in rx tcb
;
	lxi	d,omnirx$uqcb	;read MX message from OMNINET HARDWARE MX Q
	mvi	c,READQ
	call	bdos
;
	lxi	b,rxtcb		;send TCB pointer to hardware
	call	omni$strobe
;
	push	psw		;return MX message
	lxi	d,omnirx$uqcb
	mvi	c,WRITEQ
	call	bdos
	pop	psw		;restore return code from omni$strobe routine
;
	jc	RXretry		;no choice except to retry if not accepted
;
	lxi	b,rxrslt	;wait for a completion from hardware
	call	wfrxdone
	ani	80h		;if error on message, re-post buffer
	jnz	RXretry
;
;  buffer contains a valid message at this point, so process it
	lhld	newbuff		;get FMT to A
	mov	a,m
	inx	h		;get SID to C
	inx	h
	mov	c,m
;
	ani	0feh		;look for login/logoff messages
	jnz	RXl2		;message type 0 or 1?
	inx	h		;yes, check FNC
	mov	a,m
	cpi	40h		;login?
	jnz	RXl1		;not login, go on
	call	logiton		;ELSE try to find a free SLVSP in table
	jnz	RXl3		;found one (or already logged in), go on
	jmp	RX$send$err	;sorry,no free processes, go advise slave
;
RXl1:	cpi	41h		;logoff?
	jnz	RXl2		;not logoff, go on
	call	logitoff	;ELSE try to remove that slave from table
 	jnz	RXl3		;if successful, go on
	jmp	RX$send$err	;otherwise go tell slave it wasn't logged in
;
RXl2:	call	get$slvsp	;not login/logoff so get slvsp msg address
	jnz	RXl3		; for that slave if it is logged in and go
				; send message to its Q else fall through
;
;	this code sends a "NOT LOGGED IN" error message back to requester
RX$send$err:
	lhld	newbuff		;build an error message in the same buffer
	shld	err$out$msg
	mvi	m,1		;FMT=1
	inx	h
	mov	a,m		;swap DID and SID
	inx	h
	mov	b,m
	mov	m,a
	dcx	h
	mov	m,b
	inx	h		;leave FNC field alone
	inx	h
	inx	h
	mvi	m,1		;SIZ=1
	inx	h
	mvi	m,0ffh		;message = 0FFH (extended error flag)
	inx	h
	mvi	m,12		;"NOT LOGGED IN" code
	lxi	d,err$out$uqcb	;post to network transmitter process
	jmp rxl4		;using common write Q code
;
;  this code sends the message address to the appropriate SLVSP Q
RXl3:	lhld	newbuff		;DE--> msg field of correct UQCB here
	xchg			;put message ptr in UQCB message field
	mov	m,e
	inx	h
	mov	m,d
	lxi	d,-(XQCBMSG + 1);index back to UQCB base address
	dad	d
	xchg
;
rxl4:	mvi	c,WRITEQ
	call	bdos		;send it to Queue
	jmp	RXloop		;go back and get another buffer and continue
	

;	routine dynamically maps physical slave number passed in C
;	to a slave support process and returns its INUQCB message buffer addr
;	in DE and A = 0 with flags set if no room or not found, else NZ
get$slvsp:
	mov	a,c		;A= requester ID
	mvi	b,NSLAVES	;set up for table search
	lxi	h,idtbl
find$match:			;search till match or table end
	cmp	m
	jnz	not$match	; goto not$match if not this one
	inx	h		;else match found, get ptr to SLVSP message
	mov	e,m
	inx	h
	mov	d,m		;its slvsp msg addr
	stc
	sbb	a
	ret			;and return TRUE in A to caller
not$match:
	inx	h		;no match, skip to next entry
	inx	h
	inx	h
	dcr	b		;any more entries?
	jnz	find$match	;loop back until all searched
	xra	a		;else return failure (A=00)
	ret


;	removes entry (C=SID) from map table (but still returns msg ptr)
logitoff:
	call	get$slvsp
	rz			;not in table, just exit
	dcx	h		;else mark entry as free and then exit
	dcx	h
	mvi	m,0ffh
	ret

;	installs entry (C=SID) in first free entry of map table and returns
;	msg address. RETURNS A=0 if no space, else non-zero.
logiton:
	call	get$slvsp	;see if already in table
	rnz			;if so, just use old entry
	push	b		;else look for a free entry (CODE=FF)
	mvi	c,0ffh
	call	get$slvsp
	pop	b
	rz			;no free entries, exit
	dcx	h		;else enter SID in table and return success
	dcx	h
	mov	m,c
	ret			;PSW is still correct from search

;	Slave mapping table has one entry per SLVSP.  First  byte = SID
;	of the requester currently using SLVSP (0ffh if none).  Next word is
;	the address of the message field of that SLVSP's input UQCB.
idtbl:
??xx	set	0
	rept	NSLAVES
	  db	0ffh
	  dw	(INUQCB + XQCBMSG + ??xx)
??xx	  set	??xx + UQCBLEN
	endm

page
;________________________________________________________________________
;________________________________________________________________________
;									;
;	This is the network transmitter server process module.		;
;	NOTE THAT THE OMNINET TRANSPORTER MUST NOT BE DISTURBED ONCE	;
;	A TRANSMIT HAS BEEN POSTED UNTIL IT RETURNS A COMPLETION.	;
;	An MX Queue is used in this version to protect the transporter	;
;	from other processes.						;
;									;
;	This process reads a message from the SLVSP output Q and when	;
;	awakened by one posts that buffer for transmission via the	;
;	transporter to the requester.  This process then waits until	;
;	the transporter reports a completion as determined by the	;
;	wf$txdone routine.  The buffer pointer from that message is	;
;	then sent back to the FreeBuff Q and the process loops back for	;
;	another	message from the SLVSP output Q.  Transmitter errors	;
;	are considered the Transporter's problem and are ignored here.	;
;________________________________________________________________________
;________________________________________________________________________
;	Transmitter server process descriptor
ServetxPD:
	dw	0			;link
	db	0,TX$PRIORITY		;status,priority
	dw	$ + 94			;stack pointer
	db	'ServeTX '		;name
	db	0,0ffh			;console, memseg
	ds	82			;reserved for MP/M use and as stack
	dw	InitTX			;stack top has startup PC

;	There is only one output queue (SLVSP --> NTWRKIF)
OUTUQCB:
UQCBNtwrkQO0:	dw	outQCB,outQMSG	;pointer, msgadr
outQMSG:	ds	2		;used to receive msg pointer from SLVSP

;	used by ServeTX to return them to Q when done (used at init also)
pbuf$uqcb:	dw	buffQCB,oldbuff
oldbuff:	dw	0		;msg is a freed buff ptr back to pool

;	UQCB used by ServeTX to get transporter from MX Q
omnitx$uqcb:	dw	omniQ,tx$mx$msg
tx$mx$msg:	dw	0

;	transmitter transporter control block
txtcb:	db	40h			;command
	db	0			;result hi
txrsltp:
	db	0,0			;result middle and low
	db	OMNI$SOCKET		;transporter message socket code
	db	0,0,0			;data ptr (MSB,SB,LSB)
	db	0,0			;length (MSB,LSB)
	db	0			;control length
	db	0			;dest host

txrslt:	db	0,0,0,0,0,0,0,0		;result block for tx

;________________________________________________________________________
;									;
;	ServeTX initialization entry point				;
;________________________________________________________________________
InitTX:
	lxi	h,msgbuffs	;preload the Free buffer Q with buffer ptrs
	mvi	c,NMSG$BUFFS	;from start of buffer space
freeloop:
	shld	oldbuff
	push	h
	push	b
	mvi	c,WRITEQ
	lxi	d,pbuf$uqcb
	call	bdos
	pop	b
	pop	h
	lxi	d,BUFFSIZE
	dad	d
	dcr	c
	jnz	freeloop
;
	lxi	h,txrslt	;initialize TX Transporter Command Block
	mov	e,h		;to point to TX Result Block
	mov	d,l
	xchg	
	shld	txrsltp

;	ServeTX process loop
TXloop:
	mvi	c,READQ		;wait for a message in network output Q
	lxi	d,outuqcb
	call	bdos
;
	lhld	outQMSG
	mov	e,h
	mov	d,l		;put message buffer address in TX TCB
	xchg			;(NOTE, NOT (8080 byte order)
	shld	txtcb+6
;
	inx	d
	ldax	d		;set transport layer destination addr=DID
	sta	txtcb + 11
;
	lxi	h,3
	dad	d		;calculate physical message length
 	mov	l,m		;from SIZ field
	mvi	h,0
	lxi	d,6		;put in TCB length field
	dad	d
	mov	d,l
	mov	e,h
	xchg
	shld	txtcb+8
;
	lxi	d,omnitx$uqcb	;get transporter hardware MX message
	mvi	c,READQ
	call	bdos
;
TXretry:
	lxi	b,txtcb		;send TCB pointer to hardware
	call	omni$strobe	;if can't, not much else to do but try again
	jc	TXretry		;  (ALTHOUGH THIS IS A FATAL HARDWARE ERROR)
;
	lxi	b,txrslt	;wait for transmit completion
	call	wftxdone	;ignore errors here as no recovery possible
;
	lxi	d,omnitx$uqcb
	mvi	c,WRITEQ
	call	bdos		;release MX msg
;
	lhld	outQMSG		;send the buffer back to FREEBUFF Q
	shld	oldbuff
	mvi	c,WRITEQ
	lxi	d,pbuf$uqcb
	call	bdos
;
	jmp	txloop		;and go back and do it all with next msg


page
cnote:	db 'NTWRKIF (c)1982 VANO ASSOCIATES, INC. - ALL RIGHTS RESERVED'
;________________________________________________________________________
;________________________________________________________________________
;									;
;	GLOBAL Master Configuration Table and storage			;
;	(address must be installed on SysData page(9,10) at init.)	;
;________________________________________________________________________
;________________________________________________________________________
configtbl:
	db	0		;Master status byte
	db	0		;Master processor ID
	db	NSLAVES		;Maximum number of slaves supported
	db	0		;Number of logged in slaves
	dw	0		;16 bit vector of logged in slaves
	ds	16		;Slave processor ID array
	db	'PASSWORD' 	;login password

;	builds Server stacks and initializes them with PD storage pointers
??xx	set	0
	rept	NSLAVES
	  ds	SRVR$STK$SIZ - 2
	  dw	srvr$pd$base + ??xx
	  ??xx	set ??xx + SRVR$PD$SIZ
	endm

;	allocates PD storage
srvr$pd$base:
	ds	NSLAVES * SRVR$PD$SIZ

;________________________________________________________________________
;________________________________________________________________________
;									;
;	INTERPROCESS QUEUES (both local and global) and COMMON data	;
;________________________________________________________________________
;________________________________________________________________________

;	ServeRX --> SLVSP message queues (INPUT), 1/slave support proc.
INQCB$SIZE	equ	26	;constant used for index calculation
inqcb$array:			;ARRAY BASE NAME
;
;	generate INQCBs as required
??xx	set	'0'
	rept	NSLAVES
	  ds	2		;;link
	  db	4eh,74h,77h,72h	;;common name is NTwrkQI
	  db	6bh,51h,49h	;;(macro can't do lower case)
	  db	??xx		;;slave ID
	  dw	2,1		;;msglen, nmbmsgs
	  ds	12		;;MP/M pointers and buffers
??xx	  set	??xx + 1
  if (??xx EQ ('9'+1))
??xx	    set	??xx + 7
  endif
	endm

;	SLVSP --> NETWRKIF queue (OUTPUT)
outQCB:	ds	2		;link
	db	'NtwrkQO0'	;name
	dw	2,16		;msglen, nmbmsgs
	ds	48		;Used by MP/M

;	free buffer list management queue
buffQCB:
	ds	2		;link
	db	'FreeBuff'	;name
	dw	2,16		;msglen, nmbmsgs
	ds	48		;reserved for MP/M


;	global message buffer pool
msgbuffs:	ds	NMSG$BUFFS * BUFFSIZE

;	Utility Procedure to allow indirect BDOS/XDOS access as needed by RSP
bdos:	lhld	bdosadr
	pchl

page
;________________________________________________________________________
;________________________________________________________________________
;									;
;	low level omninet support routines				;
;________________________________________________________________________
;________________________________________________________________________

;	Transporter mutual exclusion QUEUE
omniQ:	ds	2
	db	'MXomniQ '
	dw	0,1			;msglen, nmsgs
	ds	12			;dqph,nqph,msgin,msgout,msgcnt,buff

;	UQCB used by omni$init to load MX Q
omni$init$uqcb:	dw	omniQ,init$mx$msg
init$mx$msg:	dw	0


;	Initialization transporter control block
inittcb:
	db	20h			;command
	db	0			;result hi
initrsltp:
	db	0,0			;result middle and low
;
initrslt:
	db	0			;result block for init


;	initializes transporter hardware and return its network ID code in A
omni$init:
	lxi	d,omniQ
	mvi	c,MAKEQ
	call	bdos		;create hardware MX Q
	lxi	d,omni$init$uqcb ;send it one message
	mvi	c,WRITEQ
	call bdos
  if INTERRUPT
	call int$init		;(optional) setup interrupt system
  endif
	lxi	h,initrslt	;install result block pointer in initialization
	mov	d,l		;TCB
	mov	e,h		;NOTE: NOT 8080 order, MSB,LSB
	xchg
	shld	initrsltp
;
	lxi	b,inittcb	;post initialization command block to 
	call	omnistrobe	;hardware
	rc			;cy=1 means can't talk to hardware
;
	lxi	b,initrslt	;wait for a completion from operation
	call	omni$wfdone
	ora	a
	ret			;return ID/result code to caller with flags set


;	sends the command block pointer in BC to transporter hardware
omni$strobe:
	lxi	h,2		;first preset result code byte in
	dad	b		;result block TCB result field --> to 0ffh
	mov	a,m
	inx	h
	mov	l,m
	mov	h,a
	mvi	m,0ffh
;
	xra	a		;send bits 23-16 of ptr to hardware (always 0)
	call	omni$st
	rc			;carry means can't talk to hardware
;
	mov	a,b		;send bits 15-8 of ptr to hardware
	call	omni$st
	rc
;
	mov	a,c		;send bits 7-0 of ptr to hardware
				;fall into omni$st to send last byte

;	called by omni$strobe to send one byte to transporter when ready
;	(waits a reasonable time for transporter to come ready and if
;	it doesn't, returns with carry set;  this is a fatal error) returns
;	cy=0 if succeeds
omni$st:
	push	psw		;save data for now
	lxi	d,50000		;set timeout
omni$st0:
	in	OMNI$STAT	;see if transporter will accept byte
	ani	OMNI$RDY
	jz	omni$st1	;if busy, go decrement timeout and retry
	pop	psw		;else output the byte and return with CY=0
	out	OMNI$DATA
	ora	a
	ret
omni$st1:
	dcx	d		;loop back if not timeout yet
	mov	a,e
	ora	d
	jnz	omni$st0
	pop	psw
	stc
	ret			;else return CY=1 as error flag


;	routine waits for a completion to occur on the result block
;	pointed to by BC.  This routine is used by the initialization
;	and receiver processes.  If there is no interrupt hardware in
;	the system, ONLY ONE MESSAGE CAN BE RECEIVED PER CLOCK TICK of
;	the system clock.  This will considerably reduce server throughput
;	in most systems.
omni$wfdone:
wfrxdone:
	ldax	b		;all completion codes are < 0f0h
	cpi	0f0h		;see if already done before suspending caller
	rc			;yes, return immediately
;	else suspend caller until a completion occurs
	push	b
  if INTERRUPT
	lxi	d,OMNI$FLAG	;wait for ISR to set flag
	mvi	c,FLAGWAITF
	call	bdos
  else
	lxi	d,1		;if no ISR, poll result block once/tick
	mvi	c,DELAY
	call	bdos
  endif
	pop	b
	jmp	omni$wfdone

;	As above but instead polls continually to give transmitter priority
;	since transmitter usually unloads messages in less time than MP/M
;	dispatch overhead, it is not worth suspending it.
;	A timeout routine is included to avoid locking up system if hardware
;	fails so diagnosing the problem is possible with RDT.
wftxdone:
	lxi	d,50000		;initialize hardware fail timeout
wftxd0:	ldax	b		;done yet?
	cpi	0f0h
	cmc			;set up carry properly in case of return
	rnc			;yes, return to caller with result in A, CY=0
wftxd1:	dcx	d		;if not timeout, loop back
	mov	a,e
	ora	d
	jnz	wftxd0
	stc
	ret			;else return to caller with CY=1 as error flag

page
  if INTERRUPT
;
;	Since the CORVUS "ENGINEERING" transporter has no interrupt hardware
;	associated with it, the details of the interrupt initialization and
;	service routines will vary from system to system.  The skeleton of
;	our code is provided here as a guide to understanding what is needed.
;
;	Routine initializes interrupt hardware and attaches ISR to XIOS
;	at run-time (in somewhat bizarre fashion.)  It would be better
;	to make your ISR a permanent part of your XIOS since if not
;	used it does no harm to the system.
int$init:
	di
	mvi	a,(jmp)		;build jump in vector
	sta	(INT$VCTR)
	lxi	h,omni$isr
	shld	(INT$VCTR + 1)	;install new isr
	out	OMNI$ACK	;clear interrupt latch
	mvi	a,OMNI$ENABLE	;unmask transporter interrupt
	out	OMNI$MASK
; this code does an extremely Klugey run-time linkage to needed XIOS routines
	lhld	1		;find CBOOT in MPM-II BIOS simulation table
	mvi	l,1
	mov	e,m
	inx	h
	mov	d,m
	push	d		;save to find exit$reg.
;
	xchg			;need to go one more level to find real entry
	inx	h
	mov	e,m
	inx	h
	mov	d,m		;this is address of real CBOOT entry in XIOS
;
	lxi	h,9		;calculate PDISP entry from CBOOT address
	dad	d
	shld	pdisp		;and save it in local vector		
;
	lxi	d,3		;XDOS address is 3 bytes above PDISP
	dad	d
	shld	xd$adr		;save it in a local vector
;
	pop	h		;get XIOS branch table address back
	mvi	l,40h		;calculate address of EXIT$REGION entry
	mov	e,m
	inx	h
	mov	d,m
	xchg
	shld	exit$region	;save it for later use in pre-empt routine
	ei
	ret

;	omninet isr sets the appropriate XDOS flag and causes a dispatch
omni$isr:
	shld	svhl
	pop	h
	push	psw		;save PSW and HL
	shld	svret		;save return address
	lxi	h,0		;swap stacks
	dad	sp
	shld	svstk
	lxi	sp,isr$stk
	push	d		;save the other registers on new stack
	push	b
;
	out	OMNI$ACK 	;clear interrupt latch
;
	lhld	exit$region	; do a PRE-EMPT by patching a RET into table
	mov	a,m		; (Very KLUGEY but there's no other way.)
	push	psw		; save what was in XIOS branch table entry
	push	h		; and put a RET there to prevent XDOS from
	mvi	m,(RET)		; re-enabling interrupts
;
	mvi	c,FLAGSETF	;call XDOS to set isr flag
	mvi	e,OMNI$FLAG
	call	xdos
;
	pop	h
	pop	psw
	mov	m,a		;restore XIOS table entry
;
	pop	b		;pop interrupted registers
	pop	d
	lhld	svstk		;restore interrupted stack
	sphl			;restore other regs. and exit
	pop	psw
	lhld	svret
	push	h
	lhld	svhl
	db	(JMP)		; via dispatcher
pdisp:	dw	0		;(link to dispatcher)

xdos:	db	(JMP)		;special XDOS entry
xd$adr:	dw	0		;for ISR use

;	ISR data areas
exit$region:
	dw	0		;address of XDOS critical region exit routine
	ds	64		;isr stack space
isr$stk:
svhl:	dw	0		;temporary reg storage
svret:	dw	0
svstk:	dw	0		;careful, make sure all of .RSP is reserved

  endif ; of if INTERRUPT

	end
