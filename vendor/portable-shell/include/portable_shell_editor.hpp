#ifndef PORTABLE_SHELL_EDITOR_HPP
#define PORTABLE_SHELL_EDITOR_HPP
#include "portable_editor.h"
#include <stdint.h>
/* Thin, allocation-free C++11 callback facade. All editing/negotiation is C.
 * Event profile is opt-in by including this header; it does not pull in the
 * full command runtime, default commands, workspace or hosted compatibility. */
namespace PortableShell {
template <class W> inline void writer(void *p, const unsigned char *b, size_t n) {
    (*static_cast<W *>(p))(b, n);
}
template <class C> inline const char *candidate(void *p, size_t i) {
    return (*static_cast<C *>(p))(i);
}
inline size_t terminalText(char *out, size_t n, const char *text, size_t maximum = 512) {
    return ps_terminal_text(out, n, text, maximum);
}
struct Terminal {
    template <class W> static void text(W w, const char *s) {
        ps_term_emit(writer<W>, &w, s);
    }
    template <class W> static void clear(W w, bool ansi = true) {
        text(w, ansi ? "\x1b[H\x1b[2J" : "\r\n");
    }
    template <class W> static void clearLine(W w, bool ansi = true) {
        text(w, ansi ? "\r\x1b[2K" : "\r");
    }
    template <class W> static void cursorLeft(W w, size_t n, bool ansi = true) {
        ps_term_left(writer<W>, &w, n, ansi);
    }
    template <class W> static void color(W w, int fg, int bg, bool ansi = true) {
        if (!ansi || fg < 30 || fg > 37 || bg < 40 || bg > 47)
            return;
        char b[] = {27, '[', '3', char('0' + fg - 30), ';', '4', char('0' + bg - 40), 'm', 0};
        text(w, b);
    }
    template <class W> static void reset(W w, bool ansi = true) {
        if (ansi)
            text(w, "\x1b[0m");
    }
};
class TelnetParser {
    ps_telnet state_{};

  public:
    TelnetParser() {
        reset();
    }
    void reset() {
        ps_telnet_init(&state_, PS_TN_BINARY, 80, 24);
    }
    uint16_t columns() const {
        return static_cast<uint16_t>(state_.columns);
    }
    uint16_t rows() const {
        return static_cast<uint16_t>(state_.rows);
    }
    const char *terminal() const {
        return state_.terminal;
    }
    bool ansi() const {
        return ps_telnet_ansi(&state_) != 0;
    }
    bool size(uint16_t cols, uint16_t rows) {
        if (cols < 40 || cols > 240 || rows < 16 || rows > 100)
            return false;
        state_.columns = cols;
        state_.rows = rows;
        return true;
    }
    template <class W> void greet(W w) {
        reset();
        ps_telnet_greet(&state_, writer<W>, &w);
    }
    template <class W, class I> void feed(uint8_t ch, W w, I input) {
        unsigned char decoded = 0;
        if (ps_telnet_feed(&state_, ch, writer<W>, &w, &decoded) == PS_TN_DATA)
            input(decoded);
    }
};
class LineEditor {
    ps_editor state_{};

  public:
    static constexpr size_t kCapacity = PS_EDITOR_CAPACITY, kHistory = PS_EDITOR_HISTORY;
    enum Event : uint8_t { None, Submit, Cancel, TooLong, Close, Complete };
    LineEditor() {
        ps_editor_init(&state_);
    }
    ~LineEditor() {
        ps_secure_clear(&state_, sizeof(state_));
    }
    const char *line() const {
        return state_.command;
    }
    size_t cursor() const {
        return state_.cursor;
    }
    size_t historySize() const {
        return state_.count;
    }
    void reset() {
        ps_editor_reset(&state_);
    }
    void clear() {
        ps_editor_clear(&state_);
    }
    void clearHistory() {
        ps_editor_clear_history(&state_);
    }
    void setEcho(bool enabled) {
        state_.echo = enabled;
    }
    void setUpper(bool enabled) {
        state_.upper = enabled;
    }
    void setTheme(unsigned theme, bool trueColor = true) {
        state_.theme = theme <= 3 ? static_cast<unsigned char>(theme) : 255;
        state_.true_color = trueColor;
    }
    void setPrompt(const char *label, bool ansi = true) {
        terminalText(state_.prompt, sizeof(state_.prompt), label, 128);
        state_.ansi = ansi;
    }
    bool remember(bool allowed) {
        return ps_editor_remember(&state_, allowed) != 0;
    }
    template <class W> void prompt(W w) {
        ps_editor_prompt(&state_, writer<W>, &w);
    }
    template <class W> void redraw(W w) {
        ps_editor_redraw(&state_, writer<W>, &w);
    }
    template <class W> void refreshPrompt(const char *label, W w, bool ansi = true) {
        ps_editor_refresh_prompt(&state_, label, ansi, writer<W>, &w);
    }
    template <class W> void redrawEvent(const char *text, W w) {
        char safe[257];
        terminalText(safe, sizeof(safe), text);
        if (state_.echo)
            Terminal::text(w, state_.ansi ? "\r\x1b[2K" : "\r\n");
        Terminal::text(w, safe);
        Terminal::text(w, "\r\n");
        if (state_.echo)
            redraw(w);
    }
    template <class W> void printHistory(W w) const {
        if (!state_.echo)
            return;
        for (size_t i = 0; i < state_.count; ++i) {
            char number[12], safe[kCapacity + 1];
            ps_decimal(number, static_cast<unsigned>(i + 1));
            Terminal::text(w, number);
            Terminal::text(w, "  ");
            terminalText(safe, sizeof(safe),
                         state_.history[(state_.next + kHistory - state_.count + i) % kHistory],
                         kCapacity);
            Terminal::text(w, safe);
            Terminal::text(w, "\r\n");
        }
    }
    template <class W> Event feed(uint8_t ch, W w) {
        return static_cast<Event>(ps_editor_feed(&state_, ch, writer<W>, &w));
    }
    template <class C, class W> void suggest(size_t start, size_t count, C c, W w) {
        ps_editor_suggest(&state_, start, count, candidate<C>, &c, writer<W>, &w);
    }
    template <class C, class W> void complete(size_t start, size_t count, C c, W w) {
        ps_editor_complete(&state_, start, count, candidate<C>, &c, writer<W>, &w);
    }
};
} // namespace PortableShell
#endif
