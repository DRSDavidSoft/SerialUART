#define _POSIX_C_SOURCE 200809L
#include "session.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
static char *expanded[1025]; static int used; static size_t total;
static void cleanup(void) { int i; for(i=0;i<used;++i) free(expanded[i]); }
static int add(const char *s) {
    size_t n=strlen(s); char *p;
    if(used==1024 || n>262144 || total+n>1048576) return -1;
    p=(char *)malloc(n+1); if(!p) return -1;
    memcpy(p,s,n+1); expanded[used++]=p; expanded[used]=NULL; total+=n; return 0;
}
static char *trim(char *s) {
    char *end; while(isspace((unsigned char)*s)) ++s;
    end=s+strlen(s); while(end>s && isspace((unsigned char)end[-1])) --end;
    *end=0; return s;
}
static int allowed(const char *s) {
    static const char *keys[]={"--port","--name","--serial","--vid","--pid","--baud","--data-bits","--parity","--stop-bits",
        "--rts-cts","--dtr","--boot-wait","--timeout","--write-timeout","--reconnect","--terminal","--receive","--quiet",
        "--discard-input","--show-config","--log","--log-mode","--log-format","--log-time","--display","--force-newline",
        "--status","--title","--clear-screen","--echo","--line-buffer","--bulk-send","--exit-key","--enter","--enter-hex",
        "--autoconnect","--include","--exclude","--block","--board","--board-name","--prefer-boards"};
    size_t i; for(i=0;i<sizeof(keys)/sizeof(keys[0]);++i) if(!strcmp(s,keys[i])) return 1;
    return 0;
}
static FILE *profile_open(const char *path) {
#ifdef _WIN32
    WCHAR wide[4096];
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,4096)) { errno=EINVAL; return NULL; }
    return _wfopen(wide,L"rb");
#else
    return fopen(path,"rb");
#endif
}
static int load(const char *path,int optional) {
    FILE *f=profile_open(path); char line[8194]; unsigned number=0; long previous=0;
    if(!f) { if(optional && errno==ENOENT) return 0; fprintf(stderr,"Cannot read profile: %s\n",path); return -1; }
    while(fgets(line,sizeof(line),f)) {
        char *s,*v=NULL,*p; size_t n; ++number;
        long current=ftell(f);
        if(current>1048576 || (current>=previous && previous>=0 && (unsigned long)(current-previous)>strlen(line))) goto bad;
        previous=current;
        if(!strchr(line,'\n') && !feof(f)) goto bad;
        s=trim(line); if(number==1 && !strncmp(s,"\xEF\xBB\xBF",3)) s=trim(s+3);
        if(!*s || *s=='#') continue;
        for(p=s;*p && *p!='=' && !isspace((unsigned char)*p);++p) {}
        if(*p) { *p++=0; v=trim(p); n=strlen(v);
            if(n>=2 && ((*v=='"' && v[n-1]=='"') || (*v=='\'' && v[n-1]=='\''))) { v[n-1]=0; ++v; }
        }
        if(!allowed(s) || add(s) || (v && add(v))) goto bad;
    }
    if(ferror(f)) goto bad;
    fclose(f); return 0;
bad:
    fprintf(stderr,"Invalid or oversized profile %s:%u (one long option per line; no actions/nesting)\n",path,number);
    fclose(f); return -1;
}
static int step(int argc,char **argv,int i) {
    static const char *flags[]={"--profiles","--help","-h","--version","--list","--list-all","--list-settings","--list-filters",
        "--list-boards","--terminal","--receive","--rts-cts","--discard-input","--show-config","--quiet","-q","--log-time"};
    size_t n; int count=2;
    for(n=0;n<sizeof(flags)/sizeof(flags[0]);++n) if(!strcmp(argv[i],flags[n])) return 1;
    if(!strcmp(argv[i],"--listen") || !strcmp(argv[i],"--connect")) count=4;
    return count<=argc-i?count:argc-i;
}
int sc_arguments(int *argc,char ***argv) {
    int i,profiles=0; const char *root; char path[4096];
    atexit(cleanup); if(add((*argv)[0])) return -1;
    for(i=1;i<*argc;i+=step(*argc,*argv,i)) if(!strcmp((*argv)[i],"--profiles")) profiles=1;
    if(profiles) {
#ifdef _WIN32
        { static char utf8[16384]; WCHAR wide[4096]; DWORD len=GetEnvironmentVariableW(L"APPDATA",wide,4096);
          root=NULL;
          if(len && len<4096 && WideCharToMultiByte(CP_UTF8,0,wide,-1,utf8,sizeof(utf8),NULL,NULL)) root=utf8;
          else if(len) return -1;
        }
        if(root) { if(snprintf(path,sizeof(path),"%s/SerialUART/settings.cfg",root)>=(int)sizeof(path) || load(path,1)) return -1; }
#else
        root=getenv("XDG_CONFIG_HOME");
        if(root && *root) {
            if(snprintf(path,sizeof(path),"%s/serialuart/settings.cfg",root)>=(int)sizeof(path) || load(path,1)) return -1;
        } else {
            root=getenv("HOME");
            if(root && (snprintf(path,sizeof(path),"%s/.config/serialuart/settings.cfg",root)>=(int)sizeof(path) || load(path,1))) return -1;
        }
#endif
        if(load(".serialuart/settings.cfg",1)) return -1;
    }
    /* Explicit profiles follow defaults; all actual CLI settings win last. */
    for(i=1;i<*argc;i+=step(*argc,*argv,i)) if(!strcmp((*argv)[i],"--config")) {
        if(i+1==*argc || load((*argv)[i+1],0)) return -1;
    }
    for(i=1;i<*argc;) {
        int count=step(*argc,*argv,i),j;
        if(strcmp((*argv)[i],"--profiles") && strcmp((*argv)[i],"--config"))
            for(j=0;j<count;++j) if(add((*argv)[i+j])) return -1;
        i+=count;
    }
    *argc=used; *argv=expanded; return 0;
}
void sc_settings(int argc,char **argv) {
    int i; puts("Expanded settings in application order (later scalar settings win):");
    for(i=1;i<argc;++i) printf("%d\t%s\n",i,argv[i]);
}
