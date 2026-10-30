#define _POSIX_C_SOURCE 200809L
#include "session.h"
#include <string.h>
#include <time.h>
#include <stdlib.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif
static FILE *log_open(const char *path,int append) {
#ifdef _WIN32
    WCHAR wide[4096];
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,4096)) return NULL;
    return _wfopen(wide,append?L"ab":L"wb");
#else
    return fopen(path,append?"ab":"wb");
#endif
}
int sc_output_open(sc_session *s) {
    if(s->log_time && (!s->log_path || !s->log_display)) {
        fputs("--log-time requires --log FILE --log-format display\n",stderr); return -1;
    }
    if(s->log_path && !(s->log=log_open(s->log_path,s->log_append))) {
        fprintf(stderr,"Cannot open log: %s\n",s->log_path); return -1;
    }
    return 0;
}
static int log_display(sc_session *s,const unsigned char *b,size_t n) {
    size_t start=0,i;
    if(!s->log || !s->log_display) return 0;
    for(i=0;i<n;++i) {
        if(s->log_time && s->log_line_start) {
            char timestamp[40]; time_t now=time(NULL); struct tm *t=gmtime(&now);
            if(!t || !strftime(timestamp,sizeof(timestamp),"[%Y-%m-%dT%H:%M:%SZ] ",t) || fputs(timestamp,s->log)==EOF) return -1;
            s->log_line_start=0;
        }
        if(b[i]=='\n' || i==n-1) {
            if(fwrite(b+start,1,i-start+1,s->log)!=i-start+1) return -1;
            start=i+1; if(b[i]=='\n') s->log_line_start=1;
        }
    }
    return 0;
}
typedef struct { sc_session *s; unsigned char bytes[8192]; size_t size; } sink;
static int flush(sink *out) {
    if(!out->size) return 0;
    if(fwrite(out->bytes,1,out->size,stdout)!=out->size || log_display(out->s,out->bytes,out->size)) return -1;
    out->size=0; return 0;
}
static int emit(sink *out,unsigned char c) {
    sc_session *s=out->s;
    if(out->size>sizeof(out->bytes)-4 && flush(out)) return -1;
    if(s->force_newline && c=='\r') c='\n';
    if(s->display==2 && (c<32 || c>=127) && c!=8 && c!=9 && c!=10 && c!=13) {
        static const char hex[]="0123456789ABCDEF";
        out->bytes[out->size++]='['; out->bytes[out->size++]=(unsigned char)hex[c>>4];
        out->bytes[out->size++]=(unsigned char)hex[c&15]; out->bytes[out->size++]=']';
    } else out->bytes[out->size++]=s->display==1 && c>=128?'?':c;
    return 0;
}
static int display_byte(sink *out,unsigned char c) {
    sc_session *s=out->s; size_t i;
    if(!s->no_status) return emit(out,c);
again:
    switch(s->osc_state) {
    case 0:
        if(c==27) { s->osc_state=1; return 0; }
        return emit(out,c);
    case 1:
        if(c==']') { s->osc_header[0]=27; s->osc_header[1]=']'; s->osc_size=2; s->osc_state=2; return 0; }
        s->osc_state=0; if(emit(out,27)) return -1; goto again;
    case 2:
        if(s->osc_size<sizeof(s->osc_header)) s->osc_header[s->osc_size++]=c;
        else {
            for(i=0;i<s->osc_size;++i) if(emit(out,s->osc_header[i])) return -1;
            s->osc_state=0; s->osc_size=0; goto again;
        }
        if(c==';' && s->osc_size==4 && (s->osc_header[2]=='0' || s->osc_header[2]=='2')) { s->osc_state=3; return 0; }
        if(c==';' || c<32 || c==127) {
            for(i=0;i<s->osc_size;++i) if(emit(out,s->osc_header[i])) return -1;
            s->osc_state=0; s->osc_size=0;
        }
        return 0;
    case 3:
        if(c==7) s->osc_state=0;
        else if(c==27) s->osc_state=4;
        return 0;
    default:
        if(c=='\\' || c==7) s->osc_state=0;
        else if(c!=27) s->osc_state=3;
        return 0;
    }
}
int sc_output(sc_session *s,const unsigned char *b,size_t n,int echo) {
    sink out; size_t i; out.s=s; out.size=0;
    if(s->log && !s->log_display && !echo && fwrite(b,1,n,s->log)!=n) return -1;
    for(i=0;i<n;++i) if(display_byte(&out,b[i])) return -1;
    if(flush(&out)) return -1;
    return s->log && fflush(s->log)?-1:0;
}
int sc_output_close(sc_session *s) {
    sink out; size_t i; int rc=0; out.s=s; out.size=0;
    if(s->osc_state==1) { if(emit(&out,27)) rc=-1; }
    else if(s->osc_state==2) for(i=0;i<s->osc_size;++i) if(emit(&out,s->osc_header[i])) rc=-1;
    if(flush(&out)) rc=-1;
    if(s->log && fclose(s->log)) rc=-1;
    s->log=NULL; return rc;
}
int sc_input(sc_session *s,unsigned char c,unsigned char *out,size_t cap,size_t *n) {
    *n=0;
    if(c=='\n' && s->input_cr) {
        s->input_cr=0;
        if(s->line_buffer || s->enter_size) return 0;
    }
    s->input_cr=c=='\r';
    if(s->line_buffer && (c==8 || c==127)) {
        if(s->line_size) {
            --s->line_size;
            while(s->line_size && (s->line[s->line_size]&0xc0)==0x80) --s->line_size;
            if(s->echo && sc_output(s,(const unsigned char *)"\b \b",3,1)) return -1;
        }
        return 0;
    }
    if(c=='\r' || c=='\n') {
        size_t ending=s->enter_size?s->enter_size:1;
        if(s->line_size+ending>cap) return -1;
        memcpy(out,s->line,s->line_size); *n=s->line_size; s->line_size=0;
        if(s->enter_size) memcpy(out+*n,s->enter,ending); else out[*n]=c;
        *n+=ending;
        if(s->echo && sc_output(s,(const unsigned char *)"\r\n",2,1)) return -1;
    } else if(s->line_buffer) {
        if(s->line_size==sizeof(s->line)) return -1;
        s->line[s->line_size++]=c;
        if(s->echo && sc_output(s,&c,1,1)) return -1;
    } else {
        if(!cap) return -1;
        out[0]=c; *n=1;
        if(s->echo && sc_output(s,&c,1,1)) return -1;
    }
    return 0;
}
#ifdef _WIN32
static DWORD saved_mode; static int output_changed,title_changed; static WCHAR saved_title[32768];
static void restore_console(void) {
    if(output_changed) SetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE),saved_mode);
    if(title_changed) SetConsoleTitleW(saved_title);
}
#else
static int pushed_title;
static void restore_console(void) { if(pushed_title) { fputs("\033[23;0t",stdout); fflush(stdout); } }
#endif
void sc_console(sc_session *s) {
    static int initialized;
#ifdef _WIN32
    DWORD mode; HANDLE h=GetStdHandle(STD_OUTPUT_HANDLE);
    if(!GetConsoleMode(h,&mode)) return;
    if(!initialized) {
        saved_mode=mode;
        output_changed=SetConsoleMode(h,mode|ENABLE_VIRTUAL_TERMINAL_PROCESSING)!=0;
    }
    if(s->title) {
        WCHAR title[512];
        if(!title_changed) GetConsoleTitleW(saved_title,32768);
        if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s->title,-1,title,512)) {
            SetConsoleTitleW(title); title_changed=1;
        }
    }
#else
    if(!isatty(1)) return;
    if(s->title) {
        if(!pushed_title) fputs("\033[22;0t",stdout);
        pushed_title=1; fprintf(stdout,"\033]0;%s\007",s->title);
    }
#endif
    if(!initialized) { atexit(restore_console); initialized=1; }
    if(s->clear_screen) fputs("\033[2J\033[H",stdout);
    fflush(stdout);
}
