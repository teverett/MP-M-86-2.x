# Builds (syntax/type-checks) every /src-c/<module>/*.c C port file and
# runs the differential tests under tests/src-c/ that verify each one
# against its real assembled src/<module> reference (see docs/src-c.md).
#
# `kernel.elf` below links the boot stub and all kernel-port objects,
# and boots far enough to print a signon under QEMU. It does not call
# the ported routines, so it is not a working MP/M-86 kernel yet (see
# docs/src-c.md). The differential tests compile each tested .c file
# with host `cc` as a ctypes-loadable library; `make build` is a
# separate cross-compiler pass (`-c`, matching flags) so type errors
# show up independently of test failures.
#
# Usage (from this directory, src-c/):
#   make            # build, then test
#   make build      # compile every <module>/*.c to a .o
#   make test       # run every tests/src-c/test_*.py
#   make kernel.elf # build boot/'s own stub + every <module>/*.c,
#                    # then link the whole thing with linker.ld into
#                    # one real, bootable(ish) image -- see this
#                    # file's own "kernel.elf:" rule below for what
#                    # "bootable(ish)" means here
#   make run        # build kernel.elf, then boot it under QEMU with
#                    # serial output on stdout (Ctrl-C to stop; it
#                    # halts after signing on, so QEMU just idles)
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
BUILD_DIR  := obj
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

.PHONY: all build test boot run clean

all: build test

build: $(ALL_OBJS)
	@echo "Compiled $(words $(ALL_OBJS)) file(s) successfully."

define MODULE_RULE
$(BUILD_DIR)/$(1)/%.o: $(1)/%.c $(1)/defs.h | $(BUILD_DIR)/$(1)
	$$(CC) $$(CFLAGS) -I $(1) -c $$< -o $$@
endef
$(foreach m,$(MODULES),$(eval $(call MODULE_RULE,$(m))))

$(foreach m,$(MODULES),$(BUILD_DIR)/$(m)) $(BUILD_DIR)/boot $(BUILD_DIR)/freestanding-include:
	mkdir -p $@

# boot/'s own stub -- boot.s isn't a .c file (no CFLAGS, no defs.h,
# hence not folded into MODULE_RULE above) and kernel_main.c has no
# defs.h of its own either (it's not a ported module, see its own
# header comment).
$(BUILD_DIR)/boot/boot.o: $(BOOT_DIR)/boot.s | $(BUILD_DIR)/boot
	$(CC) -c $< -o $@

$(BUILD_DIR)/boot/kernel_main.o: $(BOOT_DIR)/kernel_main.c | $(BUILD_DIR)/boot
	$(CC) $(CFLAGS) -c $< -o $@

$(STRING_SHIM_OBJ): freestanding-include/string_shim.c | $(BUILD_DIR)/freestanding-include
	$(CC) $(CFLAGS) -c $< -o $@

# boot (alias) -- just the stub, standalone, same as `make -C boot`.
boot: $(BOOT_OBJS)

# kernel.elf -- the real deliverable: boot/'s own stub linked
# together with every compiled src-c/kernel/ object (not
# src-c/loader/ -- see KERNEL_OBJS's own comment above) into one
# image via the real `ld` + linker.ld. This links successfully and
# boots/signs on under QEMU -- but "bootable(ish)" because that still
# doesn't mean a working MP/M-86 kernel: nothing in kernel_main.c
# calls into any of these ported routines yet (see docs/src-c.md's
# own "`src-c/boot/`" section for why: every one of them takes a
# `uint8_t *mem` array + a 16-bit offset, built for host-side
# differential testing, not real pointers in a real 32-bit protected-
# mode address space).
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
kernel.elf: $(BOOT_OBJS) $(KERNEL_OBJS) $(STRING_SHIM_OBJ) linker.ld
	$(LD) -T linker.ld -o kernel.elf $(BOOT_OBJS) $(KERNEL_OBJS) $(STRING_SHIM_OBJ)
	i686-elf-grub-file --is-x86-multiboot kernel.elf

# run -- boot kernel.elf under QEMU with serial output on stdout.
# Halts forever right after signing on (by design, see
# boot/kernel_main.c), so this just idles once that's printed --
# Ctrl-C to stop.
run: kernel.elf
	$(QEMU) -kernel kernel.elf -serial stdio -display none -no-reboot -no-shutdown

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
