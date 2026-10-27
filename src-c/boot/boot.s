/*
 * boot.s -- kernel.elf's entry, _start: zero .bss, set up a stack,
 * call kernel_main (kernel_main.c).
 *
 * Two loaders start it, both in 32-bit protected mode with flat
 * segments: src-c's own boot sector (bootsect.s), which loads the
 * file's bytes from the floppy -- hence _start zeroing .bss itself --
 * and QEMU's -kernel (`make run-elf`), through the Multiboot header
 * below. Neither is MP/M-86's own 16-bit boot chain (src-assem's
 * ROM86Q.A86 -> MPMLDR -> MPM.SYS); docs/src-c.md, "Running the port",
 * has how the C kernel runs MP/M from here.
 *
 * Multiboot1 requires a small header (magic/flags/checksum) within
 * the first 8KB of the image, found by the bootloader scanning for
 * the magic value -- done here as 3 plain words, not a struct, since
 * nothing else in this file needs to reference it by name.
 */
	.set MB_MAGIC, 0x1BADB002
	.set MB_FLAGS, 0x0
	.set MB_CHECKSUM, -(MB_MAGIC + MB_FLAGS)

	/* "a" (SHF_ALLOC) is required explicitly -- gas doesn't infer
	 * it for a section name it doesn't already recognize, and a
	 * non-ALLOC section gets shoved to the end of the file by ld
	 * regardless of this script's own section ordering below,
	 * which silently pushes the header past the first-8KB window
	 * multiboot bootloaders scan (found by `i686-elf-grub-file
	 * --is-x86-multiboot` failing, then confirmed via objdump -h
	 * showing .multiboot's own file offset landing after .text's). */
	.section .multiboot, "a"
	.align 4
	.long MB_MAGIC
	.long MB_FLAGS
	.long MB_CHECKSUM

	.section .bss
	.align 16
stack_bottom:
	.skip 16384	/* 16KB -- plenty for this stub; kernel_main never recurses */
stack_top:

	.section .text
	.global _start
	.type _start, @function
_start:
	/* zero .bss: a Multiboot loader does, src-c's boot sector
	 * (bootsect.s) loads only the file's bytes */
	mov $__bss_start, %edi
	mov $__bss_end, %ecx
	sub %edi, %ecx
	xor %eax, %eax
	cld
	rep stosb
	mov $stack_top, %esp
	call kernel_main
	/* kernel_main never returns, but if it somehow did: stop for
	 * good rather than fall into whatever garbage follows. */
	cli
hang:
	hlt
	jmp hang
