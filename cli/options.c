#include "session.h"
#include "protocol.h"
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
void sc_init(sc_session *s) { memset(s,0,sizeof(*s)); s->bulk=1; s->log_append=1; s->log_line_start=1; }
static int toggle(const char *v,int *out) {
    if(!strcmp(v,"on")) *out=1;
    else if(!strcmp(v,"off")) *out=0;
    else return -1;
    return 1;
}
static int usb_id(const char *v,unsigned *out) {
    char *end; unsigned long n; size_t len=strlen(v);
    if(!len || len>4) return -1;
    errno=0; n=strtoul(v,&end,16);
    if(errno || *end || n>65535 || !isxdigit((unsigned char)*v)) return -1;
    *out=(unsigned)n; return 0;
}
static int add_rule(sc_session *s,int kind,const char *value) {
    static const char *keys[]={"port","name","serial","vid","pid","board"};
    sc_rule *r; char copy[4096],*field,*next;
    if(s->rule_count==64 || !*value || strlen(value)>=sizeof(copy)) return -1;
    r=&s->rules[s->rule_count]; memset(r,0,sizeof(*r)); r->kind=kind; strcpy(copy,value); field=copy;
    do {
        char *op; size_t i,n; next=strchr(field,','); if(next) *next++=0;
        op=strpbrk(field,"=~"); if(!op) return -1;
        n=(size_t)(op-field);
        for(i=0;i<6;++i) if(strlen(keys[i])==n && !strncmp(field,keys[i],n)) break;
        if(i==6 || !op[1] || strlen(op+1)>=sizeof(r->value[i]) || (r->present&(1u<<i))) return -1;
        r->present|=1u<<i; if(*op=='~') r->loose|=1u<<i;
        if(!strcmp(op+1,"*")) { r->present &= ~(1u<<i); }
        else if((i==3 || i==4) && *op=='=') {
            unsigned id; if(usb_id(op+1,&id)) return -1;
            snprintf(r->value[i],sizeof(r->value[i]),"%04X",id);
        } else strcpy(r->value[i],op+1);
        field=next;
    } while(field);
    if(!r->present) return -1;
    ++s->rule_count; return 1;
}
static int alias(sc_session *s,const char *v) {
    const char *colon=strchr(v,':'),*eq=strchr(v,'='); char vid[5],pid[5]; size_t n,m; sc_alias *a;
    if(s->alias_count==64 || !colon || !eq || colon>=eq || !eq[1] || strlen(eq+1)>=512) return -1;
    n=(size_t)(colon-v); m=(size_t)(eq-colon-1);
    if(!n || n>4 || !m || m>4) return -1;
    memcpy(vid,v,n); vid[n]=0; memcpy(pid,colon+1,m); pid[m]=0;
    a=&s->aliases[s->alias_count];
    if(usb_id(vid,&a->vid) || usb_id(pid,&a->pid) || !a->vid || !a->pid) return -1;
    strcpy(a->name,eq+1); ++s->alias_count; return 1;
}
int sc_option(sc_session *s,const char *a,const char *v) {
    static const char *options[]={"--display","--log","--log-mode","--log-format","--force-newline","--status","--title",
        "--clear-screen","--echo","--line-buffer","--bulk-send","--exit-key","--enter","--enter-hex",
        "--autoconnect","--include","--exclude","--block","--board","--board-name"};
    size_t i;
    if(!strcmp(a,"--log-time")) { s->log_time=1; return 1; }
    if(!strcmp(a,"--list")) { s->list_kind=1; return 1; }
    if(!strcmp(a,"--list-all")) { s->list_kind=2; return 1; }
    if(!strcmp(a,"--list-boards")) { s->list_kind=3; return 1; }
    if(!strcmp(a,"--list-filters")) { s->list_kind=4; return 1; }
    if(!strcmp(a,"--list-settings")) { s->list_kind=5; return 1; }
    for(i=0;i<sizeof(options)/sizeof(options[0]);++i) if(!strcmp(a,options[i])) break;
    if(i==sizeof(options)/sizeof(options[0])) return 0;
    if(!v) return 2;
    if(!strcmp(a,"--display")) {
        if(!strcmp(v,"utf8")) s->display=0;
        else if(!strcmp(v,"ascii")) s->display=1;
        else if(!strcmp(v,"raw")) s->display=2;
        else return -1;
    } else if(!strcmp(a,"--log")) { if(!*v) return -1; s->log_path=v; }
    else if(!strcmp(a,"--log-mode")) {
        if(!strcmp(v,"append")) s->log_append=1;
        else if(!strcmp(v,"overwrite")) s->log_append=0;
        else return -1;
    } else if(!strcmp(a,"--log-format")) {
        if(!strcmp(v,"raw")) s->log_display=0;
        else if(!strcmp(v,"display")) s->log_display=1;
        else return -1;
    } else if(!strcmp(a,"--force-newline")) return toggle(v,&s->force_newline);
    else if(!strcmp(a,"--status")) { int on; if(toggle(v,&on)<0) return -1; s->no_status=!on; }
    else if(!strcmp(a,"--echo")) return toggle(v,&s->echo);
    else if(!strcmp(a,"--line-buffer")) return toggle(v,&s->line_buffer);
    else if(!strcmp(a,"--bulk-send")) return toggle(v,&s->bulk);
    else if(!strcmp(a,"--clear-screen")) return toggle(v,&s->clear_screen);
    else if(!strcmp(a,"--title")) {
        const unsigned char *p=(const unsigned char *)v; if(strlen(v)>240) return -1;
        for(;*p;++p) if(*p<32 || *p==127) return -1;
        s->title=v; s->no_status=1;
    } else if(!strcmp(a,"--exit-key")) {
        if(strlen(v)!=1 || !isalpha((unsigned char)*v) || (unsigned char)*v>127) return -1;
        s->exit_key=toupper((unsigned char)*v)-'A'+1;
    } else if(!strcmp(a,"--enter")) {
        if(!strcmp(v,"cr")) { s->enter[0]='\r'; s->enter_size=1; }
        else if(!strcmp(v,"lf")) { s->enter[0]='\n'; s->enter_size=1; }
        else if(!strcmp(v,"crlf")) { memcpy(s->enter,"\r\n",2); s->enter_size=2; }
        else if(!strncmp(v,"text:",5)) {
            if(!v[5] || strlen(v+5)>sizeof(s->enter)) return -1;
            s->enter_size=strlen(v+5); memcpy(s->enter,v+5,s->enter_size);
        } else if(!strcmp(v,"preserve")) s->enter_size=0;
        else return -1;
    } else if(!strcmp(a,"--enter-hex")) {
        if(suart_hex(v,s->enter,sizeof(s->enter),&s->enter_size) || !s->enter_size) return -1;
    } else if(!strcmp(a,"--autoconnect")) {
        if(!strcmp(v,"none")) s->auto_connect=0;
        else if(!strcmp(v,"one")) s->auto_connect=1;
        else if(!strcmp(v,"any")) s->auto_connect=2;
        else return -1;
    } else if(!strcmp(a,"--include")) return add_rule(s,1,v);
    else if(!strcmp(a,"--exclude")) return add_rule(s,2,v);
    else if(!strcmp(a,"--block")) return add_rule(s,3,v);
    else if(!strcmp(a,"--board")) return alias(s,v);
    else if(!strcmp(a,"--board-name")) { if(!*v) return -1; s->board_name=v; }
    return 1;
}
void sc_help(void) {
    puts("\nSimplySerial-inspired terminal and session features:\n"
         "  --config FILE             One long option per line; CLI settings win\n"
         "  --profiles                Opt into global + .serialuart/settings.cfg\n"
         "  --list-settings | --list-boards | --list-filters | --list-all\n"
         "  --autoconnect none|one|any Opt into first eligible port; ONE pins identity\n"
         "  --board VID:PID=NAME       Custom USB model name (repeatable)\n"
         "  --board-name NAME          Select an exact recognized model name\n"
         "  --include RULE --exclude RULE --block RULE (repeatable)\n"
         "    RULE: port=/dev/ttyUSB0,name~adapter,vid=2341 (AND within rule)\n"
         "    Fields: port,name,serial,vid,pid,board; = exact, ~ contains ignoring case\n"
         "  --log FILE                 RX capture, append by default, flushed promptly\n"
         "  --log-mode append|overwrite --log-format raw|display --log-time\n"
         "  --display utf8|ascii|raw    Pass bytes / ASCII replacement / [XX] controls\n"
         "  --force-newline on|off     Replace received CR with LF\n"
         "  --status on|off            Allow/strip OSC 0/2 title updates\n"
         "  --echo on|off --line-buffer on|off --bulk-send on|off\n"
         "  --enter preserve|cr|lf|crlf|text:TEXT --enter-hex HEX\n"
         "  --exit-key LETTER          Ctrl+letter exits terminal (prefix q still works)\n"
         "  --title TEXT --clear-screen on|off (terminal only)\n"
         "Profiles, display transforms and auto-selection are opt-in; pipes stay byte-exact by default.");
}
