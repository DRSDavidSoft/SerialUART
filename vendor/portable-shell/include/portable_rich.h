#ifndef PORTABLE_RICH_H
#define PORTABLE_RICH_H
/* Optional C99 byte renderer. Caller supplies capabilities, storage and scheduling.
 * No allocation, locale, OS APIs, or runtime library dependencies. */
#include "portable_terminal.h"
#include <stdint.h>
#ifndef PS_RICH_MARKUP
#define PS_RICH_MARKUP 1
#endif
#ifndef PS_RICH_DOCUMENT
#define PS_RICH_DOCUMENT 1
#endif
#ifndef PS_RICH_SYNTAX
#define PS_RICH_SYNTAX 1
#endif
enum {PS_COLOR_NONE,PS_COLOR_16,PS_COLOR_256,PS_COLOR_RGB};
enum {PS_TEXT_BOLD=1,PS_TEXT_DIM=2,PS_TEXT_ITALIC=4,PS_TEXT_UNDERLINE=8,
      PS_TEXT_STRIKE=16,PS_TEXT_REVERSE=32,PS_TEXT_BLINK=64};
enum {PS_ALIGN_LEFT,PS_ALIGN_CENTER,PS_ALIGN_RIGHT,PS_ALIGN_FULL};
typedef unsigned (*ps_rich_width_fn)(void *,uint32_t);
typedef struct {
    ps_terminal_write write;void *context;unsigned color;
    ps_rich_width_fn width;void *width_context;
} ps_rich;
static inline void ps_rich_raw(ps_rich *r,const char *s,size_t n){if(r&&r->write&&n)r->write(r->context,(const unsigned char *)s,n);}
static inline void ps_rich_literal(ps_rich *r,const char *s){ps_rich_raw(r,s,ps_text_length(s));}
static inline void ps_rich_spaces(ps_rich *r,unsigned n){while(n--)ps_rich_raw(r," ",1);}
/* Strict UTF-8 decoding; invalid bytes consume one byte and become U+FFFD. */
static inline unsigned ps_rich_decode(const char *s,size_t n,uint32_t *cp){
    unsigned i,k;uint32_t v,min;unsigned char c;
    if(!n){*cp=0;return 0;}c=(unsigned char)s[0];
    if(c<128){*cp=c;return 1;}
    if(c>=0xc2&&c<=0xdf){k=2;v=c&31;min=128;}
    else if(c>=0xe0&&c<=0xef){k=3;v=c&15;min=2048;}
    else if(c>=0xf0&&c<=0xf4){k=4;v=c&7;min=65536;}
    else {*cp=0xfffd;return 1;}
    if(n<k){*cp=0xfffd;return 1;}
    for(i=1;i<k;++i){c=(unsigned char)s[i];if((c&0xc0)!=0x80){*cp=0xfffd;return 1;}v=(v<<6)|(c&63);}
    if(v<min||v>0x10ffff||(v>=0xd800&&v<=0xdfff)){*cp=0xfffd;return 1;}
    *cp=v;return k;
}
/* Conservative cell-width policy. Override for a terminal's Unicode version.
 * Scalar widths, not a full grapheme/ZWJ emoji or bidirectional shaping engine. */
static inline unsigned ps_rich_cell_width(uint32_t c){
    if(c<32||(c>=127&&c<160))return 0;
    if((c>=0x300&&c<=0x36f)||(c>=0x1ab0&&c<=0x1aff)||
       (c>=0x1dc0&&c<=0x1dff)||(c>=0x20d0&&c<=0x20ff)||
       (c>=0xfe00&&c<=0xfe0f)||(c>=0xfe20&&c<=0xfe2f)||c==0x200d||
       (c>=0xe0100&&c<=0xe01ef))return 0;
    return c>=0x1100&&(c<=0x115f||c==0x2329||c==0x232a||
      (c>=0x2e80&&c<=0xa4cf&&c!=0x303f)||(c>=0xac00&&c<=0xd7a3)||
      (c>=0xf900&&c<=0xfaff)||(c>=0xfe10&&c<=0xfe19)||
      (c>=0xfe30&&c<=0xfe6f)||(c>=0xff00&&c<=0xff60)||
      (c>=0xffe0&&c<=0xffe6)||(c>=0x1f300&&c<=0x1faff)||
      (c>=0x20000&&c<=0x3fffd))?2:1;
}
static inline unsigned ps_rich_width(ps_rich *r,uint32_t c){
    return r->width?r->width(r->width_context,c):ps_rich_cell_width(c);
}
/* Input ESC/C0/C1 controls become spaces: user strings cannot inject terminal commands. */
static inline unsigned ps_rich_text(ps_rich *r,const char *s,size_t n,unsigned limit){
    unsigned cells=0,k,w;uint32_t c;
    while(n){k=ps_rich_decode(s,n,&c);w=ps_rich_width(r,c);
        if(c<32||(c>=127&&c<160)){w=1;if(w>limit-cells)break;ps_rich_raw(r," ",1);}
        else {if(w>limit-cells)break;if(c==0xfffd&&k==1&&(unsigned char)*s>=128)ps_rich_literal(r,"\xef\xbf\xbd");else ps_rich_raw(r,s,k);}
        cells+=w;s+=k;n-=k;
    }return cells;
}
static inline unsigned ps_rich_measure(ps_rich *r,const char *s,size_t n){
    unsigned total=0,k;uint32_t c;
    while(n){k=ps_rich_decode(s,n,&c);total+=(c<32||(c>=127&&c<160))?1:ps_rich_width(r,c);s+=k;n-=k;}return total;
}
static inline uint32_t ps_rich_palette(unsigned i){
    static const uint32_t base[16]={0x000000,0x800000,0x008000,0x808000,0x000080,0x800080,0x008080,0xc0c0c0,
        0x808080,0xff0000,0x00ff00,0xffff00,0x0000ff,0xff00ff,0x00ffff,0xffffff};
    unsigned r,g,b;if(i<16)return base[i];
    if(i>=232){r=8+(i-232)*10;return ((uint32_t)r<<16)|((uint32_t)r<<8)|r;}
    i-=16;r=i/36;g=i/6%6;b=i%6;r=r?55+r*40:0;g=g?55+g*40:0;b=b?55+b*40:0;return ((uint32_t)r<<16)|((uint32_t)g<<8)|b;
}
static inline unsigned ps_rich_nearest(uint32_t rgb,unsigned count){
    unsigned i,best=0;uint32_t distance=0xffffffffu;
    for(i=0;i<count;++i){uint32_t p=ps_rich_palette(i);int32_t r=(int32_t)((rgb>>16)&255)-(int32_t)((p>>16)&255);
        int32_t g=(int32_t)((rgb>>8)&255)-(int32_t)((p>>8)&255),b=(int32_t)(rgb&255)-(int32_t)(p&255);
        uint32_t d=(uint32_t)(r*r+g*g+b*b);if(d<distance){distance=d;best=i;}}
    return best;
}
static inline void ps_rich_color(ps_rich *r,uint32_t rgb,unsigned background){
    char b[48];unsigned index;if(!r->color)return;
    if(r->color==PS_COLOR_RGB)ps_terminal_format(b,sizeof b,"\033[%u;2;%u;%u;%um",background?48:38,(unsigned)(rgb>>16)&255,(unsigned)(rgb>>8)&255,(unsigned)rgb&255);
    else {index=ps_rich_nearest(rgb,r->color==PS_COLOR_16?16:256);
        if(r->color==PS_COLOR_16)ps_terminal_format(b,sizeof b,"\033[%um",(background?40:30)+(index%8)+(index>=8?60:0));
        else ps_terminal_format(b,sizeof b,"\033[%u;5;%um",background?48:38,index);}
    ps_rich_literal(r,b);
}
static inline void ps_rich_reset(ps_rich *r){if(r->color)ps_rich_literal(r,"\033[0m");}
static inline void ps_rich_attributes(ps_rich *r,unsigned flags){
    static const unsigned codes[7]={1,2,3,4,9,7,5};unsigned i;char b[16];
    if(!r->color)return;
    for(i=0;i<7;++i)if(flags&(1u<<i)){ps_terminal_format(b,sizeof b,"\033[%um",codes[i]);ps_rich_literal(r,b);}
}
static inline void ps_rich_field(ps_rich *r,const char *s,unsigned width,unsigned align){
    unsigned n=ps_rich_measure(r,s,ps_text_length(s)),pad=n<width?width-n:0,left=align==PS_ALIGN_RIGHT?pad:align==PS_ALIGN_CENTER?pad/2:0;
    ps_rich_spaces(r,left);n=ps_rich_text(r,s,ps_text_length(s),width-left);ps_rich_spaces(r,width-left-n);
}
/* Word wrapping with cell-based alignment. Explicit LF retained. Long words split.
 * Returns line count; width must be at least two cells for CJK. */
static inline unsigned ps_rich_wrap_indented(ps_rich *r,const char *s,unsigned width,unsigned align,unsigned indent){
    unsigned lines=0;if(!s||width<2)return 0;
    while(*s){
        const char *end=s,*last=0,*next;unsigned cells=0,k,w,pad,gaps=0,extra=0,each=0;uint32_t c;size_t n=ps_text_length(s);
        while((size_t)(end-s)<n&&*end!='\n'){
            k=ps_rich_decode(end,n-(size_t)(end-s),&c);w=(c<32||(c>=127&&c<160))?1:ps_rich_width(r,c);
            if(w>width-cells)break;
            if(*end==' ')last=end;
            cells+=w;end+=k;
        }
        next=end;if(*end&&*end!='\n'&&*end!=' '&&last&&last>s){end=last;next=last+1;}
        else if(*next=='\n')++next;
        else while(*next==' ')++next;
        if(end==s&&*s!='\n'){k=ps_rich_decode(s,n,&c);end=s+k;next=end;} /* Defensive custom-width fallback. */
        cells=ps_rich_measure(r,s,(size_t)(end-s));pad=cells<width?width-cells:0;
        if(align==PS_ALIGN_FULL&&*next&&*end!='\n'){const char *p;for(p=s;p<end;++p)if(*p==' ')++gaps;if(gaps){each=pad/gaps;extra=pad%gaps;}}
        ps_rich_spaces(r,indent);
        if(align==PS_ALIGN_CENTER)ps_rich_spaces(r,pad/2);
        else if(align==PS_ALIGN_RIGHT)ps_rich_spaces(r,pad);
        while(s<end){k=ps_rich_decode(s,(size_t)(end-s),&c);ps_rich_text(r,s,k,width);if(*s==' '&&gaps){ps_rich_spaces(r,each+(extra?1:0));if(extra)--extra;}s+=k;}
        ps_rich_literal(r,"\r\n");++lines;s=next;
    }return lines;
}
static inline unsigned ps_rich_wrap(ps_rich *r,const char *s,unsigned width,unsigned align){
    return ps_rich_wrap_indented(r,s,width,align,0);
}
#if PS_RICH_MARKUP
typedef struct {uint32_t rgb;unsigned flags;} ps_rich_span;
static inline int ps_rich_tag(const char *s,size_t n,const char *word){size_t i;for(i=0;i<n&&word[i]&&s[i]==word[i];++i){}return i==n&&!word[i];}
/* Small bounded markup grammar: [bold], [italic], [underline], [dim], [strike],
 * [reverse], [blink], [#RRGGBB], [/] and [[ for literal [. Unknown tags stay literal. */
static inline unsigned ps_rich_markup(ps_rich *r,const char *s){
    ps_rich_span stack[8],current={0xabb2bf,0};unsigned depth=0,errors=0,k;uint32_t cp;
    while(*s){
        if(s[0]=='['&&s[1]=='['){ps_rich_raw(r,"[",1);s+=2;continue;}
        if(*s=='['){const char *end=s+1;unsigned flag=0,valid=0;uint32_t rgb=current.rgb;
            while(*end&&*end!=']'&&end-s<16)++end;
            if(*end==']'){size_t n=(size_t)(end-s-1);const char *tag=s+1;
                if(n==1&&*tag=='/'){if(depth){current=stack[--depth];ps_rich_reset(r);ps_rich_color(r,current.rgb,0);ps_rich_attributes(r,current.flags);}else ++errors;s=end+1;continue;}
                if(ps_rich_tag(tag,n,"bold"))flag=PS_TEXT_BOLD;
                else if(ps_rich_tag(tag,n,"dim"))flag=PS_TEXT_DIM;
                else if(ps_rich_tag(tag,n,"italic"))flag=PS_TEXT_ITALIC;
                else if(ps_rich_tag(tag,n,"underline"))flag=PS_TEXT_UNDERLINE;
                else if(ps_rich_tag(tag,n,"strike"))flag=PS_TEXT_STRIKE;
                else if(ps_rich_tag(tag,n,"reverse"))flag=PS_TEXT_REVERSE;
                else if(ps_rich_tag(tag,n,"blink"))flag=PS_TEXT_BLINK;
                if(flag)valid=1;
                if(n==7&&*tag=='#'){unsigned i;rgb=0;valid=1;for(i=1;i<7;++i){unsigned char c=(unsigned char)tag[i];unsigned h=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:99;if(h>15){valid=0;break;}rgb=(rgb<<4)|h;}}
                if(valid&&depth<8){stack[depth++]=current;current.rgb=rgb;current.flags|=flag;ps_rich_color(r,rgb,0);ps_rich_attributes(r,current.flags);s=end+1;continue;}
                if(valid)++errors;
            }
        }
        if(*s=='\n'){ps_rich_literal(r,"\r\n");++s;continue;}
        k=ps_rich_decode(s,ps_text_length(s),&cp);ps_rich_text(r,s,k,2);s+=k;
    }
    ps_rich_reset(r);return errors+depth;
}
#endif
#if PS_RICH_SYNTAX
/* Lightweight lexical highlighting for C-like code and JSON, not a parser.
 * Pretty formatting stays with the producer; source whitespace is preserved. */
static inline void ps_rich_code(ps_rich *r,const char *s){
    const char *p;unsigned char quote;uint32_t color;
    while(*s){
        p=s;color=0xabb2bf;
        if(*s=='"'||*s=='\''){quote=(unsigned char)*s++;while(*s){if(*s=='\\'&&s[1])s+=2;else if((unsigned char)*s++==quote)break;}color=0x98c379;}
        else if(s[0]=='/'&&s[1]=='/'){while(*s&&*s!='\n')++s;color=0x7f848e;}
        else if(*s>='0'&&*s<='9'){while((*s>='0'&&*s<='9')||*s=='.')++s;color=0xd19a66;}
        else if((*s>='a'&&*s<='z')||(*s>='A'&&*s<='Z')||*s=='_'){
            while((*s>='a'&&*s<='z')||(*s>='A'&&*s<='Z')||(*s>='0'&&*s<='9')||*s=='_')++s;
            color=0x61afef;
        }else ++s;
        ps_rich_color(r,color,0);
        if(*p=='\n')ps_rich_literal(r,"\r\n");
        else if(*p=='\t')ps_rich_spaces(r,4);
        else ps_rich_text(r,p,(size_t)(s-p),(unsigned)-1);
    }ps_rich_reset(r);
}
#endif
#if PS_RICH_DOCUMENT
/* Deliberate Markdown subset: headings, unordered lists, quotes and fenced code.
 * Inline Markdown, links/images and full CommonMark are not interpreted. */
static inline unsigned ps_rich_document(ps_rich *r,const char *s,unsigned width){
    char line[512];unsigned count=0,fenced=0;
    while(*s){unsigned n=0;const char *body=line;
        while(*s&&*s!='\n'){if(n+1<sizeof line)line[n++]=*s;++s;}if(*s)++s;line[n]=0;
        if(n>=3&&line[0]=='~'&&line[1]=='~'&&line[2]=='~'){fenced=!fenced;continue;}
        if(n>=3&&line[0]=='\x60'&&line[1]=='\x60'&&line[2]=='\x60'){fenced=!fenced;continue;}
        if(fenced){
#if PS_RICH_SYNTAX
            ps_rich_code(r,line);
#else
            ps_rich_text(r,line,n,width);
#endif
            ps_rich_literal(r,"\r\n");++count;continue;
        }
        if(*body=='#'){while(*body=='#')++body;if(*body==' ')++body;ps_rich_color(r,0x56b6c2,0);ps_rich_attributes(r,PS_TEXT_BOLD);}
        else if(*body=='>'&&body[1]==' '){ps_rich_color(r,0x7f848e,0);body+=2;}
        else if((*body=='-'||*body=='*')&&body[1]==' '){ps_rich_color(r,0xe5c07b,0);}
        if(!*body){ps_rich_literal(r,"\r\n");++count;}else count+=ps_rich_wrap(r,body,width,PS_ALIGN_LEFT);
        ps_rich_reset(r);
    }return count;
}
#endif
#endif
