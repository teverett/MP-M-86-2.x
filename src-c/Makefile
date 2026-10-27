# Builds src-c, the C port of src-assem/ (see docs/src-c.md): every
# <module>/*.c, kernel.elf (the kernel, the runtime that runs it as
# MP/M-86 in 32-bit protected mode, the C RSPs and gensys's low-memory
# image), the boot sector, and a bootable floppy with KERNEL.SYS and the
# C transients on it -- all from src-c's own sources, nothing from
# src-assem/'s build. The differential tests under tests/src-c/ check
# each port against its real assembled src-assem/ reference; they
# compile the tested .c files with host `cc` as ctypes libraries, and
# `make build` is a separate cross-compiler pass (`-c`, matching flags)
# so type errors show up independently of test failures.
#
# Usage (from this directory, src-c/):
#   make            # build, then test
#   make build      # compile every <module>/*.c to a .o
#   make test       # run every tests/src-c/test_*.py
#   make kernel.elf # the kernel (Multiboot, linked at 1MB)
#   make disk       # build/mpm86c.img, the bootable floppy
#   make run        # build the floppy and boot it under QEMU: the
#                    # console on this terminal (Ctrl-A X quits)
#   make run-elf    # the same, QEMU loading kernel.elf (-kernel)
#   make clean
#
# boot/ has no Makefile of its own -- folded in here (this Makefile
# builds its two files, boot.s/kernel_main.c, directly) rather than
# keeping a second, separate build for just that stub; see the
# "kernel.elf" rule's own header comment for what boot/'s files
# actually are.
#
# CC/LD/the common CFLAGS come from build.mk -- see that file for the
# real toolchain reasoning. `make build` below is compile-only (`-c`,
# no link), so `LD` isn't invoked by it -- only by `kernel.elf` below.
# `-I freestanding-include` supplies the one header this tree needs
# that isn't a standard *freestanding* one at all (`string.h`, used
# by a handful of files for `memcpy`/`memcmp`) as a declarations-only
# shim -- see that header's own comment.
include build.mk
CFLAGS += -std=c11 -I freestanding-include

# Each module directory under src-c/ is a separate symbol space with its
# own defs.h (src-c/kernel/defs.h vs src-c/loader/defs.h) -- see either
# one's own header comment for why they're not shared.
MODULES    := kernel loader
BUILD_DIR  := build
TEST_DIR   := ../tests/src-c
BOOT_DIR   := boot

ALL_SRCS := $(foreach m,$(MODULES),$(wildcard $(m)/*.c))
ALL_OBJS := $(foreach m,$(MODULES),$(patsubst $(m)/%.c,$(BUILD_DIR)/$(m)/%.o,$(wildcard $(m)/*.c)))
TESTS    := $(wildcard $(TEST_DIR)/test_*.py)
BOOT_OBJS := $(BUILD_DIR)/boot/boot.o $(BUILD_DIR)/boot/kernel_main.o
# kernel.elf links src-c/kernel/ only, not src-c/loader/ -- in the
# real system these are two separate executables (the loader runs
# first, then jumps to the kernel; they never coexist in one linked
# image either), so `make build`/`make test`'s own "MODULES" grouping
# doesn't apply here the same way. See that target's own comment
# below for what actually happened trying to link even just this.
KERNEL_OBJS := $(patsubst kernel/%.c,$(BUILD_DIR)/kernel/%.o,$(wildcard kernel/*.c))
STRING_SHIM_OBJ := $(BUILD_DIR)/freestanding-include/string_shim.o
# The runtime that runs the kernel as a real multitasking MP/M-86
# (rt/rt.c's header comment), and the C programs it runs (rsp/).
# gensys/ builds the low-memory image they start from.
RT_OBJS  := $(BUILD_DIR)/rt/rt.o $(BUILD_DIR)/rt/rt_asm.o
RSP_OBJS := $(patsubst rsp/%.c,$(BUILD_DIR)/rsp/%.o,$(wildcard rsp/*.c))
UTILS_OBJS := $(patsubst utils/%.c,$(BUILD_DIR)/utils/%.o,$(wildcard utils/*.c))
GENSYS_OBJS := $(patsubst gensys/%.c,$(BUILD_DIR)/gensys/%.o,$(wildcard gensys/*.c))

.PHONY: all build test boot run run-elf disk clean

all: build test

build: $(ALL_OBJS)
	@echo "Compiled $(words $(ALL_OBJS)) file(s) successfully."

define MODULE_RULE
$(BUILD_DIR)/$(1)/%.o: $(1)/%.c $(1)/defs.h | $(BUILD_DIR)/$(1)
	$$(CC) $$(CFLAGS) -I $(1) -c $$< -o $$@
endef
$(foreach m,$(MODULES),$(eval $(call MODULE_RULE,$(m))))

$(foreach m,$(MODULES),$(BUILD_DIR)/$(m)) $(BUILD_DIR)/boot $(BUILD_DIR)/freestanding-include \
		$(BUILD_DIR)/rt $(BUILD_DIR)/rsp $(BUILD_DIR)/gensys $(BUILD_DIR)/utils:
	mkdir -p $@


# rt/rt_syms.h and rsp/rsp_syms.h hold the /src-assem offsets src-c uses, as
# constants; tests/src-c/test_syms.py checks them against the asm
$(BUILD_DIR)/rt/rt.o: rt/rt.c rt/rt.h rt/rt_syms.h gensys/gensys.h kernel/defs.h | $(BUILD_DIR)/rt
	$(CC) $(CFLAGS) -I kernel -I rt -I gensys -c $< -o $@

$(BUILD_DIR)/rt/rt_asm.o: rt/rt_asm.s | $(BUILD_DIR)/rt
	$(CC) -c $< -o $@

$(BUILD_DIR)/rsp/%.o: rsp/%.c rsp/rsp.h rsp/rsp_syms.h rt/rt.h kernel/defs.h gensys/gensys.h | $(BUILD_DIR)/rsp
	$(CC) $(CFLAGS) -Wno-unterminated-string-initialization -I kernel -I rt -I rsp -I gensys -c $< -o $@

# src-c/utils: the PL/M utilities (src-assem/utils), as C programs
$(BUILD_DIR)/utils/%.o: utils/%.c utils/utils_syms.h rsp/rsp.h rt/rt.h kernel/defs.h | $(BUILD_DIR)/utils
	$(CC) $(CFLAGS) -Wno-unterminated-string-initialization -I kernel -I rt -I rsp -I utils -c $< -o $@

# src-c/gensys: the low-memory image, from src-c's own data
$(BUILD_DIR)/gensys/%.o: gensys/%.c gensys/gensys.h gensys/gensys_syms.h kernel/defs.h | $(BUILD_DIR)/gensys
	$(CC) $(CFLAGS) -I kernel -I gensys -c $< -o $@

# boot/'s own stub -- boot.s isn't a .c file (no CFLAGS, no defs.h,
# hence not folded into MODULE_RULE above). kernel_main.c isn't a
# ported module, but it calls into kernel/ and uses kernel/defs.h's
# real field offsets, hence `-I kernel`.
$(BUILD_DIR)/boot/boot.o: $(BOOT_DIR)/boot.s | $(BUILD_DIR)/boot
	$(CC) -c $< -o $@

# The floppy boot sector (16-bit, at 0000:7C00) and the kernel's flat
# image it loads to 1MB (boot/bootsect.s); src-c/tools/bootsect.py fills
# in where the image is on the disk.
$(BUILD_DIR)/boot/bootsect.o: $(BOOT_DIR)/bootsect.s | $(BUILD_DIR)/boot
	$(CC) -c $< -o $@

$(BUILD_DIR)/boot/bootsect.bin: $(BUILD_DIR)/boot/bootsect.o
	$(LD) -Ttext=0x7C00 -e start --oformat=binary -o $@ $<
	@test "$$(wc -c < $@ | tr -d ' ')" = 512 || { echo "$@ isn't 512 bytes"; rm -f $@; exit 1; }

$(BUILD_DIR)/boot/kernel.bin: kernel.elf | $(BUILD_DIR)/boot
	$(OBJCOPY) -O binary kernel.elf $@

# disk -- src-c's bootable MP/M-86 floppy: the boot sector, KERNEL.SYS,
# the C transients' CMD files and a sample SUB file (tools/mkdisk.py)
DISK := $(BUILD_DIR)/mpm86c.img

disk: $(DISK)

$(DISK): tools/mkdisk.py tools/bootsect.py rsp/rsp_syms.h $(BUILD_DIR)/boot/bootsect.bin \
		$(BUILD_DIR)/boot/kernel.bin $(BUILD_DIR)/rsp/submit.o $(BUILD_DIR)/rsp/random.o \
		$(UTILS_OBJS) utils/utils_syms.h
	$(PYTHON) tools/mkdisk.py -o $@

$(BUILD_DIR)/boot/kernel_main.o: $(BOOT_DIR)/kernel_main.c kernel/defs.h gensys/gensys.h | $(BUILD_DIR)/boot
	$(CC) $(CFLAGS) -I kernel -I gensys -c $< -o $@

$(STRING_SHIM_OBJ): freestanding-include/string_shim.c | $(BUILD_DIR)/freestanding-include
	$(CC) $(CFLAGS) -c $< -o $@

# boot (alias) -- just the stub, standalone, same as `make -C boot`.
boot: $(BOOT_OBJS)

# kernel.elf -- the real deliverable: boot/'s own stub linked
# together with every compiled src-c/kernel/ object (not
# src-c/loader/ -- see KERNEL_OBJS's own comment above) into one
# image via the real `ld` + linker.ld. This links successfully and
# boots under QEMU into the real ported XIOS (kernel_main.c calls
# xios86q.c's init_entry/xios_conout_entry with real inb/outb) and
# then sup_init on real memory -- but "bootable(ish)" because that
# still doesn't mean a working MP/M-86 kernel: nothing else is called
# yet (see docs/src-c.md's own "`src-c/boot/`" section for why: the
# other routines take a `uint8_t *mem` array + a 16-bit offset, built
# for host-side differential testing, not real pointers in a real
# 32-bit protected-mode address space).
#
# Getting the link itself to succeed needed real fixes, not just
# flag-tuning -- several genuinely different real routines in
# genuinely different real files had independently been ported to
# the *same* generic C name (because their real asm labels happen to
# match: `init`/`entry` for each module's own dispatcher, plus
# `conin_entry`/`conout_entry` for `CHARIO.CIO` vs `XIOS86Q.A86`'s own
# different routines), harmless for isolated per-file differential
# testing but a hard link failure once linked as one real unit --
# resolved by renaming the C symbols themselves (module-prefixed:
# `bdosif_init`, `cioif_entry`, `xios_conin_entry`, etc.), not a
# build-system `-D` patch; see docs/src-c.md's own "`kernel.elf`"
# section for the full list and reasoning. `memcpy`/`memcmp` also
# needed real bodies (not just declarations) once anything actually
# links against them -- freestanding-include/string_shim.c.
kernel.elf: $(BOOT_OBJS) $(KERNEL_OBJS) $(RT_OBJS) $(RSP_OBJS) $(UTILS_OBJS) $(GENSYS_OBJS) $(STRING_SHIM_OBJ) linker.ld
	$(LD) -T linker.ld -o kernel.elf $(BOOT_OBJS) $(KERNEL_OBJS) $(RT_OBJS) $(RSP_OBJS) $(UTILS_OBJS) $(GENSYS_OBJS) $(STRING_SHIM_OBJ)
	i686-elf-grub-file --is-x86-multiboot kernel.elf

# run -- boot src-c's floppy (`disk`) under QEMU, the way ./run.sh boots
# src-assem's: the BIOS boots it, src-c's boot sector loads KERNEL.SYS.
# The console is QEMU's serial port, on this terminal (Ctrl-A X quits,
# Ctrl-A C is QEMU's monitor); the list device (LPT1) prints to
# build/printer.txt. What MP/M writes stays on build/mpm86c.img until
# `make disk` rebuilds it.
run: $(DISK)
	$(QEMU) -drive file=$(DISK),format=raw,if=floppy -boot a -serial mon:stdio \
		-parallel file:$(BUILD_DIR)/printer.txt -display none -no-reboot

# run-elf -- the same, with QEMU loading kernel.elf itself (-kernel,
# Multiboot) instead of the boot sector: for debugging the kernel
run-elf: kernel.elf $(DISK)
	$(QEMU) -kernel kernel.elf -drive file=$(DISK),format=raw,if=floppy -serial mon:stdio \
		-parallel file:$(BUILD_DIR)/printer.txt -display none -no-reboot

# -- Run every differential test from the repo root, same as a developer
# invoking `python3 tests/src-c/test_foo.py` by hand -- each test already
# resolves its own paths from REPO_ROOT, not from the caller's cwd.
test:
	@status=0; \
	for t in $(TESTS); do \
		echo "== $$t =="; \
		(cd .. && $(PYTHON) tests/src-c/$$(basename $$t)) || status=1; \
	done; \
	exit $$status

clean:
	rm -rf $(BUILD_DIR) kernel.elf
