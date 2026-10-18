; Interrupt vectors for Xerox 820

; WARNING WARNING DANGER Will Robinson!
; These routines do not save either the long or floating point
; accumulators. You will need to add code to this if either
; longs or floats are ever used in the code
	public iff_

; local stack for interrupt service routines

stksize	equ	2048	; probably too much
	dseg
	public istack
	public spsave
	bss	spsave,2	; user's saved stack pointer
	bss	istack,stksize
	cseg

; CTC channel 0 interrupt handler
	public	_ctcint_
	public	ctcint_
_ctcint_:
	db	0edh,73h	; ld (spsave),sp
	dw	spsave

	lxi	sp,istack+stksize

	db	0ddh,0e5h	; push ix
	db	0fdh,0e5h	; push iy
	push	h
	push	d
	push	b
	push	psw
	xra	a
	sta	iff_
	call	ctcint_
	jmp	intret

; SIO transmit interrupt handler #0
	public	_ltx0int_
	public	ltxint_
_ltx0int_:
	db	0edh,73h	; ld (spsave),sp
	dw	spsave

	lxi	sp,istack+stksize

	db	0ddh,0e5h	; push ix
	db	0fdh,0e5h	; push iy
	push	h
	push	d
	push	b
	push	psw
	xra	a
	sta	iff_
	lxi	h,0		; line number 0
	push	h
	call	ltxint_
	pop	h
	jmp	intret

; SIO transmit interrupt handler #1
	public	_ltx1int_
	public	ltxint_
_ltx1int_:
	db	0edh,73h	; ld (spsave),sp
	dw	spsave

	lxi	sp,istack+stksize

	db	0ddh,0e5h	; push ix
	db	0fdh,0e5h	; push iy
	push	h
	push	d
	push	b
	push	psw
	xra	a
	sta	iff_
	lxi	h,1		; line number 1
	push	h
	call	ltxint_
	pop	h
	jmp	intret

; SIO receiver interrupt handler #0
	public	_lrx0int_
	public	lrxint_
_lrx0int_:
	db	0edh,73h	; ld (spsave),sp
	dw	spsave

	lxi	sp,istack+stksize

	db	0ddh,0e5h	; push ix
	db	0fdh,0e5h	; push iy
	push	h
	push	d
	push	b
	push	psw
	xra	a
	sta	iff_
	lxi	h,0		; line number 0
	push	h
	call	lrxint_
	pop	h
	jmp	intret

; SIO receiver interrupt handler #1
	public	_lrx1int_
	public	lrxint_
_lrx1int_:
	db	0edh,73h	; ld (spsave),sp
	dw	spsave

	lxi	sp,istack+stksize

	db	0ddh,0e5h	; push ix
	db	0fdh,0e5h	; push iy
	push	h
	push	d
	push	b
	push	psw
	xra	a
	sta	iff_
	lxi	h,1		; line number 1
	push	h
	call	lrxint_
	pop	h
	jmp	intret

; Serial transmit interrupt handler
	public	_stxint_
	public	stxint_
_stxint_:
	db	0edh,73h	; ld (spsave),sp
	dw	spsave

	lxi	sp,istack+stksize

	db	0ddh,0e5h	; push ix
	db	0fdh,0e5h	; push iy
	push	h
	push	d
	push	b
	push	psw
	xra	a
	sta	iff_
	call	stxint_
	jmp	intret

; Serial receive interrupt handler
	public	_srxint_
	public	srxint_
_srxint_:
	db	0edh,73h	; ld (spsave),sp
	dw	spsave

	lxi	sp,istack+stksize

	db	0ddh,0e5h	; push ix
	db	0fdh,0e5h	; push iy
	push	h
	push	d
	push	b
	push	psw
	xra	a
	sta	iff_
	call	srxint_
	jmp	intret

; publical/Status interrupt handler #0
	public	_lex0int_
	public	lexint_
_lex0int_:
	db	0edh,73h	; ld (spsave),sp
	dw	spsave

	lxi	sp,istack+stksize

	db	0ddh,0e5h	; push ix
	db	0fdh,0e5h	; push iy
	push	h
	push	d
	push	b
	push	psw
	xra	a
	sta	iff_
	lxi	h,0		; line number 0
	push	h
	call	lexint_
	pop	h
	jmp	intret

; publical/Status interrupt handler #1
	public	_lex1int_
	public	lexint_
_lex1int_:
	db	0edh,73h	; ld (spsave),sp
	dw	spsave

	lxi	sp,istack+stksize

	db	0ddh,0e5h	; push ix
	db	0fdh,0e5h	; push iy
	push	h
	push	d
	push	b
	push	psw
	xra	a
	sta	iff_
	lxi	h,1		; line number 1
	push	h
	call	lexint_
	pop	h
	jmp	intret

; Special status interrupt handler #0
	public	_lsp0int_
	public	lspint_
_lsp0int_:
	db	0edh,73h	; ld (spsave),sp
	dw	spsave

	lxi	sp,istack+stksize

	db	0ddh,0e5h	; push ix
	db	0fdh,0e5h	; push iy
	push	h
	push	d
	push	b
	push	psw
	xra	a
	sta	iff_
	lxi	h,0		; line number 0
	push	h
	call	lspint_
	pop	h
	jmp	intret

; Special status interrupt handler #1
	public	_lsp1int_
	public	lspint_
_lsp1int_:
	db	0edh,73h	; ld (spsave),sp
	dw	spsave

	lxi	sp,istack+stksize

	db	0ddh,0e5h	; push ix
	db	0fdh,0e5h	; push iy
	push	h
	push	d
	push	b
	push	psw
	xra	a
	sta	iff_
	lxi	h,1		; line number 1
	push	h
	call	lspint_
	pop	h
	jmp	intret

	public	_noint_
	public	noint_
_noint_:
	db	0edh,73h	; ld (spsave),sp
	dw	spsave

	lxi	sp,istack+stksize

	db	0ddh,0e5h	; push ix
	db	0fdh,0e5h	; push iy
	push	h
	push	d
	push	b
	push	psw
	xra	a
	sta	iff_
	call	noint_
intret:
	mvi	a,1
	sta	iff_
	pop	psw
	pop	b
	pop	d
	pop	h
	db	0fdh,0e1h	; pop iy
	db	0ddh,0e1h	; pop ix

	db	0edh,7bh	; ld sp,(spsave)
	dw	spsave

	ei
	db	0edh,4dh	; reti

	public putchar_
crtout equ	0f00ch		; 820 monitor display routine
putchar_:
	lxi	h,2
	dad	sp	; hl -> character
	mov	a,m
	jmp	crtout

; Interrupt register saving sequence
; Taken from page 2-81, Zilog Microprocessor Applications Reference book
; Here in case I decide to use it
save:	xthl			; save hl
	push	d		;      de
	push	b		;      bc
	push	psw		;      af
	db	0ddh,0e5h	; push ix
	db	0fdh,0e5h	; push iy
	xra	a
	sta	iff_
	call	go		; save pc
	mvi	a,1
	sta	iff_
	db	0fdh,0e1h	; pop iy
	db	0ddh,0e1h	; pop ix
	pop	psw		;     af
	pop	b		;     bc
	pop	d		;     de
	pop	h		;     hl
	ei
	reti

go:	pchl	

	end
