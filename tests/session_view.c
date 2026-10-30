#include "session.h"
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif
int main(int argc,char **argv) {
    static sc_session s; int i,c,rc=0; sc_init(&s);
    if(sc_windows_arguments(&argc,&argv)) return 1;
#ifdef _WIN32
    _setmode(_fileno(stdin),_O_BINARY); _setmode(_fileno(stdout),_O_BINARY);
#endif
    setvbuf(stdout,NULL,_IONBF,0);
    for(i=1;i<argc;++i) {
        int option=sc_option(&s,argv[i],NULL);
        if(option==2) { if(i+1==argc || sc_option(&s,argv[i],argv[i+1])!=1) return 2; ++i; }
        else if(option!=1) return 2;
    }
    if(sc_output_open(&s)) return 1;
    /* Byte-sized feeds deliberately split every escape and UTF-8 sequence. */
    while((c=getchar())!=EOF) {
        unsigned char b=(unsigned char)c;
        if(sc_output(&s,&b,1,0)) { rc=1; break; }
    }
    if(ferror(stdin) || sc_output_close(&s)) rc=1;
    return rc;
}
