	title	'LOGOFF Transient Program'

;***************************************************************
;***************************************************************
;**                                                           **
;**      L O G O F F   T r a n s i e n t   P r o g r a m      **
;**                                                           **
;***************************************************************
;***************************************************************

;
; Equates
;
BDOS	equ	0005h
buff	equ	0080h

print	equ	9
version	equ	12
logoff	equ	65
cfgtbl	equ	69

active	equ	0001$0000b

start:
	lxi	h,0
	dad	sp
	lxi	sp,CCPStack+2
	push	h	; save CCP stack pointer
	mvi	c,version
	call	BDOS	; get version number
	mov	a,h
	ani	0000$0010b
	jz	versionerr ; CP/Net must be loaded
	mvi	e,0	; default Mstr ID = 00h
	lxi	h,buff
	mov	a,m	; get # chars in the command tail
	ora	a
	jz	dologoff ; default logoff if empty command tail
	mov	c,a	; A = # chars in command tail
	xra	a
scanblnks:
	inx	h
	mov	a,m
	cpi	' '
	jnz	pastblnks ; skip leading blanks
	dcr	c
	jnz	scanblnks
	jmp	dologoff
pastblnks:
	cpi	'['
	jnz	logofferr
scanMstrID:
	inx	h
	dcr	c
	jz	logofferr
	mov	a,m
	cpi	']'
	jz	dologoff
	sui	'0'
	cpi	10
	jc	updateID
	adi	('0'-'A'+10) and 0ffh
	cpi	16
	jnc	logofferr
updateID:
	push	psw
	mov	a,e
	add	a
	add	a
	add	a
	add	a
	mov	e,a	; accum * 16
	pop	psw
	add	e
	mov	e,a
	jmp	scanMstrID

dologoff:
	mvi	c,logoff
	call	BDOS
	inr	a
	jnz	logoffOK
	lxi	d,logoffailedmsg
	jmp	printmsg

versionerr:
	lxi	d,versionerrmsg
	jmp	printmsg

logofferr:
	lxi	d,logofferrmsg
printmsg:
	mvi	c,print
	call	BDOS
	jmp	Exit

logoffOK:
	mvi	c,cfgtbl
	call	BDOS	; get config table address
	mov	a,m
	ani	not active ; turn off active bit
	mov	m,a
Exit:
	pop	h
	sphl		; restore CCP stack pointer
	ret

;
; Local Data Segment
;
versionerrmsg:
	db	'CP/Net is not loaded.'
	db	'$'

logofferrmsg:
	db	'Illegal LOGOFF command.'
	db	'$'

logoffailedmsg:
	db	'LOGOFF failed.'
	db	'$'

lclstack:
	dw	0c7c7h,0c7c7h,0c7c7h,0c7c7h,0c7c7h
	dw	0c7c7h,0c7c7h,0c7c7h,0c7c7h,0c7c7h
CCPStack:
	dw	$-$

	end	start
