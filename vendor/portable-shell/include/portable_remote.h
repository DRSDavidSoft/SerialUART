#ifndef PORTABLE_REMOTE_H
#define PORTABLE_REMOTE_H
/* HTTP route facade for OS or firmware providers. No sockets, allocation, libc,
   HTTP parser or authentication policy. The transport passes a complete decoded
   request only after authorization and body-size checks. Responses are byte sinks.
   A provider returns an HTTP status; it must not emit a body then change status.
   Methods: GET /list, GET|PUT /files/PATH, POST /command. */
typedef void (*ps_remote_write_fn)(void *,const unsigned char *,unsigned);
typedef int (*ps_remote_action_fn)(void *,const char *,const unsigned char *,unsigned,
                                  ps_remote_write_fn,void *);
typedef struct {
    void *context;
    ps_remote_action_fn list,read,write,execute;
} ps_remote_provider;
static int ps_remote_equal(const char *a,const char *b){while(*a&&*a==*b){++a;++b;}return *a==*b;}
static int ps_remote_path(const char *p){
    unsigned segment=0,dots=0;
    if(!*p)return 0;
    for(;;++p){unsigned char c=(unsigned char)*p;
        if(!c||c=='/'){if(!segment||(dots==segment&&segment<=2))return 0;if(!c)return 1;segment=dots=0;}
        else {if(c<32||c==127||c=='\\'||c==':'||c=='%'||c=='?'||c=='#')return 0;++segment;if(c=='.')++dots;}
    }
}
static int ps_remote_dispatch(const ps_remote_provider *p,const char *method,const char *path,
                              const unsigned char *body,unsigned size,ps_remote_write_fn out,void *ctx){
    ps_remote_action_fn action=0;const char *name="";unsigned i;
    if(!p||!method||!path||!out||(!body&&size))return 400;
    if(ps_remote_equal(path,"/list")){if(!ps_remote_equal(method,"GET"))return 405;action=p->list;}
    else if(ps_remote_equal(path,"/command")){if(!ps_remote_equal(method,"POST"))return 405;action=p->execute;}
    else {
        const char prefix[]="/files/";
        for(i=0;prefix[i]&&path[i]==prefix[i];++i){}
        if(prefix[i])return 404;
        name=path+i;if(!ps_remote_path(name))return 400;
        if(ps_remote_equal(method,"GET"))action=p->read;
        else if(ps_remote_equal(method,"PUT"))action=p->write;
        else return 405;
    }
    if(!action)return 501;
    return action(p->context,name,body,size,out,ctx);
}
#endif
