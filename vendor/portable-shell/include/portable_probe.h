#ifndef PORTABLE_PROBE_H
#define PORTABLE_PROBE_H
#include "portable_terminal.h"
/* Terminal-size query with a bounded, nonblocking event decoder.
 * A host supplies monotonic uint32_t ticks, including rollover, and calls tick
 * even without input. Never waits or reads a transport. Every consumed byte is
 * part of the pending reply; unrelated bytes are replayed to the owning editor.
 * Terminal input is not an authenticated device capability. */
#include <stdint.h>
typedef struct {
    uint32_t started;
    unsigned row, column;
    unsigned char active, length;
    unsigned char bytes[16];
} ps_terminal_probe;
enum { PS_PROBE_WAIT = 0, PS_PROBE_READY = 1, PS_PROBE_INVALID = -1, PS_PROBE_TIMEOUT = -2 };
static inline void ps_probe_begin(ps_terminal_probe *s, uint32_t now,
                                  ps_terminal_write write, void *ctx) {
    ps_secure_clear(s, sizeof(*s));
    s->active = 1;
    s->started = now;
    static const char query[] = "\033[s\033[999;999H\033[6n";
    write(ctx, (const unsigned char *)query, sizeof(query) - 1);
}
static inline void ps_probe_restore(ps_terminal_write write, void *ctx) {
    write(ctx, (const unsigned char *)"\033[u", 3);
}
/* Invalid input is retained in bytes[] for the owner to replay (e.g. a typed
 * character or an arrow). An overflowing reply is discarded by the owner. */
static inline int ps_probe_feed(ps_terminal_probe *s, unsigned char ch) {
    unsigned value = 0, digits = 0, field = 0, i;
    if (!s->active) return PS_PROBE_INVALID;
    /* Drain an overlong CSI through its final byte (or the fixed deadline),
     * so a numeric suffix cannot become a new command after rejection. */
    if (s->length >= sizeof(s->bytes)) {
        if (ch >= 0x40 && ch <= 0x7e) { s->active = 0; return PS_PROBE_INVALID; }
        return PS_PROBE_WAIT;
    }
    s->bytes[s->length++] = ch;
    if ((s->length == 1 && ch != 27) || (s->length == 2 && ch != '[')) {
        s->active = 0; return PS_PROBE_INVALID;
    }
    if (s->length <= 2) return PS_PROBE_WAIT;
    if ((ch >= '0' && ch <= '9') || ch == ';') return PS_PROBE_WAIT;
    s->active = 0;
    if (ch != 'R') return PS_PROBE_INVALID;
    for (i = 2; i + 1 < s->length; ++i) {
        unsigned c = s->bytes[i];
        if (c == ';') {
            if (field || !digits || !value) return PS_PROBE_INVALID;
            s->row = value; field = 1; value = digits = 0;
        } else {
            if (++digits > 4) return PS_PROBE_INVALID;
            value = value * 10 + c - '0';
        }
    }
    if (!field || !digits || !value) return PS_PROBE_INVALID;
    s->column = value;
    return PS_PROBE_READY;
}
static inline int ps_probe_tick(ps_terminal_probe *s, uint32_t now) {
    if (s->active && (uint32_t)(now - s->started) >= 500u) {
        s->active = 0;
        return PS_PROBE_TIMEOUT;
    }
    return PS_PROBE_WAIT;
}
#endif
