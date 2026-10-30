#include "network.h"
#include "SerialUART.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv) {
    suart_net *n; unsigned char b[4096]; uint64_t end;
    if(argc<3) return 2;
    n=suart_net_open(argv[1],"127.0.0.1",(unsigned)atoi(argv[2]),0,argc>3?argv[3]:NULL);
    if(!n) return 1;
    puts("READY"); fflush(stdout); end=suart_millis()+60000;
    while(suart_millis()<end) {
        size_t got=0;
        if(suart_net_read(n,b,sizeof(b),&got)>0 && got) suart_net_write(n,b,got);
        suart_sleep(1);
    }
    suart_net_close(n); return 0;
}
