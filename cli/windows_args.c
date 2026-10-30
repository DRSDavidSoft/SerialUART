#include "session.h"
#include <stdlib.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
static char **arguments; static int count; static UINT saved_cp;
static void cleanup_arguments(void) {
    int i; for(i=0;i<count;++i) free(arguments[i]); free(arguments);
    fflush(stdout); fflush(stderr); if(saved_cp) SetConsoleOutputCP(saved_cp);
}
#endif
int sc_windows_arguments(int *argc,char ***argv) {
#ifdef _WIN32
    LPWSTR *wide; int i; DWORD mode;
    wide=CommandLineToArgvW(GetCommandLineW(),argc); if(!wide) return -1;
    arguments=(char **)calloc((size_t)*argc+1,sizeof(char *));
    if(!arguments) { LocalFree(wide); return -1; }
    count=*argc; atexit(cleanup_arguments);
    for(i=0;i<*argc;++i) {
        int size=WideCharToMultiByte(CP_UTF8,0,wide[i],-1,NULL,0,NULL,NULL);
        if(!size || !(arguments[i]=(char *)malloc((size_t)size))) { LocalFree(wide); return -1; }
        WideCharToMultiByte(CP_UTF8,0,wide[i],-1,arguments[i],size,NULL,NULL);
    }
    LocalFree(wide); *argv=arguments;
    if(GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE),&mode) || GetConsoleMode(GetStdHandle(STD_ERROR_HANDLE),&mode)) {
        saved_cp=GetConsoleOutputCP(); SetConsoleOutputCP(CP_UTF8);
    }
#else
    (void)argc; (void)argv;
#endif
    return 0;
}
