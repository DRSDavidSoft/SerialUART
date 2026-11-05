#include "rpc.h"
#include "json.h"
#include "network.h"
#include "protocol.h"
#include <stdlib.h>
#include <string.h>
#define REQUEST_LIMIT 16384
#define RESPONSE_LIMIT 65536
typedef struct {
    sc_session *features;suart_selector selection;suart_config config;
    suart_port *port;suart_device device;suart_net *network;
    uint64_t rx,tx;int subscribed,quit;unsigned long native;int serial_status;
    j_doc doc;char result[RESPONSE_LIMIT],response[RESPONSE_LIMIT];
} rpc;
static int text(const j_doc *d,int object,const char *key,char *out,size_t cap) {
    int t=j_field(d,object,key),n;if(t<0) return 0;
    n=j_text(d,t,out,cap);return n<0 || strlen(out)!=(size_t)n?-1:1;
}
static int number(const j_doc *d,int p,const char *key,unsigned max,unsigned *out) { return j_uint(d,j_field(d,p,key),max,out); }
static int boolean(const j_doc *d,int p,const char *key,int *out) {
    int t=j_field(d,p,key);if(t<0) return 0;
    if(d->tokens[t].type!=J_TRUE && d->tokens[t].type!=J_FALSE) return -1;
    *out=d->tokens[t].type==J_TRUE;return 0;
}
static int keys(const j_doc *d,int p,const char *allowed) {
    unsigned i;char key[129],needle[132];
    if(p<0) return 1;
    if(d->tokens[p].type!=J_OBJECT) return 0;
    for(i=(unsigned)p+1;i<d->tokens[p].next;i=d->tokens[d->tokens[i].next].next) {
        int n=j_text(d,(int)i,key,sizeof(key));if(n<0 || strlen(key)!=(size_t)n) return 0;
        snprintf(needle,sizeof(needle),"|%s|",key);if(!strstr(allowed,needle)) return 0;
    }
    return 1;
}
static void close_port(rpc *r) { suart_close(r->port);r->port=NULL;r->subscribed=0;r->rx=r->tx=0;memset(&r->device,0,sizeof(r->device)); }
static int send_response(rpc *r,const char *s,size_t n) {
    if(r->network) return suart_net_write(r->network,s,n);
    return fwrite(s,1,n,stdout)!=n || fflush(stdout)?-1:0;
}
static void config_json(j_out *o,const suart_config *c) {
    j_print(o,"{\"baud\":%u,\"data_bits\":%u,\"parity\":\"%c\",\"stop_bits\":%s,\"rts_cts\":%s}",
        c->baud,c->data_bits,c->parity,c->stop_bits==15?"1.5":c->stop_bits==2?"2":"1",c->rts_cts?"true":"false");
}
static void device_json(j_out *o,const suart_device_info *d) {
    j_raw(o,"{\"path\":");j_quote(o,d->device.path);j_raw(o,",\"name\":");j_quote(o,d->device.name);
    j_raw(o,",\"serial\":");j_quote(o,d->device.serial);j_raw(o,",\"manufacturer\":");j_quote(o,d->manufacturer);
    j_raw(o,",\"product\":");j_quote(o,d->product);j_raw(o,",\"instance_id\":");j_quote(o,d->instance_id);
    j_raw(o,",\"location\":");j_quote(o,d->location);
    j_print(o,",\"vid\":%u,\"pid\":%u,\"is_usb\":%s,\"problem_code\":",d->device.vid,d->device.pid,d->is_usb?"true":"false");
    if(d->problem_known) j_print(o,"%u",d->problem_code);else j_raw(o,"null");j_raw(o,"}");
}
typedef struct { rpc *r;j_out *o;suart_selector_ex filter;unsigned offset,limit,total,shown; } listing;
static int device(const suart_device_info *d,void *ctx) {
    listing *l=(listing *)ctx;char board[512];
    if(!suart_device_matches(&l->filter,d) || sc_device_state(l->r->features,&d->device)) return 0;
    if(l->r->features->board_name) { sc_board(l->r->features,&d->device,board,sizeof(board));if(strcmp(board,l->r->features->board_name)) return 0; }
    if(l->total++<l->offset || l->shown==l->limit) return 0;
    if(l->shown++) j_raw(l->o,",");
    device_json(l->o,d);return l->o->failed;
}
static int serial_error(rpc *r,int status) { r->serial_status=status;r->native=suart_native_error(r->port);return -32000; }
static int execute(rpc *r,const char *method,int p,j_out *out) {
    j_doc *d=&r->doc;unsigned max=4096,timeout=0;int rc;
    r->native=0;r->serial_status=0;
    if(!strcmp(method,"system.info")) {
        if(!keys(d,p,"")) return -32602;
        j_raw(out,"{\"application\":\"SerialUART\",\"version\":\"" SUART_VERSION "\",\"api_version\":1,\"request_bytes\":16384,\"transfer_bytes\":4096,\"transports\":[\"stdio\",\"tcp\",\"websocket\"],\"discovery_source\":");
        j_quote(out,suart_discovery_source());j_raw(out,"}");return 0;
    }
    if(!strcmp(method,"devices.list")) {
        listing l;char match[512];memset(&l,0,sizeof(l));l.r=r;l.o=out;l.limit=16;l.filter.device=r->selection;
        l.filter.instance_id=r->features->instance_id;l.filter.match=r->features->match;
        if(!keys(d,p,"|offset||limit||match|") || number(d,p,"offset",100000,&l.offset) || number(d,p,"limit",16,&l.limit) || !l.limit) return -32602;
        rc=text(d,p,"match",match,sizeof(match));if(rc<0) return -32602;if(rc) l.filter.match=match;
        j_raw(out,"{\"devices\":[");rc=suart_enumerate_ex(device,&l);if(rc) return serial_error(r,rc);
        j_print(out,"],\"total\":%u,\"offset\":%u,\"next_offset\":",l.total,l.offset);
        if(l.offset+l.shown<l.total) j_print(out,"%u",l.offset+l.shown);else j_raw(out,"null");j_raw(out,"}");return 0;
    }
    if(!strcmp(method,"port.open")) {
        suart_selector s=r->selection;suart_config c=r->config;
        char path[512],name[512],serial[256],instance[1024],match[512],parity[8],dtr[16];
        const char *old_instance=r->features->instance_id,*old_match=r->features->match;int old_auto=r->features->auto_connect,old_pin=r->features->pinned;
        unsigned stop=c.stop_bits;int has_instance,has_match,stop_token;
        if(r->port) return -32001;
        if(!keys(d,p,"|path||name||serial||instance_id||match||vid||pid||baud||data_bits||parity||stop_bits||rts_cts||dtr||boot_wait_ms|")) return -32602;
#define STRING(KEY,BUFFER,DEST) do { rc=text(d,p,KEY,BUFFER,sizeof(BUFFER));if(rc<0) return -32602;if(rc) DEST=BUFFER; } while(0)
        STRING("path",path,s.path);STRING("name",name,s.name);STRING("serial",serial,s.serial);
        has_instance=text(d,p,"instance_id",instance,sizeof(instance));has_match=text(d,p,"match",match,sizeof(match));
        if(has_instance<0 || has_match<0 || number(d,p,"vid",65535,&s.vid) || number(d,p,"pid",65535,&s.pid) ||
           number(d,p,"baud",4000000,&c.baud) || !c.baud || number(d,p,"data_bits",8,&c.data_bits) || c.data_bits<5 ||
           number(d,p,"boot_wait_ms",10000,&c.boot_wait_ms) || boolean(d,p,"rts_cts",&c.rts_cts)) return -32602;
        stop_token=j_field(d,p,"stop_bits");
        if(stop_token>=0 && d->tokens[stop_token].type==J_NUMBER && d->tokens[stop_token].end-d->tokens[stop_token].start==3 &&
           !memcmp(d->text+d->tokens[stop_token].start,"1.5",3)) stop=15;
        else if(number(d,p,"stop_bits",2,&stop) || stop<1) return -32602;
        c.stop_bits=stop;rc=text(d,p,"parity",parity,sizeof(parity));if(rc<0 || (rc && (strlen(parity)!=1 || !strchr("NEOMS",*parity)))) return -32602;if(rc) c.parity=*parity;
        rc=text(d,p,"dtr",dtr,sizeof(dtr));if(rc<0) return -32602;
        if(rc) { if(!strcmp(dtr,"keep")) c.dtr=SUART_DTR_KEEP;else if(!strcmp(dtr,"off")) c.dtr=SUART_DTR_OFF;else if(!strcmp(dtr,"on")) c.dtr=SUART_DTR_ON;else if(!strcmp(dtr,"reset")) c.dtr=SUART_DTR_RESET;else return -32602; }
        if(has_instance) r->features->instance_id=instance;
        if(has_match) r->features->match=match;
        r->features->auto_connect=r->features->pinned=0;
        rc=sc_open(r->features,&s,&c,&r->device,&r->port,&r->native);
        r->features->instance_id=old_instance;r->features->match=old_match;r->features->auto_connect=old_auto;r->features->pinned=old_pin;
        if(rc) { close_port(r);r->serial_status=rc;return -32000; }
        r->rx=r->tx=0;j_raw(out,"{\"path\":");j_quote(out,r->device.path);j_raw(out,",\"configuration\":");config_json(out,&c);j_raw(out,"}");return 0;
    }
    if(!strcmp(method,"port.close") || !strcmp(method,"server.shutdown")) {
        if(!keys(d,p,"")) return -32602;
        close_port(r);if(!strcmp(method,"server.shutdown")) r->quit=1;j_raw(out,"true");return 0;
    }
    if(!strcmp(method,"port.status")) {
        suart_config c;if(!keys(d,p,"")) return -32602;
        j_print(out,"{\"connected\":%s,\"subscribed\":%s,\"rx_bytes\":%llu,\"tx_bytes\":%llu,\"path\":",r->port?"true":"false",r->subscribed?"true":"false",(unsigned long long)r->rx,(unsigned long long)r->tx);
        j_quote(out,r->device.path);j_raw(out,",\"configuration\":");
        if(r->port && !suart_get_config(r->port,&c)) config_json(out,&c);else j_raw(out,"null");j_raw(out,"}");return 0;
    }
    if(strcmp(method,"port.read") && strcmp(method,"port.write") && strcmp(method,"port.subscribe") && strcmp(method,"port.signals") &&
       strcmp(method,"port.set_signal") && strcmp(method,"port.break") && strcmp(method,"port.discard")) return -32601;
    if(!r->port) return -32002;
    if(!strcmp(method,"port.subscribe")) {
        int enabled=1;if(!keys(d,p,"|enabled|") || boolean(d,p,"enabled",&enabled)) return -32602;
        r->subscribed=enabled;j_raw(out,enabled?"true":"false");return 0;
    }
    if(!strcmp(method,"port.read") || !strcmp(method,"port.write")) {
        unsigned char bytes[4096];char encoded[5465];size_t n=0;
        if(!strcmp(method,"port.read")) {
            if(!keys(d,p,"|max_bytes||timeout_ms|") || number(d,p,"max_bytes",4096,&max) || !max || number(d,p,"timeout_ms",1000,&timeout)) return -32602;
            if(r->subscribed) return -32003;
            rc=suart_read(r->port,bytes,max,&n,timeout);if(rc) { int code=serial_error(r,rc);close_port(r);return code; }
            r->rx+=n;if(sc_output(r->features,bytes,n,0)) return -32004;
            suart_base64(bytes,n,encoded,sizeof(encoded));j_raw(out,"{\"data\":");j_quote(out,encoded);j_print(out,",\"encoding\":\"base64\",\"bytes\":%u}",(unsigned)n);return 0;
        }
        timeout=1000;
        if(!keys(d,p,"|data||timeout_ms|") || text(d,p,"data",encoded,sizeof(encoded))!=1 || number(d,p,"timeout_ms",1000,&timeout) || !timeout ||
           suart_unbase64(encoded,strlen(encoded),bytes,sizeof(bytes),&n)) return -32602;
        { size_t sent=0;uint64_t end=suart_millis()+timeout;
          while(sent<n) { size_t got=0;uint64_t now=suart_millis();if(now>=end) break;
              rc=suart_write(r->port,bytes+sent,n-sent,&got,(unsigned)(end-now));sent+=got;if(rc) break; }
          r->tx+=sent;if(sent<n) { serial_error(r,SUART_ERROR);close_port(r);j_print(out,"{\"written\":%u,\"delivery_uncertain\":true}",(unsigned)sent);return -32005; }
          j_print(out,"{\"written\":%u}",(unsigned)sent);return 0; }
    }
    if(!strcmp(method,"port.signals")) {
        suart_modem m;if(!keys(d,p,"")) return -32602;rc=suart_get_modem(r->port,&m);if(rc) return serial_error(r,rc);
        j_print(out,"{\"known\":%u,\"asserted\":%u}",m.known,m.asserted);return 0;
    }
    if(!strcmp(method,"port.set_signal")) {
        char line[8];int asserted=0;
        if(!keys(d,p,"|line||asserted|") || text(d,p,"line",line,sizeof(line))!=1 || j_field(d,p,"asserted")<0 || boolean(d,p,"asserted",&asserted)) return -32602;
        if(!strcmp(line,"dtr")) rc=suart_set_dtr(r->port,asserted);else if(!strcmp(line,"rts")) rc=suart_set_rts(r->port,asserted);else return -32602;
    } else if(!strcmp(method,"port.break")) {
        unsigned duration=250;if(!keys(d,p,"|duration_ms|") || number(d,p,"duration_ms",1000,&duration) || !duration) return -32602;rc=suart_send_break(r->port,duration);
    } else {
        int receive=0,transmit=0;if(!keys(d,p,"|receive||transmit|") || boolean(d,p,"receive",&receive) || boolean(d,p,"transmit",&transmit) || (!receive&&!transmit)) return -32602;
        rc=suart_discard(r->port,(receive?SUART_RX:0)|(transmit?SUART_TX:0));
    }
    if(rc) return serial_error(r,rc);
    j_raw(out,"true");return 0;
}
static const char *error_message(int code) {
    switch(code) {
    case -32700:return "Parse error or input limit exceeded";case -32600:return "Invalid Request";
    case -32601:return "Method not found";case -32602:return "Invalid params";case -32603:return "Response capacity exceeded";
    case -32001:return "Close the current port before opening another";case -32002:return "No open port";
    case -32003:return "Unsubscribe before reading explicitly";case -32004:return "Capture log failed";
    case -32005:return "Incomplete write; delivery uncertain; connection closed";default:return "Native serial operation failed";
    }
}
static void error(j_out *o,const char *id,int code,rpc *r) {
    j_raw(o,"{\"jsonrpc\":\"2.0\",\"id\":");j_raw(o,id);j_print(o,",\"error\":{\"code\":%d,\"message\":",code);j_quote(o,error_message(code));
    if(code==-32000) j_print(o,",\"data\":{\"status\":%d,\"native\":%lu}",r->serial_status,r->native);
    if(code==-32005) { j_raw(o,",\"data\":");j_raw(o,r->result); }
    j_raw(o,"}}");
}
static int request(rpc *r,int node,j_out *out,int comma) {
    j_doc *d=&r->doc;int id,params,code;char version[8],method[128],identifier[258]="null";j_out result={r->result,0,sizeof(r->result),0};
    id=j_field(d,node,"id");params=j_field(d,node,"params");
    if(id>=0) {
        j_token *t=&d->tokens[id];size_t n=t->end-t->start;
        if((t->type!=J_STRING && t->type!=J_NUMBER && t->type!=J_NULL) || n>256) { id=-2; }
        else { memcpy(identifier,d->text+t->start,n);identifier[n]=0; }
    }
    if(id==-2 || d->tokens[node].type!=J_OBJECT || text(d,node,"jsonrpc",version,sizeof(version))!=1 || strcmp(version,"2.0") ||
       text(d,node,"method",method,sizeof(method))!=1) {
        if(comma) j_raw(out,",");
        error(out,"null",-32600,r);return 1;
    }
    r->result[0]=0;
    code=execute(r,method,params,&result);if(result.failed) code=-32603;
    if(id<0) return 0;
    if(comma) j_raw(out,",");
    if(code) error(out,identifier,code,r);
    else { j_raw(out,"{\"jsonrpc\":\"2.0\",\"id\":");j_raw(out,identifier);j_raw(out,",\"result\":");j_raw(out,r->result);j_raw(out,"}"); }
    return 1;
}
static int dispatch(rpc *r,const char *line,size_t n) {
    j_out out={r->response,0,sizeof(r->response),0};unsigned i,count=0;r->response[0]=0;
    if(j_parse(&r->doc,line,n)) error(&out,"null",-32700,r);
    else if(r->doc.tokens[0].type==J_ARRAY) {
        if(!r->doc.tokens[0].count || r->doc.tokens[0].count>16) error(&out,"null",-32600,r);
        else {
            j_raw(&out,"[");for(i=1;i<r->doc.tokens[0].next;i=r->doc.tokens[i].next) count+=(unsigned)request(r,(int)i,&out,count!=0);
            if(!count) return 0;
            j_raw(&out,"]");
        }
    } else if(!request(r,0,&out,0)) return 0;
    if(out.failed || out.size>65533) { out.size=0;out.failed=0;r->response[0]=0;error(&out,"null",-32603,r); }
    j_raw(&out,"\n");return send_response(r,out.data,out.size);
}
static int events(rpc *r) {
    unsigned char bytes[4096];char encoded[5465];size_t n=0;j_out out={r->response,0,sizeof(r->response),0};int rc;
    if(!r->subscribed || !r->port) return 0;
    rc=suart_read(r->port,bytes,sizeof(bytes),&n,0);
    if(rc) { close_port(r);j_raw(&out,"{\"jsonrpc\":\"2.0\",\"method\":\"port.disconnected\",\"params\":{}}\n"); }
    else if(n) {
        r->rx+=n;if(sc_output(r->features,bytes,n,0)) return -1;
        suart_base64(bytes,n,encoded,sizeof(encoded));j_raw(&out,"{\"jsonrpc\":\"2.0\",\"method\":\"port.received\",\"params\":{\"data\":");j_quote(&out,encoded);
        j_print(&out,",\"encoding\":\"base64\",\"bytes\":%u}}\n",(unsigned)n);
    }
    return out.size?send_response(r,out.data,out.size):0;
}
int sc_rpc_run(const char *transport,unsigned port,const char *origin,sc_session *features,
               const suart_selector *selection,const suart_config *config,
               int (*input)(unsigned char *,size_t,size_t *),volatile sig_atomic_t *stopping) {
    rpc *r=(rpc *)calloc(1,sizeof(*r));char line[REQUEST_LIMIT+1];size_t used=0;int over=0,result=0;
    if(!r) return 1;
    r->features=features;r->selection=*selection;r->config=*config;features->output_silent=1;features->bulk=1;
    if(strcmp(transport,"stdio")) {
        r->network=suart_net_open(!strcmp(transport,"websocket")?"rpc-ws":"tcp","127.0.0.1",port,0,origin);
        if(!r->network) { fputs("Cannot bind local API endpoint\n",stderr);free(r);return 1; }
    }
    fprintf(stderr,"SerialUART API ready: %s%s\n",transport,r->network?" on 127.0.0.1":" (NDJSON stdin/stdout)");
    while(!r->quit && !*stopping) {
        unsigned char bytes[4096];size_t n=0,i;int rc=r->network?suart_net_read(r->network,bytes,sizeof(bytes),&n):input(bytes,sizeof(bytes),&n);
        if(rc<0) { if(!r->network) { result=1;break; }close_port(r);used=0;over=0;continue; }
        if(rc==2) { if(used || over) dispatch(r,"",0);break; }
        for(i=0;i<n && !r->quit;++i) {
            if(bytes[i]=='\n') {
                if(over || used) {
                    if(dispatch(r,over?"":line,over?0:used)) {
                        /* A departed peer must not execute later requests in this chunk. */
                        close_port(r);used=0;over=0;if(!r->network) result=1;break;
                    }
                }
                used=0;over=0;
            } else if(used==REQUEST_LIMIT) over=1;else if(!over) line[used++]=(char)bytes[i];
        }
        if(result) break;
        if(events(r)) { close_port(r);if(!r->network) { result=1;break; } }
        suart_sleep(2);
    }
    close_port(r);suart_net_close(r->network);free(r);return result;
}
