#ifndef PORTABLE_COMMAND_LINE_H
#define PORTABLE_COMMAND_LINE_H
/* Optional bounded command parser. Expansion values are copied literally, never
 * reparsed or field-split. All argv/path pointers borrow the plan. No OS calls. */
#include "portable_terminal.h"
#ifndef PS_SHELL_COMMANDS
#define PS_SHELL_COMMANDS 16
#endif
#ifndef PS_SHELL_STORAGE
#define PS_SHELL_STORAGE 4096
#endif
#define PS_SHELL_ARGUMENTS 32
enum {PS_CHAIN_END,PS_CHAIN_ALWAYS,PS_CHAIN_AND,PS_CHAIN_OR,PS_CHAIN_PIPE};
typedef const char *(*ps_shell_lookup_fn)(void *,const char *);
typedef struct {
    int argc;
    const char *argv[PS_SHELL_ARGUMENTS+5];
    const char *input,*output,*error;
    unsigned append,append_error,merge_error,next;
} ps_shell_command;
typedef struct {
    ps_shell_command commands[PS_SHELL_COMMANDS];
    char storage[PS_SHELL_STORAGE];
    unsigned count,used,error_at;
    const char *error;
} ps_shell_plan;
static inline int ps_shell_name(unsigned char c) {
    return (c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='_'||(c>='0'&&c<='9');
}
static inline int ps_shell_char(ps_shell_plan *p,char c) {
    if(p->used>=PS_SHELL_STORAGE){p->error="expanded command too long";return 0;}
    p->storage[p->used++]=c;return 1;
}
static inline int ps_shell_word(ps_shell_plan *p,const char *line,unsigned *at,
                               ps_shell_lookup_fn lookup,void *ctx,const char **word) {
    unsigned start=p->used,quote=0,any=0;
    while(line[*at]){
        char c=line[*at];
        if(!quote&&(c==' '||c=='\t'||c==';'||c=='|'||c=='&'||c=='<'||c=='>'))break;
        ++*at;any=1;
        if(c=='\''||c=='"'){
            if(!quote){quote=(unsigned)c;continue;}
            if(quote==(unsigned)c){quote=0;continue;}
        }
        if(c==96&&quote!='\''){p->error="command substitution is unsupported";return 0;}
        if(c=='\\'&&quote!='\''){
            char n=line[*at];
            if(n=='"'||n=='\\'||n=='$'||n==96||n=='\''||(!quote&&(n==' '||n=='\t'||n=='|'||n=='&'||n==';'||n=='<'||n=='>'||n=='#'))){
                ++*at;c=n;
            }else if(!n){p->error="unfinished escape";return 0;}
        }else if(c=='$'&&quote!='\''){
            char name[128];unsigned n=0,braced=line[*at]=='{';const char *value;
            if(line[*at]=='('){p->error="command substitution is unsupported";return 0;}
            if(braced)++*at;
            if(line[*at]=='?'){name[n++]='?';++*at;}
            else while(ps_shell_name((unsigned char)line[*at])){
                if(n+1>=sizeof name){p->error="variable name too long";return 0;}
                name[n++]=line[(*at)++];
            }
            if(braced){if(line[*at]!='}'||!n){p->error="invalid variable expansion";return 0;}++*at;}
            if(!n){if(!ps_shell_char(p,'$'))return 0;continue;}
            name[n]=0;value=lookup?lookup(ctx,name):"";
            if(value)while(*value)if(!ps_shell_char(p,*value++))return 0;
            continue;
        }
        if(c=='\n'||c=='\r'){p->error="use one command line per input record";return 0;}
        if(!ps_shell_char(p,c))return 0;
    }
    if(quote){p->error="unclosed quote";return 0;}
    if(!any){p->error="expected argument or path";return 0;}
    if(!ps_shell_char(p,0))return 0;
    *word=p->storage+start;return 1;
}
static inline int ps_shell_parse(const char *line,ps_shell_plan *p,ps_shell_lookup_fn lookup,void *ctx) {
    unsigned at=0,pending=0;
    p->count=p->used=p->error_at=0;p->error=0;
    if(!line){p->error="null command";return 0;}
    while(1){
        ps_shell_command *c;
        while(line[at]==' '||line[at]=='\t')++at;
        if(!line[at]||line[at]=='#'){
            if(pending && pending!=PS_CHAIN_ALWAYS){p->error="missing command after operator";goto failed;}
            return 1;
        }
        if(p->count>=PS_SHELL_COMMANDS){p->error="too many commands";goto failed;}
        c=&p->commands[p->count++];c->argc=0;c->input=c->output=c->error=0;
        c->append=c->append_error=c->merge_error=c->next=0;
        for(;;){
            unsigned redir=0,append=0;const char *word;
            while(line[at]==' '||line[at]=='\t')++at;
            if(!line[at]||line[at]=='#')break;
            if(line[at]==';'||line[at]=='|'||line[at]=='&'){
                char op=line[at++];
                if(op==';')c->next=PS_CHAIN_ALWAYS;
                else if(line[at]==op){++at;c->next=op=='|'?PS_CHAIN_OR:PS_CHAIN_AND;}
                else if(op=='|')c->next=PS_CHAIN_PIPE;
                else {p->error="background '&' is unsupported";goto failed;}
                break;
            }
            if(line[at]=='2'&&line[at+1]=='>'){redir=3;at+=2;}
            else if(line[at]=='1'&&line[at+1]=='>'){redir=2;at+=2;}
            else if(line[at]=='>'){redir=2;++at;}
            else if(line[at]=='<'){redir=1;++at;}
            if(redir){
                if(line[at]=='>'&&redir!=1){append=1;++at;}
                if(redir==3&&!append&&line[at]=='&'&&line[at+1]=='1'){
                    at+=2;
                    if(c->error||c->merge_error){p->error="duplicate stderr redirection";goto failed;}
                    c->merge_error=1;continue;
                }
                if(line[at]=='<'||line[at]=='>'||line[at]=='&'){p->error="unsupported redirection";goto failed;}
                while(line[at]==' '||line[at]=='\t')++at;
                if(!ps_shell_word(p,line,&at,lookup,ctx,&word))goto failed;
                if(redir==1){if(c->input){p->error="duplicate input";goto failed;}c->input=word;}
                if(redir==2){if(c->output||c->merge_error){p->error="place stdout redirection before 2>&1";goto failed;}c->output=word;c->append=append;}
                if(redir==3){if(c->error||c->merge_error){p->error="duplicate stderr";goto failed;}c->error=word;c->append_error=append;}
            }else {
                if(c->argc==PS_SHELL_ARGUMENTS){p->error="too many arguments";goto failed;}
                if(!ps_shell_word(p,line,&at,lookup,ctx,&word))goto failed;
                c->argv[c->argc++]=word;
            }
        }
        c->argv[c->argc]=0;
        if(!c->argc){p->error="empty command";goto failed;}
        pending=c->next;
        if(!c->next)return 1;
    }
failed:
    p->error_at=at;return 0;
}
#endif
