/*
 * rt_asm.s -- the runtime's machine-level pieces: the process context
 * switch, the interrupt entry stubs, and the MP/M image the boot copies
 * into low memory. See rt.c's header comment.
 */

	.section .text

/*
 * rt_ctx_switch(uint32_t *save_esp, uint32_t new_esp)
 * Save the callee-saved registers and EFLAGS (so each process keeps
 * its own IF) on the current stack, store ESP in *save_esp, then resume
 * the stack at new_esp the same way. A fresh stack is laid out by
 * rt_ctx_init (rt.c) to "resume" into rt_ctx_start.
 */
	.global rt_ctx_switch
	.type rt_ctx_switch, @function
rt_ctx_switch:
	mov 4(%esp), %eax
	mov 8(%esp), %edx
	pushfl
	push %ebp
	push %ebx
	push %esi
	push %edi
	mov %esp, (%eax)
	mov %edx, %esp
	pop %edi
	pop %esi
	pop %ebx
	pop %ebp
	popfl
	ret

/* A fresh context's first "return": EBX holds the PD (rt_ctx_init). */
	.global rt_ctx_start
	.type rt_ctx_start, @function
rt_ctx_start:
	push %ebx
	call rt_proc_main
	/* rt_proc_main never returns */
1:	hlt
	jmp 1b

/*
 * Interrupt stubs. IRQ0 (the PIT, remapped to vector 20h) saves every
 * general register on the interrupted process's own stack and calls
 * rt_irq0, which may switch processes; the stub finishes when this
 * process is dispatched again. CPU exceptions report and halt.
 */
	.global rt_irq0_stub
rt_irq0_stub:
	pushal
	cld
	call rt_irq0
	popal
	iret

	.global rt_spurious_stub
rt_spurious_stub:
	push %eax
	mov $0x20, %al
	out %al, $0x20
	pop %eax
	iret

.macro FAULT n
	.global rt_fault_\n
rt_fault_\n:
	.if (\n == 8) || (\n >= 10 && \n <= 14) || (\n == 17)
	.else
	push $0
	.endif
	push $\n
	jmp rt_fault_common
.endm

	FAULT 0
	FAULT 1
	FAULT 2
	FAULT 3
	FAULT 4
	FAULT 5
	FAULT 6
	FAULT 7
	FAULT 8
	FAULT 9
	FAULT 10
	FAULT 11
	FAULT 12
	FAULT 13
	FAULT 14
	FAULT 15
	FAULT 16
	FAULT 17
	FAULT 18
	FAULT 19

rt_fault_common:
	pushal
	push %esp
	call rt_fault
1:	cli
	hlt
	jmp 1b

/*
 * The MP/M-86 image /src builds (build/kernel/MPM.bin.{cseg,dseg}):
 * the combined code segment -- every module's code, then the baked-in
 * RSPs' code and data segments -- and the SYSDAT data segment, laid out
 * by tools/build_mpmsys.py (the GENSYS stand-in). The kernel's code is
 * 8086 code nothing here runs; the image is what puts SYSDAT (its
 * pools, lists, queue descriptors, CCBs), the module table and the RSP
 * chain where the real boot does.
 */
	.section .rodata
	.global rt_mpm_cseg, rt_mpm_cseg_end, rt_mpm_dseg, rt_mpm_dseg_end
	.balign 16
rt_mpm_cseg:
	.incbin "../build/kernel/MPM.bin.cseg"
rt_mpm_cseg_end:
	.balign 16
rt_mpm_dseg:
	.incbin "../build/kernel/MPM.bin.dseg"
rt_mpm_dseg_end:
