#ifndef SC_RPC_H
#define SC_RPC_H
#include "session.h"
#include <signal.h>
int sc_rpc_run(const char *transport,unsigned port,const char *origin,sc_session *,
               const suart_selector *,const suart_config *,
               int (*input)(unsigned char *,size_t,size_t *),volatile sig_atomic_t *);
#endif
