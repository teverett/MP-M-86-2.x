/*
 * linker.ld -- the real `ld` link script for src-c/Makefile's own
 * `kernel.elf` target: links src-c/boot/'s own boot stub (boot.s's
 * multiboot header/`_start`, kernel_main.c's hand-off into the XIOS) together
 * with every compiled src-c/kernel/ object (not src-c/loader/ -- see
 * that Makefile's own "kernel.elf:" rule for why) into one image,
 * same role `tools/build_mpmsys.py`/`tools/build_mpmldr.py` play for
 * /src's own (entirely custom-tooled) boot path, but via a real
 * cross-linker instead of hand-rolled Python. Lives here, not under
 * boot/ -- boot/ has no Makefile of its own at all, src-c/Makefile
 * builds its two files directly.
 *
 * Standard multiboot layout: loaded at 1MB (0x100000), comfortably
 * past real-mode/BIOS-reserved low memory and the traditional "hole"
 * around 0xA0000-0x100000 -- the conventional multiboot load address
 * for exactly that reason. The multiboot header has to land in the
 * first 8KB of the file for the bootloader's own scan to find it,
 * hence its own explicit section first.
 */
ENTRY(_start)

SECTIONS
{
	. = 1M;

	.multiboot BLOCK(4K) : ALIGN(4K)
	{
		*(.multiboot)
	}

	.text BLOCK(4K) : ALIGN(4K)
	{
		*(.text)
	}

	.rodata BLOCK(4K) : ALIGN(4K)
	{
		*(.rodata*)
	}

	.data BLOCK(4K) : ALIGN(4K)
	{
		*(.data)
	}

	.bss BLOCK(4K) : ALIGN(4K)
	{
		*(COMMON)
		*(.bss)
	}
}
