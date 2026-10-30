#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "SerialUART.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#ifdef __linux__
#include "linux_native.h"
#endif
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(int argc,char **argv) {
    suart_port *p=NULL,*other=NULL; suart_config c,actual; suart_modem modem;
    unsigned long error=0; int master,slave,available=0; unsigned char b[8]; size_t n=0;
    struct termios before,t; uint64_t end;
    CHECK(argc==4); master=atoi(argv[2]); slave=atoi(argv[3]);
    CHECK(!tcgetattr(slave,&before));
    suart_config_init(&c);
    CHECK(suart_open_ex(&p,"/nonexistent-serialuart-device",&c,&error)==SUART_ERROR && !p && error);
    c.data_bits=9; error=123;
    CHECK(suart_open_ex(&p,argv[1],&c,&error)==SUART_INVALID && !p && !error);
    suart_config_init(&c);
    CHECK(!suart_open_ex(&p,argv[1],&c,&error) && !error);
    CHECK(!suart_get_config(p,&actual));
    CHECK(actual.baud==57600 && actual.data_bits==8 && actual.parity=='N' && actual.stop_bits==1);
    CHECK(!tcgetattr(slave,&t) && !t.c_iflag && !t.c_oflag && !t.c_lflag);
#ifdef CMSPAR
    CHECK(!(t.c_cflag&CMSPAR));
#endif
#ifdef CDTR_IFLOW
    CHECK(!(t.c_cflag&(CDTR_IFLOW|CDSR_OFLOW|CCAR_OFLOW)));
#endif
    CHECK(suart_open_ex(&other,argv[1],&c,&error)==SUART_ERROR && !other && error);
    CHECK(!suart_write(p,"A\0B",3,&n,100) && n==3);
    CHECK(read(master,b,sizeof(b))==3 && !memcmp(b,"A\0B",3));
    CHECK(!suart_wait_tx(p,100));
    CHECK(write(master,"discard",7)==7);
    end=suart_millis()+1000;
    do { CHECK(!ioctl(slave,FIONREAD,&available)); if(available>=7) break; suart_sleep(1); } while(suart_millis()<end);
    CHECK(available==7 && !suart_discard(p,SUART_RX));
    CHECK(!suart_read(p,b,sizeof(b),&n,10) && !n);
    CHECK(suart_discard(p,0)==SUART_INVALID);
    CHECK(suart_wait_tx(p,60001)==SUART_INVALID);
    memset(&modem,255,sizeof(modem));
    /* PTYs have no physical modem lines; unknown must not become "off". */
    if(suart_get_modem(p,&modem)!=SUART_OK) CHECK(!modem.known && !modem.asserted);
    suart_close(p); p=NULL;
    CHECK(!suart_open(&p,argv[1],&c)); suart_close(p); p=NULL;
#ifdef __linux__
    {
        unsigned rate;
        CHECK(!suart_linux_set_rate(slave,23977));
        c.baud=31250;
        CHECK(!suart_open(&p,argv[1],&c));
        CHECK(!suart_get_config(p,&actual) && actual.baud==31250);
        suart_close(p); p=NULL;
        CHECK(!suart_linux_get_rate(slave,&rate) && rate==23977);
    }
#endif
    puts("Native PTY configuration, ownership, queues, errors and restoration passed"); return 0;
}
