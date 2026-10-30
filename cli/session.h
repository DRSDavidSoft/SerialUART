#ifndef SUART_SESSION_H
#define SUART_SESSION_H
#include "SerialUART.h"
#include <stdio.h>
typedef struct { int kind; unsigned present,loose; char value[6][512]; } sc_rule;
typedef struct { unsigned vid,pid; char name[512]; } sc_alias;
typedef struct {
    int display,force_newline,no_status,echo,line_buffer,bulk,exit_key,clear_screen;
    int auto_connect,list_kind,log_append,log_display,log_time,pinned;
    const char *log_path,*title,*board_name;
    unsigned char enter[256],line[4096]; size_t enter_size,line_size;
    sc_rule rules[64]; unsigned rule_count;
    sc_alias aliases[64]; unsigned alias_count;
    suart_device identity;
    FILE *log; int log_line_start,osc_state,osc_title;
    unsigned char osc_header[24]; size_t osc_size;
} sc_session;
void sc_init(sc_session *);
/* 0 unknown, 1 consumed, 2 requires value, -1 invalid. */
int sc_option(sc_session *,const char *,const char *);
int sc_arguments(int *,char ***);
int sc_windows_arguments(int *,char ***);
void sc_settings(int,char **);
void sc_boards(const sc_session *);
void sc_filters(const sc_session *);
void sc_board(const sc_session *,const suart_device *,char *,size_t);
int sc_device_state(const sc_session *,const suart_device *);
int sc_list(sc_session *);
int sc_open(sc_session *,const suart_selector *,const suart_config *,suart_device *,suart_port **,unsigned long *);
int sc_output_open(sc_session *);
int sc_output(sc_session *,const unsigned char *,size_t,int echo);
int sc_output_close(sc_session *);
/* Terminal byte -> outgoing bytes, 0 buffered, -1 overflow. Escape commands
 * are handled by the caller, before this editor. */
int sc_input(sc_session *,unsigned char,unsigned char *,size_t,size_t *);
void sc_console(sc_session *);
void sc_help(void);
size_t sc_utf16(unsigned *lead,unsigned unit,unsigned char out[8]);
const char *sc_key_sequence(unsigned virtual_key);
#endif
