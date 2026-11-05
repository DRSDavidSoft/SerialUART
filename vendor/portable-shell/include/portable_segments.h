#ifndef PORTABLE_SEGMENTS_H
#define PORTABLE_SEGMENTS_H
/* MIT. Byte-only seven-segment display; no allocation, clock or platform calls.
 * Segment order: a(top), b, c, d(bottom), e, f, g(middle), decimal point.
 * Each render overwrites a fixed rectangle and ends each row with CRLF.
 * Caller owns positioning, repaint scheduling, transport and observed values. */
#include "portable_terminal.h"
enum { PS_SEG_A=1, PS_SEG_B=2, PS_SEG_C=4, PS_SEG_D=8,
       PS_SEG_E=16, PS_SEG_F=32, PS_SEG_G=64, PS_SEG_DP=128,
       PS_SEG_COLON=256, PS_SEG_MAX_CELLS=32 };
typedef struct {
    unsigned horizontal, vertical; /* 1..4 horizontal cells; 1..3 vertical cells */
    unsigned unicode, color, show_inactive;
    unsigned foreground, inactive; /* ANSI 256-color indices, 0..255 */
} ps_segment_style;
static inline ps_segment_style ps_segment_default_style(void){
    ps_segment_style s={3,2,1,1,1,208,238};return s;
}
static inline unsigned ps_segment_mask(unsigned c){
    static const unsigned digits[]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
    if(c>='0'&&c<='9')return digits[c-'0'];
    switch(c){
    case 'A':case 'a':return 0x77;case 'B':case 'b':return 0x7c;
    case 'C':return 0x39;case 'c':return 0x58;case 'D':case 'd':return 0x5e;
    case 'E':case 'e':return 0x79;case 'F':case 'f':return 0x71;
    case 'H':case 'h':return 0x76;case 'L':case 'l':return 0x38;
    case 'P':case 'p':return 0x73;case 'U':case 'u':return 0x3e;
    case 'o':return 0x5c;case 'r':return 0x50;case 'n':return 0x54;
    case '-':return PS_SEG_G;case '_':return PS_SEG_D;case ' ':return 0;
    case ':':return PS_SEG_COLON;default:return 0xffff;
    }
}
/* Returns cell count, or -1 for unsupported characters/capacity.
 * '.' attaches to the preceding digit; a leading/repeated dot uses a blank cell.
 * On failure the caller must discard the partially written output. */
static inline int ps_segment_parse(const char *text,unsigned short *cells,unsigned capacity){
    unsigned n=0,c,mask;
    if(!text||!cells||capacity>PS_SEG_MAX_CELLS)return -1;
    while((c=(unsigned char)*text++)!=0){
        if(c=='.'){
            if(n&&!(cells[n-1]&(PS_SEG_DP|PS_SEG_COLON))){cells[n-1]|=PS_SEG_DP;continue;}
            mask=PS_SEG_DP;
        }else{mask=ps_segment_mask(c);if(mask==0xffff)return -1;}
        if(n==capacity)return -1;
        cells[n++]=(unsigned short)mask;
    }
    return (int)n;
}
static inline void ps_segment_emit(ps_terminal_write write,void *ctx,const char *text){
    size_t n=0;while(text[n])++n;write(ctx,(const unsigned char *)text,n);
}
static inline unsigned ps_segment_width(const unsigned short *cells,unsigned count,const ps_segment_style *s){
    unsigned i,w=0;
    if(!cells||!s||!count||count>PS_SEG_MAX_CELLS||s->horizontal<1||s->horizontal>4||s->vertical<1||s->vertical>3)return 0;
    for(i=0;i<count;++i){if(cells[i]&~511U)return 0;w+=(cells[i]&PS_SEG_COLON)?2:s->horizontal+4;}
    return w;
}
/* Returns rows emitted, zero if invalid or wider than max_columns. No truncation.
 * Monochrome never emits SGR; inactive segments remain blank in that mode. */
static inline unsigned ps_segment_render(const unsigned short *cells,unsigned count,const ps_segment_style *s,
                                         unsigned max_columns,ps_terminal_write write,void *ctx){
    unsigned i,x,y,mask,bit,on,width=ps_segment_width(cells,count,s),height;
    int previous=-1;char escape[24];
    if(!write||!width||width>max_columns||s->foreground>255||s->inactive>255)return 0;
    height=2*s->vertical+3;
    for(y=0;y<height;++y){
        for(i=0;i<count;++i){
            mask=cells[i];width=(mask&PS_SEG_COLON)?2:s->horizontal+4;
            for(x=0;x<width;++x){
                const char *glyph=" ";bit=0;
                if(mask&PS_SEG_COLON){
                    if(x==0&&(y==s->vertical||y==s->vertical+2)){bit=PS_SEG_COLON;glyph=s->unicode?"●":"o";}
                }else if(x==width-2&&y==height-1){bit=PS_SEG_DP;glyph=s->unicode?"●":".";}
                else if(x>0&&x<=s->horizontal){
                    if(y==0)bit=PS_SEG_A;
                    else if(y==s->vertical+1)bit=PS_SEG_G;
                    else if(y==height-1)bit=PS_SEG_D;
                    glyph=s->unicode?"━":"-";
                }else if((x==0||x==s->horizontal+1)&&y>0&&y<height-1&&y!=s->vertical+1){
                    bit=y<s->vertical+1?(x?PS_SEG_B:PS_SEG_F):(x?PS_SEG_C:PS_SEG_E);
                    glyph=s->unicode?"┃":"|";
                }
                on=!!(mask&bit);
                if(!bit||(!on&&(!s->show_inactive||!s->color)))glyph=" ";
                if(s->color&&bit&&previous!=(int)on){
                    ps_terminal_format(escape,sizeof escape,"\x1b[38;5;%um",on?s->foreground:s->inactive);
                    ps_segment_emit(write,ctx,escape);previous=(int)on;
                }
                ps_segment_emit(write,ctx,glyph);
            }
        }
        ps_segment_emit(write,ctx,"\r\n");
    }
    if(s->color)ps_segment_emit(write,ctx,"\x1b[0m");
    return height;
}
#endif
