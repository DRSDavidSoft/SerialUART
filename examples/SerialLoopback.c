/* Independent counting-pattern integrity test, inspired by linux-serial-test
 * and WiringPi's serialTest. This is a host program, not firmware. */
#include "SerialUART.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
static int number(const char *s,unsigned max,unsigned *out) {
    char *end; unsigned long n;
    if(!s || !*s || *s=='-' || *s=='+' || *s==' ') return -1;
    errno=0; n=strtoul(s,&end,10);
    if(errno || *end || !n || n>max) return -1;
    *out=(unsigned)n; return 0;
}
int main(int argc,char **argv) {
    suart_port *port=NULL; suart_config c; unsigned baud=115200,total=65536,timeout=10000;
    suart_counters first,last; int counters=0;
    unsigned char *pattern=NULL,reply[4096]; size_t tx=0,rx=0,errors=0,i;
    unsigned long native=0; uint32_t state=0x6d2b79f5u; uint64_t start,end; int rc=1;
    if(argc==2 && (!strcmp(argv[1],"--help") || !strcmp(argv[1],"-h"))) {
        puts("SerialLoopback | binary integrity and throughput test\n"
             "Usage: SerialLoopback PORT [BAUD [BYTES [TIMEOUT_MS]]]\n"
             "Requires a dedicated TX-to-RX loopback or byte-exact echo peer.\n"
             "Defaults: 115200 baud, 65536 bytes, total deadline 10000ms.\n"
             "Bytes: 1..1048576; timeout: 1..60000ms. No implicit DTR pulse.\n"
             "Tests all byte values then a deterministic varying pattern.\n"
             "Exit 0: exact count and contents; 1: I/O/mismatch; 2: usage; 124: timeout.\n"
             "Reported driver queue completion is not a target acknowledgement.");
        return 0;
    }
    if(argc<2 || argc>5 || (argc>2 && number(argv[2],4000000,&baud)) ||
       (argc>3 && number(argv[3],1048576,&total)) || (argc>4 && number(argv[4],60000,&timeout))) {
        fputs("Invalid arguments; see SerialLoopback --help\n",stderr); return 2;
    }
    pattern=(unsigned char *)malloc(total); if(!pattern) return 1;
    for(i=0;i<total;++i) {
        state^=state<<13; state^=state>>17; state^=state<<5;
        pattern[i]=i<256?(unsigned char)i:(unsigned char)state;
    }
    suart_config_init(&c); c.baud=baud;
    if(suart_open_ex(&port,argv[1],&c,&native)) { fprintf(stderr,"Cannot open port (native %lu)\n",native); goto done; }
    counters=suart_get_counters(port,&first)==SUART_OK;
    start=suart_millis(); end=start+timeout;
    while(rx<total || tx<total) {
        size_t n=0,cap; uint64_t now=suart_millis();
        if(now>=end) { rc=124; break; }
        /* Bound in-flight bytes so slow echoes exercise backpressure without
         * requiring unbounded peer buffers or a second application thread. */
        cap=total-tx; if(cap>4096) cap=4096;
        if(tx-rx<4096 && cap) {
            if(suart_write(port,pattern+tx,cap,&n,(unsigned)(end-now>10?10:end-now))) goto io_error;
            tx+=n;
        }
        cap=total-rx; if(cap>sizeof(reply)) cap=sizeof(reply);
        now=suart_millis();
        if(suart_read(port,reply,cap,&n,(unsigned)(now>=end?0:end-now>10?10:end-now))) goto io_error;
        if(n>tx-rx) { fputs("Received bytes which were never transmitted by this test\n",stderr); goto done; }
        for(i=0;i<n;++i) if(reply[i]!=pattern[rx+i]) ++errors;
        rx+=n;
    }
    if(rx==total && tx==total) {
        uint64_t now=suart_millis(); unsigned remaining=(unsigned)(now<end?end-now:0);
        rc=errors?1:suart_wait_tx(port,remaining)==SUART_OK?0:124;
        if(!rc) {
            size_t extra=0;
            if(suart_read(port,reply,sizeof(reply),&extra,0)!=SUART_OK || extra) rc=1;
            if(extra) fputs("Unexpected bytes remained after the expected reply\n",stderr);
        }
    }
    fprintf(stderr,"TX=%lu RX=%lu mismatches=%lu elapsed=%llu ms\n",(unsigned long)tx,(unsigned long)rx,
            (unsigned long)errors,(unsigned long long)(suart_millis()-start));
    if(counters && suart_get_counters(port,&last)==SUART_OK) {
        uint32_t frame=last.frame-first.frame,parity=last.parity-first.parity;
        uint32_t overrun=last.overrun-first.overrun,buffer_overrun=last.buffer_overrun-first.buffer_overrun;
        fprintf(stderr,"Driver deltas: frame=%lu parity=%lu overrun=%lu buffer_overrun=%lu breaks=%lu\n",
                (unsigned long)frame,(unsigned long)parity,(unsigned long)overrun,
                (unsigned long)buffer_overrun,(unsigned long)(uint32_t)(last.breaks-first.breaks));
        if(!rc && (frame || parity || overrun || buffer_overrun)) rc=1;
    } else fputs("Driver error counters unavailable; byte counts and content were checked by this program.\n",stderr);
    goto done;
io_error:
    fprintf(stderr,"Serial I/O failed (native %lu); TX=%lu RX=%lu\n",suart_native_error(port),(unsigned long)tx,(unsigned long)rx);
done:
    suart_close(port); free(pattern); return rc;
}
