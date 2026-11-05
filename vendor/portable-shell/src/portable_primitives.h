#ifndef PS_PORTABLE_PRIMITIVES_H
#define PS_PORTABLE_PRIMITIVES_H
/* Private wrappers retain the core's null/zero-count/bounded-string semantics. */
#ifdef PS_USE_LIBC
#include <string.h>
#else
#include "portable_memory.h"
#endif
static unsigned length(const char *p) {
#ifdef PS_USE_LIBC
    return p ? (unsigned)strlen(p) : 0;
#else
    unsigned n = 0;
    if (p)
        while (p[n])
            ++n;
    return n;
#endif
}
static void zero(void *p, unsigned n) {
    if (n) {
#ifdef PS_USE_LIBC
        memset(p, 0, n);
#else
        ps_portable_fill(p, 0, n);
#endif
    }
}
static void copy(void *d, const void *s, unsigned n) {
    if (n && d != s) {
#ifdef PS_USE_LIBC
        memcpy(d, s, n);
#else
        ps_portable_copy(d, s, n);
#endif
    }
}
static void move(void *d, const void *s, unsigned n) {
    if (n && d != s) {
#ifdef PS_USE_LIBC
        memmove(d, s, n);
#else
        ps_portable_move(d, s, n);
#endif
    }
}
static int equal(const char *a, const char *b) {
#ifdef PS_USE_LIBC
    return strcmp(a, b) == 0;
#else
    while (*a && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
#endif
}
static int prefix(const char *a, const char *b) {
    while (*b)
        if (*a++ != *b++)
            return 0;
    return 1;
}
static int contains(const char *a, const char *b) {
#ifdef PS_USE_LIBC
    return strstr(a, b) != 0;
#else
    if (!*b)
        return 1;
    while (*a) {
        if (prefix(a, b))
            return 1;
        ++a;
    }
    return 0;
#endif
}
static void str(char *d, const char *s, unsigned cap) {
    unsigned i = 0;
    if (!cap)
        return;
    /* Not strncpy: always terminates and tolerates a null source. */
    while (s && s[i] && i + 1 < cap) {
        d[i] = s[i];
        ++i;
    }
    d[i] = 0;
}
#endif
