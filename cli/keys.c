#include "session.h"
static size_t encode(unsigned cp,unsigned char *b) {
    if(cp<0x80) { b[0]=(unsigned char)cp; return 1; }
    if(cp<0x800) { b[0]=(unsigned char)(0xc0|(cp>>6)); b[1]=(unsigned char)(0x80|(cp&63)); return 2; }
    if(cp<0x10000) { b[0]=(unsigned char)(0xe0|(cp>>12)); b[1]=(unsigned char)(0x80|((cp>>6)&63)); b[2]=(unsigned char)(0x80|(cp&63)); return 3; }
    b[0]=(unsigned char)(0xf0|(cp>>18)); b[1]=(unsigned char)(0x80|((cp>>12)&63));
    b[2]=(unsigned char)(0x80|((cp>>6)&63)); b[3]=(unsigned char)(0x80|(cp&63)); return 4;
}
size_t sc_utf16(unsigned *lead,unsigned unit,unsigned char out[8]) {
    size_t n=0;
    if(*lead) {
        unsigned high=*lead; *lead=0;
        if(unit>=0xdc00 && unit<=0xdfff) return encode(0x10000+((high-0xd800)<<10)+unit-0xdc00,out);
        n=encode(0xfffd,out);
    }
    if(unit>=0xd800 && unit<=0xdbff) { *lead=unit; return n; }
    if(unit>0xffff || (unit>=0xdc00 && unit<=0xdfff)) unit=0xfffd;
    return n+encode(unit,out+n);
}
const char *sc_key_sequence(unsigned key) {
    static const char *function[]={"\033[11~","\033[12~","\033[13~","\033[14~","\033[15~","\033[17~",
        "\033[18~","\033[19~","\033[20~","\033[21~","\033[23~","\033[24~"};
    if(key>=0x70 && key<=0x7b) return function[key-0x70];
    switch(key) {
    case 0x26:return "\033[A";case 0x28:return "\033[B";case 0x27:return "\033[C";case 0x25:return "\033[D";
    case 0x24:return "\033[H";case 0x23:return "\033[F";case 0x2d:return "\033[2~";case 0x2e:return "\033[3~";
    case 0x21:return "\033[5~";case 0x22:return "\033[6~";
    default:return NULL;
    }
}
