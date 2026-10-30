#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "SerialUART.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <initguid.h>
#include <devguid.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <poll.h>
#include <time.h>
#include <glob.h>
#ifdef __APPLE__
#include <IOKit/IOKitLib.h>
#include <IOKit/serial/IOSerialKeys.h>
#include <CoreFoundation/CoreFoundation.h>
#endif
#endif
struct suart_port {
#ifdef _WIN32
    HANDLE fd;
#else
    int fd;
    struct termios saved;
#endif
    unsigned long error;
};
const char *suart_version(void) { return SUART_VERSION; }
void suart_config_init(suart_config *c) {
    if(!c) return;
    memset(c,0,sizeof(*c)); c->baud=57600; c->data_bits=8;
    c->parity='N'; c->stop_bits=1; c->dtr=SUART_DTR_KEEP;
}
uint64_t suart_millis(void) {
#ifdef _WIN32
    return (uint64_t)GetTickCount64();
#else
    struct timespec t; clock_gettime(CLOCK_MONOTONIC,&t);
    return (uint64_t)t.tv_sec*1000+(unsigned)t.tv_nsec/1000000;
#endif
}
void suart_sleep(unsigned ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    struct timespec t; t.tv_sec=ms/1000; t.tv_nsec=(long)(ms%1000)*1000000;
    while(nanosleep(&t,&t)<0 && errno==EINTR) {}
#endif
}
static int fail(suart_port *p) {
#ifdef _WIN32
    p->error=GetLastError();
#else
    p->error=(unsigned long)errno;
#endif
    return SUART_ERROR;
}
unsigned long suart_native_error(const suart_port *p) { return p?p->error:0; }
#ifndef _WIN32
static speed_t speed(unsigned b) {
    switch(b) {
#define BAUD(n) case n: return B##n
    BAUD(50); BAUD(75); BAUD(110); BAUD(300); BAUD(600); BAUD(1200);
    BAUD(2400); BAUD(4800); BAUD(9600); BAUD(19200); BAUD(38400);
    BAUD(57600); BAUD(115200);
#ifdef B230400
    BAUD(230400);
#endif
#ifdef B460800
    BAUD(460800);
#endif
#ifdef B921600
    BAUD(921600);
#endif
#ifdef B1000000
    BAUD(1000000);
#endif
#undef BAUD
    default: return (speed_t)0;
    }
}
#endif
int suart_set_dtr(suart_port *p,int on) {
    if(!p) return SUART_INVALID;
#ifdef _WIN32
    if(!EscapeCommFunction(p->fd,on?SETDTR:CLRDTR)) return fail(p);
#else
    int bit=TIOCM_DTR;
    if(ioctl(p->fd,on?TIOCMBIS:TIOCMBIC,&bit)<0) return fail(p);
#endif
    return SUART_OK;
}
int suart_set_rts(suart_port *p,int on) {
    if(!p) return SUART_INVALID;
#ifdef _WIN32
    if(!EscapeCommFunction(p->fd,on?SETRTS:CLRRTS)) return fail(p);
#else
    int bit=TIOCM_RTS;
    if(ioctl(p->fd,on?TIOCMBIS:TIOCMBIC,&bit)<0) return fail(p);
#endif
    return SUART_OK;
}
int suart_open(suart_port **out,const char *path,const suart_config *c) {
    suart_port *p;
    if(!out) return SUART_INVALID;
    *out=NULL;
    if(!path || !*path || !c || !c->baud || c->data_bits<5 || c->data_bits>8 ||
       !c->parity || !strchr("NEO",c->parity) || c->stop_bits<1 || c->stop_bits>2 ||
       c->dtr<SUART_DTR_KEEP || c->dtr>SUART_DTR_RESET || c->boot_wait_ms>60000) return SUART_INVALID;
#ifndef _WIN32
    if(!speed(c->baud)) return SUART_UNSUPPORTED;
#endif
    p=(suart_port *)calloc(1,sizeof(*p)); if(!p) return SUART_ERROR;
#ifdef _WIN32
    {
        DCB d; COMMTIMEOUTS t; char native[1024];
        if(strncmp(path,"COM",3)==0) snprintf(native,sizeof(native),"\\\\.\\%s",path);
        else snprintf(native,sizeof(native),"%s",path);
        p->fd=CreateFileA(native,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);
        if(p->fd==INVALID_HANDLE_VALUE) { free(p); return SUART_ERROR; }
        memset(&d,0,sizeof(d)); d.DCBlength=sizeof(d);
        if(!GetCommState(p->fd,&d)) goto bad;
        d.BaudRate=c->baud; d.ByteSize=(BYTE)c->data_bits;
        d.Parity=(BYTE)(c->parity=='N'?NOPARITY:c->parity=='E'?EVENPARITY:ODDPARITY);
        d.StopBits=(BYTE)(c->stop_bits==1?ONESTOPBIT:TWOSTOPBITS);
        d.fBinary=TRUE; d.fParity=c->parity!='N'; d.fOutxCtsFlow=c->rts_cts!=0;
        d.fOutxDsrFlow=FALSE; d.fDsrSensitivity=FALSE; d.fOutX=FALSE; d.fInX=FALSE;
        d.fNull=FALSE; d.fErrorChar=FALSE; d.fAbortOnError=FALSE;
        d.fRtsControl=c->rts_cts?RTS_CONTROL_HANDSHAKE:RTS_CONTROL_DISABLE;
        if(c->dtr!=SUART_DTR_KEEP) d.fDtrControl=c->dtr==SUART_DTR_ON?DTR_CONTROL_ENABLE:DTR_CONTROL_DISABLE;
        if(!SetCommState(p->fd,&d)) goto bad;
        memset(&t,0,sizeof(t)); t.ReadIntervalTimeout=MAXDWORD;
        if(!SetCommTimeouts(p->fd,&t)) goto bad;
    }
#else
    {
        struct termios t;
        p->fd=open(path,O_RDWR|O_NOCTTY|O_NONBLOCK);
        if(p->fd<0) { free(p); return SUART_ERROR; }
        if(tcgetattr(p->fd,&p->saved)<0) { close(p->fd); free(p); return SUART_ERROR; }
        t=p->saved; t.c_iflag=0; t.c_oflag=0; t.c_lflag=0;
        t.c_cflag &= ~(CSIZE|PARENB|PARODD|CSTOPB|HUPCL); t.c_cflag |= CLOCAL|CREAD;
        switch(c->data_bits) { case 5:t.c_cflag|=CS5;break;case 6:t.c_cflag|=CS6;break;case 7:t.c_cflag|=CS7;break;default:t.c_cflag|=CS8; }
        if(c->parity!='N') { t.c_cflag|=PARENB; t.c_iflag|=INPCK; }
        if(c->parity=='O') t.c_cflag|=PARODD;
        if(c->stop_bits==2) t.c_cflag|=CSTOPB;
#ifdef CRTSCTS
        t.c_cflag &= ~CRTSCTS; if(c->rts_cts) t.c_cflag|=CRTSCTS;
#else
        if(c->rts_cts) { suart_close(p); return SUART_UNSUPPORTED; }
#endif
        t.c_cc[VMIN]=1; t.c_cc[VTIME]=0;
        if(cfsetispeed(&t,speed(c->baud))<0 || cfsetospeed(&t,speed(c->baud))<0 || tcsetattr(p->fd,TCSANOW,&t)<0) goto bad;
        if(c->dtr!=SUART_DTR_KEEP && suart_set_dtr(p,c->dtr==SUART_DTR_ON)!=SUART_OK) goto bad;
    }
#endif
    if(c->dtr==SUART_DTR_RESET) {
        suart_sleep(100); if(suart_set_dtr(p,1)!=SUART_OK) goto bad;
    }
    suart_sleep(c->boot_wait_ms); *out=p; return SUART_OK;
bad:
    suart_close(p); return SUART_ERROR;
}
void suart_close(suart_port *p) {
    if(!p) return;
#ifdef _WIN32
    CloseHandle(p->fd);
#else
    /* Do not reintroduce a hangup-triggered DTR transition. */
    p->saved.c_cflag &= ~HUPCL;
    tcsetattr(p->fd,TCSANOW,&p->saved); close(p->fd);
#endif
    free(p);
}
static int io_bytes(suart_port *p,void *buf,size_t cap,size_t *done,unsigned ms,int writing) {
    uint64_t end;
    if(done) *done=0;
    if(!p || !done || (!buf && cap) || cap>INT_MAX || ms>60000) return SUART_INVALID;
    if(!cap) return SUART_OK;
    end=suart_millis()+ms;
#ifdef _WIN32
    if(writing) {
        COMMTIMEOUTS t; DWORD n=0;
        memset(&t,0,sizeof(t)); t.ReadIntervalTimeout=MAXDWORD; t.WriteTotalTimeoutConstant=ms?ms:1;
        if(!SetCommTimeouts(p->fd,&t)) return fail(p);
        if(!WriteFile(p->fd,buf,(DWORD)cap,&n,NULL)) { *done=n; return fail(p); }
        *done=n; return SUART_OK;
    }
    do {
        DWORD errors,n=0; COMSTAT s;
        if(!ClearCommError(p->fd,&errors,&s)) return fail(p);
        if(errors & (CE_FRAME|CE_OVERRUN|CE_RXOVER|CE_RXPARITY)) { p->error=errors; return SUART_ERROR; }
        if(s.cbInQue) {
            DWORD want=s.cbInQue<(DWORD)cap?s.cbInQue:(DWORD)cap;
            if(!ReadFile(p->fd,buf,want,&n,NULL)) return fail(p);
            *done=n; return SUART_OK;
        }
        if(suart_millis()>=end) break;
        suart_sleep(1);
    } while(1);
#else
    do {
        struct pollfd f; int ready; ssize_t n; uint64_t now=suart_millis();
        f.fd=p->fd; f.events=(short)(writing?POLLOUT:POLLIN); f.revents=0;
        ready=poll(&f,1,now<end?(int)(end-now):0);
        if(ready<0) { if(errno==EINTR) continue; return fail(p); }
        if(!ready) break;
        if(f.revents&(POLLERR|POLLNVAL)) { errno=EIO; return fail(p); }
        n=writing?write(p->fd,buf,cap):read(p->fd,buf,cap);
        if(n>0) { *done=(size_t)n; return SUART_OK; }
        if(n==0 || (f.revents&POLLHUP)) { errno=EIO; return fail(p); }
        if(errno!=EAGAIN && errno!=EWOULDBLOCK && errno!=EINTR) return fail(p);
    } while(suart_millis()<end);
#endif
    return SUART_OK;
}
int suart_read(suart_port *p,void *b,size_t n,size_t *d,unsigned ms) { return io_bytes(p,b,n,d,ms,0); }
int suart_write(suart_port *p,const void *b,size_t n,size_t *d,unsigned ms) { return io_bytes(p,(void *)b,n,d,ms,1); }
int suart_send_break(suart_port *p,unsigned ms) {
    if(!p || ms>60000) return SUART_INVALID;
#ifdef _WIN32
    if(!SetCommBreak(p->fd)) return fail(p);
    suart_sleep(ms); if(!ClearCommBreak(p->fd)) return fail(p);
#else
    if(ioctl(p->fd,TIOCSBRK)<0) return fail(p);
    suart_sleep(ms); if(ioctl(p->fd,TIOCCBRK)<0) return fail(p);
#endif
    return SUART_OK;
}
#ifdef _WIN32
static void utf8(const WCHAR *w,char *out,size_t n) {
    if(!WideCharToMultiByte(CP_UTF8,0,w,-1,out,(int)n,NULL,NULL)) out[0]=0;
}
int suart_enumerate(suart_device_fn cb,void *ctx) {
    HDEVINFO set; SP_DEVINFO_DATA info; DWORD i;
    if(!cb) return SUART_INVALID;
    set=SetupDiGetClassDevsW(&GUID_DEVCLASS_PORTS,NULL,NULL,DIGCF_PRESENT);
    if(set==INVALID_HANDLE_VALUE) return SUART_ERROR;
    memset(&info,0,sizeof(info)); info.cbSize=sizeof(info);
    for(i=0;SetupDiEnumDeviceInfo(set,i,&info);++i) {
        suart_device d; HKEY key; WCHAR text[512]; DWORD len=sizeof(text),type;
        memset(&d,0,sizeof(d));
        key=SetupDiOpenDevRegKey(set,&info,DICS_FLAG_GLOBAL,0,DIREG_DEV,KEY_READ);
        if(key==INVALID_HANDLE_VALUE) continue;
        if(RegQueryValueExW(key,L"PortName",NULL,&type,(BYTE *)text,&len)!=ERROR_SUCCESS) { RegCloseKey(key); continue; }
        RegCloseKey(key); text[511]=0; utf8(text,d.path,sizeof(d.path));
        if(strncmp(d.path,"COM",3)) continue;
        if(SetupDiGetDeviceRegistryPropertyW(set,&info,SPDRP_FRIENDLYNAME,NULL,(BYTE *)text,sizeof(text),NULL)) {
            text[511]=0; utf8(text,d.name,sizeof(d.name));
        } else snprintf(d.name,sizeof(d.name),"%s",d.path);
        if(SetupDiGetDeviceRegistryPropertyW(set,&info,SPDRP_HARDWAREID,NULL,(BYTE *)text,sizeof(text),NULL)) {
            char id[2048],*v,*p; text[511]=0; utf8(text,id,sizeof(id));
            v=strstr(id,"VID_"); p=strstr(id,"PID_");
            if(v) d.vid=(unsigned)strtoul(v+4,NULL,16);
            if(p) d.pid=(unsigned)strtoul(p+4,NULL,16);
        }
        if(SetupDiGetDeviceInstanceIdW(set,&info,text,512,NULL)) {
            char id[2048],*tail; text[511]=0; utf8(text,id,sizeof(id)); tail=strrchr(id,'\\');
            if(tail && !strchr(tail+1,'&')) snprintf(d.serial,sizeof(d.serial),"%.255s",tail+1);
        }
        if(cb(&d,ctx)) break;
    }
    SetupDiDestroyDeviceInfoList(set); return SUART_OK;
}
#elif defined(__APPLE__)
static CFTypeRef property(io_registry_entry_t e,CFStringRef key) {
    return IORegistryEntrySearchCFProperty(e,kIOServicePlane,key,kCFAllocatorDefault,kIORegistryIterateRecursively|kIORegistryIterateParents);
}
static void cftext(io_registry_entry_t e,CFStringRef key,char *out,size_t n) {
    CFTypeRef v=property(e,key);
    if(v) { if(CFGetTypeID(v)==CFStringGetTypeID()) CFStringGetCString((CFStringRef)v,out,(CFIndex)n,kCFStringEncodingUTF8); CFRelease(v); }
}
static unsigned cfnumber(io_registry_entry_t e,CFStringRef key) {
    int num=0; CFTypeRef v=property(e,key);
    if(v) { if(CFGetTypeID(v)==CFNumberGetTypeID()) CFNumberGetValue((CFNumberRef)v,kCFNumberIntType,&num); CFRelease(v); } return (unsigned)num;
}
int suart_enumerate(suart_device_fn cb,void *ctx) {
    io_iterator_t iter; io_object_t obj; if(!cb) return SUART_INVALID;
    if(IOServiceGetMatchingServices(kIOMainPortDefault,IOServiceMatching(kIOSerialBSDServiceValue),&iter)!=KERN_SUCCESS) return SUART_ERROR;
    while((obj=IOIteratorNext(iter))) {
        suart_device d; memset(&d,0,sizeof(d));
        cftext(obj,CFSTR(kIOCalloutDeviceKey),d.path,sizeof(d.path));
        cftext(obj,CFSTR("USB Product Name"),d.name,sizeof(d.name));
        cftext(obj,CFSTR("USB Serial Number"),d.serial,sizeof(d.serial));
        d.vid=cfnumber(obj,CFSTR("idVendor")); d.pid=cfnumber(obj,CFSTR("idProduct"));
        if(!d.name[0]) snprintf(d.name,sizeof(d.name),"%s",d.path);
        IOObjectRelease(obj); if(d.path[0] && cb(&d,ctx)) break;
    }
    IOObjectRelease(iter); return SUART_OK;
}
#else
#ifdef __linux__
static void filetext(const char *dir,const char *key,char *out,size_t n) {
    char path[2048]; FILE *f; snprintf(path,sizeof(path),"%s/%s",dir,key);
    f=fopen(path,"r"); if(!f) return;
    if(fgets(out,(int)n,f)) out[strcspn(out,"\r\n")]=0;
    fclose(f);
}
#endif
int suart_enumerate(suart_device_fn cb,void *ctx) {
    glob_t g; size_t i; int rc;
    if(!cb) return SUART_INVALID;
    memset(&g,0,sizeof(g)); rc=glob("/dev/tty[A-Za-z]*",0,NULL,&g);
    if(rc!=0 && rc!=GLOB_NOMATCH) { globfree(&g); return SUART_ERROR; }
    for(i=0;i<g.gl_pathc;++i) {
        suart_device d; const char *base=strrchr(g.gl_pathv[i],'/');
        if(!base || !(strncmp(base+1,"ttyUSB",6)==0 || strncmp(base+1,"ttyACM",6)==0 ||
           strncmp(base+1,"ttyS",4)==0 || strncmp(base+1,"ttyAMA",6)==0 || strncmp(base+1,"ttyTHS",6)==0)) continue;
        memset(&d,0,sizeof(d)); snprintf(d.path,sizeof(d.path),"%s",g.gl_pathv[i]);
#ifdef __linux__
        {
            char link[1024],real[PATH_MAX];
            snprintf(link,sizeof(link),"/sys/class/tty/%s/device",base+1);
            if(realpath(link,real)) {
                for(;;) {
                    char value[128]="",*slash;
                    filetext(real,"idVendor",value,sizeof(value));
                    if(value[0]) {
                        d.vid=(unsigned)strtoul(value,NULL,16); value[0]=0;
                        filetext(real,"idProduct",value,sizeof(value)); d.pid=(unsigned)strtoul(value,NULL,16);
                        filetext(real,"product",d.name,sizeof(d.name)); filetext(real,"serial",d.serial,sizeof(d.serial)); break;
                    }
                    slash=strrchr(real,'/'); if(!slash || slash==real) break; *slash=0;
                }
            }
        }
#endif
        if(!d.name[0]) snprintf(d.name,sizeof(d.name),"%s",base+1);
        if(cb(&d,ctx)) break;
    }
    globfree(&g); return SUART_OK;
}
#endif
typedef struct { const suart_selector *s; suart_device d; unsigned count; } match_ctx;
static int match(const suart_device *d,void *ctx) {
    match_ctx *m=(match_ctx *)ctx; const suart_selector *s=m->s;
    if(s->path && strcmp(s->path,d->path)) return 0;
    if(s->name && strcmp(s->name,d->name)) return 0;
    if(s->serial && strcmp(s->serial,d->serial)) return 0;
    if(s->vid && s->vid!=d->vid) return 0;
    if(s->pid && s->pid!=d->pid) return 0;
    m->d=*d; ++m->count; return m->count>1;
}
int suart_resolve(const suart_selector *s,suart_device *d) {
    match_ctx m; int rc;
    if(!s || !d || s->vid>65535 || s->pid>65535) return SUART_INVALID;
    memset(&m,0,sizeof(m)); m.s=s;
    if(s->path && !s->name && !s->serial && !s->vid && !s->pid) {
        if(!*s->path || strlen(s->path)>=sizeof(d->path)) return SUART_INVALID;
        memset(d,0,sizeof(*d)); snprintf(d->path,sizeof(d->path),"%s",s->path); return SUART_OK;
    }
    rc=suart_enumerate(match,&m); if(rc) return rc;
    if(!m.count) return SUART_NOT_FOUND;
    if(m.count>1) return SUART_AMBIGUOUS;
    *d=m.d; return SUART_OK;
}
