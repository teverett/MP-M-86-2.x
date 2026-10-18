	title	'LOGIN Transient Program'

;***************************************************************
;***************************************************************
;**                                                           **
;**       L O G I N   T r a n s i e n t   P r o g r a m       **
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
login	equ	64
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
	jz	versionerr ; jump if CP/Net not loaded
	lxi	d,passwordmsg
	lxi	h,buff
	mov	a,m	; get # chars in the command tail
	ora	a
	jz	dologin ; default login if empty command tail
	push	d	; save login msg address
	mov	c,a	; A = # chars in command tail
	xra	a
	mov	b,a	; B will accumulate master ID
scanblnks:
	inx	h
	mov	a,m
	cpi	' '
	jnz	pastblnks ; skip past leading blanks
	dcr	c
	jnz	scanblnks
	jmp	prelogin ; jump if command tail exhausted
pastblnks:
	cpi	'['
	jz	scanMstrID
	pop	d	; discard old login msg adr
	lxi	d,blankmsg
	push	d	; push new login msg adr
scanLftBrkt:
	mov	a,m
	cpi	'['
	jz	scanMstrID
	inx	d
	stax	d	;update the password
	inx	h
	dcr	c
	jnz	scanLftBrkt
	jmp	prelogin
scanMstrID:
	inx	h
	dcr	c
	jz	loginerr
	mov	a,m
	cpi	']'
	jz	prelogin
	sui	'0'
	cpi	10
	jc	updateID
	adi	('0'-'A'+10) and 0ffh
	cpi	16
	jnc	loginerr
updateID:
	push	psw
	mov	a,b
	add	a
	add	a
	add	a
	add	a
	mov	b,a	; accum * 16
	pop	psw
	add	b
	mov	b,a
	jmp	scanMstrID

prelogin:
	mov	a,b
	pop	d
	stax	d

dologin:
	mvi	c,login
	call	BDOS
	inr	a
	jnz	loginOK
	lxi	d,loginfailedmsg
	jmp	printmsg

versionerr:
	lxi	d,versionerrmsg
	jmp	printmsg

loginerr:
	lxi	d,loginerrmsg
printmsg:
	mvi	c,print
	call	BDOS
	jmp	Exit

loginOK:
	mvi	c,cfgtbl
	call	BDOS	; get configuration table address
	mov	a,m
	ori	active	; set active bit true
	mov	m,a
Exit:
	pop	h
	sphl		; restore CCP stack pointer
	ret

;
; Local Data Segment
;
passwordmsg:
	db	00h	; Master ID #
	db	'PASSWORD' ; password


versionerrmsg:
	db	'CP/Net is not loaded.'
	db	'$'

loginerrmsg:
	db	'Illegal LOGIN command.'
	db	'$'

loginfailedmsg:
	db	'LOGIN failed.'
	db	'$'

lclstack:
	dw	0c7c7h,0c7c7h,0c7c7h,0c7c7h,0c7c7h
	dw	0c7c7h,0c7c7h,0c7c7h,0c7c7h,0c7c7h
CCPStack:
	dw	$-$

blankmsg:
	db	00h	; Master ID #
	db	'        ' ; password
	end	start
