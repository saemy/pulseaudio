/***
  This file is part of PulseAudio.

  Copyright 2004-2006 Lennart Poettering

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2.1 of the License,
  or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with PulseAudio; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

#include <pulse/util.h>
#include <pulse/xmalloc.h>

#include <pulsecore/module.h>
#include <pulsecore/sink.h>
#include <pulsecore/core-scache.h>
#include <pulsecore/modargs.h>
#include <pulsecore/namereg.h>
#include <pulsecore/log.h>
#include <pulsecore/x11wrap.h>
#include <pulsecore/core-util.h>
#include <pulsecore/native-common.h>
#include <pulsecore/auth-cookie.h>
#include <pulsecore/x11prop.h>
#include <pulsecore/strlist.h>
#include <pulsecore/shared.h>
#include <pulsecore/protocol-native.h>

#include "module-x11-publish-symdef.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("X11 credential publisher");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(FALSE);
PA_MODULE_USAGE(
        "display=<X11 display> "
        "sink=<Sink to publish> "
        "source=<Source to publish> "
        "cookie=<Cookie file to publish> ");

static const char* const valid_modargs[] = {
    "display",
    "sink",
    "source",
    "cookie",
    NULL
};

struct userdata {
    pa_core *core;
    pa_module *module;
    pa_native_protocol *protocol;

    char *id;
    pa_auth_cookie *auth_cookie;

    pa_x11_wrapper *x11_wrapper;
    pa_x11_client *x11_client;

    pa_hook_slot *hook_slot;
};

static void publish_servers(struct userdata *u, pa_strlist *l) {

    if (l) {
        char *s;

        l = pa_strlist_reverse(l);
        s = pa_strlist_tostring(l);
        l = pa_strlist_reverse(l);

        pa_x11_set_prop(pa_x11_wrapper_get_display(u->x11_wrapper), "PULSE_SERVER", s);
        pa_xfree(s);
    } else
        pa_x11_del_prop(pa_x11_wrapper_get_display(u->x11_wrapper), "PULSE_SERVER");
}

static pa_hook_result_t servers_changed_cb(void *hook_data, void *call_data, void *slot_data) {
    pa_strlist *servers = call_data;
    struct userdata *u = slot_data;
    char t[256];

    pa_assert(u);

    if (!pa_x11_get_prop(pa_x11_wrapper_get_display(u->x11_wrapper), "PULSE_ID", t, sizeof(t)) || strcmp(t, u->id)) {
        pa_log_warn("PulseAudio information vanished from X11!");
        return PA_HOOK_OK;
    }

    publish_servers(u, servers);
    return PA_HOOK_OK;
}

static void x11_kill_cb(pa_x11_wrapper *w, void *userdata) {
    struct userdata *u = userdata;

    pa_assert(w);
    pa_assert(u);
    pa_assert(u->x11_wrapper == w);

    if (u->x11_client)
        pa_x11_client_free(u->x11_client);

    if (u->x11_wrapper)
        pa_x11_wrapper_unref(u->x11_wrapper);

    u->x11_client = NULL;
    u->x11_wrapper = NULL;

    pa_module_unload_request(u->module, TRUE);
}

int pa__init(pa_module*m) {
    struct userdata *u;
    pa_modargs *ma = NULL;
    char hn[256], un[128];
    char hx[PA_NATIVE_COOKIE_LENGTH*2+1];
    const char *t;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("failed to parse module arguments");
        goto fail;
    }

    m->userdata = u = pa_xnew(struct userdata, 1);
    u->core = m->core;
    u->module = m;
    u->protocol = pa_native_protocol_get(m->core);
    u->id = NULL;
    u->auth_cookie = NULL;
    u->x11_client = NULL;
    u->x11_wrapper = NULL;

    u->hook_slot = pa_hook_connect(&pa_native_protocol_hooks(u->protocol)[PA_NATIVE_HOOK_SERVERS_CHANGED], PA_HOOK_NORMAL, servers_changed_cb, u);

    if (!(u->auth_cookie = pa_auth_cookie_get(m->core, pa_modargs_get_value(ma, "cookie", PA_NATIVE_COOKIE_FILE), PA_NATIVE_COOKIE_LENGTH)))
        goto fail;

    if (!(u->x11_wrapper = pa_x11_wrapper_get(m->core, pa_modargs_get_value(ma, "display", NULL))))
        goto fail;

    if (!pa_get_fqdn(hn, sizeof(hn)) || !pa_get_user_name(un, sizeof(un)))
        goto fail;

    u->id = pa_sprintf_malloc("%s@%s/%u", un, hn, (unsigned) getpid());
    pa_x11_set_prop(pa_x11_wrapper_get_display(u->x11_wrapper), "PULSE_ID", u->id);

    publish_servers(u, pa_native_protocol_servers(u->protocol));

    if ((t = pa_modargs_get_value(ma, "source", NULL)))
        pa_x11_set_prop(pa_x11_wrapper_get_display(u->x11_wrapper), "PULSE_SOURCE", t);

    if ((t = pa_modargs_get_value(ma, "sink", NULL)))
        pa_x11_set_prop(pa_x11_wrapper_get_display(u->x11_wrapper), "PULSE_SINK", t);

    pa_x11_set_prop(pa_x11_wrapper_get_display(u->x11_wrapper), "PULSE_COOKIE",
                    pa_hexstr(pa_auth_cookie_read(u->auth_cookie, PA_NATIVE_COOKIE_LENGTH), PA_NATIVE_COOKIE_LENGTH, hx, sizeof(hx)));

    u->x11_client = pa_x11_client_new(u->x11_wrapper, NULL, x11_kill_cb, u);

    pa_modargs_free(ma);

    return 0;

fail:
    if (ma)
        pa_modargs_free(ma);

    pa__done(m);

    return -1;
}

void pa__done(pa_module*m) {
    struct userdata*u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->x11_client)
        pa_x11_client_free(u->x11_client);

    if (u->x11_wrapper) {
        char t[256];

        /* Yes, here is a race condition */
        if (!pa_x11_get_prop(pa_x11_wrapper_get_display(u->x11_wrapper), "PULSE_ID", t, sizeof(t)) || strcmp(t, u->id))
            pa_log_warn("PulseAudio information vanished from X11!");
        else {
            pa_x11_del_prop(pa_x11_wrapper_get_display(u->x11_wrapper), "PULSE_ID");
            pa_x11_del_prop(pa_x11_wrapper_get_display(u->x11_wrapper), "PULSE_SERVER");
            pa_x11_del_prop(pa_x11_wrapper_get_display(u->x11_wrapper), "PULSE_SINK");
            pa_x11_del_prop(pa_x11_wrapper_get_display(u->x11_wrapper), "PULSE_SOURCE");
            pa_x11_del_prop(pa_x11_wrapper_get_display(u->x11_wrapper), "PULSE_COOKIE");
            XSync(pa_x11_wrapper_get_display(u->x11_wrapper), False);
        }

        pa_x11_wrapper_unref(u->x11_wrapper);
    }

    if (u->auth_cookie)
        pa_auth_cookie_unref(u->auth_cookie);

    if (u->hook_slot)
        pa_hook_slot_free(u->hook_slot);

    if (u->protocol)
        pa_native_protocol_unref(u->protocol);

    pa_xfree(u->id);
    pa_xfree(u);
}
