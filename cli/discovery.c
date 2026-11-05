#include "session.h"
#include <string.h>
typedef struct { sc_session *session; suart_selector_ex selector; int json,first; } listing;
static void json_text(const char *s) {
    const unsigned char *p=(const unsigned char *)s; putchar('"');
    for(;*p;++p) {
        if(*p=='"' || *p=='\\') { putchar('\\'); putchar(*p); }
        else if(*p<32 || *p==127) printf("\\u%04x",*p);
        else putchar(*p);
    }
    putchar('"');
}
static void field(const char *name,const char *value,int first) {
    if(!first) putchar(',');
    json_text(name); putchar(':'); json_text(value);
}
static int show(const suart_device_info *d,void *context) {
    listing *l=(listing *)context; char board[512];
    if(!suart_device_matches(&l->selector,d) || sc_device_state(l->session,&d->device)) return 0;
    sc_board(l->session,&d->device,board,sizeof(board));
    if(l->session->board_name && strcmp(l->session->board_name,board)) return 0;
    if(l->json) {
        if(l->json==1 && !l->first) putchar(',');
        l->first=0; putchar('{');
        field("path",d->device.path,1); field("name",d->device.name,0);
        field("manufacturer",d->manufacturer,0); field("product",d->product,0);
        field("serial",d->device.serial,0); field("instance_id",d->instance_id,0);
        field("location",d->location,0); field("board",board,0);
        printf(",\"vid\":%u,\"pid\":%u,\"is_usb\":%s,\"problem_code\":",d->device.vid,d->device.pid,d->is_usb?"true":"false");
        if(d->problem_known) printf("%u",d->problem_code); else fputs("null",stdout);
        putchar('}');if(l->json==2) putchar('\n');
    } else {
        /* Printable escaped strings keep device metadata from injecting controls. */
        field("port",d->device.path,1); field("name",d->device.name,0); putchar('\n');
        field("manufacturer",d->manufacturer,1); field("product",d->product,0);
        printf("  USB %04X:%04X\n",d->device.vid,d->device.pid);
        field("serial",d->device.serial,1); field("instance",d->instance_id,0); putchar('\n');
        field("location",d->location,1); field("board",board,0); putchar('\n');
        if(d->problem_known) printf("Device-manager problem: %u\n",d->problem_code);
        putchar('\n');
    }
    return 0;
}
int sc_list_extended(sc_session *session,const suart_selector *selector,int json) {
    listing l; int result; memset(&l,0,sizeof(l)); l.session=session; l.selector.device=*selector;
    l.selector.instance_id=session->instance_id; l.selector.match=session->match; l.json=json; l.first=1;
    if(json==1) putchar('['); else if(!json) printf("%s\n\n",suart_discovery_source());
    result=suart_enumerate_ex(show,&l);
    if(json==1) puts("]");
    if(result) fprintf(stderr,"Device enumeration failed: %d\n",result);
    return result;
}
