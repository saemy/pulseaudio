/***
  This file is part of PulseAudio.

  Copyright 2009 Lennart Poettering

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

#include <sys/stat.h>
#include <dirent.h>

#include <pulse/xmalloc.h>
#include <pulse/volume.h>
#include <pulse/channelmap.h>

#include <pulsecore/core-error.h>
#include <pulsecore/module.h>
#include <pulsecore/core-util.h>
#include <pulsecore/modargs.h>
#include <pulsecore/log.h>
#include <pulsecore/client.h>
#include <pulsecore/conf-parser.h>

#include "module-augment-properties-symdef.h"

PA_MODULE_AUTHOR("Lennart Poettering");
PA_MODULE_DESCRIPTION("Augment the property sets of streams with additional static information");
PA_MODULE_VERSION(PACKAGE_VERSION);
PA_MODULE_LOAD_ONCE(TRUE);

#define STAT_INTERVAL 30
#define MAX_CACHE_SIZE 50

static const char* const valid_modargs[] = {
    NULL
};

struct rule {
    time_t timestamp;
    pa_bool_t good;
    time_t mtime;
    char *process_name;
    char *application_name;
    char *icon_name;
    char *role;
    pa_proplist *proplist;
};

struct userdata {
    pa_hashmap *cache;
    pa_hook_slot *client_new_slot, *client_proplist_changed_slot;
};

static void rule_free(struct rule *r) {
    pa_assert(r);

    pa_xfree(r->process_name);
    pa_xfree(r->application_name);
    pa_xfree(r->icon_name);
    pa_xfree(r->role);
    if (r->proplist)
        pa_proplist_free(r->proplist);
    pa_xfree(r);
}

static int parse_properties(
        const char *filename,
        unsigned line,
        const char *section,
        const char *lvalue,
        const char *rvalue,
        void *data,
        void *userdata) {

    struct rule *r = userdata;
    pa_proplist *n;

    if (!(n = pa_proplist_from_string(rvalue)))
        return -1;

    if (r->proplist) {
        pa_proplist_update(r->proplist, PA_UPDATE_MERGE, n);
        pa_proplist_free(n);
    } else
        r->proplist = n;

    return 0;
}

static int parse_categories(
        const char *filename,
        unsigned line,
        const char *section,
        const char *lvalue,
        const char *rvalue,
        void *data,
        void *userdata) {

    struct rule *r = userdata;
    const char *state = NULL;
    char *c;

    while ((c = pa_split(rvalue, ";", &state))) {

        if (pa_streq(c, "Game")) {
            pa_xfree(r->role);
            r->role = pa_xstrdup("game");
        } else if (pa_streq(c, "Telephony")) {
            pa_xfree(r->role);
            r->role = pa_xstrdup("phone");
        }

        pa_xfree(c);
    }

    return 0;
}

static int check_type(
        const char *filename,
        unsigned line,
        const char *section,
        const char *lvalue,
        const char *rvalue,
        void *data,
        void *userdata) {

    return pa_streq(rvalue, "Application") ? 0 : -1;
}

static int catch_all(
        const char *filename,
        unsigned line,
        const char *section,
        const char *lvalue,
        const char *rvalue,
        void *data,
        void *userdata) {

    return 0;
}

static void update_rule(struct rule *r) {
    char *fn;
    struct stat st;
    static pa_config_item table[] = {
        { "Name", pa_config_parse_string,              NULL, "Desktop Entry" },
        { "Icon", pa_config_parse_string,              NULL, "Desktop Entry" },
        { "Type", check_type,                          NULL, "Desktop Entry" },
        { "X-PulseAudio-Properties", parse_properties, NULL, "Desktop Entry" },
        { "Categories", parse_categories,              NULL, "Desktop Entry" },
        { NULL,  catch_all, NULL, NULL },
        { NULL, NULL, NULL, NULL },
    };
    pa_bool_t found = FALSE;

    pa_assert(r);
    fn = pa_sprintf_malloc(DESKTOPFILEDIR PA_PATH_SEP "%s.desktop", r->process_name);

    if (stat(fn, &st) == 0)
        found = TRUE;
    else {
        DIR *desktopfiles_dir;
        struct dirent *dir;

        /* Let's try a more aggressive search, but only one level */
        if ((desktopfiles_dir = opendir(DESKTOPFILEDIR))) {
            while ((dir = readdir(desktopfiles_dir))) {
                if (dir->d_type != DT_DIR
                    || strcmp(dir->d_name, ".") == 0
                    || strcmp(dir->d_name, "..") == 0)
                    continue;

                pa_xfree(fn);
                fn = pa_sprintf_malloc(DESKTOPFILEDIR
                                       PA_PATH_SEP "%s" PA_PATH_SEP "%s.desktop",
                                       dir->d_name, r->process_name);

                if (stat(fn, &st) == 0) {
                    found = TRUE;
                    break;
                }
            }
            closedir(desktopfiles_dir);
        }
    }
    if (!found) {
        r->good = FALSE;
        pa_xfree(fn);
        return;
    }

    if (r->good) {
        if (st.st_mtime == r->mtime) {
            /* Theoretically the filename could have changed, but if so
               having the same mtime is very unlikely so not worth tracking it in r */
            pa_xfree(fn);
            return;
        }
        pa_log_debug("Found %s (which has been updated since we last checked).", fn);
    } else
        pa_log_debug("Found %s.", fn);

    r->good = TRUE;
    r->mtime = st.st_mtime;
    pa_xfree(r->application_name);
    pa_xfree(r->icon_name);
    pa_xfree(r->role);
    r->application_name = r->icon_name = r->role = NULL;
    if (r->proplist)
        pa_proplist_clear(r->proplist);

    table[0].data = &r->application_name;
    table[1].data = &r->icon_name;

    if (pa_config_parse(fn, NULL, table, r) < 0)
        pa_log_warn("Failed to parse .desktop file %s.", fn);

    pa_xfree(fn);
}

static void apply_rule(struct rule *r, pa_proplist *p) {
    pa_assert(r);
    pa_assert(p);

    if (!r->good)
        return;

    if (r->proplist)
        pa_proplist_update(p, PA_UPDATE_MERGE, r->proplist);

    if (r->icon_name)
        if (!pa_proplist_contains(p, PA_PROP_APPLICATION_ICON_NAME))
            pa_proplist_sets(p, PA_PROP_APPLICATION_ICON_NAME, r->icon_name);

    if (r->application_name) {
        const char *t;

        t = pa_proplist_gets(p, PA_PROP_APPLICATION_NAME);

        if (!t || pa_streq(t, r->process_name))
            pa_proplist_sets(p, PA_PROP_APPLICATION_NAME, r->application_name);
    }

    if (r->role)
        if (!pa_proplist_contains(p, PA_PROP_MEDIA_ROLE))
            pa_proplist_sets(p, PA_PROP_MEDIA_ROLE, r->role);
}

static void make_room(pa_hashmap *cache) {
    pa_assert(cache);

    while (pa_hashmap_size(cache) >= MAX_CACHE_SIZE) {
        struct rule *r;

        pa_assert_se(r = pa_hashmap_steal_first(cache));
        rule_free(r);
    }
}

static pa_hook_result_t process(struct userdata *u, pa_proplist *p) {
    struct rule *r;
    time_t now;
    const char *pn;

    pa_assert(u);
    pa_assert(p);

    if (!(pn = pa_proplist_gets(p, PA_PROP_APPLICATION_PROCESS_BINARY)))
        return PA_HOOK_OK;

    if (*pn == '.' || strchr(pn, '/'))
        return PA_HOOK_OK;

    time(&now);

    pa_log_debug("Looking for .desktop file for %s", pn);

    if ((r = pa_hashmap_get(u->cache, pn))) {
        if (now-r->timestamp > STAT_INTERVAL) {
            r->timestamp = now;
            update_rule(r);
        }
    } else {
        make_room(u->cache);

        r = pa_xnew0(struct rule, 1);
        r->process_name = pa_xstrdup(pn);
        r->timestamp = now;
        pa_hashmap_put(u->cache, r->process_name, r);
        update_rule(r);
    }

    apply_rule(r, p);
    return PA_HOOK_OK;
}

static pa_hook_result_t client_new_cb(pa_core *core, pa_client_new_data *data, struct userdata *u) {
    pa_core_assert_ref(core);
    pa_assert(data);
    pa_assert(u);

    return process(u, data->proplist);
}

static pa_hook_result_t client_proplist_changed_cb(pa_core *core, pa_client *client, struct userdata *u) {
    pa_core_assert_ref(core);
    pa_assert(client);
    pa_assert(u);

    return process(u, client->proplist);
}

int pa__init(pa_module *m) {
    pa_modargs *ma = NULL;
    struct userdata *u;

    pa_assert(m);

    if (!(ma = pa_modargs_new(m->argument, valid_modargs))) {
        pa_log("Failed to parse module arguments");
        goto fail;
    }

    m->userdata = u = pa_xnew(struct userdata, 1);

    u->cache = pa_hashmap_new(pa_idxset_string_hash_func, pa_idxset_string_compare_func);
    u->client_new_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_CLIENT_NEW], PA_HOOK_EARLY, (pa_hook_cb_t) client_new_cb, u);
    u->client_proplist_changed_slot = pa_hook_connect(&m->core->hooks[PA_CORE_HOOK_CLIENT_PROPLIST_CHANGED], PA_HOOK_EARLY, (pa_hook_cb_t) client_proplist_changed_cb, u);

    pa_modargs_free(ma);

    return 0;

fail:
    pa__done(m);

    if (ma)
        pa_modargs_free(ma);

    return  -1;
}

void pa__done(pa_module *m) {
    struct userdata* u;

    pa_assert(m);

    if (!(u = m->userdata))
        return;

    if (u->client_new_slot)
        pa_hook_slot_free(u->client_new_slot);
    if (u->client_proplist_changed_slot)
        pa_hook_slot_free(u->client_proplist_changed_slot);

    if (u->cache) {
        struct rule *r;

        while ((r = pa_hashmap_steal_first(u->cache)))
            rule_free(r);

        pa_hashmap_free(u->cache, NULL, NULL);
    }

    pa_xfree(u);
}
