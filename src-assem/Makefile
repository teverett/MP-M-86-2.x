# Builds every source file in this directory (src-assem/) that is currently
# known to compile and/or assemble cleanly with this repo's own
# from-scratch toolchain (../tools/asm86/, ../tools/plmc.py). See
# ../docs/bugs.md for exactly which files are deliberately left out and
# why (genuine 1981 source defects, or a documented ../tools/asm86
# limitation) -- src-assem/ itself is never patched to make something build;
# only the lists below change as more of the source is validated.
#
# Usage (from the repo root: `make -C src-assem <target>`; or `cd src-assem &&
# make <target>`):
#   make            # build everything in ALL_GOOD below (DDT86.CMD too)
#   make disk       # build build/disk/mpm86q.img, a bootable floppy
#                   # image
#   make run        # boot that image in QEMU (builds it first if needed)
#   make clean

# Windows' python.org/GitHub Actions installs often expose only
# "python", not "python3" -- pick whichever actually exists so this
# Makefile works unmodified on Linux/macOS/Windows (see
# .github/workflows/build.yml's os matrix).
#
# This Makefile lives in src-assem/ and is invoked as `make -C src-assem` (or from
# within src-assem/ directly), so its own source paths below are relative to
# src-assem/ itself, with the build output in src-assem/build/; tools/, outside
# src-assem/, is reached via ../.
PYTHON  := $(shell command -v python3 2>/dev/null || command -v python)
PLMC    := $(PYTHON) ../tools/plmc.py
ASM86   := $(PYTHON) ../tools/asm86.py
STUB    := ../tools/plm/runtime_stub.A86
PLMC_SRC := $(wildcard ../tools/plm/*.py)   # rebuild the utilities when the compiler changes
BUILD   := build
QEMU    := qemu-system-i386

# --- .PLM utilities: compile with plmc, then concatenate the real
# mon1-4 interface stub and assemble the result (see
# tools/plm/README.md's "pl_mon1-pl_mon4 now have a real
# implementation"). All of utils/ and INS86.PLM assemble cleanly
# this way. ASS86.PLM/DIS86.PLM (with INS86.PLM and ASMTAB.PLM) are
# DDT86's overlays, not programs: ../tools/build_ddt86.py links them into
# DDT86.CMD with toolchain/DDT86.A86, as DRI's toolchain/DDT86.SUB does
# (below).
PLM_UTILS := ABORT ATTACH CONS DIR DSKRST ED ERA ERAQ PIP REN SET SHOW \
             SPOOL SSPOOL STSCMD STSCOM TOD TYPE
# STSRSP.PLM is an RSP, not a program: it's built as MPMSTAT.RSP below
# (plmc --rsp), the name its own header comment gives it.
PLM_TOOLCHAIN_OK := INS86

# --- RSPs: ECHO.A86 always assembled cleanly. CLOCK.A86/TMP.A86/
# SUBMIT.A86 each had a genuine 1981 source bug (docs/bugs.md #1-3) --
# all 3 are now FIXED directly in these files (needed so /src-c could
# assemble the real thing as a differential-test reference; see
# docs/src-c.md's own entries for each), so all 4 RSPs assemble
# cleanly now.
RSP_OK := ECHO CLOCK TMP SUBMIT RANDOM
# (RANDOM.A86 is DRI's sample random-access program from the same disk,
# orig/mpm862sr/05 -- a .CMD like SUBMIT, built by its MKALL.SUB.)

# --- Kernel: CIO.A86/SYSDAT.A86/BDOS.A86/MEM.A86/RTM.A86/SUP.A86 all
# assemble cleanly now that the name-mismatch bugs are fixed (docs/bugs.md
# #4/#5) and tools/asm86 does real jump-size relaxation (#6). XIOS.A86's
# own last blocker (docs/bugs.md #7's "dp_base" -- the same added-
# underscore bug class as everywhere else on that page, the real label
# is "dpbase", declared in SINGLES.LIB, which this file already
# includes) is now fixed too, so it assembles cleanly as well -- it's
# still not part of MPM.bin itself (XIOS86Q.A86 below replaces it
# there, targeting QEMU instead of the Intel SBC 204/534 hardware
# XIOS.A86's own drivers are written for), just validated here like
# every other file in this list.
KERNEL_OK := CIO SYSDAT BDOS MEM RTM SUP XIOS

# --- XIOS86Q.A86: a new, from-scratch XIOS targeting QEMU (serial
# console + floppy disk), replacing the real XIOS.A86's Intel SBC
# 204/534-specific drivers that have no QEMU equivalent. Assembles
# cleanly, but -- unlike everything else in this Makefile -- has never
# actually run: there's no way to execute 8086 code or drive QEMU from
# here. See the "KNOWN LIMITATIONS" comment at the top of the file
# itself before relying on it. Its own disk parameter block (dpb0) was
# originally a flagged placeholder with the wrong SPT; it's since been
# recomputed for the real target geometry and kept identical to
# loader/LDBIOS86Q.A86's own DPB, since both read the same on-disk
# filesystem.
KERNEL_NEW := XIOS86Q

# --- Loader: all four real loader modules assemble cleanly (docs/bugs.md
# #8/#9/#10, each turning out to be the same name-mismatch bug class
# once looked at more closely, plus jump-size relaxation).
LOADER_OK := LDBDOS LDBIOS LDMPM ROM

# --- LDBIOS86Q.A86: a new, from-scratch LDBIOS targeting QEMU, replacing
# the real LDBIOS.A86's iSBC 86/12-specific drivers for MPMLDR.bin's own
# use the same way XIOS86Q.A86 replaces XIOS.A86 for MPM.bin -- see that
# file's own header comment for the rationale and limitations.
LOADER_NEW := LDBIOS86Q

# --- MPMLDR.bin: LDMPM/LDBDOS/LDBIOS86Q combined into one loadable image
# by ../tools/build_mpmldr.py, DRI's own "pip the three modules' hex
# together, then GENCMD" build step (orig/mpm862sr/04/MKALL.SUB)
# re-done without GENCMD -- see that script's own docstring and
# docs/build.md for why it lays the pieces out differently than DRI did,
# and LDBIOS86Q.A86's own header comment for why it replaces LDBIOS.A86
# here.
MPMLDR_BIN := $(BUILD)/loader/MPMLDR.bin

# --- MPM.bin{.cseg,.dseg}: SUP/RTM/MEM/CIO/BDOS/XIOS86Q combined into
# one kernel image by ../tools/build_mpmsys.py -- the kernel-side
# counterpart of MPMLDR_BIN above, replacing GENSYS-86 (whose source
# was never part of this distribution -- see that script's own
# docstring and docs/build.md). Both output files come from one
# invocation of that script; tracked via a stamp file (portable across
# make versions -- GNU Make's own multiple-output "&:" grouped-target
# syntax needs 4.3+, not guaranteed on every OS this builds on).
MPMSYS_STAMP := $(BUILD)/kernel/.mpmsys.stamp
MPMSYS_BIN := $(BUILD)/kernel/MPM.bin.cseg $(BUILD)/kernel/MPM.bin.dseg

# --- ROM86Q.bin: a new, from-scratch 512-byte PC boot sector replacing
# 04/ROM.A86's EPROM bootstrap -- QEMU's SeaBIOS plays the role the
# real iSBC 86/12's EPROM played, handing off to this file instead.
# Uses BIOS INT 13h for disk I/O (confirmed via AskUserQuestion), not
# the from-scratch NEC765 driver XIOS86Q.A86/LDBIOS86Q.A86 carry --
# see ROM86Q.A86's own header comment. Built by
# ../tools/build_bootsect.py, which also has to slice the real 512-byte
# sector out of asm86's own padded output -- see that script's own
# docstring for why (the same `org`-padding behavior as docs/bugs.md
# #11).
BOOTSECT_BIN := $(BUILD)/loader/ROM86Q.bin

# --- mpm86q.img: the final bootable QEMU floppy image, combining
# BOOTSECT_BIN + MPMLDR_BIN + MPMSYS_BIN -- the three pieces above --
# plus a real on-disk `MPM.SYS` file (built by hand to the CP/M
# directory format `../tools/build_disk.py` itself documents; none of the
# three scripts above produce one). See that script's own docstring
# for the exact disk layout. Not added to ALL_GOOD: it re-invokes the
# three build scripts above itself (so it's never stale), making it
# redundant with `make all`'s own dependency graph; build it directly
# with `make disk` instead.
DISK_IMG := $(BUILD)/disk/mpm86q.img

PLM_UTILS_BIN      := $(addprefix $(BUILD)/utils/,$(addsuffix .cseg.bin,$(PLM_UTILS)))
PLM_TOOLCHAIN_BIN   := $(addprefix $(BUILD)/toolchain/,$(addsuffix .cseg.bin,$(PLM_TOOLCHAIN_OK)))
# DDT86.CMD, DRI's debugger: DDT86.A86 and its two PL/M overlays (the
# disassembler, INS86 + DIS86; the assembler, ASMTAB + ASS86), linked and
# laid out as an 8080-model CMD by ../tools/build_ddt86.py (DRI's
# toolchain/DDT86.SUB; that script's docstring has the layout)
DDT86_CMD := $(BUILD)/toolchain/DDT86.CMD
DDT86_SRC := toolchain/DDT86.A86 toolchain/INS86.PLM toolchain/DIS86.PLM toolchain/ASMTAB.PLM \
             toolchain/ASS86.PLM toolchain/OPTAB.DAT toolchain/OPTAB.LIT ../tools/build_ddt86.py \
             ../tools/plm/link.py
RSP_BIN             := $(addprefix $(BUILD)/rsp/,$(addsuffix .cseg.bin,$(RSP_OK))) $(BUILD)/rsp/MPMSTAT.cseg.bin
KERNEL_BIN          := $(addprefix $(BUILD)/kernel/,$(addsuffix .cseg.bin,$(KERNEL_OK)))
KERNEL_NEW_BIN      := $(addprefix $(BUILD)/kernel/,$(addsuffix .cseg.bin,$(KERNEL_NEW)))
LOADER_BIN          := $(addprefix $(BUILD)/loader/,$(addsuffix .cseg.bin,$(LOADER_OK)))
LOADER_NEW_BIN      := $(addprefix $(BUILD)/loader/,$(addsuffix .cseg.bin,$(LOADER_NEW)))

ALL_GOOD := $(PLM_UTILS_BIN) $(PLM_TOOLCHAIN_BIN) $(DDT86_CMD) $(RSP_BIN) $(KERNEL_BIN) $(KERNEL_NEW_BIN) $(LOADER_BIN) $(LOADER_NEW_BIN) $(MPMLDR_BIN) $(MPMSYS_BIN) $(BOOTSECT_BIN)

.PHONY: all disk run clean
all: $(ALL_GOOD)
	@echo "Built $(words $(ALL_GOOD)) file(s) successfully."

$(DDT86_CMD): $(DDT86_SRC) $(PLMC_SRC) | $(BUILD)/toolchain
	$(PYTHON) ../tools/build_ddt86.py -o $@

disk: $(DISK_IMG)

# -- Boot mpm86q.img in QEMU. ROM86Q.A86's own boot-sector messages go
# through BIOS INT 10h (the default graphical display QEMU opens), but
# everything from MPMLDR/MPM.SYS onward talks to the 8250 UART
# XIOS86Q.A86/LDBIOS86Q.A86 drive directly (COM1) -- `-serial mon:stdio`
# sends that to this terminal instead of needing a second window.
run: $(DISK_IMG)
	$(QEMU) -fda $(DISK_IMG) -serial mon:stdio

# -- .PLM utilities: compile -> concatenate real stub (dropping the
# compiler's own trailing END so the stub's END is the one that counts,
# see tools/README.md's "END is scoped to the file it appears in") ->
# assemble.
$(BUILD)/utils/%.gen.A86: utils/%.PLM $(PLMC_SRC) | $(BUILD)/utils
	$(PLMC) $< -o $@

$(BUILD)/toolchain/%.gen.A86: toolchain/%.PLM $(PLMC_SRC) | $(BUILD)/toolchain
	$(PLMC) $< -o $@

$(BUILD)/utils/%.combined.A86: $(BUILD)/utils/%.gen.A86 $(STUB)
	grep -v '^[[:space:]]*end[[:space:]]*$$' $< > $@
	cat $(STUB) >> $@

$(BUILD)/toolchain/%.combined.A86: $(BUILD)/toolchain/%.gen.A86 $(STUB)
	grep -v '^[[:space:]]*end[[:space:]]*$$' $< > $@
	cat $(STUB) >> $@

$(BUILD)/utils/%.cseg.bin: $(BUILD)/utils/%.combined.A86
	$(ASM86) $< -o $(BUILD)/utils/$*

$(BUILD)/toolchain/%.cseg.bin: $(BUILD)/toolchain/%.combined.A86
	$(ASM86) $< -o $(BUILD)/toolchain/$*

# -- RSPs and kernel modules: assembled directly, no PL/M stage.
$(BUILD)/rsp/%.cseg.bin: rsp/%.A86 | $(BUILD)/rsp
	$(ASM86) $< -o $(BUILD)/rsp/$*

# -- MPMSTAT.RSP: STSRSP.PLM compiled as an RSP (RSPASM.A86's shell,
# PD named MPMSTAT), then the same stub and assembly as the utilities.
$(BUILD)/rsp/MPMSTAT.gen.A86: utils/STSRSP.PLM utils/STSCOM.PLM $(PLMC_SRC) | $(BUILD)/rsp
	$(PLMC) $< --rsp MPMSTAT -o $@

$(BUILD)/rsp/MPMSTAT.combined.A86: $(BUILD)/rsp/MPMSTAT.gen.A86 $(STUB)
	grep -v '^[[:space:]]*end[[:space:]]*$$' $< > $@
	cat $(STUB) >> $@

$(BUILD)/rsp/MPMSTAT.cseg.bin: $(BUILD)/rsp/MPMSTAT.combined.A86
	$(ASM86) $< -o $(BUILD)/rsp/MPMSTAT

$(BUILD)/kernel/%.cseg.bin: kernel/%.A86 | $(BUILD)/kernel
	$(ASM86) $< -o $(BUILD)/kernel/$*

$(BUILD)/loader/%.cseg.bin: loader/%.A86 | $(BUILD)/loader
	$(ASM86) $< -o $(BUILD)/loader/$*

# -- MPMLDR.bin depends on all three loader modules' own sources (not
# just their .cseg.bin -- the script re-assembles LDMPM itself if it
# patches its offset constants) plus the build script.
$(MPMLDR_BIN): loader/LDMPM.A86 loader/LDBDOS.A86 loader/LDBIOS86Q.A86 ../tools/build_mpmldr.py | $(BUILD)/loader
	$(PYTHON) ../tools/build_mpmldr.py -o $@

# -- Both MPM.bin{.cseg,.dseg} come from one invocation of
# build_mpmsys.py; the stamp file is what actually carries the
# dependency, so two output files never trigger two separate runs.
# Every kernel source, not just the seven top-level .A86 files: those
# INCLUDE the .SUP/.RTM/.CIO/.BDO/.MEM/.DEF/... files, and an edit to one
# of them (COMMAND.SUP, say) otherwise left MPM.bin stale.
KERNEL_SRC := $(wildcard kernel/*)

$(MPMSYS_STAMP): $(KERNEL_SRC) ../tools/build_mpmsys.py rsp/TMP.A86 rsp/CLOCK.A86 rsp/ECHO.A86 rsp/CACHE.RSP utils/STSRSP.PLM utils/STSCOM.PLM $(PLMC_SRC) | $(BUILD)/kernel
	$(PYTHON) ../tools/build_mpmsys.py -o $(BUILD)/kernel/MPM.bin
	touch $@

# -- ROM86Q.bin: one source file, one build script, no multi-module
# combining needed.
$(BOOTSECT_BIN): loader/ROM86Q.A86 ../tools/build_bootsect.py | $(BUILD)/loader
	$(PYTHON) ../tools/build_bootsect.py -o $@

# -- mpm86q.img: ../tools/build_disk.py re-invokes build_bootsect.py/
# build_mpmldr.py/build_mpmsys.py itself, so its own real dependencies
# are those three scripts' own sources, not their outputs.
$(DISK_IMG): loader/ROM86Q.A86 ../tools/build_bootsect.py loader/LDMPM.A86 loader/LDBDOS.A86 loader/LDBIOS86Q.A86 ../tools/build_mpmldr.py $(KERNEL_SRC) ../tools/build_mpmsys.py ../tools/build_disk.py $(PLM_UTILS_BIN) $(RSP_BIN) rsp/CACHE.RSP $(DDT86_SRC) | $(BUILD)/disk
	$(PYTHON) ../tools/build_disk.py -o $@

$(BUILD)/kernel/MPM.bin.cseg $(BUILD)/kernel/MPM.bin.dseg: $(MPMSYS_STAMP)

$(BUILD)/utils $(BUILD)/toolchain $(BUILD)/rsp $(BUILD)/kernel $(BUILD)/loader $(BUILD)/disk:
	mkdir -p $@

clean:
	rm -rf $(BUILD)
