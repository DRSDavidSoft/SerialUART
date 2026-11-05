#ifndef PORTABLE_OSC_H
#define PORTABLE_OSC_H
/* Byte-only OSC emitters. Callbacks consume bytes synchronously.
 * The caller owns terminal capability policy. No probing or OS calls. */
#include "portable_terminal.h"
#ifndef PS_OSC
#define PS_OSC 1
#endif
/* Reject controls and malformed UTF-8 before emitting any bytes. */
static inline int ps_osc_text_valid(const char *text) {
    const unsigned char *p=(const unsigned char *)text;
    if(!p)return 0;
    while(*p){
        unsigned c=*p++,n=0,min=0;
        if(c<32||c==127)return 0;
        if(c<128)continue;
        if(c>=194&&c<=223){c&=31;n=1;min=128;}
        else if(c>=224&&c<=239){c&=15;n=2;min=2048;}
        else if(c>=240&&c<=244){c&=7;n=3;min=65536;}
        else return 0;
        while(n--){if((*p&192)!=128)return 0;c=(c<<6)|(*p++&63);}
        if(c<min||c>0x10ffff||(c>=0xd800&&c<=0xdfff)||(c>=128&&c<=159))return 0;
    }
    return 1;
}
static inline int ps_terminal_osc(ps_terminal_write write,void *ctx,unsigned code,const char *payload) {
    char prefix[24];size_t n;
    if(!PS_OSC||!write||code>65535||!ps_osc_text_valid(payload))return 0;
    n=ps_terminal_format(prefix,sizeof prefix,"\033]%u;",code);
    write(ctx,(const unsigned char *)prefix,n);
    write(ctx,(const unsigned char *)payload,ps_text_length(payload));
    write(ctx,(const unsigned char *)"\033\\",2);
    return 1;
}
static inline int ps_terminal_title(ps_terminal_write write,void *ctx,unsigned target,const char *title) {
    return target<=2?ps_terminal_osc(write,ctx,target,title):0;
}
enum { PS_OSC_PROGRESS_CLEAR, PS_OSC_PROGRESS_NORMAL, PS_OSC_PROGRESS_ERROR,
       PS_OSC_PROGRESS_BUSY, PS_OSC_PROGRESS_PAUSED };
static inline int ps_terminal_progress(ps_terminal_write write,void *ctx,unsigned state,unsigned percent) {
    char payload[24];
    if(state>4||percent>100)return 0;
    ps_terminal_format(payload,sizeof payload,"4;%u;%u",state,percent);
    return ps_terminal_osc(write,ctx,9,payload);
}
/* Absolute URI path: host supplies /C:/... on Windows, /... on POSIX.
 * Percent-encode UTF-8 bytes; callers pass unescaped path text. */
static inline int ps_terminal_directory(ps_terminal_write write,void *ctx,const char *host,const char *path) {
    const unsigned char *p;const char *hex="0123456789ABCDEF";
    if(!PS_OSC||!write||!host||!ps_osc_text_valid(path)||path[0]!='/')return 0;
    for(p=(const unsigned char *)host;*p;++p)
        if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='.'||*p=='-'))return 0;
    write(ctx,(const unsigned char *)"\033]7;file://",11);
    write(ctx,(const unsigned char *)host,ps_text_length(host));
    for(p=(const unsigned char *)path;*p;++p){
        unsigned c=*p;
        if((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='/'||c==':'||c=='-'||c=='_'||c=='.'||c=='~')
            write(ctx,p,1);
        else {unsigned char escaped[3]={'%',(unsigned char)hex[c>>4],(unsigned char)hex[c&15]};write(ctx,escaped,3);}
    }
    write(ctx,(const unsigned char *)"\033\\",2);return 1;
}
#endif

