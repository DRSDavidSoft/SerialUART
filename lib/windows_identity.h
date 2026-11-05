#ifndef SUART_WINDOWS_IDENTITY_H
#define SUART_WINDOWS_IDENTITY_H
#include "SerialUART.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Process live Windows devnode IDs from the port toward its parents.
 * This pure parser is also exercised on non-Windows CI runners. */
static unsigned suart_windows_id_value(const char *id,const char *key) {
    const char *p=strstr(id,key); char value[5];
    if(!p) return 65536;
    p+=4;
    if(strlen(p)<4 || strspn(p,"0123456789ABCDEFabcdef")!=4) return 65536;
    memcpy(value,p,4); value[4]=0; return (unsigned)strtoul(value,NULL,16);
}
static void suart_windows_identity(suart_device_info *info,const char *id) {
    unsigned vid=suart_windows_id_value(id,"VID_"),pid=suart_windows_id_value(id,"PID_");
    const char *tail; suart_device *d=&info->device;
    if(!d->vid && vid && vid<65536 && pid<65536) { d->vid=vid; d->pid=pid; }
    if(strncmp(id,"USB\\",4)) return;
    info->is_usb=1;
    /* A hub's serial is not this adapter's serial. Composite interface IDs
     * with generated ampersand tails are not hardware serial numbers. */
    if(vid!=d->vid || pid!=d->pid || !vid || vid>65535 || pid>65535 || strstr(id,"&MI_")) return;
    tail=strrchr(id,'\\');
    if(!d->serial[0] && tail && tail[1] && !strchr(tail+1,'&'))
        snprintf(d->serial,sizeof(d->serial),"%.255s",tail+1);
}
#endif
