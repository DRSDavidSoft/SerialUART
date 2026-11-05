#ifndef PS_IO_H
#define PS_IO_H

#include "portable_shell_legacy.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Standard I/O (stdin/stdout) */
void ps_io_stdio_init(PSConfig *config);

/* TCP/IP socket I/O */
typedef struct {
    intptr_t socket_fd;
    char read_buffer[256];
    int read_pos;
    int read_len;
} PSSocketContext;

int ps_io_socket_init(PSConfig *config, PSSocketContext *ctx, const char *host, int port);
void ps_io_socket_close(PSSocketContext *ctx);

/* UDP socket I/O */
typedef struct {
    intptr_t socket_fd;
    char read_buffer[256];
    int read_pos;
    int read_len;
    bool peer_connected;
} PSUDPContext;

int ps_io_udp_init(PSConfig *config, PSUDPContext *ctx, int port);
void ps_io_udp_close(PSUDPContext *ctx);

/* Serial port I/O (for embedded platforms) */
typedef struct {
    void *serial_handle;
    int baud_rate;
} PSSerialContext;

int ps_io_serial_init(PSConfig *config, PSSerialContext *ctx, const char *port, int baud_rate);
void ps_io_serial_close(PSSerialContext *ctx);

#ifdef __cplusplus
}
#endif

#endif /* PS_IO_H */
