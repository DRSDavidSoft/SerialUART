#include "linux_native.h"
#include <stdio.h>
static int invalid;
static int found(const suart_device_info *info,void *ctx) {
    (void)ctx; const suart_device *d=&info->device;
    printf("%s\t%s\t%04X:%04X\t%s\n",d->path,d->name,d->vid,d->pid,d->serial);
    if(!info->is_usb || !info->instance_id[0] || !info->location[0] || !info->product[0] || !info->manufacturer[0]) { invalid=1; return 1; }
    return 0;
}
int main(int argc,char **argv) {
    if(argc!=3) return 2;
    return suart_linux_enumerate_ex_at(argv[1],argv[2],found,NULL) || invalid?1:0;
}
