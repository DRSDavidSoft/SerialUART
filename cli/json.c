#include "json.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
static int digit(unsigned c) { return c>='0'&&c<='9'; }
static int hex(unsigned c) { return digit(c)?(int)c-'0':c>='a'&&c<='f'?(int)c-'a'+10:c>='A'&&c<='F'?(int)c-'A'+10:-1; }
static void space(j_doc *d) { while(d->pos<d->size && strchr(" \t\r\n",d->text[d->pos])) ++d->pos; }
static int unit(const char *p,unsigned *value) {
    unsigned i,v=0; for(i=0;i<4;++i) { int n=hex((unsigned char)p[i]); if(n<0) return -1; v=v*16+(unsigned)n; } *value=v; return 0;
}
/* Decode one JSON string character; also validates raw UTF-8 and surrogate pairs. */
static int character(const char *p,size_t end,size_t *at,unsigned *value) {
    unsigned c=(unsigned char)p[(*at)++],v,n,min,i;
    if(c<32) return -1;
    if(c=='\\') {
        if(*at>=end) return -1;
        c=(unsigned char)p[(*at)++];
        switch(c) {
        case '"':case '\\':case '/': *value=c; return 0;
        case 'b':*value=8;return 0;case 'f':*value=12;return 0;
        case 'n':*value=10;return 0;case 'r':*value=13;return 0;case 't':*value=9;return 0;
        case 'u':
            if(end-*at<4 || unit(p+*at,&v)) return -1;
            *at+=4;
            if(v>=0xd800 && v<=0xdbff) {
                unsigned low;
                if(end-*at<6 || p[*at]!='\\' || p[*at+1]!='u' || unit(p+*at+2,&low) || low<0xdc00 || low>0xdfff) return -1;
                *at+=6; v=0x10000+((v-0xd800)<<10)+low-0xdc00;
            } else if(v>=0xdc00 && v<=0xdfff) return -1;
            *value=v; return 0;
        default:return -1;
        }
    }
    if(c<128) { *value=c;return 0; }
    if(c>=0xc2 && c<=0xdf) { n=1;v=c&31;min=128; }
    else if(c>=0xe0 && c<=0xef) { n=2;v=c&15;min=2048; }
    else if(c>=0xf0 && c<=0xf4) { n=3;v=c&7;min=65536; }
    else return -1;
    if(end-*at<n) return -1;
    for(i=0;i<n;++i) { c=(unsigned char)p[(*at)++]; if((c&0xc0)!=0x80) return -1; v=(v<<6)|(c&63); }
    if(v<min || v>0x10ffff || (v>=0xd800 && v<=0xdfff)) return -1;
    *value=v; return 0;
}
int j_text(const j_doc *d,int index,char *out,size_t cap) {
    size_t at,end,n=0; unsigned c; const j_token *t;
    if(index<0 || (unsigned)index>=d->used || !cap) return -1;
    t=&d->tokens[index]; if(t->type!=J_STRING) return -1;
    at=t->start+1;end=t->end-1;
    while(at<end) {
        unsigned char b[4]; unsigned count,i;
        if(character(d->text,end,&at,&c)) return -1;
        if(c<128) { b[0]=(unsigned char)c;count=1; }
        else if(c<2048) { b[0]=(unsigned char)(0xc0|(c>>6));b[1]=(unsigned char)(0x80|(c&63));count=2; }
        else if(c<65536) { b[0]=(unsigned char)(0xe0|(c>>12));b[1]=(unsigned char)(0x80|((c>>6)&63));b[2]=(unsigned char)(0x80|(c&63));count=3; }
        else { b[0]=(unsigned char)(0xf0|(c>>18));b[1]=(unsigned char)(0x80|((c>>12)&63));b[2]=(unsigned char)(0x80|((c>>6)&63));b[3]=(unsigned char)(0x80|(c&63));count=4; }
        if(n+count>=cap) return -1;
        for(i=0;i<count;++i) out[n++]=(char)b[i];
    }
    out[n]=0;return (int)n;
}
static int value(j_doc *d,unsigned depth) {
    unsigned i; int index; char c; j_token *t;
    space(d); if(depth>16 || d->pos==d->size || d->used==1024) return -1;
    index=(int)d->used++;t=&d->tokens[index];t->start=(unsigned)d->pos;c=d->text[d->pos++];
    if(c=='{' || c=='[') {
        char close=c=='{'?'}':']';t->type=c=='{'?J_OBJECT:J_ARRAY;space(d);
        if(d->pos<d->size && d->text[d->pos]==close) ++d->pos;
        else for(;;) {
            if(c=='{') {
                int key=value(d,depth+1);char name[129],other[129];int len;
                if(key<0 || (len=j_text(d,key,name,sizeof(name)))<0) return -1;
                for(i=(unsigned)index+1;i<(unsigned)key;i=d->tokens[d->tokens[i].next].next) {
                    int m=j_text(d,(int)i,other,sizeof(other));if(m==len && !memcmp(name,other,(size_t)len)) return -1;
                }
                space(d);if(d->pos==d->size || d->text[d->pos++]!=':') return -1;
            }
            if(value(d,depth+1)<0) return -1;
            ++t->count;space(d);if(d->pos==d->size) return -1;
            if(d->text[d->pos]==close) { ++d->pos;break; }
            if(d->text[d->pos++]!=',') return -1;
        }
    } else if(c=='"') {
        unsigned cp;t->type=J_STRING;
        while(d->pos<d->size && d->text[d->pos]!='"') if(character(d->text,d->size,&d->pos,&cp)) return -1;
        if(d->pos==d->size) return -1;
        ++d->pos;
    } else if(c=='t' || c=='f' || c=='n') {
        const char *word=c=='t'?"true":c=='f'?"false":"null";size_t n=strlen(word);
        if(d->size-t->start<n || memcmp(d->text+t->start,word,n)) return -1;
        d->pos=t->start+n;t->type=c=='t'?J_TRUE:c=='f'?J_FALSE:J_NULL;
    } else {
        size_t p=t->start;t->type=J_NUMBER;
        if(d->text[p]=='-') ++p;
        if(p==d->size || !digit((unsigned char)d->text[p])) return -1;
        if(d->text[p]=='0') ++p;else while(p<d->size && digit((unsigned char)d->text[p])) ++p;
        if(p<d->size && d->text[p]=='.') { ++p;if(p==d->size || !digit((unsigned char)d->text[p])) return -1;while(p<d->size && digit((unsigned char)d->text[p])) ++p; }
        if(p<d->size && (d->text[p]=='e'||d->text[p]=='E')) {
            ++p;if(p<d->size && (d->text[p]=='+'||d->text[p]=='-')) ++p;
            if(p==d->size || !digit((unsigned char)d->text[p])) return -1;
            while(p<d->size && digit((unsigned char)d->text[p])) ++p;
        }
        d->pos=p;
    }
    t->end=(unsigned)d->pos;t->next=d->used;return index;
}
int j_parse(j_doc *d,const char *s,size_t n) {
    memset(d,0,sizeof(*d));d->text=s;d->size=n;
    if(memchr(s,0,n) || value(d,0)<0) return -1;
    space(d);return d->pos==n?0:-1;
}
int j_field(const j_doc *d,int object,const char *name) {
    unsigned i;char key[129];size_t n=strlen(name);
    if(object<0 || d->tokens[object].type!=J_OBJECT) return -1;
    for(i=(unsigned)object+1;i<d->tokens[object].next;i=d->tokens[d->tokens[i].next].next)
        if(j_text(d,(int)i,key,sizeof(key))==(int)n && !memcmp(key,name,n)) return (int)d->tokens[i].next;
    return -1;
}
int j_uint(const j_doc *d,int index,unsigned max,unsigned *out) {
    unsigned n=0,i;const j_token *t;
    if(index<0) return 0;
    t=&d->tokens[index];if(t->type!=J_NUMBER) return -1;
    for(i=t->start;i<t->end;++i) { unsigned c=(unsigned char)d->text[i];if(!digit(c) || n>max/10 || (n==max/10 && c-'0'>max%10)) return -1;n=n*10+c-'0'; }
    *out=n;return 0;
}
void j_raw(j_out *o,const char *s) {
    size_t n=strlen(s);if(o->failed || n>=o->capacity-o->size) { o->failed=1;return; }
    memcpy(o->data+o->size,s,n+1);o->size+=n;
}
void j_print(j_out *o,const char *format,...) {
    va_list args;int n;if(o->failed) return;
    va_start(args,format);n=vsnprintf(o->data+o->size,o->capacity-o->size,format,args);va_end(args);
    if(n<0 || (size_t)n>=o->capacity-o->size) o->failed=1;else o->size+=(size_t)n;
}
void j_quote(j_out *o,const char *s) {
    const unsigned char *p=(const unsigned char *)s;size_t length=strlen(s);j_raw(o,"\"");
    for(;*p;++p) {
        if(*p=='"'||*p=='\\') j_print(o,"\\%c",*p);
        else if(*p<32 || *p==127) j_print(o,"\\u%04x",*p);
        else if(*p>=128) {
            size_t start=(size_t)((const char *)p-s),at=start;unsigned cp;
            if(character(s,length,&at,&cp)) j_raw(o,"\\ufffd");
            else { char b[5]={0};memcpy(b,p,at-start);j_raw(o,b);p+=at-start-1; }
        }
        else { char b[2]={(char)*p,0};j_raw(o,b); }
    }
    j_raw(o,"\"");
}
