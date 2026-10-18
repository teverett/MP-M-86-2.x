; C-callable assembler subroutines
; NOTE! These work only on Z-80s

;output a byte to a specified port
;returns (value & 0xff)
;out(port,value);
	public	out_
out_:
	lxi	h,2	
	dad	sp	;hl -> first arg
	push b	;save
	mov	c,m	;port addr
	inx	h
	inx	h	;point to output value
	mov	a,m	;get it
	db	0edh,79h	;out (c),a
	pop	b	;restore
	mov	l,a	;return output value
	mvi	h,0
	ora	a	;set flags	
	ret

;input a byte from a specified port
;in(port);
	public	in_
in_:
	lxi	h,2
	dad	sp		;hl -> port address on stack
	push b		;save
	mov c,m		;port addr -> c
	db	0edh,68h	;in l,(c)
	mvi	h,0		;clear high byte
	mov	a,l
	ora	a		;set flags
	pop	b		;restore
	ret

; set Z-80 interrupt register
; ireg(i);
	public	ireg_
ireg_:
	lxi	h,2
	dad	sp		;hl -> arg
	push b		;save
	mov	a,m		;arg -> a
	db	0edh,47h	;ld i,a
	pop	b
	ret

	public	im2_
im2_:
	db	0edh,5eh	; im 2 - interrupt mode 2
	ret

; iff_ is a flag which keeps track of the interrupt enable state.
; The Z-80 instructions for testing the hardware IFF are TOTALLY USELESS
; because they give wrong results when an interrupt occurs during
; the instruction. GRRRR!! I wasted two weeks on this one!

	public	iff_
	bss	iff_,1

	public	ei_

ei_:	di		; for atomicity
	mvi	a,1
	sta	iff_	; set flag first
	ei
	ret

	public di_

di_:	di
	xra	a
	sta	iff_	; clear flag last
	ret

	public halt_
	public eihalt_
eihalt_:
	call	ei_
halt_:	hlt
	ret

; disable interrupts and return previous interrupt state
	public disable_
disable_:
	di		; atomicity
	lxi h,0
	lda iff_	; get software copy of IFF
	ora a
	rz		; "zero" means interrupts were already disabled
	xra a
	sta iff_
	inr a		; clear Z flag 
	inx h		; hl = 1
	ret

; restore interrupt state
	public restore_
restore_:
	lxi	h,2
	dad	sp		; fetch arg
	mov	a,m
	ora	a		; test it
	jz	intoff
	call	ei_		; 1 -> enable interrupts
	ret
intoff:	call	di_		; 0 -> disable interrupts
	ret

; Return pointer to bottom of stack as set up by Aztec startup
	public	sbot_,sbot
sbot_:
	lhld	sbot		; Top of available memory
	mov	a,l
	ora	h		; set flags
	ret	
; return current stack pointer
	public getsp_
getsp_:
	lxi	h,2	; adjust for return value on stack
	dad	sp
	mov	a,l
	ora	h
	ret

	end
