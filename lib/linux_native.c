#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include "linux_native.h"
#include <asm/termbits.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <linux/serial.h>
int suart_linux_counters(int fd,suart_counters *c) {
    struct serial_icounter_struct s;
    memset(&s,0,sizeof(s));
    if(ioctl(fd,TIOCGICOUNT,&s)<0) return -1;
    c->rx=(uint32_t)s.rx; c->tx=(uint32_t)s.tx;
    c->frame=(uint32_t)s.frame; c->parity=(uint32_t)s.parity;
    c->overrun=(uint32_t)s.overrun; c->breaks=(uint32_t)s.brk;
    c->buffer_overrun=(uint32_t)s.buf_overrun; return 0;
}
void *suart_linux_save(int fd) {
#ifdef TCGETS2
    struct termios2 *t=(struct termios2 *)malloc(sizeof(*t));
    if(!t) return NULL;
    if(ioctl(fd,TCGETS2,t)<0) { free(t); return NULL; }
    return t;
#else
    (void)fd; return NULL;
#endif
}
void suart_linux_restore(int fd,void *state) {
#ifdef TCSETS2
    struct termios2 *t=(struct termios2 *)state;
    if(t) { t->c_cflag &= ~HUPCL; ioctl(fd,TCSETS2,t); free(t); }
#else
    (void)fd; free(state);
#endif
}

int suart_linux_get_rate(int fd,unsigned *baud) {
#if defined(TCGETS2) && defined(BOTHER)
    struct termios2 t;
    if(ioctl(fd,TCGETS2,&t)<0) return -1;
    *baud=t.c_ospeed; return 0;
#else
    (void)fd; (void)baud; errno=ENOTSUP; return -1;
#endif
}
int suart_linux_set_rate(int fd,unsigned baud) {
#if defined(TCGETS2) && defined(TCSETS2) && defined(BOTHER)
    struct termios2 t; unsigned actual;
    if(ioctl(fd,TCGETS2,&t)<0) return -1;
    t.c_cflag &= ~CBAUD;
#ifdef CIBAUD
    t.c_cflag &= ~CIBAUD;
#endif
    t.c_cflag |= BOTHER; t.c_ispeed=baud; t.c_ospeed=baud;
    if(ioctl(fd,TCSETS2,&t)<0 || suart_linux_get_rate(fd,&actual)<0) return -1;
    if(actual!=baud) { errno=EINVAL; return -1; }
    return 0;
#else
    (void)fd; (void)baud; errno=ENOTSUP; return -1;
#endif
}
static void filetext(const char *dir,const char *key,char *out,size_t n) {
    char path[PATH_MAX+64]; FILE *f;
    if(snprintf(path,sizeof(path),"%s/%s",dir,key)>=(int)sizeof(path)) return;
    f=fopen(path,"r"); if(!f) return;
    if(fgets(out,(int)n,f)) out[strcspn(out,"\r\n")]=0;
    fclose(f);
}
int suart_linux_enumerate_at(const char *sys,const char *dev,suart_device_fn cb,void *ctx) {
    DIR *dir; struct dirent *entry;
    if(!sys || !dev || !cb) return SUART_INVALID;
    dir=opendir(sys); if(!dir) return SUART_ERROR;
    while((entry=readdir(dir))) {
        char path[PATH_MAX],real[PATH_MAX]; struct stat st; suart_device d;
        if(entry->d_name[0]=='.') continue;
        /* A hardware-backed tty has a device association. Never open the
         * device to probe UART type: even an open may reset a board. */
        if(snprintf(path,sizeof(path),"%s/%s/device",sys,entry->d_name)>=(int)sizeof(path) || !realpath(path,real)) continue;
        memset(&d,0,sizeof(d));
        if(snprintf(d.path,sizeof(d.path),"%s/%s",dev,entry->d_name)>=(int)sizeof(d.path)) continue;
        if(stat(d.path,&st)<0 || !S_ISCHR(st.st_mode)) continue;
        for(;;) {
            char value[128]="",*slash;
            filetext(real,"idVendor",value,sizeof(value));
            if(value[0]) {
                d.vid=(unsigned)strtoul(value,NULL,16); value[0]=0;
                filetext(real,"idProduct",value,sizeof(value)); d.pid=(unsigned)strtoul(value,NULL,16);
                filetext(real,"product",d.name,sizeof(d.name));
                filetext(real,"serial",d.serial,sizeof(d.serial)); break;
            }
            slash=strrchr(real,'/'); if(!slash || slash==real) break; *slash=0;
        }
        if(!d.name[0]) snprintf(d.name,sizeof(d.name),"%s",entry->d_name);
        if(cb(&d,ctx)) break;
    }
    closedir(dir); return SUART_OK;
}
