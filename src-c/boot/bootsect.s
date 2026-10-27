/*
 * bootsect.s -- src-c's floppy boot sector: loads kernel.elf's flat
 * image (kernel.bin, objcopy'd from it) and starts it in 32-bit
 * protected mode, the way QEMU's -kernel (Multiboot) would.
 *
 * The BIOS loads this sector to 0000:7C00 and jumps here with DL = the
 * boot drive. It:
 *   1. reads the kernel's sectors (kernel_lba, kernel_sectors: fields
 *      the disk builder fills in, src-c/tools/bootsect.py) one at a
 *      time with int 13h, retrying, to 2000:0 on (below 640K);
 *   2. turns the floppy motor off and enables A20 (port 92h);
 *   3. loads a flat GDT (code 08h, data 10h, base 0, 4G) and sets PE;
 *   4. copies the image to 1MB, where it's linked (linker.ld), and
 *      jumps to its entry (kernel_entry, the ELF entry: boot.s's
 *      _start, which zeroes .bss itself).
 * Interrupts stay off from step 3 on; the kernel sets up its own IDT.
 * Progress and errors go to the screen and to COM1 (BIOS int 14h), the
 * kernel's console.
 */
	.set LOAD_SEG,  0x2000     /* where the kernel's sectors are read to */
	.set KERNEL_PM, 0x100000   /* where it runs */
	.set SPT,       18         /* sectors per track */
	.set HEADS,     2

	.code16
	.section .text
	.global start
start:
	cli
	xor %ax, %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %ss
	mov $0x7C00, %sp
	sti
	mov %dl, boot_drive

	mov $0x00E3, %ax            /* COM1: 9600 8N1 */
	xor %dx, %dx
	int $0x14
	mov $msg_load, %si
	call print

	mov kernel_lba, %ax
	mov %ax, lba
	mov kernel_sectors, %cx
next_sector:
	push %cx
	mov lba, %ax                /* LBA -> CHS */
	xor %dx, %dx
	mov $SPT, %cx
	div %cx                     /* ax = track, dx = sector - 1 */
	mov %dl, %cl
	inc %cl                     /* cl = sector */
	mov %al, %dh
	and $HEADS - 1, %dh         /* dh = head */
	shr $1, %ax
	mov %al, %ch                /* ch = cylinder */
	mov boot_drive, %dl
	mov $3, %di                 /* tries */
retry:
	mov dest_seg, %es
	xor %bx, %bx
	mov $0x0201, %ax            /* read 1 sector to es:0 */
	push %dx
	push %cx
	int $0x13
	pop %cx
	pop %dx
	jnc read_ok
	xor %ax, %ax                /* reset the drive, try again */
	int $0x13
	dec %di
	jnz retry
	mov $msg_err, %si
	call print
1:	hlt
	jmp 1b
read_ok:
	incw lba
	addw $32, dest_seg          /* next 512 bytes: 32 paragraphs */
	pop %cx
	loop next_sector

	mov $0x3F2, %dx             /* floppy motor off */
	mov $0x0C, %al
	out %al, %dx
	in $0x92, %al               /* A20 on (fast A20) */
	or $2, %al
	and $0xFE, %al
	out %al, $0x92

	cli
	lgdt gdt_desc
	mov %cr0, %eax
	or $1, %eax
	mov %eax, %cr0
	ljmp $0x08, $pm

	.code32
pm:
	mov $0x10, %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs
	mov %ax, %ss
	mov $0x9F000, %esp
	mov $LOAD_SEG * 16, %esi    /* the image to 1MB */
	mov $KERNEL_PM, %edi
	movzwl kernel_sectors, %ecx
	shl $7, %ecx                /* 128 dwords a sector */
	cld
	rep movsl
	jmp *kernel_entry

	.code16
/* print the 0-terminated string at ds:si on the screen and COM1 */
print:
	lodsb
	or %al, %al
	jz 2f
	mov $0x0E, %ah
	xor %bx, %bx
	push %ax
	int $0x10
	pop %ax
	mov $0x01, %ah
	xor %dx, %dx
	int $0x14
	jmp print
2:	ret

msg_load:	.asciz "\r\nsrc-c boot: loading the kernel\r\n"
msg_err:	.asciz "src-c boot: disk read error\r\n"

boot_drive:	.byte 0
lba:		.word 0
dest_seg:	.word LOAD_SEG

	.balign 8
gdt:
	.quad 0
	.quad 0x00CF9A000000FFFF    /* 08h: code, base 0, 4G, 32-bit */
	.quad 0x00CF92000000FFFF    /* 10h: data */
gdt_desc:
	.word 3 * 8 - 1
	.long gdt

/* filled in by src-c/tools/bootsect.py: where the kernel is on the disk
 * and where to start it */
	.org 0x1F0
	.ascii "KRNL"
kernel_lba:	.word 0
kernel_sectors:	.word 0
kernel_entry:	.long 0
	.org 0x1FE
	.byte 0x55, 0xAA
