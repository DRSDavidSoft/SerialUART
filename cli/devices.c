#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "session.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
typedef struct { unsigned vid,pid; const char *make,*model; } sc_board_entry;
typedef struct { unsigned vid; const char *make; } sc_vendor_entry;
#include "boards.inc"
void sc_board(const sc_session *s,const suart_device *d,char *name,size_t n) {
    size_t i; name[0]=0;
    for(i=s->alias_count;i>0;--i) if(s->aliases[i-1].vid==d->vid && s->aliases[i-1].pid==d->pid) {
        snprintf(name,n,"%s",s->aliases[i-1].name); return;
    }
    for(i=0;i<sizeof(board_data)/sizeof(board_data[0]);++i) if(board_data[i].vid==d->vid && board_data[i].pid==d->pid) {
        snprintf(name,n,"%s %s",board_data[i].make,board_data[i].model); return;
    }
    for(i=0;i<sizeof(vendor_data)/sizeof(vendor_data[0]);++i) if(vendor_data[i].vid==d->vid) {
        snprintf(name,n,"%s (unknown model)",vendor_data[i].make); return;
    }
}
void sc_boards(const sc_session *s) {
    size_t i;
    puts("VID:PID\tBoard catalog (model recognition does not prove running firmware)");
    for(i=0;i<sizeof(board_data)/sizeof(board_data[0]);++i)
        printf("%04X:%04X\t%s %s\n",board_data[i].vid,board_data[i].pid,board_data[i].make,board_data[i].model);
    for(i=0;i<s->alias_count;++i) printf("%04X:%04X\t%s [custom override]\n",s->aliases[i].vid,s->aliases[i].pid,s->aliases[i].name);
}
static int contains(const char *text,const char *part) {
    size_t n=strlen(part),i;
    for(;*text;++text) {
        for(i=0;i<n && text[i] && tolower((unsigned char)text[i])==tolower((unsigned char)part[i]);++i) {}
        if(i==n) return 1;
    }
    return !n;
}
static int path_equal(const char *a,const char *b) {
#ifdef _WIN32
    if(!strncmp(a,"\\\\.\\",4)) a+=4;
    if(!strncmp(b,"\\\\.\\",4)) b+=4;
    return !_stricmp(a,b);
#else
    char aa[PATH_MAX],bb[PATH_MAX];
    return !strcmp(a,b) || (realpath(a,aa) && realpath(b,bb) && !strcmp(aa,bb));
#endif
}
static int rule_match(const sc_session *s,const sc_rule *r,const suart_device *d) {
    unsigned i; char vid[8],pid[8],board[512]; const char *fields[6];
    snprintf(vid,sizeof(vid),"%04X",d->vid); snprintf(pid,sizeof(pid),"%04X",d->pid); sc_board(s,d,board,sizeof(board));
    fields[0]=d->path; fields[1]=d->name; fields[2]=d->serial; fields[3]=vid; fields[4]=pid; fields[5]=board;
    for(i=0;i<6;++i) if(r->present&(1u<<i)) {
        if(r->loose&(1u<<i)) { if(!contains(fields[i],r->value[i])) return 0; }
        else if(i==0) { if(!path_equal(fields[i],r->value[i])) return 0; }
        else if(strcmp(fields[i],r->value[i])) return 0;
    }
    return 1;
}
int sc_device_state(const sc_session *s,const suart_device *d) {
    unsigned i; int has_include=0,included=0,excluded=0;
    for(i=0;i<s->rule_count;++i) {
        const sc_rule *r=&s->rules[i]; int matched=rule_match(s,r,d);
        if(r->kind==1) { has_include=1; if(matched) included=1; }
        if(r->kind==2 && matched) excluded=1;
        if(r->kind==3 && matched) return 2;
    }
    return excluded || (has_include && !included)?1:0;
}
void sc_filters(const sc_session *s) {
    static const char *fields[]={"port","name","serial","vid","pid","board"}; unsigned i,j;
    for(i=0;i<s->rule_count;++i) {
        const sc_rule *r=&s->rules[i]; printf("%s\t",r->kind==1?"include":r->kind==2?"exclude":"block");
        for(j=0;j<6;++j) if(r->present&(1u<<j)) printf("%s%c%s ",fields[j],r->loose&(1u<<j)?'~':'=',r->value[j]);
        putchar('\n');
    }
}
static int list_device(const suart_device *d,void *ctx) {
    sc_session *s=(sc_session *)ctx; char board[512]; int state=sc_device_state(s,d);
    if(state && s->list_kind!=2) return 0;
    sc_board(s,d,board,sizeof(board));
    printf("%s\t%s\t%04X:%04X\t%s\t%s\t%s\n",d->path,d->name,d->vid,d->pid,d->serial,board,
           state==2?"blocked":state==1?"excluded":"available"); return 0;
}
int sc_list(sc_session *s) { return suart_enumerate(list_device,s); }
typedef struct { const sc_session *s; const suart_selector *selector; suart_device *devices; size_t count; int overflow,manual,seen_path; } scan;
static int candidate(const suart_device *d,void *ctx) {
    scan *c=(scan *)ctx; const suart_selector *s=c->selector; char board[512];
    if(s->path && !path_equal(s->path,d->path)) return 0;
    if(s->path) c->seen_path=1;
    if(s->name && strcmp(s->name,d->name)) return 0;
    if(s->serial && strcmp(s->serial,d->serial)) return 0;
    if((s->vid && s->vid!=d->vid) || (s->pid && s->pid!=d->pid)) return 0;
    if(c->s->board_name) { sc_board(c->s,d,board,sizeof(board)); if(strcmp(c->s->board_name,board)) return 0; }
    if(c->manual!=2 && sc_device_state(c->s,d)>(c->manual?1:0)) return 0;
    if(c->count==256) { c->overflow=1; return 1; }
    c->devices[c->count++]=*d; return 0;
}
static int score(const sc_session *s,const suart_device *d) {
    size_t i;
    if(!s->prefer_boards) return 0;
    if(contains(d->name,"CircuitPython")) return 2;
    for(i=0;i<s->alias_count;++i) if(s->aliases[i].vid==d->vid && s->aliases[i].pid==d->pid) return 1;
    for(i=0;i<sizeof(board_data)/sizeof(board_data[0]);++i)
        if(board_data[i].vid==d->vid && board_data[i].pid==d->pid) return 1;
    return 0;
}
static int compare_device(const sc_session *s,const suart_device *aa,const suart_device *bb) {
    int sa=score(s,aa),sb=score(s,bb); if(sa!=sb) return sb-sa;
#ifdef _WIN32
    if(!_strnicmp(aa->path,"COM",3) && !_strnicmp(bb->path,"COM",3)) {
        int n=atoi(aa->path+3)-atoi(bb->path+3); if(n) return n;
    }
#endif
    return strcmp(aa->path,bb->path);
}
int sc_open(sc_session *s,const suart_selector *sel,const suart_config *config,suart_device *device,suart_port **port,unsigned long *error) {
    suart_selector pinned={0}; scan c; int rc; size_t i;
    *port=NULL; *error=0;
    if(s->pinned && s->auto_connect==1) {
        pinned=*sel;
        if(s->identity.serial[0]) { pinned.serial=s->identity.serial; pinned.vid=s->identity.vid; pinned.pid=s->identity.pid; }
        else { pinned.path=s->identity.path; pinned.vid=s->identity.vid; pinned.pid=s->identity.pid; }
        sel=&pinned;
    }
    if(!s->auto_connect && !s->board_name) {
        rc=suart_resolve(sel,device); if(rc) return rc;
        /* Resolve available metadata even for a path-only manual selection. */
        if(sel->path && !sel->name && !sel->serial && !sel->vid && !sel->pid) {
            memset(&c,0,sizeof(c)); c.s=s; c.selector=sel; c.manual=2;
            c.devices=(suart_device *)calloc(256,sizeof(*c.devices)); if(!c.devices) return SUART_ERROR;
            rc=suart_enumerate(candidate,&c);
            if(!rc && c.count==1) *device=c.devices[0];
            free(c.devices);
            if(rc && s->rule_count) return rc;
        }
        if(sc_device_state(s,device)==2) return SUART_INVALID;
        return suart_open_ex(port,device->path,config,error);
    }
    memset(&c,0,sizeof(c)); c.s=s; c.selector=sel; c.manual=!s->auto_connect;
    c.devices=(suart_device *)calloc(256,sizeof(*c.devices)); if(!c.devices) return SUART_ERROR;
    rc=suart_enumerate(candidate,&c);
    if(!rc && c.overflow) rc=SUART_ERROR;
    /* Explicit virtual paths such as PTYs are not discovery entries. */
    if(!rc && !c.count && !c.seen_path && sel->path && !s->board_name) {
        suart_device d; memset(&d,0,sizeof(d));
        if(strlen(sel->path)>=sizeof(d.path)) rc=SUART_INVALID;
        else {
            snprintf(d.path,sizeof(d.path),"%s",sel->path);
            if(!sel->name && !sel->serial && !sel->vid && !sel->pid && sc_device_state(s,&d)==0) c.devices[c.count++]=d;
        }
    }
    if(!rc && !c.count) rc=SUART_NOT_FOUND;
    if(!rc && (!s->auto_connect || s->pinned) && c.count>1) rc=SUART_AMBIGUOUS;
    if(!rc) {
        for(i=1;i<c.count;++i) {
            suart_device item=c.devices[i]; size_t j=i;
            while(j && compare_device(s,&item,&c.devices[j-1])<0) { c.devices[j]=c.devices[j-1]; --j; }
            c.devices[j]=item;
        }
        rc=SUART_NOT_FOUND;
        for(i=0;i<c.count;++i) {
            *device=c.devices[i]; rc=suart_open_ex(port,device->path,config,error);
            if(!rc) {
                if(s->auto_connect==1 && !s->pinned) { s->identity=*device; s->pinned=1; }
                break;
            }
            if(rc==SUART_INVALID || rc==SUART_UNSUPPORTED || s->pinned || !s->auto_connect) break;
        }
    }
    free(c.devices); return rc;
}
