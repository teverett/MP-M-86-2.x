  title	'Sample Slave Network I/O System for CORVUS OMNINET 20 Oct 82'
	page	54

;________________________________________________________________________
;________________________________________________________________________
;									;
;	SAMPLE SLAVE NETWORK IO SYSTEM FOR CP/NET 1.2			;
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
;	ANY USE OF THIS CODE without the imbedded copyright notice and	;
;	banner is hereby strictly prohibited.				;
;									;
;	Permission is hereby granted to Digital Research Inc. to use	;
;	this source file for educational and illustrative purposes in 	;
;	conjunction with CP/Net 80 documentation.  Any other use of 	;
;	this code without the EXPRESS WRITTEN PERMISSION of VANO	;
;	ASSOCIATES INC. is hereby strictly prohibited.			;
;									;
;	This file is provided courtesy of:				;
;									;
;			R2E (Realisations Etude Electroniques)		;
;			Z.A.I. de Courtaboeuf				;
;			BP 73	91942 Les Ulis				;
;			FRANCE						;
;									;
;	who sponsored the development of one of its ancestors.		;
;________________________________________________________________________
;________________________________________________________________________

;	*****	CONSTANT DECLARATIONS	*****

FALSE	equ	0
TRUE	equ	not FALSE

;	configuration and option constants
TXTRIES equ	100		;Transmit message retries
BUFFSIZE equ	138		;max default buffer size
MAXMSG	equ	512		;largest message accepted by receiver
SKT0	equ	80h		;legal omninet socket tokens
  SKT1	equ	90h
  SKT2	equ	0a0h
  SKT3	equ	0b0h
SOCKET	equ	SKT2		;this SNIOS uses only channel 2

;	OMININET Constants
;  Completion/return codes
NOERR	equ	0		;done (no errors or retries)
ETXOK	equ	0c0h		;echo succeeded with no retries (not used here)
ETXFAIL	equ	80h		;Transmit failed
E2LONG	equ	81h		;wouldn't fit in destination socket
ENOSKT	equ	82h		;destination socket not set up
EBDCTL	equ	83h		;bad control field length
EBDSKT	equ	84h		;illegal socket number
EBDDES	equ	85h		;invalid destination node number/socket in use
EBDNODE	equ	86h		;bad node number in command (not 0-7fh or ffh)
ECMDOK	equ	0feh		;command has been read by transporter
; legal command tokens
SENDF	equ	40h		;send message
RCVF	equ	0f0h		;set up receive socket
ENDRCVF	equ	10h		;stop receive
INITF	equ	20h		;initialize transporter
; Transporter control ports
NETBASE	equ	0f8h		;base address of transporter IO interface
  TSTAT	equ	Netbase+1	;ready status port
  TCRDY	equ	10h		;status mask for ready bit
  TDATA	equ	Netbase		;command block pointer port

;	Network Status Byte Constants
;
ACTIVE		equ	10h	;slave logged in on network
RCVERR		equ	2h	;error in received message
SENDERR		equ	1h	;unable to send message

;	CP/M BDOS function constants
BDOS	equ	5		;absolute BDOS entry
PRINTF	equ	9		;print message function
CONOUTF	equ	2		;output char in E to console

;	General Constants
LF	equ	0ah		;Line Feed
CR	equ	0dh		;Carriage Return

;	*****	GENERATED CODE AND DATA BEGIN HERE	*****

;	Public Jump vector for SNIOS entry points
	jmp	ntwrkinit	;network initialization
	jmp	ntwrksts	;network status
	jmp	cnfgtbladr	;return config table addr
	jmp	sendmsg		;send message on network
	jmp	receivemsg	;receive message from network
	jmp	ntwrkerror	;network error
	jmp	ntwrkwboot	;network warm boot

;	Public Slave Configuration Table
configtbl:
Network$status:
	db	0				;network status byte
slvid1:	db	0				;slave ID (from switches)
	db	0,0,	0,0,	0,0,	0,0	;Disk map table for units A:-P:
	  db	0,0,	0,0,	0,0,	0,0
	  db	0,0,	0,0,	0,0,	0,0
	  db	0,0,	0,0,	0,0,	0,0
	db	0,0				;console device
	db	0,0				;list device
	db	0				;buffer index
;
dflt:	db	0				;FMT (DEFAULT MESSAGE BUFFER)
	  db	0				;DID
slvid2:	  db	0				;SID
	  db	5				;FNC
	  db	0				;SIZ
	  ds	1				;MSG(0)  List number
	  ds	BUFFSIZE			;MSG(1) ... MSG(128)


;	*****	PREFABRICATED OMNINET TRANSPORTER COMMAND BLOCKS   *****

;	Command block for transmitting a message
TXtcb:
 TXtcmd:	db	SENDF		;command field
		db	0		;bits 16-24 of result block ptr
 TXtrslt:	db	0,0		;result block pointer (MSB,LSB)
 TXtskt:	db	SOCKET		;socket (channel)  number
		db	0		;bits  16-24 of message buffer ptr
 TXtmsg:	db	0,0		;message buffer pointer (MSB,LSB)
 TXtdlen:	db	0,0		;data field length (MSB,LSB)
 TXtclen:	db	0		;control field length
 TXtdest:	db	0		;Destination address (transport layers)
;	Result vector for above command block
TXresult:
 TXrcode:	db	0		;return code

;	Command block for setting up a receive operation
RXtcb:
 RXtcmd:	db	RCVF		;command field
		db	0
 RXtrslt:	db	0,0		;result block pointer (MSB,LSB)
 RXtskt:	db	SOCKET		;socket number
		db	0
 RXtmsg:	db	0,0		;message address (MSB,LSB)
 RXtdlen:	db	MAXMSG/256	;max data field length (MSB,LSB)
		db	MAXMSG and 255
 RXtclen:	db	0		;max control field length
 RXtdest:	db	0		;(not used in a receive operation)
;	Result vector for receiver
RXresult:
 RXrcode:	db	0		;return code
 RXrsrce:	db	0		;source HOST #
 RXrdlen:	db	0,0		;received message length (MSB,LSB)

;	Command block for receive cancel operation
UNRXtcb:
 UNRXtcmd:	db	ENDRCVF		;command field
		db	0
 UNRXtrslt:	db	0,0		;result block pointer (MSB,LSB)
 UNRXtskt:	db	SOCKET		;socket number
;	Result vector for receive cancel
UNRXresult:
 UNRXrcode:	db	0		;return code

;	Command block for transporter initialization command
INITtcb:
 INITtcmd:	db	INITF		;command field
		db	0
 INITtrslt:	db	0,0		;result block pointer (MSB,LSB)
;	Result vector for initialization
INITresult:
 INITrcode:	db	0		;return code (if valid,=ID code)


;	*****	PUBLIC CODE ENTRIES BEGIN HERE   *****

;	Externally accessed routine to initialize transporter
;		(RETURNS A=0 if succeeds, else 0ffh.)
ntwrkinit:
	call	ntwrkerror		;init transporter, tcbs and id code
	rc				;return error if init fails
	lxi	d,initmsg		;else prinw slave ID and banner
	call	print$msg
	lda	slvid1
	call	prhex			;print slave ID
	xra	a			;and return to caller with a=0
	ret

initmsg:
	db	CR,LF,'SNIOS (c)1982 Vano Associates Inc.'
	db	CR,LF,'SLAVE ID = $'


;	Externally accessed routine inits or re-inits module
;		(RETURNS A=0 if succeeds, else 0ffh.)
ntwrkerror:
	xra	a
	sta	Network$status		;zero network status byte
	call	omni$init		;init transporter, tcbs and id code
	rc				;carry means error, A=0ffh
	sta	slvid1			;update this slaves id in table
	sta	slvid2			;and default message
	xra	a			;and return with no error
	ret


;	Externally accessed routine returns Network Status Byte in A
;		(also clears any error bits active)
ntwrksts:
	lxi	h,network$status
	mov	b,m
	mvi	a,not(RCVERR or SENDERR)
	ana	b
	mov	m,a
	mov	a,b
	ret


;	Externally accessed routine Returns Configuration Table Ptr in HL
cnfgtbladr:
	lxi	h,configtbl
	ret


;	Externally accessed routine is called each time the CCP is reloaded
;	from disk.  (Dummy procedure for now.)
ntwrkwboot:
	lxi	d,wboot$msg		;return via print$msg
	jmp	print$msg

wboot$msg:
	db	CR,LF,'<CP/NET>$'


;	Externally accessed routine sends Message BC--> on Network
;		(returns A=0 if succeeds, else A=0ffh.)
;
;	NOTE that although the OMNINET transporter does its own transport
;	  layer retries, this routine does additional retries to deal with
;	  servers that are slow in posting receive calls since transport
;	  level retries are exhausted in a very short real-time period.
sendmsg:
	mov	h,c		;move buffer pointer to Transporter ctrl block
	mov	l,b		;(note reversed byte order for Transporter.)
	shld	TXtmsg
;
	lxi	h,4		;get CP/Net message length from SIZ field
	dad	b
	mov	l,m
	mvi	h,0
	lxi	d,6		;add packet header lgth to get actual size
	dad	d		;  of packet for transport layer purposes
	mov	a,h		;swap bytes to MSB, LSB order
	mov	h,l
	mov	l,a
	shld	TXtdlen		;store length in TCB data length field
;
	inx	b		;get DID from message
	ldax	b
	sta	TXtdest		;put it into TCB destination address field
;
	lxi	d,TXTRIES	;use DE as retry counter
;
send$again:			;head of message transmission retry loop
	push	d
	lxi	b,TXtcb		;send TCB pointer to transporter hardware
	call	omni$strobe
	pop	d
	jc	snderr		;if not accepted, goto fatal error handler
;
	lxi	b,TXresult	;else poll result block until completion code
	push	d		;is returned by hardware
	call	omni$wfdone
	pop	d
;
	ani	80h		;completion codes 80h-ffh are error codes
	rz			;return 00 to caller if no errors
;
	dcx	d		;else decrement retry counter
	mov	a,e
	ora	d
	jnz	send$again	;retry transmit if any retries left
;
snderr:	mvi	a,SENDERR	;goto common exit code to update error flags
	jmp	nerr		;(part of receivemsg routine)


;	Externally accessed routine waits for a message directed to this node
;	and returns it in the buffer BC-->.  To aid debugging, a timeout of
;	about 20 seconds (2 Mhz processor) is implemented that will return an
;	error if no message is received.  That is long enough for most normal
;	servers to respond.
;
;	(RETURNS A=0 if good msg, =0ffh if  bad msg or timeout.)
receivemsg:
	mov	l,b		;swap buffer pointer bytes to MSB,LSB order
	mov	h,c
	shld	RXtmsg		;put buffer ptr to its TCB field
;
	lxi	b,RXtcb
	call	omni$strobe	;post control block address to hardware
	jc	rxerr		;fatal error if hardware won't accept it
;
	lxi	b,RXresult
	call	omni$wfdone	;else wait for a completion from hardware
	ani	80h
	rz			;return 00 to caller if no error reported
;  the rest is the fatal error handler for receive calls
	lxi	b,UNRXtcb	;otherwise cancel the receive call
	call	omni$strobe	; (using prefabricated cancel command block)
	jnc	rxerr		;If won't accept this command either, quit here
;
	lxi	b,UNRXresult	;else wait for completion of cancel command
	call	omni$wfdone	;ignore result (always fatal error return)
rxerr:	mvi	a,RCVERR	;exit via code that updates status byte

;	This is also used by sendmsg to update Network$status and return 0ffh
nerr:	lxi	h,Network$status
	ora	m
	mov	m,a		;update status
	mvi	a,0ffh
	ret			;return 0ffh to caller


;	*****   UTILITY ROUTINES CALLED BY ABOVE BEGIN HERE  *****

;	prints A in hex on console
prhex:	push	psw
	rlc
	rlc
	rlc
	rlc
	call	nibl		;print high nibble
	pop	psw		;and fall through to  print low nibble

nibl:	ani	0fh
	adi	'0'
	cpi	'9'+1
	jc	printa
	adi	7
printa:	mov	e,a
	mvi	c,CONOUTF
	jmp 	BDOS		;print ascii and return


;	print message DE--> until $ on console device
print$msg:
	mvi	c,PRINTF		;prints $ delimited string DE-->
	jmp	BDOS			;bdos(printf,wboot$msg)


;	*****	LOW LEVEL OMNINET TRANSPORTER DRIVERS BEGIN HERE   *****

;	Initialize transporter and return its ID code in A or 0ffh if can't.
;	Carry is also set if error, clear if no error.
omni$init:			;initialize pointers in our control blocks
	lxi	d,TXresult	;NOTE: this is done at run time to avoid
	mov	h,e		;  relocation problems caused by the need to
	mov	l,d		;  have pointers for CORVUS transporter use
	shld	TXtrslt		;  in MSB, LSB form instead of 8080 format.
;
	lxi	d,RXresult
	mov	h,e
	mov	l,d
	shld	RXtrslt
;
	lxi	d,UNRXresult
	mov	h,e
	mov	l,d
	shld	UNRXtrslt
;
	lxi	d,INITresult
	mov	h,e
	mov	l,d
	shld	INITtrslt
;
	lxi	b,INITtcb	;send init command block pointer to transporter
	call	omnistrobe	;to reset it and get its ID code
	sbb	a		;in case of error, preset return code 0 or ff
	rc			;fatal error if hardware won't accept pointer
;
	lxi	b,INITresult	;else wait for result of operation
	call	omni$wfdone	;wait for done
	sta	slvid1		;result code should be ID code so put in table
	sta	slvid2		;and in default message SID
;
	rlc			;set CY=bit 7 of return code
	rar			;so CY=1 if error
	rnc			;return with ID code if no error
	sbb	a		;else set carry=1 and A=0ffh and return
	ret


;	Sends the 16 bit POINTER in BC to the transporter hardware as
;	a 24 bit pointer (MSB first).  Returns CY set if hardware will
;	not accept any byte in a reasonable time else CY clear.
omni$strobe:
	lxi	h,2		;Find address of rslt block from TCB BC-->
	dad	b		;pre-set result code in block to ff (busy)
	mov	a,m
	inx	h
	mov	l,m
	mov	h,a
	mvi	m,0ffh
;
	xra	a		;MSB is always 0
	call	omni$st		;send bits 23-16 of pointer to hardware
	rc			;(abort if timeout)
;
	mov	a,b		;send bits 15-8 of pointer to hardware
	call	omni$st
	rc			;(abort if timeout)
;
	mov	a,c		;send bits 7-0 of pointer to hardware
				; (fall into omni$st)

;	called by omni$strobe to send one byte from A to transporter hardware
;	returns CY set if hardware doesn't come ready in a reasonable time.
omni$st:
	push	psw		;save data for now
	lxi	d,50000		;set timeout
omni$st0:
	in	TSTAT		;read status port and check busy bit
	ani	TCRDY
	jz	omni$st1	;if busy, go increment and test timeout
;
	pop	psw		;else output the byte
	out	TDATA		;to the transporter TCB pointer input register
	ora	a
	ret			;and return with no error shown (CY=0)
;
omni$st1:			;else
	dcx	d
	mov	a,d
	ora	e
	jnz	omni$st0	;loop back if not timed out yet
;
	pop	psw		;else
	stc
	ret			;return error flag (CY=1)


;	waits till timeout (about 20 secs) for result block BC--> to show done
;	returns A=returned status code.  If timeout occurs, the returned
;	status will still be 0FEH or 0FFH.
omni$wfdone:			
	lxi	d,0ffffh	;setup timeout counters
	mvi	l,20
;
omni$wfdone1:
	ldax	b		;is the result code still > 0f0h?
	cpi	0f0h
	rc			;no, return to caller
;
	dcx	d		;else decrement timeout
	mov	a,e
	ora	d
	jnz	omni$wfdone1	;timeout yet?
	dcr	l
	jnz	omni$wfdone1	;no, go back and check again
;
	ldax	b		;yes, timeout
	ret			;return with completion code in A


	end
