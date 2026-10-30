#ifndef SERIALUART_H
#define SERIALUART_H
#include <stddef.h>
#include <stdint.h>
#if defined(_WIN32) && defined(SUART_SHARED)
# ifdef SUART_BUILD
#  define SUART_API __declspec(dllexport)
# else
#  define SUART_API __declspec(dllimport)
# endif
#else
# define SUART_API
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define SUART_VERSION "2.0.0"
typedef struct suart_port suart_port;
enum { SUART_OK=0, SUART_ERROR=-1, SUART_INVALID=-2, SUART_NOT_FOUND=-3,
       SUART_AMBIGUOUS=-4, SUART_UNSUPPORTED=-5 };
typedef enum { SUART_DTR_KEEP, SUART_DTR_OFF, SUART_DTR_ON, SUART_DTR_RESET } suart_dtr;
typedef struct {
    unsigned baud, data_bits;
    char parity; /* N, E, O */
    unsigned stop_bits;
    int rts_cts;
    suart_dtr dtr;
    unsigned boot_wait_ms;
} suart_config;
typedef struct {
    char path[512], name[512], serial[256];
    unsigned vid, pid; /* zero if unknown */
} suart_device;
typedef struct {
    const char *path, *name, *serial; /* exact match, NULL means no filter */
    unsigned vid, pid; /* zero means no filter */
} suart_selector;
typedef int (*suart_device_fn)(const suart_device *, void *);
SUART_API const char *suart_version(void);
SUART_API void suart_config_init(suart_config *);
/* Enumeration opens no ports; callback returns nonzero to stop. */
SUART_API int suart_enumerate(suart_device_fn, void *);
/* All filters must match. Ambiguous selection fails closed. */
SUART_API int suart_resolve(const suart_selector *, suart_device *);
SUART_API int suart_open(suart_port **, const char *, const suart_config *);
SUART_API void suart_close(suart_port *);
/* Zero actual bytes with OK means timeout. timeout_ms <= 60000.
 * Writes may be partial; never replay bytes of uncertain delivery. */
SUART_API int suart_read(suart_port *, void *, size_t, size_t *actual, unsigned timeout_ms);
SUART_API int suart_write(suart_port *, const void *, size_t, size_t *actual, unsigned timeout_ms);
SUART_API int suart_set_dtr(suart_port *, int);
SUART_API int suart_set_rts(suart_port *, int);
SUART_API int suart_send_break(suart_port *, unsigned duration_ms);
SUART_API unsigned long suart_native_error(const suart_port *);
SUART_API uint64_t suart_millis(void);
SUART_API void suart_sleep(unsigned);
#ifdef __cplusplus
}
#endif
#endif
