# Builds every source file in src/ that is currently known to compile
# and/or assemble cleanly with this repo's own from-scratch toolchain
# (tools/asm86/, tools/plmc.py). See docs/bugs.md for exactly which
# files are deliberately left out and why (genuine 1981 source defects,
# or a documented tools/asm86 limitation) -- src/ itself is never
# patched to make something build; only the lists below change as more
# of the source is validated.
#
# Usage:
#   make            # build everything in ALL_GOOD below
#   make partial    # also compile (but not assemble) the two known-
#                   # incomplete toolchain files
#   make clean

# Windows' python.org/GitHub Actions installs often expose only
# "python", not "python3" -- pick whichever actually exists so this
# Makefile works unmodified on Linux/macOS/Windows (see
# .github/workflows/build.yml's os matrix).
PYTHON  := $(shell command -v python3 2>/dev/null || command -v python)
PLMC    := $(PYTHON) tools/plmc.py
ASM86   := $(PYTHON) tools/asm86.py
STUB    := tools/plm/runtime_stub.A86
BUILD   := build

# --- .PLM utilities: compile with plmc, then concatenate the real
# mon1-4 interface stub and assemble the result (see
# tools/plm/README.md's "pl_mon1-pl_mon4 now have a real
# implementation"). All of src/utils/ and INS86.PLM assemble cleanly
# this way. ASS86.PLM/DIS86.PLM compile but don't fully assemble -- they
# also need DDT86 entry points that runtime_stub.A86 explicitly doesn't
# provide (see docs/bugs.md and tools/plm/README.md's "Known
# limitations").
PLM_UTILS := ABORT ATTACH CONS DIR DSKRST ED ERA ERAQ PIP REN SET SHOW \
             SPOOL SSPOOL STSCMD STSCOM STSRSP TOD TYPE
PLM_TOOLCHAIN_OK := INS86
PLM_TOOLCHAIN_PARTIAL := ASS86 DIS86

# --- RSPs: only ECHO.A86 assembles cleanly. CLOCK.A86/TMP.A86/
# SUBMIT.A86 are genuine, undisturbed 1981 source bugs (docs/bugs.md
# #1-3) and are deliberately not attempted here.
RSP_OK := ECHO

# --- Kernel: only CIO.A86/SYSDAT.A86 assemble cleanly so far.
# BDOS.A86/MEM.A86/RTM.A86/SUP.A86/XIOS.A86 are blocked by further
# genuine source defects and/or the documented short-jump-range
# trade-off (docs/bugs.md #4-#7) and are deliberately not attempted
# here.
KERNEL_OK := CIO SYSDAT

# --- Loader: only LDBDOS.A86 assembles cleanly so far. LDBIOS.A86,
# LDMPM.A86, and ROM.A86 are each blocked by further genuine source
# defects, some with no confident fix available (docs/bugs.md #8-#10),
# and are deliberately not attempted here.
LOADER_OK := LDBDOS

PLM_UTILS_BIN      := $(addprefix $(BUILD)/utils/,$(addsuffix .cseg.bin,$(PLM_UTILS)))
PLM_TOOLCHAIN_BIN   := $(addprefix $(BUILD)/toolchain/,$(addsuffix .cseg.bin,$(PLM_TOOLCHAIN_OK)))
PLM_PARTIAL_GEN     := $(addprefix $(BUILD)/toolchain/,$(addsuffix .gen.A86,$(PLM_TOOLCHAIN_PARTIAL)))
RSP_BIN             := $(addprefix $(BUILD)/rsp/,$(addsuffix .cseg.bin,$(RSP_OK)))
KERNEL_BIN          := $(addprefix $(BUILD)/kernel/,$(addsuffix .cseg.bin,$(KERNEL_OK)))
LOADER_BIN          := $(addprefix $(BUILD)/loader/,$(addsuffix .cseg.bin,$(LOADER_OK)))

ALL_GOOD := $(PLM_UTILS_BIN) $(PLM_TOOLCHAIN_BIN) $(RSP_BIN) $(KERNEL_BIN) $(LOADER_BIN)

.PHONY: all partial clean
all: $(ALL_GOOD)
	@echo "Built $(words $(ALL_GOOD)) file(s) successfully."

partial: all $(PLM_PARTIAL_GEN)
	@echo "Compiled (but did not assemble) $(words $(PLM_PARTIAL_GEN)) partial file(s)."

# -- .PLM utilities: compile -> concatenate real stub (dropping the
# compiler's own trailing END so the stub's END is the one that counts,
# see tools/README.md's "END is scoped to the file it appears in") ->
# assemble.
$(BUILD)/utils/%.gen.A86: src/utils/%.PLM | $(BUILD)/utils
	$(PLMC) $< -o $@

$(BUILD)/toolchain/%.gen.A86: src/toolchain/%.PLM | $(BUILD)/toolchain
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
$(BUILD)/rsp/%.cseg.bin: src/rsp/%.A86 | $(BUILD)/rsp
	$(ASM86) $< -o $(BUILD)/rsp/$*

$(BUILD)/kernel/%.cseg.bin: src/kernel/%.A86 | $(BUILD)/kernel
	$(ASM86) $< -o $(BUILD)/kernel/$*

$(BUILD)/loader/%.cseg.bin: src/loader/%.A86 | $(BUILD)/loader
	$(ASM86) $< -o $(BUILD)/loader/$*

$(BUILD)/utils $(BUILD)/toolchain $(BUILD)/rsp $(BUILD)/kernel $(BUILD)/loader:
	mkdir -p $@

clean:
	rm -rf $(BUILD)
