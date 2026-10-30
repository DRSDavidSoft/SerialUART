#include "session.h"
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void) {
    static sc_session s; suart_device d; unsigned char b[8192]; size_t n; unsigned lead=0;
    sc_init(&s);
    CHECK(sc_option(&s,"--echo",NULL)==2);
    CHECK(sc_option(&s,"--display","invalid")==-1);
    CHECK(sc_option(&s,"--enter","text:MiXeD")==1);
    CHECK(!sc_input(&s,'\r',b,sizeof(b),&n) && n==5 && !memcmp(b,"MiXeD",5));
    CHECK(sc_option(&s,"--enter-hex","00 ff 0d 0a")==1);
    CHECK(!sc_input(&s,'\n',b,sizeof(b),&n) && n==4 && !memcmp(b,"\0\xff\r\n",4));
    CHECK(sc_option(&s,"--line-buffer","on")==1);
    CHECK(!sc_input(&s,'a',b,sizeof(b),&n) && !n);
    CHECK(!sc_input(&s,0xc3,b,sizeof(b),&n) && !n);
    CHECK(!sc_input(&s,0xa9,b,sizeof(b),&n) && !n);
    CHECK(!sc_input(&s,127,b,sizeof(b),&n) && !n && s.line_size==1);
    CHECK(!sc_input(&s,'B',b,sizeof(b),&n) && !n);
    CHECK(!sc_input(&s,'\r',b,sizeof(b),&n) && n==6 && !memcmp(b,"aB\0\xff\r\n",6));
    memset(s.line,'x',sizeof(s.line)); s.line_size=sizeof(s.line);
    CHECK(sc_input(&s,'x',b,sizeof(b),&n)==-1 && !n);
    sc_init(&s);
    CHECK(sc_option(&s,"--include","vid=2341,pid=0043")==1);
    CHECK(sc_option(&s,"--exclude","name~blueTOOTH")==1);
    CHECK(sc_option(&s,"--block","serial=blocked")==1);
    memset(&d,0,sizeof(d)); d.vid=0x2341; d.pid=0x0043;
    CHECK(sc_device_state(&s,&d)==0);
    d.pid=0x0042; CHECK(sc_device_state(&s,&d)==1);
    d.pid=0x0043; strcpy(d.name,"Bluetooth Adapter"); CHECK(sc_device_state(&s,&d)==1);
    strcpy(d.serial,"blocked"); CHECK(sc_device_state(&s,&d)==2);
    CHECK(sc_option(&s,"--include","unknown=x")==-1);
    CHECK(sc_option(&s,"--include","vid=xyz")==-1);
    CHECK(sc_option(&s,"--block","port=*")==-1);
    CHECK(sc_option(&s,"--board","2341:0043=Custom CaseSensitive")==1);
    { char board[512]; sc_board(&s,&d,board,sizeof(board)); CHECK(!strcmp(board,"Custom CaseSensitive")); }
    CHECK(sc_utf16(&lead,0xd83d,b)==0 && lead==0xd83d);
    CHECK(sc_utf16(&lead,0xde00,b)==4 && !lead && !memcmp(b,"\xf0\x9f\x98\x80",4));
    CHECK(sc_utf16(&lead,0xd800,b)==0);
    CHECK(sc_utf16(&lead,'A',b)==4 && !memcmp(b,"\xef\xbf\xbd" "A",4));
    CHECK(sc_utf16(&lead,0xdc00,b)==3 && !memcmp(b,"\xef\xbf\xbd",3));
    CHECK(!strcmp(sc_key_sequence(0x7b),"\033[24~"));
    CHECK(!strcmp(sc_key_sequence(0x2d),"\033[2~"));
    CHECK(sc_key_sequence(0)==NULL);
    puts("Terminal editor, byte mappings, Unicode, model overrides and filter policies passed"); return 0;
}
