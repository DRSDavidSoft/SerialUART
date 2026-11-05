#ifndef SUART_TUI_H
#define SUART_TUI_H
#include "session.h"
#include <signal.h>
int sc_tui_run(sc_session *,const suart_selector *,const suart_config *,unsigned,unsigned,
               int (*)(unsigned char *,size_t,size_t *),volatile sig_atomic_t *);
#endif
