#include "protocol.h"
#include <stdint.h>
#include <string.h>
static const char b64[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
size_t suart_base64(const unsigned char *s,size_t n,char *out,size_t cap) {
    size_t i,j=0;
    if(n>(cap/4)*3 || cap<4*((n+2)/3)+1) return 0;
    for(i=0;i<n;i+=3) {
        unsigned v=(unsigned)s[i]<<16;
        if(i+1<n) v|=(unsigned)s[i+1]<<8;
        if(i+2<n) v|=s[i+2];
        out[j++]=b64[(v>>18)&63]; out[j++]=b64[(v>>12)&63];
        out[j++]=i+1<n?b64[(v>>6)&63]:'='; out[j++]=i+2<n?b64[v&63]:'=';
    }
    out[j]=0; return j;
}
int suart_unbase64(const char *s,size_t n,unsigned char *out,size_t cap,size_t *done) {
    size_t i,j=0; *done=0;
    if(n%4) return -1;
    for(i=0;i<n;i+=4) {
        unsigned v=0,k,pad=0;
        for(k=0;k<4;++k) {
            const char *p;
            if(s[i+k]=='=') { if(k<2 || i+4!=n) return -1; ++pad; v<<=6; }
            else { if(pad || !s[i+k] || !(p=strchr(b64,s[i+k]))) return -1; v=(v<<6)|(unsigned)(p-b64); }
        }
        if(pad>2 || (pad==1 && (v&255)) || (pad==2 && (v&65535))) return -1;
        if(j+3-pad>cap) return -1;
        out[j++]=(unsigned char)(v>>16);
        if(pad<2) out[j++]=(unsigned char)(v>>8);
        if(!pad) out[j++]=(unsigned char)v;
    }
    *done=j; return 0;
}
static uint32_t rol(uint32_t x,unsigned n) { return (x<<n)|(x>>(32-n)); }
int suart_ws_accept(const char *key,char out[29]) {
    unsigned char bytes[128]={0},digest[20],decoded[32];
    uint32_t h[5]={0x67452301u,0xefcdab89u,0x98badcfeu,0x10325476u,0xc3d2e1f0u};
    size_t n,i,off,check;
    if(!key || strlen(key)!=24 || suart_unbase64(key,24,decoded,sizeof(decoded),&check) || check!=16) return -1;
    memcpy(bytes,key,24); memcpy(bytes+24,"258EAFA5-E914-47DA-95CA-C5AB0DC85B11",36);
    n=60; bytes[n]=128; bytes[126]=(unsigned char)((n*8)>>8); bytes[127]=(unsigned char)(n*8);
    for(off=0;off<128;off+=64) {
        uint32_t w[80],a=h[0],b=h[1],c=h[2],d=h[3],e=h[4];
        for(i=0;i<16;++i) { size_t j=off+i*4; w[i]=((uint32_t)bytes[j]<<24)|((uint32_t)bytes[j+1]<<16)|((uint32_t)bytes[j+2]<<8)|bytes[j+3]; }
        for(i=16;i<80;++i) w[i]=rol(w[i-3]^w[i-8]^w[i-14]^w[i-16],1);
        for(i=0;i<80;++i) {
            uint32_t f,k,t;
            if(i<20) { f=(b&c)|((~b)&d); k=0x5a827999u; }
            else if(i<40) { f=b^c^d; k=0x6ed9eba1u; }
            else if(i<60) { f=(b&c)|(b&d)|(c&d); k=0x8f1bbcdcu; }
            else { f=b^c^d; k=0xca62c1d6u; }
            t=rol(a,5)+f+e+k+w[i]; e=d; d=c; c=rol(b,30); b=a; a=t;
        }
        h[0]+=a; h[1]+=b; h[2]+=c; h[3]+=d; h[4]+=e;
    }
    for(i=0;i<20;++i) digest[i]=(unsigned char)(h[i/4]>>(24-8*(i%4)));
    return suart_base64(digest,20,out,29)==28?0:-1;
}
size_t suart_ws_frame(unsigned op,const void *data,size_t n,unsigned char *out,size_t cap) {
    size_t head=n<126?2:4;
    if(n>65535 || head+n>cap) return 0;
    out[0]=(unsigned char)(128|op);
    if(head==2) out[1]=(unsigned char)n;
    else { out[1]=126; out[2]=(unsigned char)(n>>8); out[3]=(unsigned char)n; }
    if(n) memcpy(out+head,data,n);
    return head+n;
}
/* Validate complete UTF-8 text (also rejects overlong and surrogate encodings). */
static int utf8(const unsigned char *s,size_t n) {
    size_t i=0;
    while(i<n) {
        unsigned c=s[i++],need,cp,min;
        if(c<128) continue;
        if(c>=194 && c<=223) { need=1;cp=c&31;min=128; }
        else if(c>=224 && c<=239) { need=2;cp=c&15;min=2048; }
        else if(c>=240 && c<=244) { need=3;cp=c&7;min=65536; }
        else return 0;
        if(n-i<need) return 0;
        while(need--) { c=s[i++]; if((c&192)!=128) return 0; cp=(cp<<6)|(c&63); }
        if(cp<min || cp>0x10ffff || (cp>=0xd800 && cp<=0xdfff)) return 0;
    }
    return 1;
}
static int parse(suart_ws_parser *p,suart_message_fn cb,void *ctx) {
    while(p->used>=2) {
        unsigned char *b=p->input; unsigned op=b[0]&15,fin=b[0]&128;
        size_t head=2,n=b[1]&127,i; int control=op>=8;
        if((b[0]&112) || !(b[1]&128) || !(op==0 || op==1 || op==2 || op==8 || op==9 || op==10)) return -1;
        if(n==126) { if(p->used<4) return 0; n=((size_t)b[2]<<8)|b[3]; head=4; if(n<126) return -1; }
        else if(n==127) return -1; /* bounded adapter deliberately rejects >64 KiB */
        if(control && (!fin || n>125)) return -1;
        if(n>65535) return -1;
        if(p->used<head+4+n) return 0;
        for(i=0;i<n;++i) b[head+4+i]^=b[head+(i%4)];
        if(control) {
            if(op==8 && n) {
                unsigned code;
                if(n==1) return -1;
                code=((unsigned)b[head+4]<<8)|b[head+5];
                if(code<1000 || code>=5000 || code==1004 || code==1005 || code==1006 || (code>=1015 && code<3000) || !utf8(b+head+6,n-2)) return -1;
            }
            if(cb(op,b+head+4,n,ctx)) return -1;
        } else {
            if((op==0 && !p->fragmented) || (op!=0 && p->fragmented)) return -1;
            if(p->length+n>sizeof(p->message)) return -1;
            if(op) p->fragmented=op;
            memcpy(p->message+p->length,b+head+4,n); p->length+=n;
            if(fin) {
                if(p->fragmented==1 && !utf8(p->message,p->length)) return -1;
                if(cb(p->fragmented,p->message,p->length,ctx)) return -1;
                p->length=0; p->fragmented=0;
            }
        }
        memmove(b,b+head+4+n,p->used-head-4-n); p->used-=head+4+n;
    }
    return 0;
}
int suart_ws_feed(suart_ws_parser *p,const void *data,size_t n,suart_message_fn cb,void *ctx) {
    const unsigned char *b=(const unsigned char *)data;
    while(n) {
        size_t take=sizeof(p->input)-p->used;
        if(!take) return -1;
        if(take>n) take=n;
        memcpy(p->input+p->used,b,take); p->used+=take; b+=take; n-=take;
        if(parse(p,cb,ctx)) return -1;
    }
    return 0;
}
static int nibble(char c) {
    if(c>='0' && c<='9') return c-'0';
    if(c>='a' && c<='f') return c-'a'+10;
    if(c>='A' && c<='F') return c-'A'+10;
    return -1;
}
int suart_hex(const char *s,unsigned char *out,size_t cap,size_t *done) {
    size_t n=0;
    while(*s) {
        int a,b;
        if(*s==' ' || *s==':' || *s=='\t') { ++s; continue; }
        a=nibble(*s++); if(a<0 || !*s) return -1;
        b=nibble(*s++); if(b<0 || n==cap) return -1;
        out[n++]=(unsigned char)((a<<4)|b);
    }
    *done=n; return 0;
}
