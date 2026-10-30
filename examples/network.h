#ifndef SUART_NETWORK_H
#define SUART_NETWORK_H
#include <stddef.h>
typedef struct suart_net suart_net;
/* One client, bounded queues, loopback by default. mode=tcp/ws/socketio.
 * connect_mode is TCP-only. Origin NULL rejects all browser Origins. */
suart_net *suart_net_open(const char *mode,const char *host,unsigned port,int connect_mode,const char *origin);
void suart_net_close(suart_net *);
/* receive: 0=no data, 1=data, -1=peer lost/protocol error. */
int suart_net_read(suart_net *,void *,size_t,size_t *);
int suart_net_write(suart_net *,const void *,size_t);
#endif
