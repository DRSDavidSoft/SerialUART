#ifndef PORTABLE_SHELL_DEVICE_H
#define PORTABLE_SHELL_DEVICE_H
#include "portable_shell.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Optional compatibility profile. Firmware owns all effects. The callback must
   preserve its existing parameter validation/read-only rules. Return 0 on success.
   No callback means an explicit unavailable error, never a simulated device action. */
typedef int (*ps_device_call_fn)(ps_session *, const char *command, const char *raw_tail,
                                void *context);
typedef struct ps_device_profile ps_device_profile;
typedef struct {
    ps_device_profile *owner;
    unsigned command;
} ps_device_slot;
struct ps_device_profile {
    ps_device_call_fn call;
    void *context;
    ps_device_slot slots[15];
};
/* Initialize session with PS_CASE_INSENSITIVE | PS_NO_BUILTINS | PS_LEGACY_KEYS plus desired I/O
   flags. profile and its callback context must outlive the session. Attach before
   ps_start. Raw get/set/status override the workbench meanings intentionally. */
int ps_device_attach(ps_session *, ps_device_profile *, ps_device_call_fn, void *context);
/* Optional dynamic greeting: caller supplies connected peer/product/hostname.
   Call after transport is ready. Does not assume a network or query an OS. */
void ps_device_greet(ps_session *, const char *product, const char *hostname, const char *peer);
#ifdef __cplusplus
}
#endif
#endif
