#ifndef SUART_LINUX_NATIVE_H
#define SUART_LINUX_NATIVE_H
#include "SerialUART.h"
/* Internal interfaces. Kept separate from libc termios: kernel termios2 has
 * a different ABI. Roots are explicit to allow isolated discovery fixtures. */
int suart_linux_set_rate(int fd,unsigned baud);
int suart_linux_get_rate(int fd,unsigned *baud);
void *suart_linux_save(int fd);
void suart_linux_restore(int fd,void *state);
int suart_linux_counters(int fd,suart_counters *c);
int suart_linux_enumerate_at(const char *sys,const char *dev,suart_device_fn cb,void *ctx);
#endif
