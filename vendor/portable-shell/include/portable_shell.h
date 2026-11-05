/* Convenience storage for shipped default hosts/examples. Custom configurations
   must check ps_sizeof_session(); this is not an ABI size guarantee. */
#ifndef PS_DEFAULT_SESSION_STORAGE
#define PS_DEFAULT_SESSION_STORAGE 49152
#endif
/* Optional interactive history UI and whole-line event recall. */
#ifndef PS_HISTORY_DIALOG
#define PS_HISTORY_DIALOG 1
#endif
#ifndef PS_HISTORY_EXPANSION
#define PS_HISTORY_EXPANSION 1
#endif
#ifndef PORTABLE_SHELL_H
#define PORTABLE_SHELL_H
#include "portable_util.h"
#include "portable_table_policy.h"
#ifndef PS_MOUSE
#define PS_MOUSE 1
#endif
#ifndef PS_COMPLETION_MENU
#define PS_COMPLETION_MENU 1
#endif
#ifdef __cplusplus
extern "C" {
#endif
/* OS-independent C99. Caller-owned storage, no allocator/global mutable state.
   Default backend is freestanding; libc primitives are an explicit build option. */
#if defined(PORTABLE_SHELL_DLL) && defined(_WIN32)
#define PS_API __declspec(dllexport)
#else
#define PS_API
#endif
#ifndef PS_UNDO_DEPTH
#define PS_UNDO_DEPTH 2
#endif
#ifndef PS_LINE
#define PS_LINE 512
#endif
#ifndef PS_HISTORY
#define PS_HISTORY 24
#endif
#ifndef PS_ROWS
#define PS_ROWS 32
#endif
#ifndef PS_COMMANDS
#define PS_COMMANDS 42
#endif
#define PS_ARGS 32
#ifndef PS_PROMPT_PARTS
#define PS_PROMPT_PARTS 16
#endif
#ifndef PS_PROMPT_LINES
#define PS_PROMPT_LINES 4
#endif
#if PS_PROMPT_PARTS < 1 || PS_PROMPT_LINES < 1
#error "Prompt limits must be positive"
#endif
#define PS_VALUE 128
#define PS_VERSION "0.14.0"
#ifndef PS_WORKSPACE_INLINE_EDIT
#define PS_WORKSPACE_INLINE_EDIT 1
#endif
#ifndef PS_CONTROL_SECTION_ROWS
#define PS_CONTROL_SECTION_ROWS 1
#endif
#if PS_LINE < 128 || PS_HISTORY < 1 || PS_ROWS < 1 || PS_COMMANDS < 1
#error "PortableShell needs PS_LINE >= 128, PS_HISTORY/PS_ROWS >= 1, PS_COMMANDS >= 1"
#endif
typedef unsigned char ps_byte;
typedef char
    ps_requires_8_bit_bytes[((unsigned char)256 == 0 && (unsigned char)255 == 255) ? 1 : -1];
typedef unsigned int ps_u32;
typedef char ps_requires_32_bit_unsigned[(sizeof(ps_u32) == 4) ? 1 : -1];
typedef struct ps_session ps_session;
#ifndef PS_HEX_EDITOR
#define PS_HEX_EDITOR 1
#endif
/* Draft and title stay borrowed until close. Source and draft must not overlap.
   Save accepts the complete binary buffer; failure keeps the editor open.
   Fixed-length overwrite editing; no filesystem or allocation in the core. */
typedef int (*ps_hex_save_fn)(void *, const ps_byte *, unsigned);
PS_API int ps_hex_editor_open(ps_session *,const char *title,const ps_byte *data,
    unsigned length,ps_byte *draft,unsigned capacity,ps_hex_save_fn,void *context);
PS_API unsigned ps_hex_editor_active(const ps_session *);
/* Zero selects responsive columns (default); 1..32 fixes bytes per row.
   A fixed layout that cannot fit displays a resize notice without changing data. */
PS_API int ps_hex_editor_set_columns(ps_session *,unsigned bytes_per_row);
#ifndef PS_EDITOR
#define PS_EDITOR 1
#endif
#ifndef PS_EDITOR_CAPACITY
#define PS_EDITOR_CAPACITY 2048
#endif
#if PS_EDITOR_CAPACITY < 128
#error "PS_EDITOR_CAPACITY must be at least 128"
#endif
/* Save returns 1 only after accepting the complete document; failure leaves
   the draft open. Title/context remain borrowed until close. LF text, UTF-8.
   The editor copies initial text; discard never calls save. No filesystem API. */
typedef int (*ps_editor_save_fn)(void *, const char *, unsigned);
PS_API int ps_editor_open(ps_session *, const char *title, const char *text,
                          ps_editor_save_fn, void *context);
PS_API unsigned ps_editor_active(const ps_session *);
PS_API const char *ps_editor_text(const ps_session *);
/* READ_ONLY reuses wrapping and navigation for reports and guides. It rejects
   edits, paste, undo and save; Escape closes without a discard dialog. */
enum { PS_EDITOR_HEADER=1, PS_EDITOR_BORDER=2, PS_EDITOR_READ_ONLY=4 };
PS_API void ps_editor_set_style(ps_session *, unsigned flags);
#ifndef PS_MENU_DEPTH
#define PS_MENU_DEPTH 8
#endif
#if PS_MENU_DEPTH < 1 || PS_MENU_DEPTH > 32
#error "PS_MENU_DEPTH must be 1..32"
#endif
typedef struct ps_menu ps_menu;
typedef int (*ps_menu_fn)(ps_session *, void *context);
enum { PS_MENU_DISABLED=1 };
typedef struct {
    const char *label;
    const ps_menu *submenu;
    ps_menu_fn run;
    void *context;
    unsigned flags;
} ps_menu_item;
struct ps_menu { const char *title; const ps_menu_item *items; unsigned count; };
/* Borrowed definitions/context must remain alive while open. Each item has
   either a submenu or an action. Actions run after leaving menu input mode,
   and may execute shell commands or open another menu. No allocation. */
PS_API int ps_menu_open(ps_session *, const ps_menu *);
PS_API int ps_menu_choose(ps_session *, unsigned number); /* 0 = Back/Close */
PS_API void ps_menu_refresh(ps_session *);
PS_API void ps_menu_close(ps_session *);
PS_API unsigned ps_menu_active(const ps_session *);
#ifndef PS_MENU_LIST_VIEW
#define PS_MENU_LIST_VIEW 1
#endif
#ifndef PS_MENU_CONTEXT
#define PS_MENU_CONTEXT 1
#endif
enum { PS_MENU_TABLE=1, PS_MENU_NUMBERS=2, PS_MENU_DETAILS=4, PS_MENU_LIST=8,
       PS_MENU_BREADCRUMBS=16, PS_MENU_MARKER=32 };
typedef const char *(*ps_menu_description_fn)(void *, const ps_menu *, unsigned index);
/* Called while drawing; return borrowed printable text and a PS_INK_* role.
   Refresh after provider state changes. No background polling or I/O in core. */
typedef const char *(*ps_menu_notice_fn)(void *, const ps_menu *, unsigned *role);
/* Style 0 retains the original transcript menu. Interactive default: table + numbers.
   Description callback borrows text for the duration of each render. */
PS_API void ps_menu_set_style(ps_session *, unsigned flags);
PS_API void ps_menu_set_description(ps_session *, ps_menu_description_fn, void *);
PS_API void ps_menu_set_notice(ps_session *, ps_menu_notice_fn, void *);
enum { PS_FILE_DIRECTORY=1, PS_FILE_DISABLED=2, PS_FILE_LINK=4, PS_FILE_ROOT=65535 };
typedef struct { const char *name; unsigned parent, flags; } ps_file_node;
/* Flat caller-owned tree, parent before child, at most 128 nodes. No filesystem
   calls. A file callback receives its stable node index AFTER picker close;
   cancellation reports PS_FILE_ROOT. Nodes/context remain borrowed while open. */
typedef void (*ps_file_pick_fn)(ps_session *, void *, unsigned index);
PS_API int ps_file_picker_open(ps_session *, const char *title, const ps_file_node *,
                               unsigned count, ps_file_pick_fn, void *);
PS_API unsigned ps_file_picker_active(const ps_session *);
PS_API void ps_file_picker_close(ps_session *);
enum { PS_FILE_TREE, PS_FILE_LIST, PS_FILE_SPLIT };
enum { PS_FILE_ICONS_UNICODE, PS_FILE_ICONS_NERD, PS_FILE_ICONS_EMOJI, PS_FILE_ICONS_ASCII };
/* Append children to the borrowed node array (capacity 128), preserving existing
   indices. Return new count, or 0 on failure. Called once per directory until
   reopen; no session reentry. Names must remain alive. Full snapshots report a limit. */
typedef unsigned (*ps_file_load_fn)(void *, unsigned parent, unsigned count);
PS_API void ps_file_picker_style(ps_session *, unsigned layout, unsigned icons);
PS_API int ps_file_browser_open(ps_session *, const char *, const ps_file_node *, unsigned,
                                ps_file_pick_fn, ps_file_load_fn, void *);
/* Borrowed immutable definitions; retain them and context until detach/session end.
   Callbacks run on the owning loop, must not reenter the session. No allocation.
   read supplies observed state, never a speculative requested value. write must
   atomically check expected_revision and return 0 rejected, 1 applied, 2 pending.
   A pending provider exposes PS_CONTROL_PENDING until readback or failure. */
enum {
    PS_CONTROL_TOGGLE,
    PS_CONTROL_LEVEL,
    PS_CONTROL_CHOICE,
    PS_CONTROL_TEXT,
    PS_CONTROL_ACTION,
    PS_CONTROL_LABEL
};
/* NUMBER_ONLY keeps numeric editing/range validation without presenting a
   quantity such as a port number as a percentage meter. */
enum { PS_CONTROL_VALID = 1, PS_CONTROL_PENDING = 2, PS_CONTROL_LOCKED = 4,
       PS_CONTROL_NUMBER_ONLY = 8, PS_CONTROL_INACTIVE = 16, PS_CONTROL_ATTENTION = 32 };
typedef struct {
    unsigned id;
    const char *group, *label;
    unsigned kind, minimum, maximum, step; /* numeric range <= 65535 */
    const char *const *choices;
    unsigned choice_count;
} ps_control;
typedef struct {
    const char *title;
    const ps_control *controls;
    unsigned count;
} ps_control_page;
typedef struct {
    unsigned number, flags;
    ps_u32 revision;
    char text[64]; /* ACTION: optional button label; empty uses "Run". */
} ps_control_value;
typedef int (*ps_control_read_fn)(void *, unsigned id, ps_control_value *observed);
typedef int (*ps_control_write_fn)(void *, unsigned id, const ps_control_value *requested,
                                   ps_u32 expected_revision);
typedef struct {
    const char *title, *status;
    const ps_control_page *pages;
    unsigned count; /* 1..8 pages, <=128 rows each, unique stable IDs */
    ps_control_read_fn read;
    ps_control_write_fn write;
    void *context;
} ps_panel;
PS_API int ps_panel_attach(ps_session *, const ps_panel *); /* NULL detaches */
PS_API int ps_panel_open(ps_session *, unsigned page);
/* Zero when closed; otherwise the active page index plus one. */
PS_API unsigned ps_panel_active(const ps_session *);
/* Optional application shortcuts, dispatched only outside cell editing. Return
   1 to consume a key; queue UI changes for the owner loop, never reenter here. */
#ifndef PS_PANEL_KEYS
#define PS_PANEL_KEYS 1
#endif
typedef int (*ps_panel_key_fn)(ps_session *, unsigned key, void *context);
PS_API void ps_panel_set_key_handler(ps_session *, ps_panel_key_fn, void *context);
/* A category heading or active edit has no selected value. */
PS_API int ps_panel_selection(const ps_session *, unsigned *id);
PS_API int ps_panel_select(ps_session *, unsigned id);
#ifndef PS_DASHBOARD
#define PS_DASHBOARD 1
#endif
enum { PS_PANEL_TABS, PS_PANEL_GRID };
/* Select presentation; provider IDs, validation and writes stay shared.
   Returns 0 for unsupported layout, active editing, or a disabled dashboard. */
PS_API int ps_panel_set_layout(ps_session *,unsigned layout);
PS_API void ps_panel_refresh(ps_session *); /* after provider events, redraw on tick */
typedef struct {
    const char *title;
    unsigned minimum, maximum, weight; /* maximum=0 means flexible; weight 0..16 */
} ps_column;
#ifndef PS_PANEL_TEXT
#define PS_PANEL_TEXT 1
#endif
#ifndef PS_PANEL_TEXT_BYTES
#define PS_PANEL_TEXT_BYTES 256
#endif
#if PS_PANEL_TEXT_BYTES < 4
#error PS_PANEL_TEXT_BYTES must hold a UTF-8 character
#endif
/* Optional display-only formatting for TEXT/LABEL cells. Return the byte count
   (less than capacity), or zero for ordinary rendering. Fill roles for every
   byte with PS_INK_* from portable_style.h; keep UTF-8 characters intact.
   Never emit ANSI sequences. Editing always uses the original read value.
   Called with panel.context, only during drawing; must not mutate the panel. */
typedef unsigned (*ps_panel_text_fn)(void *,unsigned id,char *text,ps_byte *roles,unsigned capacity);
/* Borrowed application text/layout; NULL restores standard panel chrome.
   Custom columns must match the active layout (2 key/value or 3 with category).
   PS_PANEL_KEYS=0 removes this optional application integration as well. */
typedef struct {
    const ps_column *columns; unsigned count;
    const char *summary,*notice,*actions,*navigation;
    ps_panel_text_fn text;
} ps_panel_chrome;
PS_API void ps_panel_set_chrome(ps_session *,const ps_panel_chrome *);
enum { PS_TABLE_GRID = 1, PS_TABLE_RED_BORDER = 2, PS_TABLE_TITLE_BAR = 4,
       PS_TABLE_CATEGORY_COLUMN = 8, PS_TABLE_COMPACT_SECTIONS = 16,
       PS_TABLE_TABS_INSIDE = 32, PS_TABLE_PLAIN_KEYS = 64,
       PS_TABLE_TAB_BRACKETS = 128, PS_TABLE_PROMINENT_KEYS = 256,
       PS_TABLE_COMPACT_FOOTER = 512, PS_TABLE_COMPACT_SCROLLBAR = 1024, PS_TABLE_SECTION_BOTTOM = 2048, PS_TABLE_STATIC_SECTIONS = 4096,
       PS_TABLE_SECTION_PLUS_MINUS = 8192, PS_TABLE_SECTION_NO_ICONS = 16384, PS_TABLE_RESERVE_SCROLLBAR = 32768, PS_TABLE_PROMPT = 65536,
       PS_TABLE_SECTION_FILLED = 131072, PS_TABLE_PROMPT_FRAME = 262144,
       PS_TABLE_SECTION_INDICATORS_ALWAYS = 524288, PS_TABLE_BACK_TO_MENU = 1048576,
       PS_TABLE_DEFAULT = 5 };
/* Fits 1..8 columns into a framed width (max 240), including padding/separators.
   Returns occupied width, or 0 if minima do not fit / definitions are invalid. */
PS_API unsigned ps_layout_columns(unsigned width, const ps_column *columns, unsigned count,
                                  unsigned *widths);
PS_API void ps_set_table_style(ps_session *, unsigned flags);
/* Marker is copied into session storage; empty removes its reserved label cells. */
/* NULL restores automatic selection marker; empty string explicitly hides it. */
PS_API void ps_set_selection_marker(ps_session *, const char *marker);
typedef void (*ps_write_fn)(void *context, const ps_byte *bytes, unsigned count);
typedef int (*ps_line_fn)(ps_session *, const char *line, void *context);
typedef int (*ps_command_fn)(ps_session *, int argc, const char *const *argv, void *context);
/* Raw tail starts immediately after the first space/tab; quotes and all remaining
   whitespace are preserved. Only command-name matching is case folded. */
typedef int (*ps_raw_command_fn)(ps_session *, const char *tail, void *context);
/* Completion providers are queried by Tab and interactive ghost previews. Return
   stable borrowed strings; enumeration must not mutate device/application state.
   PS_NO_SUGGESTIONS disables preview queries while retaining explicit Tab. */
typedef unsigned (*ps_complete_fn)(ps_session *, int argc, const char *const *argv,
                                   const char *prefix, const char **items, unsigned capacity,
                                   void *context);
typedef void (*ps_key_fn)(ps_session *, unsigned key, void *context);
typedef struct {
    const char *name, *usage, *category;
    ps_command_fn run;
    ps_complete_fn complete;
    void *context;
} ps_command;
typedef struct {
    int argc;
    char *argv[PS_ARGS];
    char storage[PS_LINE];
    unsigned error_at;
    const char *error;
} ps_args;
typedef struct {
    ps_write_fn write;
    void *context;
    unsigned columns, rows;
    unsigned flags;
} ps_config;
enum {
    PS_INTERACTIVE = 1,
    PS_COLOR = 2,
    PS_UNICODE = 4,
    PS_TELNET = 8,
    PS_TRUECOLOR = 16,
    PS_CASE_INSENSITIVE = 32,
    PS_NO_BUILTINS = 64,
    PS_LEGACY_KEYS = 128,
    PS_NO_BELL = 256,
    PS_NO_SUGGESTIONS = 512,
    PS_EXTENDED_KEYS = 1024, /* Alt-D/Alt-Backspace and Ctrl-T readline shortcuts */
    PS_NO_SYNC_OUTPUT = 2048,
    PS_NO_OSC = 4096,
    PS_NO_COMPLETION_MENU = 8192 /* Print-only candidates instead of a live selection. */
};
enum { PS_STYLE_DEFAULT, PS_STYLE_PROMPT, PS_STYLE_SUCCESS, PS_STYLE_WARNING, PS_STYLE_ERROR,
       PS_STYLE_GREEN, PS_STYLE_MUTED, PS_STYLE_COMMAND, PS_STYLE_BLUE };
enum {
    PS_KEY_UP = 256,
    PS_KEY_DOWN,
    PS_KEY_LEFT,
    PS_KEY_RIGHT,
    PS_KEY_HOME,
    PS_KEY_END,
    PS_KEY_DELETE,
    PS_KEY_PAGEUP,
    PS_KEY_PAGEDOWN,
    PS_KEY_BACKTAB,
    PS_KEY_F1,
    PS_KEY_F2,
    PS_KEY_F3,
    PS_KEY_F4,
    PS_KEY_WORDLEFT,
    PS_KEY_WORDRIGHT,
    PS_KEY_F7 = 282,
    PS_KEY_UNDO = 280, PS_KEY_REDO,
    PS_KEY_KILLWORD = 272
};
PS_API unsigned ps_sizeof_session(void);
PS_API const char *ps_memory_backend(void); /* "portable" or "libc"; no allocation */
PS_API unsigned ps_alignof_session(void);
/* Memory must be aligned (normal static storage / malloc alignment is sufficient).
   Callback consumes ALL bytes synchronously, must not reenter this session.
   One owner/event loop per session; separate sessions share no mutable state. */
PS_API ps_session *ps_init(void *memory, unsigned bytes, const ps_config *config);
PS_API void ps_start(ps_session *s);
PS_API void ps_feed(ps_session *s, const void *bytes, unsigned count);
PS_API void ps_tick(ps_session *s, ps_u32 monotonic_ms);
PS_API void ps_resize(ps_session *s, unsigned columns, unsigned rows);
PS_API void ps_finish(ps_session *s); /* EOF: execute final unterminated batch line. */
PS_API void ps_close(ps_session *s);  /* restore terminal modes */
/* Current terminal ownership, for native host adapters. Mouse implies alternate screen. */
enum { PS_TERMINAL_ALT_SCREEN = 1, PS_TERMINAL_MOUSE = 2 };
PS_API unsigned ps_terminal_state(const ps_session *);
PS_API int ps_exited(const ps_session *s);
PS_API int ps_status(const ps_session *s);
/* Owner-loop command-language providers update the most recent exit status. */
PS_API void ps_set_status(ps_session *s, int status);
PS_API int ps_execute(ps_session *s, const char *line);
PS_API int ps_execute_argv(ps_session *s, int argc, const char *const *argv);
PS_API int ps_parse(const char *line, ps_args *out);
PS_API const char *ps_option(int argc, const char *const *argv, const char *name);
PS_API int ps_register(ps_session *s, const ps_command *command);
/* Optional consumer-owned fallback, after registered commands. NULL disables it.
   The callback receives parsed argv, never an implicitly interpreted OS shell string. */
PS_API void ps_set_executor(ps_session *s, ps_command_fn run, void *context);
/* Optional raw-line language provider; registered raw commands retain precedence.
 * Provider calls ps_execute_argv for dispatch, never recursively ps_execute. */
PS_API void ps_set_line_executor(ps_session *, ps_line_fn, void *context);
/* Swap a synchronous byte sink; owner loop only, outside output callbacks.
 * Pass the returned sink/context back to restore. No output is flushed here. */
PS_API void ps_exchange_output(ps_session *, ps_write_fn *write, void **context);
/* Exchange only output color/interactive flags for scoped plain rendering. */
PS_API unsigned ps_output_flags(ps_session *, unsigned flags);
PS_API int ps_unregister(ps_session *s, const char *name);
/* Metadata remains borrowed until the next registry mutation. */
PS_API unsigned ps_command_count(const ps_session *s);
PS_API const ps_command *ps_command_at(const ps_session *s, unsigned index);
PS_API int ps_register_raw(ps_session *s, const ps_command *metadata, ps_raw_command_fn run);
/* Text is copied, max 63 UTF-8 bytes, no control characters. NULL restores default. */
PS_API int ps_set_prompt(ps_session *s, const char *text, unsigned style);
/* Copied prompt: 255 bytes, 16 colored parts, four lines. Newlines are allowed;
 * other controls/escape sequences are rejected. Zero parts restores the default.
 * Symbols are ordinary UTF-8 text. */
typedef struct { const char *text; unsigned style; } ps_prompt_part;
PS_API int ps_set_prompt_parts(ps_session *, const ps_prompt_part *, unsigned count);
PS_API void ps_set_uppercase_echo(ps_session *s, int enabled);
PS_API int ps_get_uppercase_echo(const ps_session *s);
/* Safe external event output; preserves pending input/cursor. In the board it
   becomes the contextual notice. Call from the owning loop, not from write(). */
PS_API void ps_message(ps_session *s, unsigned style, const char *text);
PS_API void ps_clear(ps_session *s);
PS_API void ps_request_exit(ps_session *s);
PS_API int ps_bind(ps_session *s, unsigned key, ps_key_fn callback, void *context);
PS_API void ps_key(ps_session *s, unsigned key);
PS_API void ps_write(ps_session *s, const char *text); /* safe printable output, strips controls */
PS_API void ps_newline(ps_session *s);
/* Emits byte 7 through the normal transport in interactive sessions unless muted.
   The receiving terminal decides whether to sound or visually indicate the bell. */
PS_API void ps_ring_bell(ps_session *s);
#ifndef PS_REPORT
#define PS_REPORT 1
#endif
enum { PS_REPORT_SECTION=1, PS_REPORT_GAP=2, PS_REPORT_NUMBER=4 };
enum { PS_REPORT_DOTS=0, PS_REPORT_SPACES=1, PS_REPORT_DENSE=2 };
/* Borrowed printable strings, consumed synchronously; NULL means empty.
   A section row can also supply a right-hand column heading. */
typedef struct { const char *label, *annotation, *value; unsigned flags; } ps_report_row;
/* Returns rows rendered, or 0 for invalid input / PS_REPORT=0. Width follows
   terminal columns; long content wraps without truncation. No session storage. */
PS_API unsigned ps_print_report(ps_session *,const ps_report_row *,unsigned count,unsigned style);
#ifndef PS_FORM
#define PS_FORM 1
#endif
enum { PS_FORM_TEXT, PS_FORM_CHOICE, PS_FORM_PASSWORD };
typedef const char *(*ps_form_validate_fn)(void *,unsigned field,const char *value);
typedef void (*ps_form_done_fn)(ps_session *,void *,int accepted);
typedef struct {
    const char *label;
    unsigned kind;
    char *value; unsigned capacity; /* writable, initially NUL-terminated */
    unsigned minimum, maximum; /* text code points; maximum=0 means storage limit */
    const char *const *choices; unsigned choice_count;
} ps_form_field;
typedef struct {
    const ps_form_field *fields; unsigned count;
    ps_form_validate_fn validate; ps_form_done_fn done; void *context;
} ps_form;
/* Descriptors and value buffers remain alive until completion. Each accepted
   step writes its value; cancelled forms retain accepted steps for the caller
   to discard or reuse. The completion callback is not invoked for a rejected step. */
PS_API int ps_form_open(ps_session *,const ps_form *);
PS_API void ps_form_close(ps_session *); /* cancel, callback receives accepted=0 */
PS_API unsigned ps_form_active(const ps_session *);
enum { PS_FORM_OVERVIEW=1 };
/* Optional summary of all fields; Tab/Shift-Tab revisit validated drafts.
   The default remains the sequential question presentation. */
PS_API void ps_form_set_style(ps_session *,unsigned flags);
/* Optional borrowed caption, retained until replaced. NULL clears it. */
PS_API void ps_form_set_title(ps_session *,const char *title);
enum { PS_HELP_HEADINGS, PS_HELP_COMPACT };
#ifndef PS_HELP_DETAILS
#define PS_HELP_DETAILS 1
#endif
/* Immutable borrowed metadata; keep alive while attached. */
typedef struct { const char *label, *description; } ps_help_item;
typedef struct {
    const char *name, *description;
    const ps_help_item *arguments; unsigned argument_count;
    const ps_help_item *options; unsigned option_count;
    const ps_help_item *examples; unsigned example_count;
} ps_help_document;
/* All metadata strings are borrowed and must outlive their registration.
 * No commands are installed by the portable_shell framework target. */
typedef struct { ps_command command; const ps_help_document *help; } ps_command_definition;
typedef struct {
    const char *description,*usage,*arguments,*options,*examples;
} ps_help_labels;
PS_API int ps_register_definition(ps_session *,const ps_command_definition *);
PS_API void ps_set_help_labels(ps_session *,const ps_help_labels *);
PS_API void ps_set_application_title(ps_session *,const char *);
PS_API void ps_set_help_documents(ps_session *, const ps_help_document *, unsigned);
/* NULL name lists the registry; non-NULL prints Usage. Returns entries printed. */
PS_API int ps_print_help(ps_session *s, const char *name, unsigned layout);
PS_API void ps_number(ps_session *s, ps_u32 value);
/* OSC returns 1 only when emitted. Batch output and PS_NO_OSC suppress it.
 * ps_set_title sets the window title. Directory accepts an absolute URI path. */
PS_API int ps_set_title(ps_session *, const char *title);
PS_API int ps_osc(ps_session *, unsigned code, const char *payload);
PS_API int ps_terminal_report_progress(ps_session *, unsigned state, unsigned percent);
PS_API int ps_report_directory(ps_session *, const char *host, const char *path);
PS_API void ps_progress(ps_session *s, const char *name, unsigned done, unsigned total);
PS_API void ps_history_clear(ps_session *s);
typedef int (*ps_history_filter_fn)(void *context, const char *line);
/* Additional application admission; default conservative credential filtering
 * always applies. Changing policy clears retained entries. NULL removes hook. */
PS_API void ps_history_filter(ps_session *s, ps_history_filter_fn filter, void *context);
PS_API void ps_destroy_session(ps_session *s); /* close + scrub; storage remains caller-owned */
PS_API unsigned ps_history_count(const ps_session *s);
PS_API const char *ps_history_get(const ps_session *s, unsigned oldest_index);
PS_API int ps_history_add(ps_session *s, const char *line);
/* Retains newest entries. 0 disables history; values above PS_HISTORY clamp. */
PS_API unsigned ps_history_limit(ps_session *s, unsigned limit);
#ifdef __cplusplus
}
#endif
#ifdef PS_ENABLE_LEGACY_API
#include "portable_shell_legacy.h"
#endif
#endif
