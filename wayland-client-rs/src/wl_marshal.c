/*
 * Copyright © 2026 Yuichiro Nakada / Project Vespera
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 */

/* wl_proxy_marshal_flags: variadic marshal via stdarg (Rust c_variadic is unstable). */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

typedef int32_t wl_fixed_t;

struct wl_array {
    size_t  size;
    size_t  alloc;
    void   *data;
};

/* wl_argument union (libwayland ABI, 8 bytes) */
union wl_argument {
    int32_t         i;
    uint32_t        u;
    wl_fixed_t      f;
    const char     *s;
    void           *o;
    uint32_t        n;
    struct wl_array *a;
    int32_t         h;
};

struct wl_message {
    const char  *name;
    const char  *signature;
    const void **types;
};

struct wl_interface {
    const char               *name;
    int                       version;
    int                       method_count;
    const struct wl_message  *methods;
    int                       event_count;
    const struct wl_message  *events;
};

extern void *wl_proxy_marshal_array_flags(
    void *proxy,
    uint32_t opcode,
    const struct wl_interface *interface,
    uint32_t version,
    uint32_t flags,
    union wl_argument *args);

void *wl_proxy_marshal_flags(
    void                     *proxy,
    uint32_t                  opcode,
    const struct wl_interface *interface,
    uint32_t                  version,
    uint32_t                  flags,
    ...)
{
    if (!proxy) return NULL;

    /* First field of proxy is the interface pointer. */
    const struct wl_interface *proxy_iface =
        *(const struct wl_interface **)proxy;

    const char *sig = NULL;
    if (proxy_iface && (int)opcode < proxy_iface->method_count &&
            proxy_iface->methods) {
        sig = proxy_iface->methods[opcode].signature;
    }

    union wl_argument args[24];
    int argc = 0;

    if (sig) {
        va_list va;
        va_start(va, flags);

        for (const char *p = sig; *p && argc < 24; ++p) {
            switch (*p) {
            case '?':
            case '0': case '1': case '2': case '3': case '4':
            case '5': case '6': case '7': case '8': case '9':
                break;
            case 'i':
                args[argc++].i = va_arg(va, int32_t);
                break;
            case 'u':
                args[argc++].u = va_arg(va, uint32_t);
                break;
            case 'f':
                /* wl_fixed_t is int32_t but passed as int due to C integer promotion. */
                args[argc++].f = (wl_fixed_t)va_arg(va, int);
                break;
            case 's':
                args[argc++].s = va_arg(va, const char *);
                break;
            case 'o':
                args[argc++].o = va_arg(va, void *);
                break;
            case 'n':
                /* new_id: caller passes NULL; ID is assigned in wl_proxy_marshal_array_flags. */
                (void)va_arg(va, void *);
                args[argc++].n = 0;
                break;
            case 'a':
                args[argc++].a = va_arg(va, struct wl_array *);
                break;
            case 'h':
                /* fd: passed as int due to C integer promotion. */
                args[argc++].h = va_arg(va, int);
                break;
            default:
                break;
            }
        }

        va_end(va);
    }

    return wl_proxy_marshal_array_flags(
        proxy, opcode, interface, version, flags,
        argc > 0 ? args : NULL);
}
