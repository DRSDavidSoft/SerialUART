#ifndef PORTABLE_TERMINAL_H
#define PORTABLE_TERMINAL_H
#include "portable_util.h"
/* MIT, Copyright (c) 2026 AtomicDeploy and PortableShell contributors.
 * Shared byte-only terminal primitives. Source lineage is recorded in the
 * repository integration documents and donor inventory.
 * Header-only C99: no allocator, system calls, or required library functions.
 * Callbacks consume bytes synchronously. One owner per state object. */
#include <stddef.h>
#include <stdarg.h>
#include <limits.h>
#ifdef PS_TERMINAL_USE_LIBC
#include <string.h>
#endif
typedef void (*ps_terminal_write)(void *, const unsigned char *, size_t);
/* log10(2) < 1/3: this bounds decimal digits plus sign and terminator. */
enum { PS_TERMINAL_INTEGER_BYTES = sizeof(unsigned) * CHAR_BIT / 3 + 3 };
/* Optional shift/subtract decimal division for MCUs without hardware divide.
 * Keeps compiler division helpers out of the small header-only profile. The
 * default lets the host compiler choose its native implementation. */
#ifndef PS_TERMINAL_SOFT_DIV10
#define PS_TERMINAL_SOFT_DIV10 0
#endif
static inline unsigned ps_div10(unsigned value, unsigned *remainder) {
#if PS_TERMINAL_SOFT_DIV10
    unsigned quotient = 0, rest = 0, bits = (unsigned)(sizeof(unsigned) * CHAR_BIT);
    while (bits) {
        --bits; rest = (rest << 1) | ((value >> bits) & 1u);
        quotient <<= 1;
        if (rest >= 10) { rest -= 10; quotient |= 1u; }
    }
    *remainder = rest; return quotient;
#else
    *remainder = value % 10; return value / 10;
#endif
}
/* Small bounded renderer formatter, not a printf replacement: only %d, %u, %02u,
 * %s and %% are supported. Returns the full required byte count. */
static inline size_t ps_terminal_format(char *out, size_t capacity, const char *format, ...) {
    size_t used = 0, i, n;
    va_list args;
    va_start(args, format);
    while (*format) {
        const char *text = 0;
        char digits[PS_TERMINAL_INTEGER_BYTES], literal[2] = {*format++, 0};
        if (literal[0] == '%') {
            unsigned width = 0, value;
            if (format[0] == '0' && format[1] == '2') {
                width = 2;
                format += 2;
            }
            if (*format == 'u' || *format == 'd') {
                unsigned negative = 0;
                if (*format++ == 'd') {
                    int signed_value = va_arg(args, int);
                    negative = signed_value < 0;
                    value = negative ? 0u - (unsigned)signed_value : (unsigned)signed_value;
                } else value = va_arg(args, unsigned);
                n = 0;
                do {
                    unsigned remainder;
                    value = ps_div10(value, &remainder);
                    digits[n++] = (char)('0' + remainder);
                } while (value);
                while (n < width)
                    digits[n++] = '0';
                for (i = 0; i < n / 2; ++i) {
                    char c = digits[i];
                    digits[i] = digits[n - 1 - i];
                    digits[n - 1 - i] = c;
                }
                if (negative) {
                    for (i = n; i; --i) digits[i] = digits[i - 1];
                    digits[0] = '-'; ++n;
                }
                digits[n] = 0;
                text = digits;
            } else if (*format == 's') {
                ++format;
                text = va_arg(args, const char *);
                if (!text)
                    text = "";
            } else if (*format == '%') {
                ++format;
                text = "%";
            } else
                text = "?";
        } else
            text = literal;
        while (*text) {
            if (capacity && used + 1 < capacity)
                out[used] = *text;
            ++used;
            ++text;
        }
    }
    va_end(args);
    if (capacity)
        out[used < capacity ? used : capacity - 1] = 0;
    return used;
}
/* Trusted application output: '%' substitutes the signed number each time.
 * This is not printf syntax; '%%' deliberately substitutes twice. Caller owns
 * terminal-control sanitization, just as for the raw byte writer. */
static inline void ps_terminal_number_pattern(ps_terminal_write write, void *context,
                                              const char *pattern, int value) {
    char number[PS_TERMINAL_INTEGER_BYTES];
    size_t n;
    if (!write || !pattern) return;
    n = ps_terminal_format(number, sizeof number, "%d", value);
    while (*pattern) {
        if (*pattern == '%') write(context, (const unsigned char *)number, n);
        else write(context, (const unsigned char *)pattern, 1);
        ++pattern;
    }
}
static inline const char *ps_text_find(const char *text, const char *part) {
    size_t i, j;
    for (i = 0; text[i]; ++i) {
        for (j = 0; part[j] && text[i + j] == part[j]; ++j) {
        }
        if (!part[j])
            return text + i;
    }
    return *part ? 0 : text;
}
static inline size_t ps_text_length(const char *s) {
    size_t n = 0;
    if (s)
        while (s[n])
            ++n;
    return n;
}
static inline void ps_secure_clear(void *p, size_t n) {
    volatile unsigned char *b = (volatile unsigned char *)p;
    while (n--)
        *b++ = 0;
}
static inline void ps_bytes_copy(void *d, const void *s, size_t n) {
#ifdef PS_TERMINAL_USE_LIBC
    if (n)
        memcpy(d, s, n);
#else
    unsigned char *to = (unsigned char *)d;
    const unsigned char *from = (const unsigned char *)s;
    while (n--)
        *to++ = *from++;
#endif
}
static inline void ps_bytes_move(void *d, const void *s, size_t n) {
#ifdef PS_TERMINAL_USE_LIBC
    if (n)
        memmove(d, s, n);
#else
    unsigned char *to = (unsigned char *)d;
    const unsigned char *from = (const unsigned char *)s;
    size_t i;
    if (d == s)
        return;
    for (i = 1; i < n; ++i)
        if (to == from + i) {
            while (n) {
                --n;
                to[n] = from[n];
            }
            return;
        }
    ps_bytes_copy(d, s, n);
#endif
}
static inline unsigned ps_ascii_fold(unsigned c) {
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}
static inline int ps_text_equal(const char *a, const char *b) {
    while (*a && *a == *b) {
        ++a;
        ++b;
    }
    return *a == *b;
}
static inline int ps_text_prefix(const char *a, const char *b, size_t n) {
    size_t i;
    for (i = 0; i < n; ++i)
        if (!a[i] || ps_ascii_fold((unsigned char)a[i]) != ps_ascii_fold((unsigned char)b[i]))
            return 0;
    return 1;
}
static inline size_t ps_decimal(char *out, unsigned value) {
    char digits[10];
    size_t n = 0, i;
    do {
        unsigned remainder;
        value = ps_div10(value, &remainder);
        digits[n++] = (char)('0' + remainder);
    } while (value);
    for (i = 0; i < n; ++i)
        out[i] = digits[n - 1 - i];
    out[n] = 0;
    return n;
}
#ifndef PS_MOUSE
#define PS_MOUSE 1
#endif
/* SGR mouse payload following CSI: <button;column;row M/m. One-based cells.
   Parse atomically: malformed/overflowing packets never become UI actions. */
typedef struct { unsigned button, column, row, release; } ps_mouse_event;
static inline int ps_mouse_parse(const char *p, ps_mouse_event *event) {
    unsigned values[3],i;
    if (*p++!='<') return 0;
    for(i=0;i<3;++i){
        unsigned n=0,digits=0;
        while(*p>='0'&&*p<='9'){
            if(++digits>5 || n>6553 || (n==6553&&*p>'5'))return 0;
            n=n*10+(unsigned)(*p-'0');
            ++p;
        }
        if(!digits)return 0;
        values[i]=n;
        if(i<2&&*p++!=';')return 0;
    }
    if(!values[1]||!values[2]||(*p!='M'&&*p!='m')||p[1])return 0;
    event->button=values[0];event->column=values[1];event->row=values[2];event->release=*p=='m';
    return 1;
}

/* Consume one complete escape sequence, including private string payloads.
 * Input is NUL-terminated or bounded by maximum; never scan beyond that bound. */
static inline size_t ps_escape_span(const char *s, size_t maximum) {
    size_t i = 1;
    unsigned kind;
    if (!maximum || (unsigned char)s[0] != 27)
        return 0;
    if (i >= maximum || !s[i])
        return i;
    kind = (unsigned char)s[i++];
    if (kind == '[') {
        while (i < maximum && s[i]) {
            unsigned c = (unsigned char)s[i++];
            if (c >= 0x40 && c <= 0x7e)
                break;
        }
    } else if (kind == ']' || kind == 'P' || kind == 'X' || kind == '^' || kind == '_') {
        while (i < maximum && s[i]) {
            if (s[i] == '\a') {
                ++i;
                break;
            }
            if (s[i] == 27 && i + 1 < maximum && s[i + 1] == '\\') {
                i += 2;
                break;
            }
            ++i;
        }
    } else if (kind >= 0x20 && kind <= 0x2f) {
        while (i < maximum && s[i] >= 0x20 && s[i] <= 0x2f)
            ++i;
        if (i < maximum && s[i] >= 0x30 && s[i] <= 0x7e)
            ++i;
    }
    return i;
}
static inline size_t ps_terminal_text(char *out, size_t capacity, const char *s, size_t maximum) {
    size_t n = 0, i = 0;
    if (!out || !capacity)
        return 0;
    while (s && i < maximum && s[i] && n + 1 < capacity) {
        unsigned c = (unsigned char)s[i];
        if (c == 27) {
            i += ps_escape_span(s + i, maximum - i);
            continue;
        }
        ++i;
        if (c == '\r' || c == '\n' || c == '\t')
            out[n++] = ' ';
        else if (c >= 32)
            out[n++] = (char)(c < 127 ? c : '?');
    }
    out[n] = 0;
    return n;
}
/* Conservative default; callers still classify commands/properties before
 * retention. Substring matching deliberately favors rejecting a benign line. */
static inline int ps_history_safe(const char *text, size_t maximum) {
    static const char *const forbidden[] = {"password", "passwd",     "passphrase",    "token",
                                            "secret",   "credential", "authorization", "bearer",
                                            "psk",      "auth",       "login",         "username"};
    size_t i, j, k;
    int nonspace = 0;
    if (!text)
        return 0;
    for (i = 0; i < maximum && text[i]; ++i) {
        unsigned c = (unsigned char)text[i];
        if (c < 32 || c == 127 || c == '?' || c == '@')
            return 0;
        if (c != ' ')
            nonspace = 1;
        for (j = 0; j < PS_COUNT_OF(forbidden); ++j) {
            for (k = 0; forbidden[j][k] && i + k < maximum && text[i + k]; ++k)
                if (ps_ascii_fold((unsigned char)text[i + k]) != (unsigned char)forbidden[j][k])
                    break;
            if (!forbidden[j][k])
                return 0;
        }
    }
    return i < maximum && nonspace;
}

enum { PS_TN_BINARY = 1, PS_TN_CONTROLS = 2 };
enum { PS_TN_NONE = 0, PS_TN_DATA = 1, PS_TN_RESIZE = 2, PS_TN_TERMINAL = 3 };
typedef struct {
    unsigned char state, verb, length, overflow, flags;
    unsigned char us[5], peer[5], reject_us[32], reject_peer[32], sub[64];
    unsigned columns, rows;
    char terminal[41];
} ps_telnet;
static inline int ps_tn_slot(unsigned option) {
    return option == 0    ? 0
           : option == 1  ? 1
           : option == 3  ? 2
           : option == 24 ? 3
           : option == 31 ? 4
                          : -1;
}
static inline void ps_tn_command(ps_terminal_write write, void *context, unsigned verb,
                                 unsigned option) {
    unsigned char b[3] = {255, (unsigned char)verb, (unsigned char)option};
    write(context, b, sizeof b);
}
static inline void ps_telnet_init(ps_telnet *s, unsigned flags, unsigned columns, unsigned rows) {
    ps_secure_clear(s, sizeof(*s));
    s->flags = (unsigned char)flags;
    s->columns = columns ? columns : 80;
    s->rows = rows ? rows : 24;
    ps_bytes_copy(s->terminal, "unknown", sizeof "unknown");
}
static inline int ps_telnet_ansi(const ps_telnet *s) {
    return !(ps_text_length(s->terminal) == 4 && ps_text_prefix(s->terminal, "dumb", 4));
}
static inline void ps_telnet_greet(ps_telnet *s, ps_terminal_write write, void *context) {
    ps_tn_command(write, context, 251, 1);
    s->us[1] = 2;
    ps_tn_command(write, context, 251, 3);
    s->us[2] = 2;
    ps_tn_command(write, context, 253, 3);
    s->peer[2] = 2;
    ps_tn_command(write, context, 253, 31);
    s->peer[4] = 2;
    ps_tn_command(write, context, 253, 24);
    s->peer[3] = 2;
    if (s->flags & PS_TN_BINARY) {
        ps_tn_command(write, context, 251, 0);
        s->us[0] = 2;
        ps_tn_command(write, context, 253, 0);
        s->peer[0] = 2;
    }
}
static inline void ps_tn_negotiate(ps_telnet *s, unsigned verb, unsigned opt,
                                   ps_terminal_write write, void *ctx) {
    int local = verb == 253 || verb == 254, positive = verb == 253 || verb == 251,
        slot = ps_tn_slot(opt);
    unsigned yes = local ? 251 : 253, no = local ? 252 : 254;
    unsigned char *rejected = local ? s->reject_us : s->reject_peer, *state;
    int allowed = local ? (opt == 1 || opt == 3) : (opt == 3 || opt == 24 || opt == 31);
    if (opt == 0 && (s->flags & PS_TN_BINARY))
        allowed = 1;
    if (!allowed) {
        if (positive) {
            if (!(rejected[opt / 8] & (1u << (opt % 8))))
                ps_tn_command(write, ctx, no, opt);
            rejected[opt / 8] |= (unsigned char)(1u << (opt % 8));
        } else
            rejected[opt / 8] &= (unsigned char)~(1u << (opt % 8));
        return;
    }
    state = local ? s->us + slot : s->peer + slot;
    if (positive) {
        if (*state != 1) {
            if (*state != 2)
                ps_tn_command(write, ctx, yes, opt);
            *state = 1;
            if (!local && opt == 24) {
                const unsigned char b[] = {255, 250, 24, 1, 255, 240};
                write(ctx, b, sizeof(b));
            }
        }
    } else {
        if (*state == 1)
            ps_tn_command(write, ctx, no, opt);
        *state = 0;
    }
}
static inline void ps_tn_append(ps_telnet *s, unsigned c) {
    if (s->length < sizeof(s->sub))
        s->sub[s->length++] = (unsigned char)c;
    else
        s->overflow = 1;
}
static inline int ps_tn_sub(ps_telnet *s) {
    unsigned i, w, h;
    if (s->overflow || !s->length)
        return PS_TN_NONE;
    if (s->sub[0] == 31 && s->length == 5 && s->peer[4] == 1) {
        w = ((unsigned)s->sub[1] << 8) | s->sub[2];
        h = ((unsigned)s->sub[3] << 8) | s->sub[4];
        if (w)
            s->columns = w > 240 ? 240 : w;
        if (h)
            s->rows = h > 100 ? 100 : h;
        return PS_TN_RESIZE;
    }
    if (s->sub[0] == 24 && s->length > 2 && s->length <= 42 && s->sub[1] == 0 && s->peer[3] == 1) {
        for (i = 2; i < s->length; ++i)
            if (s->sub[i] < 32 || s->sub[i] > 126)
                return PS_TN_NONE;
        ps_bytes_copy(s->terminal, s->sub + 2, s->length - 2);
        s->terminal[s->length - 2] = 0;
        return PS_TN_TERMINAL;
    }
    return PS_TN_NONE;
}
/* One byte at a time; DATA writes the decoded byte. Other events never dispatch
 * user input. Malformed/oversized subnegotiations stay quarantined until IAC SE. */
static inline int ps_telnet_feed(ps_telnet *s, unsigned char c, ps_terminal_write write, void *ctx,
                                 unsigned char *decoded) {
    switch (s->state) {
    case 0:
        if (c == 255)
            s->state = 1;
        else {
            *decoded = c;
            return PS_TN_DATA;
        }
        break;
    case 1:
        if (c == 255) {
            s->state = 0;
            *decoded = c;
            return PS_TN_DATA;
        }
        if (c >= 251 && c <= 254) {
            s->verb = c;
            s->state = 2;
        } else if (c == 250) {
            s->length = s->overflow = 0;
            s->state = 3;
        } else {
            s->state = 0;
            if ((s->flags & PS_TN_CONTROLS) && (c == 244 || c == 247 || c == 248)) {
                *decoded = (unsigned char)(c == 244 ? 3 : c == 247 ? 127 : 21);
                return PS_TN_DATA;
            }
        }
        break;
    case 2:
        ps_tn_negotiate(s, s->verb, c, write, ctx);
        s->state = 0;
        break;
    case 3:
        if (c == 255)
            s->state = 4;
        else
            ps_tn_append(s, c);
        break;
    case 4:
        if (c == 240) {
            s->state = 0;
            return ps_tn_sub(s);
        } else {
            if (c == 255)
                ps_tn_append(s, c);
            else
                s->overflow = 1;
            s->state = 3;
        }
        break;
    default:
        s->state = 0;
        break;
    }
    return PS_TN_NONE;
}
#endif
