#ifndef SUART_PROTOCOL_H
#define SUART_PROTOCOL_H
#include <stddef.h>
/* Bounded, OS-independent protocol helpers. No allocations. */
int suart_ws_accept(const char *key,char out[29]);
size_t suart_ws_frame(unsigned opcode,const void *data,size_t n,unsigned char *out,size_t cap);
typedef struct {
    unsigned char input[65550], message[65536];
    size_t used, length;
    unsigned fragmented;
} suart_ws_parser;
/* callback opcodes: 1=text, 2=binary, 8=close, 9=ping, 10=pong.
 * Client frames must be masked; invalid frames return -1. */
typedef int (*suart_message_fn)(unsigned,const unsigned char *,size_t,void *);
int suart_ws_feed(suart_ws_parser *,const void *,size_t,suart_message_fn,void *);
int suart_hex(const char *,unsigned char *,size_t,size_t *);
size_t suart_base64(const unsigned char *,size_t,char *,size_t);
int suart_unbase64(const char *,size_t,unsigned char *,size_t,size_t *);
#endif
