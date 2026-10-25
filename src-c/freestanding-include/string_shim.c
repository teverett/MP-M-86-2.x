/*
 * string_shim.c -- real bodies for the two functions
 * freestanding-include/string.h declares (`memcpy`/`memcmp`), needed
 * now that src-c/Makefile's own `kernel.elf` target actually links
 * (not just compile-checks) -- a declaration alone satisfies `-c`,
 * but `ld` needs a real definition somewhere once anything calls
 * them. Plain byte-at-a-time bodies, no attempt at the usual
 * libc-style word-at-a-time/`rep movsb`-style optimization: this
 * exists only to make `kernel.elf` link at all, not as a
 * differential-tested port of anything in /src.
 */
#include <stddef.h>

void *memcpy(void *dest, const void *src, size_t n) {
    unsigned char *d = dest;
    const unsigned char *s = src;
    for (size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
    return dest;
}

int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *pa = a;
    const unsigned char *pb = b;
    for (size_t i = 0; i < n; i++) {
        if (pa[i] != pb[i]) {
            return pa[i] < pb[i] ? -1 : 1;
        }
    }
    return 0;
}
