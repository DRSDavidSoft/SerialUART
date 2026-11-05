#ifndef PORTABLE_SHELL_LEGACY_H
#define PORTABLE_SHELL_LEGACY_H
/* Optional hosted source adapter for the original atomicdeploy API.
   Link portable_shell_legacy; the core itself remains allocator/OS independent.
   Public function compatibility, not binary or internal-struct compatibility. */
#include "portable_shell.h"
#include <stddef.h>
#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct PortableShell PortableShell;
typedef struct PSCommand PSCommand;
typedef struct PSParameter PSParameter;
typedef int (*PSReadFunc)(void *);
typedef int (*PSWriteFunc)(void *, char);
typedef int (*PSWriteStrFunc)(void *, const char *);
typedef int (*PSCommandFunc)(PortableShell *, int, char **);
typedef enum { PS_PARAM_INT, PS_PARAM_FLOAT, PS_PARAM_STRING, PS_PARAM_BOOL } PSParamType;
typedef struct {
    PSReadFunc read_char;
    PSWriteFunc write_char;
    PSWriteStrFunc write_str;
    void *io_ctx;
    uint16_t history_size;
    bool vt100_enabled;
    const char *prompt;
} PSConfig;
struct PSCommand {
    const char *name, *help;
    PSCommandFunc func;
    PSCommand *next;
    PortableShell *owner;
};
struct PSParameter {
    const char *name;
    PSParamType type;
    void *value_ptr;
    size_t max_size;
    PSParameter *next;
    unsigned slot;
};
#define PS_LEGACY_PARAMETERS 32
struct PortableShell {
    PSConfig config;
    PSCommand *commands;
    PSParameter *parameters;
    bool running, tui_mode;
    ps_session *core;
    void *storage;
    int io_error;
    ps_control controls[PS_LEGACY_PARAMETERS];
    ps_control_value observed[PS_LEGACY_PARAMETERS];
    ps_control_page page;
    ps_panel panel;
};
#define PS_MAX_CMD_LENGTH PS_LINE
#define PS_MAX_ARGS PS_ARGS
#define PS_PROMPT_MAX_LENGTH 64
#define PS_KEY_CTRL_C 0x03
#define PS_KEY_CTRL_L 0x0c
#define PS_KEY_BACKSPACE 0x7f
#define PS_KEY_ENTER 0x0d
#define PS_KEY_TAB 0x09
#define PS_KEY_ESC 0x1b
PortableShell *ps_create(PSConfig *);
void ps_destroy(PortableShell *);
void ps_run(PortableShell *);
void ps_stop(PortableShell *);
int ps_process_char(PortableShell *, char);
int ps_register_command(PortableShell *, const char *, const char *, PSCommandFunc);
int ps_unregister_command(PortableShell *, const char *);
int ps_register_parameter(PortableShell *, const char *, PSParamType, void *, size_t);
int ps_unregister_parameter(PortableShell *, const char *);
PSParameter *ps_get_parameter(PortableShell *, const char *);
int ps_cmd_help(PortableShell *, int, char **);
int ps_cmd_clear(PortableShell *, int, char **);
int ps_cmd_get(PortableShell *, int, char **);
int ps_cmd_set(PortableShell *, int, char **);
int ps_cmd_edit(PortableShell *, int, char **);
void ps_print(PortableShell *, const char *);
void ps_println(PortableShell *, const char *);
void ps_print_prompt(PortableShell *);
void ps_vt100_clear_screen(PortableShell *);
void ps_vt100_clear_line(PortableShell *);
void ps_vt100_cursor_home(PortableShell *);
void ps_vt100_cursor_move(PortableShell *, int);
void ps_vt100_cursor_left(PortableShell *, int);
void ps_vt100_cursor_right(PortableShell *, int);
void ps_vt100_color(PortableShell *, int, int);
void ps_vt100_reset(PortableShell *);
void ps_bell(PortableShell *);
#ifdef __cplusplus
}
#endif
#endif
