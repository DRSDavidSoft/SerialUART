#define _POSIX_C_SOURCE 200809L
#include "network.h"
#include "protocol.h"
#include "SerialUART.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_t;
#define BAD INVALID_SOCKET
#define shut closesocket
#else
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netdb.h>
typedef int socket_t;
#define BAD (-1)
#define shut close
#endif
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
struct suart_net {
    socket_t server,peer;
    int mode,ready,connected,await_pong;
    const char *origin;
    uint64_t deadline,ping;
    char http[8192]; size_t http_used;
    suart_ws_parser parser;
    unsigned char queue[65536]; size_t queued;
};
static int blocked(void) {
#ifdef _WIN32
    int e=WSAGetLastError(); return e==WSAEWOULDBLOCK || e==WSAEINPROGRESS;
#else
    return errno==EAGAIN || errno==EWOULDBLOCK || errno==EINTR || errno==EINPROGRESS;
#endif
}
static int nonblock(socket_t s) {
#ifdef _WIN32
    u_long yes=1; return ioctlsocket(s,FIONBIO,&yes);
#else
    int flags=fcntl(s,F_GETFL,0); return flags<0?-1:fcntl(s,F_SETFL,flags|O_NONBLOCK);
#endif
}
static int sendall(socket_t s,const void *data,size_t n) {
    const char *b=(const char *)data; uint64_t end=suart_millis()+2000;
    while(n) {
        int sent=send(s,b,(int)n,MSG_NOSIGNAL);
        if(sent>0) { b+=sent; n-=(size_t)sent; }
        else if(sent<0 && blocked() && suart_millis()<end) suart_sleep(1);
        else return -1;
    }
    return 0;
}
static int frame(suart_net *n,unsigned op,const void *b,size_t len) {
    unsigned char out[65540]; size_t size=suart_ws_frame(op,b,len,out,sizeof(out));
    return size?sendall(n->peer,out,size):-1;
}
static void drop(suart_net *n) {
    if(n->peer!=BAD) shut(n->peer);
    n->peer=BAD; n->ready=n->connected=n->await_pong=0;
    n->http_used=n->queued=0; memset(&n->parser,0,sizeof(n->parser));
}
suart_net *suart_net_open(const char *mode,const char *host,unsigned port,int client,const char *origin) {
    struct addrinfo hints,*list=NULL,*a; char service[16]; socket_t s=BAD; suart_net *n;
    int kind=!strcmp(mode,"tcp")?0:!strcmp(mode,"ws")?1:!strcmp(mode,"socketio")?2:-1;
    if(kind<0 || (client && kind) || !port || port>65535) return NULL;
#ifdef _WIN32
    { WSADATA w; if(WSAStartup(MAKEWORD(2,2),&w)) return NULL; }
#endif
    memset(&hints,0,sizeof(hints)); hints.ai_socktype=SOCK_STREAM; hints.ai_family=AF_UNSPEC;
    snprintf(service,sizeof(service),"%u",port);
    if(getaddrinfo(host,service,&hints,&list)) goto bad;
    for(a=list;a;a=a->ai_next) {
        int yes=1;
        s=socket(a->ai_family,a->ai_socktype,a->ai_protocol); if(s==BAD) continue;
#ifdef _WIN32
        if(!client) setsockopt(s,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,(const char *)&yes,sizeof(yes));
#else
        if(!client) setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes));
#endif
        if(nonblock(s)) { shut(s); s=BAD; continue; }
        if(client) {
            int rc=connect(s,a->ai_addr,(int)a->ai_addrlen);
            if(rc && blocked()) {
                fd_set wr; struct timeval tv; int error=0;
#ifdef _WIN32
                int length=sizeof(error);
#else
                socklen_t length=sizeof(error);
#endif
                FD_ZERO(&wr); FD_SET(s,&wr); tv.tv_sec=3; tv.tv_usec=0;
                rc=select((int)s+1,NULL,&wr,NULL,&tv)>0?0:-1;
                if(!rc && (getsockopt(s,SOL_SOCKET,SO_ERROR,(char *)&error,&length) || error)) rc=-1;
            }
            if(!rc) break;
        } else if(!bind(s,a->ai_addr,(int)a->ai_addrlen) && !listen(s,1)) break;
        shut(s); s=BAD;
    }
    freeaddrinfo(list); list=NULL;
    if(s==BAD) goto bad;
    n=(suart_net *)calloc(1,sizeof(*n)); if(!n) { shut(s); goto bad; }
    n->mode=kind; n->server=client?BAD:s; n->peer=client?s:BAD;
    n->ready=client; n->origin=origin; return n;
bad:
    if(list) freeaddrinfo(list);
#ifdef _WIN32
    WSACleanup();
#endif
    return NULL;
}
void suart_net_close(suart_net *n) {
    if(!n) return;
    drop(n); if(n->server!=BAD) shut(n->server); free(n);
#ifdef _WIN32
    WSACleanup();
#endif
}
static int equal_ci(const char *a,const char *b) {
    while(*a && *b) { unsigned x=(unsigned char)*a++,y=(unsigned char)*b++; if(x>='A' && x<='Z') x+=32; if(y>='A' && y<='Z') y+=32; if(x!=y) return 0; }
    return *a==*b;
}
static int token(const char *value,const char *wanted) {
    char copy[1024],*p,*next;
    if(strlen(value)>=sizeof(copy)) return 0;
    strcpy(copy,value); p=copy;
    while(p) {
        char *end; next=strchr(p,','); if(next) *next++=0;
        while(*p==' ' || *p=='\t') ++p;
        end=p+strlen(p); while(end>p && (end[-1]==' ' || end[-1]=='\t')) *--end=0;
        if(equal_ci(p,wanted)) return 1;
        p=next;
    }
    return 0;
}
static int handshake(suart_net *n) {
    char *p,*line,*key=NULL,*origin=NULL,accept[29],response[512]; int upgrade=0,connection=0,version=0;
    line=strstr(n->http,"\r\n"); if(!line) return -1; *line=0;
    if(n->mode==2) {
        if(strcmp(n->http,"GET /socket.io/?EIO=4&transport=websocket HTTP/1.1")) return -1;
    } else if(strcmp(n->http,"GET / HTTP/1.1") && strcmp(n->http,"GET /serial HTTP/1.1")) return -1;
    p=line+2;
    while((line=strstr(p,"\r\n")) && line!=p) {
        char *colon; *line=0; colon=strchr(p,':'); if(!colon) return -1; *colon++=0;
        while(*colon==' ' || *colon=='\t') ++colon;
        { char *end=colon+strlen(colon); while(end>colon && (end[-1]==' ' || end[-1]=='\t')) *--end=0; }
        if(equal_ci(p,"Sec-WebSocket-Key")) { if(key) return -1; key=colon; }
        if(equal_ci(p,"Upgrade")) upgrade=token(colon,"websocket");
        if(equal_ci(p,"Connection")) connection=token(colon,"upgrade");
        if(equal_ci(p,"Sec-WebSocket-Version")) version=!strcmp(colon,"13");
        if(equal_ci(p,"Origin")) { if(origin) return -1; origin=colon; }
        p=line+2;
    }
    if(!upgrade || !connection || !version || !key || (origin && (!n->origin || strcmp(origin,n->origin))) || suart_ws_accept(key,accept)) return -1;
    snprintf(response,sizeof(response),"HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n",accept);
    if(sendall(n->peer,response,strlen(response))) return -1;
    n->ready=1;
    if(n->mode==2) {
        const char *open="0{\"sid\":\"serialuart\",\"upgrades\":[],\"pingInterval\":25000,\"pingTimeout\":20000,\"maxPayload\":65535}";
        n->ping=suart_millis()+25000; n->deadline=suart_millis()+20000;
        return frame(n,1,open,strlen(open));
    }
    return 0;
}
static int message(unsigned op,const unsigned char *b,size_t len,void *ctx) {
    suart_net *n=(suart_net *)ctx;
    if(op==8) { frame(n,8,b,len); return -1; }
    if(op==9) return frame(n,10,b,len);
    if(op==10) return 0;
    if(n->mode==2) {
        const char *prefix="42[\"serial\",\""; size_t start=strlen(prefix),decoded;
        if(op!=1) return -1;
        if(len==1 && b[0]=='3' && n->await_pong) { n->await_pong=0; return 0; }
        if(len>=2 && b[0]=='4' && b[1]=='0' && !n->connected) {
            if(len!=2 && !(len>=4 && b[2]=='{' && b[len-1]=='}')) return -1;
            const char *ack="40{\"sid\":\"serialuart\"}";
            n->connected=1; return frame(n,1,ack,strlen(ack));
        }
        if(len==2 && b[0]=='4' && b[1]=='1') return -1;
        if(!n->connected || len<start+2 || memcmp(b,prefix,start) || b[len-2]!='"' || b[len-1]!=']') return -1;
        if(suart_unbase64((const char *)b+start,len-start-2,n->queue+n->queued,sizeof(n->queue)-n->queued,&decoded)) return -1;
        n->queued+=decoded; return 0;
    }
    if(n->queued+len>sizeof(n->queue)) return -1;
    memcpy(n->queue+n->queued,b,len); n->queued+=len; return 0;
}
int suart_net_read(suart_net *n,void *out,size_t cap,size_t *done) {
    unsigned char b[4096]; int got; *done=0;
    if(n->queued) {
        size_t take=n->queued<cap?n->queued:cap;
        memcpy(out,n->queue,take); memmove(n->queue,n->queue+take,n->queued-take); n->queued-=take; *done=take; return 1;
    }
    if(n->peer==BAD) {
        if(n->server==BAD) return -1;
        n->peer=accept(n->server,NULL,NULL);
        if(n->peer==BAD) return blocked()?0:-1;
        if(nonblock(n->peer)) { drop(n); return -1; }
        n->ready=n->mode==0; n->deadline=suart_millis()+5000;
    }
    if(!n->ready && suart_millis()>n->deadline) { drop(n); return -1; }
    if(n->ready && n->mode==2) {
        uint64_t now=suart_millis();
        if((!n->connected || n->await_pong) && now>n->deadline) { drop(n); return -1; }
        if(!n->await_pong && now>=n->ping) {
            if(frame(n,1,"2",1)) { drop(n); return -1; }
            n->await_pong=1; n->deadline=now+20000; n->ping=now+25000;
        }
    }
    got=recv(n->peer,(char *)b,sizeof(b),0);
    if(got<0 && blocked()) return 0;
    if(got<=0) { drop(n); return -1; }
    if(!n->mode) {
        memcpy(n->queue,b,(size_t)got); n->queued=(size_t)got;
    } else if(!n->ready) {
        size_t i;
        for(i=0;i<(size_t)got;++i) {
            if(n->http_used+1>=sizeof(n->http) || !b[i]) { drop(n); return -1; }
            n->http[n->http_used++]=(char)b[i]; n->http[n->http_used]=0;
            if(n->http_used>=4 && !memcmp(n->http+n->http_used-4,"\r\n\r\n",4)) {
                if(handshake(n) || suart_ws_feed(&n->parser,b+i+1,(size_t)got-i-1,message,n)) { drop(n); return -1; }
                break;
            }
        }
    } else if(suart_ws_feed(&n->parser,b,(size_t)got,message,n)) { drop(n); return -1; }
    return 0;
}
int suart_net_write(suart_net *n,const void *data,size_t len) {
    int rc;
    /* No peer means incoming serial data is deliberately discarded. */
    if(n->peer==BAD || !n->ready || (n->mode==2 && !n->connected)) return 0;
    if(n->mode==0) rc=sendall(n->peer,data,len);
    else if(n->mode==1) rc=frame(n,2,data,len);
    else {
        char b[8192]; size_t size;
        if(len>4096) return -1;
        memcpy(b,"42[\"serial\",\"",13); size=suart_base64((const unsigned char *)data,len,b+13,sizeof(b)-15);
        if(!size && len) return -1;
        memcpy(b+13+size,"\"]",2); rc=frame(n,1,b,15+size);
    }
    if(rc) drop(n);
    return rc;
}
