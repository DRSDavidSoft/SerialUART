#include "linux_native.h"
#include <stdio.h>
static int found(const suart_device *d,void *ctx) {
    (void)ctx;
    printf("%s\t%s\t%04X:%04X\t%s\n",d->path,d->name,d->vid,d->pid,d->serial);
    return 0;
}
int main(int argc,char **argv) {
    if(argc!=3) return 2;
    return suart_linux_enumerate_at(argv[1],argv[2],found,NULL)?1:0;
}
