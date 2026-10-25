/*
 * string.h -- minimal shim for src-c/Makefile's own cross-compile
 * check (i686-elf-gcc). <string.h> isn't one of the handful of
 * headers the C standard requires a freestanding implementation to
 * ship (stdint.h/stddef.h/stdarg.h/etc. are; string.h isn't), and
 * this bare "elf" cross toolchain has no libc at all -- so plain
 * `#include <string.h>` fails outright (file not found), not just a
 * missing-symbol problem.
 *
 * Declares only the handful of functions src-c/kernel's own
 * submit.c/tmp.c/xios86q.c and src-c/loader's own ldbios86q.c
 * actually call (`memcpy`/`memcmp`) -- just enough for `make build`'s
 * own compile-only (`-c`, no link) type-check pass to succeed; there
 * are no definitions anywhere, so this would need real ones (a few
 * lines each) before any future link step could use these files.
 */
#ifndef SRC_C_FREESTANDING_STRING_H
#define SRC_C_FREESTANDING_STRING_H

#include <stddef.h>

void *memcpy(void *dest, const void *src, size_t n);
int memcmp(const void *a, const void *b, size_t n);

#endif
