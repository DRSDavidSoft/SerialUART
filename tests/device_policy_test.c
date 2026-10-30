#include "session.h"
#include <string.h>
#include <stdlib.h>
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); return 1; } } while(0)
static suart_device devices[4]; static unsigned count; static const char *busy;
int suart_enumerate(suart_device_fn callback,void *ctx) {
    unsigned i; for(i=0;i<count;++i) if(callback(&devices[i],ctx)) break; return 0;
}
int suart_resolve(const suart_selector *s,suart_device *d) {
    memset(d,0,sizeof(*d)); if(!s->path) return SUART_NOT_FOUND;
    snprintf(d->path,sizeof(d->path),"%s",s->path); return 0;
}
int suart_open_ex(suart_port **p,const char *path,const suart_config *c,unsigned long *error) {
    (void)c; *p=NULL; *error=0;
    if(busy && !strcmp(path,busy)) { *error=123; return SUART_ERROR; }
    *p=(suart_port *)(void *)&devices[0]; return 0;
}
static void setup(void) {
    memset(devices,0,sizeof(devices)); count=2;
    strcpy(devices[0].path,"port-A"); strcpy(devices[1].path,"port-B");
    devices[0].vid=devices[1].vid=0x2e8a; devices[0].pid=devices[1].pid=0x0005;
    strcpy(devices[0].serial,"SERIAL-A"); strcpy(devices[1].serial,"SERIAL-B");
    strcpy(devices[0].name,"Fixture device"); strcpy(devices[1].name,"Fixture device");
}
int main(void) {
    static sc_session s; suart_selector sel={0}; suart_config config; suart_device d; suart_port *p; unsigned long error;
    memset(&config,0,sizeof(config)); setup(); sc_init(&s);
    CHECK(sc_option(&s,"--autoconnect","one")==1); busy="port-A";
    CHECK(!sc_open(&s,&sel,&config,&d,&p,&error) && !strcmp(d.path,"port-B") && s.pinned);
    strcpy(devices[1].path,"port-C");
    CHECK(!sc_open(&s,&sel,&config,&d,&p,&error) && !strcmp(d.path,"port-C"));
    strcpy(devices[0].serial,"SERIAL-B");
    CHECK(sc_open(&s,&sel,&config,&d,&p,&error)==SUART_AMBIGUOUS && !p);
    setup(); sc_init(&s); busy=NULL;
    CHECK(sc_option(&s,"--autoconnect","any")==1);
    CHECK(!sc_open(&s,&sel,&config,&d,&p,&error) && !strcmp(d.path,"port-A"));
    strcpy(devices[0].path,"port-Z");
    CHECK(!sc_open(&s,&sel,&config,&d,&p,&error) && !strcmp(d.path,"port-B"));
    /* A filtered real port must never be retried as a metadata-less path. */
    CHECK(sc_option(&s,"--exclude","name~fixture")==1); sel.path="port-B";
    CHECK(sc_open(&s,&sel,&config,&d,&p,&error)==SUART_NOT_FOUND && !p);
    setup(); sc_init(&s);
    CHECK(sc_option(&s,"--block","name~fixture")==1);
    CHECK(sc_open(&s,&sel,&config,&d,&p,&error)==SUART_INVALID && !p);
    puts("Automatic selection, busy-port fallback, pinned identity, ambiguity and explicit blocking passed"); return 0;
}
