/*
 * boot.s -- the one hand-written asm stub in this whole pipeline.
 *
 * This is NOT a port of anything in /src -- MP/M-86's own real boot
 * chain (ROM.A86 -> LDBIOS.A86/LDMPM.A86 -> the kernel) is a 16-bit
 * real-mode sequence with no C compiler involved anywhere, and stays
 * exactly as-is; it isn't touched by this file. This is instead the
 * proof-of-pipeline for a *second*, parallel boot path: a real
 * `ld`-linked, 32-bit protected-mode image built from C via
 * i686-elf-gcc, multiboot-loadable directly by QEMU's own `-kernel`
 * flag (no GRUB/ISO step needed for this). See this directory's own
 * README.md for why 32-bit protected mode is a deliberately
 * different execution model than MP/M-86's real one, and what that
 * means for ever wiring the real ported kernel logic in here.
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
	mov $stack_top, %esp
	call kernel_main
	/* kernel_main never returns, but if it somehow did: stop for
	 * good rather than fall into whatever garbage follows. */
	cli
hang:
	hlt
	jmp hang
