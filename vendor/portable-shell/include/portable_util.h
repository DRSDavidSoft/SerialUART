#ifndef PORTABLE_UTIL_H
#define PORTABLE_UTIL_H
#include <stddef.h>
/* Array counts, never byte sizes. C callers must pass an array, not a pointer.
 * C++ rejects pointers through array-reference template deduction. */
#ifdef __cplusplus
extern "C++" {
namespace portable_shell {
template <typename T, size_t N>
constexpr size_t count_of(const T (&)[N]) noexcept { return N; }
}
}
#define PS_COUNT_OF(array) (::portable_shell::count_of(array))
#else
#define PS_COUNT_OF(array) (sizeof(array) / sizeof((array)[0]))
#endif
/* Literal concatenation deliberately rejects pointer expressions. */
#define PS_LITERAL_LENGTH(text) (sizeof("" text) - 1u)
#endif
