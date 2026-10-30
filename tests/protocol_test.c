#include "protocol.h"
#include "SerialUART.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static unsigned count,last_op; static size_t last_size; static unsigned char last[32];
static int message(unsigned op,const unsigned char *b,size_t n,void *ctx) {
    (void)ctx; ++count; last_op=op; last_size=n; if(n<=sizeof(last)) memcpy(last,b,n); return 0;
}
int main(void) {
    char accept[29],base[32]; unsigned char bytes[32],frame[32]; size_t n,i;
    suart_ws_parser p; suart_config c; suart_selector selector={0}; suart_device d; suart_port *port=NULL;
    const unsigned char hello[]={0x81,0x85,0x37,0xfa,0x21,0x3d,0x7f,0x9f,0x4d,0x51,0x58};
    const unsigned char fragments[]={0x01,0x81,1,2,3,4,'a'^1, 0x89,0x80,0,0,0,0, 0x80,0x81,1,2,3,4,'b'^1};
    CHECK(!suart_ws_accept("dGhlIHNhbXBsZSBub25jZQ==",accept));
    CHECK(!strcmp(accept,"s3pPLMBiTxaQ9kYGzzhZRbK+xOo="));
    CHECK(suart_ws_accept("invalid",accept)<0);
    CHECK(suart_base64((const unsigned char *)"\0\xff\x01",3,base,sizeof(base))==4);
    CHECK(!strcmp(base,"AP8B")); CHECK(!suart_unbase64(base,4,bytes,sizeof(bytes),&n));
    CHECK(n==3 && bytes[0]==0 && bytes[1]==255 && bytes[2]==1);
    CHECK(suart_unbase64("A===",4,bytes,sizeof(bytes),&n)<0);
    CHECK(suart_unbase64("AB==",4,bytes,sizeof(bytes),&n)<0);
    CHECK(!suart_hex("00 ff:41",bytes,sizeof(bytes),&n)); CHECK(n==3 && bytes[1]==255);
    CHECK(suart_hex("0",bytes,sizeof(bytes),&n)<0); CHECK(suart_hex("gg",bytes,sizeof(bytes),&n)<0);
    CHECK(suart_ws_frame(2,"abc",3,frame,sizeof(frame))==5 && frame[0]==0x82 && frame[1]==3);
    memset(&p,0,sizeof(p));
    for(i=0;i<sizeof(hello);++i) CHECK(!suart_ws_feed(&p,hello+i,1,message,NULL));
    CHECK(count==1 && last_op==1 && last_size==5 && !memcmp(last,"Hello",5));
    memset(&p,0,sizeof(p)); count=0;
    CHECK(!suart_ws_feed(&p,fragments,sizeof(fragments),message,NULL));
    CHECK(count==2 && last_op==1 && last_size==2 && !memcmp(last,"ab",2));
    memset(&p,0,sizeof(p)); CHECK(suart_ws_feed(&p,"\x81\x01x",3,message,NULL)<0);
    memset(&p,0,sizeof(p)); CHECK(suart_ws_feed(&p,"\x80\x80\0\0\0\0",6,message,NULL)<0);
    memset(&p,0,sizeof(p)); CHECK(suart_ws_feed(&p,"\x81\x81\0\0\0\0\xff",7,message,NULL)<0);
    memset(&p,0,sizeof(p)); CHECK(suart_ws_feed(&p,"\x88\x81\0\0\0\0x",7,message,NULL)<0);
    suart_config_init(&c); CHECK(c.baud==57600 && c.dtr==SUART_DTR_KEEP);
    c.data_bits=9; CHECK(suart_open(&port,"nonexistent",&c)==SUART_INVALID && !port);
    CHECK(suart_read(NULL,bytes,sizeof(bytes),&n,0)==SUART_INVALID);
    CHECK(suart_wait_tx(NULL,0)==SUART_INVALID);
    CHECK(suart_discard(NULL,SUART_RX)==SUART_INVALID);
    CHECK(suart_read_until(NULL,bytes,sizeof(bytes),&n,0,0)==SUART_INVALID && !n);
    CHECK(suart_resolve(NULL,&d)==SUART_INVALID);
    selector.path="/a/virtual/path"; CHECK(!suart_resolve(&selector,&d)); CHECK(!strcmp(d.path,selector.path));
    puts("Protocol, binary codecs and API validation passed"); return 0;
}
