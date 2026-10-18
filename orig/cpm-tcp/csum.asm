; Compute 1's complement sum on block of BIG-ENDIAN 16-bit integers
; (Note that this byte-swaps each element).
; Written as assembler assist for Internet checksumming routine

; called from C as
; unsigned short
; csum(blk,cnt)
; unsigned short *blk;	/* Pointer to block */
; unsigned short cnt;	/* Number of words (NOT bytes) */
	public	csum_
csum_:
	push 	b
	db	0ddh,0e5h	; push ix
	lxi	h,6
	dad	sp	; hl -> blk
	mov	e,m
	inx	h
	mov	d,m	; de = blk
	inx	h
	mov	c,m
	inx	h
	mov	b,m	; bc = cnt
	lxi	h,0	; clear sum
	xra	a	; clear carry
	push	d
	db	0ddh,0e1h	; pop ix; ix = blk

csloop:	mov	a,b
	ora	c
	jz	csdone

	db	0ddh,56h,0	; ld d,(ix+0)
	db	0ddh,5eh,1	; ld e,(ix+1); de = ntohs(*blk)
	dad	d		; hl += de
	jnc	nocarry
	inx	h		; end-around carry
nocarry:
	db	0ddh,23h	; inc ix; blk++
	db	0ddh,23h	; inc ix
	dcx	b		; cnt--
	jmp	csloop

csdone:	db	0ddh,0e1h	; pop ix
	pop	b
	mov	a,l
	ora	h
	ret
