# Builds (syntax/type-checks) every /src-c/<module>/*.c C port file and
# runs the differential tests under tests/src-c/ that verify each one
# against its real assembled src/<module> reference (see docs/src-c.md).
#
# There's no linked kernel/loader binary here yet -- the goal is a real
# bootable MP/M-86 kernel, but each routine is first verified in
# isolation (see docs/src-c.md's own "Status" section) before any
# link/boot step is built. Each routine is only ever driven by its own
# differential test, which compiles the same .c file itself (via
# `cc ... -shared`) and calls into it with ctypes; `make build` below
# is a separate, lighter compile-only pass (`-c`, matching flags) so a
# type error shows up on its own instead of inside a test failure.
#
# Usage (from this directory, src-c/):
#   make            # build, then test
#   make build      # compile every <module>/*.c to a .o
#   make test       # run every tests/src-c/test_*.py
#   make clean

# Windows' python.org/GitHub Actions installs often expose only
# "python", not "python3" -- same reasoning as the top-level Makefile.
PYTHON := $(shell command -v python3 2>/dev/null || command -v python)
CC     := cc
CFLAGS := -std=c11 -Wall -Wextra -Werror

# Each module directory under src-c/ is a separate symbol space with its
# own defs.h (src-c/kernel/defs.h vs src-c/loader/defs.h) -- see either
# one's own header comment for why they're not shared.
MODULES    := kernel loader
BUILD_DIR  := obj
TEST_DIR   := ../tests/src-c

ALL_SRCS := $(foreach m,$(MODULES),$(wildcard $(m)/*.c))
ALL_OBJS := $(foreach m,$(MODULES),$(patsubst $(m)/%.c,$(BUILD_DIR)/$(m)/%.o,$(wildcard $(m)/*.c)))
TESTS    := $(wildcard $(TEST_DIR)/test_*.py)

.PHONY: all build test clean

all: build test

build: $(ALL_OBJS)
	@echo "Compiled $(words $(ALL_OBJS)) file(s) successfully."

define MODULE_RULE
$(BUILD_DIR)/$(1)/%.o: $(1)/%.c $(1)/defs.h | $(BUILD_DIR)/$(1)
	$$(CC) $$(CFLAGS) -I $(1) -c $$< -o $$@
endef
$(foreach m,$(MODULES),$(eval $(call MODULE_RULE,$(m))))

$(foreach m,$(MODULES),$(BUILD_DIR)/$(m)):
	mkdir -p $@

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
	rm -rf $(BUILD_DIR)
