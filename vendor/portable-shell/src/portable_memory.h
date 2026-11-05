#ifndef PS_PORTABLE_MEMORY_H
#define PS_PORTABLE_MEMORY_H
/* Compiler-provided C99 type header; no OS or runtime functions. */
#include <stddef.h>
/* Only the standard-symbol runtime uses volatile accesses to prevent recursive
   library-call synthesis. Core-local bodies remain eligible for optimization. */
#ifdef PS_MEMORY_VOLATILE
#define PS_BYTE_ACCESS volatile
#else
#define PS_BYTE_ACCESS
#endif
static inline void *ps_portable_copy(void *dst, const void *src, size_t n) {
    PS_BYTE_ACCESS unsigned char *d = (PS_BYTE_ACCESS unsigned char *)dst;
    const PS_BYTE_ACCESS unsigned char *s = (const PS_BYTE_ACCESS unsigned char *)src;
    void *result = dst;
    if (dst != src)
        while (n--)
            *d++ = *s++;
    return result;
}
static inline void *ps_portable_fill(void *dst, int value, size_t n) {
    PS_BYTE_ACCESS unsigned char *d = (PS_BYTE_ACCESS unsigned char *)dst;
    void *result = dst;
    while (n--)
        *d++ = (unsigned char)value;
    return result;
}
static inline void *ps_portable_move(void *dst, const void *src, size_t n) {
    PS_BYTE_ACCESS unsigned char *d = (PS_BYTE_ACCESS unsigned char *)dst;
    const PS_BYTE_ACCESS unsigned char *s = (const PS_BYTE_ACCESS unsigned char *)src;
    size_t i;
    if (dst == src || !n)
        return dst;
    /* Equality is defined across objects. Pointer ordering/subtraction is not.
       A destination inside the source range requires backward copying. */
    for (i = 1; i < n; ++i)
        if (d == s + i) {
            while (n) {
                --n;
                d[n] = s[n];
            }
            return dst;
        }
    while (n--)
        *d++ = *s++;
    return dst;
}
static inline int ps_portable_compare(const void *a, const void *b, size_t n) {
    const PS_BYTE_ACCESS unsigned char *x = (const PS_BYTE_ACCESS unsigned char *)a;
    const PS_BYTE_ACCESS unsigned char *y = (const PS_BYTE_ACCESS unsigned char *)b;
    while (n--) {
        unsigned char u = *x++, v = *y++;
        if (u != v)
            return u < v ? -1 : 1;
    }
    return 0;
}
#undef PS_BYTE_ACCESS
#endif
