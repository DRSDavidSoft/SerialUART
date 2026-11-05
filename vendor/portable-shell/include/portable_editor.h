#ifndef PORTABLE_EDITOR_H
#define PORTABLE_EDITOR_H
#include "portable_terminal.h"
#include "portable_undo.h"
#ifndef PS_EDITOR_UNDO_DEPTH
#define PS_EDITOR_UNDO_DEPTH 2
#endif
#include "portable_style.h"
/* Bounded event editor profile; source lineage is in the donor inventory.
 * No command execution, allocation or application state. Hidden by default.
 * Compile separately from the full session when a firmware already owns its
 * dispatcher and TUI. Capacity macros must agree in all consuming units. */
#ifndef PS_EDITOR_CAPACITY
#define PS_EDITOR_CAPACITY 512
#endif
#ifndef PS_EDITOR_HISTORY
#define PS_EDITOR_HISTORY 8
#endif
#ifndef PS_EDITOR_PROMPT_BYTES
#define PS_EDITOR_PROMPT_BYTES 64
#endif
#ifndef PS_EDITOR_SUGGEST_BYTES
#define PS_EDITOR_SUGGEST_BYTES 80
#endif
#ifndef PS_EDITOR_PROMPT_SUFFIX
#define PS_EDITOR_PROMPT_SUFFIX "> "
#endif
#ifndef PS_EDITOR_PROMPT_STYLE
#define PS_EDITOR_PROMPT_STYLE "\x1b[36m"
#endif
#if PS_EDITOR_PROMPT_BYTES < 6 || PS_EDITOR_SUGGEST_BYTES < 2
#error "Prompt needs >=6 bytes and suggestion storage >=2"
#endif
#if PS_EDITOR_CAPACITY < 32 || PS_EDITOR_HISTORY < 0
#error "Event editor requires capacity >=32 and history >=0"
#endif
enum {
    PS_EDIT_NONE,
    PS_EDIT_SUBMIT,
    PS_EDIT_CANCEL,
    PS_EDIT_TOO_LONG,
    PS_EDIT_CLOSE,
    PS_EDIT_COMPLETE
};
typedef const char *(*ps_editor_candidate)(void *, size_t);
typedef struct {
#if PS_EDITOR_UNDO_DEPTH
    ps_undo undo;
    char undo_frames[PS_EDITOR_UNDO_DEPTH+1][PS_EDITOR_CAPACITY+1];
    unsigned undo_positions[PS_EDITOR_UNDO_DEPTH+1];
#endif
    unsigned paste,paste_match,csi_number;
    char command[PS_EDITOR_CAPACITY + 1];
#if PS_EDITOR_HISTORY
    char history[PS_EDITOR_HISTORY][PS_EDITOR_CAPACITY + 1], draft[PS_EDITOR_CAPACITY + 1];
#endif
    char prompt[PS_EDITOR_PROMPT_BYTES], suggestion[PS_EDITOR_SUGGEST_BYTES];
    size_t next, count, offset, draft_cursor, cursor;
    unsigned char escape, attribute, escape_bytes, echo, cr_pending, overflow, ansi, upper;
    unsigned char theme, true_color, suggestion_space;
} ps_editor;
static inline void ps_term_emit(ps_terminal_write write, void *ctx, const char *text) {
    write(ctx, (const unsigned char *)text, ps_text_length(text));
}
static inline void ps_term_left(ps_terminal_write write, void *ctx, size_t n, int ansi) {
    char b[16];
    size_t length;
    if (!n || n > PS_EDITOR_CAPACITY)
        return;
    if (!ansi) {
        while (n--)
            ps_term_emit(write, ctx, "\b");
        return;
    }
    b[0] = 27;
    b[1] = '[';
    length = ps_decimal(b + 2, (unsigned)n);
    b[length + 2] = 'D';
    b[length + 3] = 0;
    ps_term_emit(write, ctx, b);
}
static inline void ps_editor_init(ps_editor *s) {
    ps_secure_clear(s, sizeof(*s));
#if PS_EDITOR_UNDO_DEPTH
    ps_undo_bind(&s->undo,&s->undo_frames[0][0],s->undo_positions,PS_EDITOR_CAPACITY+1,PS_EDITOR_UNDO_DEPTH+1);
#endif
    s->ansi = 1;
    s->theme = 255;
    s->true_color = 1;
    ps_bytes_copy(s->prompt, "shell", 6);
}
static inline void ps_editor_clear(ps_editor *s) {
#if PS_EDITOR_UNDO_DEPTH
    s->undo.frames=&s->undo_frames[0][0];s->undo.positions=s->undo_positions;
#endif
#if PS_EDITOR_UNDO_DEPTH
    ps_undo_clear(&s->undo);
#endif
    s->paste=s->paste_match=s->csi_number=0;
    ps_secure_clear(s->command, sizeof(s->command));

#if PS_EDITOR_HISTORY
    ps_secure_clear(s->draft, sizeof(s->draft));
#endif
    ps_secure_clear(s->suggestion, sizeof(s->suggestion));
    s->cursor = s->draft_cursor = s->offset = 0;
    s->escape = s->attribute = s->escape_bytes = s->overflow = 0;
}
static inline void ps_editor_clear_history(ps_editor *s) {

#if PS_EDITOR_HISTORY
    ps_secure_clear(s->history, sizeof(s->history));
    ps_secure_clear(s->draft, sizeof(s->draft));
#endif
    ps_secure_clear(s->suggestion, sizeof(s->suggestion));
    s->next = s->count = s->offset = s->draft_cursor = 0;
}
static inline void ps_editor_reset(ps_editor *s) {
    ps_editor_clear(s);
    ps_editor_clear_history(s);
    s->cr_pending = s->echo = s->upper = 0;
}
static inline void ps_editor_prompt(ps_editor *s, ps_terminal_write write, void *ctx) {
    if (!s->echo)
        return;
    if (s->ansi)
        ps_term_emit(write, ctx, s->theme <= PS_THEME_DEFAULT ? ps_style_prompt(s->theme, s->true_color) : PS_EDITOR_PROMPT_STYLE);
    ps_term_emit(write, ctx, s->prompt);
    ps_term_emit(write, ctx, PS_EDITOR_PROMPT_SUFFIX);
    if (s->ansi)
        ps_term_emit(write, ctx, "\x1b[0m");
}
static inline void ps_editor_emit_input(ps_editor *s, ps_terminal_write write, void *ctx,
                                        const char *text) {
    if (!s->upper) {
        ps_term_emit(write, ctx, text);
        return;
    }
    while (*text) {
        unsigned char c = (unsigned char)*text++;
        if (c >= 'a' && c <= 'z')
            c -= 32;
        write(ctx, &c, 1);
    }
}
static inline void ps_editor_bell(ps_editor *s, ps_terminal_write write, void *ctx) {
    if (s->echo)
        ps_term_emit(write, ctx, "\a");
}
static inline void ps_editor_left(ps_editor *s, ps_terminal_write write, void *ctx, size_t n) {
    if (s->echo)
        ps_term_left(write, ctx, n, s->ansi);
}
static inline void ps_editor_redraw(ps_editor *s, ps_terminal_write write, void *ctx) {
    if (!s->echo)
        return;
    ps_term_emit(write, ctx, s->ansi ? "\r\x1b[2K" : "\r");
    ps_editor_prompt(s, write, ctx);
    ps_editor_emit_input(s, write, ctx, s->command);
    if (s->ansi && s->suggestion[0]) {
        ps_term_emit(write, ctx, "\x1b[90m");
        ps_editor_emit_input(s, write, ctx, s->suggestion);
        ps_term_emit(write, ctx, "\x1b[0m");
        ps_editor_left(s, write, ctx, ps_text_length(s->suggestion));
    }
    ps_editor_left(s, write, ctx, ps_text_length(s->command) - s->cursor);
}
static inline void ps_editor_erase_suggestion(ps_editor *s, ps_terminal_write write, void *ctx) {
    if (!s->suggestion[0])
        return;
    if (s->echo && s->ansi)
        ps_term_emit(write, ctx, "\x1b[K");
    ps_secure_clear(s->suggestion, sizeof(s->suggestion));
}
/* Application-owned identity changed: redraw without losing an edited draft,
 * cursor or history. Call only for a visible prompt, not over an active TUI.
 * Network identity validation/persistence remains the application's job. */
static inline void ps_editor_refresh_prompt(ps_editor *s, const char *label, int ansi,
                                            ps_terminal_write write, void *ctx) {
    ps_terminal_text(s->prompt, sizeof(s->prompt), label, 128);
    s->ansi = ansi != 0;
    if (s->echo && !s->ansi) ps_term_emit(write, ctx, "\r\n");
    ps_editor_redraw(s, write, ctx);
}
static inline int ps_editor_remember(ps_editor *s, int allowed) {
#if PS_EDITOR_HISTORY
    size_t latest = (s->next + PS_EDITOR_HISTORY - 1) % PS_EDITOR_HISTORY;
    if (!allowed || !s->echo || s->overflow || !ps_history_safe(s->command, sizeof(s->command)))
        return 0;
    if (s->count && ps_text_equal(s->history[latest], s->command))
        return 0;
    ps_secure_clear(s->history[s->next], sizeof(s->history[s->next]));
    ps_bytes_copy(s->history[s->next], s->command, ps_text_length(s->command) + 1);
    s->next = (s->next + 1) % PS_EDITOR_HISTORY;
    if (s->count < PS_EDITOR_HISTORY)
        ++s->count;
    s->offset = 0;
    return 1;
#else
    (void)s; (void)allowed; return 0;
#endif
}
static inline void ps_editor_history(ps_editor *s, int older, ps_terminal_write write, void *ctx) {
#if PS_EDITOR_HISTORY
    if (!s->echo || !s->count) {
        ps_editor_bell(s, write, ctx);
        return;
    }
    if (older) {
        if (s->offset == s->count) {
            ps_editor_bell(s, write, ctx);
            return;
        }
        if (!s->offset) {
            ps_secure_clear(s->draft, sizeof(s->draft));
            ps_bytes_copy(s->draft, s->command, ps_text_length(s->command) + 1);
            s->draft_cursor = s->cursor;
        }
        ++s->offset;
    } else {
        if (!s->offset) {
            ps_editor_bell(s, write, ctx);
            return;
        }
        --s->offset;
    }
    if (s->offset) {
        ps_bytes_copy(s->command,
                      s->history[(s->next + PS_EDITOR_HISTORY - s->offset) % PS_EDITOR_HISTORY],
                      sizeof(s->command));
        s->cursor = ps_text_length(s->command);
    } else {
        ps_bytes_copy(s->command, s->draft, sizeof(s->command));
        s->cursor = s->draft_cursor;
    }
    ps_editor_redraw(s, write, ctx);
#else
    (void)older; ps_editor_bell(s, write, ctx);
#endif
}
static inline void ps_editor_shift_left(ps_editor *s) {
    size_t i;
    for (i = s->cursor; i < PS_EDITOR_CAPACITY; ++i)
        s->command[i] = s->command[i + 1];
    s->command[PS_EDITOR_CAPACITY] = 0;
}
static inline void ps_editor_fix_cursor(ps_editor *s, int clear, ps_terminal_write write,
                                        void *ctx) {
    size_t length = ps_text_length(s->command);
    if (s->cursor != length) {
        if (s->ansi) {
            ps_term_emit(write, ctx, "\x1b[s");
            ps_editor_emit_input(s, write, ctx, s->command + s->cursor);
            ps_term_emit(write, ctx, "\x1b[K\x1b[u");
        } else {
            ps_editor_emit_input(s, write, ctx, s->command + s->cursor);
            if (clear)
                ps_term_emit(write, ctx, " ");
            ps_editor_left(s, write, ctx, length - s->cursor + (clear ? 1 : 0));
        }
    } else if (clear)
        ps_term_emit(write, ctx, s->ansi ? "\x1b[K" : " \b");
}
static inline void ps_editor_move(ps_editor *s, unsigned direction, ps_terminal_write write,
                                  void *ctx) {
    size_t length = ps_text_length(s->command), size, target;
    if ((direction == 'C' || direction == 'F') && s->cursor == length && s->suggestion[0]) {
        size = ps_text_length(s->suggestion);
        ps_bytes_copy(s->command + s->cursor, s->suggestion, size);
        s->cursor += size;
        if (s->suggestion_space) s->command[s->cursor++] = ' ';
        s->command[s->cursor] = 0;
        ps_editor_emit_input(s, write, ctx, s->suggestion);
        if (s->suggestion_space) ps_term_emit(write, ctx, " ");
        ps_secure_clear(s->suggestion, sizeof(s->suggestion));
        s->offset = 0;
        return;
    }
    ps_editor_erase_suggestion(s, write, ctx);
    if (direction == 'D') {
        if (!s->cursor) {
            ps_editor_bell(s, write, ctx);
            return;
        }
        --s->cursor;
        ps_editor_left(s, write, ctx, 1);
    } else if (direction == 'C') {
        if (s->cursor == length) {
            ps_editor_bell(s, write, ctx);
            return;
        }
        if (s->echo) {
            if (s->ansi)
                ps_term_emit(write, ctx, "\x1b[C");
            else {
                char c[2] = {s->command[s->cursor], 0};
                ps_editor_emit_input(s, write, ctx, c);
            }
        }
        ++s->cursor;
    } else if (direction == 'H' || direction == 'F') {
        target = direction == 'H' ? 0 : length;
        while (s->cursor != target)
            ps_editor_move(s, s->cursor > target ? 'D' : 'C', write, ctx);
    } else if (direction == 'A' || direction == 'B')
        ps_editor_history(s, direction == 'A', write, ctx);
    else
        ps_editor_bell(s, write, ctx);
}
static inline void ps_editor_escape(ps_editor *s, unsigned ch, ps_terminal_write write, void *ctx) {
    if (s->escape == 1) {
        if (ch == '[' || ch == 'O') {
            s->escape = 2;
            return;
        }
        s->escape = 0;
        ps_editor_move(s, ch, write, ctx);
        return;
    }
    if (++s->escape_bytes > 16) {
        s->escape = s->attribute = s->escape_bytes = 0;
        ps_editor_bell(s, write, ctx);
        return;
    }
    if ((s->escape == 2 || s->escape == 3) && ((ch >= '0' && ch <= '9') || ch == ';')) {
        if(ch>='0'&&ch<='9'&&s->csi_number<1000)s->csi_number=s->csi_number*10+ch-'0';
        if (!s->attribute)
            s->attribute = (unsigned char)ch;
        s->escape = 3;
        return;
    }
    if (s->escape == 3) {
        if (ch == '~') {
            if(s->csi_number==200){s->paste=1;s->paste_match=0;}
            else if (s->attribute == '3') {
                ps_editor_erase_suggestion(s, write, ctx);
                if (s->cursor < ps_text_length(s->command)) {
                    ps_editor_shift_left(s);
                    if (s->echo)
                        ps_editor_fix_cursor(s, 1, write, ctx);
                } else
                    ps_editor_bell(s, write, ctx);
            } else if (s->attribute == '1' || s->attribute == '4' || s->attribute == '7' ||
                       s->attribute == '8')
                ps_editor_move(s, s->attribute == '1' || s->attribute == '7' ? 'H' : 'F', write,
                               ctx);
            else if (s->attribute != '2')
                ps_editor_bell(s, write, ctx);
        } else
            ps_editor_move(s, ch, write, ctx);
    } else
        ps_editor_move(s, ch, write, ctx);
    s->escape = s->attribute = s->escape_bytes = 0;
}
static inline int ps_editor_feed_inner(ps_editor *s, unsigned char ch, ps_terminal_write write,
                                 void *ctx) {
    size_t target, i;
    if (!s->escape && ch != 27)
        ps_editor_erase_suggestion(s, write, ctx);
    if (s->cr_pending && (ch == '\n' || !ch)) {
        s->cr_pending = 0;
        return PS_EDIT_NONE;
    }
    if (ch == '\r' || ch == '\n') {
        s->cr_pending = ch == '\r';
        ps_term_emit(write, ctx, "\r\n");
        return s->overflow ? PS_EDIT_TOO_LONG : PS_EDIT_SUBMIT;
    }
    s->cr_pending = 0;
    if (ch == 3) {
        ps_editor_clear(s);
        ps_term_emit(write, ctx, "^C\r\n");
        return PS_EDIT_CANCEL;
    }
    if (s->overflow)
        return PS_EDIT_NONE;
    if (s->escape) {
        ps_editor_escape(s, ch, write, ctx);
        return PS_EDIT_NONE;
    }
    if (ch == 27) {
        s->csi_number=0;
        s->escape = 1;
        return PS_EDIT_NONE;
    }
    if (ch == '\t')
        return s->echo ? PS_EDIT_COMPLETE : PS_EDIT_NONE;
    if (ch == 1 || ch == 5) {
        ps_editor_move(s, ch == 1 ? 'H' : 'F', write, ctx);
        return PS_EDIT_NONE;
    }
    if (ch == 21 || ch == 23) {
        target = s->cursor;
        if (ch == 21)
            target = 0;
        else {
            while (target && s->command[target - 1] == ' ')
                --target;
            while (target && s->command[target - 1] != ' ')
                --target;
        }
        if (target < s->cursor) {
            ps_bytes_move(s->command + target, s->command + s->cursor,
                          ps_text_length(s->command + s->cursor) + 1);
            s->cursor = target;
            ps_editor_redraw(s, write, ctx);
        }
        return PS_EDIT_NONE;
    }
    if (ch == 4) {
        if (!s->command[0])
            return PS_EDIT_CLOSE;
        if (s->cursor < ps_text_length(s->command)) {
            ps_editor_shift_left(s);
            if (s->echo)
                ps_editor_fix_cursor(s, 1, write, ctx);
        } else
            ps_editor_bell(s, write, ctx);
        return PS_EDIT_NONE;
    }
    if (ch == 12) {
        if (s->echo) {
            ps_term_emit(write, ctx, s->ansi ? "\x1b[H\x1b[2J" : "\r\n");
            ps_editor_redraw(s, write, ctx);
        }
        return PS_EDIT_NONE;
    }
    if (ch == 8 || ch == 127) {
        if (!s->cursor)
            ps_editor_bell(s, write, ctx);
        else {
            --s->cursor;
            ps_editor_shift_left(s);
            if (s->echo) {
                ps_editor_left(s, write, ctx, 1);
                ps_editor_fix_cursor(s, 1, write, ctx);
            }
        }
        return PS_EDIT_NONE;
    }
    if (ch < 32 || ch >= 127) {
        ps_editor_bell(s, write, ctx);
        return PS_EDIT_NONE;
    }
    if (ps_text_length(s->command) >= PS_EDITOR_CAPACITY) {
        s->overflow = 1;
        ps_editor_bell(s, write, ctx);
        return PS_EDIT_NONE;
    }
    for (i = PS_EDITOR_CAPACITY; i > s->cursor; --i)
        s->command[i] = s->command[i - 1];
    s->command[s->cursor++] = (char)ch;
    if (s->echo) {
        char text[2] = {(char)ch, 0};
        ps_editor_emit_input(s, write, ctx, text);
        ps_editor_fix_cursor(s, 0, write, ctx);
    }
    return PS_EDIT_NONE;
}
/* Bracketed paste never executes controls or submits embedded newlines. */
static inline int ps_editor_feed(ps_editor *s,unsigned char ch,ps_terminal_write write,void *ctx){
    int result;
#if PS_EDITOR_UNDO_DEPTH
    unsigned pasted=s->paste;
#endif
#if PS_EDITOR_UNDO_DEPTH
    s->undo.frames=&s->undo_frames[0][0];s->undo.positions=s->undo_positions;
#endif
    if(s->paste){
        static const char end[]="\033[201~";
        if(ch==(unsigned char)end[s->paste_match]){
            if(++s->paste_match==sizeof(end)-1){
                s->paste=s->paste_match=0;
#if PS_EDITOR_UNDO_DEPTH
                ps_undo_record(&s->undo,s->command,(unsigned)s->cursor);
#endif
            }return PS_EDIT_NONE;
        }
        s->paste_match=0;
        if(ch==27){s->paste_match=1;return PS_EDIT_NONE;}
        if(ch=='\r'||ch=='\n'||ch=='\t')ch=' ';
        else if(ch<32||ch==127)return PS_EDIT_NONE;
    }
#if PS_EDITOR_UNDO_DEPTH
    if((!s->escape&&ch==31)||(s->escape==1&&ch=='_')){
        unsigned cursor;
        ps_undo_record(&s->undo,s->command,(unsigned)s->cursor);
        int redo=s->escape!=0;s->escape=0;
        if(ps_undo_step(&s->undo,redo,s->command,&cursor)){s->cursor=cursor;s->overflow=0;ps_editor_redraw(s,write,ctx);}
        else ps_editor_bell(s,write,ctx);
        return PS_EDIT_NONE;
    }
    if(!pasted)ps_undo_record(&s->undo,s->command,(unsigned)s->cursor);
#endif
    result=ps_editor_feed_inner(s,ch,write,ctx);
#if PS_EDITOR_UNDO_DEPTH
    if(result==PS_EDIT_SUBMIT||result==PS_EDIT_TOO_LONG||result==PS_EDIT_CANCEL)ps_undo_clear(&s->undo);
    else if(!pasted)ps_undo_record(&s->undo,s->command,(unsigned)s->cursor);
#endif
    return result;
}
static inline void ps_editor_complete(ps_editor *s, size_t start, size_t count,
                                      ps_editor_candidate candidate, void *cctx,
                                      ps_terminal_write write, void *ctx) {
    size_t typed, matches = 0, common = 0, i, n, extra;
    const char *first = 0, *name;
    if (!s->echo || s->overflow || s->escape || s->cursor != ps_text_length(s->command) ||
        start > s->cursor)
        return;
    ps_editor_erase_suggestion(s, write, ctx);
    typed = s->cursor - start;
    for (i = 0; i < count; ++i) {
        name = candidate(cctx, i);
        if (!name || !ps_text_prefix(name, s->command + start, typed))
            continue;
        if (!matches) {
            first = name;
            common = ps_text_length(name);
        } else {
            n = 0;
            while (n < common && name[n] &&
                   ps_ascii_fold((unsigned char)name[n]) == ps_ascii_fold((unsigned char)first[n]))
                ++n;
            common = n;
        }
        ++matches;
    }
    if (!matches) {
        ps_editor_bell(s, write, ctx);
        return;
    }
    extra = matches == 1 ? 1 : 0;
    if (start + common + extra > PS_EDITOR_CAPACITY) {
        ps_editor_bell(s, write, ctx);
        return;
    }
    if (common > typed || matches == 1) {
        ps_bytes_copy(s->command + start, first, common);
        s->cursor = start + common;
        if (extra)
            s->command[s->cursor++] = ' ';
        s->command[s->cursor] = 0;
        s->offset = 0;
        ps_editor_redraw(s, write, ctx);
        return;
    }
    ps_term_emit(write, ctx, "\r\n");
    for (i = 0; i < count; ++i) {
        name = candidate(cctx, i);
        if (name && ps_text_prefix(name, s->command + start, typed)) {
            ps_term_emit(write, ctx, name);
            ps_term_emit(write, ctx, "  ");
        }
    }
    ps_term_emit(write, ctx, "\r\n");
    ps_editor_redraw(s, write, ctx);
}
static inline int ps_editor_offer(ps_editor *s, const char *suffix, int space,
                                  ps_terminal_write write, void *ctx) {
    size_t size = ps_text_length(suffix);
    if (!size || size >= sizeof(s->suggestion) ||
        s->cursor + size + (space != 0) > PS_EDITOR_CAPACITY)
        return 0;
    ps_bytes_copy(s->suggestion, suffix, size + 1);
    s->suggestion_space = space != 0;
    ps_term_emit(write, ctx, "\x1b[90m");
    ps_editor_emit_input(s, write, ctx, s->suggestion);
    ps_term_emit(write, ctx, "\x1b[0m");
    ps_editor_left(s, write, ctx, size);
    return 1;
}
static inline void ps_editor_suggest(ps_editor *s, size_t start, size_t count,
                                     ps_editor_candidate candidate, void *cctx,
                                     ps_terminal_write write, void *ctx) {
    size_t i, typed;
    const char *name;
    if (!s->echo || !s->ansi || s->overflow || s->escape ||
        s->cursor != ps_text_length(s->command) || start >= s->cursor)
        return;
    ps_editor_erase_suggestion(s, write, ctx);
    /* Retained history has already passed application admission and secret
     * filtering. Prefer the newest exact prefix; preserve its raw tail. */

#if PS_EDITOR_HISTORY
    for (i = 0; i < s->count; ++i) {
        size_t j;
        name = s->history[(s->next + PS_EDITOR_HISTORY - 1 - i) % PS_EDITOR_HISTORY];
        for (j = 0; j < s->cursor && name[j] == s->command[j]; ++j) {}
        if (j == s->cursor && name[j] && ps_editor_offer(s, name + j, 0, write, ctx))
            return;
    }
#endif
    typed = s->cursor - start;
    for (i = 0; i < count; ++i) {
        name = candidate(cctx, i);
        if (!name || !ps_text_prefix(name, s->command + start, typed) ||
            ps_text_length(name) <= typed)
            continue;
        if (ps_editor_offer(s, name + typed, 1, write, ctx)) return;
    }
}
#endif
